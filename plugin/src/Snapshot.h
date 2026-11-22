#pragma once

#include "Equipment.h"

namespace Snapshot
{
	// Game state as JSON matching `Snapshot` in shared/schema/bridge.schema.json.
	// Main thread only.
	nlohmann::json Build();

	// The items that could go into a slot, as `ItemList` in the schema.
	nlohmann::json BuildItemList(Equipment::Slot a_slot);
}
