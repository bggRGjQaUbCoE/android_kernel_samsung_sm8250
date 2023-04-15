/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */


#ifndef _PHYRX_USER_INFO_H_
#define _PHYRX_USER_INFO_H_
#if !defined(__ASSEMBLER__)
#endif

#include "receive_user_info.h"

// ################ START SUMMARY #################
//
//	Dword	Fields
//	0-2	struct receive_user_info receive_user_info_details;
//
// ################ END SUMMARY #################

#define NUM_OF_DWORDS_PHYRX_USER_INFO 3

struct phyrx_user_info {
    struct            receive_user_info                       receive_user_info_details;
};

/*

struct receive_user_info receive_user_info_details

			Overview of receive parameters that the MAC needs to
			prepend to every received MSDU/MPDU.



			PHY RX gathers this info for the MAC
*/

#define PHYRX_USER_INFO_0_RECEIVE_USER_INFO_RECEIVE_USER_INFO_DETAILS_OFFSET 0x00000000
#define PHYRX_USER_INFO_0_RECEIVE_USER_INFO_RECEIVE_USER_INFO_DETAILS_LSB 0
#define PHYRX_USER_INFO_0_RECEIVE_USER_INFO_RECEIVE_USER_INFO_DETAILS_MASK 0xffffffff
#define PHYRX_USER_INFO_1_RECEIVE_USER_INFO_RECEIVE_USER_INFO_DETAILS_OFFSET 0x00000004
#define PHYRX_USER_INFO_1_RECEIVE_USER_INFO_RECEIVE_USER_INFO_DETAILS_LSB 0
#define PHYRX_USER_INFO_1_RECEIVE_USER_INFO_RECEIVE_USER_INFO_DETAILS_MASK 0xffffffff
#define PHYRX_USER_INFO_2_RECEIVE_USER_INFO_RECEIVE_USER_INFO_DETAILS_OFFSET 0x00000008
#define PHYRX_USER_INFO_2_RECEIVE_USER_INFO_RECEIVE_USER_INFO_DETAILS_LSB 0
#define PHYRX_USER_INFO_2_RECEIVE_USER_INFO_RECEIVE_USER_INFO_DETAILS_MASK 0xffffffff


#endif // _PHYRX_USER_INFO_H_
