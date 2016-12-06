#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

//#include "SDL.h"
 
#include "emu.h"
#include "roms.h"
#include "resfile.h"
#include "unzip.h"
//#include "stb_zlib.h"
#include "conf.h"
#include "stb_image.h"


void zread_char(ZFILE *gz, char *c, int len) {
	int rc;
	rc = gn_unzip_fread(gz, (Uint8*)c, len);
	//printf("HS  %s %d\n",c,rc);
}
void zread_uint8(ZFILE *gz, Uint8 *c) {
	int rc;
	rc = gn_unzip_fread(gz, c, 1);
	//printf("H8  %02x %d\n",*c,rc);
}
void zread_uint32le(ZFILE *gz, Uint32 *c) {
	int rc;
	rc = gn_unzip_fread(gz, (Uint8*)c, sizeof(Uint32));
#ifdef WORDS_BIGENDIAN
	*c=SwapLONG(*c);
#endif
	//printf("H32  %08x %d\n",*c,rc);
}

/*
 * Load a rom definition file from gngeo.dat (rom/name.drv)
 * return ROM_DEF*, NULL on error
 */
ROM_DEF *res_load_drv(char *name) {
	char *gngeo_dat = CF_STR(cf_get_item_by_name("datafile"));
	ROM_DEF *drv;
	char drvfname[32];
	PKZIP *pz;
	ZFILE *z;
	int i;

	drv = calloc(sizeof(ROM_DEF), 1);

	/* Open the rom driver def */
	
	printf("%s\n",gngeo_dat);
	
	pz = gn_open_zip(gngeo_dat);
	if (pz == NULL) {
		fprintf(stderr, "Can't open the %s\n", gngeo_dat);
		return NULL;
	}
	sprintf(drvfname, "rom/%s.drv", name);
    printf("Driver = %s\n",drvfname);
    
	if ((z=gn_unzip_fopen(pz,drvfname,0x0)) == NULL) {
		fprintf(stderr, "Can't open rom driver for %s\n", name);
		return NULL;
	}
     printf(" done Driver = %s\n",drvfname);
	//Fill the driver struct
	zread_char(z, drv->name, 32);
	zread_char(z, drv->parent, 32);
	zread_char(z, drv->longname, 128);
	zread_uint32le(z, &drv->year);
	for (i = 0; i < 10; i++)
		zread_uint32le(z, &drv->romsize[i]);
	zread_uint32le(z, &drv->nb_romfile);
	for (i = 0; i < drv->nb_romfile; i++) {
		zread_char(z, drv->rom[i].filename, 32);
		zread_uint8(z, &drv->rom[i].region);
		zread_uint32le(z, &drv->rom[i].src);
		zread_uint32le(z, &drv->rom[i].dest);
		zread_uint32le(z, &drv->rom[i].size);
		zread_uint32le(z, &drv->rom[i].crc);
	}
	gn_unzip_fclose(z);
	gn_close_zip(pz);
	return drv;
}



/*
 * Load a stb image from gngeo.dat
 * return a SDL_Surface, NULL on error
 * supported format: bmp, tga, jpeg, png, psd
 * 24&32bpp only
 */
 
void *res_load_data(char *name) {
	PKZIP *pz;
	Uint8 * buffer;
	unsigned int size;

	pz = gn_open_zip(CF_STR(cf_get_item_by_name("datafile")));
	if (!pz)
		return NULL;
	buffer = gn_unzip_file_malloc(pz, name, 0x0, &size);
	gn_close_zip(pz);
	return buffer;
}
