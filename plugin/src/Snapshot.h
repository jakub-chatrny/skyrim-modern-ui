#pragma once

namespace Snapshot
{
	// Game state as JSON matching `Snapshot` in shared/schema/bridge.schema.json.
	// Main thread only.
	nlohmann::json Build();
}
