/*
 * Copyright (c) 2022 Roelof Rietbroek, with segments from Nordic Semiconductor zephyr sample codes
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 
 Roelof Rietbroek (r.rietbroek@utwente.nl)  
*/
#include <zephyr/kernel.h>
#include <nrf_modem_gnss.h>
#include <stdio.h>
#include "featherw_datalogger.h"
#include "lz4file.h"
#include "config.h"
#include "gnss.h"
#include "modem.h"
#include "led_buttons.h"

#ifdef CONFIG_UPLOAD_CLIENT
#include "uploadclient.h"
#endif 

#ifdef CONFIG_SUPL_CLIENT_LIB
#include "supl_support.h"
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(GNSSR,CONFIG_GNSSR_LOG_LEVEL);

/*state variabless*/
static uint64_t                 log_timestamp;
static struct device_status dev_status;

/* Note: the actual message queue  and semaphore are defined in gnss.c */
extern struct k_msgq nmea_queue;
K_SEM_DEFINE(rollover_event_sem, 0, 1);



static struct k_poll_event events[2] = {
	K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_SEM_AVAILABLE,
					K_POLL_MODE_NOTIFY_ONLY,
					&rollover_event_sem, 0),
	K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
					K_POLL_MODE_NOTIFY_ONLY,
					&nmea_queue, 0),
};

/* config with defaults */
static struct config confdata;

#ifdef CONFIG_UPLOAD_CLIENT
void sync_files(){
	if (confdata.upload == 1){
		int prev_ledstatus=get_led_status();
		set_led_status(LED_UPLOADING);
		LOG_INF("Syncing data files");
		char lz4file[50];
		char lz4fullfile[100];
		char lz4renamed[103];
		char datadir[50];
		struct fs_dir_t dirp;
		(void)get_sd_data_path(datadir,NULL);
		(void) lsdir_init(datadir, &dirp);	

		bool lte_active=false;
		
		while(lsdir_next(".lz4",&dirp,lz4file) == 0){
			if(!lte_active){
				gnss_stop();
				lte_connect();
				lte_active=true;
			}
			(void)get_sd_data_path(lz4fullfile,lz4file);
			printk("Uploading lz4file found %s\n",lz4fullfile);
			if(webdavUploadFile(lz4fullfile,&confdata) == UPLOADCLNT_SUCCESS){
				/*rename file */
				LOG_INF("Sucessfully uploaded file %s, renaming",lz4file);
				strcpy(lz4renamed,lz4fullfile);
				strcat(lz4renamed,"_ok");
				fs_rename(lz4fullfile,lz4renamed);
			}else{
				LOG_INF("cannot currently upload file %s, trying later",lz4file);
			}
		}



		(void) lsdir_close(&dirp);	


		if (lte_active){
			LOG_INF("Closing LTE link and restarting GNSS\n");
			lte_disconnect();
			k_sleep(K_MSEC(1000));
			gnss_start();
			k_sleep(K_MSEC(1000));
		}

		
		set_led_status(prev_ledstatus);
	}else{
		LOG_INF("Skipping sync due to disabled user options upload");
	}

}
#endif



/* open a new unused logging stream */
int rollover_lz4log(lz4streamfile * lz4fid){

	char filenamebase[55];
	char datestr[18];
	
	
	/* File potentially needs closing */
	if (lz4fid->isOpen){
		lz4close(lz4fid);
	}
	
	gnss_get_current_datetimestr(datestr);

	sprintf(filenamebase,"%s_%s.lz4",confdata.filebase,datestr);
			
	get_sd_data_path(lz4fid->filename, filenamebase);
	
	LOG_INF("Opening %s",lz4fid->filename);		
	
	if (lz4open(lz4fid->filename,lz4fid) != LZ4_SUCCESS){
	
		return -1;
	}
	log_timestamp=k_uptime_get();

	return 0;

}





int main(void)
{
	set_led_status(LED_SEARCHING);

	/* NOTE leds and button event are intialized in a separate thread */
	
	if(setup_modem() !=0){
		set_led_status(LED_ERROR);
		return -1;
	}

	print_boardinfo();
	
	LOG_INF("Mounting and initializing featherwing sdcard\n");

	if (mount_sdcard() != FEA_SUCCESS){
		set_led_status(LED_ERROR);
		return -1;
	}

	if (initialize_sdcard_files() != FEA_SUCCESS){
		set_led_status(LED_ERROR);
		return -1;
	}

	LOG_INF("Loading config data");
	/* read configuration */
	if (read_config(&confdata) != CONF_SUCCESS){
		set_led_status(LED_ERROR);
		return -1;
	}


	/*initialize device status*/
	strcpy(dev_status.device_id,confdata.filebase);
	dev_status.uptime=k_uptime_get()/(MSEC_PER_SEC*3600.0);
	dev_status.longitude=0.0;
	dev_status.height=0.0;
	dev_status.latitude=0.0;
	dev_status.batvoltage=-1.0;

#ifdef CONFIG_UPLOAD_CLIENT
	/* register TLS certificate in the modem */
	if (confdata.webdav.usetls == 1){
		if (cert_provision(confdata.webdav.tlscert) != UPLOADCLNT_SUCCESS){
			set_led_status(LED_ERROR);
			return -1;
		}
	}

#endif
	LOG_INF("Starting GNSS-R logger application\n");


	static struct lz4streamfile lz4fid ;
	init_lz4stream(&lz4fid,true);
		
	/* start and initialize gnss */

	if (init_gnss(confdata.agps) !=0){
		set_led_status(LED_ERROR);
		LOG_ERR("Cannot initialize GNSS");
		return -1;
	}
	if(start_gnss() != 0){
		set_led_status(LED_ERROR);
		LOG_ERR("Cannot start GNSS");
		return -1;
	}

	LOG_INF("Getting GNSS data...\n");
	set_led_status(LED_SEARCHING);

	struct nrf_modem_gnss_nmea_data_frame *nmea_data;
	
	/* start polling loop */
	for (;;) {
		/* wait for a nmea message, log_rollover event or pvt_fix */
		(void)k_poll(events, 2, K_FOREVER);
		
		
		
		/* Check for log rollover */
		if (events[0].state == K_POLL_STATE_SEM_AVAILABLE &&
		    k_sem_take(events[0].sem, K_NO_WAIT) == 0) {
			if(rollover_lz4log(&lz4fid) != 0){
				LOG_ERR("failed to roll over log file");
			}


#ifdef CONFIG_UPLOAD_CLIENT
			sync_files();
#endif 
			events[0].state = K_POLL_STATE_NOT_READY;
		}	
		
		
		/* Handle new NMEA data */
		if (events[1].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE){
				/*only get nmea data and write it to file when the log is open and there is a position fix*/
				if(k_msgq_get(events[1].msgq, &nmea_data, K_NO_WAIT) == 0){
					if(lz4fid.isOpen && got_fix()){
						lz4write(&lz4fid,nmea_data->nmea_str);	
					}
					/*free the nmea_data to make room for a new message */
					k_free(nmea_data);
				}
			events[1].state = K_POLL_STATE_NOT_READY;

		}	

	}


	return 0;
}
