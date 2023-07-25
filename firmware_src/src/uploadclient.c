/* Copyright 2022, Roelof Rietbroek (r.rietbroek@utwente.nl)
 * License: see license file
 * Tools to work with webdav uploads
 */


#include "uploadclient.h"
#include <string.h>
#include <nrf_socket.h>
#include <net/socket.h>
#include <net/tls_credentials.h>
#include <net/http_client.h>
#include <logging/log.h>
#include <fs/fs.h>
#include <stdio.h>
#include "featherw_datalogger.h"

LOG_MODULE_DECLARE(GNSSR,CONFIG_GNSSR_LOG_LEVEL);

#define TLS_SEC_TAG 42

#define HTTP_PORT 80
#define HTTPS_PORT 443
/* mimic later status codes from https://docs.zephyrproject.org/apidoc/latest/group__http__status__codes.html*/
enum http_status{
	HTTP_100_CONTINUE = 100 , HTTP_101_SWITCHING_PROTOCOLS = 101 , HTTP_102_PROCESSING = 102 , HTTP_103_EARLY_HINTS = 103 ,
  HTTP_200_OK = 200 , HTTP_201_CREATED = 201,HTTP_404_NOT_FOUND = 404 };

static enum http_status last_http_status=HTTP_404_NOT_FOUND;

static const char certfx[] = {
#include "../cert/GeantCA.pem"
};

#define MAX_RECV_BUF_LEN 256
static uint8_t recv_buf_ipv4[MAX_RECV_BUF_LEN];

#define UPLOAD_BUF_LEN 512
static uint8_t upload_buffer[UPLOAD_BUF_LEN];


static nrf_sec_cipher_t ciphersuites_list [] ={0xC02B,0xC030, 0xC02F, 0xC024, 0xC00A, 0xC023, 0xC009, 0xC014, 0xC027, 0xC013,0x008D,0x00AE,0x008C,0xC0A8,0x00FF};

/*static uint16_t ciphersuites[] = {*/
	/*{ "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384", 0xC024 },*/
	/*{ "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA", 0xC00A },*/
	/*{ "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256", 0xC023 },*/
	/*{ "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA", 0xC009 },*/
	/*{ "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA", 0xC014 },*/
	/*{ "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256", 0xC027 },*/
	/*{ "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA", 0xC013 },*/
	/*{ "TLS_PSK_WITH_AES_256_CBC_SHA", 0x008D },*/
	/*{ "TLS_PSK_WITH_AES_128_CBC_SHA256", 0x00AE },*/
	/*{ "TLS_PSK_WITH_AES_128_CBC_SHA", 0x008C },*/
	/*{ "TLS_PSK_WITH_AES_128_CCM_8", 0xC0A8 },*/
	/*{ "TLS_EMPTY_RENEGOTIATIONINFO_SCSV", 0x00FF },*/
/*#if defined(CONFIG_EXTENDED_CIPHERSUITE_LIST)*/
	/*{ "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256", 0xC02B },*/
	/*{ "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384", 0xC030 },*/
	/*{ "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256", 0xC02F },*/
/*#endif*/
/*};*/

/* Provision certificate to modem */
int cert_provision(const char *cacert)
{
	
	if ( IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS)){
		LOG_INF("Registering TLS certificate in the modem\n");
		size_t calen=strlen(certfx);
		/*size_t calen=strlen(cacert);*/

		int ret = tls_credential_add(TLS_SEC_TAG,
						 TLS_CREDENTIAL_CA_CERTIFICATE,
						 certfx,
						 calen);
		if (ret < 0) {
			LOG_ERR("Failed to register public certificate: %d",ret);
				return UPLOADCLNT_ERROR;
		}
	}
	return UPLOADCLNT_SUCCESS;

}


/* function is needed to keep pushing data in a socket as the send call may only do thi partially*/
static ssize_t sendall(int sock, const void *buf, size_t len)
{
	while (len) {
		ssize_t out_len = send(sock, buf, len, 0);

		if (out_len < 0) {
			return -errno;
		}

		buf = (const char *)buf + out_len;
		len -= out_len;
	}

	return 0;
}




/* Number of getaddrinfo attempts */
#define GAI_ATTEMPT_COUNT  3

static int http_fd;

int tls_setup(int fd, const char * hostname)
{
	int err;
	int verify;

	/* Security tag that we have provisioned the certificate with */
	const sec_tag_t tls_sec_tag[] = {
		TLS_SEC_TAG,
	};

	/* Set up TLS peer verification */
	verify = TLS_PEER_VERIFY_REQUIRED;

	err = setsockopt(fd, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));
	if (err) {
		printk("Failed to setup peer verification, err %d, %s\n", errno, strerror(errno));
		return err;
	}

	/* Associate the socket with the security tag
	 * we have provisioned the certificate with.
	 */
	err = setsockopt(fd, SOL_TLS, TLS_SEC_TAG_LIST, tls_sec_tag, sizeof(tls_sec_tag));
	if (err) {
		printk("Failed to setup TLS sec tag, err %d, %s\n", errno, strerror(errno));
		return err;
	}

	err = setsockopt(fd, SOL_TLS, TLS_HOSTNAME, hostname, strlen(hostname));
	if (err) {
		printk("Failed to setup TLS hostname, err %d, %s\n", errno, strerror(errno));
		return err;
	}
	return 0;
}


