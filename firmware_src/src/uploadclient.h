
/* Copyright 2022, Roelof Rietbroek (r.rietbroek@utwente.nl)
 * License: see license file
 * Tools to work with http (file) uploads
 */

#define UPLOADCLNT_SUCCESS 0
#define UPLOADCLNT_ERROR 1

#include "config.h"
int cert_provision(const char * cacert);

int webdavUploadFile(const char * filename, const struct config * conf);

