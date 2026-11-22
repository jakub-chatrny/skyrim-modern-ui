#include "PCH.h"

#include "Outfits.h"

#include "Equipment.h"

namespace
{
	constexpr std::uint32_t UNIQUE_ID = 'SMUI';
	constexpr std::uint32_t RECORD = 'OUTF';
	constexpr std::uint32_t VERSION = 1;

	struct Outfit
	{
		bool               defined = false;
		Equipment::FormSet forms{};
	};

	struct State
	{
		std::uint32_t                      active = 0;
		std::array<Outfit, Outfits::COUNT> outfits{};
		// Items the last switch wanted but the player no longer had. Not saved.
		std::uint32_t missing = 0;
	};

	::State state;

	// ---- co-save ---------------------------------------------------------

	void OnSave(SKSE::SerializationInterface* a_intfc)
	{
		// The active outfit is whatever is worn at save time.
		state.outfits[state.active] = { true, Equipment::CurrentForms() };

		if (!a_intfc->OpenRecord(RECORD, VERSION)) {
			logger::error("Outfits: could not open the co-save record");
			return;
		}
		a_intfc->WriteRecordData(state.active);
		for (const auto& outfit : state.outfits) {
			const std::uint8_t defined = outfit.defined ? 1 : 0;
			a_intfc->WriteRecordData(defined);
			for (const auto formID : outfit.forms) {
				a_intfc->WriteRecordData(formID);
			}
		}
	}

	void OnLoad(SKSE::SerializationInterface* a_intfc)
	{
		state = {};

		std::uint32_t type = 0;
		std::uint32_t version = 0;
		std::uint32_t length = 0;
		while (a_intfc->GetNextRecordInfo(type, version, length)) {
			if (type != RECORD || version != VERSION) {
				logger::warn("Outfits: skipping unknown co-save record {:#x} v{}", type, version);
				continue;
			}

			::State loaded;
			bool    ok = a_intfc->ReadRecordData(loaded.active) == sizeof(loaded.active) && loaded.active < Outfits::COUNT;
			for (auto& outfit : loaded.outfits) {
				std::uint8_t defined = 0;
				ok = ok && a_intfc->ReadRecordData(defined) == sizeof(defined);
				outfit.defined = defined != 0;
				for (auto& formID : outfit.forms) {
					RE::FormID saved = 0;
					ok = ok && a_intfc->ReadRecordData(saved) == sizeof(saved);
					// Load order can change between sessions; a form that no longer
					// resolves (its mod was removed) becomes an empty slot.
					if (saved != 0 && !a_intfc->ResolveFormID(saved, formID)) {
						formID = 0;
					}
				}
			}

			if (ok) {
				state = loaded;
				logger::info("Outfits: loaded, outfit {} active", state.active + 1);
			} else {
				logger::error("Outfits: co-save record is damaged; starting with empty outfits");
			}
		}
	}

	void OnRevert(SKSE::SerializationInterface*)
	{
		state = {};
	}
}

void Outfits::Install()
{
	const auto* serialization = SKSE::GetSerializationInterface();
	serialization->SetUniqueID(UNIQUE_ID);
	serialization->SetSaveCallback(OnSave);
	serialization->SetLoadCallback(OnLoad);
	serialization->SetRevertCallback(OnRevert);
}

bool Outfits::Switch(std::size_t a_index)
{
	if (a_index >= COUNT || a_index == state.active) {
		return false;
	}

	const auto worn = Equipment::CurrentForms();
	state.outfits[state.active] = { true, worn };

	auto& target = state.outfits[a_index];
	if (!target.defined) {
		// First visit: it starts out as what is being worn.
		target = { true, worn };
	}

	state.missing = static_cast<std::uint32_t>(Equipment::Apply(target.forms));
	state.active = static_cast<std::uint32_t>(a_index);
	logger::info("Outfits: switched to outfit {} ({} item(s) missing)", a_index + 1, state.missing);
	return true;
}

nlohmann::json Outfits::State()
{
	auto defined = nlohmann::json::array();
	for (std::size_t i = 0; i < COUNT; ++i) {
		defined.push_back(state.outfits[i].defined || i == state.active);
	}
	return {
		{ "active", state.active },
		{ "defined", std::move(defined) },
		{ "missing", state.missing },
	};
}
