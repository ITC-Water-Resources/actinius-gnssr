/*
* Copyright (c) 2022 Roelof Rietbroek <r.rietbroek@utwente.nl>
*
* SPDX-License-Identi327722 Apache-2.0
*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <logging/log.h>
#include "lz4file.h"
#include <fs/fs.h>
#include <fs/fs_interface.h>
#include <kernel.h>


LOG_MODULE_REGISTER(LZ4STREAM,LOG_LEVEL_DBG);





void init_lz4stream(lz4streamfile * lz4id, const bool reuseContext){
	lz4id->ctx=NULL;
	lz4id->fid=NULL;
	lz4id->isOpen=false;
	lz4id->reuseContext=reuseContext;
	lz4id->nsrcdata=0;
}

static const LZ4F_preferences_t kPrefs = {
    {LZ4F_max64KB, LZ4F_blockLinked, LZ4F_noContentChecksum, LZ4F_frame,
      0 /* unknown content size */, 0 /* no dictID */ , LZ4F_noBlockChecksum },
    -1,   /* compression level; 0 == default. use -1 to avoid HC calls which use more memory*/
    1,   /* autoflush*/
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

static void tempname(char * dest,const char * src){
	strcpy(dest,src);
	strcat(dest,".tmp");
}

int lz4open(const char * path, lz4streamfile * lz4id){
	

	/* Dynamic allocation of buffers*/
	/*LOG_DBG("allocating %d for file %s\n",sizeof(lz4streamfile),path);*/
	/*lz4streamfile *lz4id =k_malloc(sizeof(lz4streamfile)); */
	if (lz4id == NULL){
		LOG_ERR("Cannot allocate lz4 admin struct");
		return LZ4_ERR_IO;
	}
	lz4id->fid=k_malloc(sizeof(struct fs_file_t));
	/*fs_file_t_init(lz4id->fid);*/
	
	strcpy(lz4id->filename,path);
	
	/*append .tmp to path for a file which is not finalized */
	char pathtmp[204];
	tempname(pathtmp,lz4id->filename);

	if ( fs_open(lz4id->fid,pathtmp,FS_O_WRITE|FS_O_CREATE)!=0){
		k_free(lz4id);
		lz4id=NULL;
		LOG_ERR("Cannot open lz4 output file");
		return LZ4_ERR_IO;
	}
	

	lz4id->cap = LZ4F_compressBound(CHUNKSIZE, &kPrefs);   /* large enough for any input <= IN_CHUNK_SIZE */
	LOG_DBG("Buffer size needed %d reserved %d\n",lz4id->cap,BUFFERSIZE);
	

	assert(LZ4F_HEADER_SIZE_MAX <= lz4id->cap);
	assert(lz4id->cap <= BUFFERSIZE);

	///Setup  compression context (if it is not allocated)
	if (!lz4id->ctx){
		handle_lz4error(LZ4F_createCompressionContext(&(lz4id->ctx), LZ4F_VERSION));
	}
    	{
		size_t const headerSize = LZ4F_compressBegin(lz4id->ctx, lz4id->destbuf, lz4id->cap, &kPrefs);
        	if (handle_lz4error(headerSize)) {
            		return LZ4_ERR_IO;
        	}
       		
		//write the frameheader to the output file
		const size_t written=fs_write(lz4id->fid,lz4id->destbuf, headerSize);
		LOG_DBG("Written %d bytes into header",written);
		if (handle_lz4error(written)){
			return LZ4_ERR_IO;
		}
	}
	
	lz4id->isOpen=true;
	return LZ4_SUCCESS;
}


int lz4write(lz4streamfile * lz4id,const char * data){
	size_t nwritten=0;

	int ndata=0;

	if (data != NULL){
		ndata=strlen(data);
	}

	/* compress srcbuffer if end of file or when new data does not fit */

	if (CHUNKSIZE < (ndata +lz4id->nsrcdata) || (data == NULL && lz4id->nsrcdata > 0) ){

		nwritten=LZ4F_compressUpdate(lz4id->ctx,lz4id->destbuf,lz4id->cap,lz4id->srcbuf,lz4id->nsrcdata,NULL);
		lz4id->nsrcdata=0;

		if (nwritten > 0){
			/* write compressed bytes to file if needed*/
			assert(nwritten < BUFFERSIZE);
			fs_write(lz4id->fid,lz4id->destbuf,nwritten);	
			fs_sync(lz4id->fid);
		}
		if (handle_lz4error(nwritten)){
			return LZ4_ERR_COMPRESS;
		}
	}


	/*copy new data to the end of srcbuffer */
	if (ndata > 0){
		assert(ndata < CHUNKSIZE);
		memcpy(&(lz4id->srcbuf[lz4id->nsrcdata]),data,ndata);
		lz4id->nsrcdata+=ndata;

	}


	
	/* possibly finalize file */
	if (data == NULL){
		/* Now end the compression frame*/
		nwritten=LZ4F_compressEnd(lz4id->ctx,lz4id->destbuf,lz4id->cap,NULL);
		if (nwritten > 0){
			assert(nwritten < BUFFERSIZE);
			fs_write(lz4id->fid,lz4id->destbuf,nwritten);	
			fs_sync(lz4id->fid);
		}
	}

	if (handle_lz4error(nwritten)){
		return LZ4_ERR_COMPRESS;
	}

	return LZ4_SUCCESS;
}

int lz4close(lz4streamfile * lz4id){
	//possibly write pending data (this will be done by adding a NULL pointer as the data
	if (!lz4id->isOpen){
		LOG_ERR(" File is not open so not closing lz4 file");
		return LZ4F_ERROR_GENERIC;
	}

	lz4write(lz4id,NULL);	
	fs_close(lz4id->fid);
	k_free(lz4id->fid);
	if (!lz4id->reuseContext){
		LZ4F_freeCompressionContext(lz4id->ctx);
	}
	
	
	/*rename temporary file */
	char pathtmp[204];
	tempname(pathtmp,lz4id->filename);
	
	fs_rename(pathtmp,lz4id->filename);

	lz4id->isOpen=false;
	strcpy(lz4id->filename,"");

	/*k_free(lz4id);*/
	/*lz4id=NULL;*/

	LOG_DBG("Successfully closed file\n");
	return LZ4_SUCCESS;

}



 /*TESTING MAIN function */
/*int main(){*/
	/*char * filename ="orig.lz4";*/
	/*char * inpf="./orig.txt";*/
	/*size_t bufsz=82;*/
	/*char *linebuf=NULL;*/
	


	/*static struct lz4streamfile lz4fid ;*/
	/*init_lz4stream(&lz4fid,true);*/
	/*if (lz4open(filename,lz4fid) != LZ4_SUCCESS){*/
	
		/*return -1;*/
	/*}*/
	

	/*FILE* fin=fopen(inpf,"rt");*/


	/*int line_size = getline(&linebuf, &bufsz, fin);*/
	/*lz4write(lz4fid,linebuf);*/
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



