/*  gngeo a neogeo emulator
 *  Copyright (C) 2001 Peponas Mathieu
 * 
 *  This program is free software; you can redistribute it and/or modify  
 *  it under the terms of the GNU General Public License as published by   
 *  the Free Software Foundation; either version 2 of the License, or    
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 
 */  
    
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <devices/ahi.h>
#include <exec/exec.h>
#include <proto/ahi.h>
#include <proto/dos.h>
#include <proto/exec.h>
 
#include "emu.h"
#include "memory.h"
#include "profiler.h"
#include "ym2610/ym2610.h"
#ifdef GP2X
#include "ym2610-940/940shared.h"
#endif

#ifdef USE_OSS
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <pthread.h>

#endif

#define USE_AHI_V4 TRUE

#define CHANNELS   2
#define MAXSAMPLES 16

#define INT_FREQ   50

#define MIXER_MAX_CHANNELS 16
//#define CPU_FPS 60
#define BUFFER_LEN 16384
//#define BUFFER_LEN 
extern int throttle;
static int audio_sample_rate;
Uint16 play_buffer[BUFFER_LEN];

#ifndef GP2X
#define NB_SAMPLES 512 /* better resolution */
#else
//#define NB_SAMPLES 128
//#define NB_SAMPLES 64
//#define NB_SAMPLES 512
#endif
Uint8 * stream = NULL;

static ULONG ahiID;

APTR samples[MAXSAMPLES] = { 0 };
struct AHISampleInfo sample;

struct {
  BOOL      FadeVolume;
  Fixed     Volume;
  sposition Position;
} channelstate[CHANNELS];

struct {
  struct AHIEffDSPMask mask;
  UBYTE                mode[CHANNELS];
} maskeffect = {0};
struct AHIEffDSPEcho   echoeffect = {0};

struct Library      *AHIBase;
struct MsgPort      *AHImp     = NULL;
struct AHIRequest   *AHIio     = NULL;
BYTE                 AHIDevice = -1;
struct AHIAudioCtrl *actrl     = NULL;

LONG mixfreq = 0;

/* Prototypes */

BOOL  OpenAHI(void);
void  CloseAHI(void);
BOOL  AllocAudio(void);
void  FreeAudio(void);
UWORD LoadSample(unsigned char * , ULONG );
void  UnloadSample(UWORD );


/******************************************************************************
** PlayerFunc *****************************************************************
******************************************************************************/

  __interrupt  static void PlayerFunc(
      struct Hook *hook,
     struct AHIAudioCtrl *actrl,
    APTR ignored) {

 
  if (stream)
{
	YM2610Update_stream(NB_SAMPLES);
	memcpy(stream, (Uint8 *) play_buffer, NB_SAMPLES << 2);
}
	
 
}

struct Hook PlayerHook = {
  0,0,
  (ULONG (* )()) PlayerFunc,
  NULL,
  NULL,
};

/******************************************************************************
**** OpenAHI ******************************************************************
******************************************************************************/

/* Open the device for low-level usage */

BOOL OpenAHI(void) {

  if(AHImp = CreateMsgPort()) {
    if(AHIio = (struct AHIRequest *)CreateIORequest(
        AHImp,sizeof(struct AHIRequest))) {

#if USE_AHI_V4
      AHIio->ahir_Version = 4;
#else
      AHIio->ahir_Version = 2;
#endif

      if(!(AHIDevice = OpenDevice(AHINAME, AHI_NO_UNIT,
          (struct IORequest *) AHIio,NULL))) {
        AHIBase = (struct Library *) AHIio->ahir_Std.io_Device;
        return TRUE;
      }
    }
  }
  FreeAudio();
  return FALSE;
}


/******************************************************************************
**** CloseAHI *****************************************************************
******************************************************************************/

/* Close the device */

void CloseAHI(void) {

  if(! AHIDevice)
    CloseDevice((struct IORequest *)AHIio);
  AHIDevice=-1;
  DeleteIORequest((struct IORequest *)AHIio);
  AHIio=NULL;
  DeleteMsgPort(AHImp);
  AHImp=NULL;
}


/******************************************************************************
**** AllocAudio ***************************************************************
******************************************************************************/

/* Ask user for an audio mode and allocate it */

