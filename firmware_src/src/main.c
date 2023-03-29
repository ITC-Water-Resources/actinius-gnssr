/*
 * Copyright (c) 2022 Roelof Rietbroek, with segments from Nordic Semiconductor zephyr sample codes
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 
 Roelof Rietbroek (r.rietbroek@utwente.nl)  
*/

#include <zephyr.h>
#include <nrf_socket.h>
#include <net/socket.h>
#include <stdio.h>
#include <modem/at_cmd.h>
#include <modem/at_notif.h>
#include <modem/modem_info.h>
#include "featherw_datalogger.h"
#include <drivers/gpio.h>
#include "lz4file.h"
#include "config.h"

#ifdef CONFIG_UPLOAD_CLIENT
#include "uploadclient.h"
#endif 

#ifdef CONFIG_SUPL_CLIENT_LIB
#include <supl_os_client.h>
#include <supl_session.h>
#include "supl_support.h"
#endif

#define AT_XSYSTEMMODE      "AT\%XSYSTEMMODE=1,0,1,1"
#define AT_ACTIVATE_GPS     "AT+CFUN=31"
#define AT_ACTIVATE_LTE     "AT+CFUN=21"
#define AT_DEACTIVATE_LTE   "AT+CFUN=20"

#define GNSS_INIT_AND_START 1
#define GNSS_STOP           2
#define GNSS_RESTART        3

#define AT_CMD_SIZE(x) (sizeof(x) - 1)

#define AT_MAGPIO      "AT\%XMAGPIO=1,0,0,1,1,1574,1577"


#define STACKSIZE 1024
#define PRIORITY 7
#include <logging/log.h>
LOG_MODULE_REGISTER(GNSSR,CONFIG_GNSSR_LOG_LEVEL);

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

/* config with defaults */
/*static struct config confdata={"",0,{"","",""}};*/
static struct config confdata;

K_SEM_DEFINE(lte_ready, 0, 1);

/*void bsd_recoverable_error_handler(uint32_t error)*/
/*{*/
	/*printf("Err: %lu\n", (unsigned long)error);*/
/*}*/

static int setup_modem(void)
{
	for (int i = 0; i < ARRAY_SIZE(at_commands); i++) {

		if (at_cmd_write(at_commands[i], NULL, 0, NULL) != 0) {
			return -1;
		}
	}

	return 0;
}

/* LED and button stuff */

#define LED_ERROR 0
#define LED_SEARCHING  1
#define LED_LOGGING  2
#define LED_UPLOADING 3

#define RED_LED_PIN		DT_GPIO_PIN(DT_NODELABEL(red_led), gpios)
#define GREEN_LED_PIN		DT_GPIO_PIN(DT_NODELABEL(green_led), gpios)
#define BLUE_LED_PIN		DT_GPIO_PIN(DT_NODELABEL(blue_led), gpios)

#define BUTTON_NODE    		DT_NODELABEL(button0)
#define BUTTON_GPIO_LABEL	DT_GPIO_LABEL(BUTTON_NODE, gpios)
#define BUTTON_GPIO_PIN		DT_GPIO_PIN(BUTTON_NODE, gpios)
#define BUTTON_GPIO_FLAGS	GPIO_INPUT | DT_GPIO_FLAGS(BUTTON_NODE, gpios)

#define LED_ON 0
#define LED_OFF !LED_ON

static struct device *gpio_dev;
static struct gpio_callback gpio_cb;

static int led_status= LED_SEARCHING;
static bool button_pressed=false;
static struct device_status dev_status;

void button_pressed_callback(const struct device *gpiob, struct gpio_callback *cb, gpio_port_pins_t pins)
{
	button_pressed = true;
}

bool init_button(void)
{
	int ret = gpio_pin_configure(gpio_dev, BUTTON_GPIO_PIN, BUTTON_GPIO_FLAGS);
	if (ret != 0) {
        LOG_ERR("Error %d: failed to configure %s pin %d\n",
            	ret, log_strdup(BUTTON_GPIO_LABEL), BUTTON_GPIO_PIN);
        
		return false;
    }

    ret = gpio_pin_interrupt_configure(gpio_dev,
                                       BUTTON_GPIO_PIN,
                                       GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0) {
        LOG_ERR("Error %d: failed to configure interrupt on %s pin %d\n",
               ret, log_strdup(BUTTON_GPIO_LABEL), BUTTON_GPIO_PIN);

        return false;
    }

    gpio_init_callback(&gpio_cb, button_pressed_callback, BIT(BUTTON_GPIO_PIN));
    gpio_add_callback(gpio_dev, &gpio_cb);

	return true;
}


