/*
 * Copyright (C) 2022, Broadcom.
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
*/

#ifndef _epivers_h_
#define _epivers_h_

#define EPI_MAJOR_VERSION	101

#define EPI_MINOR_VERSION	16

#define EPI_RC_NUMBER		90

#define EPI_INCREMENTAL_NUMBER	0

#define EPI_BUILD_NUMBER	0

#define EPI_VERSION		101, 16, 90, 0

#define EPI_VERSION_NUM		0x65105a00

#define EPI_VERSION_DEV		101.16.90

/* Driver Version String, ASCII, 32 chars max */
#if defined (WLTEST)
#define EPI_VERSION_STR		"101.16.90 (wlan=r969326 WLTEST)"
#elif (defined (BCMDBG_ASSERT) && !defined (BCMDBG_ASSERT_DISABLED))
#define EPI_VERSION_STR		"101.16.90 (wlan=r969326 ASSRT)"
#else
#define EPI_VERSION_STR		"101.16.90 (wlan=r969326)"
#endif /* BCMINTERNAL */

#endif /* _epivers_h_ */
