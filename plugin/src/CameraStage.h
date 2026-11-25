#pragma once

// Phase 2, approach 1: show the real in-world player by swinging the
// third-person camera in front of them while the Character screen is open.
// Technique follows the open-source "Show Player In Menus" mod
// (derickso/ShowPlayerInMenus, MIT).
//
// Approach 2 on top of that (hideWorld): every scene-graph branch that does not
// lead to the player is culled while the stage is active, so only the
// character is drawn, in front of a black backdrop plane.
//
// Tunables are re-read on every Enter() from
//   Data/SKSE/Plugins/SkyrimModernUI.json
// so framing can be adjusted without rebuilding or restarting the game.
namespace CameraStage
{
	struct Config
	{
		bool  enabled = true;
		bool  itemPictures = true;   // make item pictures in game (Thumbnails.h)
		bool  hideWorld = true;   // approach 2: cull everything in the scene except the player
		bool  backdrop = true;            // black plane mesh behind the character
		float backdropDistance = 150.0f;  // how far behind the character, game units
		float backdropHeight = 70.0f;     // height of the plane's centre above the feet
		float backdropScale = 10.0f;      // the mesh is 512 units square at scale 1
		bool  blackFog = true;            // fog colours set to black while open
		bool  noFog = true;               // outdoors: weather fog switched off while open
		bool  hideHud = true;     // hide the vanilla HUD (compass, crosshair, bars) while open
		bool  studioLights = true;  // light the character evenly, whatever the weather or time
		float lightStrength = 0.8f; // key light brightness; the fill light is a little under half of it
		float lightRadius = 600.0f; // how far the lights reach, game units
		bool  faceOpenSpace = true;  // turn the player toward open space when a wall would pull the camera in
		bool  pauseGame = false;  // paused = frozen pose; unpaused = animated, world keeps running
		float distance = 260.0f;  // camera distance from the player, game units
		float offsetX = -3.0f;    // camera sideways shift; centres the character in the arch
		float offsetZ = -45.0f;   // camera height shift; lower camera = character higher on screen
		float rotation = 0.0f;    // radians added to "directly in front"
		float pitch = 0.1f;       // radians, camera looking slightly down
		float fov = 0.0f;         // 0 = leave the game's FOV alone
	};

	// Reads the JSON file (missing file or keys = defaults). Main thread only.
	Config LoadConfig();

	// Main thread only. Enter() does nothing and returns false when the camera
	// is in a state this spike does not handle (mount, furniture, VATS, ...).
	bool Enter(const Config& a_config);
	void Leave();

	// True between Enter() and Leave().
	[[nodiscard]] bool IsActive();

	// True for nodes the stage itself attached to the player (the backdrop).
	[[nodiscard]] bool IsOwnNode(const RE::NiAVObject* a_object);

	// Call once per frame on the main thread while the game runs. Re-applies the
	// world hiding to objects that appeared since the last call. Cheap when idle.
	void Tick();

	// Inventory tab: the character is hidden (the item preview is drawn where
	// the page shows the item). Undone by Leave() as well.
	void SetCharacterHidden(bool a_hidden);

	// Turns the character in place (dragging on the Character tab); the camera,
	// the backdrop and the lights stay. Undone by Leave().
	void TurnCharacter(float a_radians);
}
