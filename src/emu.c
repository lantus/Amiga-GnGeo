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
#include <stdlib.h>
#include <time.h>

#include "emu.h"
#include "memory.h"
#include "frame_skip.h"
#include "pd4990a.h"
#include "messages.h"
#include "profiler.h"
#include "debug.h"

#include "timer.h"
//#include "streams.h"
#include "ym2610/2610intf.h"
#include "sound.h"
#include "screen.h"
#include "neocrypt.h"
#include "conf.h"
//#include "driver.h"
//#include "gui_interf.h"
#ifdef FULL_GL
#include "videogl.h"
#endif
#ifdef GP2X
#include "gp2x.h"

#include "ym2610-940/940shared.h"
#include "ym2610-940/940private.h"
#endif
#include "menu.h"
#include "event.h"

int frame;
int nb_interlace = 256;
int current_line;
static int arcade;

extern int irq2enable, irq2start, irq2repeat, irq2control, irq2taken;
extern int lastirq2line;
extern int irq2repeat_limit;
extern Uint32 irq2pos_value;

inline short SwapSHORT(short val)
{
	__asm __volatile
	(
		"ror.w	#8,%0"

		: "=d" (val)
		: "0" (val)
		);

	return val;
}

inline long SwapLONG(long val)
{
	__asm __volatile
	(
		"ror.w	#8,%0 \n\t"
		"swap	%0 \n\t"
		"ror.w	#8,%0"

		: "=d" (val)
		: "0" (val)
		);

	return val;
}

void setup_misc_patch(char *name) {


	if (!strcmp(name, "ssideki")) {
		WRITE_WORD_ROM(&memory.rom.cpu_m68k.p[0x2240], 0x4e71);
	}

	//if (!strcmp(name, "fatfury3")) {
	//	WRITE_WORD_ROM(memory.rom.cpu_m68k.p, 0x0010);
	//}

	if (!strcmp(name, "mslugx")) {
		/* patch out protection checks */
		int i;
		Uint8 *RAM = memory.rom.cpu_m68k.p;
		for (i = 0; i < memory.rom.cpu_m68k.size; i += 2) {
			if ((READ_WORD_ROM(&RAM[i + 0]) == 0x0243)
					&& (READ_WORD_ROM(&RAM[i + 2]) == 0x0001) && /* andi.w  #$1, D3 */
			(READ_WORD_ROM(&RAM[i + 4]) == 0x6600)) { /* bne xxxx */

				WRITE_WORD_ROM(&RAM[i + 4], 0x4e71);
				WRITE_WORD_ROM(&RAM[i + 6], 0x4e71);
			}
		}

		WRITE_WORD_ROM(&RAM[0x3bdc], 0x4e71);
		WRITE_WORD_ROM(&RAM[0x3bde], 0x4e71);
		WRITE_WORD_ROM(&RAM[0x3be0], 0x4e71);
		WRITE_WORD_ROM(&RAM[0x3c0c], 0x4e71);
		WRITE_WORD_ROM(&RAM[0x3c0e], 0x4e71);
		WRITE_WORD_ROM(&RAM[0x3c10], 0x4e71);

		WRITE_WORD_ROM(&RAM[0x3c36], 0x4e71);
		WRITE_WORD_ROM(&RAM[0x3c38], 0x4e71);
	}

}

void neogeo_reset(void) {
    //	memory.vid.modulo = 1; /* TODO: Move to init_video */
	sram_lock = 0;
	sound_code = 0;
	pending_command = 0;
	result_code = 0;
#ifdef ENABLE_940T
	shared_ctl->sound_code = sound_code;
	shared_ctl->pending_command = pending_command;
	shared_ctl->result_code = result_code;
#endif
	if (memory.rom.cpu_m68k.size > 0x100000)
		cpu_68k_bankswitch(0x100000);
	else
		cpu_68k_bankswitch(0);
	cpu_68k_reset();

}

void init_sound(void) {

	if (conf.sound) init_sdl_audio();

#ifdef ENABLE_940T
		printf("Init all neo");
		shared_data->sample_rate = conf.sample_rate;
		shared_data->z80_cycle = (z80_overclk == 0 ? 73333 : 73333
				+ (z80_overclk * 73333 / 100.0));
		//gp2x_add_job940(JOB940_INITALL);
		gp2x_add_job940(JOB940_INITALL);
		wait_busy_940(JOB940_INITALL);
		printf("The YM2610 have been initialized\n");
#else
        printf("cpu_z80_init()\n");
		cpu_z80_init();
		//streams_sh_start();
        printf("YM2610_sh_start()\n");		
		YM2610_sh_start();
#endif
	if (conf.sound)	pause_audio(0);
		conf.snd_st_reg_create = 1;
 
}

