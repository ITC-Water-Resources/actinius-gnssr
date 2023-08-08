
/* Copyright 2022, Roelof Rietbroek (r.rietbroek@utwente.nl)
 * License: see license file
 * read and write configuration in json format
 */

#include <cJSON.h>
#include "config.h"
#include "featherw_datalogger.h"
#include <fs/fs.h>
#include <string.h>
#include <sys/base64.h>
#include <logging/log.h>
LOG_MODULE_DECLARE(GNSSR,CONFIG_GNSSR_LOG_LEVEL);

#define JSONBUFLEN 2400

#define AUTH_PREFIX "Authorization: Basic "

static char jsonbuf[JSONBUFLEN];

void set_defaults(struct config * conf){
	strcpy(conf->filebase,"icarus_gnssr0");
	conf->upload=1;
#ifdef CONFIG_UPLOAD_CLIENT
	strcpy(conf->webdav.host,"httpbin.org");
	strcpy(conf->webdav.url,"/put");
	strcpy(conf->webdav.auth,"testuser:testpassword");

	conf->webdav.usetls=1;
	/* note below is the root certificate used by httpbin.org, change this for your own, and do include the line ends (you can also provide your own in the config.json file*/
	strcpy(conf->webdav.tlscert,"-----BEGIN CERTIFICATE-----\nMIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\nADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\nb24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\nMAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\nb3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\nca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\nIFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\nVOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\njgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\nAYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\nA4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\nU5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\nN+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\no/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\nrqXRfboQnoZsG4q5WTP468SQvvG5\n-----END CERTIFICATE-----\n");


#endif

}

int read_config(struct config *conf){
	char configfile[100];
	if(get_sd_config_path(configfile,"config.json")!= FEA_SUCCESS){
		return CONF_ERR;
	}
	
	struct fs_file_t fid;
	
	if (file_exists(configfile)){
		LOG_INF("Reading config from %s\n",log_strdup(configfile));
		/* read from file */
		if ( fs_open(&fid,configfile,FS_O_READ)!=0){
			LOG_ERR("cannot open configfile %s for reading",log_strdup(configfile));
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
		LOG_INF("%d %d %d %s\n",sizeof(conf->webdav.auth),prefixlen,strlen(auth->valuestring),log_strdup(auth->valuestring));
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
		LOG_INF("base64 encoded Authentication header:\n%d %s\n",nwr,log_strdup(conf->webdav.auth));

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
		}
#endif

		cJSON_Delete(monitor);

	}else{
		/* write defaults to file */
		LOG_INF("Writing defaults to configfile %s",configfile);
		set_defaults(conf);		
		
		cJSON * monitor = cJSON_CreateObject();
		cJSON_AddNumberToObject(monitor,"upload",conf->upload);
		cJSON_AddStringToObject(monitor,"filebase",conf->filebase);
		
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
		if ( fs_open(&fid,configfile,FS_O_WRITE|FS_O_CREATE)!=0){
			LOG_ERR("cannot open configfile %s",configfile);
			return CONF_ERR;

		}
		
		fs_write(&fid,jsonbuf, strlen(jsonbuf));
		fs_close(&fid);
	}

	return CONF_SUCCESS;
}



int get_jsonstatus(char *jsonbuffer, int buflen, const struct device_status * status){
		cJSON * monitor = cJSON_CreateObject();
		cJSON_AddStringToObject(monitor,"device_id",status->device_id);
		cJSON_AddNumberToObject(monitor,"uptime",status->uptime);
		cJSON_AddNumberToObject(monitor,"longitude",status->longitude);
		cJSON_AddNumberToObject(monitor,"latitude",status->latitude);
		cJSON_AddNumberToObject(monitor,"altitude",status->altitude);
		cJSON * bat_array = cJSON_AddArrayToObject(monitor, "battery_mvolt");
		for (int i =0 ;i<24; i++){
			cJSON *batmvolt=cJSON_CreateNumber(status->battery_mvolt[i]);
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

		/*struct fs_file_t fid;*/
		
		/*[>[> write to file <]<]*/
		/*if ( fs_open(&fid,file,FS_O_WRITE|FS_O_CREATE)!=0){*/
			/*LOG_ERR("cannot open statusfile %s",file);*/
			/*return CONF_ERR;*/

		/*}*/
		
		/*fs_write(&fid,jsonbuf, strlen(jsonbuf));*/
		/*fs_close(&fid);*/

		return CONF_SUCCESS;
}
