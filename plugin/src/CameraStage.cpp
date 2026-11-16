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
	};

	Saved            saved;
	SettingOverrides settings;

	// Scene objects culled by HideWorld(), kept alive until ShowWorld().
	std::vector<RE::NiPointer<RE::NiAVObject>> culled;
	bool                                       hudWasVisible = false;
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

		std::size_t depth = 0;
		for (RE::NiAVObject* child = player3D; child->parent; child = child->parent, ++depth) {
			for (auto& sibling : child->parent->GetChildren()) {
				// Never cull the branch holding the world camera.
				if (sibling && sibling.get() != child && !IsAncestorOf(sibling.get(), camera)) {
					Cull(sibling.get());
				}
			}
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
	config.hideWorld = flag("hideWorld", config.hideWorld);
	config.backdrop = flag("backdrop", config.backdrop);
	config.backdropDistance = num("backdropDistance", config.backdropDistance);
	config.backdropHeight = num("backdropHeight", config.backdropHeight);
	config.backdropScale = num("backdropScale", config.backdropScale);
	config.blackFog = flag("blackFog", config.blackFog);
	config.noFog = flag("noFog", config.noFog);
	config.hideHud = flag("hideHud", config.hideHud);
	config.pauseGame = flag("pauseGame", config.pauseGame);
	config.distance = num("distance", config.distance);
	config.offsetX = num("offsetX", config.offsetX);
	config.offsetZ = num("offsetZ", config.offsetZ);
	config.rotation = num("rotation", config.rotation);
	config.pitch = num("pitch", config.pitch);
	config.fov = num("fov", config.fov);
	return config;
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

	third->pitchZoomOffset = 0.1f;  // distance independent of pitch
	third->posOffsetExpected = third->posOffsetActual = RE::NiPoint3(a_config.offsetX, 0.0f, a_config.offsetZ);

	if (a_config.fov > 0.0f) {
		camera->GetRuntimeData2().worldFOV = a_config.fov;
	}

	camera->Update();

	if (a_config.hideWorld) {
		HideWorld(player, a_config.blackFog, a_config.noFog);
	}
	if (a_config.backdrop) {
		AttachBackdrop(player, a_config);
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
	saved.active = false;

	DetachBackdrop();
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
	if (!saved.active || !worldHidden) {
		return;
	}
	// Critters, NPCs and effects keep being attached while the game runs.
	if (auto* player = RE::PlayerCharacter::GetSingleton()) {
		CullPass(player);
	}
	ReapplyFog();
}
