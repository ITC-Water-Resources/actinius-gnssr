/*
 * Copyright (c) 2023 Roelof Rietbroek, with segments from Nordic Semiconductor zephyr sample codes
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 
 Roelof Rietbroek (r.rietbroek@utwente.nl)  
*/
int setup_modem(void);

int lte_connect(void);
void lte_disconnect(void);

int enable_gnss_mode(void);
void print_boardinfo();

