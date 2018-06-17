
#include "audio.h"

#ifdef HAVE_XAUDIO2

#include <xaudio2.h>
#include <xaudio2fx.h>
#include <SDL.h>

#include "dr_wav.h"

struct AudioContext 
{
	IXAudio2 *xaudio2;
	uint32_t	output_5p1;
	IXAudio2MasteringVoice *mastering_voice;

	unsigned int wav_channels;
	unsigned int wav_samplerate;
	drwav_uint64 wav_sample_count;
	float *		 wav_samples;

	struct AudioVoice *voice;
	XAUDIO2_BUFFER    buffer;

	XAUDIO2_EFFECT_DESCRIPTOR reverb_effect;
	XAUDIO2_EFFECT_CHAIN	  effect_chain;
	ReverbParameters		  reverb_params;
	bool					  reverb_enabled;
};

struct AudioVoice
{
	AudioContext *context;
	IXAudio2SourceVoice *voice;
};

struct AudioFilter
{
	AudioContext *context;
	XAUDIO2_FILTER_PARAMETERS params;
};

void xaudio_destroy_context(AudioContext *p_context)
{
	if (p_context != NULL)
	{
		p_context->mastering_voice->DestroyVoice();
		p_context->xaudio2->Release();
		delete p_context;
	}
}

AudioVoice *xaudio_create_voice(AudioContext *p_context, float *p_buffer, size_t p_buffer_size, int p_sample_rate, int p_num_channels)
{
	// create the effect chain
	IUnknown *xapo = NULL;
	HRESULT hr = XAudio2CreateReverb(&xapo);

	if (FAILED(hr))
	{
		return NULL;
	}

	// create effect chain
	p_context->reverb_effect.InitialState = p_context->reverb_enabled;
	p_context->reverb_effect.OutputChannels = (p_context->output_5p1) ? 6 : p_context->wav_channels;
	p_context->reverb_effect.pEffect = xapo;

	p_context->effect_chain.EffectCount = 1;
	p_context->effect_chain.pEffectDescriptors = &p_context->reverb_effect;

	// create a source voice
	WAVEFORMATEX waveFormat;
	waveFormat.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
	waveFormat.nChannels = p_num_channels;
	waveFormat.nSamplesPerSec = p_sample_rate;
	waveFormat.nBlockAlign = p_num_channels * 4;
	waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
	waveFormat.wBitsPerSample = 32;
	waveFormat.cbSize = 0;

	IXAudio2SourceVoice *voice;
	hr = p_context->xaudio2->CreateSourceVoice(&voice, &waveFormat, XAUDIO2_VOICE_USEFILTER, XAUDIO2_MAX_FREQ_RATIO, NULL, NULL, &p_context->effect_chain);
	xapo->Release();

	if (FAILED(hr)) 
	{
		return NULL;
	}

	voice->SetVolume(1.0f);

	// submit the array
	SDL_zero(p_context->buffer);
	p_context->buffer.AudioBytes = 4 * p_buffer_size * p_num_channels;
	p_context->buffer.pAudioData = (byte *)p_buffer;
	p_context->buffer.Flags = XAUDIO2_END_OF_STREAM;
	p_context->buffer.PlayBegin = 0;
	p_context->buffer.PlayLength = p_buffer_size;
	p_context->buffer.LoopBegin = 0;
	p_context->buffer.LoopLength = 0;
	p_context->buffer.LoopCount = 0;

	// return a voice struct
	AudioVoice *result = new AudioVoice();
	result->context = p_context;
	result->voice = voice;
	return result;
}

void xaudio_reverb_set_params(AudioContext *context)
{
	XAUDIO2FX_REVERB_PARAMETERS native_params = { 0 };

	native_params.WetDryMix = context->reverb_params.WetDryMix;
	native_params.ReflectionsDelay = context->reverb_params.ReflectionsDelay;
	native_params.ReverbDelay = context->reverb_params.ReverbDelay;
	native_params.RearDelay = context->reverb_params.RearDelay;
	native_params.PositionLeft = context->reverb_params.PositionLeft;
	native_params.PositionRight = context->reverb_params.PositionRight;
	native_params.PositionMatrixLeft = context->reverb_params.PositionMatrixLeft;
	native_params.PositionMatrixRight = context->reverb_params.PositionMatrixRight;
	native_params.EarlyDiffusion = context->reverb_params.EarlyDiffusion;
	native_params.LateDiffusion = context->reverb_params.LateDiffusion;
	native_params.LowEQGain = context->reverb_params.LowEQGain;
	native_params.LowEQCutoff = context->reverb_params.LowEQCutoff;
	native_params.HighEQGain = context->reverb_params.HighEQGain;
	native_params.HighEQCutoff = context->reverb_params.HighEQCutoff;
	native_params.RoomFilterFreq = context->reverb_params.RoomFilterFreq;
	native_params.RoomFilterMain = context->reverb_params.RoomFilterMain;
	native_params.RoomFilterHF = context->reverb_params.RoomFilterHF;
	native_params.ReflectionsGain = context->reverb_params.ReflectionsGain;
	native_params.ReverbGain = context->reverb_params.ReverbGain;
	native_params.DecayTime = context->reverb_params.DecayTime;
	native_params.Density = context->reverb_params.Density;
	native_params.RoomSize = context->reverb_params.RoomSize;

	/* 2.8+ only but zero-initialization catches this 
	native_params.DisableLateField = 0; */

	HRESULT hr = context->voice->voice->SetEffectParameters(
		0, 
		&native_params,
		sizeof(XAUDIO2FX_REVERB_PARAMETERS));
}


