#pragma once

// The Quests tab: the player's journal as JSON, and its two actions.
// Main thread only.
namespace Quests
{
	// Every quest the journal shows, as `QuestLog` in shared/schema/bridge.schema.json:
	// active quests with their shown objectives, journal entries and target,
	// and completed quests.
	[[nodiscard]] nlohmann::json Build();

	// Tracks or untracks a quest (its compass markers), the way the journal's
	// own button does: through the quest's Papyrus SetActive. Asynchronous.
	bool ToggleTracked(RE::FormID a_quest);
}
