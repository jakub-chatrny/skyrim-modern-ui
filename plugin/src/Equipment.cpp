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

	bool IsTwoHanded(const RE::TESForm* a_form)
	{
		if (const auto* weapon = a_form->As<RE::TESObjectWEAP>()) {
			return weapon->IsTwoHandedSword() || weapon->IsTwoHandedAxe() || weapon->IsBow() || weapon->IsCrossbow();
		}
		if (const auto* spell = a_form->As<RE::SpellItem>()) {
			return spell->IsTwoHanded();
		}
		return false;
	}

	// Spells the player knows that can be held in this hand: real spells (not
	// powers, shouts or abilities), cast from a hand. Two-handed spells (rituals)
	// are offered for the right hand only.
	std::vector<RE::SpellItem*> HandSpells(RE::PlayerCharacter* a_player, bool a_left)
	{
		const auto* objects = RE::BGSDefaultObjectManager::GetSingleton();
		const auto* either = objects->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECT::kEitherHandEquip);
		const auto* right = HandSlot(false);
		const auto* left = HandSlot(true);

		std::vector<RE::SpellItem*> result;
		const auto consider = [&](RE::SpellItem* a_spell) {
			if (!a_spell || a_spell->GetSpellType() != RE::MagicSystem::SpellType::kSpell ||
				a_spell->GetCastingType() == RE::MagicSystem::CastingType::kConstantEffect) {
				return;
			}
			const auto* slot = a_spell->GetEquipSlot();
			const bool  fits = slot == either || slot == (a_left ? left : right) ||
			                  (!a_left && a_spell->IsTwoHanded());
			if (fits && (!a_left || !a_spell->IsTwoHanded()) && std::ranges::find(result, a_spell) == result.end()) {
				result.push_back(a_spell);
			}
		};
		if (auto* base = a_player->GetActorBase()) {
			if (auto* list = base->GetSpellList(); list && list->spells) {
				for (std::uint32_t i = 0; i < list->numSpells; ++i) {
					consider(list->spells[i]);
				}
			}
		}
		for (auto* spell : a_player->GetActorRuntimeData().addedSpells) {
			consider(spell);
		}
		return result;
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
		// Two-handed weapons and spells are reported for both hands; show them
		// once. The same one-handed spell (or two daggers) in each hand stays.
		if (left && form == player->GetEquippedObject(false) && IsTwoHanded(form)) {
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
	if (auto* spell = item ? item.form->As<RE::SpellItem>() : nullptr) {
		// What Papyrus' UnequipSpell does: the spell leaves the hand(s) holding it.
		RE::PlayerCharacter::GetSingleton()->DeselectSpell(spell);
		logger::info("Unequip {}: spell {} ({:#010x})", SLOT_NAMES[static_cast<std::size_t>(a_slot)], spell->GetName(),
			spell->GetFormID());
		return true;
	}
	if (!object) {
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

	// Spells for the hands, after the items.
	if (a_slot == Slot::kLeftHand || a_slot == Slot::kRightHand) {
		std::vector<Item> spells;
		for (auto* spell : HandSpells(player, a_slot == Slot::kLeftHand)) {
			spells.push_back({ spell, nullptr, 1 });
		}
		std::ranges::sort(spells, [&](const Item& a_lhs, const Item& a_rhs) { return name(a_lhs) < name(a_rhs); });
		result.items.insert(result.items.end(), spells.begin(), spells.end());
	}
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

namespace
{
	// The game's own inventory entry for a form (not a copy), and the extra
	// data list of its worn copy, else its first one, else none.
	std::pair<RE::InventoryEntryData*, RE::ExtraDataList*> LiveEntry(RE::PlayerCharacter* a_player, RE::FormID a_formID)
	{
		auto* changes = a_player->GetInventoryChanges();
		if (!changes || !changes->entryList) {
			return {};
		}
		for (auto* entry : *changes->entryList) {
			if (!entry || !entry->object || entry->object->GetFormID() != a_formID) {
				continue;
			}
			RE::ExtraDataList* chosen = nullptr;
			if (entry->extraLists) {
				for (auto* list : *entry->extraLists) {
					if (!list) {
						continue;
					}
					if (!chosen) {
						chosen = list;
					}
					if (list->HasType(RE::ExtraDataType::kWorn) || list->HasType(RE::ExtraDataType::kWornLeft)) {
						chosen = list;
						break;
					}
				}
			}
			return { entry, chosen };
		}
		return {};
	}
}

namespace
{
	// Drops wait for the screen to close. Dropped while it is open (world
	// hidden, a worn copy still attached), the item crashed the game or never
	// showed up; afterwards it lands at the player's feet like any drop.
	struct PendingDrop
	{
		RE::FormID formID = 0;
		int        attempts = 0;
	};
	std::vector<PendingDrop> pendingDrops;
	int                      dropWait = 0;  // frames left after the screen closed
	constexpr int            DROP_DELAY_FRAMES = 15;
	constexpr int            DROP_ATTEMPTS = 3;

	bool IsWorn(const RE::ExtraDataList* a_list)
	{
		return a_list && (a_list->HasType(RE::ExtraDataType::kWorn) || a_list->HasType(RE::ExtraDataType::kWornLeft));
	}

	// How many the player has (the entry's own count is only the change from
	// the base container).
	std::int32_t Count(RE::PlayerCharacter* a_player, RE::TESBoundObject* a_object)
	{
		const auto counts = a_player->GetInventoryCounts([&](RE::TESBoundObject& a_candidate) { return &a_candidate == a_object; });
		const auto it = counts.find(a_object);
		return it != counts.end() ? it->second : 0;
	}

	// The extra data list of a copy that is not worn, or null for a plain copy;
	// false when every copy is still worn. No entry means plain copies only.
	bool UnwornCopy(RE::InventoryEntryData* a_entry, std::int32_t a_count, RE::ExtraDataList*& a_list)
	{
		a_list = nullptr;
		std::int32_t withLists = 0;
		if (a_entry && a_entry->extraLists) {
			for (auto* list : *a_entry->extraLists) {
				if (!list) {
					continue;
				}
				withLists += list->GetCount();
				if (!IsWorn(list) && !a_list) {
					a_list = list;
				}
			}
		}
		// Copies without an extra data list are never worn.
		return a_list || a_count > withLists;
	}

	std::int32_t Queued(RE::FormID a_formID)
	{
		return static_cast<std::int32_t>(std::ranges::count_if(pendingDrops, [&](const PendingDrop& a_drop) { return a_drop.formID == a_formID; }));
	}
}

bool Equipment::Drop(RE::FormID a_formID)
{
	auto* player = RE::PlayerCharacter::GetSingleton();
	auto* object = RE::TESForm::LookupByID<RE::TESBoundObject>(a_formID);
	if (!object || object->Is(RE::FormType::Spell)) {
		logger::info("Drop {:#010x}: not an item", a_formID);
		return false;
	}
	// Items from the character's own starting inventory have no entry unless
	// something about them changed (worn, enchanted, ...), so count instead.
	const auto count = Count(player, object);
	if (count <= 0) {
		logger::info("Drop {:#010x}: not in the inventory", a_formID);
		return false;
	}
	const auto [entry, list] = LiveEntry(player, a_formID);
	// The game refuses to drop quest items too.
	if (entry && entry->IsQuestObject()) {
		logger::info("Drop {:#010x}: quest item, kept", a_formID);
		return false;
	}
	if (Queued(a_formID) >= count) {
		logger::info("Drop {:#010x}: all {} already queued", a_formID, count);
		return false;
	}
	// When the next copy to go is worn, take it off now; the drop itself waits.
	RE::ExtraDataList* unworn = nullptr;
	if (!UnwornCopy(entry, count - Queued(a_formID), unworn) && IsWorn(list)) {
		RE::ActorEquipManager::GetSingleton()->UnequipObject(player, object, list, 1, nullptr, false, false, true, true);
		logger::info("Drop: {} ({:#010x}) taken off", object->GetName(), a_formID);
	}
	pendingDrops.push_back({ a_formID, 0 });
	logger::info("Drop: {} ({:#010x}) queued until the screen closes", object->GetName(), a_formID);
	return true;
}

bool Equipment::TickDrops(bool a_screenOpen)
{
	if (a_screenOpen || pendingDrops.empty()) {
		dropWait = DROP_DELAY_FRAMES;
		return false;
	}
	if (--dropWait > 0) {
		return false;
	}

	auto* player = RE::PlayerCharacter::GetSingleton();
	std::vector<PendingDrop> retry;
	for (auto& drop : pendingDrops) {
		auto* object = RE::TESForm::LookupByID<RE::TESBoundObject>(drop.formID);
		const auto count = player && object ? Count(player, object) : 0;
		if (count <= 0) {
			logger::info("Drop {:#010x}: no longer in the inventory", drop.formID);
			continue;
		}
		// No entry is fine: a plain copy from the starting inventory.
		const auto [entry, list] = LiveEntry(player, drop.formID);
		RE::ExtraDataList* unworn = nullptr;
		if (!UnwornCopy(entry, count, unworn)) {
			if (++drop.attempts < DROP_ATTEMPTS) {
				RE::ActorEquipManager::GetSingleton()->UnequipObject(player, object, list, 1, nullptr, false, false, true, true);
				retry.push_back(drop);
			} else {
				logger::warn("Drop {:#010x}: still worn after {} tries, kept", drop.formID, DROP_ATTEMPTS);
			}
			continue;
		}
		player->RemoveItem(object, 1, RE::ITEM_REMOVE_REASON::kDropping, unworn, nullptr);
		logger::info("Drop: {} ({:#010x}) dropped", object->GetName(), drop.formID);
	}
	pendingDrops = std::move(retry);
	dropWait = DROP_DELAY_FRAMES;
	return true;
}

std::vector<RE::FormID> Equipment::QueuedDrops()
{
	std::vector<RE::FormID> ids;
	for (const auto& drop : pendingDrops) {
		ids.push_back(drop.formID);
	}
	return ids;
}

bool Equipment::ToggleFavorite(RE::FormID a_formID)
{
	auto* player = RE::PlayerCharacter::GetSingleton();
	auto* changes = player->GetInventoryChanges();
	const auto [entry, list] = LiveEntry(player, a_formID);
	if (!changes || !entry) {
		logger::info("Favourite {:#010x}: not in the inventory", a_formID);
		return false;
	}
	const bool was = entry->IsFavorited();
	if (was) {
		changes->RemoveFavorite(entry, list);
	} else {
		changes->SetFavorite(entry, list);
	}
	logger::info("Favourite: {} ({:#010x}) {}", entry->object->GetName(), a_formID, was ? "cleared" : "set");
	return true;
}

bool Equipment::Use(RE::FormID a_formID)
{
	auto* player = RE::PlayerCharacter::GetSingleton();
	auto* object = RE::TESForm::LookupByID<RE::TESBoundObject>(a_formID);
	const auto count = object ? Count(player, object) : 0;
	if (count <= 0) {
		logger::info("Use {:#010x}: not in the inventory", a_formID);
		return false;
	}

	using Type = RE::FormType;
	switch (object->GetFormType()) {
	case Type::Weapon:
	case Type::Armor:
	case Type::Ammo:
	case Type::Light:
	case Type::Scroll:
		{
			// Worn: take it off. Otherwise put it on, as the Character screen does.
			const auto [entry, list] = LiveEntry(player, a_formID);
			if (IsWorn(list)) {
				const auto amount = object->IsAmmo() ? static_cast<std::uint32_t>(count) : 1u;
				RE::ActorEquipManager::GetSingleton()->UnequipObject(player, object, list, amount, nullptr, false, false, true, true);
				logger::info("Use: {} ({:#010x}) taken off", object->GetName(), a_formID);
				return true;
			}
			return Equip(a_formID, std::nullopt);
		}
	case Type::AlchemyItem:
	case Type::Ingredient:
		// Equipping a potion, food or ingredient is how the game drinks or eats it.
		RE::ActorEquipManager::GetSingleton()->EquipObject(player, object, nullptr, 1);
		logger::info("Use: {} ({:#010x}) consumed", object->GetName(), a_formID);
		return true;
	default:
		logger::info("Use: {} ({:#010x}) cannot be used from this screen", object->GetName(), a_formID);
		return false;
	}
}
