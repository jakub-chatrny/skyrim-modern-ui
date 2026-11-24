#include "PCH.h"

#include "CharacterMenu.h"

#include "CameraStage.h"
#include "Equipment.h"
#include "Outfits.h"
#include "Thumbnails.h"
#include "PrismaUI_API.h"
#include "Snapshot.h"

namespace
{
	// Path relative to Data/PrismaUI/views/. scripts/deploy.sh copies ui/dist there.
	constexpr auto VIEW_PATH = "SkyrimModernUI/index.html";
	// DirectInput scan code. 0x3E = F4. (The Prisma example plugin uses F3.)
	constexpr std::uint32_t TOGGLE_KEY = 0x3E;

	PRISMA_UI_API::IVPrismaUI1* prisma = nullptr;
	PrismaView                  view = 0;

	void PushSnapshot()
	{
		const auto snapshot = Snapshot::Build().dump();
		logger::debug("Snapshot: {}", snapshot);
		// dump() again: turns the JSON text into a JS string literal.
		const auto payload = nlohmann::json(snapshot).dump();
		const auto script = "window.smuiReceive && window.smuiReceive(" + payload + ")";
		prisma->Invoke(view, script.c_str());
	}

	// Our own open/closed state. Prisma applies Show/Focus/Unfocus/Hide asynchronously, so
	// asking it (HasFocus) right after a call returns stale answers.
	bool isOpen = false;
	std::chrono::steady_clock::time_point lastToggle{};

	void Open()
	{
		logger::info("Open");
		isOpen = true;
		const auto stage = CameraStage::LoadConfig();
		CameraStage::Enter(stage);
		prisma->Show(view);
		if (!prisma->Focus(view, stage.pauseGame)) {
			logger::warn("Prisma UI refused focus");
		}
		// Ask for pictures of everything that could be equipped, so the popups
		// have them ready. Only items without one are actually queued, and only
		// when picture-making is switched on.
		Thumbnails::SetEnabled(stage.itemPictures);
		for (std::size_t i = 0; i < Equipment::SLOT_NAMES.size(); ++i) {
			for (const auto& item : Equipment::ListFor(static_cast<Equipment::Slot>(i)).items) {
				Thumbnails::Request(item.form);
			}
		}
		PushSnapshot();
	}

	void Close()
	{
		logger::info("Close");
		isOpen = false;
		prisma->Unfocus(view);
		prisma->Hide(view);
		Thumbnails::Abort();
		CameraStage::Leave();
	}

	// Opening and closing in quick succession leaves Prisma's cursor menu (FocusMenu) open
	// with the view hidden: its close request is dropped while its open request is still
	// queued, and the player is then stuck in menu mode. Never toggle faster than this.
	constexpr auto TOGGLE_COOLDOWN = 400ms;

	// True while the game is in any menu of its own (journal, inventory, map,
	// dialogue, main menu, loading screen, message box, console, ...). The
	// Character screen must not open on top of those.
	bool GameMenuIsOpen()
	{
		const auto ui = RE::UI::GetSingleton();
		if (!ui) {
			return true;
		}
		if (ui->GameIsPaused()) {
			return true;
		}
		using Flag = RE::UI_MENU_FLAGS;
		for (const auto& menu : ui->menuStack) {
			if (menu && menu->menuFlags.any(Flag::kPausesGame, Flag::kUsesMenuContext, Flag::kModal)) {
				return true;
			}
		}
		return false;
	}

	void Toggle(const char* a_reason)
	{
		if (!isOpen && GameMenuIsOpen()) {
			logger::info("Toggle ignored ({}): a game menu is open", a_reason);
			return;
		}

		const auto now = std::chrono::steady_clock::now();
		if (now - lastToggle < TOGGLE_COOLDOWN) {
			logger::info("Toggle ignored ({}): too soon after the previous one", a_reason);
			return;
		}
		lastToggle = now;
		logger::info("Toggle ({})", a_reason);
		if (isOpen) {
			Close();
		} else {
			Open();
		}
	}

