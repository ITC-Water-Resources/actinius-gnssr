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


/*#define LOG_ERR(x) fprintf(stderr,"%s",x)*/
LOG_MODULE_DECLARE(main);





/*
 * CHUNKSIZE (maximum size of the input src data)
*/

#define CHUNKSIZE 1024

/*
 * Struct holding the administrative parts of an open lz4stream
 */
typedef struct lz4streamfile {
	LZ4F_compressionContext_t ctx;
	FILE* fid;
	size_t cap;
	char * destbuf;
} lz4streamfile;


static const LZ4F_preferences_t kPrefs = {
    {LZ4F_max64KB, LZ4F_blockLinked, LZ4F_contentChecksumEnabled, LZ4F_frame,
      0 /* unknown content size */, 0 /* no dictID */ , LZ4F_noBlockChecksum },
    0,   /* compression level; 0 == default */
    0,   /* autoflush */
    0,   /* favor decompression speed */
    { 0, 0, 0 },  /* reserved, must be set to 0 */
};




lz4streamfile* lz4open(const char * path){
	

	/* Dynamic allocation of buffers*/


	lz4streamfile *lz4id = malloc(sizeof(lz4streamfile)); 
	if (lz4id == NULL){
		LOG_ERR("Cannot allocate lz4 admin struct");
		return NULL;
	}
	
	lz4id->fid= fopen(path, "wb");
	if (lz4id->fid == NULL){
		free(lz4id);
		lz4id=NULL;
	}
	

	lz4id->cap = LZ4F_compressBound(CHUNKSIZE, &kPrefs);   /* large enough for any input <= IN_CHUNK_SIZE */
	assert(lz4id->cap > LZ4F_HEADER_SIZE_MAX);
	lz4id->destbuf = malloc(lz4id->cap);

	///Setup Frame header
	size_t const ctxCreation = LZ4F_createCompressionContext(&(lz4id->ctx), LZ4F_VERSION);
	
    	{
		size_t const headerSize = LZ4F_compressBegin(lz4id->ctx, lz4id->destbuf, lz4id->cap, &kPrefs);
        	if (LZ4F_isError(headerSize)) {
            		return NULL;
        	}
       		
		//write the frameheader to the output file
		const size_t written=fwrite(lz4id->destbuf, 1, headerSize, lz4id->fid);
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

	if (nwritten > 0){

		fwrite(lz4id->destbuf,1,nwritten,lz4id->fid);	
	}

	return LZ4_SUCCESS;
}

int lz4close(lz4streamfile * lz4id){
	//possibly write pending data (this will be done by adding a NULL pointer as the data
	lz4write(lz4id,NULL);	
	fclose(lz4id->fid);
	LZ4F_freeCompressionContext(lz4id->ctx);
	free(lz4id->destbuf);
	free(lz4id);
	lz4id=NULL;

	return 0;

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



