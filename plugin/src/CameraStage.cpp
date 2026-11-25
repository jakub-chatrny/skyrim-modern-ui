#include "PCH.h"

#include "CameraStage.h"

#include <fstream>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <unordered_set>
#include <vector>

namespace
{
	constexpr auto CONFIG_PATH = "Data/SKSE/Plugins/SkyrimModernUI.json";

	// INI settings the third-person camera reads every update. Overridden while
	// the stage is active, restored on Leave().
	class SettingOverrides
	{
	public:
		void Set(const char* a_name, float a_value)
		{
			auto* setting = RE::INISettingCollection::GetSingleton()->GetSetting(a_name);
			if (!setting) {
				logger::warn("INI setting not found: {}", a_name);
				return;
			}
			_saved.emplace_back(setting, setting->data.f);
			setting->data.f = a_value;
		}

		void Restore()
		{
			for (auto it = _saved.rbegin(); it != _saved.rend(); ++it) {
				it->first->data.f = it->second;
			}
			_saved.clear();
		}

	private:
		std::vector<std::pair<RE::Setting*, float>> _saved;
	};

	struct Saved
	{
		bool         active = false;
		bool         wasFirstPerson = false;
		RE::NiPoint2 freeRotation;
		bool         freeRotationEnabled = false;
		bool         toggleAnimCam = false;
		RE::NiPoint3 posOffsetExpected;
		float        targetZoomOffset = 0.0f;
		float        pitchZoomOffset = 0.0f;
		float        worldFOV = 0.0f;
		float        playerPitch = 0.0f;
		float        playerYaw = 0.0f;
		bool         turned = false;  // the player was turned to face open space
		RE::NiPoint3 playerPosition;  // where the player stood when the screen opened
		int          frames = 0;      // Tick() calls since Enter (or the last retry)
		float        distance = 0.0f; // camera distance in use
		float        fov = 0.0f;      // field of view held while the camera is closer (0 = none)
	};

	Saved            saved;
	SettingOverrides settings;

	// Scene objects culled by HideWorld(), kept alive until ShowWorld().
	std::vector<RE::NiPointer<RE::NiAVObject>> culled;
	bool                                       hudWasVisible = false;
	// The player's parts culled for the Inventory tab (not the stage's own nodes).
	std::vector<RE::NiPointer<RE::NiAVObject>> hiddenParts;
	bool                                       characterHidden = false;
	bool                                       hudHidden = false;

	bool IsAncestorOf(const RE::NiAVObject* a_ancestor, const RE::NiAVObject* a_object)
	{
		for (auto* it = a_object; it; it = it->parent) {
			if (it == a_ancestor) {
				return true;
			}
		}
		return false;
	}

	// Culls an object and remembers it. If the game has un-culled something we
	// culled earlier (it does this to actor roots every frame), cull it again
	// without recording it twice.
	std::unordered_set<RE::NiAVObject*> culledSet;

	void Cull(RE::NiAVObject* a_object)
	{
		if (!a_object || a_object->GetAppCulled()) {
			return;
		}
		a_object->SetAppCulled(true);
		if (culledSet.insert(a_object).second) {
			culled.emplace_back(a_object);
		}
	}

	void CullWithChildren(RE::NiAVObject* a_object)
	{
		if (!a_object) {
			return;
		}
		Cull(a_object);
		if (auto* node = a_object->AsNode()) {
			for (auto& child : node->GetChildren()) {
				Cull(child.get());
			}
		}
	}

	// The game resets the culled flag on an actor's root node each frame (fade
	// handling), so hiding the root does not stick. Hide what hangs below it.
	void CullActor(RE::Actor* a_actor)
	{
		auto* root = a_actor->Get3D(false);
		auto* node = root ? root->AsNode() : nullptr;
		if (!node) {
			return;
		}
		for (auto& child : node->GetChildren()) {
			Cull(child.get());
		}
	}

	void CullOtherActors(RE::PlayerCharacter* a_player)
	{
		auto* lists = RE::ProcessLists::GetSingleton();
		if (!lists) {
			return;
		}
		for (const auto* handles : { &lists->highActorHandles, &lists->middleHighActorHandles }) {
			for (const auto& handle : *handles) {
				const auto actor = handle.get();
				if (actor && actor.get() != a_player) {
					CullActor(actor.get());
				}
			}
		}
	}

