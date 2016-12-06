#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
 
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#if defined(HAVE_LIBZ) && defined (HAVE_MMAP)
#include <zlib.h>
#endif

#include "memory.h"
#include "state.h"
#include "fileio.h"
#include "screen.h"
#include "sound.h"
#include "emu.h"
//#include "streams.h"

#ifdef USE_STARSCREAM
static int m68k_flag=0x1;
#elif USE_GENERATOR68K
static int m68k_flag=0x2;
#elif USE_CYCLONE
static int m68k_flag=0x3;
#endif

#ifdef USE_RAZE
static int z80_flag=0x4;
#elif USE_MAMEZ80
static int z80_flag=0x8;
#elif USE_DRZ80
static int z80_flag=0xC;
#endif

#ifdef WORDS_BIGENDIAN
static int endian_flag=0x10;
#else
static int endian_flag=0x0;
#endif

#if defined (WII)
#define ROOTPATH "sd:/apps/gngeo/"
#elif defined (__AMIGA__)
#define ROOTPATH "data/"
#else
#define ROOTPATH ""
#endif

#if !defined(HAVE_LIBZ) || !defined (HAVE_MMAP)
#define gzopen fopen
#define gzread(f,data,size) fread(data,size,1,f)
#define gzwrite(f,data,size) fwrite(data,size,1,f)
#define gzclose fclose
#define gzFile FILE
#define gzeof feof
#define gzseek fseek

#endif

static ST_REG *reglist;
static ST_MODULE st_mod[ST_MODULE_END];
static Rect buf_rect    =	{24, 16, 304, 224};
static Rect screen_rect =	{ 0,  0, 304, 224};
//SDL_Surface *state_img_tmp;

void cpu_68k_mkstate(gzFile *gzf,int mode);
void cpu_z80_mkstate(gzFile *gzf,int mode);
void ym2610_mkstate(gzFile *gzf,int mode);

void create_state_register(ST_MODULE_TYPE module,const char *reg_name,
			   Uint8 num,void *data,int size,ST_DATA_TYPE type) {
    ST_REG *t=(ST_REG*)calloc(1,sizeof(ST_REG));
    t->next=st_mod[module].reglist;
    st_mod[module].reglist=t;
    t->reg_name=strdup(reg_name);
    t->data=data;
    t->size=size;
    t->type=type;
    t->num=num;
}

void set_pre_save_function(ST_MODULE_TYPE module,void (*func)(void)) {
    st_mod[module].pre_save_state=func;
}

void set_post_load_function(ST_MODULE_TYPE module,void (*func)(void)) {
    st_mod[module].post_load_state=func;
}

static void *find_data_by_name(ST_MODULE_TYPE module,Uint8 num,char *name) {
    ST_REG *t=st_mod[module].reglist;
    while(t) {
	if ((!strcmp(name,t->reg_name)) && (t->num==num)) {
	    /*
	     *len=t->size;
	     *type=t->type;
	     */
	    return t->data;
	}
	t=t->next;
    }
    return NULL;
}

static int sizeof_st_type(ST_DATA_TYPE type) {
    switch (type) {
    case REG_UINT8:
    case REG_INT8:
	return 1;
    case REG_UINT16:
    case REG_INT16:
	return 2;
    case REG_UINT32:
    case REG_INT32:
	return 4;
    }
    return 0; /* never go here */
}

void swap_buf16_if_need(Uint8 src_endian,Uint16* buf,Uint32 size)
{
    int i;
#ifdef WORDS_BIGENDIAN
    Uint8  my_endian=1;
#else
    Uint8  my_endian=0;
#endif
    if (my_endian!=src_endian) {
	for (i=0;i<size;i++)
	    SwapSHORT(buf[i]);
    }
}

void swap_buf32_if_need(Uint8 src_endian,Uint32* buf,Uint32 size)
{
    int i;
#ifdef WORDS_BIGENDIAN
    Uint8  my_endian=1;
#else
    Uint8  my_endian=0;
#endif
    if (my_endian!=src_endian) {
	for (i=0;i<size;i++)
	    buf[i]=SwapLONG(buf[i]);
    }
}