void init_neo(void) {
#ifdef ENABLE_940T
	int z80_overclk = CF_VAL(cf_get_item_by_name("z80clock"));
#endif

	neogeo_init_save_state();

#ifdef GP2X
	gp2x_ram_ptr_reset();
#endif

	cpu_68k_init();
//	neogeo_reset();
	pd4990a_init();
//	setup_misc_patch(rom_name);

	init_sound();

	neogeo_reset();
}

static void take_screenshot(void) {
 
}

static int fc;
static int last_line;
static int skip_this_frame = 0;

static inline int neo_interrupt(void) {
    static int frames;

	pd4990a_addretrace();
	// printf("neogeo_frame_counter_speed %d\n",neogeo_frame_counter_speed);
	if (!(memory.vid.irq2control & 0x8)) {
		if (fc >= neogeo_frame_counter_speed) {
			fc = 0;
			neogeo_frame_counter++;
		}
		fc++;
	}

	skip_this_frame = skip_next_frame;
	skip_next_frame = frame_skip(0);

	if (!skip_this_frame) {
		PROFILER_START(PROF_VIDEO);

		draw_screen();

		PROFILER_STOP(PROF_VIDEO);
	}
    /*
    frames++;
    printf("FRAME %d\n",frames);
    if (frames==262) {
        FILE *f;
        sleep(1);
        f=fopen("/tmp/video.dmp","wb");
        fwrite(&memory.vid.ram,0x20000,1,f);
        fclose(f);
    }
    */
	return 1;
}

static inline void update_screen(void) {

 
	if (memory.vid.irq2control & 0x40)
		memory.vid.irq2start = (memory.vid.irq2pos + 3) / 0x180; /* ridhero gives 0x17d */
	else
		memory.vid.irq2start = 1000;

	if (!skip_this_frame) {
		if (last_line < 21) { /* there was no IRQ2 while the beam was in the
							 * visible area -> no need for scanline rendering */
			draw_screen();
		} else {
			draw_screen_scanline(last_line - 21, 262, 1);
		}
	}

	last_line = 0;

	pd4990a_addretrace();
	if (fc >= neogeo_frame_counter_speed) {
		fc = 0;
		neogeo_frame_counter++;
	}
	fc++;

	skip_this_frame = skip_next_frame;
	skip_next_frame = frame_skip(0);
}

static inline int update_scanline(void) {
	memory.vid.irq2taken = 0;

	if (memory.vid.irq2control & 0x10) {

		if (current_line == memory.vid.irq2start) {
			if (memory.vid.irq2control & 0x80)
				memory.vid.irq2start += (memory.vid.irq2pos + 3) / 0x180;
			memory.vid.irq2taken = 1;
		}
	}

	if (memory.vid.irq2taken) {
		if (!skip_this_frame) {
			if (last_line < 21)
				last_line = 21;
			if (current_line < 20)
				current_line = 20;
			draw_screen_scanline(last_line - 21, current_line - 20, 0);
		}
		last_line = current_line;
	}
	current_line++;
	return memory.vid.irq2taken;
}

static Uint16 pending_save_state = 0, pending_load_state = 0;
static int slow_motion = 0;

static inline void state_handling(int save,int load) {
	if (save) {
		//if (conf.sound) SDL_LockAudio();
		save_state(conf.game, save - 1);
		//if (conf.sound) SDL_UnlockAudio();
		reset_frame_skip();
	}
	if (load) {
		//if (conf.sound) SDL_LockAudio();
		load_state(conf.game, load - 1);
		//if (conf.sound) SDL_UnlockAudio();
		reset_frame_skip();
	}
	pending_load_state = pending_save_state = 0;
}

