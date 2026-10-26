// Dialogue Boost VST3 (single component: processor + controller + editor)
#include "plugin.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/vst/vstaudioprocessoralgo.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "base/source/fstreamer.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace dlb {

const FUID PluginUID (0x6D1B4F21, 0x3A8C4E55, 0x9F02B7C4, 0xD1A0E713);

PluginEntry::PluginEntry () {}

tresult PLUGIN_API PluginEntry::initialize (FUnknown* context)
{
	tresult r = SingleComponentEffect::initialize (context);
	if (r != kResultOk) return r;

	addAudioInput (STR16 ("Input"), SpeakerArr::kStereo);
	addAudioOutput (STR16 ("Output"), SpeakerArr::kStereo);

	parameters.addParameter (new RangeParameter (STR16 ("Boost"), kBoost, STR16 ("dB"), 0.0,
	                                             12.0, 6.0, 0, ParameterInfo::kCanAutomate));
	parameters.addParameter (new RangeParameter (STR16 ("Clarity"), kClarity, STR16 ("%"), 0.0,
	                                             200.0, 100.0, 0, ParameterInfo::kCanAutomate));
	parameters.addParameter (new RangeParameter (STR16 ("Ceiling"), kCeiling, STR16 ("dB"), -6.0,
	                                             0.0, -1.5, 0, ParameterInfo::kCanAutomate));
	parameters.addParameter (STR16 ("Bypass"), nullptr, 1, 0,
	                         ParameterInfo::kCanAutomate | ParameterInfo::kIsBypass, kBypass);

	syncDsp ();
	return kResultOk;
}

void PluginEntry::syncDsp ()
{
	dsp.boostDb = plain (kBoost);
	dsp.clarity = plain (kClarity) / 100.0;
	dsp.ceilingDb = plain (kCeiling);
}

double PluginEntry::plain (ParamID id)
{
	return normalizedParamToPlain (id, getParamNormalized (id));
}

tresult PLUGIN_API PluginEntry::setBusArrangements (SpeakerArrangement* inputs, int32 numIns,
                                                    SpeakerArrangement* outputs, int32 numOuts)
{
	if (numIns != 1 || numOuts != 1) return kResultFalse;
	if (inputs[0] != outputs[0]) return kResultFalse;
	int32 ch = SpeakerArr::getChannelCount (inputs[0]);
	if (ch < 1 || ch > 2) return kResultFalse;
	removeAudioBusses ();
	addAudioInput (STR16 ("Input"), inputs[0]);
	addAudioOutput (STR16 ("Output"), outputs[0]);
	return kResultTrue;
}

