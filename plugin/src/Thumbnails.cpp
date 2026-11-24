#include "PCH.h"

#include "Thumbnails.h"

#include <d3d11_1.h>

#include <atomic>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_set>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#pragma warning(push, 0)
#include <stb_image_write.h>
#pragma warning(pop)

// Technique from Modex (ModExplorerMenu by patchulidev, GPL-3.0 with modding
// exception, src/ui/components/Item3DPreview.cpp and src/ui/core/UIMenuImpl.cpp),
// with the safety rules Grid Inventory (skypia0147-dev, same licence) learned
// from crash reports, see its src/ui/ItemPreview.cpp:
//
//  - The vanilla inventory preview (Inventory3DManager) is driven from the
//    PostDisplay of a menu of our own, because that is where the game draws
//    menus and where the inventory menu itself calls Render(). Called from
//    anywhere else, Render() draws nothing.
//  - In PostDisplay: save a square of the frame, clear it, Render() paints the
//    item there, copy it out, put the frame back. The item never shows.
//  - The preview keeps at most 7 models; UnloadInventoryItem does not free a
//    slot and a load into a full array silently fails, so the scene is reset
//    (End3D + Begin3D) at 5.
//  - End3D crashes on an entry that is still loading or half-built, and every
//    reported crash had UnloadInventoryItem right before End3D: never do that,
//    and only tear down when the scene is safe.

namespace
{
	// Inside the page's own folder, addressed relative to the page, the same way
	// its bundled assets load. scripts/deploy.sh keeps this folder.
	// v2: transparent background (v1 pictures, opaque on black, are left unused).
	constexpr auto CACHE_DIR = "Data/PrismaUI/views/SkyrimModernUI/thumbs/v2";
	constexpr auto CACHE_URL = "thumbs/v2/";
	constexpr auto MENU_NAME = "SkyrimModernUI Pictures";
	constexpr auto FAILED_FILE = "failed.txt";  // items whose picture could not be made

	// The captured square is a little larger than the model's bounding sphere.
	constexpr float CAPTURE_MARGIN = 1.1f;
	// After the model has landed, frames to let textures stream in.
	constexpr int SETTLE_FRAMES = 3;
	// Frames to wait for a model before giving the item up.
	constexpr int GIVE_UP_FRAMES = 90;
	// Frames to wait for a safe moment to tear the scene down before skipping it.
	constexpr int TEARDOWN_FRAMES = 300;
	// Reset the scene before a load once this many models are held (capacity 7).
	constexpr std::size_t RESET_AT = 5;
	// Pictures are stored at most this big; the capture is scaled down.
	constexpr std::uint32_t PICTURE_SIZE = 256;

	// ---- shared between the main thread and the menu's PostDisplay --------

	std::mutex                     stateLock;  // guards the four containers below
	std::unordered_set<RE::FormID> cached;     // has a file
	std::unordered_set<RE::FormID> rejected;   // no model, or capture failed; not retried this session
	std::deque<RE::FormID>         queue;
	std::unordered_set<RE::FormID> queued;     // in `queue` or being pictured now

	std::atomic<bool> enabled{ false };     // the screen is open: work through the queue
	std::atomic<bool> working{ false };     // an item is being pictured right now
	std::atomic<bool> sessionOver{ false }; // PostDisplay has torn the scene down; the menu can close
	std::atomic<bool> batchDone{ false };   // the queue ran empty
	std::atomic<bool> newPictures{ false }; // a picture was saved since the page was last told
	bool              menuRegistered = false;
	bool              making = false;       // "itemPictures" in the JSON
	bool              menuOpen = false;     // main thread's view: kShow sent and not yet kHide

	// ---- PostDisplay only ----------------------------------------------------

	std::atomic<bool>   inSession{ false };  // Begin3D outstanding (set on menu show, cleared in PostDisplay)
	RE::TESBoundObject* current = nullptr;
	RE::FormID          currentID = 0;  // the form the picture is for (a spell's id, not its display model's)
	int                 framesWaited = 0;
	int                 framesLoaded = 0;
	int                 teardownWaited = 0;

