#include "PCH.h"

#include "Equipment.h"

// <wingdi.h> defines GetObject as a macro, which breaks BGSDefaultObjectManager::GetObject.
#undef GetObject

namespace
{
	using Biped = RE::BGSBipedObjectForm::BipedObjectSlot;
	using Equipment::Slot;

	// Which screen slot a worn piece of armor belongs to. Checked in this order,
	// so a robe that also covers the head counts as body. Slots 44-47 and 55 are
	// not used by the base game; cloak and mask follow the common mod conventions
	// (cloaks on 46/47, face masks on 44/55).
	struct ArmorRule
	{
		Slot                                  slot;
		std::initializer_list<Biped>          parts;
	};
	const ArmorRule ARMOR_RULES[] = {
		{ Slot::kLeftHand, { Biped::kShield } },
		{ Slot::kBody, { Biped::kBody } },
		{ Slot::kHead, { Biped::kHair, Biped::kHead, Biped::kCirclet } },
		{ Slot::kHands, { Biped::kHands } },
		{ Slot::kFeet, { Biped::kFeet } },
		{ Slot::kNeck, { Biped::kAmulet } },
		{ Slot::kRing, { Biped::kRing } },
		{ Slot::kCloak, { Biped::kModChestPrimary, Biped::kModBack } },
		{ Slot::kMask, { Biped::kModMouth, Biped::kModFaceJewelry } },
	};

	std::optional<Slot> SlotForArmor(const RE::TESObjectARMO* a_armor)
	{
		for (const auto& rule : ARMOR_RULES) {
			for (const auto part : rule.parts) {
				if (a_armor->HasPartOf(part)) {
					return rule.slot;
				}
			}
		}
		return std::nullopt;
	}

	bool IsRanged(const RE::TESForm* a_form)
	{
		const auto* weapon = a_form ? a_form->As<RE::TESObjectWEAP>() : nullptr;
		return weapon && (weapon->IsBow() || weapon->IsCrossbow());
	}

	const RE::BGSEquipSlot* HandSlot(bool a_left)
	{
		const auto* objects = RE::BGSDefaultObjectManager::GetSingleton();
		return objects->GetObject<RE::BGSEquipSlot>(
			a_left ? RE::DEFAULT_OBJECT::kLeftHandEquip : RE::DEFAULT_OBJECT::kRightHandEquip);
	}
}

std::optional<Equipment::Slot> Equipment::SlotFromName(std::string_view a_name)
{
	for (std::size_t i = 0; i < SLOT_NAMES.size(); ++i) {
		if (SLOT_NAMES[i] == a_name) {
			return static_cast<Slot>(i);
		}
	}
	return std::nullopt;
}

Equipment::Loadout Equipment::Read()
{
	Loadout loadout;
	auto*   player = RE::PlayerCharacter::GetSingleton();
	auto&   slots = loadout.slots;
	const auto at = [&](Slot a_slot) -> Item& { return slots[static_cast<std::size_t>(a_slot)]; };

	loadout.inventory = player->GetInventory([](RE::TESBoundObject& a_object) {
		return a_object.IsArmor() || a_object.IsWeapon() || a_object.IsAmmo() || a_object.Is(RE::FormType::Light);
	});

	// Worn armor, by biped slot.
	for (auto& [object, data] : loadout.inventory) {
		auto& [count, entry] = data;
		const auto* armor = object->As<RE::TESObjectARMO>();
		if (!armor || !entry || !entry->IsWorn()) {
			continue;
		}
		if (const auto slot = SlotForArmor(armor); slot && !at(*slot)) {
			at(*slot) = { object, entry.get(), count };
		}
	}

	// Hands. A bow or crossbow goes to the ranged slot instead of the right hand:
	// the game has no separate ranged slot, the design does.
	for (const bool left : { false, true }) {
		auto* form = player->GetEquippedObject(left);
		if (!form) {
			continue;
		}
		const Slot slot = left ? Slot::kLeftHand : (IsRanged(form) ? Slot::kRanged : Slot::kRightHand);
		if (at(slot)) {
			continue;  // a shield already claimed the left hand
		}
		// Two-handed weapons are reported for both hands; show them once.
		if (left && form == player->GetEquippedObject(false)) {
			continue;
		}
		Item item{ form, player->GetEquippedEntryData(left), 1 };
		at(slot) = item;
	}

	if (auto* ammo = player->GetCurrentAmmo()) {
		Item item{ ammo, nullptr, 0 };
		if (const auto it = loadout.inventory.find(ammo); it != loadout.inventory.end()) {
			item.count = it->second.first;
			item.entry = it->second.second.get();
		}
		at(Slot::kAmmo) = item;
	}

	return loadout;
}