tresult PLUGIN_API PluginEntry::canProcessSampleSize (int32 s)
{
	return (s == kSample32 || s == kSample64) ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API PluginEntry::setupProcessing (ProcessSetup& setup)
{
	tresult r = SingleComponentEffect::setupProcessing (setup);
	SpeakerArrangement arr = SpeakerArr::kStereo;
	getBusArrangement (kOutput, 0, arr);
	dsp.prepare (setup.sampleRate, SpeakerArr::getChannelCount (arr));
	syncDsp ();
	return r;
}

tresult PLUGIN_API PluginEntry::setActive (TBool state)
{
	if (state) dsp.reset ();
	return kResultOk;
}

uint32 PLUGIN_API PluginEntry::getLatencySamples ()
{
	return (uint32)dsp.latency ();
}

tresult PLUGIN_API PluginEntry::process (ProcessData& data)
{
	// parameter changes (take last value of each queue)
	if (data.inputParameterChanges)
	{
		int32 n = data.inputParameterChanges->getParameterCount ();
		for (int32 i = 0; i < n; ++i)
		{
			IParamValueQueue* q = data.inputParameterChanges->getParameterData (i);
			if (!q) continue;
			ParamValue v;
			int32 off;
			if (q->getPoint (q->getPointCount () - 1, off, v) != kResultTrue) continue;
			ParamID id = q->getParameterId ();
			if (id == kBypass) bypass = v > 0.5;
			else if (id == kBoost) dsp.boostDb = normalizedParamToPlain (id, v);
			else if (id == kClarity) dsp.clarity = normalizedParamToPlain (id, v) / 100.0;
			else if (id == kCeiling) dsp.ceilingDb = normalizedParamToPlain (id, v);
		}
	}

	if (data.numInputs == 0 || data.numOutputs == 0 || data.numSamples <= 0) return kResultOk;

	AudioBusBuffers& in = data.inputs[0];
	AudioBusBuffers& out = data.outputs[0];
	int32 ch = std::min (in.numChannels, out.numChannels);
	int32 n = data.numSamples;

	if (data.symbolicSampleSize == kSample32)
	{
		if (bypass)
		{
			for (int32 c = 0; c < ch; ++c)
				if (in.channelBuffers32[c] != out.channelBuffers32[c])
					memcpy (out.channelBuffers32[c], in.channelBuffers32[c], n * sizeof (float));
		}
		else
			dsp.process (in.channelBuffers32, out.channelBuffers32, ch, n);
	}
	else
	{
		if (bypass)
		{
			for (int32 c = 0; c < ch; ++c)
				if (in.channelBuffers64[c] != out.channelBuffers64[c])
					memcpy (out.channelBuffers64[c], in.channelBuffers64[c], n * sizeof (double));
		}
		else
			dsp.process (in.channelBuffers64, out.channelBuffers64, ch, n);
	}
	out.silenceFlags = 0;
	grDb.store (bypass ? 0.f : (float)dsp.grDb);
	return kResultOk;
}

tresult PLUGIN_API PluginEntry::setState (IBStream* state)
{
	if (!state) return kResultFalse;
	IBStreamer s (state, kLittleEndian);
	double v[4];
	for (int i = 0; i < 4; ++i)
		if (!s.readDouble (v[i])) return kResultFalse;
	ParamID ids[4] = {kBoost, kClarity, kCeiling, kBypass};
	for (int i = 0; i < 4; ++i) setParamNormalized (ids[i], v[i]);
	syncDsp ();
	bypass = v[3] > 0.5;
	return kResultOk;
}

tresult PLUGIN_API PluginEntry::getState (IBStream* state)
{
	if (!state) return kResultFalse;
	IBStreamer s (state, kLittleEndian);
	ParamID ids[4] = {kBoost, kClarity, kCeiling, kBypass};
	for (int i = 0; i < 4; ++i) s.writeDouble (getParamNormalized (ids[i]));
	return kResultOk;
}

tresult PLUGIN_API PluginEntry::setParamNormalized (ParamID tag, ParamValue value)
{
	tresult r = SingleComponentEffect::setParamNormalized (tag, value);
	if (editor) editor->refresh ();
	return r;
}

IPlugView* PLUGIN_API PluginEntry::createView (FIDString name)
{
	if (name && strcmp (name, ViewType::kEditor) == 0)
		return new Editor (this);
	return nullptr;
}

// called by editor
void PluginEntry::editParam (ParamID id, double norm, int phase)
{
	if (phase == 0) beginEdit (id);
	norm = std::min (1.0, std::max (0.0, norm));
	SingleComponentEffect::setParamNormalized (id, norm);
	performEdit (id, norm);
	if (phase == 2) endEdit (id);
	// keep dsp in sync even if host doesn't echo the change back
	syncDsp ();
	if (id == kBypass) bypass = norm > 0.5;
}

} // namespace dlb

BEGIN_FACTORY_DEF ("Abid Tools", "", "")
	DEF_CLASS2 (INLINE_UID_FROM_FUID (dlb::PluginUID), PClassInfo::kManyInstances,
	            kVstAudioEffectClass, "Dialogue Boost", 0, "Fx|EQ|Dynamics", "1.0.0",
	            kVstVersionString, dlb::PluginEntry::createInstance)
END_FACTORY
