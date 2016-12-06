#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>


#include <exec/exec.h>
#include <dos/dos.h> 
#include <libraries/lowlevel.h>
#include <devices/gameport.h>
#include <devices/timer.h>
#include <devices/keymap.h>
#include <devices/input.h>
#include <devices/inputevent.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <proto/keymap.h>
#include <proto/lowlevel.h>


#include "emu.h"
#include "screen.h"
#include "event.h"
#include "conf.h"
#include "memory.h"


struct Library *LowLevelBase = NULL;


/* gameport stuff */
static struct MsgPort *gameport_mp = NULL;
static struct IOStdReq *gameport_io = NULL;
static BOOL gameport_is_open = FALSE;
static BOOL gameport_io_in_progress = FALSE;
static struct InputEvent gameport_ie;
static BYTE gameport_ct;        /* controller type */
struct GamePortTrigger gameport_gpt = {
  GPTF_UPKEYS | GPTF_DOWNKEYS,    /* gpt_Keys */
  0,                /* gpt_Timeout */
  1,                /* gpt_XDelta */
  1                /* gpt_YDelta */
};



static GNGEO_BUTTON get_mapid(char *butid) {
	printf("Get mapid %s\n",butid);
	if (!strcmp(butid,"A")) return GN_A;
	if (!strcmp(butid,"B")) return GN_B;
	if (!strcmp(butid,"C")) return GN_C;
	if (!strcmp(butid,"D")) return GN_D;

	if (!strcmp(butid,"UP")) return GN_UP;
	if (!strcmp(butid,"DOWN")) return GN_DOWN;
	if (!strcmp(butid,"UPDOWN")) return GN_UP;

	if (!strcmp(butid,"LEFT")) return GN_LEFT;
	if (!strcmp(butid,"RIGHT")) return GN_RIGHT;
	if (!strcmp(butid,"LEFTRIGHT")) return GN_LEFT;

	if (!strcmp(butid,"JOY")) return GN_UP;

	if (!strcmp(butid,"START")) return GN_START;
	if (!strcmp(butid,"COIN")) return GN_SELECT_COIN;

	if (!strcmp(butid,"MENU")) return GN_MENU_KEY;

	if (!strcmp(butid,"HOTKEY1")) return GN_HOTKEY1;
	if (!strcmp(butid,"HOTKEY2")) return GN_HOTKEY2;
	if (!strcmp(butid,"HOTKEY3")) return GN_HOTKEY3;
	if (!strcmp(butid,"HOTKEY4")) return GN_HOTKEY4;

	return GN_NONE;
}

bool create_joymap_from_string(int player,char *jconf) {

 
	return true;
}

extern int neo_emu_done;

bool init_event(void) {
	int i;

     
    if(!LowLevelBase) LowLevelBase = (struct Library *) OpenLibrary((UBYTE *)"lowlevel.library",0);
         
     



	jmap=calloc(sizeof(JOYMAP),1);

#ifdef __AMIGA__
	conf.nb_joy = 1;
#else	
	conf.nb_joy = SDL_NumJoysticks();
#endif
 
	create_joymap_from_string(1,CF_STR(cf_get_item_by_name("p1control")));
	create_joymap_from_string(2,CF_STR(cf_get_item_by_name("p2control")));
	
	jmap->jbutton=calloc(conf.nb_joy,sizeof(struct BUT_MAP*));
	jmap->jaxe=   calloc(conf.nb_joy,sizeof(struct BUT_MAPJAXIS*));
	jmap->jhat=   calloc(conf.nb_joy,sizeof(struct BUT_MAP*));
	 
	return true;
}

#ifdef GP2X
int handle_pdep_event(SDL_Event *event) {
	static int snd_volume=75;
	char volbuf[21];
	int i;
	switch (event->type) {
	case SDL_JOYBUTTONDOWN:
		//printf("Event %d %d\n",event->jbutton.which,event->jbutton.button);
		if (event->jbutton.which==0) {
			if (event->jbutton.button==GP2X_VOL_UP && conf.sound) {
				if (snd_volume<100) snd_volume+=5; else snd_volume=100;
				gp2x_sound_volume_set(snd_volume,snd_volume);
				for (i=0;i<snd_volume/5;i++) volbuf[i]='|';
				for (i=snd_volume/5;i<20;i++) volbuf[i]='-';
				volbuf[20]=0;
				draw_message(volbuf);
			}
			if (event->jbutton.button==GP2X_VOL_DOWN && conf.sound) {
				if (snd_volume>0) snd_volume-=5; else snd_volume=0;
				gp2x_sound_volume_set(snd_volume,snd_volume);
				for (i=0;i<snd_volume/5;i++) volbuf[i]='|';
				for (i=snd_volume/5;i<20;i++) volbuf[i]='-';
				volbuf[20]=0;
				draw_message(volbuf);
			}
		}
		break;
	}
	return 0;
}
#elif DEVKIT8000_
int handle_pdep_event(SDL_Event *event) {
	switch (event->type) {
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			printf("MouseDown %d %d %d\n",event->button.state,event->button.x,event->button.y);
			break;
	}
}
#else /* Default */
int handle_pdep_event(void *event) {
 
	return 0;
}
#endif

#define EVGAME 1
#define EVMENU 2

