#include "PCH.h"

#include "Snapshot.h"

namespace
{
	using AV = RE::ActorValue;
	using json = nlohmann::json;

	json Gauge(float a_current, float a_max)
	{
		return { { "current", a_current }, { "max", a_max } };
	}

	// current = live value; max = permanent value. Temporary buffs are not
	// included in max yet (phase 3: follow the "Skyrim Character Sheet" mod).
	json Vital(RE::ActorValueOwner* a_owner, AV a_value)
	{
		return Gauge(a_owner->GetActorValue(a_value), a_owner->GetPermanentActorValue(a_value));
	}
}

json Snapshot::Build()
{
	auto* player = RE::PlayerCharacter::GetSingleton();
	auto* av = player->AsActorValueOwner();

	const auto resist = [&](AV a_value) { return av->GetActorValue(a_value); };
	const char* slots[] = { "head", "body", "hands", "feet", "leftHand", "rightHand",
		                    "mask", "cloak", "neck", "ring", "ranged", "ammo" };

	json equipment = json::object();
	for (const auto* slot : slots) {
		equipment[slot] = nullptr;  // TODO(phase 3): equipped item per slot
	}

	return {
		{ "schemaVersion", 1 },
		{ "player",
			{
				{ "name", player->GetDisplayFullName() },
				{ "level", player->GetLevel() },
				{ "xpProgress", 0.0 },  // TODO(phase 3): PlayerSkills xp / levelThreshold
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
		// TODO(phase 3): real damage numbers, skills, survival (optional).
		{ "damage",
			{ { "rightHand", 0 }, { "leftHand", 0 }, { "ranged", 0 }, { "meleeSpeed", 0 }, { "rangedSpeed", 0 }, { "magicPower", 0 } } },
		{ "skills", json::array() },
		{ "equipment", equipment },
	};
}
