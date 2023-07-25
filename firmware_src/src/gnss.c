/*
* Copyright (c) 2023 Roelof Rietbroek <r.rietbroek@utwente.nl>
*
* SPDX-License-Identifier: Apache-2.0
* Refactored functions to work with GNSS parts of the modem
*/
#include <nrf_modem.h>
#include "led_buttons.h"
#include <zephyr/kernel.h>
#include "modem.h"
#include <nrf_modem_gnss.h>
#include <stdio.h>
#include <zephyr/logging/log.h>
#if defined(CONFIG_SUPL_CLIENT_LIB)
#include "supl_support.h"
#endif

LOG_MODULE_DECLARE(GNSSR,CONFIG_GNSSR_LOG_LEVEL);

static uint8_t cnt=0;
static const char update_indicator[] = {'\\', '|', '/', '-'};
static uint32_t gnss_fixed =0;
static struct nrf_modem_gnss_pvt_data_frame pvt_data;
static uint8_t last_day=0;
static uint64_t fix_timestamp;
static int agps=0;
#if defined(CONFIG_SUPL_CLIENT_LIB)
static struct nrf_modem_gnss_agps_data_frame last_agps;
#endif

extern struct k_sem rollover_event_sem;
K_MSGQ_DEFINE(nmea_queue, sizeof(struct nrf_modem_gnss_nmea_data_frame *), 10, 4);

uint32_t got_fix(void){
	return gnss_fixed;
}

void print_housekeeping_data(struct nrf_modem_gnss_pvt_data_frame *pvt_ptr)
{
	
	printk("---------housekeeping data-------\n");
	printf("Longitude:  %f\n", pvt_ptr->longitude);
	printf("Latitude:   %f\n", pvt_ptr->latitude);
	printf("Altitude:   %f\n", pvt_ptr->altitude);
	printf("Sigma Pos:  %f\n", pvt_ptr->accuracy);
	printf("Speed:      %f\n", pvt_ptr->speed);
	printf("Heading:    %f\n", pvt_ptr->heading);
	printk("Date:       %02u-%02u-%02u\n", pvt_ptr->datetime.day,
					       pvt_ptr->datetime.month,
					       pvt_ptr->datetime.year);
	printk("Time (UTC): %02u:%02u:%02u\n", pvt_ptr->datetime.hour,
					       pvt_ptr->datetime.minute,
					      pvt_ptr->datetime.seconds);
	printf("PDOP:           %.01f\n", pvt_ptr->pdop);
	printf("HDOP:           %.01f\n", pvt_ptr->hdop);
	printf("VDOP:           %.01f\n", pvt_ptr->vdop);
	printf("TDOP:           %.01f\n", pvt_ptr->tdop);
	printf("---------------------------------\n");
}

void print_searching(struct nrf_modem_gnss_pvt_data_frame *pvt_ptr)
{
	uint8_t  tracked          = 0;
	uint8_t  in_fix           = 0;
	uint8_t  unhealthy        = 0;

	for (int i = 0; i < NRF_MODEM_GNSS_MAX_SATELLITES; ++i) {

		if (pvt_ptr->sv[i].sv > 0)
		     {

			tracked++;

			if (pvt_ptr->sv[i].flags &
					NRF_MODEM_GNSS_SV_FLAG_USED_IN_FIX) {
				in_fix++;
			}

			if (pvt_ptr->sv[i].flags &
					NRF_MODEM_GNSS_SV_FLAG_UNHEALTHY) {
				unhealthy++;
			}
		}
	}

	printk("\033[1;1H");
	printk("\033[2J");
	printk("Tracking: %d Using: %d Unhealthy: %d\n", tracked,
							 in_fix,
							 unhealthy);
	printk("Seconds since last fix: %lld\n",
			       (k_uptime_get() - fix_timestamp) / 1000);
	if (pvt_ptr->flags & NRF_MODEM_GNSS_PVT_FLAG_DEADLINE_MISSED) {
		printk("GNSS notification deadline missed\n");
	}
	if (pvt_ptr->flags & NRF_MODEM_GNSS_PVT_FLAG_NOT_ENOUGH_WINDOW_TIME) {
		printk("GNSS operation blocked by insufficient time windows\n");
	}
	printk("Searching [%c]\n",update_indicator[(++cnt)%4]);
}