BOOL AllocAudio(void) {
  struct AHIAudioModeRequester *req;
  BOOL   rc = FALSE;

 
  

	    ahiID = AHI_BestAudioID(
				     AHIDB_Stereo,TRUE,
				     AHIDB_Panning,TRUE,
				     AHIDB_Bits,8,
				     AHIDB_MaxChannels,2,
				     AHIDB_MinMixFreq,11025,
				     AHIDB_MaxMixFreq,22050,
				     TAG_DONE);
				     
	actrl = AHI_AllocAudio( AHIA_AudioID,ahiID,
				AHIA_MixFreq,11025,
				AHIA_Channels,1,
				AHIA_Sounds, 1,
				AHIA_SoundFunc,(int)&PlayerHook,
				TAG_DONE);				     
                       
  return rc;
}


/******************************************************************************
**** FreeAudio ****************************************************************
******************************************************************************/

/* Release the audio hardware */

void FreeAudio() {

  AHI_FreeAudio(actrl);
  actrl = NULL;
}
 

int init_audio(void)
{
    stream = NULL;
    //SDL_InitSubSystem(SDL_INIT_AUDIO);

//    desired = (SDL_AudioSpec *) malloc(sizeof(SDL_AudioSpec));
//    obtain = (SDL_AudioSpec *) malloc(sizeof(SDL_AudioSpec));
    audio_sample_rate = conf.sample_rate;
//    desired->freq = conf.sample_rate;
//    desired->samples = NB_SAMPLES;
    
#ifdef WORDS_BIGENDIAN
//    desired->format = AUDIO_S16MSB;
#else	/* */
//    desired->format = AUDIO_S16;
#endif	/* */
//    desired->channels = 2;
//    desired->callback = update_sdl_stream;
	 
	//desired->callback = NULL;
//    desired->userdata = NULL;
    //SDL_OpenAudio(desired, NULL);
//    SDL_OpenAudio(desired, obtain);
//    printf("Obtained sample rate: %d\n",obtain->freq);
//    conf.sample_rate=obtain->freq;

    OpenAHI();
    AllocAudio();
    
    ULONG obtainedMixingfrequency = 0;
    ULONG audioCallbackFrequency = 22050;    
    AHI_ControlAudio(actrl, AHIC_MixFreq_Query, (Tag)&obtainedMixingfrequency, TAG_DONE);
    
    // Calculate the sample factor.
    ULONG sampleCount = (ULONG)floor(obtainedMixingfrequency / audioCallbackFrequency);
      
    // 32 bits (4 bytes) are required per sample for storage (16bit stereo).
    ULONG sampleBufferSize = (NB_SAMPLES * AHI_SampleFrameSize(AHIST_M16S));

    stream = (unsigned char *)AllocVec(sampleBufferSize, MEMF_PUBLIC|MEMF_CLEAR);
    memset(stream,0x00, sampleBufferSize);
    sample.ahisi_Type = AHIST_M16S;
    sample.ahisi_Address = stream; 
    sample.ahisi_Length = sampleBufferSize;

    AHI_LoadSound(0, AHIST_DYNAMICSAMPLE, &sample, actrl);

    /* start playing sample 0 */
    if(!(AHI_ControlAudio(actrl, AHIC_Play, TRUE, TAG_DONE)))
    {
        AHI_SetFreq(0, 11025, actrl, AHISF_IMM);
        AHI_SetVol(0, 0x10000, 0x8000, actrl, AHISF_IMM);
        AHI_SetSound(0, 0, 0, 0, actrl, AHISF_IMM);
    }

    
    return 1;
}

void close_audio(void) {
//    SDL_PauseAudio(1);
//    SDL_CloseAudio();
//    SDL_QuitSubSystem(SDL_INIT_AUDIO);
//	if (desired) free(desired);
//	desired = NULL;
//	if (obtain) free(obtain);
//	obtain = NULL;

          // Stop sounds
          AHI_ControlAudio(actrl,
              AHIC_Play, FALSE,
              TAG_DONE);
   FreeAudio();
    CloseAHI();

}

void pause_audio(int on) {
//	printf("PAUSE audio %d\n",on);
//    SDL_PauseAudio(on);
}
 
