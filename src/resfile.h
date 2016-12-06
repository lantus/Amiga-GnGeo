#ifndef RESFILE_H
#define RESFILE_H
 
#include "roms.h"

ROM_DEF *res_load_drv(char *name);
void *res_load_data(char *name);

#endif