	std::filesystem::path FilePath(RE::FormID a_formID)
	{
		return std::filesystem::path(CACHE_DIR) / std::format("{:08X}.png", a_formID);
	}

	bool WritePng(const std::filesystem::path& a_path, const std::uint8_t* a_rgba, std::uint32_t a_size)
	{
		std::ofstream file(a_path, std::ios::binary | std::ios::trunc);
		if (!file) {
			return false;
		}
		const auto write = [](void* a_context, void* a_data, int a_length) {
			static_cast<std::ofstream*>(a_context)->write(static_cast<const char*>(a_data), a_length);
		};
		return stbi_write_png_to_func(write, &file, static_cast<int>(a_size), static_cast<int>(a_size), 4, a_rgba,
				   static_cast<int>(a_size * 4)) != 0 &&
		       file.good();
	}

	// Box filter from a_size x a_size down to a_target x a_target (RGBA).
	std::vector<std::uint8_t> Shrink(const std::vector<std::uint8_t>& a_rgba, std::uint32_t a_size, std::uint32_t a_target)
	{
		if (a_size <= a_target) {
			return a_rgba;
		}
		std::vector<std::uint8_t> out(static_cast<std::size_t>(a_target) * a_target * 4);
		for (std::uint32_t y = 0; y < a_target; ++y) {
			const std::uint32_t y0 = y * a_size / a_target;
			const std::uint32_t y1 = std::max(y0 + 1, (y + 1) * a_size / a_target);
			for (std::uint32_t x = 0; x < a_target; ++x) {
				const std::uint32_t x0 = x * a_size / a_target;
				const std::uint32_t x1 = std::max(x0 + 1, (x + 1) * a_size / a_target);
				// Colours weighted by alpha, so transparent pixels do not darken edges.
				std::uint64_t sum[3]{};
				std::uint64_t alpha = 0;
				for (std::uint32_t sy = y0; sy < y1; ++sy) {
					for (std::uint32_t sx = x0; sx < x1; ++sx) {
						const auto* p = a_rgba.data() + (static_cast<std::size_t>(sy) * a_size + sx) * 4;
						for (int c = 0; c < 3; ++c) {
							sum[c] += static_cast<std::uint64_t>(p[c]) * p[3];
						}
						alpha += p[3];
					}
				}
				const std::uint32_t count = (y1 - y0) * (x1 - x0);
				auto*               q = out.data() + (static_cast<std::size_t>(y) * a_target + x) * 4;
				for (int c = 0; c < 3; ++c) {
					q[c] = alpha ? static_cast<std::uint8_t>(sum[c] / alpha) : 0;
				}
				q[3] = static_cast<std::uint8_t>(alpha / count);
			}
		}
		return out;
	}

	// ---- the preview scene ---------------------------------------------------

	bool LoadInFlight(RE::Inventory3DManager* a_manager)
	{
		auto& runtime = a_manager->GetRuntimeData();
		if (runtime.loadTask) {
			return true;
		}
		for (auto& entry : runtime.loadedModels) {
			if (!entry.spModel) {
				return true;
			}
		}
		return false;
	}

	// Null = End3D may walk the scene now; otherwise why not.
	const char* UnsafeToTearDown(RE::Inventory3DManager* a_manager)
	{
		if (LoadInFlight(a_manager)) {
			return "a load is in flight";
		}
		for (auto& entry : a_manager->GetRuntimeData().loadedModels) {
			if (entry.spModel && entry.spModel->worldBound.radius <= 0.0f) {
				return "an entry has no geometry yet";
			}
			if (!entry.itemBase || !entry.modelObj) {
				return "an entry is half built";
			}
		}
		return nullptr;
	}

	const char* ModelPath(RE::TESForm* a_form)
	{
		try {
			const auto* model = a_form ? a_form->As<RE::TESModel>() : nullptr;
			return model ? model->GetModel() : nullptr;
		} catch (...) {
			return nullptr;  // a recycled form pointer in the engine's list
		}
	}

