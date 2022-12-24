/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 
Modified by Roelof Rietbroek (r.rietbroek@utwente.nl) to 
*/

#include <zephyr.h>
#include <nrf_socket.h>
#include <net/socket.h>
#include <stdio.h>
#include <modem/at_cmd.h>
#include <modem/at_notif.h>
#include "featherw_datalogger.h"

#include "lz4file.h"

#ifdef CONFIG_SUPL_CLIENT_LIB
#include <supl_os_client.h>
#include <supl_session.h>
#include "supl_support.h"
#endif

#define AT_XSYSTEMMODE      "AT\%XSYSTEMMODE=1,0,1,0"
#define AT_ACTIVATE_GPS     "AT+CFUN=31"
#define AT_ACTIVATE_LTE     "AT+CFUN=21"
#define AT_DEACTIVATE_LTE   "AT+CFUN=20"

#define GNSS_INIT_AND_START 1
#define GNSS_STOP           2
#define GNSS_RESTART        3

#define AT_CMD_SIZE(x) (sizeof(x) - 1)

#define AT_MAGPIO      "AT\%XMAGPIO=1,0,0,1,1,1574,1577"

static const char update_indicator[] = {'\\', '|', '/', '-'};
static const char *const at_commands[] = {
	AT_XSYSTEMMODE,
	AT_MAGPIO,
	AT_ACTIVATE_GPS
};

static int                   gnss_fd;

static bool                  got_fix;
static uint64_t                 fix_timestamp;
static uint64_t                 log_timestamp;
static nrf_gnss_data_frame_t last_pvt;

K_SEM_DEFINE(lte_ready, 0, 1);

void bsd_recoverable_error_handler(uint32_t error)
{
	printf("Err: %lu\n", (unsigned long)error);
}

static int setup_modem(void)
{
	for (int i = 0; i < ARRAY_SIZE(at_commands); i++) {

		if (at_cmd_write(at_commands[i], NULL, 0, NULL) != 0) {
			return -1;
		}
	}

	return 0;
}

#ifdef CONFIG_SUPL_CLIENT_LIB
/* Accepted network statuses read from modem */
static const char status1[] = "+CEREG: 1";
static const char status2[] = "+CEREG:1";
static const char status3[] = "+CEREG: 5";
static const char status4[] = "+CEREG:5";

static void wait_for_lte(void *context, const char *response)
{
	if (!memcmp(status1, response, AT_CMD_SIZE(status1)) ||
		!memcmp(status2, response, AT_CMD_SIZE(status2)) ||
		!memcmp(status3, response, AT_CMD_SIZE(status3)) ||
		!memcmp(status4, response, AT_CMD_SIZE(status4))) {
		k_sem_give(&lte_ready);
	}
}

static int activate_lte(bool activate)
{
	if (activate) {
		if (at_cmd_write(AT_ACTIVATE_LTE, NULL, 0, NULL) != 0) {
			return -1;
		}

		at_notif_register_handler(NULL, wait_for_lte);
		if (at_cmd_write("AT+CEREG=2", NULL, 0, NULL) != 0) {
			return -1;
		}

		k_sem_take(&lte_ready, K_FOREVER);

		at_notif_deregister_handler(NULL, wait_for_lte);
		if (at_cmd_write("AT+CEREG=0", NULL, 0, NULL) != 0) {
			return -1;
		}
	} else {
		if (at_cmd_write(AT_DEACTIVATE_LTE, NULL, 0, NULL) != 0) {
			return -1;
		}
	}

	return 0;
}
#endif

