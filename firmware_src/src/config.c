
/* Copyright 2023, Roelof Rietbroek (r.rietbroek@utwente.nl)
 * License: see license file
 * read and write configuration in json format
 */

#include <cJSON.h>
#include "config.h"
#include "featherw_datalogger.h"
#include "led_buttons.h"
#include <zephyr/fs/fs.h>
#include <string.h>
#include <zephyr/sys/base64.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(GNSSR,CONFIG_GNSSR_LOG_LEVEL);


#define AUTH_PREFIX "Authorization: Basic "

struct config confdata;
struct device_status dev_status;

static uint8_t hrprev=24; /*invalid hour so initial battery voltage measurement is always triggered*/

char jsonbuf[JSONBUFLEN];

void set_defaults(struct config * conf){
	strcpy(conf->filebase,"icarus_gnssr0");
	conf->upload=0;
	conf->agps=0;
	conf->psm_mode=0;
	conf->pvt_low=1;
#ifdef CONFIG_SUPL_CLIENT_LIB
	conf->agps=1;
#endif
#ifdef CONFIG_UPLOAD_CLIENT
	conf->upload=1;
	strcpy(conf->webdav.host,"httpbin.org");
	strcpy(conf->webdav.url,"/put");
	strcpy(conf->webdav.auth,"testuser:testpassword");

	conf->webdav.usetls=1;
	/* note below is the root certificate used by httpbin.org, change this for your own, and do include the line ends (you can also provide your own in the config.json file*/
	strcpy(conf->webdav.tlscert,"-----BEGIN CERTIFICATE-----$MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF$ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6$b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL$MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv$b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj$ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM$9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw$IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6$VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L$93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm$jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC$AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA$A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI$U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs$N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv$o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU$5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy$rqXRfboQnoZsG4q5WTP468SQvvG5$-----END CERTIFICATE-----$");
	


#endif

}

void replace_lineend(char * intls)
{
    char oldc='$';
    char newc='\n';
    int i = 0;

    /* Run till end of string */
    while(intls[i] != '\0')
    {
        if(intls[i] == oldc)
        {
            intls[i] = newc;
        }

        i++;
    }
}

int read_config(struct config *conf){
	char configfile[100];
	if(get_sd_config_path(configfile,"config_" CONFIG_GNSSR_VERSION ".json")!= FEA_SUCCESS){
		return CONF_ERR;
	}
	
	struct fs_file_t fid;
	fs_file_t_init(&fid);

	if (file_exists(configfile)){
		LOG_INF("Reading config from %s\n",configfile);
		/* read from file */
		if ( fs_open(&fid,configfile,FS_O_READ)!=0){
			LOG_ERR("cannot open configfile %s for reading",configfile);
			return CONF_ERR;

		}
		ssize_t buflen=fs_gets(jsonbuf,JSONBUFLEN,&fid);
		fs_close(&fid);
		/*int expected_return_code = (1 << ARRAY_SIZE(config_descr)) - 1;*/
			
		LOG_INF("buflen %d\n",buflen);

		/// Parse JSON string
		cJSON * monitor=cJSON_Parse(jsonbuf);


		if (monitor == NULL ){
			LOG_ERR("ERROR parsing JSON configuration");
			return CONF_ERR;
		}

		/// Fill config struct
		cJSON *upload= cJSON_GetObjectItemCaseSensitive(monitor, "upload");

		conf->upload=upload->valueint;
		
		cJSON *psm_mode= cJSON_GetObjectItemCaseSensitive(monitor, "psm_mode");

		conf->psm_mode=psm_mode->valueint;
		
		cJSON *pvt_low= cJSON_GetObjectItemCaseSensitive(monitor, "pvt_low");

		conf->pvt_low=pvt_low->valueint;
		
		cJSON *agps= cJSON_GetObjectItemCaseSensitive(monitor, "agps");

		conf->agps=agps->valueint;

		cJSON * filebase=cJSON_GetObjectItemCaseSensitive(monitor,"filebase");

		strcpy(conf->filebase,filebase->valuestring);
	

#ifdef CONFIG_UPLOAD_CLIENT
		/* get webdav items */
		cJSON * webdav=cJSON_GetObjectItemCaseSensitive(monitor,"webdav");
		
		cJSON * host=cJSON_GetObjectItemCaseSensitive(webdav,"host");
		strcpy(conf->webdav.host,host->valuestring);
		
		cJSON * url=cJSON_GetObjectItemCaseSensitive(webdav,"url");
		strcpy(conf->webdav.url,url->valuestring);
		

		cJSON * auth=cJSON_GetObjectItemCaseSensitive(webdav,"auth");
		size_t nwr=0;
		size_t prefixlen=strlen(AUTH_PREFIX);
		strcpy(conf->webdav.auth,AUTH_PREFIX);
		LOG_INF("%d %d %d %s\n",sizeof(conf->webdav.auth),prefixlen,strlen(auth->valuestring),auth->valuestring);
		(void)base64_encode(&conf->webdav.auth[0]+prefixlen,0,&nwr,auth->valuestring,strlen(auth->valuestring));
		LOG_INF("required base64 buffer size %d\n",nwr);
		if(base64_encode(&conf->webdav.auth[0]+prefixlen,sizeof(conf->webdav.auth)-prefixlen,&nwr,auth->valuestring,strlen(auth->valuestring)) != 0){
			LOG_ERR("ERROR in  base64 encoding of authentication");
			return CONF_ERR;
		}
		/* add a carriage return and line end */
		memcpy(&conf->webdav.auth[0]+prefixlen+nwr,"\r\n\0",3);
		/*strncat(conf->webdav.auth+nwr,"\r\n",2);*/
		printk(conf->webdav.auth);
		LOG_INF("base64 encoded Authentication header:\n%d %s\n",nwr,conf->webdav.auth);

		cJSON *usetls= cJSON_GetObjectItemCaseSensitive(webdav, "usetls");

		if (IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS)) {
			conf->webdav.usetls=usetls->valueint;
		}else{
			/* Always force to zero if firmware does not support TLS */
			if (usetls->valueint == 1){
				LOG_WRN("DISABLING TLS as firmware does not support it");
			}
			conf->webdav.usetls=0;
		}
		
		if ( conf->webdav.usetls == 1){
			cJSON * tlscert=cJSON_GetObjectItemCaseSensitive(webdav,"tlscert");

			if (tlscert == NULL){
				LOG_ERR("ERROR TLS requested but no certificate is given");
				return CONF_ERR;
			}
			strcpy(conf->webdav.tlscert,tlscert->valuestring);
			//replace $ with line endings
			replace_lineend(conf->webdav.tlscert);
		}
#endif

		cJSON_Delete(monitor);

	}else{
		/* write defaults to file */
		LOG_INF("Writing defaults to configfile %s",configfile);
		set_defaults(conf);		
		
		cJSON * monitor = cJSON_CreateObject();
		cJSON_AddNumberToObject(monitor,"upload",conf->upload);
		cJSON_AddNumberToObject(monitor,"agps",conf->upload);
		cJSON_AddNumberToObject(monitor,"psm_mode",conf->psm_mode);
		cJSON_AddNumberToObject(monitor,"pvt_low",conf->pvt_low);
		cJSON_AddStringToObject(monitor,"filebase",conf->filebase);

#ifdef CONFIG_GNSSR_VERSION
		cJSON_AddStringToObject(monitor,"version",CONFIG_GNSSR_VERSION);
#endif

#ifdef CONFIG_GNSSR_CONTACT
		cJSON_AddStringToObject(monitor,"contact",CONFIG_GNSSR_CONTACT);
#endif

#ifdef CONFIG_UPLOAD_CLIENT
		/* add webdav items */
		cJSON * webdav= cJSON_CreateObject();

		cJSON_AddItemToObject(monitor, "webdav", webdav);
		
		cJSON_AddStringToObject(webdav,"host",conf->webdav.host);
		cJSON_AddStringToObject(webdav,"url",conf->webdav.url);
		cJSON_AddStringToObject(webdav,"auth",conf->webdav.auth);

		cJSON_AddNumberToObject(webdav,"usetls",conf->webdav.usetls);
		cJSON_AddStringToObject(webdav,"tlscert",conf->webdav.tlscert);
#endif
		/* print json to string */
		int retcode= cJSON_PrintPreallocated(monitor,jsonbuf,JSONBUFLEN,1);


		cJSON_Delete(monitor);
		if(retcode != 1){
			LOG_ERR("cannot encode in JSON");
			return CONF_ERR;
		}

		/* write to file */
		retcode= fs_open(&fid,configfile,FS_O_WRITE|FS_O_CREATE);
		if ( retcode != 0){
			LOG_ERR("cannot open configfile %s, err %d",configfile,retcode);
			return CONF_ERR;

		}
		
		fs_write(&fid,jsonbuf, strlen(jsonbuf));
		fs_close(&fid);
	}

	return CONF_SUCCESS;
}