	// The loaded model for the current item: by item, or else an entry with the
	// same model file (the engine shares one entry between items that use one file).
	RE::NiAVObject* CurrentModel(RE::Inventory3DManager* a_manager)
	{
		auto& models = a_manager->GetRuntimeData().loadedModels;
		for (auto& entry : models) {
			if ((entry.itemBase == current || entry.modelObj == current) && entry.spModel) {
				return entry.spModel.get();
			}
		}
		const char* path = ModelPath(current);
		if (path && *path) {
			for (auto& entry : models) {
				if (!entry.spModel) {
					continue;
				}
				for (RE::TESForm* source : { entry.itemBase, static_cast<RE::TESForm*>(entry.modelObj) }) {
					const char* other = ModelPath(source);
					if (other && _stricmp(other, path) == 0) {
						return entry.spModel.get();
					}
				}
			}
		}
		return nullptr;
	}

	// Most armor records set only the male "ground" model (Skyrim.esm: 1881 of
	// them), and for some the inventory preview's load then never finishes
	// (here: Iron Armor, Fishing Clothes). Grid Inventory fills the empty slot
	// from the other before loading (its IconCache.cpp, "Capturable"), except
	// when the model swaps textures, where the copy would draw the bare mesh.
	// In memory only; the ground model is what a dropped item looks like.
	void HealGroundModel(RE::TESBoundObject* a_object)
	{
		auto* armor = a_object ? a_object->As<RE::TESObjectARMO>() : nullptr;
		if (!armor) {
			return;
		}
		auto&       male = armor->worldModels[RE::SEXES::kMale];
		auto&       female = armor->worldModels[RE::SEXES::kFemale];
		const char* malePath = male.GetModel();
		const char* femalePath = female.GetModel();
		const bool  hasMale = malePath && *malePath;
		const bool  hasFemale = femalePath && *femalePath;
		if (hasMale == hasFemale) {
			return;
		}
		const auto& source = hasMale ? male : female;
		if (source.numAlternateTextures > 0) {
			return;
		}
		if (hasMale) {
			female.SetModel(malePath);
		} else {
			male.SetModel(femalePath);
		}
		logger::info("Thumbnail {:08X}: {} has a ground model for one sex only; filled the other", armor->GetFormID(),
			armor->GetName());
	}

	struct Rect
	{
		std::int32_t left = 0, top = 0, size = 0;
	};

	// Where the model is on screen, from the interface scene's camera (Modex's
	// arithmetic): a square around its bounding sphere.
	std::optional<Rect> ModelRect(RE::NiAVObject* a_model, std::uint32_t a_width, std::uint32_t a_height)
	{
		auto* scene = RE::UI3DSceneManager::GetSingleton();
		if (!scene) {
			return std::nullopt;
		}
		const auto& frustum = scene->viewFrustum;
		const float depth = a_model->local.translate.y;
		const float minX = -frustum.fLeft * depth;
		const float minZ = -frustum.fBottom * depth;
		const float width = -frustum.fRight * depth - minX;
		const float height = -frustum.fTop * depth - minZ;
		const float ratioX = width / static_cast<float>(a_width);
		const float ratioY = height / static_cast<float>(a_height);
		if (ratioX == 0.0f || ratioY == 0.0f || !std::isfinite(ratioX) || !std::isfinite(ratioY)) {
			return std::nullopt;
		}

		const auto& bound = a_model->worldBound;
		const float centreX = -(bound.center.x + minX) / ratioX;
		const float centreY = -(bound.center.z + minZ) / ratioY;
		const float radius = bound.radius / std::abs(ratioX) * CAPTURE_MARGIN;
		if (!std::isfinite(centreX) || !std::isfinite(centreY) || !std::isfinite(radius) || radius < 8.0f) {
			return std::nullopt;
		}

		const auto size = static_cast<std::int32_t>(std::min({ radius * 2.0f, static_cast<float>(a_width), static_cast<float>(a_height) }));
		const auto left = std::clamp(static_cast<std::int32_t>(centreX - size / 2.0f), 0, static_cast<std::int32_t>(a_width) - size);
		const auto top = std::clamp(static_cast<std::int32_t>(centreY - size / 2.0f), 0, static_cast<std::int32_t>(a_height) - size);
		return Rect{ left, top, size };
	}