void main_loop(void) {
	int neo_emu_done = 0;
	int m68k_overclk = CF_VAL(cf_get_item_by_name("68kclock"));
	int z80_overclk = CF_VAL(cf_get_item_by_name("z80clock"));
	//int nb_frames = 0;
	int a,i;
#ifdef GP2X
 
	int snd_volume = 60;
	char volbuf[21];

	FILE *sndbuf;
	unsigned int sample_len = conf.sample_rate / 60.0;
	static unsigned int gp2x_timer;
	static unsigned int gp2x_timer_prev;
#endif
 
	Uint32 cpu_68k_timeslice = (m68k_overclk == 0 ? 200000 : 200000
			+ (m68k_overclk * 200000 / 100.0));
	Uint32 cpu_68k_timeslice_scanline = cpu_68k_timeslice / 264.0;
	Uint32 cpu_z80_timeslice = (z80_overclk == 0 ? 73333 : 73333 + (z80_overclk
			* 73333 / 100.0));
	Uint32 tm_cycle = 0;

	Uint32 cpu_z80_timeslice_interlace = cpu_z80_timeslice
			/ (float) nb_interlace;

#ifdef GP2X
	gp2x_sound_volume_set(snd_volume, snd_volume);
#endif

	reset_frame_skip();
	my_timer();

	//pause_audio(0);
	while (!neo_emu_done) {
        
		if (conf.test_switch == 1)
			conf.test_switch = 0;

		//neo_emu_done=
		if (handle_event()) {
			int interp = interpolation;
//			SDL_BlitSurface(buffer, &buf_rect, state_img, &screen_rect);
			interpolation = 0;
			if (conf.sound) pause_audio(1);
		//	if (run_menu() == 2) {
		//		neo_emu_done = 1;/*printf("Unlock audio\n");SDL_UnlockAudio()*/
		//		return;
		//	} // A bit ugly...
			if (conf.sound) pause_audio(0);
			//neo_emu_done = 1;
			interpolation = interp;
			reset_frame_skip();
			reset_event();
		}

        

//		if (slow_motion)
//			SDL_Delay(100);

#ifndef ENABLE_940T
		if (conf.sound) {
			PROFILER_START(PROF_Z80);

			for (i = 0; i < nb_interlace; i++) {
				cpu_z80_run(cpu_z80_timeslice_interlace);
				my_timer();
			}

			//cpu_z80_run(cpu_z80_timeslice);
			PROFILER_STOP(PROF_Z80);
		} /*
		 else
		 my_timer();*/
#endif
#ifdef ENABLE_940T
		if (conf.sound) {
			PROFILER_START(PROF_Z80);
			wait_busy_940(JOB940_RUN_Z80);
#if 0
			if (gp2x_timer) {
				gp2x_timer_prev=gp2x_timer;
				gp2x_timer=gp2x_memregl[0x0A00>>2];
				shared_ctl->sample_todo=(unsigned int)(((gp2x_timer-gp2x_timer_prev)*conf.sample_rate)/7372800.0);
			} else {
				gp2x_timer=gp2x_memregl[0x0A00>>2];
				shared_ctl->sample_todo=sample_len;
			}
#endif
			gp2x_add_job940(JOB940_RUN_Z80);
			PROFILER_STOP(PROF_Z80);
		}

#endif

		if (!conf.debug) {
			if (conf.raster) {
				current_line = 0;
				for (i = 0; i < 264; i++) {
					tm_cycle = cpu_68k_run(cpu_68k_timeslice_scanline
							- tm_cycle);
					if (update_scanline())
						cpu_68k_interrupt(2);
				}
				tm_cycle = cpu_68k_run(cpu_68k_timeslice_scanline - tm_cycle);
				//state_handling(pending_save_state, pending_load_state); 
				update_screen();				        
				memory.watchdog++;
				if (memory.watchdog > 7) {
                    printf("WATCHDOG RESET\n");
					cpu_68k_reset();
                }
				cpu_68k_interrupt(1);
			} else {
				PROFILER_START(PROF_68K);
				tm_cycle = cpu_68k_run(cpu_68k_timeslice - tm_cycle);
				PROFILER_STOP(PROF_68K);
				a = neo_interrupt();

				/* state handling (we save/load before interrupt) */
				//state_handling(pending_save_state, pending_load_state);

				memory.watchdog++;

				if (memory.watchdog > 7) { /* Watchdog reset after ~0.13 == ~7.8 frames */
                    printf("WATCHDOG RESET %d\n",memory.watchdog);
					cpu_68k_reset();
                }

				if (a) {
					cpu_68k_interrupt(a);
				}
			}
		} else {
			/* we are in debug mode -> we are just here for event handling */
			neo_emu_done = 1;
		}

#ifdef ENABLE_PROFILER
		profiler_show_stat();
#endif
		PROFILER_START(PROF_ALL);
	}
	pause_audio(1);
#ifdef ENABLE_940T
	wait_busy_940(JOB940_RUN_Z80);
	wait_busy_940(JOB940_RUN_Z80_NMI);
	shared_ctl->z80_run = 0;
#endif

}

void cpu_68k_dpg_step(void) {
	static Uint32 nb_cycle;
	static Uint32 line_cycle;
	Uint32 cpu_68k_timeslice = 200000;
	Uint32 cpu_68k_timeslice_scanline = 200000 / (float) 262;
	Uint32 cycle;
	if (nb_cycle == 0) {
		main_loop(); /* update event etc. */
	}
	cycle = cpu_68k_run_step();
	add_bt(cpu_68k_getpc());
	line_cycle += cycle;
	nb_cycle += cycle;
	if (nb_cycle >= cpu_68k_timeslice) {
		nb_cycle = line_cycle = 0;
		if (conf.raster) {
			update_screen();
		} else {
			neo_interrupt();
		}
		//state_handling(pending_save_state, pending_load_state);
		cpu_68k_interrupt(1);
	} else {
		if (line_cycle >= cpu_68k_timeslice_scanline) {
			line_cycle = 0;
			if (conf.raster) {
				if (update_scanline())
					cpu_68k_interrupt(2);
			}
		}
	}
}

void debug_loop(void) {
	int a;
	do {
	  a = cpu_68k_debuger(cpu_68k_dpg_step, /*dump_hardware_reg*/NULL);
	} while (a != -1);
}