int get_jsonstatus(char *jsonbuffer, int buflen){
		cJSON * monitor = cJSON_CreateObject();
		cJSON_AddStringToObject(monitor,"device_id",dev_status.device_id);
		cJSON_AddNumberToObject(monitor,"uptime",dev_status.uptime);
		cJSON_AddNumberToObject(monitor,"longitude",dev_status.longitude);
		cJSON_AddNumberToObject(monitor,"latitude",dev_status.latitude);
		cJSON_AddNumberToObject(monitor,"altitude",dev_status.altitude);
		cJSON * bat_array = cJSON_AddArrayToObject(monitor, "battery_mvolt");
		for (int i =0 ;i<24; i++){
			cJSON *batmvolt=cJSON_CreateNumber(dev_status.battery_mvolt[i]);
			cJSON_AddItemToArray(bat_array, batmvolt);
		}

		/*[> print json to string <]*/
		int retcode= cJSON_PrintPreallocated(monitor,jsonbuffer,buflen,1);


		cJSON_Delete(monitor);
		if(retcode != 1){
			LOG_ERR("cannot encode in JSON");
			return CONF_ERR;
		}
		//add a line end
		strcat(jsonbuffer,"\n");


		return CONF_SUCCESS;
}



int init_device_status(){
		LOG_INF("Initializing device status");
		strcpy(dev_status.device_id,confdata.filebase);
		dev_status.longitude=0.0;
		dev_status.altitude=0.0;
		dev_status.latitude=0.0;

		for(int i=0; i< 24;i++){
			dev_status.battery_mvolt[i]=9999;
		}

		return 0;

}

int update_device_status(const struct nrf_modem_gnss_pvt_data_frame *pvt) {
	///Only do this every hour 
	uint8_t hr=pvt->datetime.hour;
	if(hrprev != hr){
		LOG_INF("Updating device status");
		dev_status.longitude=pvt->longitude;
		dev_status.altitude=pvt->altitude;
		dev_status.latitude=pvt->latitude;
		dev_status.altitude=pvt->altitude;
		
#ifdef CONFIG_ADC
		get_battery_voltage(&dev_status.battery_mvolt[hr]);
		LOG_INF("Battery Voltage %d [mV]",dev_status.battery_mvolt[hr]);
#endif

		hrprev=hr;
	}
	
	///update status
	dev_status.uptime=k_uptime_get()/(MSEC_PER_SEC*3600.0);

	return 0;

}

