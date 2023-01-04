
/* Copyright 2022, Roelof Rietbroek (r.rietbroek@utwente.nl)
 * License: see license file
 * read and write configuration in json format
 */

#include <cJSON.h>
#include "config.h"
#include "featherw_datalogger.h"
#include <fs/fs.h>
#include <string.h>

/* temporary workaround using the logger*/
#define LOG_ERR(...) printk(__VA_ARGS__)
#define LOG_DBG(...) printk(__VA_ARGS__)
#define LOG_INF(...) printk(__VA_ARGS__)

#define JSONBUFLEN 300

/*static const struct json_obj_descr webdav_descr[] = {*/
	/*JSON_OBJ_DESCR_PRIM(struct webdav_config, rooturl, JSON_TOK_STRING),*/
	/*JSON_OBJ_DESCR_PRIM(struct webdav_config, user, JSON_TOK_STRING),*/
	/*JSON_OBJ_DESCR_PRIM(struct webdav_config, passw, JSON_TOK_STRING),*/
/*};*/

/*static const struct json_obj_descr config_descr[] = {*/
	/*JSON_OBJ_DESCR_PRIM(struct config, filebase, JSON_TOK_STRING),*/
	/*JSON_OBJ_DESCR_PRIM(struct config, upload, JSON_TOK_NUMBER),*/
	/*JSON_OBJ_DESCR_OBJECT(struct config, webdav, webdav_descr),*/
/*};*/

/*static struct config config_data={"icarus_gnssr0",0,{"https://changeme/files/remote.php/nonshib-webdav","username","passw"}};*/

void set_defaults(struct config * conf){
	strcpy(conf->filebase,"icarus_gnssr0");
	conf->upload=0;
	strcpy(conf->webdav.rooturl,"https://changeme/files/remote.php/nonshib-webdav");
	strcpy(conf->webdav.user,"changemeuser");
	strcpy(conf->webdav.passw,"changepassw");
}

int read_config(struct config *conf){
	char configfile[100];
	if(get_sd_config_path(configfile,"config.json")!= FEA_SUCCESS){
		return CONF_ERR;
	}
	
	struct fs_file_t fid;
	char jsonbuf[JSONBUFLEN];
	
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

		cJSON * filebase=cJSON_GetObjectItemCaseSensitive(monitor,"filebase");

		strcpy(conf->filebase,filebase->valuestring);
		
		/* get webdav items */
		cJSON * webdav=cJSON_GetObjectItemCaseSensitive(monitor,"webdav");
		
		cJSON * rooturl=cJSON_GetObjectItemCaseSensitive(webdav,"rooturl");
		strcpy(conf->webdav.rooturl,rooturl->valuestring);
		
		cJSON * user=cJSON_GetObjectItemCaseSensitive(webdav,"user");
		strcpy(conf->webdav.user,user->valuestring);
		
		cJSON * passw=cJSON_GetObjectItemCaseSensitive(webdav,"passw");
		strcpy(conf->webdav.passw,passw->valuestring);



		cJSON_Delete(monitor);

		///
		/*int ret = json_obj_parse(jsonbuf,buflen,config_descr,ARRAY_SIZE(config_descr),conf);*/
		/*LOG_INF("pointer to conf after %p\n",conf);*/

		/*if (ret != expected_return_code){*/
			/*LOG_ERR("ERROR parsing JSON configuration");*/
			/*return CONF_ERR;*/
		/*}*/
	}else{
		/* write defaults to file */
		LOG_INF("Writing defaults to configfile %s",configfile);
		set_defaults(conf);		
		
		cJSON * monitor = cJSON_CreateObject();
		cJSON_AddNumberToObject(monitor,"upload",conf->upload);
		cJSON_AddStringToObject(monitor,"filebase",conf->filebase);
		
		/* add webdav items */
		cJSON * webdav= cJSON_CreateObject();

		cJSON_AddItemToObject(monitor, "webdav", webdav);
		
		cJSON_AddStringToObject(webdav,"rooturl",conf->webdav.rooturl);
		cJSON_AddStringToObject(webdav,"user",conf->webdav.user);
		cJSON_AddStringToObject(webdav,"passw",conf->webdav.passw);
		

		/* print json to string */
		int retcode= cJSON_PrintPreallocated(monitor,jsonbuf,JSONBUFLEN,1);


		cJSON_Delete(monitor);
		/*LOG_INF("Set defaults to conf %p  %s\n",conf,conf->filebase);*/
		/*int retcode= json_obj_encode_buf(config_descr, ARRAY_SIZE(config_descr), conf, jsonbuf,JSONBUFLEN);*/
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


