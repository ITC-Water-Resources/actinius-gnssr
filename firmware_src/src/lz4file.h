/*
* Copyright (c) 2022 Roelof Rietbroek <r.rietbroek@utwente.nl>
*
* SPDX-License-Identifier: Apache-2.0
*/



#define LZ4_ERR_OVERSIZED -1
#define LZ4_SUCCESS  0;
#define LZ4_ERR_COMPRESS  -2;
typedef struct lz4streamfile lz4streamfile;

lz4streamfile * lz4open(const char *path);
int lz4write(lz4streamfile * lz4id, const char * data);
int lz4close(lz4streamfile *lz4id);


