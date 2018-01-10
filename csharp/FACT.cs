/* FACT# - C# Wrapper for FACT
 *
 * Copyright (c) 2018 Ethan Lee.
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software in a
 * product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Ethan "flibitijibibo" Lee <flibitijibibo@flibitijibibo.com>
 *
 */

#region Using Statements
using System;
using System.Runtime.InteropServices;
#endregion

public static class FACT
{
	#region Native Library Name

	const string nativeLibName = "FACT.dll";

	#endregion

	#region Delegates

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	public delegate int FACTReadFileCallback(
		IntPtr hFile,
		IntPtr buffer,
		uint nNumberOfBytesToRead,
		IntPtr lpOverlapped /* FACTOverlapped* */
	);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	public delegate int FACTGetOverlappedResultCallback(
		IntPtr hFile,
		IntPtr lpOverlapped, /* FACTOverlapped* */
		int bWait
	);

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	public delegate void FACTNotificationCallback(
		IntPtr pNotification /* const FACTNotification* */
	);

	#endregion

	#region Structures

	[StructLayout(LayoutKind.Sequential)]
	public unsafe struct FACTRendererDetails
	{
		public fixed char rendererID[0xFF];
		public fixed char displayName[0xFF];
		public int defaultDevice;
	}

	[StructLayout(LayoutKind.Sequential)]
	public unsafe struct FACTGUID
	{
		public uint Data1;
		public ushort Data2;
		public ushort Data3;
		public fixed byte Data4[8];
	}

	[StructLayout(LayoutKind.Sequential, Pack = 1)]
	public struct FACTWaveFormatEx
	{
		public ushort wFormatTag;
		public ushort nChannels;
		public uint nSamplesPerSec;
		public uint nAvgBytesPerSec;
		public ushort nBlockAlign;
		public ushort wBitsPerSample;
		public ushort cbSize;
	}

