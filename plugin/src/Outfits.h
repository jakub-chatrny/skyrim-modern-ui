#pragma once

// Four equipment sets. One is active: it is, by definition, whatever the player
// is wearing right now. The other three are stored sets of form ids. Switching
// stores the current gear into the outgoing outfit and puts on the incoming one.
// An outfit that was never used starts as a copy of what is worn at that moment,
// so switching to it changes nothing until gear is changed.
//
// Stored in the SKSE co-save, so outfits belong to a save game.
namespace Outfits
{
	inline constexpr std::size_t COUNT = 4;

	// Call once from SKSEPluginLoad. Registers the co-save callbacks.
	void Install();

	// Main thread only. Returns false for a bad index or the already-active outfit.
	bool Switch(std::size_t a_index);

	// `Outfits` in shared/schema/bridge.schema.json.
	nlohmann::json State();
}
