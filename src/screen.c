
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <exec/exec.h>
#include <dos/dos.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxmacros.h>
#include <intuition/intuition.h>
#include <libraries/asl.h>
#include <libraries/lowlevel.h>
#include <devices/gameport.h>
#include <devices/timer.h>
#include <devices/keymap.h>
#include <devices/input.h>


#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <proto/intuition.h>
#include <proto/asl.h>
#include <proto/keymap.h>
#include <proto/lowlevel.h>

#include "emu.h"
#include "screen.h"
#include "video.h"
#include "conf.h"

#include "blitter.h"
#include "effect.h"



int effect_none_init(void);

int effect_smooth_init(void);

blitter_func blitter[] = {
	{"soft", "Software blitter", blitter_soft_init, NULL, blitter_soft_update, blitter_soft_fullscreen,
		blitter_soft_close},

	{NULL, NULL, NULL, NULL, NULL, NULL, NULL}
};

effect_func effect[] = {

	{"none", "No effect", 1, 1, effect_none_init, NULL},
 
	{NULL, NULL, 0, 0, NULL, NULL}
};

 

RGB2YUV rgb2yuv[65536];

void
init_rgb2yuv_table(void) {
	static char init = 0;
	Uint32 i;
	Uint8 y, u, v, r, g, b;
	if (init == 0) {
		init = 1;
		for (i = 0; i <= 65535; i++) {
			r = ((i & 0xF800) >> 11) << 3;
			g = ((i & 0x7E0) >> 5) << 2;
			b = (i & 0x1F) << 3;

			y = (0.257 * r) + (0.504 * g) + (0.098 * b) + 16;
			u = (0.439 * r) - (0.368 * g) - (0.071 * b) + 128;
			v = -(0.148 * r) - (0.291 * g) + (0.439 * b) + 128;

			rgb2yuv[i].y = (y << 8) | y;
			rgb2yuv[i].u = u;
			rgb2yuv[i].v = v;
			rgb2yuv[i].yuy2 = (y << 24) | (v << 16) | (y << 8) | u;
		}
	}
}

#ifdef I386_ASM
Uint32 scan_mask = 0xf7def7de;
void do_inner_interpolation_i386(Uint16 * dst, Uint16 * src);
#endif

void print_blitter_list(void) {
	int i = 0;
	while (blitter[i].name != NULL) {
		printf("%-12s : %s\n", blitter[i].name, blitter[i].desc);
		i++;
	}
}

void print_effect_list(void) {
	int i = 0;
	while (effect[i].name != NULL) {
		printf("%-12s : %s\n", effect[i].name, effect[i].desc);
		i++;
	}
}

LIST* create_effect_list(void) {
	LIST *el = NULL;
	int i = 0;
	while (effect[i].name != NULL) {
		el = list_append(el, &effect[i]);
		i++;
	}
	return el;
}

LIST* create_blitter_list(void) {
	LIST *bl = NULL;
	int i = 0;
	while (blitter[i].name != NULL) {
		bl = list_append(bl, &blitter[i]);
		i++;
	}
	return bl;
}

Uint8 get_effect_by_name(char *name) {
	int i = 0;

	while (effect[i].name != NULL) {
		if (!strcmp(effect[i].name, name)) {
			return i;
		}
		i++;
	}
	/* invalid effect */
	printf("Invalid effect.\n");
	return 0;
}

Uint8 get_blitter_by_name(char *name) {
	int i = 0;

	while (blitter[i].name != NULL) {
		if (!strcmp(blitter[i].name, name)) {
			return i;
		}
		i++;
	}
	/* invalid blitter */
	printf("Invalid blitter, using soft blitter.\n");
	return 0;
}

int screen_init() {
	CONF_ITEM *cf_blitter, *cf_effect, *cf_interpol, *cf_scale, *cf_fs;

	/* screen configuration init */
	cf_blitter = cf_get_item_by_name("blitter");
	cf_effect = cf_get_item_by_name("effect");
	cf_interpol = cf_get_item_by_name("interpolation");
	cf_scale = cf_get_item_by_name("scale");
	cf_fs = cf_get_item_by_name("fullscreen");

/*
	if (CF_BOOL(cf_get_item_by_name("screen320"))) {
		visible_area.x = 16;
		visible_area.y = 16;
		visible_area.w = 320;
		visible_area.h = 224;
	} else {
		visible_area.x = 24;
		visible_area.y = 16;
		visible_area.w = 304;
		visible_area.h = 224;
	}
*/
		visible_area.x = 16;
		visible_area.y = 16;
		visible_area.w = 320;
		visible_area.h = 224;


	/* Initialization of some variables */
	/*
		interpolation = conf.interpolation;
		nblitter = conf.nblitter;
		neffect = conf.neffect;
	 */
	interpolation = CF_BOOL(cf_interpol);
	nblitter = get_blitter_by_name(CF_STR(cf_blitter));
	neffect = get_effect_by_name(CF_STR(cf_effect));
	fullscreen = CF_BOOL(cf_fs);
	conf.res_x = 304;
	conf.res_y = 224;

	if (CF_VAL(cf_scale) == 0)
		scale = 1;
	else
		scale = CF_VAL(cf_scale);


	/* Init of video blitter */
	if ((*blitter[nblitter].init) () == FALSE)
		return FALSE;

	/* Init of effect */
	//if (neffect > 0)
	if ((*effect[neffect].init) () == FALSE)
		return FALSE;

 
	return TRUE;
}

