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
#include <dos/dostags.h>
#include <hardware/cia.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/ahi.h>
#include <proto/graphics.h>
 
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

#define NB_SAMPLES 128 /* better resolution */ 
Uint8 * stream = NULL;
 
 

static void sub_invoc(void);	// Sound sub-process
void sub_func(void);
struct Process *sound_process;
int quit_sig, pause_sig,
	resume_sig, ahi_sig;		// Sub-process signals
struct Task *main_task;			// Main task
int main_sig;					// Main task signals
static ULONG sound_func(void);	// AHI callback
struct MsgPort *ahi_port;		// Port and IORequest for AHI
struct AHIRequest *ahi_io;
struct AHIAudioCtrl *ahi_ctrl;	// AHI control structure
struct AHISampleInfo sample;	// SampleInfos for double buffering
struct Hook sf_hook;			// Hook for callback function
int play_buf;					// Number of buffer currently playing
bool ready;                     // Is the audio ready?
	
// Library bases
struct Library *AHIBase;

// CIA-A base
extern struct CIA ciaa; 

int init_audio(void)
{
	// Find our (main) task
	main_task = FindTask(NULL);

	// Create signal for communication
	main_sig = AllocSignal(-1);

	// Create sub-process and wait until it is ready
	if ((sound_process = CreateNewProcTags(
		NP_Entry, (ULONG)&sub_invoc,
		NP_Name, (ULONG)"NeoSoundProcess",
		NP_Priority, 1, 
		TAG_DONE)) != NULL)
		Wait(1 << main_sig); 
}

void close_audio(void) {

	// Tell sub-process to quit and wait for completion
	if (sound_process != NULL) {
		Signal(&(sound_process->pr_Task), 1 << quit_sig);
		Wait(1 << main_sig);
	}

	// Free signal
	FreeSignal(main_sig); 

}


void sub_invoc(void)
{
	// Get pointer to the DigitalRenderer object and call sub_func()
    sub_func();
}

void pause_audio(int on) {
 
}

ULONG sound_func(void)
{
	register struct AHIAudioCtrl *ahi_ctrl asm ("a2");	 
    YM2610Update_stream(NB_SAMPLES);	
	AHI_SetSound(0, play_buf, 0, 0, ahi_ctrl, 0);
	Signal(&(sound_process->pr_Task), 1 << (ahi_sig));
	return 0;
}
 
void sub_func(void)
{
	ahi_port = NULL;
	ahi_io = NULL;
	ahi_ctrl = NULL;
	sample.ahisi_Address = NULL;
	ready = FALSE;

	// Create signals for communication
	quit_sig = AllocSignal(-1);
	pause_sig = AllocSignal(-1);
	resume_sig = AllocSignal(-1);
	ahi_sig = AllocSignal(-1);

	// Open AHI
	if ((ahi_port = CreateMsgPort()) == NULL)
		goto wait_for_quit;
	if ((ahi_io = (struct AHIRequest *)CreateIORequest(ahi_port, sizeof(struct AHIRequest))) == NULL)
		goto wait_for_quit;
	ahi_io->ahir_Version = 2;
	if (OpenDevice(AHINAME, AHI_NO_UNIT, (struct IORequest *)ahi_io, NULL))
		goto wait_for_quit;
	AHIBase = (struct Library *)ahi_io->ahir_Std.io_Device;

	// Initialize callback hook
	sf_hook.h_Entry = sound_func;

	// Open audio control structure
	if ((ahi_ctrl = AHI_AllocAudio(
		AHIA_AudioID, 0x0002000b,
		AHIA_MixFreq, conf.sample_rate,
		AHIA_Channels, 1,
		AHIA_Sounds, 1,
		AHIA_SoundFunc, (ULONG)&sf_hook,
		TAG_DONE)) == NULL)
		goto wait_for_quit;


    // 32 bits (4 bytes) are required per sample for storage (16bit stereo).
    ULONG sampleBufferSize = (NB_SAMPLES * AHI_SampleFrameSize(AHIST_M16S));
  
	// Prepare SampleInfos and load sounds (two sounds for double buffering)
	sample.ahisi_Type = AHIST_M16S;
	sample.ahisi_Length = sampleBufferSize;
	sample.ahisi_Address = AllocVec(sampleBufferSize, MEMF_PUBLIC | MEMF_CLEAR);
 
	if (sample.ahisi_Address == NULL)
		goto wait_for_quit;
	AHI_LoadSound(0, AHIST_DYNAMICSAMPLE, &sample, ahi_ctrl);
 
	// Set parameters
	play_buf = 0;
	AHI_SetVol(0, 0x10000, 0x8000, ahi_ctrl, AHISF_IMM);
	AHI_SetFreq(0, conf.sample_rate << 1, ahi_ctrl, AHISF_IMM);
	AHI_SetSound(0, play_buf, 0, 0, ahi_ctrl, AHISF_IMM);

	// Start audio output
	AHI_ControlAudio(ahi_ctrl, AHIC_Play, TRUE, TAG_DONE);

	// We are now ready for commands
	ready = TRUE;
	Signal(main_task, 1 << main_sig);

	// Accept and execute commands
	for (;;) {
		ULONG sigs = Wait((1 << quit_sig) | (1 << pause_sig) | (1 << resume_sig) | (1 << ahi_sig));

		// Quit sub-process
		if (sigs & (1 << quit_sig))
			goto quit;

		// Pause sound output
		if (sigs & (1 << pause_sig))
			AHI_ControlAudio(ahi_ctrl, AHIC_Play, FALSE, TAG_DONE);

		// Resume sound output
		if (sigs & (1 << resume_sig))
			AHI_ControlAudio(ahi_ctrl, AHIC_Play, TRUE, TAG_DONE);

		// Calculate next buffer
		if (sigs & (1 << ahi_sig))
		{
			memcpy(sample.ahisi_Address, (Uint8 *)play_buffer, NB_SAMPLES << 2);
        }
	}

wait_for_quit:
	// Initialization failed, wait for quit signal
	Wait(1 << quit_sig);

quit:
	// Free everything
	if (ahi_ctrl != NULL) {
		AHI_ControlAudio(ahi_ctrl, AHIC_Play, FALSE, TAG_DONE);
		AHI_FreeAudio(ahi_ctrl);
		CloseDevice((struct IORequest *)ahi_io);
	}

	FreeVec(sample.ahisi_Address);
 

	if (ahi_io != NULL)
		DeleteIORequest((struct IORequest *)ahi_io);

	if (ahi_port != NULL)
		DeleteMsgPort(ahi_port);

	FreeSignal(quit_sig);
	FreeSignal(pause_sig);
	FreeSignal(resume_sig);
	FreeSignal(ahi_sig);

	// Quit (synchronized with main task)
	Forbid();
	Signal(main_task, 1 << main_sig);
}
