#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#pragma warning(push)
#include <nlohmann/json.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#pragma warning(pop)

#include <chrono>
#include <memory>
#include <string>

using namespace std::literals;

namespace logger = SKSE::log;
