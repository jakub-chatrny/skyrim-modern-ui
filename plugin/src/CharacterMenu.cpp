#include "PCH.h"

#include "CharacterMenu.h"

#include "CameraStage.h"
#include "Equipment.h"
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
		PushSnapshot();
	}

	void Close()
	{
		logger::info("Close");
		isOpen = false;
		prisma->Unfocus(view);
		prisma->Hide(view);
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
				// The fresh snapshot follows from the game's equip event.
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
			} else {
				// TODO(phase 5): switchOutfit.
				logger::info("Command not implemented yet: {}", text);
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

			if (!a_events) {
				return RE::BSEventNotifyControl::kContinue;
			}

			for (auto* event = *a_events; event; event = event->next) {
				const auto* button = event->AsButtonEvent();
				if (!button || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard) {
					continue;
				}
				if (button->GetIDCode() != TOGGLE_KEY) {
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
		bool _keyHeld = false;
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

	static HotkeySink sink;
	RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&sink);

	static EquipSink equipSink;
	RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESEquipEvent>(&equipSink);

	logger::info("Character screen installed, toggle key scan code {:#x}", TOGGLE_KEY);
}