Uint32 how_many_slot(char *game) {
	char *st_name;
	FILE *f;
//    char *st_name_len;
#ifdef EMBEDDED_FS
	char *gngeo_dir=ROOTPATH"save/";
#else
	char *gngeo_dir=get_gngeo_dir();
#endif
	Uint32 slot=0;
	st_name=(char*)alloca(strlen(gngeo_dir)+strlen(game)+5);
	while (1) {
		sprintf(st_name,"%s%s.%03d",gngeo_dir,game,slot);
		if (st_name && (f=fopen(st_name,"rb"))) {
			fclose(f);
			slot++;
		} else
		    return slot;
	}
}
#if 0
SDL_Surface *load_state_img(char *game,int slot) {
	char *st_name;
//    char *st_name_len;
#ifdef EMBEDDED_FS
	char *gngeo_dir="save/";
#else
	char *gngeo_dir=get_gngeo_dir();
#endif
	
#ifdef WORDS_BIGENDIAN
	Uint8  my_endian=1;
#else
	Uint8  my_endian=0;
#endif
	char string[20];
	gzFile *gzf;
	Uint8  endian;
	Uint32 rate;

    st_name=(char*)alloca(strlen(gngeo_dir)+strlen(game)+5);
    sprintf(st_name,"%s%s.%03d",gngeo_dir,game,slot);

    if ((gzf=gzopen(st_name,"rb"))==NULL) {
	printf("%s not found\n",st_name);
	return NULL;
    }

    memset(string,0,20);
    gzread(gzf,string,6);

    if (strcmp(string,"GNGST1")) {
	printf("%s is not a valid gngeo st file\n",st_name);
	gzclose(gzf);
	return NULL; 
    }

    gzread(gzf,&endian,1);

    if (my_endian!=endian) {
	printf("This save state comme from a different endian architecture.\n"
	       "This is not currently supported :(\n");
	return NULL;
    }

    gzread(gzf,&rate,4); // don't care
    
    gzread(gzf,state_img_tmp->pixels,304*224*2);
    gzclose(gzf);
    return state_img_tmp;
}

bool load_state(char *game,int slot) {
    char *st_name;
//    char *st_name_len;
#ifdef EMBEDDED_FS
    char *gngeo_dir="save/";
#else
    char *gngeo_dir=get_gngeo_dir();
#endif

#ifdef WORDS_BIGENDIAN
    Uint8  my_endian=1;
#else
    Uint8  my_endian=0;
#endif

    int i;
    gzFile *gzf;
    char string[20];
    Uint8 a,num;
    ST_DATA_TYPE type;
    void *data;
    Uint32 len;

    Uint8  endian;
    Uint32 rate;

    st_name=(char*)alloca(strlen(gngeo_dir)+strlen(game)+5);
    sprintf(st_name,"%s%s.%03d",gngeo_dir,game,slot);

    if ((gzf=gzopen(st_name,"rb"))==NULL) {
	printf("%s not found\n",st_name);
	return false;
    }

    memset(string,0,20);
    gzread(gzf,string,6);

    if (strcmp(string,"GNGST1")) {
	printf("%s is not a valid gngeo st file\n",st_name);
	gzclose(gzf);
	return false;
    }


    gzread(gzf,&endian,1);

    if (my_endian!=endian) {
	printf("This save state comme from a different endian architecture.\n"
	       "This is not currently supported :(\n");
	return false;
    }

    gzread(gzf,&rate,4);
    swap_buf32_if_need(endian,&rate,1);

#ifdef GP2X
    if (rate==0 && conf.sound) {
	    gn_popup_error("Failed!",
			   "This save state is incompatible "
			   "because you have sound enabled "
			   "and this save state don't have sound data");
	    return false;
    }
    if (rate!=0 && conf.sound==0) {
	    gn_popup_error("Failed!",
			   "This save state is incompatible "
			   "because you don't have sound enabled "
			   "and this save state need it");
	    return false;
    } else if (rate!=conf.sample_rate && conf.sound) {
	    conf.sample_rate=rate;
	    close_audio();
	    init_audio();
    }
#else
    if (rate==0 && conf.sound) {
	/* disable sound */
	conf.sound=0;
	pause_audio(1);
	close_sdl_audio();
    } else if (rate!=0 && conf.sound==0) {
	/* enable sound */
	conf.sound=1;
	conf.sample_rate=rate;
	if (!conf.snd_st_reg_create) {
	    cpu_z80_init();
	    init_audio();
	    //streams_sh_start();
	    YM2610_sh_start();
	    conf.snd_st_reg_create=1;
	} else 
	    init_audio();
	pause_audio(0);
    } else if (rate!=conf.sample_rate && conf.sound) {
	conf.sample_rate=rate;
	close_audio();
	init_audio();
    }
#endif


    gzread(gzf,state_img->pixels,304*224*2);
    swap_buf16_if_need(endian,state_img->pixels,304*224);

    

    while(!gzeof(gzf)) {
	gzread(gzf,&a,1); /* name size */
	memset(string,0,20);
	gzread(gzf,string,a); /* regname */
	gzread(gzf,&num,1); /* regname num */
	gzread(gzf,&a,1); /* module id */
	gzread(gzf,&len,4);
	gzread(gzf,&type,1);
	data=find_data_by_name(a,num,string);
	if (data) {
	    gzread(gzf,data,len);
	    switch(type) {
	    case REG_UINT16:
	    case REG_INT16:
		swap_buf16_if_need(endian,data,len>>1);
		break;
	    case REG_UINT32:
	    case REG_INT32:
		swap_buf32_if_need(endian,data,len>>2);
		break;
	    case REG_INT8:
	    case REG_UINT8:
		/* nothing */
		break;
	    }
	} else {
	    /* unknow reg, ignore it*/
	    printf("skeeping unknow reg %s\n",string);
	    gzseek(gzf,len,SEEK_CUR);
	}
    
    
	// /*if (a==ST_68k)*/ printf("LO %02d %20s %02x %08x \n",a,string,num,len/*,*(Uint32*)data*/);
    }
    gzclose(gzf);

    for(i=0;i<ST_MODULE_END;i++) {
	if (st_mod[i].post_load_state) st_mod[i].post_load_state();
    }

    return true;
}

