
#define CONF_SUCCESS 0

#define CONF_ERR 1

struct webdav_config {
	char rooturl[200];
	char user[20];
	char passw[20];
#ifdef CONFIG_UPLOAD_CLIENT
	char tlscert[2200];
#endif

};


struct config {
	char filebase[20];
	int upload;
	struct webdav_config webdav;
};


int read_config(struct config *conf);
