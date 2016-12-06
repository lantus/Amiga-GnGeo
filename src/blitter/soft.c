#include <stdio.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
 
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
 
 
#include <cybergraphx/cybergraphics.h>
#include <proto/cybergraphics.h>

 
#include "../emu.h"
#include "../screen.h"
#include "../video.h"
#include "../effect.h"
#include "../conf.h"
#ifdef GP2X
#include "../gp2x.h"

#define SC_STATUS     0x1802>>1
#define SC_DISP_FIELD (1<<8)

#endif

#ifdef DEVKIT8000
static Rect screen_rect;
#else
static Rect screen_rect;
#endif
 
static int vsync;

 
UBYTE       *bufferpixels = NULL;
byte        *videoBuffer = NULL;
static BOOL initialized = false;
static UWORD emptypointer[] = {
  0x0000, 0x0000,    /* reserved, must be NULL */
  0x0000, 0x0000,     /* 1 row of image data */
  0x0000, 0x0000    /* reserved, must be NULL */
};

/** Hardware window */
struct Window *_hardwareWindow;
/** Hardware screen */
struct Screen *_hardwareScreen;
 
 
static int    firsttime=1;

extern Uint32 *current_pc_pal;
struct RastPort *theRastPort = NULL;
struct RastPort *theTmpRastPort = NULL;
struct BitMap *theBitMap = NULL;
struct BitMap *theTmpBitMap = NULL;
struct Window *theWindow = NULL;

struct Library *CyberGfxBase=0;

void initAmigaGraphics(void)
{



    if (firsttime)
    {
        
  
    uint i = 0;
    ULONG modeId = INVALID_ID;

    if(!CyberGfxBase) CyberGfxBase = (struct Library *) OpenLibrary((UBYTE *)"cybergraphics.library",41L);


    _hardwareWindow = NULL;
 
    _hardwareScreen = NULL;
        firsttime = 0;
        bufferpixels = (unsigned char *)malloc(352*256*2);
 


       modeId = BestCModeIDTags(CYBRBIDTG_NominalWidth, 320,
				      CYBRBIDTG_NominalHeight, 240,
				      CYBRBIDTG_Depth,16,
				      TAG_DONE );


        if(modeId == INVALID_ID) {
          printf("Could not find a valid screen mode");
          exit(-1);
        }
struct Rectangle rect;
    rect.MinX = 16;
    rect.MinY = 16;
    rect.MaxX = 304;
    rect.MaxY = 224;
    
         _hardwareScreen = OpenScreenTags(NULL,
                         SA_Depth, 16,
                         SA_DisplayID, modeId,
                         SA_Width, 320,
                         SA_Height,240,
                         SA_Type, CUSTOMSCREEN,
                         SA_Overscan, OSCAN_TEXT,
                         SA_Quiet,TRUE,
                         SA_ShowTitle, FALSE,
                         SA_Draggable, FALSE,
                         SA_Exclusive, TRUE,
                         SA_AutoScroll, FALSE,
                          SA_DClip,       (ULONG)&rect,
                         TAG_END);
 

        _hardwareWindow = OpenWindowTags(NULL,
                      	    WA_Left, 16,
                			WA_Top, 16,
                			WA_Width, 320,
                			WA_Height, 240,
                			WA_Title, NULL,
        					SA_AutoScroll, FALSE,
                			WA_CustomScreen, (ULONG)_hardwareScreen,
                			WA_Backdrop, TRUE,
                			WA_Borderless, TRUE,
                			WA_DragBar, FALSE,
                			WA_Activate, TRUE,
                			WA_SimpleRefresh, TRUE,
                			WA_NoCareRefresh, TRUE, 
                            WA_IDCMP,           IDCMP_RAWKEY|IDCMP_MOUSEBUTTONS|IDCMP_MOUSEMOVE,    
                            WA_Flags,           WFLG_REPORTMOUSE|WFLG_RMBTRAP,                   		      		 
                      	    TAG_END);

        
    
        theRastPort=_hardwareWindow->RPort;
        theBitMap=theRastPort->BitMap;
    
        SetPointer (_hardwareWindow, emptypointer, 0, 0, 0, 0);
 
        initialized = true;
        
        memset (bufferpixels,0,304*224*2);

 
 

    }
}

int
blitter_soft_init()
{
    screen_rect.x = 16;
    screen_rect.y = 16;
    screen_rect.w = 304;
    screen_rect.h = 224;

    visible_area.x = 0;
    visible_area.y = 0 ;
    visible_area.w = 304;
    visible_area.h = 224;

	Uint32 width = visible_area.w;
	Uint32 height = visible_area.h;


	if (vsync) {
		height=240;
		screen_rect.y = 8;

	} else {
		height=visible_area.h;
		screen_rect.y = 0;
		yscreenpadding=0;
	}

	screen_rect.w=visible_area.w;
	screen_rect.h=visible_area.h;


	if (neffect!=0)	scale =1;
	if (scale == 1) {
	    width *=effect[neffect].x_ratio;
	    height*=effect[neffect].y_ratio;
	} else {
	    if (scale > 3) scale=3;
	    width *=scale;
	    height *=scale;
	}


	initAmigaGraphics();
	
	return TRUE;
}


  ULONG eclocks_per_second; /* EClock frequency in Hz */
 extern char fps_str[32];
 
 /**********************************************************************/
static void video_do_fps (struct RastPort *rp, int yoffset)
{
  ULONG x;
  static struct EClockVal start_time = {0, 0};
  struct EClockVal end_time;
  char msg[4];

  ReadEClock (&end_time);
  x = end_time.ev_lo - start_time.ev_lo;
  if (x != 0) {
    x = (eclocks_per_second + (x >> 1)) / x;   /* round to nearest */
    msg[0] = (x % 1000) / 100 + '0';
    msg[1] = (x % 100) / 10 + '0';
    msg[2] = (x % 10) + '0';
    msg[3] = '\0';
    Move (rp, 24, yoffset + 6);
    Text (rp, fps_str, 2);
  }
  start_time = end_time;
}

 
 
void blitter_soft_update()
{    
    ULONG  DestMod = 0;
    ULONG * pDest  = NULL;
    struct TagItem mesTags[] = {{LBMI_BASEADDRESS, (ULONG) &pDest},
       {LBMI_BYTESPERROW, (ULONG) &DestMod},
       {TAG_END}};

    APTR  handle  = LockBitMapTagList( theRastPort->BitMap, &mesTags[0] );
    CopyMemQuick(bufferpixels + 24    ,pDest,  0x26000);
    video_do_fps(theRastPort,0);        
    UnLockBitMap( handle );  
}

    
void
blitter_soft_close()    
{
 
}
void
blitter_soft_fullscreen() {
 
}
                  