	// Walk from the player's 3D up to the scene root and cull every sibling on
	// the way, i.e. everything that is not the player or one of its ancestors.
	// Safe to repeat: already-culled objects are skipped.
	std::size_t CullPass(RE::PlayerCharacter* a_player)
	{
		const auto* camera = RE::Main::WorldRootCamera();
		auto*       player3D = a_player->Get3D(false);
		if (!player3D) {
			return 0;
		}

		// The interface 3D scene (where the inventory preview, and so the item
		// pictures, are drawn) may hang in the same scene graph as the world.
		const auto* ui3d = RE::UI3DSceneManager::GetSingleton();
		const auto* interfaceScene = ui3d ? static_cast<const RE::NiAVObject*>(ui3d->shadowSceneNode) : nullptr;

		std::size_t depth = 0;
		for (RE::NiAVObject* child = player3D; child->parent; child = child->parent, ++depth) {
			for (auto& sibling : child->parent->GetChildren()) {
				// Never cull the branch holding the world camera or the interface scene.
				if (sibling && sibling.get() != child && !IsAncestorOf(sibling.get(), camera) &&
					!(interfaceScene && IsAncestorOf(sibling.get(), interfaceScene))) {
					Cull(sibling.get());
				}
			}
		}

		static bool reported = false;
		if (!reported) {
			reported = true;
			const RE::NiAVObject* worldTop = player3D;
			while (worldTop->parent) {
				worldTop = worldTop->parent;
			}
			logger::info("HideWorld: interface scene {} the world's scene graph (world top {}, interface top {})",
				interfaceScene && IsAncestorOf(worldTop, interfaceScene) ? "IS IN" : "is not in", fmt::ptr(worldTop),
				[&]() {
					const RE::NiAVObject* top = interfaceScene;
					while (top && top->parent) {
						top = top->parent;
					}
					return fmt::ptr(top);
				}());
		}

		// The sky hangs outside the player's branch on some paths; cull it explicitly.
		// Like actor roots, the sky's own root appears to be un-culled by the game
		// every frame (the dome kept drawing), so its children are culled as well.
		if (auto* sky = RE::Sky::GetSingleton()) {
			CullWithChildren(sky->root.get());
			CullWithChildren(sky->moonsRoot.get());
			CullWithChildren(sky->auroraRoot.get());
		}

		CullOtherActors(a_player);
		return depth;
	}

	// Turn the fog black at its source (the weather
	// record outdoors, the cell's lighting indoors). On its own this only shifted
	// the outdoor colour (beige -> brown) while the sky dome was still drawing,
	// and did nothing indoors.
	struct SavedColor
	{
		RE::Color* target;
		RE::Color  value;
	};
	std::vector<SavedColor> fogColors;

	void Blacken(RE::Color& a_color)
	{
		fogColors.push_back({ std::addressof(a_color), a_color });
		a_color.red = a_color.green = a_color.blue = 0;
	}

	void BlackenWeather(RE::TESWeather* a_weather)
	{
		if (!a_weather) {
			return;
		}
		using Type = RE::TESWeather::ColorTypes;
		for (std::uint32_t time = 0; time < RE::TESWeather::ColorTime::kTotal; ++time) {
			Blacken(a_weather->colorData[Type::kFogNear][time]);
			Blacken(a_weather->colorData[Type::kFogFar][time]);
		}
	}

	void BlackenFog(RE::PlayerCharacter* a_player)
	{
		auto* cell = a_player->GetParentCell();
		if (cell && cell->IsInteriorCell()) {
			if (auto* lighting = cell->GetLighting()) {
				Blacken(lighting->fogColorNear);
				Blacken(lighting->fogColorFar);
			}
			logger::info("Fog: interior cell, {} colours blackened", fogColors.size());
			return;
		}

		if (auto* sky = RE::Sky::GetSingleton()) {
			BlackenWeather(sky->currentWeather);
			if (sky->lastWeather != sky->currentWeather) {
				BlackenWeather(sky->lastWeather);
			}
			// Take effect this frame instead of after the next sky update.
			using Type = RE::TESWeather::ColorTypes;
			sky->skyColor[Type::kFogNear] = RE::NiColor(0.0f, 0.0f, 0.0f);
			sky->skyColor[Type::kFogFar] = RE::NiColor(0.0f, 0.0f, 0.0f);
		}
		logger::info("Fog: exterior, {} weather colours blackened", fogColors.size());
	}

	// Black fog still dims whatever it covers, the character included, and the
	// backdrop picks it up too. Outdoors, switch the weather's fog off entirely
	// (maximum opacity 0, range pushed far away).
	struct SavedFogRange
	{
		RE::TESWeather*         weather;
		RE::TESWeather::FogData value;
	};
	std::vector<SavedFogRange> fogRanges;

	void RemoveWeatherFog(RE::TESWeather* a_weather)
	{
		if (!a_weather) {
			return;
		}
		fogRanges.push_back({ a_weather, a_weather->fogData });
		auto& fog = a_weather->fogData;
		fog.dayNear = fog.nightNear = 1.0e6f;
		fog.dayFar = fog.nightFar = 2.0e6f;
		fog.dayMax = fog.nightMax = 0.0f;
	}

