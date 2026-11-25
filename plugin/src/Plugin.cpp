#include "PCH.h"

#include "CharacterMenu.h"
#include "Outfits.h"

namespace
{
	// Prisma UI disables exactly these while a view has focus and re-enables
	// them on unfocus (its ViewManager.cpp, kBlockedControlGroups).
	using UEFlag = RE::UserEvents::USER_EVENT_FLAG;
	constexpr UEFlag PRISMA_BLOCKED[] = { UEFlag::kWheelZoom, UEFlag::kLooking, UEFlag::kJumping, UEFlag::kMovement,
		UEFlag::kActivate, UEFlag::kPOVSwitch, UEFlag::kVATS };

	// After a save loads, the player sometimes could not move until the
	// Character screen was opened and closed. Log the control state, and if it
	// is Prisma's block exactly (those groups off, fighting still on, which a
	// quest scene would also turn off), give the controls back.
	void CheckControlsAfterLoad()
	{
		auto* controls = RE::ControlMap::GetSingleton();
		if (!controls) {
			return;
		}
		auto&      enabled = controls->GetRuntimeData().enabledControls;
		const auto bits = enabled.underlying();
		logger::info("After load: enabled control groups {:#06x}", bits);

		const bool allBlocked = std::ranges::all_of(PRISMA_BLOCKED, [&](UEFlag a_flag) { return !enabled.all(a_flag); });
		if (allBlocked && enabled.all(UEFlag::kFighting)) {
			for (const auto flag : PRISMA_BLOCKED) {
				controls->ToggleControls(flag, true, false);
			}
			logger::warn("After load: Prisma UI's control block was still in place; controls re-enabled (now {:#06x})",
				controls->GetRuntimeData().enabledControls.underlying());
		}
	}

	// No debugger under Proton: the log file is the debugger. Flush everything.
	void SetupLog()
	{
		auto logsFolder = logger::log_directory();
		if (!logsFolder) {
			SKSE::stl::report_and_fail("SKSE log_directory not provided, logs can't be written");
		}

		auto logPath = *logsFolder / "SkyrimModernUI.log";
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
		auto log = std::make_shared<spdlog::logger>("global", std::move(sink));
		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%H:%M:%S.%e] [%t] [%l] [%s:%#] %v");  // %t: thread id, to compare with other plugins' logs
		spdlog::set_level(spdlog::level::trace);
		spdlog::flush_on(spdlog::level::trace);
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SetupLog();
	logger::info("SkyrimModernUI loaded");

	Outfits::Install();

	const auto* messaging = SKSE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
			if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
				CharacterMenu::Install();
			} else if (a_msg->type == SKSE::MessagingInterface::kPostLoadGame || a_msg->type == SKSE::MessagingInterface::kNewGame) {
				CheckControlsAfterLoad();
				SKSE::GetTaskInterface()->AddTask([]() { CharacterMenu::Prewarm(); });
			}
		})) {
		logger::error("Failed to register SKSE messaging listener");
		return false;
	}

	return true;
}
