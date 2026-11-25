#include "PCH.h"

#include "Snapshot.h"

#include <bit>
#include <unordered_set>

#include "Equipment.h"
#include "Outfits.h"
#include "Thumbnails.h"

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

	std::string FillDescription(std::string_view a_template, float a_magnitude, float a_duration);

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
		if (entry) {
			if (const auto value = entry->GetValue(); value > 0) {
				stats.push_back(Stat("Value", std::to_string(value)));
			}
		}

		// Enchantment, as the inventory describes it: the player's own
		// enchantment on this copy first, else the item's built-in one.
		std::string enchantment;
		RE::EnchantmentItem* enchanting = entry ? entry->GetEnchantment() : nullptr;
		if (!enchanting) {
			if (const auto* enchantable = form->As<RE::TESEnchantableForm>()) {
				enchanting = enchantable->formEnchanting;
			}
		}
		if (enchanting) {
			for (const auto* effect : enchanting->effects) {
				if (!effect || !effect->baseEffect) {
					continue;
				}
				auto text = FillDescription(effect->baseEffect->magicItemDescription.c_str(), effect->GetMagnitude(),
					static_cast<float>(effect->GetDuration()));
				if (!text.empty()) {
					enchantment += enchantment.empty() ? text : " " + text;
				}
			}
		}

		// A picture is asked for the first time an item is reported, and
		// reported once it exists.
		Thumbnails::Request(form);
		const auto thumb = Thumbnails::UrlFor(form->GetFormID());

		return {
			{ "thumb", thumb.empty() ? json(nullptr) : json(thumb) },
			{ "formId", std::format("0x{:08X}", form->GetFormID()) },
			{ "name", name ? name : "" },
			{ "type", type },
			{ "icon", icon },
			{ "stats", std::move(stats) },
			{ "enchantment", enchantment.empty() ? json(nullptr) : json(enchantment) },
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

	RE::TESGlobal* FindGlobal(std::string_view a_editorID);

	// "Restore <mag> points of Health for <dur> seconds." -> numbers filled in,
	// <Global=Name> replaced by that global's value, <50> kept as 50, any other
	// <tag> dropped.
	std::string FillDescription(std::string_view a_template, float a_magnitude, float a_duration)
	{
		std::string out;
		out.reserve(a_template.size());
		for (std::size_t i = 0; i < a_template.size();) {
			if (a_template[i] != '<') {
				out += a_template[i++];
				continue;
			}
			const auto end = a_template.find('>', i);
			if (end == std::string_view::npos) {
				out += a_template.substr(i);
				break;
			}
			const auto tag = a_template.substr(i + 1, end - i - 1);
			if (tag == "mag") {
				out += Number(a_magnitude);
			} else if (tag == "dur") {
				out += Number(a_duration);
			} else if (!tag.empty() && tag.find_first_not_of("0123456789.") == std::string_view::npos) {
				// Ability texts write fixed numbers as <50>; the game shows them bold.
				out += tag;
			} else if (tag.starts_with("Global=")) {
				if (auto* global = FindGlobal(tag.substr(7))) {
					out += Number(global->value);
				}
			}
			i = end + 1;
		}
		return out;
	}

	std::string ActorValueName(AV a_value)
	{
		if (a_value == AV::kNone) {
			return {};
		}
		const char* name = RE::ActorValueList::GetActorValueName(a_value);
		return name ? name : "";
	}

	const char* SpellSource(const RE::MagicItem* a_spell)
	{
		if (!a_spell) {
			return "other";
		}
		using Type = RE::MagicSystem::SpellType;
		switch (a_spell->GetSpellType()) {
		case Type::kSpell: return "spell";
		case Type::kDisease: return "disease";
		case Type::kPower: return "power";
		case Type::kLesserPower: return "lesserPower";
		case Type::kAbility: return "ability";
		case Type::kPoison: return "poison";
		case Type::kEnchantment:
		case Type::kStaffEnchantment: return "enchantment";
		case Type::kPotion: return "potion";
		case Type::kIngredient: return "ingredient";
		case Type::kVoicePower: return "shout";
		case Type::kAddiction: return "addiction";
		default: return "other";
		}
	}

	// Active effects as the Magic menu's "Active Effects" list shows them: one
	// row per spell, potion or food, hidden and inactive effects left out.
	json ActiveEffects(RE::PlayerCharacter* a_player)
	{
		struct Row
		{
			std::string name;
			std::string description;
			float       remaining = -1.0f;  // seconds; -1 = no end (abilities)
			bool        positive = true;
			const char* kind = "other";
			// What the effect is, for the UI to pick its icon.
			const char* source = "other";  // spell type: spell, ability, disease, potion, ...
			bool        racial = false;     // granted by the player's race
			std::string av;                 // the actor value it changes ("OneHanded", "ResistFire")
			std::string archetype;          // "ValueModifier", "Cloak", "Paralysis", ...
			std::string school;             // magic skill ("Alteration"), empty for none
		};
		std::vector<Row>                                rows;
		std::unordered_map<const RE::MagicItem*, std::size_t> bySpell;

		// Abilities the player's race grants (Nord frost resistance, Khajiit claws).
		std::unordered_set<const RE::MagicItem*> racialSpells;
		if (const auto* race = a_player->GetRace(); race && race->actorEffects) {
			for (std::uint32_t i = 0; i < race->actorEffects->numSpells; ++i) {
				racialSpells.insert(race->actorEffects->spells[i]);
			}
		}

		auto* list = a_player->AsMagicTarget()->GetActiveEffectList();
		if (!list) {
			return json::array();
		}
		using EffectFlag = RE::EffectSetting::EffectSettingData::Flag;
		using ActiveFlag = RE::ActiveEffect::Flag;
		for (auto* effect : *list) {
			if (!effect || !effect->effect || !effect->effect->baseEffect) {
				continue;
			}
			const auto* base = effect->effect->baseEffect;
			if (base->data.flags.any(EffectFlag::kHideInUI) ||
				effect->flags.any(ActiveFlag::kInactive, ActiveFlag::kDispelled)) {
				continue;
			}
			const auto* spell = effect->spell;
			const float remaining = effect->duration > 0.0f ? std::max(0.0f, effect->duration - effect->elapsedSeconds) : -1.0f;

			auto found = bySpell.find(spell);
			if (found != bySpell.end() && spell) {
				// Same spell, another effect: keep the first description, the longest time.
				auto& row = rows[found->second];
				if (row.description.empty()) {
					row.description = FillDescription(base->magicItemDescription.c_str(), effect->magnitude, effect->duration);
				}
				if (remaining > row.remaining) {
					row.remaining = remaining;
				}
				row.positive = row.positive && !base->IsDetrimental();
				continue;
			}

			Row row;
			const char* spellName = spell ? spell->GetFullName() : nullptr;
			row.name = spellName && *spellName ? spellName : base->GetFullName();
			if (row.name.empty()) {
				continue;
			}
			row.description = FillDescription(base->magicItemDescription.c_str(), effect->magnitude, effect->duration);
			// Abilities (standing stones, Survival's needs, blessings) keep their
			// text on the spell, not on the effect, as the Magic menu shows it.
			if (row.description.empty()) {
				if (const auto* withText = spell ? spell->As<RE::TESDescription>() : nullptr) {
					RE::BSString text;
					const_cast<RE::TESDescription*>(withText)->GetDescription(text, const_cast<RE::MagicItem*>(spell));
					row.description = FillDescription(text.c_str(), effect->magnitude, effect->duration);
				}
			}
			row.remaining = remaining;
			row.positive = !base->IsDetrimental();
			if (spell && spell->IsFood()) {
				row.kind = "food";
			} else if (base->data.resistVariable == AV::kResistFrost || base->data.primaryAV == AV::kResistFrost) {
				row.kind = "cold";
			}
			row.source = SpellSource(spell);
			row.racial = spell && racialSpells.contains(spell);
			row.av = ActorValueName(base->data.primaryAV);
			row.archetype = RE::EffectArchetypes::GetArchetypeName(base->GetArchetype());
			row.school = ActorValueName(base->GetMagickSkill());
			if (spell) {
				bySpell.emplace(spell, rows.size());
			}
			rows.push_back(std::move(row));
		}

		json out = json::array();
		for (auto& row : rows) {
			out.push_back({
				{ "name", std::move(row.name) },
				{ "description", std::move(row.description) },
				{ "remaining", row.remaining },
				{ "positive", row.positive },
				{ "kind", row.kind },
				{ "source", row.source },
				{ "racial", row.racial },
				{ "av", std::move(row.av) },
				{ "archetype", std::move(row.archetype) },
				{ "school", std::move(row.school) },
			});
		}
		return out;
	}

	// Survival Mode (Creation Club, part of Anniversary Edition) keeps its needs
	// in global variables; a need value counts up from 0 (fine) to its maximum.
	// Global editor IDs are kept at run time, so they can be found by name.
	// Every Survival_ global is logged once, so other names can be added from the log.
	RE::TESGlobal* FindGlobal(std::string_view a_editorID)
	{
		static std::unordered_map<std::string, RE::TESGlobal*> globals = [] {
			std::unordered_map<std::string, RE::TESGlobal*> map;
			if (auto* data = RE::TESDataHandler::GetSingleton()) {
				for (auto* global : data->GetFormArray<RE::TESGlobal>()) {
					const char* id = global ? global->GetFormEditorID() : nullptr;
					if (id && *id) {
						map.emplace(id, global);
						if (std::string_view(id).starts_with("Survival_")) {
							SKSE::log::info("survival global {} = {}", id, global->value);
						}
					}
				}
			}
			return map;
		}();
		const auto found = globals.find(std::string(a_editorID));
		return found != globals.end() ? found->second : nullptr;
	}

	json Survival()
	{
		const auto* enabled = FindGlobal("Survival_ModeEnabled");
		if (!enabled || enabled->value < 1.0f) {
			return nullptr;
		}
		json out = json::object();
		// need -> how well the player is doing, as a gauge that is full when fine.
		const auto need = [&](const char* a_key, std::string_view a_value, std::string_view a_max) {
			const auto* value = FindGlobal(a_value);
			const auto* max = FindGlobal(a_max);
			if (value && max && max->value > 0.0f) {
				out[a_key] = Gauge(std::clamp(max->value - value->value, 0.0f, max->value), max->value);
			}
		};
		need("energy", "Survival_ExhaustionNeedValue", "Survival_ExhaustionNeedMaxValue");
		need("hunger", "Survival_HungerNeedValue", "Survival_HungerNeedMaxValue");
		need("warmth", "Survival_ColdNeedValue", "Survival_ColdNeedMaxValue");
		return out.empty() ? json(nullptr) : out;
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

	json snapshot = {
		{ "schemaVersion", 1 },
		{ "player",
			{
				{ "name", player->GetDisplayFullName() },
				{ "level", player->GetLevel() },
				{ "xpProgress", progress ? Fraction(progress->xp, progress->levelThreshold) : 0.0f },
				{ "gold", player->GetGoldAmount() },
				{ "dragonSouls", static_cast<int>(av->GetActorValue(AV::kDragonSouls)) },
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
		{ "outfits", Outfits::State() },
		{ "picturesPending", Thumbnails::Pending() },
		{ "effects", ActiveEffects(player) },
	};
	json dropping = json::array();
	for (const auto id : Equipment::QueuedDrops()) {
		dropping.push_back(std::format("0x{:08X}", id));
	}
	snapshot["dropping"] = std::move(dropping);
	if (auto survival = Survival(); !survival.is_null()) {
		snapshot["survival"] = std::move(survival);
	}
	return snapshot;
}

namespace
{
	// Inventory tab categories, as the Inventory design's submenu names them.
	const char* Category(RE::TESBoundObject* a_object)
	{
		using Type = RE::FormType;
		switch (a_object->GetFormType()) {
		case Type::Weapon:
		case Type::Ammo:
			return "weapons";
		case Type::Armor:
			return "armor";
		case Type::AlchemyItem:
			return a_object->As<RE::AlchemyItem>()->IsFood() ? "food" : "potions";
		case Type::Ingredient:
			return "ingredients";
		case Type::Book:
			return "books";
		default:
			return "misc";
		}
	}

	const char* WeaponKind(const RE::TESObjectWEAP* a_weapon)
	{
		using Type = RE::WEAPON_TYPE;
		switch (a_weapon->GetWeaponType()) {
		case Type::kOneHandSword: return "One-handed · Sword";
		case Type::kOneHandDagger: return "One-handed · Dagger";
		case Type::kOneHandAxe: return "One-handed · War Axe";
		case Type::kOneHandMace: return "One-handed · Mace";
		case Type::kTwoHandSword: return "Two-handed · Greatsword";
		case Type::kTwoHandAxe: return "Two-handed · Battleaxe";
		case Type::kBow: return "Bow";
		case Type::kCrossbow: return "Crossbow";
		case Type::kStaff: return "Staff";
		default: return "Weapon";
		}
	}

	std::string ArmorKind(const RE::TESObjectARMO* a_armor)
	{
		using Part = RE::BGSBipedObjectForm::BipedObjectSlot;
		const char* weight = a_armor->IsHeavyArmor() ? "Heavy Armor" : a_armor->IsLightArmor() ? "Light Armor" : "Clothing";
		const char* part = "Apparel";
		if (a_armor->IsShield()) {
			part = "Shield";
		} else if (a_armor->HasPartOf(Part::kBody)) {
			part = "Body";
		} else if (a_armor->HasPartOf(Part::kHead) || a_armor->HasPartOf(Part::kHair) || a_armor->HasPartOf(Part::kCirclet)) {
			part = "Head";
		} else if (a_armor->HasPartOf(Part::kHands)) {
			part = "Hands";
		} else if (a_armor->HasPartOf(Part::kFeet)) {
			part = "Feet";
		} else if (a_armor->HasPartOf(Part::kAmulet)) {
			return "Jewelry · Neck";
		} else if (a_armor->HasPartOf(Part::kRing)) {
			return "Jewelry · Ring";
		}
		return std::format("{} · {}", weight, part);
	}

	std::string TypeLabel(RE::TESBoundObject* a_object)
	{
		using Type = RE::FormType;
		switch (a_object->GetFormType()) {
		case Type::Weapon:
			return WeaponKind(a_object->As<RE::TESObjectWEAP>());
		case Type::Armor:
			return ArmorKind(a_object->As<RE::TESObjectARMO>());
		case Type::Ammo:
			return a_object->As<RE::TESAmmo>()->IsBolt() ? "Bolt" : "Arrow";
		case Type::AlchemyItem:
			{
				auto* potion = a_object->As<RE::AlchemyItem>();
				return potion->IsFood() ? "Food" : potion->IsPoison() ? "Poison" : "Potion";
			}
		case Type::Ingredient:
			return "Ingredient";
		case Type::Book:
			{
				auto* book = a_object->As<RE::TESObjectBOOK>();
				return book->TeachesSpell() ? "Spell Tome" : book->TeachesSkill() ? "Skill Book" : book->IsNoteScroll() ? "Note" : "Book";
			}
		case Type::Scroll:
			return "Scroll";
		case Type::SoulGem:
			return "Soul Gem";
		case Type::KeyMaster:
			return "Key";
		case Type::Light:
			return "Torch";
		default:
			return "Miscellaneous";
		}
	}

	// Which ring colour and glyph an enchantment effect gets in the design.
	const char* Element(const RE::EffectSetting* a_effect)
	{
		switch (a_effect->data.resistVariable) {
		case AV::kResistFrost: return "frost";
		case AV::kResistFire: return "fire";
		case AV::kResistShock: return "shock";
		default: return "holy";
		}
	}

	// The effect texts of a magic item, numbers filled in.
	std::string EffectsText(RE::MagicItem* a_item, std::uint16_t a_knownMask = 0xFFFF)
	{
		std::string text;
		std::size_t index = 0;
		for (const auto* effect : a_item->effects) {
			const bool known = (a_knownMask >> index++) & 1;
			if (!effect || !effect->baseEffect || !known) {
				continue;
			}
			auto line = FillDescription(effect->baseEffect->magicItemDescription.c_str(), effect->GetMagnitude(),
				static_cast<float>(effect->GetDuration()));
			if (line.empty()) {
				line = effect->baseEffect->GetName();
			}
			if (!line.empty()) {
				text += text.empty() ? line : " " + line;
			}
		}
		return text;
	}

	std::string FormDescription(RE::TESForm* a_form, RE::TESDescription* a_description, std::uint32_t a_field = 'CSED')
	{
		if (!a_description) {
			return {};
		}
		RE::BSString out;
		a_description->GetDescription(out, a_form, a_field);
		return out.c_str() ? std::string(out.c_str()) : std::string();
	}

	// Skyrim has no rarity; this is ours, from the enchantment (the Inventory
	// design: one enchantment rare, two epic). Unique items, which the game
	// will not let you disenchant, and Daedric artifacts are legendary.
	constexpr RE::FormID MAGIC_DISALLOW_ENCHANTING = 0x000C27BD;
	constexpr RE::FormID VENDOR_ITEM_DAEDRIC_ARTIFACT = 0x000917E8;

	const char* Rarity(RE::TESBoundObject* a_object, std::size_t a_enchantments)
	{
		if (const auto* keywords = a_object->As<RE::BGSKeywordForm>()) {
			if (keywords->HasKeywordID(VENDOR_ITEM_DAEDRIC_ARTIFACT) ||
				(a_enchantments > 0 && keywords->HasKeywordID(MAGIC_DISALLOW_ENCHANTING))) {
				return "legendary";
			}
		}
		return a_enchantments >= 2 ? "epic" : a_enchantments == 1 ? "rare" : "common";
	}

	constexpr RE::FormID GOLD = 0x0000000F;

	std::string SkillName(AV a_value)
	{
		for (const auto& skill : SKILLS) {
			if (skill.value == a_value) {
				return skill.name;
			}
		}
		return ActorValueName(a_value);
	}

	const char* SoulName(RE::SOUL_LEVEL a_level)
	{
		switch (a_level) {
		case RE::SOUL_LEVEL::kPetty: return "Petty";
		case RE::SOUL_LEVEL::kLesser: return "Lesser";
		case RE::SOUL_LEVEL::kCommon: return "Common";
		case RE::SOUL_LEVEL::kGreater: return "Greater";
		case RE::SOUL_LEVEL::kGrand: return "Grand";
		default: return "Empty";
		}
	}

	// What the game knows about an item beyond its card text, as label / value
	// rows for the details panel: most items have no description at all.
	json Facts(RE::PlayerCharacter* a_player, RE::TESBoundObject* a_object, RE::InventoryEntryData* a_entry)
	{
		json facts = json::array();
		const auto add = [&](std::string_view a_label, std::string a_value) {
			facts.push_back({ { "label", a_label }, { "value", std::move(a_value) } });
		};
		const auto yesNo = [](bool a_yes) { return std::string(a_yes ? "Yes" : "No"); };

		if (const auto* weapon = a_object->As<RE::TESObjectWEAP>()) {
			if (const auto skill = SkillName(weapon->weaponData.skill.get()); !skill.empty()) {
				add("Skill", skill);
			}
			if (!weapon->IsStaff()) {
				add("Speed", Number(weapon->GetSpeed(), 2));
				if (!weapon->IsBow() && !weapon->IsCrossbow()) {
					add("Reach", Number(weapon->GetReach(), 2));
				}
				add("Stagger", Number(weapon->GetStagger(), 2));
				add("Critical damage", std::to_string(weapon->GetCritDamage()));
			}
		} else if (const auto* armor = a_object->As<RE::TESObjectARMO>()) {
			if (armor->IsHeavyArmor()) {
				add("Skill", "Heavy Armor");
			} else if (armor->IsLightArmor()) {
				add("Skill", "Light Armor");
			}
		} else if (auto* book = a_object->As<RE::TESObjectBOOK>()) {
			if (book->TeachesSpell()) {
				if (auto* spell = book->GetSpell()) {
					add("Teaches", spell->GetName());
					add("Learned", yesNo(a_player->HasSpell(spell)));
				}
			} else if (book->TeachesSkill()) {
				if (const auto skill = SkillName(book->GetSkill()); !skill.empty()) {
					add("Reading raises", skill);
				}
			}
			if (!book->TeachesSpell()) {
				add("Read", yesNo(book->IsRead()));
			}
		} else if (const auto* gem = a_object->As<RE::TESSoulGem>()) {
			add("Capacity", SoulName(gem->GetMaximumCapacity()));
			auto soul = a_entry ? a_entry->GetSoulLevel() : RE::SOUL_LEVEL::kNone;
			if (soul == RE::SOUL_LEVEL::kNone) {
				soul = gem->GetContainedSoul();
			}
			add("Soul", SoulName(soul));
			if (gem->CanHoldNPCSoul()) {
				add("Holds", "Black souls");
			}
		} else if (auto* ingredient = a_object->As<RE::IngredientItem>()) {
			const auto total = ingredient->effects.size();
			const auto known = std::popcount(static_cast<unsigned>(ingredient->gamedata.knownEffectFlags & ((1u << total) - 1)));
			add("Effects known", std::format("{} of {}", known, total));
		}

		if (a_entry && a_entry->IsQuestObject()) {
			add("Quest item", "Cannot be dropped");
		}
		if (a_entry && !a_entry->IsOwnedBy(a_player, true)) {
			add("Stolen", "Yes");
		}
		return facts;
	}
}

json Snapshot::BuildInventory()
{
	auto* player = RE::PlayerCharacter::GetSingleton();
	auto  inventory = player->GetInventory([](RE::TESBoundObject& a_object) { return a_object.GetPlayable(); });

	json items = json::array();
	for (auto& [object, data] : inventory) {
		auto& [count, entry] = data;
		if (!object || count <= 0 || object->GetFormID() == GOLD || !entry) {
			continue;
		}
		const char* name = entry->GetDisplayName();
		if (!name || !*name) {
			continue;  // nameless records are not meant to be seen
		}

		json stats = json::object();
		if (object->IsWeapon()) {
			stats["damage"] = std::round(player->GetDamage(entry.get()));
		} else if (const auto* ammo = object->As<RE::TESAmmo>()) {
			stats["damage"] = std::round(ammo->GetRuntimeData().data.damage);
		} else if (object->IsArmor()) {
			if (const float rating = player->GetArmorValue(entry.get()); rating > 0.0f) {
				stats["armor"] = std::round(rating);
			}
		}
		stats["weight"] = std::round(entry->GetWeight() * 10.0f) / 10.0f;
		stats["value"] = entry->GetValue();

		// Enchantment: the player's own on this copy first, else the item's.
		RE::EnchantmentItem* enchanting = entry->GetEnchantment();
		if (!enchanting) {
			if (const auto* enchantable = object->As<RE::TESEnchantableForm>()) {
				enchanting = enchantable->formEnchanting;
			}
		}
		json enchantments = json::array();
		if (enchanting) {
			for (const auto* effect : enchanting->effects) {
				if (effect && effect->baseEffect) {
					enchantments.push_back({ { "name", effect->baseEffect->GetName() }, { "element", Element(effect->baseEffect) } });
				}
			}
		}

		// What the item does or is, as the game's own item card puts it.
		std::string description;
		if (enchanting) {
			description = EffectsText(enchanting);
		}
		if (auto* potion = object->As<RE::AlchemyItem>()) {
			description = EffectsText(potion);
		} else if (auto* ingredient = object->As<RE::IngredientItem>()) {
			description = EffectsText(ingredient, ingredient->gamedata.knownEffectFlags);
			if (description.empty()) {
				description = "Effects not yet discovered.";
			}
		} else if (auto* scroll = object->As<RE::ScrollItem>()) {
			description = EffectsText(scroll);
		} else if (auto* book = object->As<RE::TESObjectBOOK>()) {
			description = FormDescription(book, &book->itemCardDescription, 'MANC');
		}
		if (const auto own = FormDescription(object, object->As<RE::TESDescription>()); !own.empty() && !object->Is(RE::FormType::Book)) {
			description = description.empty() ? own : own + " " + description;
		}

		Thumbnails::Request(object);
		const auto thumb = Thumbnails::UrlFor(object->GetFormID());
		const auto enchantCount = enchantments.size();

		items.push_back({
			{ "formId", std::format("0x{:08X}", object->GetFormID()) },
			{ "name", name },
			{ "category", Category(object) },
			{ "typeLabel", TypeLabel(object) },
			{ "count", count },
			{ "worn", entry->IsWorn() },
			{ "favorite", entry->IsFavorited() },
			{ "stats", std::move(stats) },
			{ "enchantments", std::move(enchantments) },
			{ "rarity", Rarity(object, enchantCount) },
			{ "description", description.empty() ? json(nullptr) : json(description) },
			{ "facts", Facts(player, object, entry.get()) },
			{ "thumb", thumb.empty() ? json(nullptr) : json(thumb) },
		});
	}
	return { { "items", std::move(items) } };
}

json Snapshot::BuildItemList(Equipment::Slot a_slot)
{
	const auto candidates = Equipment::ListFor(a_slot);

	json items = json::array();
	for (const auto& item : candidates.items) {
		auto entry = SlotItem(a_slot, item);
		entry["count"] = item.count;
		entry["equipped"] = item.form == candidates.equipped;
		entry["favorite"] = item.entry && item.entry->IsFavorited();
		items.push_back(std::move(entry));
	}

	return {
		{ "slot", Equipment::SLOT_NAMES[static_cast<std::size_t>(a_slot)] },
		{ "items", std::move(items) },
	};
}