int open_http_socket(const char * hostname,int usetls)
{
	int err = UPLOADCLNT_ERROR;
	int proto;
	int gai_cnt = 0;
	uint16_t port;
	struct addrinfo *addr;
	struct addrinfo *info;

	
	if (usetls && IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS)) {
		proto = IPPROTO_TLS_1_2;
		port = htons(HTTPS_PORT);
	}else{
		proto = IPPROTO_TCP;
		port = htons(HTTP_PORT);
	}

	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = proto,
		/* Either a valid,
		 * NULL-terminated access point name or NULL.
		 */
		.ai_canonname = NULL,
	};

	/* Try getaddrinfo many times, sleep a bit between retries */
	do {
		err = getaddrinfo(hostname, NULL, &hints, &info);
		gai_cnt++;

		if (err) {
			if (gai_cnt < GAI_ATTEMPT_COUNT) {
				/* Sleep between retries */
				k_sleep(K_MSEC(1000 * gai_cnt));
			} else {
				/* Return if no success after many retries */
				LOG_ERR("Failed to resolve hostname %s on IPv4, errno: %d)\n",
					log_strdup(hostname), errno);

				return UPLOADCLNT_ERROR;
			}
		}
	} while (err);

	/* Create socket */
	http_fd = socket(AF_INET, SOCK_STREAM, proto);
	if (http_fd < 0) {
		LOG_ERR("Failed to create socket, errno %d\n", errno);
		goto cleanup;
	}

	struct timeval timeout = {
		.tv_sec = 1,
		.tv_usec = 0,
	};

	err = setsockopt(http_fd,
			 NRF_SOL_SOCKET,
			 NRF_SO_RCVTIMEO,
			 &timeout,
			 sizeof(timeout));
	if (err) {
		LOG_ERR("Failed to setup socket timeout, errno %d\n", errno);
		err=-errno;
		return err;
	}

	if(usetls && IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS)){
		sec_tag_t sec_tag_list[] = {
			TLS_SEC_TAG,
		};

	
		err = setsockopt(http_fd, SOL_TLS, TLS_SEC_TAG_LIST,
				 sec_tag_list, sizeof(sec_tag_list));
		if (err < 0) {
			LOG_ERR("Failed to set TLS secure option (%d)", -errno);
			err = -errno;
			return err;
		}

		err = setsockopt(http_fd, SOL_TLS, TLS_HOSTNAME,
				 hostname,
				 strlen(hostname));
		if (err < 0) {
			LOG_ERR("Failed to set TLS_HOSTNAME "
				"option (%d)", -errno);
			err = -errno;
			return err;
		}
	}
	
	/* Not connected */
	err = UPLOADCLNT_ERROR;

	for (addr = info; addr != NULL; addr = addr->ai_next) {
		struct sockaddr *const sa = addr->ai_addr;

		switch (sa->sa_family) {
		case AF_INET6:
			((struct sockaddr_in6 *)sa)->sin6_port = port;
			LOG_INF("ipv6 found\n");
			break;
		case AF_INET:
			((struct sockaddr_in *)sa)->sin_port = port;
			char ip[255] = { 0 };

			inet_ntop(NRF_AF_INET,
				  (void *)&((struct sockaddr_in *)
				  sa)->sin_addr,
				  ip,
				  255);
			LOG_INF("ipv4 %s (%x) port %d\n",
				log_strdup(ip),
				((struct sockaddr_in *)sa)->sin_addr.s_addr,
				ntohs(port));
			break;
		}


		err = setsockopt(http_fd, SOL_TLS, TLS_CIPHERSUITE_LIST, ciphersuites_list,
				 sizeof(ciphersuites_list));
		if (err < 0) {
			LOG_ERR("Failed to set TLS ciphersuites (%d)", -errno);
			err = -errno;
			continue;
		}
		


		err = connect(http_fd, sa, addr->ai_addrlen);
		if (err) {
			/* Try next address */
			LOG_ERR("Unable to connect, errno %d\n", errno);
			err=-errno;
		} else {
			/* Successfully Connected! */
			return UPLOADCLNT_SUCCESS;
		}
	}
	return err;

cleanup:
	freeaddrinfo(info);

	if (err) {
		/* Unable to connect, close socket */
		close(http_fd);
		http_fd = -1;
	}

	return UPLOADCLNT_ERROR;
}

int close_http_socket(void)
{
	if (close(http_fd) < 0) {
		LOG_ERR("Failed to close HTTP(S) socket\n");
		return UPLOADCLNT_ERROR;
	}
	http_fd=-1;
	return UPLOADCLNT_SUCCESS;

}