	// Calls Render() with the whole frame saved before and put back after, so
	// whatever it paints never reaches the screen. GPU copies only.
	ID3D11Texture2D* pumpSaved = nullptr;

	void PumpRender(RE::Inventory3DManager* a_manager)
	{
		auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
		if (!renderer) {
			return;
		}
		const auto& data = renderer->GetRuntimeData();
		auto*       context = reinterpret_cast<ID3D11DeviceContext*>(data.context);
		auto*       device = reinterpret_cast<ID3D11Device*>(data.forwarder);
		auto*       target = reinterpret_cast<ID3D11RenderTargetView*>(data.renderWindows[0].renderView);
		if (!context || !device || !target) {
			return;
		}

		ID3D11Resource* resource = nullptr;
		target->GetResource(&resource);
		ID3D11Texture2D* frame = nullptr;
		if (resource) {
			resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&frame));
			resource->Release();
		}
		if (!frame) {
			return;
		}

		D3D11_TEXTURE2D_DESC frameDesc{};
		frame->GetDesc(&frameDesc);
		if (pumpSaved) {
			D3D11_TEXTURE2D_DESC savedDesc{};
			pumpSaved->GetDesc(&savedDesc);
			if (savedDesc.Width != frameDesc.Width || savedDesc.Height != frameDesc.Height || savedDesc.Format != frameDesc.Format) {
				pumpSaved->Release();
				pumpSaved = nullptr;
			}
		}
		if (!pumpSaved) {
			D3D11_TEXTURE2D_DESC desc = frameDesc;
			desc.MipLevels = desc.ArraySize = 1;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = 0;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			device->CreateTexture2D(&desc, nullptr, &pumpSaved);
		}
		if (pumpSaved && frameDesc.SampleDesc.Count == 1) {
			context->CopyResource(pumpSaved, frame);
			a_manager->Render();
			context->CopyResource(frame, pumpSaved);
		}
		frame->Release();
	}

	bool Capture(RE::Inventory3DManager* a_manager, RE::NiAVObject* a_model, RE::FormID a_formID)
	{
		auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
		if (!renderer) {
			return false;
		}
		const auto& data = renderer->GetRuntimeData();
		auto*       context = reinterpret_cast<ID3D11DeviceContext*>(data.context);
		auto*       device = reinterpret_cast<ID3D11Device*>(data.forwarder);
		auto*       target = reinterpret_cast<ID3D11RenderTargetView*>(data.renderWindows[0].renderView);
		if (!context || !device || !target) {
			logger::warn("Thumbnail capture: no D3D device or render target");
			return false;
		}

		ID3D11Resource* resource = nullptr;
		target->GetResource(&resource);
		ID3D11Texture2D* frame = nullptr;
		if (resource) {
			resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&frame));
			resource->Release();
		}
		if (!frame) {
			logger::warn("Thumbnail capture: render target is not a texture");
			return false;
		}

		D3D11_TEXTURE2D_DESC frameDesc{};
		frame->GetDesc(&frameDesc);
		const bool bgra = frameDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || frameDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		const bool rgba = frameDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || frameDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		const auto rect = ModelRect(a_model, frameDesc.Width, frameDesc.Height);
		if ((!bgra && !rgba) || frameDesc.SampleDesc.Count != 1 || !rect) {
			logger::warn("Thumbnail capture: unusable frame (format {}, {} samples) or model position", static_cast<int>(frameDesc.Format),
				frameDesc.SampleDesc.Count);
			frame->Release();
			return false;
		}

		D3D11_TEXTURE2D_DESC desc = frameDesc;
		desc.Width = desc.Height = static_cast<UINT>(rect->size);
		desc.MipLevels = desc.ArraySize = 1;
		desc.MiscFlags = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;  // saved frame pixels
		desc.BindFlags = 0;
		desc.CPUAccessFlags = 0;
		ID3D11Texture2D* saved = nullptr;
		device->CreateTexture2D(&desc, nullptr, &saved);
		desc.Usage = D3D11_USAGE_STAGING;  // the picture, read back by the CPU
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		ID3D11Texture2D* readback = nullptr;
		device->CreateTexture2D(&desc, nullptr, &readback);
		ID3D11Texture2D* readbackWhite = nullptr;  // the same, painted over white
		device->CreateTexture2D(&desc, nullptr, &readbackWhite);

		bool ok = false;
		if (saved && readback && readbackWhite) {
			const D3D11_BOX box{ static_cast<UINT>(rect->left), static_cast<UINT>(rect->top), 0,
				static_cast<UINT>(rect->left + rect->size), static_cast<UINT>(rect->top + rect->size), 1 };

			// 1. Save what the frame shows there.
			context->CopySubresourceRegion(saved, 0, 0, 0, 0, frame, 0, &box);

			// 2. Clear it, 3. the engine paints the item, 4. copy it out; once over
			// black and once over white. Where the item is solid both agree; where it
			// is transparent the background shows, and the difference gives the alpha.
			ID3D11DeviceContext1* context1 = nullptr;
			context->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&context1));
			const auto paint = [&](float a_background, ID3D11Texture2D* a_into) {
				if (context1) {
					const D3D11_RECT clearRect{ rect->left, rect->top, rect->left + rect->size, rect->top + rect->size };
					const FLOAT      colour[4]{ a_background, a_background, a_background, 1.0f };
					context1->ClearView(target, colour, &clearRect, 1);
				}
				a_manager->Render();
				context->CopySubresourceRegion(a_into, 0, 0, 0, 0, frame, 0, &box);
			};
			paint(0.0f, readback);
			paint(1.0f, readbackWhite);
			if (context1) {
				context1->Release();
			}
			// 5. Put the frame back.
			context->CopySubresourceRegion(frame, 0, static_cast<UINT>(rect->left), static_cast<UINT>(rect->top), 0, saved, 0, nullptr);

			D3D11_MAPPED_SUBRESOURCE mapped{};
			D3D11_MAPPED_SUBRESOURCE mappedWhite{};
			const bool               mappedBlack = SUCCEEDED(context->Map(readback, 0, D3D11_MAP_READ, 0, &mapped));
			const bool               mappedOnWhite = mappedBlack && SUCCEEDED(context->Map(readbackWhite, 0, D3D11_MAP_READ, 0, &mappedWhite));
			if (mappedBlack && mappedOnWhite) {
				const auto                size = static_cast<std::uint32_t>(rect->size);
				std::vector<std::uint8_t> pixels(static_cast<std::size_t>(size) * size * 4);
				std::uint32_t             lit = 0;  // pixels with some of the item in them
				for (std::uint32_t y = 0; y < size; ++y) {
					const auto* black = static_cast<const std::uint8_t*>(mapped.pData) + static_cast<std::size_t>(y) * mapped.RowPitch;
					const auto* white = static_cast<const std::uint8_t*>(mappedWhite.pData) + static_cast<std::size_t>(y) * mappedWhite.RowPitch;
					auto*       out = pixels.data() + static_cast<std::size_t>(y) * size * 4;
					for (std::uint32_t x = 0; x < size; ++x) {
						// Over black a pixel is colour * alpha; over white, that plus (1 - alpha).
						int difference = 0;
						for (int c = 0; c < 3; ++c) {
							difference += white[x * 4 + c] - black[x * 4 + c];
						}
						const int alpha = std::clamp(255 - difference / 3, 0, 255);
						for (int c = 0; c < 3; ++c) {
							const int channel = bgra ? 2 - c : c;
							const int premultiplied = black[x * 4 + channel];
							out[x * 4 + c] = alpha ? static_cast<std::uint8_t>(std::min(255, premultiplied * 255 / alpha)) : 0;
						}
						out[x * 4 + 3] = static_cast<std::uint8_t>(alpha);
						lit += alpha > 8 ? 1 : 0;
					}
				}
				context->Unmap(readback, 0);
				context->Unmap(readbackWhite, 0);

				if (lit == 0) {
					// Nothing was painted: an empty picture is worse than the icon.
					logger::warn("Thumbnail {:08X}: the engine painted nothing ({}x{} at {},{})", a_formID, size, size, rect->left,
						rect->top);
				} else {
					const auto pictureSize = std::min(size, PICTURE_SIZE);
					const auto path = FilePath(a_formID);
					ok = WritePng(path, Shrink(pixels, size, pictureSize).data(), pictureSize);
					if (ok) {
						logger::info("Thumbnail {:08X}: saved {}x{} (from {}x{} at {},{}, {}% painted)", a_formID, pictureSize, pictureSize,
							size, size, rect->left, rect->top, lit * 100ull / (static_cast<std::uint64_t>(size) * size));
					} else {
						logger::warn("Thumbnail {:08X}: could not write {}", a_formID, path.string());
					}
				}
			} else if (mappedBlack) {
				context->Unmap(readback, 0);
			}
		} else {
			logger::warn("Thumbnail capture: could not create {}x{} textures", rect->size, rect->size);
		}

		if (saved) {
			saved->Release();
		}
		if (readback) {
			readback->Release();
		}
		if (readbackWhite) {
			readbackWhite->Release();
		}
		frame->Release();
		return ok;
	}

	void Finish(bool a_saved)
	{
		if (!current) {
			return;
		}
		const auto formID = currentID;
		{
			std::scoped_lock lock(stateLock);
			(a_saved ? cached : rejected).insert(formID);
			queued.erase(formID);
		}
		if (a_saved) {
			newPictures = true;
		} else {
			// Remember it, so later sessions do not pause on it again. Deleting
			// the pictures folder (or this file) retries everything.
			std::ofstream failed(std::filesystem::path(CACHE_DIR) / FAILED_FILE, std::ios::app);
			failed << std::format("{:08X}\n", formID);
		}
		current = nullptr;
		working = false;
	}

	// Ends the session when the scene allows it. True once it has ended.
	bool TryTeardown(RE::Inventory3DManager* a_manager)
	{
		if (!inSession) {
			return true;
		}
		if (const char* why = UnsafeToTearDown(a_manager)) {
			if (++teardownWaited < TEARDOWN_FRAMES) {
				return false;
			}
			// Leave the Begin3D unpaired rather than risk the crash; the next
			// inventory open or session resets it.
			logger::warn("Thumbnails: skipping End3D after {} frames ({})", TEARDOWN_FRAMES, why);
		} else {
			a_manager->End3D();  // no UnloadInventoryItem before it, on purpose
		}
		inSession = false;
		teardownWaited = 0;
		return true;
	}

	// Once per frame, from the capture menu's PostDisplay.
	void Step()
	{
		static bool announced = false;
		if (!announced) {
			announced = true;
			logger::info("Thumbnails: capture menu is being drawn");
		}

		auto* manager = RE::Inventory3DManager::GetSingleton();
		if (!manager) {
			return;
		}

		// Stop: the screen closed or there is nothing left to do.
		const auto stop = [&]() {
			if (current) {
				std::scoped_lock lock(stateLock);
				queued.erase(currentID);
				current = nullptr;
				working = false;
			}
			if (TryTeardown(manager)) {
				sessionOver = true;
			}
		};

		if (!enabled) {
			// The screen closed: end the session only if that is safe right now.
			if (current) {
				std::scoped_lock lock(stateLock);
				queued.erase(currentID);
				current = nullptr;
				working = false;
			}
			if (inSession && !UnsafeToTearDown(manager)) {
				manager->End3D();
				inSession = false;
			}
			sessionOver = true;
			return;
		}

		if (!current) {
			RE::FormID next = 0;
			{
				std::scoped_lock lock(stateLock);
				if (!queue.empty()) {
					next = queue.front();
					queue.pop_front();
				}
			}
			if (next == 0) {
				stop();
				if (sessionOver) {
					batchDone = true;
				}
				return;
			}

			// A spell is pictured by the model the Magic menu shows for it.
			if (auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(next)) {
				current = spell->GetMenuDisplayObject();
			} else {
				current = RE::TESForm::LookupByID<RE::TESBoundObject>(next);
			}
			currentID = next;
			if (!current || current->Is(RE::FormType::LeveledItem)) {
				std::scoped_lock lock(stateLock);
				rejected.insert(next);
				queued.erase(next);
				current = nullptr;
				return;
			}
			working = true;
			framesWaited = 0;
			framesLoaded = 0;

			if (!inSession) {
				manager->Begin3D(RE::INTERFACE_LIGHT_SCHEME::kInventory);
				inSession = true;
			} else if (manager->GetRuntimeData().loadedModels.size() >= RESET_AT) {
				if (UnsafeToTearDown(manager)) {
					// Put the item back and try again next frame.
					std::scoped_lock lock(stateLock);
					queue.push_front(next);
					current = nullptr;
					working = false;
					return;
				}
				manager->End3D();
				manager->Begin3D(RE::INTERFACE_LIGHT_SCHEME::kInventory);
			}
			HealGroundModel(current);
			manager->UnloadInventoryItem();
			manager->LoadInventoryItem(current, nullptr);
			return;
		}

		auto* model = CurrentModel(manager);
		if (!model || model->worldBound.radius <= 0.0f || framesLoaded + 1 < SETTLE_FRAMES) {
			// The engine finishes loading the preview inside Render(); vanilla
			// and Grid Inventory call it every frame. Only models already in
			// memory loaded without it (4 of 12 here; the rest stayed pending).
			PumpRender(manager);
		}
		if (!model || model->worldBound.radius <= 0.0f) {
			if (++framesWaited > GIVE_UP_FRAMES) {
				const auto& runtime = manager->GetRuntimeData();
				logger::info("Thumbnail {:08X}: model did not load in {} frames (load task {}, {} model(s) held, match {}, radius {:.1f})",
					currentID, GIVE_UP_FRAMES, runtime.loadTask ? "pending" : "none", runtime.loadedModels.size(),
					model ? "found" : "none", model ? model->worldBound.radius : 0.0f);
				Finish(false);
			}
			return;
		}
		if (++framesLoaded < SETTLE_FRAMES) {
			return;
		}
		Finish(Capture(manager, model, currentID));
	}

	// ---- the capture menu ----------------------------------------------------

	// No Flash movie and no input. kCustomRendering makes the game call our
	// PostDisplay in its menu drawing pass; kUsesMenuContext puts the game in
	// menu mode; kPausesGame is required: Inventory3DManager::Render() draws
	// nothing while the game runs (Grid Inventory's GridMenu.cpp says so, citing
	// Modex's "show3DPreview requires pauseGame"). Measured here too: without
	// it loads stayed pending and nothing was painted. So the game pauses while
	// pictures are being made, about 0.1 s per item.
	// The session begins when the menu is shown, as both references do.
	class CaptureMenu : public RE::IMenu
	{
	public:
		CaptureMenu()
		{
			menuFlags.set(RE::UI_MENU_FLAGS::kCustomRendering, RE::UI_MENU_FLAGS::kUsesMenuContext,
				RE::UI_MENU_FLAGS::kPausesGame, RE::UI_MENU_FLAGS::kDisablePauseMenu, RE::UI_MENU_FLAGS::kAllowSaving);
			depthPriority = 0;
		}

		void PostDisplay() override { Step(); }

		RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& a_message) override
		{
			if (a_message.type == RE::UI_MESSAGE_TYPE::kShow && !inSession) {
				if (auto* manager = RE::Inventory3DManager::GetSingleton()) {
					manager->Begin3D(RE::INTERFACE_LIGHT_SCHEME::kInventory);
					inSession = true;
					logger::info("Thumbnails: preview session begun");
				}
			}
			return RE::IMenu::ProcessMessage(a_message);
		}

		static RE::IMenu* Create() { return new CaptureMenu(); }
	};

	void SendMenuMessage(RE::UI_MESSAGE_TYPE a_type)
	{
		if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
			queue->AddMessage(MENU_NAME, a_type, nullptr);
		}
	}
}

