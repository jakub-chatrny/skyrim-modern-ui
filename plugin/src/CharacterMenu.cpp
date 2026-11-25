#include "PCH.h"

#include "CharacterMenu.h"

#include <fstream>
#include <numbers>

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
	// The key that opens and closes the screen: "toggleKey" in the settings
	// file, Tab by default. Whatever the game had on that key during play
	// (Tab: its own menu) moves to F4, the screen's key before.
	constexpr auto          CONFIG_PATH = "Data/SKSE/Plugins/SkyrimModernUI.json";
	constexpr std::uint32_t KEY_F4 = 0x3E;  // DirectInput scan codes
	std::uint32_t           toggleKey = 0x0F;
	std::string             toggleKeyName = "Tab";

	// Names the settings file may use; a plain number is a scan code.
	constexpr std::pair<std::string_view, std::uint32_t> KEY_NAMES[] = {
		{ "Tab", 0x0F }, { "F1", 0x3B }, { "F2", 0x3C }, { "F3", 0x3D }, { "F4", 0x3E }, { "F5", 0x3F }, { "F6", 0x40 },
		{ "F7", 0x41 }, { "F8", 0x42 }, { "F9", 0x43 }, { "F10", 0x44 }, { "F11", 0x57 }, { "F12", 0x58 },
		{ "Insert", 0xD2 }, { "Home", 0xC7 }, { "End", 0xCF }, { "PageUp", 0xC9 }, { "PageDown", 0xD1 },
		{ "Backspace", 0x0E }, { "J", 0x24 }, { "K", 0x25 }, { "L", 0x26 }, { "U", 0x16 }, { "I", 0x17 }, { "O", 0x18 },
		{ "P", 0x19 }, { "N", 0x31 }, { "M", 0x32 },
	};

	void ReadToggleKey()
	{
		std::ifstream file(CONFIG_PATH);
		const auto    json = file ? nlohmann::json::parse(file, nullptr, false) : nlohmann::json();
		const auto    it = json.is_object() ? json.find("toggleKey") : json.end();
		if (!json.is_object() || it == json.end()) {
			return;  // default
		}
		if (it->is_number_unsigned() && it->get<std::uint32_t>() > 0 && it->get<std::uint32_t>() < 256) {
			toggleKey = it->get<std::uint32_t>();
			toggleKeyName = std::format("Key {:#x}", toggleKey);
			for (const auto& [name, code] : KEY_NAMES) {
				if (code == toggleKey) {
					toggleKeyName = name;
				}
			}
			return;
		}
		if (it->is_string()) {
			const auto wanted = it->get<std::string>();
			for (const auto& [name, code] : KEY_NAMES) {
				if (_stricmp(std::string(name).c_str(), wanted.c_str()) == 0) {
					toggleKey = code;
					toggleKeyName = name;
					return;
				}
			}
		}
		logger::warn("toggleKey {} in {} is not a key this plugin knows; using {}", it->dump(), CONFIG_PATH, toggleKeyName);
	}

	// Moves what the game does with the toggle key during play to F4, in the
	// game's key map in memory (as if controlmap.txt said so). Menus keep the
	// key (Tab still goes back in the game's menus). Safe to repeat.
	//
	// The game finds a key's event by binary search in a list sorted by key
	// (CommonLib's ControlMap::GetUserEventName mirrors it), so the list is
	// sorted again after the change, and F4 is used only while it is free:
	// two events on one key would leave both unfound.
	void FreeToggleKey()
	{
		constexpr std::uint16_t UNBOUND = 0xFF;  // what the game uses for "no key"
		if (toggleKey == KEY_F4) {
			return;
		}
		auto* controls = RE::ControlMap::GetSingleton();
		auto* gameplay = controls ? controls->controlMap[RE::ControlMap::InputContextID::kGameplay] : nullptr;
		if (!gameplay) {
			logger::warn("Key map not available; {} keeps its game binding too", toggleKeyName);
			return;
		}
		auto&      mappings = gameplay->deviceMappings[RE::INPUT_DEVICE::kKeyboard];
		const bool f4Taken = std::ranges::any_of(mappings, [](const auto& a_mapping) { return a_mapping.inputKey == KEY_F4; });
		bool       moved = false;
		for (auto& mapping : mappings) {
			if (mapping.inputKey != toggleKey) {
				continue;
			}
			// Only one event can take F4; any further one is unbound.
			if (!f4Taken && !moved) {
				mapping.inputKey = static_cast<std::uint16_t>(KEY_F4);
				logger::info("Key map: \"{}\" moved from {} to F4", mapping.eventID.c_str(), toggleKeyName);
			} else {
				mapping.inputKey = UNBOUND;
				logger::warn("Key map: \"{}\" unbound from {} (F4 is taken)", mapping.eventID.c_str(), toggleKeyName);
			}
			moved = true;
		}
		if (moved) {
			std::stable_sort(mappings.begin(), mappings.end(),
				[](const auto& a_lhs, const auto& a_rhs) { return a_lhs.inputKey < a_rhs.inputKey; });
		}
	}

	PRISMA_UI_API::IVPrismaUI1* prisma = nullptr;
	PrismaView                  view = 0;

	void PushSnapshot()
	{
		auto json = Snapshot::Build();
		json["toggleKey"] = toggleKeyName;
		const auto snapshot = json.dump();
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
	// Frames since the screen opened, for the slow-frame log below.
	int framesSinceOpen = 0;

	void Open()
	{
		logger::info("Open");
		isOpen = true;
		framesSinceOpen = 0;
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
		Thumbnails::SetPreview(0, 0.0f, 0.0f, 0.0f, 0.0f);
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
			} else if (type == "drop" || type == "favorite" || type == "use") {
				// formId arrives as "0x0001397E".
				const auto id = command.value("formId", "");
				RE::FormID formID = 0;
				const auto [end, error] = std::from_chars(id.data() + (id.starts_with("0x") ? 2 : 0), id.data() + id.size(), formID, 16);
				const auto act = [&]() {
					return type == "drop" ? Equipment::Drop(formID) : type == "use" ? Equipment::Use(formID) : Equipment::ToggleFavorite(formID);
				};
				if (error != std::errc{} || end != id.data() + id.size()) {
					logger::warn("{}: bad formId in {}", type, text);
				} else if (act()) {
					// Neither fires an equip event every time (a favourite never
					// does), so answer with fresh data on the next task run.
					SKSE::GetTaskInterface()->AddTask([]() {
						if (isOpen) {
							PushSnapshot();
						}
					});
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
			} else if (type == "preview" && !isOpen) {
				// The page can ask while it is hidden (a snapshot pushed ahead of time);
				// only an open screen shows the preview.
				Thumbnails::SetPreview(0, 0.0f, 0.0f, 0.0f, 0.0f);
			} else if (type == "preview") {
				// The Inventory tab: hide the character, and show this item in 3D in
				// the box the page left transparent (fractions of the window).
				const bool open = command.value("open", false);
				CameraStage::SetCharacterHidden(open);
				RE::FormID formID = 0;
				if (open && command.contains("formId") && command["formId"].is_string()) {
					const auto id = command["formId"].get<std::string>();
					std::from_chars(id.data() + (id.starts_with("0x") ? 2 : 0), id.data() + id.size(), formID, 16);
				}
				const auto rect = command.value("rect", nlohmann::json::object());
				Thumbnails::SetPreview(formID, rect.value("left", 0.0f), rect.value("top", 0.0f), rect.value("width", 0.0f),
					rect.value("height", 0.0f));
			} else if (type == "turnCharacter") {
				CameraStage::TurnCharacter(command.value("degrees", 0.0f) * std::numbers::pi_v<float> / 180.0f);
			} else if (type == "turn") {
				Thumbnails::TurnPreview(command.value("degrees", 0.0f));
			} else if (type == "listInventory") {
				const auto payload = nlohmann::json(Snapshot::BuildInventory().dump()).dump();
				const auto script = "window.smuiInventory && window.smuiInventory(" + payload + ")";
				prisma->Invoke(view, script.c_str());
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
			// Slow frames right after opening, to find what the first open waits for.
			{
				static auto last = std::chrono::steady_clock::now();
				const auto  now = std::chrono::steady_clock::now();
				const auto  ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count();
				last = now;
				if (isOpen && ++framesSinceOpen <= 300 && ms > 100) {
					logger::info("Slow frame: {} ms, frame {} after opening", ms, framesSinceOpen);
				}
			}
			Equipment::TickDrops(isOpen);
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
				if (button->GetIDCode() != toggleKey) {
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
			case 0x13: name = "r"; break;
			case 0x1E: name = "a"; break;
			case 0x1F: name = "s"; break;
			case 0x20: name = "d"; break;
			case 0x21: name = "f"; break;
			case 0x2D: name = "x"; break;
			case 0x2C: name = "z"; break;
			case 0x2E: name = "c"; break;
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

void CharacterMenu::Prewarm()
{
	// The first open after starting the game froze for about two seconds
	// before the page showed: the page had never been built. Give it the
	// player's data now, while it is hidden, so it builds then.
	FreeToggleKey();  // again: loading may have read the key map anew
	if (prisma && !isOpen) {
		logger::info("Prewarm: page built ahead of the first open");
		PushSnapshot();
	}
}

void CharacterMenu::Install()
{
	ReadToggleKey();
	FreeToggleKey();
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

	logger::info("Character screen installed, toggle key {} (scan code {:#x})", toggleKeyName, toggleKey);
}