int handle_event(void) {
    
    static ULONG previous = 0;

  /* CD32 joypad handler code supplied by Gabry (ggreco@iol.it) */

 
  if (LowLevelBase != NULL) {

       
    ULONG joypos = ReadJoyPort (1);
 
 
    if (joypos & JPF_BUTTON_RED)
        joy_state[0][GN_A] =1;
    else
        joy_state[0][GN_A] =0;                
         

    
    if (joypos & JP_DIRECTION_MASK) {
      if (joypos & JPF_JOY_LEFT) {
        joy_state[0][GN_LEFT] =1;   
      }
      else
      {
        joy_state[0][GN_LEFT] =0;         
      } 
            
            
      if (joypos & JPF_JOY_RIGHT) {
        joy_state[0][GN_RIGHT] =1;                     
      }
      else
      {
        joy_state[0][GN_RIGHT] =0;                                
      }
        
      if (joypos & JPF_JOY_UP) {
        joy_state[0][GN_UP] =1;               
      }
      else
      {
        joy_state[0][GN_UP] =0;                           
      } 
      
      if (joypos & JPF_JOY_DOWN) {
        joy_state[0][GN_DOWN] =1;         
      }
      else
      {
        joy_state[0][GN_DOWN] =0;                     
      }
    }

    if (joypos & JP_TYPE_GAMECTLR) {
      

      // Play/Pause = ESC (Menu)
      if (joypos & JPF_BUTTON_PLAY) {
        joy_state[0][GN_START] = 1;        
      } else  {
        joy_state[0][GN_START] = 0;
      }

      // YELLOW = SHIFT (button 2) (Run)
      if (joypos & JPF_BUTTON_YELLOW)
        joy_state[0][GN_B] = 1;        
      else
        joy_state[0][GN_B] = 0;        

      // BLUE = SPACE (button 3) (Open/Operate)

      if (joypos & JPF_BUTTON_BLUE)
        joy_state[0][GN_D] = 1;        
      else
        joy_state[0][GN_D] = 1;        

      // GREEN = RETURN (show msg)

      if (joypos & JPF_BUTTON_GREEN ) {
        joy_state[0][GN_C] = 1;        
      } else {
        joy_state[0][GN_C] = 1;        
      }

      // FORWARD & REVERSE - ALT (Button1) Strafe left/right

      if (joypos & JPF_BUTTON_FORWARD) {
        joy_state[0][GN_SELECT_COIN] = 1;
      } else if (joypos & JPF_BUTTON_REVERSE) {
        joy_state[0][GN_SELECT_COIN] = 1;
      } else
        joy_state[0][GN_SELECT_COIN] = 0;
    }

   
    
  }

	/* Update coin data */
	memory.intern_coin = 0x7;
	if (joy_state[0][GN_SELECT_COIN])
		memory.intern_coin &= 0x6;
	if (joy_state[1][GN_SELECT_COIN])
		memory.intern_coin &= 0x5;
	/* Update start data TODO: Select */
	memory.intern_start = 0x8F;
	if (joy_state[0][GN_START])
		memory.intern_start &= 0xFE;
	if (joy_state[1][GN_START])
		memory.intern_start &= 0xFB;

	/* Update P1 */
	memory.intern_p1 = 0xFF;
	if (joy_state[0][GN_UP] && (!joy_state[0][GN_DOWN]))
	    memory.intern_p1 &= 0xFE;
	if (joy_state[0][GN_DOWN] && (!joy_state[0][GN_UP]))
	    memory.intern_p1 &= 0xFD;
	if (joy_state[0][GN_LEFT] && (!joy_state[0][GN_RIGHT]))
	    memory.intern_p1 &= 0xFB;
	if (joy_state[0][GN_RIGHT] && (!joy_state[0][GN_LEFT]))
	    memory.intern_p1 &= 0xF7;
	if (joy_state[0][GN_A])
	    memory.intern_p1 &= 0xEF;	// A
	if (joy_state[0][GN_B])
	    memory.intern_p1 &= 0xDF;	// B
	if (joy_state[0][GN_C])
	    memory.intern_p1 &= 0xBF;	// C
	if (joy_state[0][GN_D])
	    memory.intern_p1 &= 0x7F;	// D

	/* Update P1 */
	memory.intern_p2 = 0xFF;
	if (joy_state[1][GN_UP] && (!joy_state[1][GN_DOWN]))
	    memory.intern_p2 &= 0xFE;
	if (joy_state[1][GN_DOWN] && (!joy_state[1][GN_UP]))
	    memory.intern_p2 &= 0xFD;
	if (joy_state[1][GN_LEFT] && (!joy_state[1][GN_RIGHT]))
	    memory.intern_p2 &= 0xFB;
	if (joy_state[1][GN_RIGHT] && (!joy_state[1][GN_LEFT]))
	    memory.intern_p2 &= 0xF7;
	if (joy_state[1][GN_A])
	    memory.intern_p2 &= 0xEF;	// A
	if (joy_state[1][GN_B])
	    memory.intern_p2 &= 0xDF;	// B
	if (joy_state[1][GN_C])
	    memory.intern_p2 &= 0xBF;	// C
	if (joy_state[1][GN_D])
	    memory.intern_p2 &= 0x7F;	// D

 
	if  (joy_state[0][GN_START] || joy_state[0][GN_SELECT_COIN])
		return 1;
 
    
    return 0; 
}

/*
int handle_event(void) {
	return handle_event_inter(EVGAME);
}
*/
static int last=-1;
static int counter=40;

void reset_event(void) {
 
    int i;
	last=-1;
	
    
	for (i = 0; i < GN_MAX_KEY; i++)
		joy_state[0][i] = 0;
        	
	counter=40;
}

int wait_event(void) {
 
	return 0;
}
