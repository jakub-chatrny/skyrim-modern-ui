#include "PCH.h"

#include "Snapshot.h"

#include "Equipment.h"

namespace
{
	using AV = RE::ActorValue;
	using json = nlohmann::json;
	using Equipment::Slot;

	json Gauge(float a_current, float a_max)
	{
		return { { "current", a_current }, { "max", a_max } };
	}

	// current = live value; max = permanent value. Temporary buffs are not
	// included in max.
	json Vital(RE::ActorValueOwner* a_owner, AV a_value)
	{
		return Gauge(a_owner->GetActorValue(a_value), a_owner->GetPermanentActorValue(a_value));
	}

	std::string Number(float a_value, int a_decimals = 0)
	{
		return std::format("{:.{}f}", a_value, a_decimals);
	}

	json Stat(std::string_view a_label, std::string a_value)
	{
		return { { "label", a_label }, { "value", std::move(a_value) } };
	}

	const char* WeaponIcon(const RE::TESObjectWEAP* a_weapon)
	{
		using Type = RE::WEAPON_TYPE;
		switch (a_weapon->GetWeaponType()) {
		case Type::kOneHandSword:
			return "weapon-sword";
		case Type::kOneHandDagger:
			return "weapon-dagger";
		case Type::kOneHandAxe:
			return "weapon-axe";
		case Type::kOneHandMace:
			return "weapon-mace";
		case Type::kTwoHandSword:
			return "weapon-greatsword";
		case Type::kTwoHandAxe:
			return "weapon-battleaxe";
		case Type::kBow:
			return "weapon-bow";
		case Type::kCrossbow:
			return "weapon-crossbow";
		case Type::kStaff:
			return "weapon-staff";
		default:
			return "weapon";
		}
	}

	// One equipped item as the schema's SlotItem, or null for an empty slot.
	json SlotItem(Slot a_slot, const Equipment::Item& a_item)
	{
		if (!a_item) {
			return nullptr;
		}

		auto*       player = RE::PlayerCharacter::GetSingleton();
		auto*       form = a_item.form;
		auto*       entry = a_item.entry;
		const char* name = entry ? entry->GetDisplayName() : form->GetName();  // display name includes "(Fine)" etc.

		std::string type = "armor";
		std::string icon = "armor";
		json        stats = json::array();
		const auto  slotName = Equipment::SLOT_NAMES[static_cast<std::size_t>(a_slot)];

		if (const auto* armor = form->As<RE::TESObjectARMO>()) {
			const bool jewelry = a_slot == Slot::kNeck || a_slot == Slot::kRing;
			if (armor->IsShield()) {
				type = icon = "shield";
			} else if (jewelry) {
				type = "jewelry";
				icon = std::format("jewelry-{}", slotName);
			} else {
				type = armor->IsClothing() ? "clothing" : "armor";
				icon = std::format("{}-{}", type, slotName);
			}
			// Jewelry and clothing have no armor rating worth showing.
			if (const float rating = entry ? player->GetArmorValue(entry) : 0.0f; rating > 0.0f) {
				stats.push_back(Stat("Armor", Number(rating)));
			}
		} else if (const auto* weapon = form->As<RE::TESObjectWEAP>()) {
			type = "weapon";
			icon = WeaponIcon(weapon);
			if (entry) {
				stats.push_back(Stat("Damage", Number(player->GetDamage(entry))));
			}
			stats.push_back(Stat("Speed", Number(weapon->GetSpeed(), 2)));
		} else if (const auto* ammo = form->As<RE::TESAmmo>()) {
			type = "ammo";
			icon = "ammo-arrow";
			stats.push_back(Stat("Damage", Number(ammo->GetRuntimeData().data.damage)));
			stats.push_back(Stat("Count", std::to_string(a_item.count)));
		} else if (const auto* spell = form->As<RE::SpellItem>()) {
			type = icon = "spell";
			stats.push_back(Stat("Cost", Number(spell->CalculateMagickaCost(player))));
		} else if (form->Is(RE::FormType::Light)) {
			type = icon = "torch";
		}

		if (entry && type != "ammo") {
			stats.push_back(Stat("Weight", Number(entry->GetWeight(), 1)));
		}

		return {
			{ "formId", std::format("0x{:08X}", form->GetFormID()) },
			{ "name", name ? name : "" },
			{ "type", type },
			{ "icon", icon },
			{ "stats", std::move(stats) },
		};
	}

	struct SkillInfo
	{
		AV          value;
		const char* id;
		const char* name;
	};
	// Order matches PlayerSkills::Data::Skills (actor values 6..23).
	constexpr SkillInfo SKILLS[] = {
		{ AV::kOneHanded, "OneHanded", "One-Handed" },
		{ AV::kTwoHanded, "TwoHanded", "Two-Handed" },
		{ AV::kArchery, "Archery", "Archery" },
		{ AV::kBlock, "Block", "Block" },
		{ AV::kSmithing, "Smithing", "Smithing" },
		{ AV::kHeavyArmor, "HeavyArmor", "Heavy Armor" },
		{ AV::kLightArmor, "LightArmor", "Light Armor" },
		{ AV::kPickpocket, "Pickpocket", "Pickpocket" },
		{ AV::kLockpicking, "Lockpicking", "Lockpicking" },
		{ AV::kSneak, "Sneak", "Sneak" },
		{ AV::kAlchemy, "Alchemy", "Alchemy" },
		{ AV::kSpeech, "Speech", "Speech" },
		{ AV::kAlteration, "Alteration", "Alteration" },
		{ AV::kConjuration, "Conjuration", "Conjuration" },
		{ AV::kDestruction, "Destruction", "Destruction" },
		{ AV::kIllusion, "Illusion", "Illusion" },
		{ AV::kRestoration, "Restoration", "Restoration" },
		{ AV::kEnchanting, "Enchanting", "Enchanting" },
	};

