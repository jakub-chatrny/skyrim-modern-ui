#pragma once

// The Character screen: one Prisma UI view, toggled with a hotkey.
// Bridge contract (see shared/schema/bridge.schema.json):
//   plugin -> UI : window.smuiReceive(json)   Snapshot
//   UI -> plugin : window.smuiCommand(json)   Command
namespace CharacterMenu
{
	// Call once on kDataLoaded. Logs and does nothing if Prisma UI is missing.
	void Install();
}
