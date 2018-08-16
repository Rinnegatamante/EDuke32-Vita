//-------------------------------------------------------------------------
/*
Copyright (C) 2010 EDuke32 developers and contributors

This file is part of EDuke32.

EDuke32 is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
//-------------------------------------------------------------------------

/*
 * A reimplementation of Jim Dose's FX_MAN routines, using  SDL_mixer 1.2.
 *   Whee. FX_MAN is also known as the "Apogee Sound System", or "ASS" for
 *   short. How strangely appropriate that seems.
 */

#define _NEED_SDLMIXER	1

#include <vitasdk.h>
#include "audio_decoder.h"

#include "compat.h"

#include <stdio.h>
#include <errno.h>

#include "duke3d.h"
extern "C"{
#include "cache1d.h"
#include "sdlayer.h"
#include "music.h"
};

#if defined FORK_EXEC_MIDI  // fork/exec based external midi player
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
static char **external_midi_argv;
static pid_t external_midi_pid=-1;
static int8_t external_midi_restart=0;
#endif

#ifdef __ANDROID__ //TODO fix
static char *external_midi_tempfn = "eduke32-music.mid";
#else
static char *external_midi_tempfn = "ux0:data/EDuke32/eduke32-music.mid";
#endif

static int32_t external_midi = 1;

int32_t MUSIC_ErrorCode = MUSIC_Ok;

static char warningMessage[80];
static char errorMessage[80];

static int32_t music_initialized = 0;
static int32_t music_context = 0;
static int32_t music_loopflag = MUSIC_PlayOnce;
static Mix_Music *music_musicchunk = NULL;

#define BUFSIZE 8192 // Max dimension of audio buffer size
#define NSAMPLES 2048 // Number of samples for output

static void setErrorMessage(const char *msg)
{
    Bstrncpyz(errorMessage, msg, sizeof(errorMessage));
}

// Music block struct
struct DecodedMusic{
	uint8_t* audiobuf;
	uint8_t* audiobuf2;
	uint8_t* cur_audiobuf;
	FILE* handle;
	bool isPlaying;
	bool loop;
	volatile bool pauseTrigger;
	volatile bool closeTrigger;
	volatile bool changeVol;
};

// Internal stuffs
DecodedMusic* BGM = NULL;
std::unique_ptr<AudioDecoder> audio_decoder;
SceUID thread, Audio_Mutex, Talk_Mutex;
volatile bool mustExit = false;
float old_vol = 1.0;
int32_t bgmvolume = 255;

// Audio thread code
static int bgmThread(unsigned int args, void* arg){
	
	// Initializing audio port
	int ch = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, NSAMPLES, 48000, SCE_AUDIO_OUT_MODE_STEREO);
	sceAudioOutSetConfig(ch, -1, -1, (SceAudioOutMode)-1);
	old_vol = bgmvolume;
	int vol = 128 * bgmvolume;
	int vol_stereo[] = {vol, vol};
	sceAudioOutSetVolume(ch, (SceAudioOutChannelFlag)(SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH), vol_stereo);
	
	DecodedMusic* mus;
	for (;;){
		
		// Waiting for an audio output request
		sceKernelWaitSema(Audio_Mutex, 1, NULL);
		
		// Fetching track
		mus = BGM;
		
		// Checking if a new track is available
		if (mus == NULL){
			
			//If we enter here, we probably are in the exiting procedure
			if (mustExit){
				sceAudioOutReleasePort(ch);
				mustExit = false;
				sceKernelExitThread(0);
			}
		
		}
		
		// Initializing audio decoder
		audio_decoder = AudioDecoder::Create(mus->handle, "Track");
		audio_decoder->Open(mus->handle);
		audio_decoder->SetLooping(mus->loop);
		audio_decoder->SetFormat(48000, AudioDecoder::Format::S16, 2);
		
		// Initializing audio buffers
		mus->audiobuf = (uint8_t*)malloc(BUFSIZE);
		mus->audiobuf2 = (uint8_t*)malloc(BUFSIZE);
		mus->cur_audiobuf = mus->audiobuf;
		
		// Audio playback loop
		for (;;){
		
			// Check if the music must be paused
			if (mus->pauseTrigger || mustExit){
			
				// Check if the music must be closed
				if (mus->closeTrigger){
					free(mus->audiobuf);
					free(mus->audiobuf2);
					audio_decoder.reset();
					free(mus);
					BGM = NULL;
					if (!mustExit){
						sceKernelSignalSema(Talk_Mutex, 1);
						break;
					}
				}
				
				// Check if the thread must be closed
				if (mustExit){
				
					// Check if the audio stream has already been closed
					if (mus != NULL){
						mus->closeTrigger = true;
						continue;
					}
					
					// Recursively closing all the threads
					sceAudioOutReleasePort(ch);
					mustExit = false;
					sceKernelExitDeleteThread(0);
					
				}
			
				mus->isPlaying = !mus->isPlaying;
				mus->pauseTrigger = false;
			}
			
			// Check if a volume change is required
			if (mus->changeVol){
				old_vol = bgmvolume;
				int vol = 128 * bgmvolume;
				int vol_stereo[] = {vol, vol};
				sceAudioOutSetVolume(ch, (SceAudioOutChannelFlag)(SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH), vol_stereo);
				mus->changeVol = false;
			}
			
			if (mus->isPlaying){
				
				// Check if audio playback finished
				if ((!mus->loop) && audio_decoder->IsFinished()) mus->isPlaying = false;
				
				// Update audio output
				if (mus->cur_audiobuf == mus->audiobuf) mus->cur_audiobuf = mus->audiobuf2;
				else mus->cur_audiobuf = mus->audiobuf;
				audio_decoder->Decode(mus->cur_audiobuf, BUFSIZE);	
				sceAudioOutOutput(ch, mus->cur_audiobuf);
				
			}
			
		}
		
	}
	
}

