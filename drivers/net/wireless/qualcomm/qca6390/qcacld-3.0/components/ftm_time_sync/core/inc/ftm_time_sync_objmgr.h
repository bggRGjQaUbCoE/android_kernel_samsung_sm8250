/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: This file contains various object manager related wrappers and helpers
 */

#ifndef _FTM_TIME_SYNC_OBJMGR_H
#define _FTM_TIME_SYNC_OBJMGR_H

#include "wlan_cmn.h"
#include "wlan_objmgr_cmn.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_objmgr_global_obj.h"

/**
 * ftm_timesync_vdev_get_ref() - Wrapper to increment ftm_timesync ref count
 * @vdev: vdev object
 *
 * Wrapper for ftm_timesync to increment ref count after checking valid
 * object state.
 *
 * Return: SUCCESS/FAILURE
 */
static inline
QDF_STATUS ftm_timesync_vdev_get_ref(struct wlan_objmgr_vdev *vdev)
{
	return wlan_objmgr_vdev_try_get_ref(vdev, FTM_TIME_SYNC_ID);
}

/**
 * ftm_timesync_vdev_put_ref() - Wrapper to decrement ftm_timesync ref count
 * @vdev: vdev object
 *
 * Wrapper for ftm_timesync to decrement ref count of vdev.
 *
 * Return: SUCCESS/FAILURE
 */
static inline
void ftm_timesync_vdev_put_ref(struct wlan_objmgr_vdev *vdev)
{
	return wlan_objmgr_vdev_release_ref(vdev, FTM_TIME_SYNC_ID);
}

/**
 * ftm_timesync_vdev_get_priv(): Wrapper to retrieve vdev priv obj
 * @vdev: vdev pointer
 *
 * Wrapper for ftm_timesync to get vdev private object pointer.
 *
 * Return: Private object of vdev
 */
static inline struct ftm_timesync_vdev_priv *
ftm_timesync_vdev_get_priv(struct wlan_objmgr_vdev *vdev)
{
	struct ftm_timesync_vdev_priv *vdev_priv;

	vdev_priv = wlan_objmgr_vdev_get_comp_private_obj(
					vdev, WLAN_UMAC_COMP_FTM_TIME_SYNC);
	QDF_BUG(vdev_priv);

	return vdev_priv;
}

#endif /* _FTM_TIME_SYNC_OBJMGR_H */
