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
// Hardware double buffering.
struct ScreenBuffer *_hardwareScreenBuffer[2];
byte _currentScreenBuffer;

#define REG(xn, parm) parm __asm(#xn)
#define REGARGS __regargs
#define STDARGS __stdargs
#define SAVEDS __saveds
#define ALIGNED __attribute__ ((aligned(4))
#define FAR
#define CHIP
#define INLINE __inline__

extern void REGARGS c2p1x1_8_c5_bm_040(
REG(d0, UWORD chunky_x),
REG(d1, UWORD chunky_y),
REG(d2, UWORD offset_x),
REG(d3, UWORD offset_y),
REG(a0, UBYTE *chunky_buffer),
REG(a1, struct BitMap *bitmap));

extern void REGARGS c2p1x1_6_c5_bm_040(
REG(d0, UWORD chunky_x),
REG(d1, UWORD chunky_y),
REG(d2, UWORD offset_x),
REG(d3, UWORD offset_y),
REG(a0, UBYTE *chunky_buffer),
REG(a1, struct BitMap *bitmap));

extern void REGARGS c2p1x1_8_c5_bm(
REG(d0, UWORD chunky_x),
REG(d1, UWORD chunky_y),
REG(d2, UWORD offset_x),
REG(d3, UWORD offset_y),
REG(a0, UBYTE *chunky_buffer),
REG(a1, struct BitMap *bitmap));

extern void REGARGS c2p1x1_6_c5_bm(
REG(d0, UWORD chunky_x),
REG(d1, UWORD chunky_y),
REG(d2, UWORD offset_x),
REG(d3, UWORD offset_y),
REG(a0, UBYTE *chunky_buffer),
REG(a1, struct BitMap *bitmap));
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
    _hardwareScreenBuffer[0] = NULL;
    _hardwareScreenBuffer[1] = NULL;
    _currentScreenBuffer = 0;
    _hardwareScreen = NULL;
        firsttime = 0;
        bufferpixels = (unsigned char *)malloc(304*224*2);

        //if (M_CheckParm ("-ntsc"))
//        {
//            printf("I_InitGraphics: NTSC mode set \n");
            //modeId = NTSC_MONITOR_ID;
//        }
//        else
            modeId = PAL_MONITOR_ID;


       modeId = BestCModeIDTags(CYBRBIDTG_NominalWidth, 320,
				      CYBRBIDTG_NominalHeight, 240,
				      CYBRBIDTG_Depth,32,
				      TAG_DONE );


        if(modeId == INVALID_ID) {
          printf("Could not find a valid screen mode");
          exit(-1);
        }

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
                         TAG_END);


        // Create the hardware screen.


        _hardwareScreenBuffer[0] = AllocScreenBuffer(_hardwareScreen, NULL, SB_SCREEN_BITMAP);
        _hardwareScreenBuffer[1] = AllocScreenBuffer(_hardwareScreen, NULL, 0);

        _currentScreenBuffer = 1;

        _hardwareWindow = OpenWindowTags(NULL,
                      	    WA_Left, 16,
                			WA_Top, 16,
                			WA_Width, 304,
                			WA_Height, 224,
                			WA_Title, NULL,
        					SA_AutoScroll, FALSE,
                			WA_CustomScreen, (ULONG)_hardwareScreen,
                			WA_Backdrop, TRUE,
                			WA_Borderless, TRUE,
                			WA_DragBar, FALSE,
                			WA_Activate, TRUE,
                			WA_SimpleRefresh, TRUE,
                			WA_NoCareRefresh, TRUE,                        		      		 
                      	    TAG_END);

        
    
        theRastPort=_hardwareWindow->RPort;
        theBitMap=theRastPort->BitMap;
    
        SetPointer (_hardwareWindow, emptypointer, 1, 16, 0, 0);

        videoBuffer = (unsigned char *) (bufferpixels);

        initialized = true;
        
        memset (bufferpixels,0,304*224*2);

 
 

    }
}

