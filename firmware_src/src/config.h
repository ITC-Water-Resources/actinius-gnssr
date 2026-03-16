
#include <nrf_modem_gnss.h>



#define CONF_SUCCESS 0

#define CONF_ERR 1

#ifndef CONFIG_H
#define CONFIG_H

#define JSONBUFLEN 3000

#ifdef CONFIG_UPLOAD_CLIENT
struct webdav_config {
	char host[100];
	char url[100];
	char auth[120];
	int usetls;
	char tlscert[2200];

};

#endif

struct config {
	char filebase[20];
	int agps;
	int upload;
	int psm_mode;
	int pvt_low;
#ifdef CONFIG_UPLOAD_CLIENT
	struct webdav_config webdav;
#endif
};


int read_config(struct config *conf);

struct device_status {
	char device_id[20];
	float uptime;
	float longitude;
	float latitude;
	float altitude;
	uint16_t battery_mvolt[24];
};

int get_jsonstatus(char *jsonbuffer, int buflen);

int init_device_status();
int update_device_status(const struct nrf_modem_gnss_pvt_data_frame * pvt);



#endif