	[StructLayout(LayoutKind.Sequential, Pack = 1)]
	public struct FACTWaveFormatExtensible
	{
		public FACTWaveFormatEx Format;
		public ushort Samples; /* FIXME: union! */
		public uint dwChannelMask;
		public FACTGUID SubFormat;
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACTOverlapped
	{
		public IntPtr Internal; /* ULONG_PTR */
		public IntPtr InternalHigh; /* ULONG_PTR */
		public uint Offset; /* FIXME: union! */
		public uint OffsetHigh; /* FIXME: union! */
		public IntPtr hEvent;
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACTFileIOCallbacks
	{
		public IntPtr readFileCallback; /* FACTReadCallback */
		public IntPtr getOverlappedResultCallback; /* FACTGetOverlappedResultCallback */
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACTNotification
	{
		public byte type;
		public int timeStamp;
		public IntPtr pvContext;
		public uint fillter; /* TODO: Notifications */
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACTNotificationDescription
	{
		public byte type;
		public byte flags;
		public IntPtr pSoundBank; /* FACTSoundBank* */
		public IntPtr pWaveBank; /* FACTWaveBank* */
		public IntPtr pCue; /* FACTCue* */
		public IntPtr pWave; /* FACTWave* */
		public ushort cueIndex;
		public ushort waveIndex;
		public IntPtr pvContext;
	}

	public struct FACTRuntimeParameters
	{
		public uint lookAheadTime;
		public IntPtr pGlobalSettingsBuffer;
		public uint globalSettingsBufferSize;
		public uint globalSettingsFlags;
		public uint globalSettingsAllocAttributes;
		public FACTFileIOCallbacks fileIOCallbacks;
		public IntPtr fnNotificationCallback; /* FACTNotificationCallback */
		public IntPtr pRendererID; /* wchar_t* */
		public IntPtr pXAudio2;
		public IntPtr pMasteringVoice;
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACTStreamingParameters
	{
		public IntPtr file;
		public uint offset;
		public uint flags;
		public ushort packetSize;
	}

	[StructLayout(LayoutKind.Sequential)] /* FIXME: union! */
	public struct FACTWaveBankMiniWaveFormat
	{
		/*struct
		{
			uint wFormatTag : 2;
			uint nChannels : 3;
			uint nSamplesPerSec : 18;
			uint wBlockAlign : 8;
			uint wBitsPerSample : 1;
		};*/
		public uint dwValue;
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACTWaveBankRegion
	{
		public uint dwOffset;
		public uint dwLength;
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACTWaveBankSampleRegion
	{
		public uint dwStartSample;
		public uint dwTotalSamples;
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACTWaveBankEntry
	{
		public uint dwFlagsAndDuration; /* FIXME: union! */
		FACTWaveBankMiniWaveFormat Format;
		FACTWaveBankRegion PlayRegion;
		FACTWaveBankSampleRegion LoopRegion;
	}

	[StructLayout(LayoutKind.Sequential)]
	public unsafe struct FACTWaveProperties
	{
		public fixed byte friendlyName[64];
		public FACTWaveBankMiniWaveFormat format;
		public uint durationInSamples;
		public FACTWaveBankSampleRegion loopRegion;
		public int streaming;
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACTWaveInstanceProperties
	{
		public FACTWaveProperties properties;
		public int backgroundMusic;
	}

	[StructLayout(LayoutKind.Sequential)]
	public unsafe struct FACTCueProperties
	{
		public fixed char friendlyName[0xFF];
		public int interactive;
		public ushort iaVariableIndex;
		public ushort numVariations;
		public byte maxInstances;
		public byte currentInstances;
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACTTrackProperties
	{
		public uint duration;
		public ushort numVariations;
		public byte numChannels;
		public ushort waveVariation;
		public byte loopCount;
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACTVariationProperties
	{
		public ushort index;
		public byte weight;
		public float iaVariableMin;
		public float iaVariableMax;
		public int linger;
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACTSoundProperties
	{
		public ushort category;
		public byte priority;
		public short pitch;
		public float volume;
		public ushort numTracks;
		public FACTTrackProperties arrTrackProperties; /* FIXME: [1] */
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACTSoundVariationProperties
	{
		public FACTVariationProperties variationProperties;
		public FACTSoundProperties soundProperties;
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACTCueInstanceProperties
	{
		public uint allocAttributes;
		public FACTCueProperties cueProperties;
		public FACTSoundVariationProperties activeVariationProperties;
	}

	#endregion

	#region Constants

	public const int FACT_CONTENT_VERSION = 46;

	public const uint FACT_FLAG_MANAGEDATA =	0x00000001;

	public const uint FACT_FLAG_STOP_RELEASE =	0x00000000;
	public const uint FACT_FLAG_STOP_IMMEDIATE =	0x00000001;

	public const uint FACT_FLAG_BACKGROUND_MUSIC =	0x00000002;
	public const uint FACT_FLAG_UNITS_MS =		0x00000004;
	public const uint FACT_FLAG_UNITS_SAMPLES =	0x00000008;

	public const uint FACT_STATE_CREATED =		0x00000001;
	public const uint FACT_STATE_PREPARING =	0x00000002;
	public const uint FACT_STATE_PREPARED =		0x00000004;
	public const uint FACT_STATE_PLAYING =		0x00000008;
	public const uint FACT_STATE_STOPPING =		0x00000010;
	public const uint FACT_STATE_STOPPED =		0x00000020;
	public const uint FACT_STATE_PAUSED =		0x00000040;
	public const uint FACT_STATE_INUSE =		0x00000080;
	public const uint FACT_STATE_PREPAREFAILED =	0x80000000;

	public const short FACTPITCH_MIN =		-1200;
	public const short FACTPITCH_MAX =		 1200;
	public const short FACTPITCH_MIN_TOTAL =	-2400;
	public const short FACTPITCH_MAX_TOTAL =	 2400;

	public const float FACTVOLUME_MIN = 0.0f;
	public const float FACTVOLUME_MAX = 16777216.0f;

	public const ushort FACTINDEX_INVALID =		0xFFFF;
	public const ushort FACTVARIABLEINDEX_INVALID =	0xFFFF;
	public const ushort FACTCATEGORY_INVALID =	0xFFFF;

	#endregion

	#region AudioEngine Interface

	/* FIXME: Do we want to actually reproduce the COM stuff or what...? -flibit */
	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTCreateEngine(
		uint dwCreationFlags,
		out IntPtr ppEngine /* FACTAudioEngine** */
	);

	/* FIXME: AddRef/Release? Or just ignore COM garbage... -flibit */

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTAudioEngine_GetRendererCount(
		IntPtr pEngine, /* FACTAudioEngine* */
		out ushort pnRendererCount
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTAudioEngine_GetRendererDetails(
		IntPtr pEngine, /* FACTAudioEngine* */
		ushort nRendererIndex,
		out FACTRendererDetails pRendererDetails
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTAudioEngine_GetFinalMixFormat(
		IntPtr pEngine, /* FACTAudioEngine* */
		out FACTWaveFormatExtensible pFinalMixFormat
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTAudioEngine_Initialize(
		IntPtr pEngine, /* FACTAudioEngine* */
		ref FACTRuntimeParameters pParams
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTAudioEngine_Shutdown(
		IntPtr pEngine /* FACTAudioEngine* */
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTAudioEngine_DoWork(
		IntPtr pEngine /* FACTAudioEngine* */
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTAudioEngine_CreateSoundBank(
		IntPtr pEngine, /* FACTAudioEngine* */
		IntPtr pvBuffer,
		uint dwSize,
		uint dwFlags,
		uint dwAllocAttributes,
		out IntPtr ppSoundBank /* FACTSoundBank** */
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTAudioEngine_CreateInMemoryWaveBank(
		IntPtr pEngine, /* FACTAudioEngine* */
		IntPtr pvBuffer,
		uint dwSize,
		uint dwFlags,
		uint dwAllocAttributes,
		out IntPtr ppWaveBank /* FACTWaveBank** */
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTAudioEngine_CreateStreamingWaveBank(
		IntPtr pEngine, /* FACTAudioEngine* */
		ref FACTStreamingParameters pParms,
		out IntPtr ppWaveBank /* FACTWaveBank** */
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTAudioEngine_PrepareWave(
		IntPtr pEngine, /* FACTAudioEngine* */
		uint dwFlags,
		[MarshalAs(UnmanagedType.LPStr)] string szWavePath,
		uint wStreamingPacketSize,
		uint dwAlignment,
		uint dwPlayOffset,
		byte nLoopCount,
		out IntPtr ppWave /* FACTWave** */
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTAudioEngine_PrepareInMemoryWave(
		IntPtr pEngine, /* FACTAudioEngine* */
		uint dwFlags,
		FACTWaveBankEntry entry,
		uint[] pdwSeekTable, /* Optional! */
		byte[] pbWaveData,
		uint dwPlayOffset,
		byte nLoopCount,
		out IntPtr ppWave /* FACTWave** */
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTAudioEngine_PrepareStreamingWave(
		IntPtr pEngine, /* FACTAudioEngine* */
		uint dwFlags,
		FACTWaveBankEntry entry,
		FACTStreamingParameters streamingParams,
		uint dwAlignment,
		uint[] pdwSeekTable, /* Optional! */
		byte[] pbWaveData,
		uint dwPlayOffset,
		byte nLoopCount,
		out IntPtr ppWave /* FACTWave** */
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTAudioEngine_RegisterNotification(
		IntPtr pEngine, /* FACTAudioEngine* */
		ref FACTNotificationDescription pNotificationDescription
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTAudioEngine_UnRegisterNotification(
		IntPtr pEngine, /* FACTAudioEngine* */
		ref FACTNotificationDescription pNotificationDescription
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern ushort FACTAudioEngine_GetCategory(
		IntPtr pEngine, /* FACTAudioEngine* */
		[MarshalAs(UnmanagedType.LPStr)] string szFriendlyName
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTAudioEngine_Stop(
		IntPtr pEngine, /* FACTAudioEngine* */
		ushort nCategory,
		uint dwFlags
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTAudioEngine_SetVolume(
		IntPtr pEngine, /* FACTAudioEngine* */
		ushort nCategory,
		float volume
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTAudioEngine_Pause(
		IntPtr pEngine, /* FACTAudioEngine* */
		ushort nCategory,
		int fPause
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern ushort FACTAudioEngine_GetGlobalVariableIndex(
		IntPtr pEngine, /* FACTAudioEngine* */
		[MarshalAs(UnmanagedType.LPStr)] string szFriendlyName
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTAudioEngine_SetGlobalVariable(
		IntPtr pEngine, /* FACTAudioEngine* */
		ushort nIndex,
		float nValue
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTAudioEngine_GetGlobalVariable(
		IntPtr pEngine, /* FACTAudioEngine* */
		ushort nIndex,
		out float pnValue
	);

	#endregion

	#region SoundBank Interface

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern ushort FACTSoundBank_GetCueIndex(
		IntPtr pSoundBank, /* FACTSoundBank* */
		[MarshalAs(UnmanagedType.LPStr)] string szFriendlyName
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTSoundBank_GetNumCues(
		IntPtr pSoundBank, /* FACTSoundBank* */
		out ushort pnNumCues
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTSoundBank_GetCueProperties(
		IntPtr pSoundBank, /* FACTSoundBank* */
		ushort nCueIndex,
		out FACTCueProperties pProperties
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTSoundBank_Prepare(
		IntPtr pSoundBank, /* FACTSoundBank* */
		ushort nCueIndex,
		uint dwFlags,
		int timeOffset,
		out IntPtr ppCue /* FACTCue** */
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTSoundBank_Play(
		IntPtr pSoundBank, /* FACTSoundBank* */
		ushort nCueIndex,
		uint dwFlags,
		int timeOffset,
		out IntPtr ppCue /* FACTCue** */
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTSoundBank_Play(
		IntPtr pSoundBank, /* FACTSoundBank* */
		ushort nCueIndex,
		uint dwFlags,
		int timeOffset,
		IntPtr ppCue /* Pass IntPtr.Zero! */
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTSoundBank_Stop(
		IntPtr pSoundBank, /* FACTSoundBank* */
		ushort nCueIndex,
		uint dwFlags
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTSoundBank_Destroy(
		IntPtr pSoundBank /* FACTSoundBank* */
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTSoundBank_GetState(
		IntPtr pSoundBank, /* FACTSoundBank* */
		out uint pdwState
	);

	#endregion

	#region WaveBank Interface

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTWaveBank_Destroy(
		IntPtr pWaveBank /* FACTWaveBank* */
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTWaveBank_GetState(
		IntPtr pWaveBank, /* FACTWaveBank* */
		out uint pdwState
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTWaveBank_GetNumWaves(
		IntPtr pWaveBank, /* FACTWaveBank* */
		out ushort pnNumWaves
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern ushort FACTWaveBank_GetWaveIndex(
		IntPtr pWaveBank, /* FACTWaveBank* */
		[MarshalAs(UnmanagedType.LPStr)] string szFriendlyName
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTWaveBank_GetWaveProperties(
		IntPtr pWaveBank, /* FACTWaveBank* */
		ushort nWaveIndex,
		out FACTWaveProperties pWaveProperties
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTWaveBank_Prepare(
		IntPtr pWaveBank, /* FACTWaveBank* */
		ushort nWaveIndex,
		uint dwFlags,
		uint dwPlayOffset,
		byte nLoopCount,
		out IntPtr ppWave /* FACTWave** */
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTWaveBank_Play(
		IntPtr pWaveBank, /* FACTWaveBank* */
		ushort nWaveIndex,
		uint dwFlags,
		uint dwPlayOffset,
		byte nLoopCount,
		out IntPtr ppWave /* FACTWave** */
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTWaveBank_Stop(
		IntPtr pWaveBank, /* FACTWaveBank* */
		ushort nWaveIndex,
		uint dwFlags
	);

	#endregion

	#region Wave Interface

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTWave_Destroy(
		IntPtr pWave /* FACTWave* */
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTWave_Play(
		IntPtr pWave /* FACTWave* */
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTWave_Stop(
		IntPtr pWave, /* FACTWave* */
		uint dwFlags
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTWave_Pause(
		IntPtr pWave, /* FACTWave* */
		int fPause
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTWave_GetState(
		IntPtr pWave, /* FACTWave* */
		out uint pdwState
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTWave_SetPitch(
		IntPtr pWave, /* FACTWave* */
		short pitch
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTWave_SetVolume(
		IntPtr pWave, /* FACTWave* */
		float volume
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTWave_SetMatrixCoefficients(
		IntPtr pWave, /* FACTWave* */
		uint uSrcChannelCount,
		uint uDstChannelCount,
		float[] pMatrixCoefficients
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTWave_GetProperties(
		IntPtr pWave, /* FACTWave* */
		out FACTWaveInstanceProperties pProperties
	);

	#endregion

	#region Cue Interface

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTCue_Destroy(
		IntPtr pCue /* FACTCue* */
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTCue_Play(
		IntPtr pCue /* FACTCue* */
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTCue_Stop(
		IntPtr pCue, /* FACTCue* */
		uint dwFlags
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTCue_GetState(
		IntPtr pCue, /* FACTCue* */
		out uint pdwState
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTCue_SetMatrixCoefficients(
		IntPtr pCue, /* FACTCue* */
		uint uSrcChannelCount,
		uint uDstChannelCount,
		float[] pMatrixCoefficients
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern ushort FACTCue_GetVariableIndex(
		IntPtr pCue, /* FACTCue* */
		[MarshalAs(UnmanagedType.LPStr)] string szFriendlyName
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTCue_SetVariable(
		IntPtr pCue, /* FACTCue* */
		ushort nIndex,
		float nValue
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTCue_GetVariable(
		IntPtr pCue, /* FACTCue* */
		ushort nIndex,
		out float nValue
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTCue_Pause(
		IntPtr pCue, /* FACTCue* */
		int fPause
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTCue_GetProperties(
		IntPtr pCue, /* FACTCue* */
		out FACTCueInstanceProperties ppProperties
	);

	/* FIXME: Can we reproduce these two functions...? -flibit */

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTCue_SetOutputVoices(
		IntPtr pCue, /* FACTCue* */
		IntPtr pSendList /* Optional XAUDIO2_VOICE_SENDS */
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACTCue_SetOutputVoiceMatrix(
		IntPtr pCue, /* FACTCue* */
		IntPtr pDestinationVoice, /* Optional IXAudio2Voice */
		uint SourceChannels,
		uint DestinationChannels,
		float[] pLevelMatrix /* SourceChannels * DestinationChannels */
	);

	#endregion

	#region 3D Audio API

	public const float FACT3DAUDIO_PI =	3.141592654f;
	public const float FACT3DAUDIO_2PI =	6.283185307f;

	public const float LEFT_AZIMUTH =			(3.0f * FACT3DAUDIO_PI / 2.0f);
	public const float RIGHT_AZIMUTH =			(FACT3DAUDIO_PI / 2.0f);
	public const float FRONT_LEFT_AZIMUTH =			(7.0f * FACT3DAUDIO_PI / 4.0f);
	public const float FRONT_RIGHT_AZIMUTH =		(FACT3DAUDIO_PI / 4.0f);
	public const float FRONT_CENTER_AZIMUTH =		0.0f;
	public const float LOW_FREQUENCY_AZIMUTH =		FACT3DAUDIO_2PI;
	public const float BACK_LEFT_AZIMUTH =			(5.0f * FACT3DAUDIO_PI / 4.0f);
	public const float BACK_RIGHT_AZIMUTH =			(3.0f * FACT3DAUDIO_PI / 4.0f);
	public const float BACK_CENTER_AZIMUTH =		FACT3DAUDIO_PI;
	public const float FRONT_LEFT_OF_CENTER_AZIMUTH =	(15.0f * FACT3DAUDIO_PI / 8.0f);
	public const float FRONT_RIGHT_OF_CENTER_AZIMUTH =	(FACT3DAUDIO_PI / 8.0f);

	public const uint SPEAKER_FRONT_LEFT =			0x00000001;
	public const uint SPEAKER_FRONT_RIGHT =			0x00000002;
	public const uint SPEAKER_FRONT_CENTER =		0x00000004;
	public const uint SPEAKER_LOW_FREQUENCY =		0x00000008;
	public const uint SPEAKER_BACK_LEFT =			0x00000010;
	public const uint SPEAKER_BACK_RIGHT =			0x00000020;
	public const uint SPEAKER_FRONT_LEFT_OF_CENTER =	0x00000040;
	public const uint SPEAKER_FRONT_RIGHT_OF_CENTER =	0x00000080;
	public const uint SPEAKER_BACK_CENTER =			0x00000100;
	public const uint SPEAKER_SIDE_LEFT =			0x00000200;
	public const uint SPEAKER_SIDE_RIGHT =			0x00000400;
	public const uint SPEAKER_TOP_CENTER =			0x00000800;
	public const uint SPEAKER_TOP_FRONT_LEFT =		0x00001000;
	public const uint SPEAKER_TOP_FRONT_CENTER =		0x00002000;
	public const uint SPEAKER_TOP_FRONT_RIGHT =		0x00004000;
	public const uint SPEAKER_TOP_BACK_LEFT =		0x00008000;
	public const uint SPEAKER_TOP_BACK_CENTER =		0x00010000;
	public const uint SPEAKER_TOP_BACK_RIGHT =		0x00020000;

	public const uint SPEAKER_MONO =	SPEAKER_FRONT_CENTER;
	public const uint SPEAKER_STEREO =	(SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT);
	public const uint SPEAKER_2POINT1 =
		(	SPEAKER_FRONT_LEFT	|
			SPEAKER_FRONT_RIGHT	|
			SPEAKER_LOW_FREQUENCY	);
	public const uint SPEAKER_SURROUND =
		(	SPEAKER_FRONT_LEFT	|
			SPEAKER_FRONT_RIGHT	|
			SPEAKER_FRONT_CENTER	|
			SPEAKER_BACK_CENTER	);
	public const uint SPEAKER_QUAD =
		(	SPEAKER_FRONT_LEFT	|
			SPEAKER_FRONT_RIGHT	|
			SPEAKER_BACK_LEFT	|
			SPEAKER_BACK_RIGHT	);
	public const uint SPEAKER_4POINT1 =
		(	SPEAKER_FRONT_LEFT	|
			SPEAKER_FRONT_RIGHT	|
			SPEAKER_LOW_FREQUENCY	|
			SPEAKER_BACK_LEFT	|
			SPEAKER_BACK_RIGHT	);
	public const uint SPEAKER_5POINT1 =
		(	SPEAKER_FRONT_LEFT	|
			SPEAKER_FRONT_RIGHT	|
			SPEAKER_FRONT_CENTER	|
			SPEAKER_LOW_FREQUENCY	|
			SPEAKER_BACK_LEFT	|
			SPEAKER_BACK_RIGHT	);
	public const uint SPEAKER_7POINT1 =
		(	SPEAKER_FRONT_LEFT		|
			SPEAKER_FRONT_RIGHT		|
			SPEAKER_FRONT_CENTER		|
			SPEAKER_LOW_FREQUENCY		|
			SPEAKER_BACK_LEFT		|
			SPEAKER_BACK_RIGHT		|
			SPEAKER_FRONT_LEFT_OF_CENTER	|
			SPEAKER_FRONT_RIGHT_OF_CENTER	);
	public const uint SPEAKER_5POINT1_SURROUND =
		(	SPEAKER_FRONT_LEFT	|
			SPEAKER_FRONT_RIGHT	|
			SPEAKER_FRONT_CENTER	|
			SPEAKER_LOW_FREQUENCY	|
			SPEAKER_SIDE_LEFT	|
			SPEAKER_SIDE_RIGHT	);
	public const uint SPEAKER_7POINT1_SURROUND =
		(	SPEAKER_FRONT_LEFT	|
			SPEAKER_FRONT_RIGHT	|
			SPEAKER_FRONT_CENTER	|
			SPEAKER_LOW_FREQUENCY	|
			SPEAKER_BACK_LEFT	|
			SPEAKER_BACK_RIGHT	|
			SPEAKER_SIDE_LEFT	|
			SPEAKER_SIDE_RIGHT	);

	/* FIXME: Hmmmm */
	public const uint SPEAKER_XBOX = SPEAKER_5POINT1;

	public const uint FACT3DAUDIO_CALCULATE_MATRIX =		0x00000001;
	public const uint FACT3DAUDIO_CALCULATE_DELAY =			0x00000002;
	public const uint FACT3DAUDIO_CALCULATE_LPF_DIRECT =		0x00000004;
	public const uint FACT3DAUDIO_CALCULATE_LPF_REVERB =		0x00000008;
	public const uint FACT3DAUDIO_CALCULATE_REVERB =		0x00000010;
	public const uint FACT3DAUDIO_CALCULATE_DOPPLER =		0x00000020;
	public const uint FACT3DAUDIO_CALCULATE_EMITTER_ANGLE =		0x00000040;
	public const uint FACT3DAUDIO_CALCULATE_ZEROCENTER =		0x00010000;
	public const uint FACT3DAUDIO_CALCULATE_REDIRECT_TO_LFE =	0x00020000;

	/* FIXME: Everything about this type blows */
	public const int FACT3DAUDIO_HANDLE_BYTESIZE = 20;
	[StructLayout(LayoutKind.Sequential)]
	public unsafe struct FACT3DAUDIO_HANDLE
	{
		public fixed byte handle[FACT3DAUDIO_HANDLE_BYTESIZE];
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACT3DAUDIO_VECTOR
	{
		public float x;
		public float y;
		public float z;
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACT3DAUDIO_DISTANCE_CURVE_POINT
	{
		public float Distance;
		public float DSPSetting;
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACT3DAUDIO_DISTANCE_CURVE
	{
		IntPtr pPoints; /* FACT3DAUDIO_DISTANCE_CURVE_POINT* */
		public uint PointCount;
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACT3DAUDIO_CONE
	{
		public float InnerAngle;
		public float OuterAngle;
		public float InnerVolume;
		public float OuterVolume;
		public float InnerLPF;
		public float OuterLPF;
		public float InnerReverb;
		public float OuterReverb;
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACT3DAUDIO_LISTENER
	{
		public FACT3DAUDIO_VECTOR OrientFront;
		public FACT3DAUDIO_VECTOR OrientTop;
		public FACT3DAUDIO_VECTOR Position;
		public FACT3DAUDIO_VECTOR Velocity;
		public IntPtr pCone; /* FACT3DAUDIO_CONE* */
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACT3DAUDIO_EMITTER
	{
		public IntPtr pCone; /* FACT3DAUDIO_CONE* */
		public FACT3DAUDIO_VECTOR OrientFront;
		public FACT3DAUDIO_VECTOR OrientTop;
		public FACT3DAUDIO_VECTOR Position;
		public FACT3DAUDIO_VECTOR Velocity;
		public float InnerRadius;
		public float InnerRadiusAngle;
		public uint ChannelCount;
		public float ChannelRadius;
		public IntPtr pChannelAzimuths; /* float */
		public IntPtr pVolumeCurve;
		public IntPtr pLFECurve; /* FACT3DAUDIO_DISTANCE_CURVE* */
		public IntPtr pLPFDirectCurve; /* FACT3DAUDIO_DISTANCE_CURVE* */
		public IntPtr pLPFReverbCurve; /* FACT3DAUDIO_DISTANCE_CURVE* */
		public IntPtr pReverbCurve; /* FACT3DAUDIO_DISTANCE_CURVE* */
		public float CurveDistanceScaler;
		public float DopplerScaler;
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct FACT3DAUDIO_DSP_SETTINGS
	{
		public IntPtr pMatrixCoefficients; /* float* */
		public IntPtr pDelayTimes; /* float* */
		public uint SrcChannelCount;
		public uint DstChannelCount;
		public float LPFDirectCoefficient;
		public float LPFReverbCoefficient;
		public float ReverbLevel;
		public float DopplerFactor;
		public float EmitterToListenerAngle;
		public float EmitterToListenerDistance;
		public float EmitterVelocityComponent;
		public float ListenerVelocityComponent;
	}

	public static readonly float[] aStereoLayout = new float[]
	{
		LEFT_AZIMUTH,
		RIGHT_AZIMUTH
	};
	public static readonly float[] a2Point1Layout = new float[]
	{
		LEFT_AZIMUTH,
		RIGHT_AZIMUTH,
		LOW_FREQUENCY_AZIMUTH
	};
	public static readonly float[] aQuadLayout = new float[]
	{
		FRONT_LEFT_AZIMUTH,
		FRONT_RIGHT_AZIMUTH,
		BACK_LEFT_AZIMUTH,
		BACK_RIGHT_AZIMUTH
	};
	public static readonly float[] a4Point1Layout = new float[]
	{
		FRONT_LEFT_AZIMUTH,
		FRONT_RIGHT_AZIMUTH,
		LOW_FREQUENCY_AZIMUTH,
		BACK_LEFT_AZIMUTH,
		BACK_RIGHT_AZIMUTH
	};
	public static readonly float[] a5Point1Layout = new float[]
	{
		FRONT_LEFT_AZIMUTH,
		FRONT_RIGHT_AZIMUTH,
		FRONT_CENTER_AZIMUTH,
		LOW_FREQUENCY_AZIMUTH,
		BACK_LEFT_AZIMUTH,
		BACK_RIGHT_AZIMUTH
	};
	public static readonly float[] a7Point1Layout = new float[]
	{
		FRONT_LEFT_AZIMUTH,
		FRONT_RIGHT_AZIMUTH,
		FRONT_CENTER_AZIMUTH,
		LOW_FREQUENCY_AZIMUTH,
		BACK_LEFT_AZIMUTH,
		BACK_RIGHT_AZIMUTH,
		LEFT_AZIMUTH,
		RIGHT_AZIMUTH
	};

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern void FACT3DAudioInitialize(
		uint SpeakerChannelMask,
		float SpeedOfSound,
		FACT3DAUDIO_HANDLE Instance
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern void FACT3DAudioCalculate(
		FACT3DAUDIO_HANDLE Instance,
		ref FACT3DAUDIO_LISTENER pListener,
		ref FACT3DAUDIO_EMITTER pEmitter,
		uint Flags,
		out FACT3DAUDIO_DSP_SETTINGS pDSPSettings
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACT3DInitialize(
		IntPtr pEngine, /* FACTAudioEngine* */
		FACT3DAUDIO_HANDLE F3DInstance
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACT3DCalculate(
		FACT3DAUDIO_HANDLE F3DInstance,
		ref FACT3DAUDIO_LISTENER pListener,
		ref FACT3DAUDIO_EMITTER pEmitter,
		out FACT3DAUDIO_DSP_SETTINGS pDSPSettings
	);

	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern uint FACT3DApply(
		ref FACT3DAUDIO_DSP_SETTINGS pDSPSettings,
		IntPtr pCue /* FACTCue* */
	);

	#endregion

	#region FACT I/O API

	/* IntPtr refers to an FACTIOStream* */
	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern IntPtr FACT_fopen([MarshalAs(UnmanagedType.LPStr)] string path);

	/* IntPtr refers to an FACTIOStream* */
	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern IntPtr FACT_memopen(IntPtr mem, int len);

	/* io refers to an FACTIOStream* */
	[DllImport(nativeLibName, CallingConvention = CallingConvention.Cdecl)]
	public static extern void FACT_close(IntPtr io);

	#endregion
}