bool Equipment::Unequip(Slot a_slot)
{
	const auto  loadout = Read();
	const auto& item = loadout[a_slot];
	auto*       object = item ? item.form->As<RE::TESBoundObject>() : nullptr;
	if (!object) {
		// Empty slot, or a spell (spells are not TESBoundObjects; not handled yet).
		logger::info("Unequip {}: nothing to unequip", SLOT_NAMES[static_cast<std::size_t>(a_slot)]);
		return false;
	}

	const RE::BGSEquipSlot* hand = nullptr;
	if (a_slot == Slot::kRightHand || a_slot == Slot::kRanged) {
		hand = HandSlot(false);
	} else if (a_slot == Slot::kLeftHand && !object->IsArmor()) {
		hand = HandSlot(true);
	}

	auto* player = RE::PlayerCharacter::GetSingleton();
	const auto count = a_slot == Slot::kAmmo ? static_cast<std::uint32_t>(std::max(item.count, 1)) : 1u;
	RE::ActorEquipManager::GetSingleton()->UnequipObject(player, object, nullptr, count, hand);
	logger::info("Unequip {}: {} ({:#010x})", SLOT_NAMES[static_cast<std::size_t>(a_slot)], object->GetName(),
		object->GetFormID());
	return true;
}

bool Equipment::Equip(RE::FormID a_formID, std::optional<Slot> a_slot)
{
	auto* player = RE::PlayerCharacter::GetSingleton();

	// Spells are not inventory items; they go straight into a hand.
	if (auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(a_formID)) {
		if (!player->HasSpell(spell)) {
			logger::warn("Equip: player does not know {} ({:#010x})", spell->GetName(), a_formID);
			return false;
		}
		RE::ActorEquipManager::GetSingleton()->EquipSpell(player, spell, HandSlot(a_slot == Slot::kLeftHand));
		logger::info("Equip: spell {} ({:#010x})", spell->GetName(), a_formID);
		return true;
	}

	auto* object = RE::TESForm::LookupByID<RE::TESBoundObject>(a_formID);
	if (!object) {
		logger::warn("Equip: form {:#010x} is not an item", a_formID);
		return false;
	}

	const auto counts = player->GetInventoryCounts();
	const auto owned = counts.find(object);
	if (owned == counts.end() || owned->second <= 0) {
		logger::warn("Equip: player does not have {} ({:#010x})", object->GetName(), a_formID);
		return false;
	}

	// Weapons name their hand; armor, shields, torches and ammo find their own place.
	const RE::BGSEquipSlot* hand = nullptr;
	if (object->IsWeapon()) {
		hand = HandSlot(a_slot == Slot::kLeftHand);
	}

	// Ammo is equipped as a whole stack.
	const auto count = object->IsAmmo() ? static_cast<std::uint32_t>(owned->second) : 1u;
	RE::ActorEquipManager::GetSingleton()->EquipObject(player, object, nullptr, count, hand);
	logger::info("Equip: {} ({:#010x})", object->GetName(), a_formID);
	return true;
}