int effect_none_init(void) {
#ifdef PANDORA
	system("sudo /usr/pandora/scripts/op_videofir.sh none");
#endif
	return TRUE;
}
#ifdef PANDORA
int effect_smooth_init(void) {
	system("sudo /usr/pandora/scripts/op_videofir.sh default");
	return TRUE;
}
#endif
void screen_change_blitter_and_effect(void) {
	CONF_ITEM *cf_blitter, *cf_effect;
/*
			if (bname == NULL) bname = CF_STR(cf_get_item_by_name("blitter"));
			if (ename == NULL) ename = CF_STR(cf_get_item_by_name("effect"));
	 */

	cf_blitter = cf_get_item_by_name("blitter");
	cf_effect = cf_get_item_by_name("effect");

	(*blitter[nblitter].close) ();

	nblitter = get_blitter_by_name(CF_STR(cf_blitter));
	neffect = get_effect_by_name(CF_STR(cf_effect));
//	printf("set %s %s \n", bname, ename);

	//SDL_QuitSubSystem(SDL_INIT_VIDEO);

	if ((*blitter[nblitter].init) () == FALSE) {
		nblitter = 0;
		sprintf(CF_STR(cf_get_item_by_name("blitter")), "soft");
		printf("revert to soft\n");
		if ((*blitter[nblitter].init) () == FALSE )
			exit(-1);
	} /*else
		snprintf(CF_STR(cf_get_item_by_name("blitter")), 255, "%s", bname);
*/
	if ((*effect[neffect].init) () == FALSE) {
		printf("revert to none\n");
		neffect = 0;
		sprintf(CF_STR(cf_get_item_by_name("effect")), "none");
	} /*else
		snprintf(CF_STR(cf_get_item_by_name("effect")), 255, "%s", ename);
*/
//	SDL_InitSubSystem(SDL_INIT_VIDEO);


 
}

int screen_reinit(void) {



/*
	if (CF_BOOL(cf_get_item_by_name("screen320"))) {
		visible_area.x = 16;
		visible_area.y = 16;
		visible_area.w = 320;
		visible_area.h = 224;
	} else {
		visible_area.x = 24;
		visible_area.y = 16;
		visible_area.w = 304;
		visible_area.h = 224;
	}
*/

		visible_area.x = 16;
		visible_area.y = 16;
		visible_area.w = 320;
		visible_area.h = 224;

	/* Initialization of some variables */
	/*
		interpolation = conf.interpolation;
		nblitter = conf.nblitter;
		neffect = conf.neffect;
	 */
	interpolation = CF_BOOL(cf_get_item_by_name("interpolation"));
	fullscreen = CF_BOOL(cf_get_item_by_name("fullscreen"));
	conf.res_x = 304;
	conf.res_y = 224;

	if (CF_VAL(cf_get_item_by_name("scale")) == 0)
		scale = 1;
	else
		scale = CF_VAL(cf_get_item_by_name("scale"));
printf("AA Blitter %s effect %s\n",CF_STR(cf_get_item_by_name("blitter")),CF_STR(cf_get_item_by_name("effect")));
	screen_change_blitter_and_effect();

	return TRUE;
}

int screen_resize(int w, int h) {
 
	return TRUE;
}

static inline void do_interpolation() {
//	Uint16 *dst = (Uint16 *) blend->pixels + 16 + (352 << 4);
//	Uint16 *src = (Uint16 *) buffer->pixels + 16 + (352 << 4);
//	Uint32 s, d;
//	Uint8 w, h;
//#ifdef I386_ASM
//	do_inner_interpolation_i386(dst, src);
//#else
	/* we copy pixels from buffer surface to blend surface */
/*	for (w = 224; w > 0; w--) {
		for (h = 160; h > 0; h--) {
			s = *(Uint32 *) src;
			d = *(Uint32 *) dst;

			*(Uint32 *) dst =
					((d & 0xf7def7de) >> 1) + ((s & 0xf7def7de) >> 1) +
					(s & d & 0x08210821);

			dst += 2;
			src += 2;
		}
		src += 32; //(visible_area.x<<1);
		dst += 32; //(visible_area.x<<1);
	}
#endif
	/* Swap Buffers */
//	tmp = blend;
//	blend = buffer;
//	buffer = tmp;
}

 

void screen_update() {
	if (interpolation == 1)
		do_interpolation();
 
	if (effect[neffect].update != NULL)
		(*effect[neffect].update) ();

	(*blitter[nblitter].update) ();
}

void screen_close() {
//	SDL_FreeSurface(blend);
}

void screen_fullscreen() {
	fullscreen ^= 1;
	blitter[nblitter].fullscreen();
}
