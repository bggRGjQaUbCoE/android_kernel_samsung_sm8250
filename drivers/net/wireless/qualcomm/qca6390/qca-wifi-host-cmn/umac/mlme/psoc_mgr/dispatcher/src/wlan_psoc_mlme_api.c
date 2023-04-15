/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
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
 * DOC: Implements PSOC MLME public APIs
 */

#include <wlan_objmgr_psoc_obj.h>
#include <wlan_mlme_dbg.h>
#include <include/wlan_psoc_mlme.h>
#include <wlan_psoc_mlme_api.h>
#include <qdf_module.h>

struct psoc_mlme_obj *wlan_psoc_mlme_get_cmpt_obj(struct wlan_objmgr_psoc *psoc)
{
	struct psoc_mlme_obj *psoc_mlme;

	psoc_mlme = wlan_objmgr_psoc_get_comp_private_obj(psoc,
							  WLAN_UMAC_COMP_MLME);
	if (!psoc_mlme) {
		mlme_err("PSOC MLME component object is NULL");
		return NULL;
	}

	return psoc_mlme;
}

qdf_export_symbol(wlan_psoc_mlme_get_cmpt_obj);

mlme_psoc_ext_t *wlan_psoc_mlme_get_ext_hdl(struct wlan_objmgr_psoc *psoc)
{
	struct psoc_mlme_obj *psoc_mlme;

	psoc_mlme = wlan_psoc_mlme_get_cmpt_obj(psoc);
	if (psoc_mlme)
		return psoc_mlme->ext_psoc_ptr;

	return NULL;
}

qdf_export_symbol(wlan_psoc_mlme_get_ext_hdl);

void wlan_psoc_mlme_set_ext_hdl(struct psoc_mlme_obj *psoc_mlme,
				mlme_psoc_ext_t *psoc_ext_hdl)
{
	psoc_mlme->ext_psoc_ptr = psoc_ext_hdl;
}

qdf_export_symbol(wlan_psoc_mlme_set_ext_hdl);
