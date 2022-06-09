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

#define FEA_SUCCES 0
#define FEA_ERR_INIT -1
#define FEA_ERR_SECCOUNT -2
#define FEA_ERR_SECSIZE -3
#define FEA_ERR_MOUNT -4


static const char *sdroot="/SD:/";
static const char *sdconfig= "/SD:/config";
static const char *sddata= "/SD:/data";
static const char *sdtest= "/SD:/tests";
/*
 * Forward declare routines
 */


int mount_sdcard(void);

int get_sd_path(char * outpath, const char * dir, const char * filename);

ssize_t fs_gets(char * linebuffer,size_t bufsz,struct fs_file_t* fid);

