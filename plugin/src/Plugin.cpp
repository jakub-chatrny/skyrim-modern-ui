#include "PCH.h"

#include "CharacterMenu.h"

namespace
{
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
		spdlog::set_pattern("[%H:%M:%S.%e] [%l] [%s:%#] %v");
		spdlog::set_level(spdlog::level::trace);
		spdlog::flush_on(spdlog::level::trace);
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SetupLog();
	logger::info("SkyrimModernUI loaded");

	const auto* messaging = SKSE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
			if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
				CharacterMenu::Install();
			}
		})) {
		logger::error("Failed to register SKSE messaging listener");
		return false;
	}

	return true;
}