	// Which thread Prisma calls JS listeners on is not documented, so everything
	// that touches the game goes through the SKSE task queue (main thread).
	void OnCommand(const char* a_json)
	{
		const std::string text = a_json ? a_json : "";
		SKSE::GetTaskInterface()->AddTask([text]() {
			const auto command = nlohmann::json::parse(text, nullptr, false);
			if (!command.is_object() || !command.contains("type") || !command["type"].is_string()) {
				logger::warn("Malformed command from UI: {}", text);
				return;
			}

			const auto type = command["type"].get<std::string>();
			if (type == "ready") {
				PushSnapshot();
			} else if (type == "close") {
				if (isOpen) {
					Toggle("close command from UI");
				}
			} else if (type == "unequip") {
				const auto slot = Equipment::SlotFromName(command.value("slot", ""));
				if (!slot) {
					logger::warn("unequip: unknown slot in {}", text);
				} else {
					Equipment::Unequip(*slot);
				}
				// Items: the fresh snapshot follows from the game's equip event.
				// Spells fire none when they leave a hand, so answer on the next
				// task run, once the change has been applied.
				SKSE::GetTaskInterface()->AddTask([]() {
					if (isOpen) {
						PushSnapshot();
					}
				});
			} else if (type == "equip") {
				// formId arrives as "0x0001397E".
				const auto id = command.value("formId", "");
				RE::FormID formID = 0;
				const auto [end, error] = std::from_chars(id.data() + (id.starts_with("0x") ? 2 : 0), id.data() + id.size(), formID, 16);
				if (error != std::errc{} || end != id.data() + id.size()) {
					logger::warn("equip: bad formId in {}", text);
				} else {
					Equipment::Equip(formID, Equipment::SlotFromName(command.value("slot", "")));
				}
			} else if (type == "listItems") {
				const auto slot = Equipment::SlotFromName(command.value("slot", ""));
				if (!slot) {
					logger::warn("listItems: unknown slot in {}", text);
				} else {
					const auto payload = nlohmann::json(Snapshot::BuildItemList(*slot).dump()).dump();
					const auto script = "window.smuiItems && window.smuiItems(" + payload + ")";
					prisma->Invoke(view, script.c_str());
				}
			} else if (type == "switchOutfit") {
				const auto index = command.value("index", -1);
				if (index < 0 || !Outfits::Switch(static_cast<std::size_t>(index))) {
					logger::info("switchOutfit: nothing to do for {}", text);
				}
				// Always answer: the UI waits for the snapshot to move its highlight,
				// and an outfit identical to the current one fires no equip event.
				PushSnapshot();
			} else {
				logger::info("Unknown command: {}", text);
			}
		});
	}

