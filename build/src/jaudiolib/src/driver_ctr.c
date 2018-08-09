/*
 Copyright (C) 2015 Felipe Izzo
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 
 See the GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 
 */


 /* Fixes needed */

#include <vitasdk.h>
#include "inttypes.h"

#ifndef UNREFERENCED_PARAMETER
# define UNREFERENCED_PARAMETER(x) x=x
#endif

#define TICKS_PER_SEC 1000000

static int32_t Initialized = 0;
static int32_t Playing = 0;
static int32_t nSamples = 0;

int chn = -1;

volatile int stop_audio = 0;
volatile uint32_t bufsize = 0;

uint64_t lastMix =0;

volatile uint8_t *audiobuffer;

#define SAMPLE_RATE   44100
#define AUDIOSIZE 16384

static int audio_thread(int args, void *argp)
{
	chn = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, AUDIOSIZE / 2, SAMPLE_RATE, SCE_AUDIO_OUT_MODE_MONO);
	sceAudioOutSetConfig(chn, -1, -1, -1);
	int vol[] = {32767, 32767};
    sceAudioOutSetVolume(chn, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, vol);
	
    while (!stop_audio)
    {
		//if (update){ 
		int i = 0;
		while (i < bufsize) {
			sceAudioOutOutput(chn, &audiobuffer[i]);
			i += AUDIOSIZE;
		}
			//update = false;
		//}
		
    }
	 
	sceAudioOutReleasePort(chn);
    free(audiobuffer);

    sceKernelExitDeleteThread(0);
    return 0;
}

int32_t CTRDrv_GetError(void)
{
    return 0;
}

const char *CTRDrv_ErrorString( int32_t ErrorNumber )
{
    UNREFERENCED_PARAMETER(ErrorNumber);
    return "No sound, Ok.";
}

int32_t CTRDrv_PCM_Init(int32_t *mixrate, int32_t *numchannels, void * initdata)
{

    Initialized = 1;
	
	SceUID audiothread = sceKernelCreateThread("Audio Thread", (void*)&audio_thread, 0x10000100, 0x10000, 0, 0, NULL);
	int res = sceKernelStartThread(audiothread, sizeof(audiothread), &audiothread);
	if (res != 0){
		return 0;
	}

    UNREFERENCED_PARAMETER(numchannels);
    UNREFERENCED_PARAMETER(initdata);
    return 0;
}

void CTRDrv_PCM_Shutdown(void)
{
    if(!Initialized)
        return;

    Initialized = 0;
	stop_audio = 1;
}

int32_t CTRDrv_PCM_BeginPlayback(char *BufferStart, int32_t BufferSize,
						int32_t NumDivisions, void ( *CallBackFunc )( void ) )
{
    if(!Initialized)
        return 0;

	audiobuffer = BufferStart;
    bufsize = BufferSize;

    UNREFERENCED_PARAMETER(NumDivisions);
    
    return 0;
}

void CTRDrv_PCM_StopPlayback(void)
{
    if(!Initialized)
        return;

    stop_audio = 1;
}

void CTRDrv_PCM_Lock(void)
{
}

void CTRDrv_PCM_Unlock(void)
{
}