void Thumbnails::Install()
{
	std::error_code error;
	std::filesystem::create_directories(CACHE_DIR, error);
	if (error) {
		logger::error("Thumbnails: cannot create {}: {}", CACHE_DIR, error.message());
		return;
	}

	{
		std::scoped_lock lock(stateLock);
		for (const auto& entry : std::filesystem::directory_iterator(CACHE_DIR, error)) {
			RE::FormID formID = 0;
			const auto stem = entry.path().stem().string();
			const auto [end, parseError] = std::from_chars(stem.data(), stem.data() + stem.size(), formID, 16);
			if (parseError == std::errc{} && end == stem.data() + stem.size() && entry.path().extension() == ".png") {
				cached.insert(formID);
			}
		}
		std::ifstream failed(std::filesystem::path(CACHE_DIR) / FAILED_FILE);
		for (std::string line; std::getline(failed, line);) {
			RE::FormID formID = 0;
			const auto [end, parseError] = std::from_chars(line.data(), line.data() + line.size(), formID, 16);
			if (parseError == std::errc{} && formID != 0) {
				rejected.insert(formID);
			}
		}
		logger::info("Thumbnails: {} cached picture(s), {} known failure(s) in {}", cached.size(), rejected.size(), CACHE_DIR);
	}

	if (auto* ui = RE::UI::GetSingleton()) {
		ui->Register(MENU_NAME, CaptureMenu::Create);
		menuRegistered = true;
	}
}

