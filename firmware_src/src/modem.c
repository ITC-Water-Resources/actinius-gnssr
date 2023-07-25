
/*
 * Copyright (c) 2023 Roelof Rietbroek, with segments from Nordic Semiconductor zephyr sample codes
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 
 Roelof Rietbroek (r.rietbroek@utwente.nl)  
*/

#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <modem/modem_info.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(GNSSR,CONFIG_GNSSR_LOG_LEVEL);


K_SEM_DEFINE(lte_ready, 0, 1);

static void lte_lc_event_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) {
	case LTE_LC_EVT_NW_REG_STATUS:
		if ((evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME) ||
		    (evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_ROAMING)) {
			LOG_INF("Connected to LTE network");
			k_sem_give(&lte_ready);
		}else{

			LOG_INF("Unhandled LTE registration status %d",evt->nw_reg_status);
		}
		break;

	default:

		LOG_INF("Ignoring LTE event %d",evt->type);
		break;
	}
}


int lte_connect(void)
{
	int err;

	LOG_INF("Connecting to LTE network");
	err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_LTE);
	/*err = lte_lc_connect_async(lte_lc_event_handler);*/
	if (err) {
		LOG_ERR("Failed to activate LTE, error: %d", err);
		return -1;
	}

	err=k_sem_take(&lte_ready, K_SECONDS(30));
	
	if( err == -EAGAIN){
		LOG_ERR("Timeout in waiting for LTE");
		return -1;
	}
	/* Wait for a while, because with IPv4v6 PDN the IPv6 activation takes a bit more time. */
	k_sleep(K_SECONDS(1));
	return 0;
}

void lte_disconnect(void)
{
	int err;

	err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_DEACTIVATE_LTE);
	if (err) {
		LOG_ERR("Failed to deactivate LTE, error: %d", err);
		return;
	}

	LOG_INF("LTE disconnected");
}


int enable_gnss_mode(void){

	/* Enable GNSS. */
	if (lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_GNSS) != 0) {
		LOG_ERR("Failed to activate GNSS functional mode");
		return -1;
	}
	return 0;
}



/*static const char *const at_commands[] = {*/
	/*AT_XSYSTEMMODE,*/
	/*AT_MAGPIO,*/
	/*AT_ACTIVATE_GPS*/
/*};*/

int setup_modem(void)
{
	/*if(lte_lc_system_mode_set(LTE_LC_SYSTEM_MODE_GPS, LTE_LC_SYSTEM_MODE_PREFER_AUTO) != 0){*/
		/*LOG_ERR("Failed to set modem system mode");*/
		/*return -1;*/
	/*}*/
	
	/*int err = nrf_modem_lib_init(NORMAL_MODE);*/
	/*if (err) {*/
		/*LOG_ERR("Modem library initialization failed, error: %d", err);*/
		/*return err;*/
	/*}*/

	if (lte_lc_init() != 0) {
		LOG_ERR("Failed to initialize LTE link controller");
		return -1;
	}
	
	lte_lc_register_handler(lte_lc_event_handler);

	/* set the modem to normal mode and then optionally directly to flight mode with UICC on*/
	/*if(lte_lc_func_mode_set(LTE_LC_FUNC_MODE_NORMAL) != 0){*/
		/*LOG_ERR("Cannot set modem to normal mode");*/
		/*return -1;*/
	/*}*/

	/*if(lte_lc_func_mode_set(LTE_LC_FUNC_MODE_OFFLINE_UICC_ON) != 0){*/
		/*LOG_ERR("Cannot set modem to  flight mode");*/
		/*return -1;*/
	/*}*/
	

	return 0;
}



