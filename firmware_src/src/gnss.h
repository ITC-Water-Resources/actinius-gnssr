
/*
* Copyright (c) 2023 Roelof Rietbroek <r.rietbroek@utwente.nl>
*
* SPDX-License-Identifier: Apache-2.0
*/

#include <stdint.h>

uint32_t got_fix(void);

void gnss_get_current_datetimestr(char cptr[]);

int32_t init_gnss(int useagps);
int32_t start_gnss(void);
int32_t stop_gnss(void);
