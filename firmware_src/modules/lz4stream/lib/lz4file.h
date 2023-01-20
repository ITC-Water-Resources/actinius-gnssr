/*
* Copyright (c) 2022 Roelof Rietbroek <r.rietbroek@utwente.nl>
*
* SPDX-License-Identifier: Apache-2.0
*/



#define LZ4_ERR_OVERSIZED -1
#define LZ4_SUCCESS  0
#define LZ4_ERR_COMPRESS  -2
#define LZ4_ERR_IO -3

/*
 * CHUNKSIZE (maximum size of the input src data)
*/

#define CHUNKSIZE 4096
/* buffer size determined by trial and error */
#define BUFFERSIZE 4200 
/*
 * Struct holding the administrative parts of an open lz4stream
 */

#include <zephyr.h>
#include "lz4frame_static.h"


typedef struct lz4streamfile {
	LZ4F_compressionContext_t ctx;
	struct fs_file_t * fid;
	size_t cap;
	char destbuf[BUFFERSIZE];
	char srcbuf[CHUNKSIZE];
	char filename[200];
	int nsrcdata;
	bool isOpen;
        bool reuseContext;
}lz4streamfile;

int lz4open(const char *path, lz4streamfile * lz4id);
int lz4write(lz4streamfile * lz4id, const char * data);
int lz4close(lz4streamfile *lz4id);
void init_lz4stream(lz4streamfile * lz4id, const bool reuseContext);
