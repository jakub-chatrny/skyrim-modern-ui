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

	// ---- the Inventory tab's live 3D preview ---------------------------------
	//
	// Same scene as the pictures, so it waits for them: while any picture is
	// queued the preview is not drawn. Then the item is loaded, scaled and
	// moved so its bounding sphere fills the page's preview box, turned, and
	// Render() paints it straight into the frame (no save / restore), where
	// the page has left that box transparent.
	//
	// The engine keeps one node per model file and reuses it, so whatever we
	// do to the node (scale, turn, move) is undone before anything else uses
	// the scene: another preview item, a picture, the end of the session.

	struct PreviewRect
	{
		float left = 0.0f, top = 0.0f, width = 0.0f, height = 0.0f;  // fractions of the screen
	};
	std::atomic<RE::FormID> previewID{ 0 };  // main thread writes, PostDisplay reads
	PreviewRect             previewRect;     // guarded by stateLock
	std::atomic<float>      previewTurn{ 0.0f };   // degrees asked for since PostDisplay last looked
	std::atomic<int>        previewIdle{ 0 };      // frames since the player last turned it

	// PostDisplay only.
	RE::FormID          previewLoaded = 0;
	RE::TESBoundObject* previewObject = nullptr;
	float               previewAngle = 0.0f;  // radians
	struct SavedNode
	{
		RE::NiPointer<RE::NiAVObject> node;
		RE::NiMatrix3                 rotate;
		RE::NiPoint3                  translate;
		float                         scale = 1.0f;
	} previewSaved;

	// The slow turn while nobody turns it, degrees per frame, and how long a
	// turn by the player holds it still.
	constexpr float PREVIEW_SPIN = 0.35f;
	constexpr int   PREVIEW_HOLD_FRAMES = 180;
	// Share of the box the bounding sphere's diameter fills.
	constexpr float PREVIEW_FILL = 0.9f;

	void RestorePreviewNode()
	{
		auto& saved = previewSaved;
		if (!saved.node) {
			return;
		}
		saved.node->local.rotate = saved.rotate;
		saved.node->local.translate = saved.translate;
		saved.node->local.scale = saved.scale;
		RE::NiUpdateData update;
		saved.node->Update(update);
		saved.node.reset();
	}

	void ForgetPreview()
	{
		RestorePreviewNode();
		previewLoaded = 0;
		previewObject = nullptr;
	}

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

	// The loaded model for an item: by item, or else an entry with the same
	// model file (the engine shares one entry between items that use one file).
	RE::NiAVObject* ModelOf(RE::Inventory3DManager* a_manager, RE::TESBoundObject* a_object)
	{
		auto& models = a_manager->GetRuntimeData().loadedModels;
		for (auto& entry : models) {
			if ((entry.itemBase == a_object || entry.modelObj == a_object) && entry.spModel) {
				return entry.spModel.get();
			}
		}
		const char* path = ModelPath(a_object);
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

	RE::NiAVObject* CurrentModel(RE::Inventory3DManager* a_manager) { return ModelOf(a_manager, current); }

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
		ForgetPreview();
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

	// Paints the preview box in the page's background colour (#070B10), so
	// the box does not show whatever the frame had there. Values are sRGB;
	// an sRGB render target takes linear ones.
	void ClearPreviewBox(const PreviewRect& a_box)
	{
		auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
		const auto screen = RE::BSGraphics::Renderer::GetScreenSize();
		if (!renderer || screen.width == 0 || screen.height == 0) {
			return;
		}
		const auto& data = renderer->GetRuntimeData();
		auto*       context = reinterpret_cast<ID3D11DeviceContext*>(data.context);
		auto*       target = reinterpret_cast<ID3D11RenderTargetView*>(data.renderWindows[0].renderView);
		if (!context || !target) {
			return;
		}
		D3D11_RENDER_TARGET_VIEW_DESC view{};
		target->GetDesc(&view);
		const bool srgb = view.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || view.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		const auto channel = [&](float a_srgb) {
			return srgb ? (a_srgb <= 0.04045f ? a_srgb / 12.92f : std::pow((a_srgb + 0.055f) / 1.055f, 2.4f)) : a_srgb;
		};
		const FLOAT colour[4]{ channel(7.0f / 255.0f), channel(11.0f / 255.0f), channel(16.0f / 255.0f), 1.0f };

		// The frame may be larger than the screen size the UI works in.
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
		D3D11_TEXTURE2D_DESC desc{};
		frame->GetDesc(&desc);
		frame->Release();
		const D3D11_RECT rect{ static_cast<LONG>(a_box.left * desc.Width), static_cast<LONG>(a_box.top * desc.Height),
			static_cast<LONG>((a_box.left + a_box.width) * desc.Width), static_cast<LONG>((a_box.top + a_box.height) * desc.Height) };

		ID3D11DeviceContext1* context1 = nullptr;
		context->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&context1));
		if (context1) {
			context1->ClearView(target, colour, &rect, 1);
			context1->Release();
		}
	}

	// Scale and move the preview model so its bounding sphere fills the page's
	// box, turned by previewAngle about the vertical. Same projection as
	// ModelRect (and Grid Inventory's ItemPreview::UpdateParking), inverted.
	void PlacePreview(RE::NiAVObject* a_model)
	{
		auto* scene = RE::UI3DSceneManager::GetSingleton();
		const auto screen = RE::BSGraphics::Renderer::GetScreenSize();
		if (!scene || screen.width == 0 || screen.height == 0) {
			return;
		}
		PreviewRect box;
		{
			std::scoped_lock lock(stateLock);
			box = previewRect;
		}
		const float boxLeft = box.left * screen.width;
		const float boxTop = box.top * screen.height;
		const float boxWidth = box.width * screen.width;
		const float boxHeight = box.height * screen.height;
		if (boxWidth < 8.0f || boxHeight < 8.0f) {
			return;
		}

		if (previewSaved.node.get() != a_model) {
			RestorePreviewNode();
			previewSaved = { RE::NiPointer<RE::NiAVObject>(a_model), a_model->local.rotate, a_model->local.translate, a_model->local.scale };
		}

		// Turn, from the pose the engine gave it.
		RE::NiMatrix3 spin;
		spin.SetEulerAnglesXYZ(0.0f, 0.0f, previewAngle);
		a_model->local.rotate = spin * previewSaved.rotate;
		a_model->local.scale = previewSaved.scale;
		RE::NiUpdateData update;
		a_model->Update(update);

		const auto& frustum = scene->viewFrustum;
		const float depth = a_model->local.translate.y;
		const float minX = -frustum.fLeft * depth;
		const float minZ = -frustum.fBottom * depth;
		const float ratioX = (-frustum.fRight * depth - minX) / static_cast<float>(screen.width);
		const float ratioY = (-frustum.fTop * depth - minZ) / static_cast<float>(screen.height);
		if (ratioX == 0.0f || ratioY == 0.0f || !std::isfinite(ratioX) || !std::isfinite(ratioY)) {
			return;
		}

		// Size: the sphere's diameter on screen to PREVIEW_FILL of the box.
		const float radiusPixels = a_model->worldBound.radius / std::abs(ratioX);
		if (radiusPixels > 0.5f) {
			const float factor = std::clamp(PREVIEW_FILL * std::min(boxWidth, boxHeight) / (2.0f * radiusPixels), 0.02f, 50.0f);
			a_model->local.scale = previewSaved.scale * factor;
			a_model->Update(update);
		}

		// Place: the sphere's centre on the box's centre.
		const auto& centre = a_model->worldBound.center;
		const float modelX = -(centre.x + minX) / ratioX;
		const float modelY = -(centre.z + minZ) / ratioY;
		const float dx = boxLeft + boxWidth / 2.0f - modelX;
		const float dy = boxTop + boxHeight / 2.0f - modelY;
		if (std::isfinite(dx) && std::isfinite(dy)) {
			a_model->local.translate.x += -dx * ratioX;
			a_model->local.translate.z += -dy * ratioY;
			a_model->Update(update);
		}
	}

	// Once per frame while the preview is wanted and no picture is being made.
	void PreviewStep(RE::Inventory3DManager* a_manager)
	{
		const auto id = previewID.load();
		if (!inSession) {
			a_manager->Begin3D(RE::INTERFACE_LIGHT_SCHEME::kInventory);
			inSession = true;
		}

		if (previewLoaded != id) {
			ForgetPreview();
			auto* object = RE::TESForm::LookupByID<RE::TESBoundObject>(id);
			if (!object || object->Is(RE::FormType::LeveledItem)) {
				previewLoaded = id;  // nothing to show; the box stays empty
				return;
			}
			if (a_manager->GetRuntimeData().loadedModels.size() >= RESET_AT) {
				if (UnsafeToTearDown(a_manager)) {
					return;  // next frame
				}
				a_manager->End3D();
				a_manager->Begin3D(RE::INTERFACE_LIGHT_SCHEME::kInventory);
			}
			HealGroundModel(object);
			a_manager->UnloadInventoryItem();
			a_manager->LoadInventoryItem(object, nullptr);
			previewLoaded = id;
			previewObject = object;
			previewAngle = 0.0f;
			logger::info("Preview: {} ({:08X})", object->GetName(), id);
			return;
		}
		if (!previewObject) {
			return;
		}

		auto* model = ModelOf(a_manager, previewObject);
		if (!model || model->worldBound.radius <= 0.0f) {
			// Loading finishes inside Render(); keep it off screen until then.
			PumpRender(a_manager);
			return;
		}

		// Turn: what the player asked for, else a slow spin once they let go.
		const float asked = previewTurn.exchange(0.0f);
		if (asked != 0.0f) {
			previewIdle = 0;
		}
		const int   idle = previewIdle.fetch_add(1);
		const float degrees = asked + (idle > PREVIEW_HOLD_FRAMES ? PREVIEW_SPIN : 0.0f);
		previewAngle = std::fmod(previewAngle + degrees * 0.017453292f, 6.2831853f);

		PlacePreview(model);
		{
			PreviewRect box;
			{
				std::scoped_lock lock(stateLock);
				box = previewRect;
			}
			ClearPreviewBox(box);
		}
		a_manager->Render();  // into the frame, under the page's transparent box
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
			ForgetPreview();
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
			if (next == 0 && previewID.load() != 0) {
				PreviewStep(manager);
				return;
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
			// Pictures first: the preview item is loaded again afterwards.
			ForgetPreview();

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

	// Pictures from before v2 (opaque, on black) sit in the parent folder and
	// are no longer shown; remove them, and their failure list.
	{
		const auto oldDir = std::filesystem::path(CACHE_DIR).parent_path();
		std::size_t removed = 0;
		for (const auto& entry : std::filesystem::directory_iterator(oldDir, error)) {
			const auto extension = entry.path().extension();
			if (entry.is_regular_file() && (extension == ".png" || entry.path().filename() == FAILED_FILE)) {
				std::error_code removeError;
				removed += std::filesystem::remove(entry.path(), removeError) ? 1 : 0;
			}
		}
		if (removed > 0) {
			logger::info("Thumbnails: removed {} old picture file(s) from {}", removed, oldDir.string());
		}
		error.clear();
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

	const bool previewing = previewID.load() != 0;
	if (!menuOpen && (Pending() > 0 || previewing)) {
		sessionOver = false;
		SendMenuMessage(RE::UI_MESSAGE_TYPE::kShow);
		menuOpen = true;
	} else if (menuOpen && sessionOver && !previewing) {
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

void Thumbnails::SetPreview(RE::FormID a_formID, float a_left, float a_top, float a_width, float a_height)
{
	{
		std::scoped_lock lock(stateLock);
		previewRect = { a_left, a_top, a_width, a_height };
	}
	// The preview needs the capture menu, whether or not pictures are made.
	previewID = menuRegistered ? a_formID : 0;
}

void Thumbnails::TurnPreview(float a_degrees)
{
	float now = previewTurn.load();
	while (!previewTurn.compare_exchange_weak(now, now + a_degrees)) {
	}
}

void Thumbnails::SetEnabled(bool a_enabled)
{
	making = a_enabled;
}
