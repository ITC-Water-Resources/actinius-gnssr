/*
* Copyright (c) 2022 Roelof Rietbroek <r.rietbroek@utwente.nl>
*
* SPDX-License-Identifier: Apache-2.0
*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lz4frame_static.h"
#include <assert.h>
#include <logging/log.h>
#include "lz4file.h"
#include <fs/fs.h>
#include <fs/fs_interface.h>
/*#define LOG_ERR(x) fprintf(stderr,"%s",x)*/
LOG_MODULE_DECLARE(GNSSR);



/*
 * CHUNKSIZE (maximum size of the input src data)
*/

#define CHUNKSIZE 1024
/* buffer size determined by trial and error */
#define BUFFERSIZE 65544
/*
 * Struct holding the administrative parts of an open lz4stream
 */
typedef struct lz4streamfile {
	LZ4F_compressionContext_t ctx;
	struct fs_file_t * fid;
	size_t cap;
	char destbuf[BUFFERSIZE];
} lz4streamfile;


static const LZ4F_preferences_t kPrefs = {
    {LZ4F_max64KB, LZ4F_blockIndependent, LZ4F_noContentChecksum, LZ4F_frame,
      0 /* unknown content size */, 0 /* no dictID */ , LZ4F_noBlockChecksum },
    -1,   /* compression level; 0 == default. use -1 to avoid HC calls which use more memory*/
    0,   /* autoflush */
    0,   /* favor decompression speed */
    { 0, 0, 0 },  /* reserved, must be set to 0 */
};


int handle_lz4error(size_t errcode){

        	if (LZ4F_isError(errcode)) {
			const char * errmessage = LZ4F_getErrorName(errcode);
			LOG_ERR("Failed to create LZ4 frame header:%s",errmessage);
            		return -1;
        	}else{
			return 0;
		}

}

lz4streamfile* lz4open(const char * path){
	

	/* Dynamic allocation of buffers*/


	lz4streamfile *lz4id =malloc(sizeof(lz4streamfile)); 
	if (lz4id == NULL){
		LOG_ERR("Cannot allocate lz4 admin struct");
		return NULL;
	}
	lz4id->fid=malloc(sizeof(struct fs_file_t));
	fs_file_t_init(lz4id->fid);
	if ( fs_open(lz4id->fid,path,FS_O_WRITE|FS_O_CREATE)!=0){
		free(lz4id);
		lz4id=NULL;
		LOG_ERR("Cannot open lz4 output file");
		return NULL;
	}
	

	lz4id->cap = LZ4F_compressBound(CHUNKSIZE, &kPrefs);   /* large enough for any input <= IN_CHUNK_SIZE */
	LOG_DBG("Buffer size needed %d allocated %d\n",lz4id->cap,BUFFERSIZE);
	assert(LZ4F_HEADER_SIZE_MAX <= lz4id->cap);
	assert(lz4id->cap <= BUFFERSIZE);

	///Setup Frame header
	size_t const ctxCreation = LZ4F_createCompressionContext(&(lz4id->ctx), LZ4F_VERSION);
	
    	{
		size_t const headerSize = LZ4F_compressBegin(lz4id->ctx, lz4id->destbuf, lz4id->cap, &kPrefs);
        	if (handle_lz4error(headerSize)) {
            		return NULL;
        	}
       		
		//write the frameheader to the output file
		const size_t written=fs_write(lz4id->fid,lz4id->destbuf, headerSize);
		if (handle_lz4error(written)){
			return NULL;
		}
	}

	return lz4id;
}


int lz4write(lz4streamfile * lz4id,const char * data){
	size_t nwritten=0;
	if (data == NULL){
		///no more data-> end compression
		nwritten = LZ4F_compressEnd(lz4id->ctx,lz4id->destbuf,lz4id->cap,NULL);
		
	}else{
		const int ndata=strlen(data);
		nwritten = LZ4F_compressUpdate(lz4id->ctx,lz4id->destbuf,lz4id->cap,data,ndata,NULL);
	}
	if (handle_lz4error(nwritten)){
		return LZ4_ERR_COMPRESS;
	}
	if (nwritten > 0){
		assert(nwritten < BUFFERSIZE);
		fs_write(lz4id->fid,lz4id->destbuf,nwritten);	
	
	}

	return LZ4_SUCCESS;
}

int lz4close(lz4streamfile * lz4id){
	//possibly write pending data (this will be done by adding a NULL pointer as the data
	lz4write(lz4id,NULL);	
	fs_close(lz4id->fid);
	free(lz4id->fid);
	LZ4F_freeCompressionContext(lz4id->ctx);
	free(lz4id);
	lz4id=NULL;

	return LZ4_SUCCESS;

}



 /*TESTING MAIN function */
/*int main(){*/
	/*char * filename ="orig.lz4";*/
	/*char * inpf="./orig.txt";*/
	/*size_t bufsz=82;*/
	/*char *linebuf=NULL;*/
	/*struct lz4streamfile * lz4id=lz4open(filename);*/
	/*FILE* fin=fopen(inpf,"rt");*/


	/*int line_size = getline(&linebuf, &bufsz, fin);*/
	/*lz4write(lz4id,linebuf);*/
	  /*while (line_size >= 0)*/
	  /*{*/
		    /*line_size = getline(&linebuf, &bufsz, fin);*/
		/*if (line_size >= 0){*/
		
			/*lz4write(lz4id,linebuf);*/
		/*}*/
	/*}*/
	/*fclose(fin);*/
	/*lz4close(lz4id);*/
/*}*/



