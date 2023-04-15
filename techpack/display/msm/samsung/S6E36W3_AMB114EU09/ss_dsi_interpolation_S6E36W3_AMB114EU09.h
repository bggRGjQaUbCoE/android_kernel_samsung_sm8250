/*
 * =================================================================
 *
 *       Filename:  ss_dsi_flash_dimming_S6E36W3_AMB114EU09.c
 *
 *    Description:  Smart dimming algorithm implementation
 *
 *        Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2012, Samsung Electronics. All rights reserved.

 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _SS_DSI_INTERPOLATION_S6E36W3_AMB114EU09_H_
#define _SS_DSI_INTERPOLATION_S6E36W3_AMB114EU09_H_

#include "ss_dsi_panel_common.h"

#define GAMMA_SIZE 34
#define AOR_SIZE 2
#define ELVSS_SIZE 3

#define BIT_SHIFT 20
#define BIT_SHFIT_MUL 1048576 // pow(2,BIT_SHIFT)

int table_parsing_data_S6E36W3_AMB114EU09(struct samsung_display_driver_data *vdd,
		struct brightness_table *br_tbl);
int table_gamma_update_S6E36W3_AMB114EU09(struct samsung_display_driver_data *vdd,
		struct brightness_table *br_tbl);
int init_interpolation_S6E36W3_AMB114EU09(struct samsung_display_driver_data *vdd,
		struct brightness_table *br_tbl, enum INTERPOLATION_MODE mode);
int flash_gamma_support_S6E36W3_AMB114EU09(struct samsung_display_driver_data *vdd);

#endif