	void RemoveFog()
	{
		if (auto* sky = RE::Sky::GetSingleton()) {
			RemoveWeatherFog(sky->currentWeather);
			if (sky->lastWeather != sky->currentWeather) {
				RemoveWeatherFog(sky->lastWeather);
			}
		}
		logger::info("Fog: switched off in {} weather record(s)", fogRanges.size());
	}

	void RestoreFog()
	{
		for (auto it = fogRanges.rbegin(); it != fogRanges.rend(); ++it) {
			it->weather->fogData = it->value;
		}
		fogRanges.clear();
		for (auto it = fogColors.rbegin(); it != fogColors.rend(); ++it) {
			*it->target = it->value;
		}
		fogColors.clear();
	}

	// Each frame the sky recomputes its fog values from the weather; keep its
	// current values in line so the change is visible immediately.
	void ReapplyFog()
	{
		auto* sky = RE::Sky::GetSingleton();
		if (!sky) {
			return;
		}
		if (!fogColors.empty()) {
			using Type = RE::TESWeather::ColorTypes;
			sky->skyColor[Type::kFogNear] = RE::NiColor(0.0f, 0.0f, 0.0f);
			sky->skyColor[Type::kFogFar] = RE::NiColor(0.0f, 0.0f, 0.0f);
		}
		if (!fogRanges.empty()) {
			sky->fogClamp = 0.0f;
		}
	}

	// Black backdrop: the game's own black plane mesh (used in dungeons to block
	// light), hung on the player's 3D root a little behind the character. Being a
	// child of the player it is never touched by the cull pass and follows the
	// player's facing. The mesh is a 512 x 512 square lying flat (normal along Z)
	// and single-sided; which way it faces is unknown, so two copies are attached
	// back to back.
	constexpr auto BACKDROP_MODEL = "markers\\blackplane01.nif";

	RE::NiPointer<RE::NiNode>              backdropParent;
	std::vector<RE::NiPointer<RE::NiNode>> backdropPlanes;

	RE::NiMatrix3 RotationAboutX(float a_radians)
	{
		const float c = std::cos(a_radians);
		const float s = std::sin(a_radians);
		RE::NiMatrix3 m;
		m.entry[0][0] = 1.0f; m.entry[0][1] = 0.0f; m.entry[0][2] = 0.0f;
		m.entry[1][0] = 0.0f; m.entry[1][1] = c;    m.entry[1][2] = -s;
		m.entry[2][0] = 0.0f; m.entry[2][1] = s;    m.entry[2][2] = c;
		return m;
	}

	// The plane's texture is black, but it is a normally lit surface: outdoors the
	// sun still put a specular sheen on it (brown, with the character's shadow
	// visible in it). Zero the specular response. The material is shared with the
	// game's own black planes; losing their sheen for the rest of the session is
	// harmless, they exist to be black.
	void MakeMatte(RE::NiAVObject* a_object)
	{
		RE::BSVisit::TraverseScenegraphGeometries(a_object, [](RE::BSGeometry* a_geometry) {
			auto* property = a_geometry->GetGeometryRuntimeData().shaderProperty.get();
			auto* material = property ? static_cast<RE::BSLightingShaderMaterialBase*>(property->material) : nullptr;
			if (material) {
				material->specularColor = RE::NiColor(0.0f, 0.0f, 0.0f);
				material->specularColorScale = 0.0f;
			}
			return RE::BSVisit::BSVisitControl::kContinue;
		});
	}

	void AttachBackdrop(RE::PlayerCharacter* a_player, const CameraStage::Config& a_config)
	{
		auto* root = a_player->Get3D(false);
		auto* parent = root ? root->AsNode() : nullptr;
		if (!parent) {
			logger::warn("Backdrop: player has no third-person 3D node");
			return;
		}

		RE::NiPointer<RE::NiNode>           model;
		const RE::BSModelDB::DBTraits::ArgsType args{};
		const auto                          result = RE::BSModelDB::Demand(BACKDROP_MODEL, model, args);
		if (!model) {
			logger::warn("Backdrop: could not load {} (result {})", BACKDROP_MODEL, static_cast<int>(result));
			return;
		}

		// Actors face +Y, the camera stands in front of the player, so "behind the
		// character as seen by the camera" is -Y in the player's own space.
		const float halfPi = std::numbers::pi_v<float> / 2.0f;
		for (const float angle : { -halfPi, halfPi }) {
			auto* clone = model->Clone();
			auto* plane = clone ? clone->AsNode() : nullptr;
			if (!plane) {
				logger::warn("Backdrop: clone failed");
				continue;
			}
			MakeMatte(plane);
			plane->local.rotate = RotationAboutX(angle);
			plane->local.translate = RE::NiPoint3(0.0f, -a_config.backdropDistance, a_config.backdropHeight);
			plane->local.scale = a_config.backdropScale;
			parent->AttachChild(plane, true);
			backdropPlanes.emplace_back(plane);
		}

		backdropParent.reset(parent);
		RE::NiUpdateData update{};
		parent->Update(update);
		logger::info("Backdrop: attached {} plane(s), {} units behind, scale {}", backdropPlanes.size(),
			a_config.backdropDistance, a_config.backdropScale);
	}

