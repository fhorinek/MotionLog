/*
 * storage.h
 *
 *  Created on: 23.7.2014
 *      Author: horinek
 */

#ifndef STORAGE_H_
#define STORAGE_H_

#include "../psychosonda.h"

#include "FatFs/ff.h"
#include "FatFs/diskio.h"

//Glue for LUFA
#define DATAFLASH_PAGE_SIZE		512

extern DataBuffer * sd_buffer;

bool storage_init();
void storage_deinit();
void storage_copy_file(char * src, char * dst);
bool storage_ready();


#endif /* STORAGE_H_ */