bool save_state(char *game,int slot) {
     char *st_name;
//    char *st_name_len;
#ifdef EMBEDDED_FS
     char *gngeo_dir="save/";
#else
     char *gngeo_dir=get_gngeo_dir();
#endif

    Uint8 i;
    gzFile *gzf;
    char string[20];
    Uint8 a,num;
    ST_DATA_TYPE type;
    void *data;
    Uint32 len;
#ifdef WORDS_BIGENDIAN
    Uint8  endian=1;
#else
    Uint8  endian=0;
#endif
    Uint32 rate=(conf.sound?conf.sample_rate:0);

    st_name=(char*)alloca(strlen(gngeo_dir)+strlen(game)+5);
    sprintf(st_name,"%s%s.%03d",gngeo_dir,game,slot);

    if ((gzf=gzopen(st_name,"wb"))==NULL) {
	printf("can't write to %s\n",st_name);
	return false;
    }

/*
#ifndef GP2X
    SDL_BlitSurface(buffer, &buf_rect, state_img, &screen_rect);
#endif
*/

    gzwrite(gzf,"GNGST1",6);
    gzwrite(gzf,&endian,1);
    gzwrite(gzf,&rate,4);
    gzwrite(gzf,state_img->pixels,304*224*2);
    for(i=0;i<ST_MODULE_END;i++) {
	ST_REG *t=st_mod[i].reglist;
	if (st_mod[i].pre_save_state) st_mod[i].pre_save_state();
	while(t) {
	    // /*if (i==ST_68k)*/ printf("SV %02d %20s %02x %08x \n",i,t->reg_name,t->num,t->size/*,*(Uint32*)t->data*/);
	    
	    a=strlen(t->reg_name);
	    gzwrite(gzf,&a,1); /* strlen(regname) */
	    gzwrite(gzf,t->reg_name,strlen(t->reg_name)); /* regname */
	    gzwrite(gzf,&t->num,1); /* regname num */
	    gzwrite(gzf,&i,1); /* module id */
	    gzwrite(gzf,&t->size,4);
	    gzwrite(gzf,&t->type,1);
	    gzwrite(gzf,t->data,t->size);

	    t=t->next;
	}
    }
    gzclose(gzf);
   return true;

}

#else

static gzFile *open_state(char *game,int slot,int mode) {
	char *st_name;
//    char *st_name_len;
#ifdef EMBEDDED_FS
	char *gngeo_dir=ROOTPATH"save/";
#else
	char *gngeo_dir=get_gngeo_dir();
#endif
	char string[20];
	char *m=(mode==STWRITE?"wb":"rb");
	gzFile *gzf;
	int  flags;
	Uint32 rate;

    st_name=(char*)alloca(strlen(gngeo_dir)+strlen(game)+5);
    sprintf(st_name,"%s%s.%03d",gngeo_dir,game,slot);

	if ((gzf = gzopen(st_name, m)) == NULL) {
		printf("%s not found\n", st_name);
		return NULL;
    }

	if(mode==STREAD) {

		memset(string, 0, 20);
		gzread(gzf, string, 6);

		if (strcmp(string, "GNGST2")) {
			printf("%s is not a valid gngeo st file\n", st_name);
			gzclose(gzf);
			return NULL;
		}

		gzread(gzf, &flags, sizeof (int));

		if (flags != (m68k_flag | z80_flag | endian_flag)) {
			printf("This save state comme from a different endian architecture.\n"
					"This is not currently supported :(\n");
			gzclose(gzf);
			return NULL;
		}
	} else {
		int flags=m68k_flag | z80_flag | endian_flag;
		gzwrite(gzf, "GNGST2", 6);
		gzwrite(gzf, &flags, sizeof(int));
	}
	return gzf;
}

int mkstate_data(FILE *gzf,void *data,int size,int mode) {
	if (mode==STREAD)
		return gzread(gzf,data,size);
	return gzwrite(gzf,data,size);
}

 

static void neogeo_mkstate(gzFile *gzf,int mode) {
 
}

bool save_state(char *game,int slot) {
 
	return true;
}
bool load_state(char *game,int slot) {
 
	return true;
}
#endif



/* neogeo state register */ 
static Uint8 st_current_pal,st_current_fix;

static void neogeo_pre_save_state(void) {

    //st_current_pal=(current_pal==memory.pal1?0:1);
    //st_current_fix=(current_fix==memory.rom.bios_sfix.p?0:1);
    //printf("%d %d\n",st_current_pal,st_current_fix);
    
}

static void neogeo_post_load_state(void) {
 
 
}

void clear_state_reg(void) {
     
}

void neogeo_init_save_state(void) {
   
    
}





