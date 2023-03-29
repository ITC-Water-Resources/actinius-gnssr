
#define CONF_SUCCESS 0

#define CONF_ERR 1

#ifndef CONFIG_H
#define CONFIG_H

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
	int upload;
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
	float height;
};

int write_status(const char *file, const struct device_status * status);

#endif
