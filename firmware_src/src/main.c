/*
 * Copyright (c) 202 Roelof Rietbroek <r.rietbroek@utwente.nl>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <logging/log.h>
#include "featherw_datalogger.h"
#include "lz4file.h"

LOG_MODULE_REGISTER(main);

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
	
	/*
	 * Main Program loop
	 */
	while (1) {
		status= lsdir("/SD:/CONFIG");

		if (status != FEA_SUCCES) {
			return;	
		}

		k_sleep(K_MSEC(5000));
	}
}