int
blitter_soft_init()
{
    screen_rect.x = 0;
    screen_rect.y = 0;
    screen_rect.w = 304;
    screen_rect.h = 224;

    visible_area.x = 16;
    visible_area.y = 16;
    visible_area.w = 320;
    visible_area.h = 224;

	Uint32 width = visible_area.w;
	Uint32 height = visible_area.h;
#ifdef GP2X

#else

	//int hw_surface=(CF_BOOL(cf_get_item_by_name("hwsurface"))?SDL_HWSURFACE:SDL_SWSURFACE);
#endif
	//int screen_size=CF_BOOL(cf_get_item_by_name("screen320"));
#ifdef DEVKIT8000
	Uint32 sdl_flags = hw_surface|(fullscreen?SDL_FULLSCREEN:0)|SDL_DOUBLEBUF;

	screen_rect.w=visible_area.w;
	screen_rect.h=240;
	height=240;
#else



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


#endif
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


void WritePixelArray16New( APTR SrcRect, UWORD SrcX, UWORD SrcY, UWORD SrcMod,
struct RastPort * RastP, UWORD DestX, UWORD DestY, UWORD SizeX, UWORD SizeY );
 

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
    Text (rp, fps_str, 3);
  }
  start_time = end_time;
}

 
 
void blitter_soft_update()
{
    
WritePixelArray16New(bufferpixels,16,16,640,theRastPort,0,0,320,224);

video_do_fps(theRastPort,0);
}
    
void
blitter_soft_close()    
{
 
}
void
blitter_soft_fullscreen() {
 
}


void WritePixelArray16New( APTR SrcRect, UWORD SrcX, UWORD SrcY, UWORD SrcMod, 
struct RastPort * RastP, UWORD DestX, UWORD DestY, UWORD SizeX, UWORD SizeY )
{
    ULONG  DestMod = 0;
    ULONG * pDest  = NULL;
    struct TagItem mesTags[] = {{LBMI_BASEADDRESS, (ULONG) &pDest},
       {LBMI_BYTESPERROW, (ULONG) &DestMod},
       {TAG_END}};
    
    
    APTR  handle  = LockBitMapTagList( RastP->BitMap, &mesTags[0] );

   if( handle )
   {    
        UWORD curDestX = 0;
        UWORD curSrcX = 0;
        UBYTE * pSrc  = (UBYTE*)SrcRect;
        
        pSrc+=(SrcY*SrcMod)+(SrcX);
        
        for( ; SizeY > 0; DestY++, SizeY-- )
        {
           for(   curDestX = DestX, curSrcX = SrcX;
           curDestX < (DestX+SizeX); curDestX++, curSrcX++ )
           {
               ULONG pixel=*((UWORD*)pSrc);   
               pSrc+=2;        
               *(pDest++)= 0xFF000000|(((pixel&0xF800)<<8)|((pixel&0x7E0)<<5)|(pixel&0x1F)<<3);
               pixel=*((UWORD*)pSrc);
               pSrc+=2;
               *(pDest++)= 0xFF000000|(((pixel&0xF800)<<8)|((pixel&0x7E0)<<5)|(pixel&0x1F)<<3);                                                   
               pixel=*((UWORD*)pSrc);               
               pSrc+=2;
               *(pDest++)= 0xFF000000|(((pixel&0xF800)<<8)|((pixel&0x7E0)<<5)|(pixel&0x1F)<<3);
               pixel=*((UWORD*)pSrc);                           
               pSrc+=2;
               *(pDest++)= 0xFF000000|(((pixel&0xF800)<<8)|((pixel&0x7E0)<<5)|(pixel&0x1F)<<3);
               pixel=*((UWORD*)pSrc);
               pSrc+=2;
               *(pDest++)= 0xFF000000|(((pixel&0xF800)<<8)|((pixel&0x7E0)<<5)|(pixel&0x1F)<<3);
               pixel=*((UWORD*)pSrc);
               pSrc+=2;
               *(pDest++)= 0xFF000000|(((pixel&0xF800)<<8)|((pixel&0x7E0)<<5)|(pixel&0x1F)<<3);
               pixel=*((UWORD*)pSrc);
               pSrc+=2;
               *(pDest++)= 0xFF000000|(((pixel&0xF800)<<8)|((pixel&0x7E0)<<5)|(pixel&0x1F)<<3);
               pixel=*((UWORD*)pSrc);
               pSrc+=2;
               *(pDest++)= 0xFF000000|(((pixel&0xF800)<<8)|((pixel&0x7E0)<<5)|(pixel&0x1F)<<3);                                                            
 
                curDestX+=7;
                curSrcX+=7;
           }

        }

        UnLockBitMap( handle );
   }
}                         
                         