static int gnss_ctrl(uint32_t ctrl)
{
	int retval;

	nrf_gnss_fix_retry_t    fix_retry    = 0;
	nrf_gnss_fix_interval_t fix_interval = 1;
	nrf_gnss_delete_mask_t	delete_mask  = 0;
	nrf_gnss_nmea_mask_t	nmea_mask    = NRF_GNSS_NMEA_GSV_MASK |
					       NRF_GNSS_NMEA_GSA_MASK |
					       NRF_GNSS_NMEA_GLL_MASK |
					       NRF_GNSS_NMEA_GGA_MASK |
					       NRF_GNSS_NMEA_RMC_MASK;

	if (ctrl == GNSS_INIT_AND_START) {
		gnss_fd = nrf_socket(NRF_AF_LOCAL,
				     NRF_SOCK_DGRAM,
				     NRF_PROTO_GNSS);

		if (gnss_fd >= 0) {
			printk("GPS Socket created\n");
		} else {
			printk("Could not init socket (err: %d)\n", gnss_fd);
			return -1;
		}

		retval = nrf_setsockopt(gnss_fd,
					NRF_SOL_GNSS,
					NRF_SO_GNSS_FIX_RETRY,
					&fix_retry,
					sizeof(fix_retry));
		if (retval != 0) {
			printk("Failed to set fix retry value\n");
			return -1;
		}

		retval = nrf_setsockopt(gnss_fd,
					NRF_SOL_GNSS,
					NRF_SO_GNSS_FIX_INTERVAL,
					&fix_interval,
					sizeof(fix_interval));
		if (retval != 0) {
			printk("Failed to set fix interval value\n");
			return -1;
		}

		retval = nrf_setsockopt(gnss_fd,
					NRF_SOL_GNSS,
					NRF_SO_GNSS_NMEA_MASK,
					&nmea_mask,
					sizeof(nmea_mask));
		if (retval != 0) {
			printk("Failed to set nmea mask\n");
			return -1;
		}
	}

	if ((ctrl == GNSS_INIT_AND_START) ||
	    (ctrl == GNSS_RESTART)) {
		retval = nrf_setsockopt(gnss_fd,
					NRF_SOL_GNSS,
					NRF_SO_GNSS_START,
					&delete_mask,
					sizeof(delete_mask));
		if (retval != 0) {
			printk("Failed to start GPS\n");
			return -1;
		}
	}

	if (ctrl == GNSS_STOP) {
		retval = nrf_setsockopt(gnss_fd,
					NRF_SOL_GNSS,
					NRF_SO_GNSS_STOP,
					&delete_mask,
					sizeof(delete_mask));
		if (retval != 0) {
			printk("Failed to stop GPS\n");
			return -1;
		}
	}

	return 0;
}

static int init_app(void)
{
	int retval;

	if (setup_modem() != 0) {
		printk("Failed to initialize modem\n");
		return -1;
	}

	retval = gnss_ctrl(GNSS_INIT_AND_START);

	return retval;
}

