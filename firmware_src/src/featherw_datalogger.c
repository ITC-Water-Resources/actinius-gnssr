/*
* Copyright (c) 2022 Roelof Rietbroek <r.rietbroek@utwente.nl>
*
* SPDX-License-Identifier: Apache-2.0
refactored and modified from the fat_fs zephyr example by Tavish Naruka <tavishnaruka@gmail.com>
*/

#include <device.h>
#include <disk/disk_access.h>
#include <fs/fs.h>
#include <ff.h>
#include <fs/fs_interface.h>
#include <string.h>


#include "featherw_datalogger.h"
#include <sys/types.h>

/*#include <logging/log.h>*/
/*LOG_MODULE_DECLARE(GNSSR,LOG_LEVEL_DBG);*/


/* temporary workaround using the logger*/
#define LOG_ERR(...) printk(__VA_ARGS__)
#define LOG_DBG(...) printk(__VA_ARGS__)
#define LOG_INF(...) printk(__VA_ARGS__)

static FATFS fat_fs;
/* mounting info */
static struct fs_mount_t mp = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
};
static const char *disk_mount_pt = "/SD:";
static const char *sddata= "/SD:/data";
static const char *sdconfig= "/SD:/config";

/*
*  Note the fatfs library is able to mount only strings inside _VOLUME_STRS
*  in ffconf.h
*/

int mount_sdcard(void)
{
	/* raw disk i/o */
	static const char *disk_pdrv = "SD";
	uint64_t memory_size_mb;
	uint32_t block_count;
	uint32_t block_size;

	if (disk_access_init(disk_pdrv) != 0) {
		LOG_ERR("Storage init ERROR!");
		return FEA_ERR_INIT;
	}

	if (disk_access_ioctl(disk_pdrv,
			DISK_IOCTL_GET_SECTOR_COUNT, &block_count)) {
		LOG_ERR("Unable to get sector count");
		return FEA_ERR_SECCOUNT;
	}
	LOG_INF("Block count %u", block_count);

	if (disk_access_ioctl(disk_pdrv,
			DISK_IOCTL_GET_SECTOR_SIZE, &block_size)) {
		LOG_ERR("Unable to get sector size");
		return FEA_ERR_SECSIZE;
	}
	LOG_DBG("Sector size %u\n", block_size);

	memory_size_mb = (uint64_t)block_count * block_size;
	LOG_DBG("Memory Size(MB) %u\n", (uint32_t)(memory_size_mb >> 20));

	mp.mnt_point = disk_mount_pt;

	int res = fs_mount(&mp);

	if (res == FR_OK) {
		return FEA_SUCCESS;
	} else {
		LOG_ERR("Unable to mount SD card");
		return FEA_ERR_MOUNT;
	}

}

/*Retrieve a file path on a certain directory on the sdcard*/
int get_sd_path(char * outpath,const char * dir, const char * filename){
	strcpy(outpath,dir);
	if (filename != NULL){
		strcat(outpath,"/");
		strcat(outpath,filename);
	}
	return FEA_SUCCESS;
}

int get_sd_config_path(char * outpath, const char * filename){
	return get_sd_path(outpath,sdconfig, filename);
	
}

int get_sd_data_path(char * outpath, const char * filename){
	return get_sd_path(outpath,sddata, filename);
	
}

/* initialize directories if they do not exist*/
int initialize_sdcard_files(void){
	char dir[100];
	get_sd_data_path(dir,NULL);
	if (!file_exists(dir)){
		int stat=fs_mkdir(dir);	
		if (stat  != 0){
			return FEA_ERR_INIT;
		}
	}
		
	/* also intialize config directory */
	get_sd_config_path(dir,NULL);
	if (!file_exists(dir)){
		int stat=fs_mkdir(dir);	
		if (stat  != 0){
			return FEA_ERR_INIT;
		}
	}
		
	return FEA_SUCCESS;

}




bool file_exists(const char * path){

	struct fs_dirent entry;
	if (fs_stat(path,&entry)== 0){
		return true;	
	}else{
		return false;
	}

}

/*Reads a new line from a file*/
ssize_t fs_gets(char * linebuffer,size_t bufsz,struct fs_file_t* fid){
	ssize_t nread=fs_read(fid,linebuffer,bufsz);
	if(nread < bufsz && nread >0){
		linebuffer[nread]='\0';
		return nread;
	}else if (nread == bufsz){
		/*Possibly more has been read than necessary: Find the line end*/
		char * lnend;
		if ((lnend=strchr(linebuffer,'\n')) == NULL){
			/*if((lnend=strchr(linebuffer,'\r')) == NULL){*/
				lnend=linebuffer+bufsz-1;
			/*}*/
		}
		ptrdiff_t slen=lnend-linebuffer+1;
		/*printk("slen %d\n",(int)slen);*/
		if (slen < bufsz){
			linebuffer[slen]='\0';
				off_t offs=bufsz-slen;
				/*LOG_INF("nread %d, seeking backwards with %d\n",(int)nread,(int)offs);*/
			if (fs_seek(fid,-offs,FS_SEEK_CUR) < 0){
				return -2;
			}
			return slen;
		}else{
			/*LOG_INF("returning full buffer\n");*/
			return bufsz;
		}
	}else{
		/*Errors and EOF's*/
		return -1;
	}
	
}