	void DetachBackdrop()
	{
		if (backdropParent) {
			for (auto& plane : backdropPlanes) {
				backdropParent->DetachChild2(plane.get());
			}
		}
		backdropPlanes.clear();
		backdropParent.reset();
	}

	// Studio lights. The character is lit by the world (weather, time of day,
	// where they stand), so at night or indoors they come out dark. While the
	// screen is open two point lights hang in front of them, a warm key light
	// high on one side and a cooler, weaker fill on the other, attached to the
	// player's 3D like the backdrop and restricted to it, so nothing else is lit.
	RE::NiPointer<RE::NiNode>                studioParent;
	std::vector<RE::NiPointer<RE::NiPointLight>> studioLights;

	void AttachLights(RE::PlayerCharacter* a_player, const CameraStage::Config& a_config)
	{
		auto* root = a_player->Get3D(false);
		auto* parent = root ? root->AsNode() : nullptr;
		auto* scene = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];
		if (!parent || !scene) {
			logger::warn("Lights: no player 3D or scene to light");
			return;
		}

		struct Spec
		{
			RE::NiPoint3 position;  // player space: +Y toward the camera, +Z up
			RE::NiColor  color;
			float        strength;
		};
		const float k = a_config.lightStrength;
		const Spec  specs[]{
            { { -70.0f, 130.0f, 160.0f }, { 1.0f, 0.95f, 0.88f }, k },         // key: front left, above the head
            { { 90.0f, 110.0f, 80.0f }, { 0.78f, 0.86f, 1.0f }, k * 0.45f },  // fill: front right, chest height
		};

		RE::ShadowSceneNode::LIGHT_CREATE_PARAMS params{};
		params.dynamic = true;
		params.shadowLight = false;
		params.portalStrict = false;
		params.affectLand = false;
		params.affectWater = false;
		params.neverFades = true;
		params.fov = 0.0f;
		params.falloff = 1.0f;
		params.nearDistance = 5.0f;
		params.depthBias = 0.0f;
		params.sceneGraphIndex = 0;
		params.restrictedNode = parent;  // only the player is lit
		params.lensFlareData = nullptr;

		for (const auto& spec : specs) {
			auto* light = RE::NiPointLight::Create();
			if (!light) {
				continue;
			}
			auto& data = light->GetLightRuntimeData();
			data.ambient = RE::NiColor(0.0f, 0.0f, 0.0f);
			data.diffuse = spec.color;
			data.radius = RE::NiPoint3(a_config.lightRadius, a_config.lightRadius, a_config.lightRadius);
			data.fade = spec.strength;
			light->SetLightAttenuation(a_config.lightRadius);
			light->local.translate = spec.position;
			parent->AttachChild(light, true);
			scene->AddLight(light, params);
			studioLights.emplace_back(light);
		}
		studioParent.reset(parent);
		RE::NiUpdateData update{};
		parent->Update(update);
		logger::info("Lights: {} studio light(s), strength {}, radius {}", studioLights.size(), k, a_config.lightRadius);
	}

	void DetachLights()
	{
		auto* scene = RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];
		for (auto& light : studioLights) {
			if (scene) {
				scene->RemoveLight(light.get());
			}
			if (studioParent) {
				studioParent->DetachChild2(light.get());
			}
		}
		studioLights.clear();
		studioParent.reset();
	}

	bool worldHidden = false;

	void HideWorld(RE::PlayerCharacter* a_player, bool a_blackFog, bool a_noFog)
	{
		const auto depth = CullPass(a_player);
		if (a_blackFog) {
			BlackenFog(a_player);
		}
		if (a_noFog) {
			RemoveFog();
		}
		worldHidden = true;
		logger::info("HideWorld: culled {} scene objects across {} levels", culled.size(), depth);
	}

	void ShowWorld()
	{
		worldHidden = false;
		RestoreFog();
		for (auto& object : culled) {
			object->SetAppCulled(false);
		}
		culled.clear();
		culledSet.clear();
	}

	void SetHudHidden(bool a_hidden)
	{
		const auto ui = RE::UI::GetSingleton();
		const auto hud = ui ? ui->GetMenu(RE::HUDMenu::MENU_NAME) : nullptr;
		if (!hud || !hud->uiMovie) {
			return;
		}
		if (a_hidden) {
			hudWasVisible = hud->uiMovie->GetVisible();
			hud->uiMovie->SetVisible(false);
			hudHidden = true;
		} else if (hudHidden) {
			hud->uiMovie->SetVisible(hudWasVisible);
			hudHidden = false;
		}
	}

	RE::ThirdPersonState* ThirdPerson(RE::PlayerCamera* a_camera)
	{
		return static_cast<RE::ThirdPersonState*>(
			a_camera->GetRuntimeData().cameraStates[RE::CameraState::kThirdPerson].get());
	}
}