static void print_searching(nrf_gnss_data_frame_t *pvt_data,const u_int8_t cnt)
{
	uint8_t  tracked          = 0;
	uint8_t  in_fix           = 0;
	uint8_t  unhealthy        = 0;

	for (int i = 0; i < NRF_GNSS_MAX_SATELLITES; ++i) {

		if ((pvt_data->pvt.sv[i].sv > 0) &&
		    (pvt_data->pvt.sv[i].sv < 33)) {

			tracked++;

			if (pvt_data->pvt.sv[i].flags &
					NRF_GNSS_SV_FLAG_USED_IN_FIX) {
				in_fix++;
			}

			if (pvt_data->pvt.sv[i].flags &
					NRF_GNSS_SV_FLAG_UNHEALTHY) {
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
	if (pvt_data->pvt.flags & NRF_GNSS_PVT_FLAG_DEADLINE_MISSED) {
		printk("GNSS notification deadline missed\n");
	}
	if (pvt_data->pvt.flags & NRF_GNSS_PVT_FLAG_NOT_ENOUGH_WINDOW_TIME) {
		printk("GNSS operation blocked by insufficient time windows\n");
	}
	printk("Searching [%c]\n",update_indicator[cnt%4]);
}

static void print_logging(const u_int8_t cnt){

	printk("\033[1;1H");
	printk("\033[2J");
	printk("Seconds since last log rollover: %lld\n",
			       (k_uptime_get() - log_timestamp) / 1000);

	printk("Logging [%c]\n",update_indicator[cnt%4]);
}



static void print_housekeeping_data(nrf_gnss_data_frame_t *pvt_data)
{
	
	printk("---------housekeeping data-------\n");
	printf("Longitude:  %f\n", pvt_data->pvt.longitude);
	printf("Latitude:   %f\n", pvt_data->pvt.latitude);
	printf("Altitude:   %f\n", pvt_data->pvt.altitude);
	printf("Speed:      %f\n", pvt_data->pvt.speed);
	printf("Heading:    %f\n", pvt_data->pvt.heading);
	printk("Date:       %02u-%02u-%02u\n", pvt_data->pvt.datetime.day,
					       pvt_data->pvt.datetime.month,
					       pvt_data->pvt.datetime.year);
	printk("Time (UTC): %02u:%02u:%02u\n", pvt_data->pvt.datetime.hour,
					       pvt_data->pvt.datetime.minute,
					      pvt_data->pvt.datetime.seconds);
	printk("---------------------------------\n");
}



/* open a new unused logging stream */
int rollover_lz4log(lz4streamfile * lz4fid){

	int nthfile;
	char filenamebase[30];
	char lz4fout[80];
	for (nthfile=0;nthfile < 100;++nthfile){	
		sprintf(filenamebase,"nmealog_%04u-%02u-%02u_%02d.lz4",
					last_pvt.pvt.datetime.year,
					last_pvt.pvt.datetime.month,
					last_pvt.pvt.datetime.day,nthfile);
			
		get_sd_data_path(lz4fout, filenamebase);
		/* continue finding a new file if it already exists */
		if (!file_exists(lz4fout)){
			break;
		}
	}
	if (nthfile >= 100){
		printk("Cannot find an available file name, quitting");

		/* File potentially needs closing */
		lz4close(lz4fid);
		return -1;
	}
	/* if we land here we found an available file name*/
		
	/* potentially close file */
	lz4close(lz4fid);
	
	printk("Opening %s %p\n",lz4fout,lz4fid);		
	
	if (lz4open(lz4fout,lz4fid) != LZ4_SUCCESS){
	
		return -1;
	}
	log_timestamp=k_uptime_get();

	return 0;

}

static const int fixes_per_log=50;
static int fix_counter=0;

int process_gnss_data(nrf_gnss_data_frame_t *gnss_data,lz4streamfile * lz4fid)
{
	int retval;

	retval = nrf_recv(gnss_fd,
			  gnss_data,
			  sizeof(nrf_gnss_data_frame_t),
			  NRF_MSG_DONTWAIT);

	if (retval > 0) {

		switch (gnss_data->data_id) {
		case NRF_GNSS_PVT_DATA_ID:
			bool log_rollover =false;
			
			log_rollover = gnss_data->pvt.datetime.day != last_pvt.pvt.datetime.day;

			/*log_rollover = (fix_counter)%fixes_per_log == 0;*/

			memcpy(&last_pvt,
			       gnss_data,
			       sizeof(nrf_gnss_data_frame_t));
			got_fix = false;

			if ((gnss_data->pvt.flags &
				NRF_GNSS_PVT_FLAG_FIX_VALID_BIT)
				== NRF_GNSS_PVT_FLAG_FIX_VALID_BIT) {

				got_fix = true;
				fix_timestamp = k_uptime_get();
				++fix_counter;
				if (log_rollover){
					print_housekeeping_data(&last_pvt);
					rollover_lz4log(lz4fid);
				}

			}
			break;

		case NRF_GNSS_NMEA_DATA_ID:
			if (got_fix && lz4fid->isOpen){
				/* put nmea data in the lz4log if it's open*/
				
				lz4write(lz4fid,gnss_data->nmea);
			}

			break;

		case NRF_GNSS_AGPS_DATA_ID:
#ifdef CONFIG_SUPL_CLIENT_LIB
			printk("\033[1;1H");
			printk("\033[2J");
			printk("New AGPS data requested, contacting SUPL server, flags %d\n",
			       gnss_data->agps.data_flags);
			gnss_ctrl(GNSS_STOP);
			activate_lte(true);
			printk("Established LTE link\n");
			if (open_supl_socket() == 0) {
				printf("Starting SUPL session\n");
				supl_session(&gnss_data->agps);
				printk("Done\n");
				close_supl_socket();
			}
			activate_lte(false);
			gnss_ctrl(GNSS_RESTART);
			k_sleep(K_MSEC(2000));
#endif
			break;

		default:
			break;
		}
	}

	return retval;
}

#ifdef CONFIG_SUPL_CLIENT_LIB
int inject_agps_type(void *agps,
		     size_t agps_size,
		     nrf_gnss_agps_data_type_t type,
		     void *user_data)
{
	ARG_UNUSED(user_data);
	int retval = nrf_sendto(gnss_fd,
				agps,
				agps_size,
				0,
				&type,
				sizeof(type));

	if (retval != 0) {
		printk("Failed to send AGNSS data, type: %d (err: %d)\n",
		       type,
		       errno);
		return -1;
	}

	printk("Injected AGPS data, flags: %d, size: %d\n", type, agps_size);

	return 0;
}
#endif





int main(void)
{
	nrf_gnss_data_frame_t gnss_data;
	uint8_t		      cnt = 0;

#ifdef CONFIG_SUPL_CLIENT_LIB
	static struct supl_api supl_api = {
		.read       = supl_read,
		.write      = supl_write,
		.handler    = inject_agps_type,
		.logger     = supl_logger,
		.counter_ms = k_uptime_get
	};
#endif

	printk("Mounting and initializing featherwing sdcard\n");

	if (mount_sdcard() != FEA_SUCCESS){

		return -1;
	}

	if (initialize_sdcard_files() != FEA_SUCCESS){
		return -1;
	}
	

	printk("Starting GNSS-R logger application\n");

	if (init_app() != 0) {
		return -1;
	}

#ifdef CONFIG_SUPL_CLIENT_LIB
	int rc = supl_init(&supl_api);

	if (rc != 0) {
		return rc;
	}
#endif
	static struct lz4streamfile lz4fid ;
	init_lz4stream(&lz4fid,true);
		
	printk("Getting GNSS data...\n");
	while (1) {

		do {
			/* Loop until we don't have more
			 * data to read
			 */
		} while (process_gnss_data(&gnss_data,&lz4fid) > 0);
		
		
		if (!got_fix) {
			print_searching(&last_pvt,++cnt);
		} else {
			print_logging(++cnt);
		}


		k_sleep(K_MSEC(500));

	}

	return 0;
}