void turn_leds_off(void)
{
	gpio_pin_set(gpio_dev, RED_LED_PIN, LED_OFF);
	gpio_pin_set(gpio_dev, GREEN_LED_PIN, LED_OFF);
	gpio_pin_set(gpio_dev, BLUE_LED_PIN, LED_OFF);
}

void init_leds(void)
{
	gpio_pin_configure(gpio_dev, RED_LED_PIN, GPIO_OUTPUT_HIGH);
	gpio_pin_configure(gpio_dev, GREEN_LED_PIN, GPIO_OUTPUT_HIGH);
	gpio_pin_configure(gpio_dev, BLUE_LED_PIN, GPIO_OUTPUT_HIGH);

	turn_leds_off();

}


int led_button_checker(void){
	k_sleep(K_MSEC(1000));

	gpio_dev = device_get_binding(DT_LABEL(DT_NODELABEL(gpio0)));

	if (!gpio_dev) {
		LOG_ERR("Error getting GPIO device binding\r\n");

		return -1;
	}

	if (!init_button()) {
		return -1;
	}

	init_leds();


	while(1){
		switch (led_status){
		case LED_SEARCHING:
			/*blinking yellow for a second every 5 seconds*/	
			gpio_pin_set(gpio_dev, RED_LED_PIN, LED_ON);
			gpio_pin_set(gpio_dev, GREEN_LED_PIN, LED_ON);
			k_sleep(K_MSEC(1000));
			turn_leds_off();
			k_sleep(K_MSEC(4000));
			break;
		case LED_LOGGING:
			/*flash green for a 10th of a second second every 10 seconds*/	
			gpio_pin_set(gpio_dev, GREEN_LED_PIN, LED_ON);
			k_sleep(K_MSEC(100));
			turn_leds_off();
			k_sleep(K_MSEC(9900));
			break;
		case LED_ERROR:
			/*blinking red a second every 10 seconds*/	
			gpio_pin_set(gpio_dev, RED_LED_PIN, LED_ON);
			k_sleep(K_MSEC(1000));
			turn_leds_off();
			k_sleep(K_MSEC(9000));
			break;
		case LED_UPLOADING:
			/*blinking blue a second every 5 seconds*/	
			gpio_pin_set(gpio_dev, BLUE_LED_PIN, LED_ON);
			k_sleep(K_MSEC(1000));
			turn_leds_off();
			k_sleep(K_MSEC(4000));
			break;
		default:
			/* shouldn't occur really, but allow sleep so this function does not spin */
			k_sleep(K_MSEC(1000));
			break;
		}

	}
	return 0;

}

K_THREAD_DEFINE(led_button_checker_id, STACKSIZE, led_button_checker, NULL, NULL, NULL,
		PRIORITY, 0, 0);

#if defined(CONFIG_UPLOAD_CLIENT) || defined(CONFIG_SUPL_CLIENT_LIB)
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
		

		if (k_sem_take(&lte_ready, K_MSEC(30000)) != 0){
			return -1;
		}

		
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
	nrf_gnss_nmea_mask_t	nmea_mask    = NRF_GNSS_NMEA_GSV_MASK | NRF_GNSS_NMEA_RMC_MASK ;
	
	/* allow low elevation tracking */
	nrf_gnss_elevation_mask_t gnss_lowelev = 2;

	nrf_setsockopt(gnss_fd, NRF_SOL_GNSS, NRF_SO_GNSS_FIX_INTERVAL, &fix_interval, sizeof(fix_interval));	

	
	if (ctrl == GNSS_INIT_AND_START) {
		gnss_fd = nrf_socket(NRF_AF_LOCAL,
				     NRF_SOCK_DGRAM,
				     NRF_PROTO_GNSS);

		if (gnss_fd >= 0) {
			LOG_INF("GPS Socket created\n");
		} else {
			LOG_ERR("Could not init socket (err: %d)\n", gnss_fd);
			return -1;
		}

		retval = nrf_setsockopt(gnss_fd,
					NRF_SOL_GNSS,
					NRF_SO_GNSS_FIX_RETRY,
					&fix_retry,
					sizeof(fix_retry));
		if (retval != 0) {
			LOG_ERR("Failed to set fix retry value\n");
			return -1;
		}

		retval = nrf_setsockopt(gnss_fd,
					NRF_SOL_GNSS,
					NRF_SO_GNSS_FIX_INTERVAL,
					&fix_interval,
					sizeof(fix_interval));
		if (retval != 0) {
			LOG_ERR("Failed to set fix interval value\n");
			return -1;
		}

		retval = nrf_setsockopt(gnss_fd,
					NRF_SOL_GNSS,
					NRF_SO_GNSS_NMEA_MASK,
					&nmea_mask,
					sizeof(nmea_mask));
		if (retval != 0) {
			LOG_ERR("Failed to set nmea mask\n");
			return -1;
		}
		
		retval = nrf_setsockopt(gnss_fd,
					NRF_SOL_GNSS,
					NRF_SO_GNSS_ELEVATION_MASK,
					&gnss_lowelev,
					sizeof(gnss_lowelev));
		if (retval != 0) {
			LOG_ERR("Failed to set low elevation mask\n");
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
			LOG_ERR("Failed to start GPS\n");
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
			LOG_ERR("Failed to stop GPS\n");
			return -1;
		}
	}

	return 0;
}


