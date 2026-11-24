#include "PCH.h"

#include "Snapshot.h"

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
	if (auto survival = Survival(); !survival.is_null()) {
		snapshot["survival"] = std::move(survival);
	}
	return snapshot;
}

json Snapshot::BuildItemList(Equipment::Slot a_slot)
{
	const auto candidates = Equipment::ListFor(a_slot);

	json items = json::array();
	for (const auto& item : candidates.items) {
		auto entry = SlotItem(a_slot, item);
		entry["count"] = item.count;
		entry["equipped"] = item.form == candidates.equipped;
		items.push_back(std::move(entry));
	}

	return {
		{ "slot", Equipment::SLOT_NAMES[static_cast<std::size_t>(a_slot)] },
		{ "items", std::move(items) },
	};
}
