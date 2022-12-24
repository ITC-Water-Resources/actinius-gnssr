/*
* Copyright (c) 2022 Roelof Rietbroek <r.rietbroek@utwente.nl>
*
* SPDX-License-Identifier: Apache-2.0
*/




#include <fs/fs_interface.h>
#include <sys/types.h>

/* 
 * ERROR CODES
 */


#ifndef FEATHERW_H
#define FEATHERW_H

#define FEA_SUCCESS 0
#define FEA_ERR_INIT -1
#define FEA_ERR_SECCOUNT -2
#define FEA_ERR_SECSIZE -3
#define FEA_ERR_MOUNT -4


//static const char *sdroot="/SD:/";
//static const char *sdconfig= "/SD:/config";
//static const char *sdtest= "/SD:/tests";
/*
 * Forward declare routines
 */


int mount_sdcard(void);
int initialize_sdcard_files(void);

int get_sd_data_path(char * outpath, const char * filename);

ssize_t fs_gets(char * linebuffer,size_t bufsz,struct fs_file_t* fid);

bool file_exists(const char *path);

#endif /* FEATHERW_H */
