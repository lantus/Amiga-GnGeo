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
//#include "SDL.h"
#include "messages.h"
#include "video.h"
#include "emu.h"
#include "timer.h"
#include "frame_skip.h"
#include "screen.h"
#include "sound.h"
#include <stdarg.h>


static int font_w=8;
static int font_h=9;
 
//static timer_struct *msg_timer;
/*
void stop_message(int param)
{
  conf.do_message=0;
  msg_timer=NULL;
}
*/
void draw_message(const char *string)
{
    /*
       if (msg_timer!=NULL)
       del_timer(msg_timer);
       msg_timer=NULL;
     */
    strcpy(conf.message, string);
    conf.do_message = 75;
    //msg_timer=insert_timer(1.0,0,stop_message);
}


#define LEFT 1
#define RIGHT 2
#define BACKSPACE 3
#define DEL 4

 