void sync_files(){
	if (confdata.upload == 1){
		int led_status_old=led_status;
		led_status=LED_UPLOADING;	
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
				gnss_ctrl(GNSS_STOP);
				activate_lte(true);
				LOG_INF("Established LTE link\n");
				lte_active=true;
			}
			(void)get_sd_data_path(lz4fullfile,lz4file);
			printk("Uploading lz4file found %s\n",lz4fullfile);
			if(webdavUploadFile(lz4fullfile,&confdata) == UPLOADCLNT_SUCCESS){
				/*rename file */
				LOG_INF("Sucessfully uploaded file %s, renaming",log_strdup(lz4file));
				strcpy(lz4renamed,lz4fullfile);
				strcat(lz4renamed,"_ok");
				fs_rename(lz4fullfile,lz4renamed);
			}else{
				LOG_INF("cannot currently upload file %s, trying later",log_strdup(lz4file));
			}
		}



		(void) lsdir_close(&dirp);	


		if (lte_active){
			LOG_INF("Closing LTE link and restarting GNSS\n");
			activate_lte(false);
			k_sleep(K_MSEC(1000));
			gnss_ctrl(GNSS_RESTART);
			k_sleep(K_MSEC(1000));
			lte_active=false;
		}

		
		/*char uploadfilename[100];*/

		/*[>temporary dummy file to upload<]*/
		/*if(get_sd_config_path(uploadfilename,"status.json")!= FEA_SUCCESS){*/
			/*LOG_ERR("Cannot set uploadfile");*/
		/*}*/
		/*[> write status as JSON to file<]*/
		/*if(write_status(uploadfilename,&dev_status) != CONF_SUCCESS){*/
			/*LOG_ERR("Cannot write uploadfile");*/
		/*}*/
		/*[>end temporary dummy file<]*/

		/*if(uploadFile(uploadfilename,&confdata) != UPLOADCLNT_SUCCESS){*/
			/*LOG_ERR("Failed to upload file,continuing");*/

		/*}	*/


		/*sync operation ..*/
		led_status=led_status_old;

	}else{
		LOG_INF("Skipping sync due to disabled user options upload");
	}

}

