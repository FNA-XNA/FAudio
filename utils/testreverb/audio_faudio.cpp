#include "audio.h"

#include <FAudio.h>
#include <FAudioFX.h>
#include <SDL.h>

#include "dr_wav.h"

struct AudioContext 
{
	FAudio *faudio;
	bool output_5p1;
	FAudioMasteringVoice *mastering_voice;

	unsigned int wav_channels;
	unsigned int wav_samplerate;
	drwav_uint64 wav_sample_count;
	float *		 wav_samples;

	struct AudioVoice *voice;
	FAudioBuffer      buffer;
	FAudioBuffer	  silence;

	FAudioEffectDescriptor reverb_effect;
	FAudioEffectChain	   effect_chain;
	ReverbParameters	   reverb_params;
	bool				   reverb_enabled;
};

struct AudioVoice
{
	AudioContext *context;
	FAudioSourceVoice *voice;
};

struct AudioFilter
{
	AudioContext *context;
	FAudioFilterParameters params;
};

void faudio_destroy_context(AudioContext *context)
{
	if (context != NULL)
	{
		FAudioVoice_DestroyVoice(context->mastering_voice);
		// FAudioDestroy(context->faudio);
		delete context;
	}
}

AudioVoice *faudio_create_voice(AudioContext *context, float *buffer, size_t buffer_size, int sample_rate, int num_channels)
{
	// create reverb effect
	void *fapo = NULL;
	uint32_t hr = FAudioCreateReverb(&fapo, 0);

	if (hr != 0)
	{
		return NULL;
	}

	// create effect chain
	context->reverb_effect.InitialState = context->reverb_enabled;
	context->reverb_effect.OutputChannels = (context->output_5p1) ? 6 : context->wav_channels;
	context->reverb_effect.pEffect = fapo;

	context->effect_chain.EffectCount = 1;
	context->effect_chain.pEffectDescriptors = &context->reverb_effect;

	// create a source voice
	FAudioWaveFormatEx waveFormat;
	waveFormat.wFormatTag = 3;
	waveFormat.nChannels = num_channels;
	waveFormat.nSamplesPerSec = sample_rate;
	waveFormat.nAvgBytesPerSec = sample_rate * 4;
	waveFormat.nBlockAlign = num_channels * 4;
	waveFormat.wBitsPerSample = 32;
	waveFormat.cbSize = 0;

	FAudioSourceVoice *voice;
	hr = FAudio_CreateSourceVoice(context->faudio, &voice, &waveFormat, FAUDIO_VOICE_USEFILTER, FAUDIO_MAX_FREQ_RATIO, NULL, NULL, &context->effect_chain);

	if (hr != 0) 
	{
		return NULL;
	}

	FAudioVoice_SetVolume(voice, 1.0f, FAUDIO_COMMIT_NOW);
	
	// submit the array
	SDL_zero(context->buffer);
	context->buffer.AudioBytes = 4 * buffer_size * num_channels;
	context->buffer.pAudioData = (uint8_t *)buffer;
	context->buffer.Flags = FAUDIO_END_OF_STREAM;
	context->buffer.PlayBegin = 0;
	context->buffer.PlayLength = buffer_size;
	context->buffer.LoopBegin = 0;
	context->buffer.LoopLength = 0;
	context->buffer.LoopCount = 0;

	
	size_t silence_len = 2 * 48000 * num_channels;
	float *silence_buffer = new float[silence_len];
	for (uint32_t i = 0; i < silence_len; i += 1)
	{
		silence_buffer[i] = 0.0f;
	}

	SDL_zero(context->silence);
	context->silence.AudioBytes = 4 * silence_len;
	context->silence.pAudioData = (uint8_t *)silence_buffer;
	context->silence.Flags = FAUDIO_END_OF_STREAM;
	context->silence.PlayBegin = 0;
	context->silence.PlayLength = silence_len / num_channels;
	context->silence.LoopBegin = 0;
	context->silence.LoopLength = 0;
	context->silence.LoopCount = 0;

	// return a voice struct
	AudioVoice *result = new AudioVoice();
	result->context = context;
	result->voice = voice;
	return result;
}

