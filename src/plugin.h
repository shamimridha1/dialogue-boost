#pragma once
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/common/pluginview.h"
#include "dsp.h"
#include <atomic>

namespace dlb {

enum ParamIds : Steinberg::Vst::ParamID { kBoost = 0, kClarity = 1, kCeiling = 2, kBypass = 3 };

class Editor;

class PluginEntry : public Steinberg::Vst::SingleComponentEffect
{
public:
	PluginEntry ();
	static Steinberg::FUnknown* createInstance (void*)
	{
		return (Steinberg::Vst::IAudioProcessor*)new PluginEntry ();
	}

	Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setBusArrangements (Steinberg::Vst::SpeakerArrangement* inputs,
	                                                  Steinberg::int32 numIns,
	                                                  Steinberg::Vst::SpeakerArrangement* outputs,
	                                                  Steinberg::int32 numOuts) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 s) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& setup) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setActive (Steinberg::TBool state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setProcessing (Steinberg::TBool) SMTG_OVERRIDE
	{
		return Steinberg::kResultOk;
	}
	Steinberg::uint32 PLUGIN_API getLatencySamples () SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setParamNormalized (Steinberg::Vst::ParamID tag,
	                                                  Steinberg::Vst::ParamValue value) SMTG_OVERRIDE;
	Steinberg::IPlugView* PLUGIN_API createView (Steinberg::FIDString name) SMTG_OVERRIDE;

	// editor bridge
	void editParam (Steinberg::Vst::ParamID id, double norm, int phase); // 0 begin,1 move,2 end
	double plain (Steinberg::Vst::ParamID id);
	float gainReduction () const { return grDb.load (); }
	Editor* editor {nullptr};

private:
	void syncDsp ();
	Processor dsp;
	bool bypass {false};
	std::atomic<float> grDb {0.f};
};

class Editor : public Steinberg::CPluginView
{
public:
	explicit Editor (PluginEntry* owner);
	~Editor () override;
	Steinberg::tresult PLUGIN_API isPlatformTypeSupported (Steinberg::FIDString type) SMTG_OVERRIDE;
	void attachedToParent () override;
	void removedFromParent () override;
	void refresh ();

	// win32 internals (public for wndproc)
	void* hwnd {nullptr};
	PluginEntry* plug;
	int dragParam {-1};
	float shownGr {0.f};
};

} // namespace dlb
