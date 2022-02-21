/*
* Copyright (c) 2022 Roelof Rietbroek <r.rietbroek@utwente.nl>
*
* SPDX-License-Identifier: Apache-2.0
*/




/* 
 * ERROR CODES
 */

#define FEA_SUCCES 0
#define FEA_ERR_INIT -1
#define FEA_ERR_SECCOUNT -2
#define FEA_ERR_SECSIZE -3
#define FEA_ERR_MOUNT -4


static const char *disk_mount_pt = "/SD:";
/*
 * Forward declare routines
 */

int lsdir(const char *path);
int mount_sdcard(void);