std::string Thumbnails::UrlFor(RE::FormID a_formID)
{
	std::scoped_lock lock(stateLock);
	return cached.contains(a_formID) ? std::format("{}{:08X}.png", CACHE_URL, a_formID) : std::string();
}

void Thumbnails::Request(RE::TESForm* a_form)
{
	if (!menuRegistered || !making || !a_form) {
		return;
	}
	// Leveled lists have no model; a spell only through its Magic menu display
	// object. Created (0xFF...) forms get new ids per save, so a cached file
	// would be shown for the wrong item later.
	if (const auto* spell = a_form->As<RE::SpellItem>(); spell && !spell->GetMenuDisplayObject()) {
		return;
	}
	if (a_form->Is(RE::FormType::LeveledItem) || a_form->IsDynamicForm()) {
		return;
	}
	const auto       formID = a_form->GetFormID();
	std::scoped_lock lock(stateLock);
	if (cached.contains(formID) || rejected.contains(formID) || !queued.insert(formID).second) {
		return;
	}
	queue.push_back(formID);
}

std::size_t Thumbnails::Pending()
{
	std::scoped_lock lock(stateLock);
	return queue.size() + (working ? 1 : 0);
}

void Thumbnails::Tick(const std::function<void()>& a_onBatchDone)
{
	if (!menuRegistered) {
		return;
	}
	enabled = true;

	if (!menuOpen && Pending() > 0) {
		sessionOver = false;
		SendMenuMessage(RE::UI_MESSAGE_TYPE::kShow);
		menuOpen = true;
	} else if (menuOpen && sessionOver) {
		SendMenuMessage(RE::UI_MESSAGE_TYPE::kHide);
		menuOpen = false;
	}

	const bool done = batchDone.exchange(false);
	if (done) {
		logger::info("Thumbnails: batch done, {} picture(s) cached", [] {
			std::scoped_lock lock(stateLock);
			return cached.size();
		}());
	}
	// Tell the page as pictures arrive, not only at the end: a batch can take
	// a while, and one stuck item used to keep every finished picture hidden.
	if (newPictures.exchange(false) || done) {
		a_onBatchDone();
	}
}

void Thumbnails::Abort()
{
	// Close the capture menu at once: it pauses the game, and waiting for a
	// stuck load here froze the game for seconds after F4. PostDisplay gets one
	// more frame to end the session if that is safe; if not, the session stays
	// open and is reused by the next open (kShow does not begin a second one).
	enabled = false;
}

void Thumbnails::AbortTick()
{
	if (menuOpen && !enabled) {
		SendMenuMessage(RE::UI_MESSAGE_TYPE::kHide);
		menuOpen = false;
	}
}

void Thumbnails::SetEnabled(bool a_enabled)
{
	making = a_enabled;
}