static void gnss_event_handler(int event)
{
	int retval;
	struct nrf_modem_gnss_nmea_data_frame *nmea_data;

	switch (event) {
	case NRF_MODEM_GNSS_EVT_PVT:
		retval = nrf_modem_gnss_read(&pvt_data, sizeof(pvt_data), NRF_MODEM_GNSS_DATA_PVT);
		/*check whether the PVT is from a fixed event*/

		if(pvt_data.flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID){
			gnss_fixed+=1;
			set_led_status(LED_LOGGING);
		}else{
			gnss_fixed=0;
			set_led_status(LED_SEARCHING);
		}
		
		if (gnss_fixed == 1){
			/* print housekeeping data to screen upon obtaining a fix */
			fix_timestamp = k_uptime_get();
			print_housekeeping_data(&pvt_data);	
		}else if (gnss_fixed == 0){
			print_searching(&pvt_data);
		}
		
		/* check for day rollover */

		if(pvt_data.datetime.day != last_day){
			k_sem_give(&rollover_event_sem);

		}
		last_day=pvt_data.datetime.day;

		
		break;

	case NRF_MODEM_GNSS_EVT_NMEA:
		nmea_data = k_malloc(sizeof(struct nrf_modem_gnss_nmea_data_frame));
		if (nmea_data == NULL) {
			LOG_ERR("Failed to allocate memory for NMEA");
			break;
		}

		retval = nrf_modem_gnss_read(nmea_data,
					     sizeof(struct nrf_modem_gnss_nmea_data_frame),
					     NRF_MODEM_GNSS_DATA_NMEA);
		if (retval == 0) {
			retval = k_msgq_put(&nmea_queue, &nmea_data, K_NO_WAIT);
		}

		if (retval != 0) {
			k_free(nmea_data);
		}
		break;
#if defined(CONFIG_SUPL_CLIENT_LIB)
	case NRF_MODEM_GNSS_EVT_AGPS_REQ:
		if(agps == 0)
		{
			/*AGPS is not desired by the setting*/
			return;

		}
		if(nrf_modem_gnss_read(&last_agps,
					     sizeof(last_agps),
					     NRF_MODEM_GNSS_DATA_AGPS_REQ) != 0){
			LOG_ERR("Error retreiveing AGPS request data");
			return;
		}
		


		/* SUPL doesn't usually provide satellite real time integrity information. If GNSS asks
		 * only for satellite integrity, the request should be ignored.
		 */
		if (last_agps.sv_mask_ephe == 0 &&
		    last_agps.sv_mask_alm == 0 &&
		    last_agps.data_flags == NRF_MODEM_GNSS_AGPS_INTEGRITY_REQUEST) {
			LOG_INF("Ignoring assistance request for only satellite integrity");
			return;
		}
		
		if( lte_connect() == 0){
			if(assistance_request(&last_agps) != 0){
				LOG_ERR("Error in retrieving SUPL assisted GPS");
			}
					
			lte_disconnect();
		}else{
			LOG_ERR("SUPL failed since LTE network was not reachable");

		}

		break;
#endif /* CONFIG_SUPL_CLIENT_LIB */

	default:
		break;
	}
}

void gnss_get_current_datetimestr(char cptr[]){

	sprintf(cptr,"%04u-%02u-%02uT%02u%02u%02u",pvt_data.datetime.year,
			pvt_data.datetime.month,
			pvt_data.datetime.day,
			pvt_data.datetime.hour,
			pvt_data.datetime.minute,
			pvt_data.datetime.seconds);

}

/* init and start gnss*/
int init_gnss(int useagps)
{
	int retval;
	agps=useagps;
	if( enable_gnss_mode() != 0){

		LOG_ERR("Failed to activate GNSS mode");
		return -1;
	}
	
	/* Configure GNSS event handler */
	if (nrf_modem_gnss_event_handler_set(gnss_event_handler) != 0) {
		LOG_ERR("Failed to set GNSS event handler");
		return -1;
	}
	
	uint16_t fix_interval = 1;
	retval=nrf_modem_gnss_fix_interval_set(fix_interval);
	if (retval !=0)
	{
		LOG_ERR("cannot set GNSS fix interval");
		return retval;
	}

		
	uint16_t fix_retry    = 0;
	retval=nrf_modem_gnss_fix_retry_set(fix_retry);
	if (retval !=0)
	{
		LOG_ERR("cannot set GNSS fix retry");
		return retval;
	}



	uint16_t nmea_mask    = NRF_MODEM_GNSS_NMEA_GSV_MASK |
				NRF_MODEM_GNSS_NMEA_RMC_MASK ;

	
	retval=nrf_modem_gnss_nmea_mask_set(nmea_mask);
	if (retval !=0)
	{
		LOG_ERR("cannot set GNSS NMEA mask");
		return retval;
	}
	
	/* allow low elevation tracking */
	uint8_t gnss_lowelev = CONFIG_GNSS_MIN_ELEV;
	
	retval=nrf_modem_gnss_elevation_threshold_set(gnss_lowelev);
	if (retval !=0)
	{
		LOG_ERR("cannot set GNSS Low elevation");
		return retval;
	}

	/* This use case flag should always be set. */
	uint8_t use_case = NRF_MODEM_GNSS_USE_CASE_MULTIPLE_HOT_START;
	retval=nrf_modem_gnss_use_case_set(use_case); 
	
	if (retval !=0)
	{
		LOG_ERR("cannot set GNSS use case");
		return retval;
	}

#ifdef CONFIG_SUPL_CLIENT_LIB
	if( agps){
		LOG_INF("Initializing SUPL library\n");
		if( assistance_init() != 0){
			return -1;
		}
	}
#endif
	
	return 0;	

}

int32_t stop_gnss(void){
	int32_t retval=nrf_modem_gnss_stop();

	if (retval !=0){
		LOG_ERR("Could not stop GNSS");
	}
	return retval;
}

int32_t start_gnss(void){
	int32_t	retval=nrf_modem_gnss_start();

	if (retval !=0){
		LOG_ERR("Could not start GNSS");
	}
	return retval;
}


/*void print_logging(void){*/

	/*printk("\033[1;1H");*/
	/*printk("\033[2J");*/
	/*printk("Seconds since last log rollover: %lld\n",*/
			       /*(k_uptime_get() - log_timestamp) / 1000);*/

	/*printk("Logging [%c]\n",update_indicator[(++cnt)%4]);*/
/*}*/