void print_boardinfo(){
	
	LOG_INF("\n\n");
	LOG_INF("---begin board info---");

#ifdef CONFIG_GNSSR_CONTACT
	LOG_INF("Contact: %s",CONFIG_GNSSR_CONTACT);
#endif

#ifdef CONFIG_GNSSR_VERSION
	LOG_INF("GNSS-R_version: %s",CONFIG_GNSSR_VERSION);
#endif

	char modeminfostring [32];
	if(modem_info_init() != 0){
		LOG_ERR("error initializing modem info");
		return;
	}
	
	if(modem_info_string_get(MODEM_INFO_FW_VERSION, modeminfostring,sizeof(modeminfostring))< 0){
		LOG_ERR("cannot retrieve modem version number");
			
	}
	
	LOG_INF("modem firmware: %s",modeminfostring);
	
	/*Retrieving the CCID requires UICC to be turned on (but can be ion flight mode) */
	
	/*if(lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_UICC) == 0){*/
		/*if(modem_info_string_get(MODEM_INFO_ICCID, modeminfostring,sizeof(modeminfostring))< 0){*/
			/*LOG_ERR("cannot retrieve (e)SIM ICCID");*/
		
		/*}*/
		/*LOG_INF("(e)SIM ICCID: %s",modeminfostring);*/
	
		/*if(lte_lc_func_mode_set(LTE_LC_FUNC_MODE_OFFLINE) == 0){*/
			/*LOG_ERR("cannot deactivate UICC");*/
		/*}*/



	/*}else{*/
		/*LOG_ERR("Cannot set activate UUIC");*/

	/*}*/

	if(modem_info_string_get(MODEM_INFO_IMEI, modeminfostring,sizeof(modeminfostring))< 0){
		LOG_ERR("cannot retrieve module IMEI");
			
	}
	LOG_INF("Modem IMEI: %s",modeminfostring);

	enum lte_lc_system_mode sysmode;
	enum lte_lc_system_mode_preference sysmodepref;
	
	if (lte_lc_system_mode_get(&sysmode, &sysmodepref) != 0){
	
		LOG_ERR("cannot retrieve modem system mode");

	}else{

		switch (sysmode) {
		case LTE_LC_SYSTEM_MODE_NONE:
			LOG_INF("LTE_LC_SYSTEM_MODE_NONE");
			break;
		case LTE_LC_SYSTEM_MODE_LTEM:
			LOG_INF("LTE_LC_SYSTEM_MODE_LTEM");
			break;
		case LTE_LC_SYSTEM_MODE_NBIOT:
			LOG_INF("LTE_LC_SYSTEM_MODE_NBIOT");
			break;
		case LTE_LC_SYSTEM_MODE_GPS:
			LOG_INF("LTE_LC_SYSTEM_MODE_GPS");
			break;
		case LTE_LC_SYSTEM_MODE_NBIOT_GPS:
			LOG_INF("LTE_LC_SYSTEM_MODE_NBIOT_GPS");
			break;
		case LTE_LC_SYSTEM_MODE_LTEM_NBIOT:
			LOG_INF("LTE_LC_SYSTEM_MODE_LTEM_NBIOT");
			break;
		case LTE_LC_SYSTEM_MODE_LTEM_GPS:
			LOG_INF("LTE_LC_SYSTEM_MODE_LTEM_GPS");
			break;
		case LTE_LC_SYSTEM_MODE_LTEM_NBIOT_GPS:
			LOG_INF("LTE_LC_SYSTEM_MODE_LTEM_NBIOT_GPS");
			break;
		default:
			LOG_INF("Unspecified system mode %d",sysmode);
			break;
		}
	}
	/*enum lte_lc_func_mode funcmode;*/

	/*if (lte_lc_func_mode_get(&funcmode) != 0){*/
	
		/*LOG_ERR("cannot retrieve modem functional mode");*/

	/*}else{*/

		/*switch (funcmode) {*/
		/*case LTE_LC_FUNC_MODE_POWER_OFF:*/
			/*LOG_INF("LTE_LC_FUNC_MODE_POWER_OFF");*/
			/*break;*/
		/*case LTE_LC_FUNC_MODE_NORMAL:*/
			/*LOG_INF("LTE_LC_FUNC_MODE_NORMAL");*/
			/*break;*/
		/*default:*/
			/*LOG_INF("Unspecified functional mode %d",sysmode);*/
			/*break;*/
	/*}*/
	/*}*/
	LOG_INF("---end board info---\n\n");
		

}



