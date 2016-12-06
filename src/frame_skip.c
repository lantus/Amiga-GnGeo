/*  gngeo, a neogeo emulator
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <devices/timer.h>
#include <proto/exec.h>

#include "frame_skip.h"
#include "messages.h"
#include "emu.h"
#include "conf.h"


#ifndef uclock_t
#define uclock_t Uint32
#endif

#define TICKS_PER_SEC 1000000UL
//#define CPU_FPS 60
//static int CPU_FPS=60;
static uclock_t F;

#define MAX_FRAMESKIP 10


static char init_frame_skip = 1;
char skip_next_frame = 0;
#if defined(HAVE_GETTIMEOFDAY) && !defined(WII)
static int CPU_FPS=60;
static struct timeval init_tv = { 0, 0 };
#else
/* Looks like SDL_GetTicks is not as precise... */
static int CPU_FPS=61;
static Uint32 init_tv=0;
#endif
uclock_t bench;

#if defined(HAVE_GETTIMEOFDAY) && !defined(WII)
uclock_t get_ticks(void)
{
    struct timeval tv;

    gettimeofday(&tv, 0);
    if (init_tv.tv_sec == 0)
	init_tv = tv;
    return (tv.tv_sec - init_tv.tv_sec) * TICKS_PER_SEC + tv.tv_usec -
	init_tv.tv_usec;


}
#else

 

Uint32 get_ticks(void)
{
    return I_GetTimeMS();
}    
#endif

void reset_frame_skip(void)
{
#if defined(HAVE_GETTIMEOFDAY) && !defined(WII)
    init_tv.tv_usec = 0;
    init_tv.tv_sec = 0;
#else
    init_tv=0;
#endif
    skip_next_frame = 0;
    init_frame_skip = 1;
    if (conf.pal)
	CPU_FPS=50;
    F = (uclock_t) ((double) TICKS_PER_SEC / CPU_FPS);
}



int frame_skip(int init)
{
    static int f2skip;
    static uclock_t sec = 0;
    static uclock_t rfd;
    static uclock_t target;
    static int nbFrame = 0;
    static unsigned int nbFrame_moy = 0;
    static int nbFrame_min = 1000;
    static int nbFrame_max = 0;
    static int skpFrm = 0;
    static int count = 0;
    static int moy=60;

    if (init_frame_skip) {
		init_frame_skip = 0;
		target = get_ticks();
		bench = (CF_BOOL(cf_get_item_by_name("bench")) ? 1/*get_ticks()*/ : 0);

		nbFrame = 0;
		//f2skip=0;
		//skpFrm=0;
		sec = 0;
		return 0;
    }

    target += F;
    if (f2skip > 0 ) {
	f2skip--;
	skpFrm++;
	return 1;
    } else
	skpFrm = 0;
//	printf("%d %d\n",conf.autoframeskip,conf.show_fps);

    rfd = get_ticks();

    if (conf.autoframeskip) {
	if (rfd < target && f2skip == 0)
	    while (get_ticks() < target) {
#ifndef WIN32
		if (conf.sleep_idle) {
		    usleep(5);
		}
#endif
	} else {
	    f2skip = (rfd - target) / (double) F;
	    if (f2skip > MAX_FRAMESKIP) {
		f2skip = MAX_FRAMESKIP;
		reset_frame_skip();
	    }
	    // printf("Skip %d frame(s) %lu %lu\n",f2skip,target,rfd);
	}
    }

    nbFrame++;
    nbFrame_moy++;
 
    if (conf.show_fps) {
	    if (get_ticks() - sec >= TICKS_PER_SEC) {
		    //printf("%d\n",nbFrame);
		    if (bench) {

			    if (nbFrame_min>nbFrame) nbFrame_min=nbFrame;
			    if (nbFrame_max<nbFrame) nbFrame_max=nbFrame;
			    count++;
			    moy=nbFrame_moy/(float)count;

			    if (count==30) count=0;
			    sprintf(fps_str, "%d %d %d %d\n", nbFrame-1,nbFrame_min-1,nbFrame_max-1,moy-1);
		    } else {
			    sprintf(fps_str, "%2d", nbFrame-1);
		    }
	    
		    nbFrame = 0;
		    sec = get_ticks();
	    }
    }
    return 0;
}


static ULONG basetime = 0;
struct MsgPort *timer_msgport;
struct timerequest *timer_ioreq;
struct Library *TimerBase;

static int opentimer(ULONG unit){
	timer_msgport = CreateMsgPort();
	timer_ioreq = CreateIORequest(timer_msgport, sizeof(*timer_ioreq));
	if (timer_ioreq){
		if (OpenDevice(TIMERNAME, unit, (APTR) timer_ioreq, 0) == 0){
			TimerBase = (APTR) timer_ioreq->tr_node.io_Device;
			return 1;
		}
	}
	return 0;
}
static void closetimer(void){
	if (TimerBase){
		CloseDevice((APTR) timer_ioreq);
	}
	DeleteIORequest(timer_ioreq);
	DeleteMsgPort(timer_msgport);
	TimerBase = 0;
	timer_ioreq = 0;
	timer_msgport = 0;
}

static struct timeval startTime;

void startup(){
	GetSysTime(&startTime);
}

ULONG getMilliseconds(){
	struct timeval endTime;

	GetSysTime(&endTime);
	SubTime(&endTime,&startTime);

	return (endTime.tv_secs * 1000 + endTime.tv_micro / 1000);
}

int  I_GetTime (void)
{
    ULONG ticks;

    ticks = getMilliseconds();

    if (basetime == 0)
        basetime = ticks;

    ticks -= basetime;

    return (ticks * TICKS_PER_SEC) / 1000;
} 
//
// Same as I_GetTime, but returns time in milliseconds
//

int I_GetTimeMS(void)
{
    ULONG ticks;

    ticks = getMilliseconds();

    if (basetime == 0)
        basetime = ticks;

    return (ticks - basetime) * 1000;
}

// Sleep for a specified number of ms


void I_ExitTimer()
{
    closetimer();
}

void I_Sleep(int ms)
{
    usleep(ms);
}

void I_WaitVBL(int count)
{
    I_Sleep((count * 1000) / 70);
}


void I_InitTimer(void)
{
    // initialize timer

   opentimer(UNIT_VBLANK);
   startup();
}