	// Equipment can change while the screen is open (our own commands, scripts,
	// other mods). Push one fresh snapshot per burst of equip events, on the next
	// main-thread task run, by which time the game has applied the change.
	class EquipSink : public RE::BSTEventSink<RE::TESEquipEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*) override
		{
			if (isOpen && a_event && a_event->actor && a_event->actor->IsPlayerRef() && !_pending.exchange(true)) {
				SKSE::GetTaskInterface()->AddTask([this]() {
					_pending = false;
					if (isOpen) {
						PushSnapshot();
					}
				});
			}
			return RE::BSEventNotifyControl::kContinue;
		}

	private:
		std::atomic<bool> _pending = false;
	};

	class HotkeySink : public RE::BSTEventSink<RE::InputEvent*>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events, RE::BSTEventSource<RE::InputEvent*>*) override
		{
			// The input manager calls its sinks every frame, events or not, which makes
			// this the plugin's per-frame hook.
			CameraStage::Tick();
			if (isOpen) {
				// When new item pictures are saved, tell the page where they are.
				Thumbnails::Tick([]() { PushSnapshot(); });
			} else {
				Thumbnails::AbortTick();
			}

			if (!a_events) {
				return RE::BSEventNotifyControl::kContinue;
			}

			for (auto* event = *a_events; event; event = event->next) {
				const auto* button = event->AsButtonEvent();
				if (!button || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard) {
					continue;
				}
				if (button->GetIDCode() != TOGGLE_KEY) {
					ForwardKey(*button);
					continue;
				}
				// Act once per physical press. IsDown() (held duration == 0) can stay true
				// for several polls while the game is paused, which toggled repeatedly.
				const bool pressed = button->IsPressed();
				if (pressed && !_keyHeld) {
					Toggle("hotkey");
				}
				_keyHeld = pressed;
			}

			return RE::BSEventNotifyControl::kContinue;
		}

	private:
		// Prisma UI does not pass the keyboard on to the page, so the screen's
		// keys are read here and handed over as window.smuiKey(name), once per
		// press. Names match what the page's keydown handler uses.
		void ForwardKey(const RE::ButtonEvent& a_button)
		{
			const auto code = a_button.GetIDCode();
			if (code >= _held.size()) {
				return;
			}
			const bool pressed = a_button.IsPressed();
			const bool fresh = pressed && !_held[code];
			_held[code] = pressed;
			if (!fresh || !isOpen || !prisma) {
				return;
			}

			const char* name = nullptr;
			switch (code) {  // DirectInput scan codes
			case 0x02: name = "1"; break;
			case 0x03: name = "2"; break;
			case 0x04: name = "3"; break;
			case 0x05: name = "4"; break;
			case 0x10: name = "q"; break;
			case 0x11: name = "w"; break;
			case 0x12: name = "e"; break;
			case 0x1E: name = "a"; break;
			case 0x1F: name = "s"; break;
			case 0x20: name = "d"; break;
			case 0x1C: name = "enter"; break;
			case 0x39: name = " "; break;
			case 0xC8: name = "arrowup"; break;
			case 0xD0: name = "arrowdown"; break;
			case 0xCB: name = "arrowleft"; break;
			case 0xCD: name = "arrowright"; break;
			default: return;
			}
			const auto script = std::format("window.smuiKey && window.smuiKey('{}')", name);
			prisma->Invoke(view, script.c_str());
		}

		bool _keyHeld = false;
		std::array<bool, 256> _held{};
	};
}

void CharacterMenu::Install()
{
	prisma = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI1>();
	if (!prisma) {
		logger::error("PrismaUI.dll not found or too old; the Character screen is disabled");
		return;
	}

	view = prisma->CreateView(VIEW_PATH, [](PrismaView a_view) {
		logger::info("View DOM ready ({})", a_view);
	});

	// Everything the page writes to its console, including uncaught script
	// errors, goes into our log. There is no browser devtools in game.
	if (auto* prisma2 = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>()) {
		prisma2->RegisterConsoleCallback(view, [](PrismaView, PRISMA_UI_API::ConsoleMessageLevel a_level, const char* a_message) {
			using Level = PRISMA_UI_API::ConsoleMessageLevel;
			const auto* text = a_message ? a_message : "";
			if (a_level == Level::Error) {
				logger::error("[ui] {}", text);
			} else if (a_level == Level::Warning) {
				logger::warn("[ui] {}", text);
			} else {
				logger::info("[ui] {}", text);
			}
		});
	} else {
		logger::warn("Prisma UI is too old for console forwarding; UI errors will not be logged");
	}
	prisma->RegisterJSListener(view, "smuiCommand", OnCommand);
	prisma->Hide(view);

	Thumbnails::Install();

	static HotkeySink sink;
	RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&sink);

	static EquipSink equipSink;
	RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESEquipEvent>(&equipSink);

	logger::info("Character screen installed, toggle key scan code {:#x}", TOGGLE_KEY);
}