void faudio_reverb_set_params(AudioContext *context)
{
	FAudioVoice_SetEffectParameters(context->voice->voice, 0, &context->reverb_params, sizeof(context->reverb_params), FAUDIO_COMMIT_NOW);
}

void faudio_voice_destroy(AudioVoice *voice)
{
	FAudioVoice_DestroyVoice(voice->voice);
}

void faudio_voice_set_volume(AudioVoice *voice, float volume)
{
	FAudioVoice_SetVolume(voice->voice, volume, FAUDIO_COMMIT_NOW);
}

void faudio_voice_set_frequency(AudioVoice *voice, float frequency)
{
	FAudioSourceVoice_SetFrequencyRatio(voice->voice, frequency, FAUDIO_COMMIT_NOW);
}

void faudio_wave_load(AudioContext *context, AudioSampleWave sample, bool stereo)
{
	if (context->voice)
	{
		audio_voice_destroy(context->voice);
	}

	context->wav_samples = drwav_open_and_read_file_f32(
		(!stereo) ? audio_sample_filenames[sample] : audio_stereo_filenames[sample],
		&context->wav_channels,
		&context->wav_samplerate,
		&context->wav_sample_count);

	context->wav_sample_count /= context->wav_channels;

	context->voice = audio_create_voice(context, context->wav_samples, context->wav_sample_count, context->wav_samplerate, context->wav_channels);
	faudio_reverb_set_params(context);
}

void faudio_wave_play(AudioContext *context)
{
	FAudioSourceVoice_Stop(context->voice->voice, 0, FAUDIO_COMMIT_NOW);
	FAudioSourceVoice_FlushSourceBuffers(context->voice->voice);

	FAudioSourceVoice_SubmitSourceBuffer(context->voice->voice, &context->buffer, NULL);
	FAudioSourceVoice_SubmitSourceBuffer(context->voice->voice, &context->silence, NULL);
	FAudioSourceVoice_Start(context->voice->voice, 0, FAUDIO_COMMIT_NOW);
}

void faudio_effect_change(AudioContext *context, bool enabled, ReverbParameters *params)
{
	if (context->reverb_enabled && !enabled)
	{
		FAudioVoice_DisableEffect(context->voice->voice, 0, FAUDIO_COMMIT_NOW);
		context->reverb_enabled = enabled;
	}
	else if (!context->reverb_enabled && enabled)
	{
		FAudioVoice_EnableEffect(context->voice->voice, 0, FAUDIO_COMMIT_NOW);
		context->reverb_enabled = enabled;
	}

	context->reverb_params = *params;
	faudio_reverb_set_params(context);
}

AudioContext *faudio_create_context(bool output_5p1)
{
	// setup function pointers
	audio_destroy_context = faudio_destroy_context;
	audio_create_voice = faudio_create_voice;
	audio_voice_destroy = faudio_voice_destroy;
	audio_voice_set_volume = faudio_voice_set_volume;
	audio_voice_set_frequency = faudio_voice_set_frequency;

	audio_wave_load = faudio_wave_load;
	audio_wave_play = faudio_wave_play;

	audio_effect_change = faudio_effect_change;

	// create Faudio object
	FAudio *faudio;

	uint32_t hr = FAudioCreate(&faudio, 0, FAUDIO_DEFAULT_PROCESSOR);
	if (hr != 0)
	{
		return NULL;
	}

	// create a mastering voice
	FAudioMasteringVoice *mastering_voice;

	hr = FAudio_CreateMasteringVoice(faudio, &mastering_voice, output_5p1 ? 6 : 2, 44100, 0, 0, NULL);
	if (hr != 0)
	{
		return NULL;
	}

	// return a context object
	AudioContext *context = new AudioContext();
	context->faudio = faudio;
	context->output_5p1 = output_5p1;
	context->mastering_voice = mastering_voice;

	context->voice = NULL;
	context->wav_samples = NULL;
	SDL_zero(context->reverb_params);
	context->reverb_enabled = false;

	// load the first wave
	audio_wave_load(context, (AudioSampleWave) 0, false);

	return context;
}