CameraStage::Config CameraStage::LoadConfig()
{
	Config config;
	std::ifstream file(CONFIG_PATH);
	if (!file) {
		return config;
	}

	const auto json = nlohmann::json::parse(file, nullptr, false);
	const auto camera = json.is_object() ? json.value("camera", nlohmann::json::object()) : nlohmann::json();
	if (!camera.is_object()) {
		logger::warn("{} is not valid JSON with a \"camera\" object; using defaults", CONFIG_PATH);
		return config;
	}

	const auto num = [&](const char* a_key, float a_default) {
		const auto it = camera.find(a_key);
		return it != camera.end() && it->is_number() ? it->get<float>() : a_default;
	};
	const auto flag = [&](const char* a_key, bool a_default) {
		const auto it = camera.find(a_key);
		return it != camera.end() && it->is_boolean() ? it->get<bool>() : a_default;
	};

	config.enabled = flag("enabled", config.enabled);
	config.itemPictures = flag("itemPictures", config.itemPictures);
	config.hideWorld = flag("hideWorld", config.hideWorld);
	config.backdrop = flag("backdrop", config.backdrop);
	config.backdropDistance = num("backdropDistance", config.backdropDistance);
	config.backdropHeight = num("backdropHeight", config.backdropHeight);
	config.backdropScale = num("backdropScale", config.backdropScale);
	config.blackFog = flag("blackFog", config.blackFog);
	config.noFog = flag("noFog", config.noFog);
	config.hideHud = flag("hideHud", config.hideHud);
	config.studioLights = flag("studioLights", config.studioLights);
	config.lightStrength = num("lightStrength", config.lightStrength);
	config.lightRadius = num("lightRadius", config.lightRadius);
	config.faceOpenSpace = flag("faceOpenSpace", config.faceOpenSpace);
	config.pauseGame = flag("pauseGame", config.pauseGame);
	config.distance = num("distance", config.distance);
	config.offsetX = num("offsetX", config.offsetX);
	config.offsetZ = num("offsetZ", config.offsetZ);
	config.rotation = num("rotation", config.rotation);
	config.pitch = num("pitch", config.pitch);
	config.fov = num("fov", config.fov);
	return config;
}

namespace
{
	// Room the camera needs beyond its distance (its collision sphere and a little more).
	constexpr float CAMERA_MARGIN = 40.0f;
	// Closest the camera comes in a tight corner; nearer, it ends up inside the character.
	constexpr float MIN_DISTANCE = 100.0f;

	// Walls. The third-person camera stops at the first wall between it and the
	// player. The world is hidden, but its collision is still there, so a player
	// standing in front of a wall is shown huge. Taking the camera's position over
	// did not work (docs/technical.md), so instead the player is turned, while the
	// screen is open, to face the most open direction: the camera sits in front
	// of the player and then has room. The heading is put back on close.

	// How far along the segment a_from -> a_to the first solid thing is, as a
	// fraction (1 = clear), for a ray in the given collision layer.
	float RayFraction(RE::bhkWorld* a_world, const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, RE::COL_LAYER a_layer)
	{
		const float scale = RE::bhkWorld::GetWorldScale();
		RE::bhkPickData pick;
		pick.rayInput.from = RE::hkVector4(a_from * scale);
		pick.rayInput.to = RE::hkVector4(a_to * scale);
		pick.rayInput.filterInfo.SetCollisionLayer(a_layer);
		{
			RE::BSReadLockGuard lock(a_world->worldLock);
			a_world->PickObject(pick);
		}
		return pick.rayOutput.HasHit() ? pick.rayOutput.hitFraction : 1.0f;
	}

