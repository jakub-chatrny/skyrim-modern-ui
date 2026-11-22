#pragma once

// Item pictures, made by the game itself and cached as PNG files.
//
// While the Character screen is open, each item without a picture is loaded
// into the vanilla inventory preview (Inventory3DManager) and, in the drawing
// pass of a small invisible menu of ours, painted into a cleared square of the
// frame, copied out and the square restored, so it never shows on screen. The
// square is scaled to 256 x 256 and written to
//   Data/PrismaUI/views/SkyrimModernUI/thumbs/<form id>.png
// The page loads it as thumbs/<form id>.png, relative to itself. Because the game
// renders the pictures, items added by mods get them too. Thumbnails.cpp has
// the details and where the technique comes from.
//
// The functions below are for the main thread.
namespace Thumbnails
{
	// Call once on kDataLoaded. Reads the cache folder, registers the capture menu.
	void Install();

	// Making new pictures can be switched off with "itemPictures": false in
	// Data/SKSE/Plugins/SkyrimModernUI.json. Cached pictures are shown either way.
	// Main thread; re-read on every open.
	void SetEnabled(bool a_enabled);

	// URL of the cached picture, or an empty string when there is none yet.
	[[nodiscard]] std::string UrlFor(RE::FormID a_formID);

	// Queue an item for a picture. No-op if it has one, is queued, or failed before.
	void Request(RE::TESForm* a_form);

	// How many requested pictures are still to be made.
	[[nodiscard]] std::size_t Pending();

	// Call every frame while the screen is open; lets the queue be worked on.
	// a_onBatchDone runs once when the queue has just been emptied.
	void Tick(const std::function<void()>& a_onBatchDone);

	// The screen is closing: stop picturing; an unfinished item is asked for again next time.
	void Abort();

	// Call every frame while the screen is closed; closes the capture menu once
	// the preview scene has been torn down.
	void AbortTick();
}