void xaudio_voice_destroy(AudioVoice *p_voice)
{
	drwav_free(p_voice->context->wav_samples);
	p_voice->voice->DestroyVoice();
}

void xaudio_voice_set_volume(AudioVoice *p_voice, float p_volume)
{
	p_voice->voice->SetVolume(p_volume);
}

void xaudio_voice_set_frequency(AudioVoice *p_voice, float p_frequency)
{
	p_voice->voice->SetFrequencyRatio(p_frequency);
}

void xaudio_wave_load(AudioContext *p_context, AudioSampleWave sample, bool stereo)
{
	if (p_context->voice)
	{
		audio_voice_destroy(p_context->voice);
	}

	p_context->wav_samples = drwav_open_and_read_file_f32(
		(!stereo) ? audio_sample_filenames[sample] : audio_stereo_filenames[sample],
		&p_context->wav_channels,
		&p_context->wav_samplerate,
		&p_context->wav_sample_count);

	p_context->wav_sample_count /= p_context->wav_channels;

	p_context->voice = audio_create_voice(p_context, p_context->wav_samples, p_context->wav_sample_count, p_context->wav_samplerate, p_context->wav_channels);
	xaudio_reverb_set_params(p_context);
}

void xaudio_wave_play(AudioContext *p_context)
{
	p_context->voice->voice->Stop();
	p_context->voice->voice->FlushSourceBuffers();

	HRESULT hr = p_context->voice->voice->SubmitSourceBuffer(&p_context->buffer);

	if (FAILED(hr)) 
	{
		return;
	}

	p_context->voice->voice->Start();
}

void xaudio_effect_change(AudioContext *p_context, bool p_enabled, ReverbParameters *p_params)
{
	HRESULT hr;

	if (p_context->reverb_enabled && !p_enabled)
	{
		hr = p_context->voice->voice->DisableEffect(0);
		p_context->reverb_enabled = p_enabled;
	}
	else if (!p_context->reverb_enabled && p_enabled)
	{
		hr = p_context->voice->voice->EnableEffect(0);
		p_context->reverb_enabled = p_enabled;
	}

	memcpy(&p_context->reverb_params, p_params, sizeof(ReverbParameters));
	xaudio_reverb_set_params(p_context);
}


AudioContext *xaudio_create_context(bool output_5p1)
{
	// setup function pointers
	audio_destroy_context = xaudio_destroy_context;
	audio_create_voice = xaudio_create_voice;
	audio_voice_destroy = xaudio_voice_destroy;
	audio_voice_set_volume = xaudio_voice_set_volume;
	audio_voice_set_frequency = xaudio_voice_set_frequency;

	audio_wave_load = xaudio_wave_load;
	audio_wave_play = xaudio_wave_play;

	audio_effect_change = xaudio_effect_change;

	// create XAudio object
	IXAudio2 *xaudio2;

	HRESULT hr = XAudio2Create(&xaudio2);
	if (FAILED(hr))
	{
		return NULL;
	}

	// create a mastering voice
	IXAudio2MasteringVoice *mastering_voice;

	hr = xaudio2->CreateMasteringVoice(&mastering_voice, output_5p1 ? 6 : 2);
	if (FAILED(hr))
	{
		return NULL;
	}

	// return a context object
	AudioContext *context = new AudioContext();
	context->xaudio2 = xaudio2;
	context->output_5p1 = output_5p1;
	context->mastering_voice = mastering_voice;
	context->voice = NULL;
	context->wav_samples = NULL;
	context->reverb_params = audio_reverb_presets[0];
	context->reverb_enabled = false;

	// load the first wave
	audio_wave_load(context, (AudioSampleWave) 0, false);

	return context;
}

#endif // HAVE_XAUDIO2