	// Room for the camera in one horizontal direction, up to a_max. Twelve rays,
	// all starting on the player's own axis (a ray that starts beside the player
	// can start behind a wall and miss it), fanning out to a camera-wide band at
	// knee, chest, head and just above head height (roof edges).
	float Clearance(RE::bhkWorld* a_world, const RE::NiPoint3& a_feet, float a_yaw, float a_max)
	{
		const RE::NiPoint3 direction{ std::sin(a_yaw), std::cos(a_yaw), 0.0f };
		const RE::NiPoint3 side{ direction.y, -direction.x, 0.0f };
		constexpr float    HALF_WIDTH = 35.0f;
		float              fraction = 1.0f;
		for (const float height : { 40.0f, 90.0f, 130.0f, 160.0f }) {
			const auto from = a_feet + RE::NiPoint3(0.0f, 0.0f, height);
			for (const float across : { 0.0f, -HALF_WIDTH, HALF_WIDTH }) {
				const auto to = from + direction * a_max + side * across;
				// Line of sight catches walls and rocks; the camera's own layer also
				// catches what only the camera bumps into (trees, some clutter).
				fraction = std::min(fraction, RayFraction(a_world, from, to, RE::COL_LAYER::kLOS));
				fraction = std::min(fraction, RayFraction(a_world, from, to, RE::COL_LAYER::kCameraSphere));
			}
		}
		return fraction * a_max;
	}

	// Headings to try, best first, as turns from where the player faces.
	struct Candidate
	{
		float turn = 0.0f;
		float room = 0.0f;
	};
	std::vector<Candidate> candidates;
	std::size_t            candidate = 0;  // the one in use
	int                    retries = 0;

	void Turn(RE::PlayerCharacter* a_player, float a_heading)
	{
		a_player->SetAngle(RE::NiPoint3(a_player->data.angle.x, a_player->data.angle.y, a_heading));
	}

	// Turns the player toward open space when the camera would hit something.
	// Returns the room the camera has in the direction it ends up looking from
	// (a_need when open). Sets a_turned when the heading changed. Keeps the other
	// open headings in `candidates` for Tick() to fall back on.
	float FaceOpenSpace(RE::PlayerCharacter* a_player, const CameraStage::Config& a_config, float a_need, bool& a_turned)
	{
		a_turned = false;
		candidates.clear();
		candidate = 0;
		retries = 0;
		auto* cell = a_player->GetParentCell();
		auto* world = cell ? cell->GetbhkWorld() : nullptr;
		if (!world) {
			return a_need;
		}

		// The camera is in front of the player, a_config.rotation off centre.
		const float heading = a_player->data.angle.z;
		const auto  feet = a_player->GetPosition();
		const auto  room = [&](float a_heading) { return Clearance(world, feet, a_heading + a_config.rotation, a_need); };

		// Sixteen directions: the open ones by how little the player turns, then
		// the rest by how much room they have.
		constexpr int STEPS = 16;
		for (int i = 0; i < STEPS; ++i) {
			const float turn = (i <= STEPS / 2 ? i : i - STEPS) * (2.0f * std::numbers::pi_v<float> / STEPS);
			candidates.push_back({ turn, room(heading + turn) });
		}
		const float ahead = candidates.front().room;
		std::ranges::stable_sort(candidates, [&](const Candidate& a, const Candidate& b) {
			const bool aOpen = a.room >= a_need;
			const bool bOpen = b.room >= a_need;
			if (aOpen != bOpen) {
				return aOpen;
			}
			return aOpen ? std::abs(a.turn) < std::abs(b.turn) : a.room > b.room;
		});

		// Open ahead: stay as is; the other headings remain for a retry.
		if (ahead >= a_need) {
			return a_need;
		}
		const auto& best = candidates.front();
		logger::info("Walls: {:.0f} units of room ahead, {:.0f} needed; turning the player {:.0f} degrees ({:.0f} room)",
			ahead, a_need, best.turn * 180.0f / std::numbers::pi_v<float>, best.room);
		if (best.turn != 0.0f) {
			Turn(a_player, heading + best.turn);
			a_turned = true;
		}
		return best.room;
	}
}