extern "C"{

// The music functions...

const char *MUSIC_ErrorString(int32_t ErrorNumber)
{
    switch (ErrorNumber)
    {
    case MUSIC_Warning:
        return(warningMessage);

    case MUSIC_Error:
        return(errorMessage);

    case MUSIC_Ok:
        return("OK; no error.");

    case MUSIC_ASSVersion:
        return("Incorrect sound library version.");

    case MUSIC_SoundCardError:
        return("General sound card error.");

    case MUSIC_InvalidCard:
        return("Invalid sound card.");

    case MUSIC_MidiError:
        return("MIDI error.");

    case MUSIC_MPU401Error:
        return("MPU401 error.");

    case MUSIC_TaskManError:
        return("Task Manager error.");

        //case MUSIC_FMNotDetected:
        //    return("FM not detected error.");

    case MUSIC_DPMI_Error:
        return("DPMI error.");

    default:
        return("Unknown error.");
    } // switch

    return(NULL);
} // MUSIC_ErrorString

int32_t MUSIC_Init(int32_t SoundCard, int32_t Address)
{

    UNREFERENCED_PARAMETER(SoundCard);
    UNREFERENCED_PARAMETER(Address);

    if (music_initialized)
    {
        setErrorMessage("Music system is already initialized.");
        return(MUSIC_Error);
    } // if

    if (external_midi)
    {
		// Creating audio mutex
		Audio_Mutex = sceKernelCreateSema("Audio Mutex", 0, 0, 1, NULL);
		Talk_Mutex = sceKernelCreateSema("Talk Mutex", 0, 0, 1, NULL);
	
		// Creating audio thread
		thread = sceKernelCreateThread("Audio Thread", &bgmThread, 0x10000100, 0x10000, 0, 0, NULL);
		sceKernelStartThread(thread, sizeof(thread), &thread);
        music_initialized = 1;
        return(MUSIC_Ok);

    }

    music_initialized = 1;
    return(MUSIC_Ok);
} // MUSIC_Init


int32_t MUSIC_Shutdown(void)
{
    // TODO - make sure this is being called from the menu -- SA
#if !defined FORK_EXEC_MIDI
    if (external_midi)
        Mix_SetMusicCMD(NULL);
#endif

    MUSIC_StopSong();
    music_context = 0;
    music_initialized = 0;
    music_loopflag = MUSIC_PlayOnce;
	
	mustExit = true;
	sceKernelSignalSema(Audio_Mutex, 1);
	sceKernelWaitThreadEnd(thread, NULL, NULL);
	sceKernelDeleteSema(Audio_Mutex);
	sceKernelDeleteSema(Talk_Mutex);
	sceKernelDeleteThread(thread);

    return(MUSIC_Ok);
} // MUSIC_Shutdown


void MUSIC_SetMaxFMMidiChannel(int32_t channel)
{
    UNREFERENCED_PARAMETER(channel);
} // MUSIC_SetMaxFMMidiChannel


void MUSIC_SetVolume(int32_t volume)
{
    volume = max(0, volume);
    volume = min(volume, 255);
	bgmvolume = volume;
	if (BGM != NULL){
		BGM->changeVol = true;
	}
} // MUSIC_SetVolume


void MUSIC_SetMidiChannelVolume(int32_t channel, int32_t volume)
{
    UNREFERENCED_PARAMETER(channel);
    UNREFERENCED_PARAMETER(volume);
} // MUSIC_SetMidiChannelVolume


void MUSIC_ResetMidiChannelVolumes(void)
{
} // MUSIC_ResetMidiChannelVolumes


int32_t MUSIC_GetVolume(void)
{
    return(bgmvolume);  // convert 0-128 to 0-255.
} // MUSIC_GetVolume


void MUSIC_SetLoopFlag(int32_t loopflag)
{
	if (BGM != NULL){ BGM->loop = (loopflag > 1); }
    music_loopflag = loopflag;
} // MUSIC_SetLoopFlag


int32_t MUSIC_SongPlaying(void)
{
	if (BGM != NULL) return BGM->isPlaying;
	else return FALSE;
} // MUSIC_SongPlaying


void MUSIC_Continue(void)
{
    if (BGM != NULL) BGM->pauseTrigger = true;
} // MUSIC_Continue


void MUSIC_Pause(void)
{
    if (BGM != NULL) BGM->pauseTrigger = true;
} // MUSIC_Pause

int32_t MUSIC_StopSong(void)
{
	if (BGM != NULL){
		BGM->closeTrigger = true;
		BGM->pauseTrigger = true;
		sceKernelWaitSema(Talk_Mutex, 1, NULL);
	}

    music_musicchunk = NULL;

    return(MUSIC_Ok);
} // MUSIC_StopSong

// Duke3D-specific.  --ryan.
// void MUSIC_PlayMusic(char *_filename)
int32_t MUSIC_PlaySong(char *song, int32_t loopflag)
{
    MUSIC_StopSong();

    if (external_midi)
    {
        FILE *fp;

        fp = Bfopen(external_midi_tempfn, "wb");
        if (fp)
        {
            fwrite(song, 1, g_musicSize, fp);
            Bfclose(fp);
        }
        else initprintf("%s: fopen: %s\n", __func__, strerror(errno));
    }
	
	FILE* fd = fopen(external_midi_tempfn, "rb");
	DecodedMusic* memblock = (DecodedMusic*)malloc(sizeof(DecodedMusic));
	memblock->handle = fd;
	memblock->pauseTrigger = false;
	memblock->closeTrigger = false;
	memblock->isPlaying = true;
	memblock->loop = loopflag;
	BGM = memblock;
	sceKernelSignalSema(Audio_Mutex, 1);
	
    return MUSIC_Ok;
}


void MUSIC_SetContext(int32_t context)
{
    music_context = context;
} // MUSIC_SetContext


int32_t MUSIC_GetContext(void)
{
    return(music_context);
} // MUSIC_GetContext


void MUSIC_SetSongTick(uint32_t PositionInTicks)
{
    UNREFERENCED_PARAMETER(PositionInTicks);
} // MUSIC_SetSongTick


void MUSIC_SetSongTime(uint32_t milliseconds)
{
    UNREFERENCED_PARAMETER(milliseconds);
}// MUSIC_SetSongTime


void MUSIC_SetSongPosition(int32_t measure, int32_t beat, int32_t tick)
{
    UNREFERENCED_PARAMETER(measure);
    UNREFERENCED_PARAMETER(beat);
    UNREFERENCED_PARAMETER(tick);
} // MUSIC_SetSongPosition


void MUSIC_GetSongPosition(songposition *pos)
{
    UNREFERENCED_PARAMETER(pos);
} // MUSIC_GetSongPosition


void MUSIC_GetSongLength(songposition *pos)
{
    UNREFERENCED_PARAMETER(pos);
} // MUSIC_GetSongLength


int32_t MUSIC_FadeVolume(int32_t tovolume, int32_t milliseconds)
{
    UNREFERENCED_PARAMETER(tovolume);
    return(MUSIC_Ok);
} // MUSIC_FadeVolume


int32_t MUSIC_FadeActive(void)
{
    return(FALSE);
} // MUSIC_FadeActive


void MUSIC_StopFade(void)
{
} // MUSIC_StopFade


void MUSIC_RerouteMidiChannel(int32_t channel, int32_t (*function)(int32_t, int32_t, int32_t))
{
    UNREFERENCED_PARAMETER(channel);
    UNREFERENCED_PARAMETER(function);
} // MUSIC_RerouteMidiChannel


void MUSIC_RegisterTimbreBank(char *timbres)
{
    UNREFERENCED_PARAMETER(timbres);
} // MUSIC_RegisterTimbreBank


void MUSIC_Update(void)
{}

};