static int init_app(void)
{
	int retval;

	if (setup_modem() != 0) {
		LOG_ERR("Failed to initialize modem\n");
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

	char filenamebase[50];


	/* File potentially needs closing */
	if (lz4fid->isOpen){
		lz4close(lz4fid);
	}

	sprintf(filenamebase,"%s_%04u-%02u-%02u_%02d%02d.lz4",
			confdata.filebase,
			last_pvt.pvt.datetime.year,
			last_pvt.pvt.datetime.month,
			last_pvt.pvt.datetime.day,
			last_pvt.pvt.datetime.hour,
			last_pvt.pvt.datetime.minute);
			
	get_sd_data_path(lz4fid->filename, filenamebase);
	
	LOG_INF("Opening %s %p\n",log_strdup(lz4fid->filename),lz4fid);		
	
	if (lz4open(lz4fid->filename,lz4fid) != LZ4_SUCCESS){
	
		return -1;
	}
	log_timestamp=k_uptime_get();

	return 0;

}

/*static const int fixes_per_log=50;*/
static int fix_counter=0;

int process_gnss_data(nrf_gnss_data_frame_t *gnss_data,lz4streamfile * lz4fid)
{
	int retval;

	retval = nrf_recv(gnss_fd,
			  gnss_data,
			  sizeof(nrf_gnss_data_frame_t),
			  NRF_MSG_DONTWAIT);

	if (retval <= 0){
		/* add a period of mindfullness to wait for more data */
		k_sleep(K_MSEC(600));
	}

	/*if (retval == -1){*/
		/*led_status=LED_ERROR;*/
		/*LOG_ERR("ERROR reading gnss_data frame trying to restart GNSS\n");*/
		
		/*gnss_ctrl(GNSS_STOP);*/
		/*k_sleep(K_MSEC(2000));*/
		
		/*gnss_ctrl(GNSS_RESTART);*/
		/*k_sleep(K_MSEC(2000));*/
		/*return retval;*/
	/*}*/

	if (retval > 0) {

		switch (gnss_data->data_id) {
		case NRF_GNSS_PVT_DATA_ID:
			bool log_rollover =false;
			
			log_rollover = gnss_data->pvt.datetime.day != last_pvt.pvt.datetime.day;
			if (!log_rollover && button_pressed){
				/* force log rollover when button is pressed */
				log_rollover=true;
			}


			/*log_rollover = (fix_counter)%fixes_per_log == 0;*/

			memcpy(&last_pvt,
			       gnss_data,
			       sizeof(nrf_gnss_data_frame_t));
			got_fix = false;
			led_status=LED_SEARCHING;

			if ((gnss_data->pvt.flags &
				NRF_GNSS_PVT_FLAG_FIX_VALID_BIT)
				== NRF_GNSS_PVT_FLAG_FIX_VALID_BIT) {

				got_fix = true;
				led_status=LED_LOGGING;
				fix_timestamp = k_uptime_get();
				++fix_counter;
				if (log_rollover || !lz4fid->isOpen){
					print_housekeeping_data(&last_pvt);
					rollover_lz4log(lz4fid);
					//possibly reset button after log rollover
					button_pressed=false;

#ifdef CONFIG_UPLOAD_CLIENT
					sync_files();
#endif

				}

			}else{
				led_status=LED_SEARCHING;
			}
			break;

		case NRF_GNSS_NMEA_DATA_ID:
			if (got_fix && lz4fid->isOpen){
				/* put nmea data in the lz4log if it's open*/
				
				lz4write(lz4fid,gnss_data->nmea);
			}else if (got_fix && !lz4fid->isOpen){
				/*reopen file (we actually don;t expect to land here nbut just in case*/
				
				rollover_lz4log(lz4fid);
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
			LOG_INF("Established LTE link\n");
			if (open_supl_socket() == 0) {
				LOG_INF("Starting SUPL session\n");
				supl_session(&gnss_data->agps);
				LOG_INF("Done\n");
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
		LOG_ERR("Failed to send AGNSS data, type: %d (err: %d)\n",
		       type,
		       errno);
		return -1;
	}

	LOG_INF("Injected AGPS data, flags: %d, size: %d\n", type, agps_size);

	return 0;
}
#endif

void print_deviceinfo(void){
	char modeminfostring [20];
	/* print some modem info */
	if(modem_info_init() != 0){
		LOG_ERR("error initializing modem info");
		return;
	}
	
	if(modem_info_string_get(MODEM_INFO_FW_VERSION, modeminfostring,sizeof(modeminfostring))< 0){
		LOG_ERR("cannot retrieve modem version number");
			
	}
	
	LOG_INF("modem version %s\n",log_strdup(modeminfostring));

	if(modem_info_string_get(MODEM_INFO_ICCID, modeminfostring,sizeof(modeminfostring))< 0){
		LOG_ERR("cannot retrieve ESIM ICCID");
			
	}
	LOG_INF("ESIM ICCID %s\n",log_strdup(modeminfostring));
	
	if(modem_info_string_get(MODEM_INFO_IMEI, modeminfostring,sizeof(modeminfostring))< 0){
		LOG_ERR("cannot retrieve module IMEI");
			
	}
	LOG_INF("DEVICE IMEI %s\n",log_strdup(modeminfostring));


}




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

	LOG_INF("Mounting and initializing featherwing sdcard\n");

	if (mount_sdcard() != FEA_SUCCESS){
		led_status=LED_ERROR;
		return -1;
	}

	if (initialize_sdcard_files() != FEA_SUCCESS){
		led_status=LED_ERROR;
		return -1;
	}

	LOG_INF("conf data before %p %s\n",&confdata,log_strdup(confdata.filebase));
	/* read configuration */
	if (read_config(&confdata) != CONF_SUCCESS){
		led_status=LED_ERROR;
		return -1;
	}

	print_deviceinfo();

	/*initialize status*/
	strcpy(dev_status.device_id,confdata.filebase);
	dev_status.uptime=k_uptime_get()/(MSEC_PER_SEC*3600.0);
	dev_status.longitude=0.0;
	dev_status.height=0.0;
	dev_status.latitude=0.0;

#ifdef CONFIG_UPLOAD_CLIENT
	/* register TLS certificate in the modem */
	if (confdata.webdav.usetls == 1){
		if (cert_provision(confdata.webdav.tlscert) != UPLOADCLNT_SUCCESS){
			led_status=LED_ERROR;
			return -1;
		}
	}

#endif
	LOG_INF("Starting GNSS-R logger application\n");

	if (init_app() != 0) {
		led_status=LED_ERROR;
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
		
	LOG_INF("Getting GNSS data...\n");
	last_pvt.pvt.datetime.day=0; ///Ensure that log will be rotated on first passs throug
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



	}

	return 0;
}