static int upload_cb(int sock, struct http_request *req, void *user_data)
{
	
	/* user data is the filename, so make sure to cast is as a char array */
	const char* filename =(const char*) user_data;
	
	struct fs_file_t fid;
	ssize_t retc;
	ssize_t nsend=0;
	LOG_INF("Uploading file %s\n",log_strdup(filename));
	/* filecontent is loaded in upload_buffer at position CHUNK_SHIFT*/
	/* open file for reading */
	if ( fs_open(&fid,filename,FS_O_READ)!=0){
		LOG_ERR("cannot open file %s for reading",log_strdup(filename));
		return UPLOADCLNT_ERROR;
	}
	
	do {
		/*read up to UPLOAD_BUF_LEN bytes from file*/
		retc=fs_read(&fid,upload_buffer,UPLOAD_BUF_LEN);

		if ( retc > 0){
			LOG_DBG("sending %d bytes\n",retc);
			if( sendall(sock,upload_buffer,retc) != 0){
				LOG_ERR("Error sending bytes");
				return UPLOADCLNT_ERROR;
			}
			nsend+=retc;		
		}else if( retc <0 ){

			LOG_ERR("Error uploading file");
			return UPLOADCLNT_ERROR;

		}

	}while(retc  == UPLOAD_BUF_LEN);


	/*signal end of data */
	/*int nfinal=sprintf(upload_buffer,HTTP_CRLF);*/
	
	/*nsend+=send(sock, upload_buffer, nfinal, 0);*/

	fs_close(&fid);
	LOG_INF("Body send  was %d bytes",nsend);
	return nsend;
}


static void response_cb(struct http_response *rsp,
			enum http_final_call final_data,
			void *user_data)
{
	if (final_data == HTTP_DATA_MORE) {
		LOG_INF("Partial data received (%zd bytes)", rsp->data_len);
	} else if (final_data == HTTP_DATA_FINAL) {
		LOG_INF("All the data received (%zd bytes)", rsp->data_len);
	}


	
	/*LOG_INF("Response to uploading file %s\n", (const char *)log_strdup(user_data));*/
	if (strcmp(rsp->http_status,"Created") == 0){
		last_http_status=HTTP_201_CREATED;
	}

	LOG_INF("Response status %s\n", log_strdup(rsp->http_status));	

	printk("%s\n",rsp->recv_buf);
}

int webdavUploadFile(const char * filename,const struct config * conf){
	
	
	size_t filesize=file_size(filename);


	if( filesize == 0){
		LOG_ERR("Not uploading %s, zero size or not existent",log_strdup(filename));
		return UPLOADCLNT_ERROR;
	}

	/*return UPLOADCLNT_ERROR;*/

	char contentlenstr[40];
	int ncontentwritten=snprintk(contentlenstr,sizeof(contentlenstr),"Content-Length: %zd \r\n",filesize);
	
	/*printk("%s\n",contentlenstr);*/
	
	if (open_http_socket(conf->webdav.host,conf->webdav.usetls) != UPLOADCLNT_SUCCESS){
		LOG_ERR("Cannot open http(s) socket");
		return UPLOADCLNT_ERROR;
	}
	/*note final entry of the headers array must be  NULL so always allocate one more than needed*/
	char *headers[] = {
			"User-Agent: gnss-ir/1.0 (Actinius-icarus)\r\n",
			NULL,
			NULL,
			NULL
		};

	/*insert authentication header at the first header slot*/
	headers[1]=conf->webdav.auth;

	/* We need to set the Content-Length header*/
	headers[2]=contentlenstr;
	/*construct target url which also contains the filename*/
	char * basename=strrchr(filename,'/');	
	char url[100] = {0};
	(void) strcpy(url,conf->webdav.url);
	(void) strcpy(url+strlen(url),basename);

	struct http_request req;

	int32_t req_timeout = 300 * MSEC_PER_SEC;
	memset(&req, 0, sizeof(req));

	req.method = HTTP_PUT;
	req.url = url;
	req.host = conf->webdav.host;
	req.protocol = "HTTP/1.1";
	/*req.content_type_value="application/x-www-form-urlencoded";*/
	req.header_fields=headers;
	/*req.optional_headers=headers;*/
	req.payload_cb = upload_cb;
	/*payload_len needs to be zero otherwise a too large internal buffer will allocated*/
	req.payload_len = 0;
	req.response = response_cb;
	req.recv_buf = recv_buf_ipv4;
	req.recv_buf_len = MAX_RECV_BUF_LEN;
	
	int ret = http_client_req(http_fd, &req, req_timeout, (void*) filename);

	if (ret < 0){
		LOG_ERR("Did not succeed in http request (timeout?) errno %d",ret);
		return UPLOADCLNT_ERROR;

	}


	if (close_http_socket() != UPLOADCLNT_SUCCESS){
		LOG_ERR("Cannot properly close http(s) socket");
		return UPLOADCLNT_ERROR;
	}
	
	if (last_http_status != HTTP_201_CREATED){
		LOG_ERR("Did not succeed to create resource");
		return UPLOADCLNT_ERROR;
	}

	return UPLOADCLNT_SUCCESS;

}


