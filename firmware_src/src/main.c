/*
 * Copyright (c) 202 Roelof Rietbroek <r.rietbroek@utwente.nl>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <logging/log.h>
#include "testsuite.h"

LOG_MODULE_REGISTER(GNSSR);

void main(void)
{
	/*
	 * Mount SDCARD of the attached Featherwing
	 */
	int status;
	status=mount_sdcard();

	if (status != FEA_SUCCES) {
		return;	
	}
	
	/*Print IMEI and CCID */
	test_getIMEI_CCID();
	/*
	 * Main Program loop
	 */
	while (1) {
		status= test_lsdir();

		if (status != TEST_SUCCESS) {
			return;	
		}
		
		k_sleep(K_MSEC(10000));
		status = test_lz4compress();
		if (status != TEST_SUCCESS) {
			return;	
		}
		k_sleep(K_MSEC(60000));
	}
}

