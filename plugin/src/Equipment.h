#pragma once

#include <array>
#include <string_view>
#include <vector>

// What the player has equipped, mapped onto the Character screen's twelve
// slots, plus the equip / unequip commands. Main thread only.
namespace Equipment
{
	enum class Slot : std::size_t
	{
		kHead,
		kBody,
		kHands,
		kFeet,
		kLeftHand,
		kRightHand,
		kMask,
		kCloak,
		kNeck,
		kRing,
		kRanged,
		kAmmo,

		kTotal
	};

	// Names used in the bridge JSON (shared/schema/bridge.schema.json).
	inline constexpr std::array<std::string_view, static_cast<std::size_t>(Slot::kTotal)> SLOT_NAMES{
		"head", "body", "hands", "feet", "leftHand", "rightHand", "mask", "cloak", "neck", "ring", "ranged", "ammo"
	};

	[[nodiscard]] std::optional<Slot> SlotFromName(std::string_view a_name);

	struct Item
	{
		RE::TESForm*            form = nullptr;   // armor, weapon, ammo, light or spell
		RE::InventoryEntryData* entry = nullptr;  // null for spells; owned by Loadout::inventory or the game
		std::int32_t            count = 0;

		explicit operator bool() const noexcept { return form != nullptr; }
	};

	// Snapshot of the player's equipment. Keeps the inventory map alive because
	// the items point into it.
	struct Loadout
	{
		RE::TESObjectREFR::InventoryItemMap                         inventory;
		std::array<Item, static_cast<std::size_t>(Slot::kTotal)> slots;

		[[nodiscard]] const Item& operator[](Slot a_slot) const { return slots[static_cast<std::size_t>(a_slot)]; }
	};

	[[nodiscard]] Loadout Read();

	// Form ids of what is in each slot right now (0 = empty). This is what an outfit stores.
	using FormSet = std::array<RE::FormID, static_cast<std::size_t>(Slot::kTotal)>;
	[[nodiscard]] FormSet CurrentForms();

	// Everything in the player's inventory that could go into a slot, sorted by
	// name. Keeps the inventory map alive for the same reason as Loadout.
	struct Candidates
	{
		RE::TESObjectREFR::InventoryItemMap inventory;
		std::vector<Item>                   items;
		RE::TESForm*                        equipped = nullptr;  // what the slot holds now
	};
	[[nodiscard]] Candidates ListFor(Slot a_slot);

	// Makes the player wear exactly a_wanted: unequips slots that should be
	// empty, equips the rest. Returns how many wanted items the player no longer
	// has; those slots are left as they are.
	std::size_t Apply(const FormSet& a_wanted);

	// Both return false, after logging why, when nothing was done.
	bool Unequip(Slot a_slot);
	bool Equip(RE::FormID a_formID, std::optional<Slot> a_slot);

	// Queues one of the item to be dropped at the player's feet once the screen
	// closes; a worn copy is taken off at once.
	bool Drop(RE::FormID a_formID);
	// Once per frame: carries out queued drops a moment after the screen closed.
	bool TickDrops(bool a_screenOpen);
	// Form ids of the queued drops, one per copy.
	[[nodiscard]] std::vector<RE::FormID> QueuedDrops();
	// Marks the item as a favourite in the game's inventory, or clears the mark.
	bool ToggleFavorite(RE::FormID a_formID);

	// The Inventory tab's Enter: worn gear comes off, other gear goes on,
	// potions, food and ingredients are consumed. Books and the rest: nothing.
	bool Use(RE::FormID a_formID);
}