bool CameraStage::Enter(const Config& a_config)
{
	if (saved.active || !a_config.enabled) {
		return false;
	}

	auto* camera = RE::PlayerCamera::GetSingleton();
	auto* player = RE::PlayerCharacter::GetSingleton();
	auto* third = camera ? ThirdPerson(camera) : nullptr;
	if (!camera || !player || !third) {
		return false;
	}

	const bool firstPerson = camera->IsInFirstPerson();
	if (!firstPerson && !camera->IsInThirdPerson()) {
		logger::info("Camera stage skipped: camera is neither in first nor third person");
		return false;
	}

	saved.active = true;
	saved.wasFirstPerson = firstPerson;
	saved.freeRotation = third->freeRotation;
	saved.freeRotationEnabled = third->freeRotationEnabled;
	saved.toggleAnimCam = third->toggleAnimCam;
	saved.posOffsetExpected = third->posOffsetExpected;
	saved.targetZoomOffset = third->targetZoomOffset;
	saved.pitchZoomOffset = third->pitchZoomOffset;
	saved.worldFOV = camera->GetRuntimeData2().worldFOV;
	saved.playerPitch = player->data.angle.x;

	if (firstPerson) {
		camera->ForceThirdPerson();
	}

	// Free rotation lets the camera orbit the player instead of sitting behind
	// them; toggleAnimCam also frees it while a weapon is drawn.
	third->toggleAnimCam = true;
	third->freeRotationEnabled = true;
	third->freeRotation.x = std::numbers::pi_v<float> + a_config.rotation;  // pi = directly in front
	third->freeRotation.y = 0.0f;
	player->data.angle.x = a_config.pitch;

	// Fixed distance: with min == max the zoom level no longer matters.
	settings.Set("fVanityModeMinDist:Camera", a_config.distance);
	settings.Set("fVanityModeMaxDist:Camera", a_config.distance);
	settings.Set("fOverShoulderPosX:Camera", a_config.offsetX);
	settings.Set("fOverShoulderPosZ:Camera", a_config.offsetZ);
	settings.Set("fOverShoulderCombatPosX:Camera", a_config.offsetX);
	settings.Set("fOverShoulderCombatAddY:Camera", 0.0f);
	settings.Set("fOverShoulderCombatPosZ:Camera", a_config.offsetZ);
	settings.Set("fAutoVanityModeDelay:Camera", 10800.0f);  // no idle orbit while the screen is open
	settings.Set("fMouseWheelZoomSpeed:Camera", 10000.0f);  // snap instead of gliding into place

	saved.playerYaw = player->data.angle.z;
	saved.playerPosition = player->GetPosition();
	saved.frames = 0;
	// Walls: turn toward open space; where there is none (a tight corner),
	// bring the camera closer and narrow the field of view to match, so the
	// character is framed the same.
	float distance = a_config.distance;
	float fov = a_config.fov > 0.0f ? a_config.fov : saved.worldFOV;
	if (a_config.faceOpenSpace) {
		const float need = a_config.distance + CAMERA_MARGIN;
		const float room = FaceOpenSpace(player, a_config, need, saved.turned);
		if (room < need) {
			distance = std::max(room - CAMERA_MARGIN, MIN_DISTANCE);
			const float halfAngle = fov * std::numbers::pi_v<float> / 360.0f;
			fov = 2.0f * std::atan(std::tan(halfAngle) * distance / a_config.distance) * 180.0f / std::numbers::pi_v<float>;
			settings.Set("fVanityModeMinDist:Camera", distance);
			settings.Set("fVanityModeMaxDist:Camera", distance);
			camera->GetRuntimeData2().worldFOV = fov;
			logger::info("Walls: no open side; camera at {:.0f} instead of {:.0f}, field of view {:.1f}", distance,
				a_config.distance, fov);
		}
	}
	saved.distance = distance;
	saved.fov = distance < a_config.distance ? fov : 0.0f;
	third->pitchZoomOffset = 0.1f;  // distance independent of pitch
	third->posOffsetExpected = third->posOffsetActual = RE::NiPoint3(a_config.offsetX, 0.0f, a_config.offsetZ);

	if (a_config.fov > 0.0f && distance == a_config.distance) {
		camera->GetRuntimeData2().worldFOV = a_config.fov;
	}

	camera->Update();

	if (a_config.hideWorld) {
		HideWorld(player, a_config.blackFog, a_config.noFog);
	}
	if (a_config.backdrop) {
		AttachBackdrop(player, a_config);
	}
	if (a_config.studioLights) {
		AttachLights(player, a_config);
	}
	if (a_config.hideHud) {
		SetHudHidden(true);
	}

	logger::info("Camera stage entered (distance {}, offset {}/{}, from {} person)", a_config.distance,
		a_config.offsetX, a_config.offsetZ, firstPerson ? "first" : "third");
	return true;
}

