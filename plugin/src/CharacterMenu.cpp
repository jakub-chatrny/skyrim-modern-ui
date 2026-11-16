#include "PCH.h"

#include "CharacterMenu.h"

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
		// dump() twice: the second turns the JSON text into a JS string literal.
		const auto payload = nlohmann::json(Snapshot::Build().dump()).dump();
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
		prisma->Show(view);
		if (!prisma->Focus(view, true)) {  // pauseGame = true
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
	}

	// Opening and closing in quick succession leaves Prisma's cursor menu (FocusMenu) open
	// with the view hidden: its close request is dropped while its open request is still
	// queued, and the player is then stuck in menu mode. Never toggle faster than this.
	constexpr auto TOGGLE_COOLDOWN = 400ms;

	void Toggle(const char* a_reason)
	{
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
			} else {
				// TODO(phase 3): equip / unequip.  TODO(phase 5): switchOutfit.
				logger::info("Command not implemented yet: {}", text);
			}
		});
	}

	class HotkeySink : public RE::BSTEventSink<RE::InputEvent*>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events, RE::BSTEventSource<RE::InputEvent*>*) override
		{
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
	prisma->RegisterJSListener(view, "smuiCommand", OnCommand);
	prisma->Hide(view);

	static HotkeySink sink;
	RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&sink);

	logger::info("Character screen installed, toggle key scan code {:#x}", TOGGLE_KEY);
}