Equipment::FormSet Equipment::CurrentForms()
{
	const auto loadout = Read();
	FormSet    forms{};
	for (std::size_t i = 0; i < forms.size(); ++i) {
		forms[i] = loadout.slots[i] ? loadout.slots[i].form->GetFormID() : 0;
	}
	return forms;
}

namespace
{
	// Can this inventory object go into this slot?
	bool Fits(RE::TESBoundObject* a_object, Slot a_slot)
	{
		if (!a_object->GetPlayable()) {
			return false;
		}
		if (const auto* armor = a_object->As<RE::TESObjectARMO>()) {
			return SlotForArmor(armor) == a_slot;
		}
		if (const auto* weapon = a_object->As<RE::TESObjectWEAP>()) {
			const bool ranged = weapon->IsBow() || weapon->IsCrossbow();
			const bool twoHanded = weapon->IsTwoHandedSword() || weapon->IsTwoHandedAxe();
			switch (a_slot) {
			case Slot::kRanged:
				return ranged;
			case Slot::kRightHand:
				return !ranged && weapon->GetWeaponType() != RE::WEAPON_TYPE::kHandToHandMelee;
			case Slot::kLeftHand:
				return !ranged && !twoHanded && weapon->GetWeaponType() != RE::WEAPON_TYPE::kHandToHandMelee;
			default:
				return false;
			}
		}
		if (a_object->IsAmmo()) {
			return a_slot == Slot::kAmmo;
		}
		if (const auto* light = a_object->As<RE::TESObjectLIGH>()) {
			return a_slot == Slot::kLeftHand && light->CanBeCarried();
		}
		return false;
	}
}

Equipment::Candidates Equipment::ListFor(Slot a_slot)
{
	Candidates result;
	auto*      player = RE::PlayerCharacter::GetSingleton();

	result.equipped = Read()[a_slot].form;
	result.inventory = player->GetInventory([&](RE::TESBoundObject& a_object) { return Fits(std::addressof(a_object), a_slot); });
	for (auto& [object, data] : result.inventory) {
		if (data.first > 0) {
			result.items.push_back({ object, data.second.get(), data.first });
		}
	}

	const auto name = [](const Item& a_item) {
		const char* text = a_item.entry ? a_item.entry->GetDisplayName() : a_item.form->GetName();
		return std::string_view(text ? text : "");
	};
	std::ranges::sort(result.items, [&](const Item& a_lhs, const Item& a_rhs) { return name(a_lhs) < name(a_rhs); });
	return result;
}

std::size_t Equipment::Apply(const FormSet& a_wanted)
{
	const auto current = CurrentForms();
	auto*      player = RE::PlayerCharacter::GetSingleton();
	const auto counts = player->GetInventoryCounts();

	const auto owns = [&](RE::FormID a_formID) {
		if (const auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(a_formID)) {
			return player->HasSpell(const_cast<RE::SpellItem*>(spell));
		}
		auto*      object = RE::TESForm::LookupByID<RE::TESBoundObject>(a_formID);
		const auto it = object ? counts.find(object) : counts.end();
		return it != counts.end() && it->second > 0;
	};

	std::size_t missing = 0;

	// Take things off first, so a hand or body slot is free before its
	// replacement arrives.
	for (std::size_t i = 0; i < a_wanted.size(); ++i) {
		if (current[i] != 0 && a_wanted[i] == 0) {
			Unequip(static_cast<Slot>(i));
		}
	}
	for (std::size_t i = 0; i < a_wanted.size(); ++i) {
		if (a_wanted[i] == 0 || a_wanted[i] == current[i]) {
			continue;
		}
		if (!owns(a_wanted[i])) {
			++missing;
			logger::info("Apply: {} wants {:#010x}, which the player no longer has", SLOT_NAMES[i], a_wanted[i]);
			continue;
		}
		Equip(a_wanted[i], static_cast<Slot>(i));
	}
	return missing;
}