void CameraStage::Leave()
{
	if (!saved.active) {
		return;
	}
	SetCharacterHidden(false);
	saved.active = false;

	DetachBackdrop();
	DetachLights();
	ShowWorld();
	SetHudHidden(false);

	auto* camera = RE::PlayerCamera::GetSingleton();
	auto* player = RE::PlayerCharacter::GetSingleton();
	auto* third = camera ? ThirdPerson(camera) : nullptr;
	if (!camera || !player || !third) {
		settings.Restore();
		return;
	}

	player->data.angle.x = saved.playerPitch;
	if (saved.turned) {
		player->SetAngle(RE::NiPoint3(player->data.angle.x, player->data.angle.y, saved.playerYaw));
	}
	// Turning the player near a wall nudged them a little each time; put them
	// back where they stood (only small moves, never a teleport after a scripted move).
	if (const float moved = player->GetPosition().GetDistance(saved.playerPosition); moved > 0.5f && moved < 200.0f) {
		player->SetPosition(saved.playerPosition, true);
		logger::info("Camera stage: player had moved {:.1f} units; put back", moved);
	}
	third->toggleAnimCam = saved.toggleAnimCam;
	third->freeRotationEnabled = saved.freeRotationEnabled;
	third->freeRotation = saved.freeRotation;
	third->targetZoomOffset = saved.targetZoomOffset;
	third->pitchZoomOffset = saved.pitchZoomOffset;
	third->posOffsetExpected = third->posOffsetActual = saved.posOffsetExpected;
	camera->GetRuntimeData2().worldFOV = saved.worldFOV;

	if (saved.wasFirstPerson) {
		camera->ForceFirstPerson();
	}

	// Update once with the snap-speed setting still in place, then restore the settings.
	camera->Update();
	settings.Restore();
	logger::info("Camera stage left");
}

void CameraStage::Tick()
{
	if (!saved.active) {
		return;
	}
	if (characterHidden) {
		// Again each frame: the game shows actor parts again on its own.
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* root = player ? player->Get3D(false) : nullptr;
		if (auto* node = root ? root->AsNode() : nullptr) {
			for (auto& child : node->GetChildren()) {
				if (child && !IsOwnNode(child.get()) && !child->GetAppCulled()) {
					child->SetAppCulled(true);
					if (std::ranges::none_of(hiddenParts, [&](const auto& a_part) { return a_part.get() == child.get(); })) {
						hiddenParts.emplace_back(child.get());
					}
				}
			}
		}
	}
	if (auto* player = RE::PlayerCharacter::GetSingleton()) {
		++saved.frames;
		// A wall the checks missed still pulls the camera in. Look where the
		// camera really is a few frames after opening (or after a retry); if it
		// is far too close, turn the player to the next open heading.
		constexpr int CHECK_FRAME = 6;
		constexpr int MAX_RETRIES = 4;
		auto* camera = RE::PlayerCamera::GetSingleton();
		// The game may set the field of view back; hold the narrower one while
		// the camera stands closer than usual.
		if (camera && saved.fov > 0.0f) {
			camera->GetRuntimeData2().worldFOV = saved.fov;
		}
		if (saved.frames == CHECK_FRAME || saved.frames == 120) {
			if (camera && camera->cameraRoot) {
				const auto  head = saved.playerPosition + RE::NiPoint3(0.0f, 0.0f, 120.0f);
				const float distance = camera->cameraRoot->world.translate.GetDistance(head);
				const bool  pulledIn = distance < saved.distance * 0.6f;
				logger::info("Camera stage: frame {}, camera {:.0f} units from the player's head{}, field of view {:.1f}", saved.frames,
					distance, pulledIn ? " (pulled in)" : "", camera->GetRuntimeData2().worldFOV);
				const auto next = candidate + 1;
				// Any heading left: the checks can miss what the camera hits, so a
				// heading they rate as blocked may still work.
				const bool nextOpen = next < candidates.size();
				if (pulledIn && saved.frames == CHECK_FRAME && retries < MAX_RETRIES && nextOpen) {
					++retries;
					candidate = next;
					Turn(player, saved.playerYaw + candidates[candidate].turn);
					saved.turned = true;
					saved.frames = 0;
					logger::info("Walls: retry {}: turning the player {:.0f} degrees instead", retries,
						candidates[candidate].turn * 180.0f / std::numbers::pi_v<float>);
				}
			}
		}
	}
	if (!worldHidden) {
		return;
	}
	// Critters, NPCs and effects keep being attached while the game runs.
	if (auto* player = RE::PlayerCharacter::GetSingleton()) {
		CullPass(player);
	}
	ReapplyFog();
}

bool CameraStage::IsActive()
{
	return saved.active;
}

bool CameraStage::IsOwnNode(const RE::NiAVObject* a_object)
{
	return std::ranges::any_of(backdropPlanes, [&](const auto& a_plane) { return a_plane.get() == a_object; }) ||
	       std::ranges::any_of(studioLights, [&](const auto& a_light) { return a_light.get() == a_object; });
}

void CameraStage::SetCharacterHidden(bool a_hidden)
{
	if (a_hidden == characterHidden) {
		return;
	}
	characterHidden = a_hidden && saved.active;
	if (!characterHidden) {
		for (auto& part : hiddenParts) {
			part->SetAppCulled(false);
		}
		hiddenParts.clear();
	}
	logger::info("Camera stage: character {}", characterHidden ? "hidden" : "shown");
}