	float Fraction(float a_value, float a_of)
	{
		return a_of > 0.0f ? std::clamp(a_value / a_of, 0.0f, 1.0f) : 0.0f;
	}

	// The weapon speed multiplier actor values read 0 when nothing modifies them.
	float SpeedMult(RE::ActorValueOwner* a_owner, AV a_value)
	{
		const float mult = a_owner->GetActorValue(a_value);
		return mult > 0.0f ? mult : 1.0f;
	}
}

json Snapshot::Build()
{
	auto* player = RE::PlayerCharacter::GetSingleton();
	auto* av = player->AsActorValueOwner();

	const auto  loadout = Equipment::Read();
	const auto* progress = player->GetInfoRuntimeData().skills ? player->GetInfoRuntimeData().skills->data : nullptr;

	// Equipment
	json equipment = json::object();
	for (std::size_t i = 0; i < Equipment::SLOT_NAMES.size(); ++i) {
		const auto slot = static_cast<Slot>(i);
		equipment[std::string(Equipment::SLOT_NAMES[i])] = SlotItem(slot, loadout[slot]);
	}

	// Skills
	json skills = json::array();
	for (std::size_t i = 0; i < std::size(SKILLS); ++i) {
		json skill = {
			{ "id", SKILLS[i].id },
			{ "name", SKILLS[i].name },
			{ "level", static_cast<int>(av->GetActorValue(SKILLS[i].value)) },
		};
		if (progress) {
			skill["progress"] = Fraction(progress->skills[i].xp, progress->skills[i].levelThreshold);
		}
		skills.push_back(std::move(skill));
	}

	// Damage. Weapon numbers are the ones the game's own menus show (skill,
	// perks and tempering applied). An empty or non-weapon hand reports 0.
	const auto weaponDamage = [&](Slot a_slot) {
		const auto& item = loadout[a_slot];
		return item && item.entry && item.form->IsWeapon() ? player->GetDamage(item.entry) : 0.0f;
	};
	const auto weaponSpeed = [&](Slot a_slot, float a_mult) {
		const auto* weapon = loadout[a_slot] ? loadout[a_slot].form->As<RE::TESObjectWEAP>() : nullptr;
		return weapon ? weapon->GetSpeed() * a_mult : 0.0f;
	};
	const float rightMult = SpeedMult(av, AV::kWeaponSpeedMult);
	float       ranged = weaponDamage(Slot::kRanged);
	if (ranged > 0.0f) {
		if (const auto* ammo = loadout[Slot::kAmmo] ? loadout[Slot::kAmmo].form->As<RE::TESAmmo>() : nullptr) {
			ranged += ammo->GetRuntimeData().data.damage;
		}
	}
	const bool bareHanded = !loadout[Slot::kRightHand] && !loadout[Slot::kRanged];

	const auto resist = [&](AV a_value) { return av->GetActorValue(a_value); };

	return {
		{ "schemaVersion", 1 },
		{ "player",
			{
				{ "name", player->GetDisplayFullName() },
				{ "level", player->GetLevel() },
				{ "xpProgress", progress ? Fraction(progress->xp, progress->levelThreshold) : 0.0f },
				{ "gold", player->GetGoldAmount() },
				{ "carryWeight", Gauge(av->GetActorValue(AV::kInventoryWeight), av->GetActorValue(AV::kCarryWeight)) },
			} },
		{ "vitals",
			{
				{ "health", Vital(av, AV::kHealth) },
				{ "stamina", Vital(av, AV::kStamina) },
				{ "magicka", Vital(av, AV::kMagicka) },
			} },
		{ "resistances",
			{
				{ "armor", resist(AV::kDamageResist) },
				{ "magic", resist(AV::kResistMagic) },
				{ "fire", resist(AV::kResistFire) },
				{ "frost", resist(AV::kResistFrost) },
				{ "shock", resist(AV::kResistShock) },
				{ "poison", resist(AV::kPoisonResist) },
				{ "disease", resist(AV::kResistDisease) },
			} },
		{ "damage",
			{
				{ "rightHand", bareHanded ? av->GetActorValue(AV::kUnarmedDamage) : weaponDamage(Slot::kRightHand) },
				{ "leftHand", weaponDamage(Slot::kLeftHand) },
				{ "ranged", ranged },
				{ "meleeSpeed", weaponSpeed(Slot::kRightHand, rightMult) },
				{ "rangedSpeed", weaponSpeed(Slot::kRanged, rightMult) },
				// Not a single game value. Reported as the Destruction power
				// modifier (percent) until the design says what it should be.
				{ "magicPower", av->GetActorValue(AV::kDestructionPowerModifier) },
			} },
		{ "skills", std::move(skills) },
		{ "equipment", std::move(equipment) },
	};
}
