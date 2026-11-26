#include "PCH.h"

#include "Quests.h"

#include <map>
#include <numbers>
#include <unordered_set>

namespace
{
	using json = nlohmann::json;
	using QuestType = RE::QUEST_DATA::Type;
	using ObjectiveState = RE::QUEST_OBJECTIVE_STATE;

	// Ids the page groups and colours quests by.
	const char* TypeId(QuestType a_type)
	{
		switch (a_type) {
		case QuestType::kMainQuest: return "main";
		case QuestType::kMagesGuild: return "college";
		case QuestType::kThievesGuild: return "thieves";
		case QuestType::kDarkBrotherhood: return "darkBrotherhood";
		case QuestType::kCompanionsQuest: return "companions";
		case QuestType::kDaedric: return "daedric";
		case QuestType::kSideQuest: return "side";
		case QuestType::kCivilWar: return "civilWar";
		case QuestType::kDLC01_Vampire: return "dawnguard";
		case QuestType::kDLC02_Dragonborn: return "dragonborn";
		default: return "misc";  // kMiscellaneous, and kNone, which the journal shows there too
		}
	}

	RE::TESGlobal* FindGlobal(std::string_view a_editorID)
	{
		auto* data = RE::TESDataHandler::GetSingleton();
		if (!data) {
			return nullptr;
		}
		for (auto* global : data->GetFormArray<RE::TESGlobal>()) {
			const char* id = global ? global->GetFormEditorID() : nullptr;
			if (id && _stricmp(id, std::string(a_editorID).c_str()) == 0) {
				return global;
			}
		}
		return nullptr;
	}

	// The reference an alias of the quest points at now, by alias name.
	RE::TESObjectREFR* AliasRef(RE::TESQuest* a_quest, std::string_view a_name)
	{
		for (auto* alias : a_quest->aliases) {
			if (!alias || _stricmp(alias->aliasName.c_str(), std::string(a_name).c_str()) != 0) {
				continue;
			}
			const auto it = a_quest->refAliasMap.find(alias->aliasID);
			if (it != a_quest->refAliasMap.end()) {
				if (auto ref = it->second.get()) {
					return ref.get();
				}
			}
		}
		return nullptr;
	}

	// Journal and objective texts name things through tags: <Alias=Lucan>,
	// <Alias.ShortName=Lucan>, <Global=Reward>. Filled in from the quest as it
	// is now; a tag that cannot be filled keeps its alias name.
	std::string FillTags(std::string_view a_text, RE::TESQuest* a_quest)
	{
		std::string out;
		for (std::size_t i = 0; i < a_text.size();) {
			if (a_text[i] != '<') {
				out += a_text[i++];
				continue;
			}
			const auto end = a_text.find('>', i);
			if (end == std::string_view::npos) {
				out += a_text.substr(i);
				break;
			}
			const auto tag = a_text.substr(i + 1, end - i - 1);
			const auto equals = tag.find('=');
			if (equals != std::string_view::npos) {
				const auto kind = tag.substr(0, equals);
				const auto name = tag.substr(equals + 1);
				if (kind.starts_with("Alias")) {
					auto* ref = AliasRef(a_quest, name);
					const char* display = ref ? ref->GetDisplayFullName() : nullptr;
					out += display && *display ? std::string(display) : std::string(name);
				} else if (kind == "Global") {
					if (auto* global = FindGlobal(name)) {
						out += std::format("{}", static_cast<int>(global->value));
					}
				}
			}
			i = end + 1;
		}
		return out;
	}

	const char* Direction(float a_east, float a_north)
	{
		static constexpr const char* NAMES[] = { "North", "North-east", "East", "South-east", "South", "South-west", "West",
			"North-west" };
		const float degrees = std::atan2(a_east, a_north) * 180.0f / std::numbers::pi_v<float>;
		const int   sector = static_cast<int>(std::lround((degrees < 0.0f ? degrees + 360.0f : degrees) / 45.0f)) % 8;
		return NAMES[sector];
	}

	// Where an objective points: the target's name, where it is, and how far
	// and which way from the player (when both are in the same world or cell;
	// the compass's tracking reference walks through doors for us).
	json Target(RE::TESQuest* a_quest, RE::BGSQuestObjective* a_objective, RE::PlayerCharacter* a_player)
	{
		for (std::uint32_t i = 0; a_objective->targets && i < a_objective->numTargets; ++i) {
			auto* target = a_objective->targets[i];
			if (!target) {
				continue;
			}
			RE::ObjectRefHandle handle;
			target->GetTargetRef(handle, false, a_quest);
			const auto ref = handle.get();
			if (!ref) {
				continue;
			}

			json out = { { "name", ref->GetDisplayFullName() ? ref->GetDisplayFullName() : "" } };
			auto* location = ref->GetCurrentLocation();
			if (location) {
				out["place"] = location->GetName();
				std::vector<std::string> regions;
				for (auto* up = location->parentLoc; up && regions.size() < 2; up = up->parentLoc) {
					if (up->GetName() && *up->GetName()) {
						regions.emplace_back(up->GetName());
					}
				}
				if (!regions.empty()) {
					out["region"] = regions.size() == 1 ? regions[0] : regions[0] + "  ·  " + regions[1];
				}
			} else if (auto* cell = ref->GetParentCell(); cell && cell->GetFullName() && *cell->GetFullName()) {
				out["place"] = cell->GetFullName();
			}

			RE::ObjectRefHandle trackingHandle;
			target->GetTrackingRef(trackingHandle, a_quest);
			auto tracking = trackingHandle.get();
			if (!tracking) {
				tracking = ref;
			}
			const bool sameWorld = tracking->GetWorldspace() && tracking->GetWorldspace() == a_player->GetWorldspace();
			const bool sameCell = !tracking->GetWorldspace() && tracking->GetParentCell() == a_player->GetParentCell();
			if (sameWorld || sameCell) {
				const auto from = a_player->GetPosition();
				const auto to = tracking->GetPosition();
				// A game unit is 1.428 cm.
				out["metres"] = std::round(from.GetDistance(to) * 0.01428f);
				out["direction"] = Direction(to.x - from.x, to.y - from.y);
			}
			return out;
		}
		return nullptr;
	}

	struct Entry
	{
		RE::TESQuest*                                     quest = nullptr;
		std::vector<std::pair<std::uint16_t, json>>       objectives;  // by objective index
		std::vector<std::pair<std::uint16_t, std::string>> journal;    // by stage
		std::unordered_set<const RE::BGSQuestObjective*>  seen;
	};
}

json Quests::Build()
{
	auto* player = RE::PlayerCharacter::GetSingleton();
	std::map<RE::TESQuest*, Entry> entries;

	// The objectives the journal shows, from the player's own list.
	for (const auto& instance : player->GetPlayerRuntimeData().objectives) {
		auto* objective = instance.Objective;
		auto* quest = objective ? objective->ownerQuest : nullptr;
		if (!quest) {
			continue;
		}
		const auto state = instance.InstanceState;
		const char* stateId = state == ObjectiveState::kDisplayed          ? "active" :
		                      state == ObjectiveState::kCompletedDisplayed ? "completed" :
		                      state == ObjectiveState::kFailedDisplayed    ? "failed" :
		                                                                     nullptr;
		if (!stateId) {
			continue;  // dormant, or completed / failed without being shown
		}
		auto& entry = entries[quest];
		entry.quest = quest;
		if (!entry.seen.insert(objective).second) {
			continue;
		}
		json row = { { "text", FillTags(objective->displayText.c_str(), quest) }, { "state", stateId } };
		if (state == ObjectiveState::kDisplayed) {
			if (auto target = Target(quest, objective, player); !target.is_null()) {
				row["target"] = std::move(target);
			}
		}
		entry.objectives.emplace_back(objective->index, std::move(row));
	}

	// Journal entries: the log of every stage the player has reached.
	for (auto* item : player->GetQuestLog()) {
		auto* quest = item ? item->owner : nullptr;
		if (!quest || !item->hasLogEntry) {
			continue;
		}
		const char* text = item->GetLogEntry(quest);
		if (!text || !*text) {
			continue;
		}
		auto& entry = entries[quest];
		entry.quest = quest;
		const auto stage = item->owningStage ? item->owningStage->data.index : std::uint16_t{ 0 };
		entry.journal.emplace_back(stage, FillTags(text, quest));
	}

	json quests = json::array();
	for (auto& [quest, entry] : entries) {
		const char* name = quest->GetName();
		const char* type = TypeId(quest->GetType());
		if ((!name || !*name) && std::string_view(type) != "misc") {
			continue;
		}
		// Newest first: higher objective indices and later stages come later in a quest.
		std::ranges::sort(entry.objectives, [](const auto& a, const auto& b) {
			const bool aActive = a.second["state"] == "active";
			const bool bActive = b.second["state"] == "active";
			return aActive != bActive ? aActive : a.first > b.first;
		});
		std::ranges::stable_sort(entry.journal, [](const auto& a, const auto& b) { return a.first > b.first; });

		json objectives = json::array();
		for (auto& [index, row] : entry.objectives) {
			objectives.push_back(std::move(row));
		}
		json journal = json::array();
		for (auto& [stage, text] : entry.journal) {
			if (journal.empty() || journal.back() != text) {
				journal.push_back(std::move(text));
			}
		}
		quests.push_back({
			{ "id", std::format("0x{:08X}", quest->GetFormID()) },
			{ "name", name ? name : "" },
			{ "type", type },
			{ "tracked", quest->IsActive() },
			{ "completed", quest->IsCompleted() },
			{ "objectives", std::move(objectives) },
			{ "journal", std::move(journal) },
		});
	}
	return { { "quests", std::move(quests) } };
}

bool Quests::ToggleTracked(RE::FormID a_quest)
{
	auto* quest = RE::TESForm::LookupByID<RE::TESQuest>(a_quest);
	auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
	if (!quest || !vm) {
		logger::info("Track {:#010x}: not a quest", a_quest);
		return false;
	}
	const bool track = !quest->IsActive();
	auto*      policy = vm->GetObjectHandlePolicy();
	const auto handle = policy ? policy->GetHandleForObject(static_cast<RE::VMTypeID>(RE::FormType::Quest), quest) : 0;
	if (!handle) {
		logger::warn("Track {:#010x}: no script handle", a_quest);
		return false;
	}
	RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
	auto* args = RE::MakeFunctionArguments(static_cast<bool>(track));
	const bool sent = vm->DispatchMethodCall(handle, "Quest", "SetActive", args, callback);
	logger::info("Track: {} ({:#010x}) {}{}", quest->GetName(), a_quest, track ? "tracked" : "untracked", sent ? "" : " (call failed)");
	return sent;
}
