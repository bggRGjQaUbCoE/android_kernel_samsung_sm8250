/*
 * Linux cfg80211 Vendor Extension Code
 *
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
 * <<Broadcom-WL-IPTag/Dual:>>
 */

/*
 * New vendor interface additon to nl80211/cfg80211 to allow vendors
 * to implement proprietary features over the cfg80211 stack.
*/

#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>

#include <bcmutils.h>
#include <bcmwifi_channels.h>
#include <bcmendian.h>
#include <ethernet.h>
#include <802.11.h>
#include <linux/if_arp.h>
#include <asm/uaccess.h>

#include <dngl_stats.h>
#include "wifi_stats.h"
#include <dhd.h>
#include <dhd_debug.h>
#include <dhdioctl.h>
#include <wlioctl.h>
#include <wlioctl_utils.h>
#include <dhd_cfg80211.h>
#ifdef DHD_PKT_LOGGING
#include <dhd_pktlog.h>
#endif /* DHD_PKT_LOGGING */
#ifdef PNO_SUPPORT
#include <dhd_pno.h>
#endif /* PNO_SUPPORT */
#ifdef RTT_SUPPORT
#include <dhd_rtt.h>
#endif /* RTT_SUPPORT */

#include <ethernet.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/netdevice.h>
#include <linux/sched.h>
#include <linux/etherdevice.h>
#include <linux/wireless.h>
#include <linux/ieee80211.h>
#include <linux/wait.h>
#include <net/cfg80211.h>
#include <net/rtnetlink.h>

#include <wlioctl.h>
#include <wldev_common.h>
#include <wl_cfg80211.h>
#include <wl_cfgp2p.h>
#include <wl_cfgscan.h>
#ifdef WL_NAN
#include <wl_cfgnan.h>
#endif /* WL_NAN */

#include <wl_android.h>

#include <wl_cfgvendor.h>
#ifdef PROP_TXSTATUS
#include <dhd_wlfc.h>
#endif
#include <brcm_nl80211.h>

char*
wl_get_kernel_timestamp(void)
{
	static char buf[32];
	u64 ts_nsec;
	unsigned long rem_nsec;

	ts_nsec = local_clock();
	rem_nsec = DIV_AND_MOD_U64_BY_U32(ts_nsec, NSEC_PER_SEC);
	snprintf(buf, sizeof(buf), "%5lu.%06lu",
		(unsigned long)ts_nsec, rem_nsec / NSEC_PER_USEC);

	return buf;
}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 13, 0)) || defined(WL_VENDOR_EXT_SUPPORT)
#if defined(WL_SUPP_EVENT)
int
wl_cfgvendor_send_supp_eventstring(const char *func_name, const char *fmt, ...)
{
	char buf[SUPP_LOG_LEN] = {0};
	struct bcm_cfg80211 *cfg;
	struct wiphy *wiphy;
	va_list args;
	int len;
	int prefix_len;
	int rem_len;

	cfg = wl_cfg80211_get_bcmcfg();
	if (!cfg || !cfg->wdev) {
		WL_DBG(("supp evt invalid arg\n"));
		return BCME_OK;
	}

	wiphy = cfg->wdev->wiphy;
	prefix_len = snprintf(buf, SUPP_LOG_LEN, "[DHD]<%s> %s: ",
		wl_get_kernel_timestamp(), __func__);
	/* Remaining buffer len */
	rem_len = SUPP_LOG_LEN - (prefix_len + 1);
	/* Print the arg list on to the remaining part of the buffer */
	va_start(args, fmt);
	len = vsnprintf((buf + prefix_len), rem_len, fmt, args);
	va_end(args);
	if (len < 0) {
		return -EINVAL;
	}

	if (len > rem_len) {
		/* If return length is greater than buffer len,
		 * then its truncated buffer case.
		 */
		len = rem_len;
	}

	/* Ensure the buffer is null terminated */
	len += prefix_len;
	buf[len] = '\0';
	len++;

	return wl_cfgvendor_send_async_event(wiphy,
		bcmcfg_to_prmry_ndev(cfg), BRCM_VENDOR_EVENT_PRIV_STR, buf, len);
}

int
wl_cfgvendor_notify_supp_event_str(const char *evt_name, const char *fmt, ...)
{
	char buf[SUPP_LOG_LEN] = {0};
	struct bcm_cfg80211 *cfg;
	struct wiphy *wiphy;
	va_list args;
	int len;
	int prefix_len;
	int rem_len;

	cfg = wl_cfg80211_get_bcmcfg();
	if (!cfg || !cfg->wdev) {
		WL_DBG(("supp evt invalid arg\n"));
		return BCME_OK;
	}
	wiphy = cfg->wdev->wiphy;
	prefix_len = snprintf(buf, SUPP_LOG_LEN, "%s ", evt_name);
	/* Remaining buffer len */
	rem_len = SUPP_LOG_LEN - (prefix_len + 1);
	/* Print the arg list on to the remaining part of the buffer */
	va_start(args, fmt);
	len = vsnprintf((buf + prefix_len), rem_len, fmt, args);
	va_end(args);
	if (len < 0) {
		return -EINVAL;
	}

	if (len > rem_len) {
		/* If return length is greater than buffer len,
		 * then its truncated buffer case.
		 */
		len = rem_len;
	}

	/* Ensure the buffer is null terminated */
	len += prefix_len;
	buf[len] = '\0';
	len++;

	return wl_cfgvendor_send_async_event(wiphy,
		bcmcfg_to_prmry_ndev(cfg), BRCM_VENDOR_EVENT_PRIV_STR, buf, len);
}
#endif /* WL_SUPP_EVENT */

/*
 * This API is to be used for asynchronous vendor events. This
 * shouldn't be used in response to a vendor command from its
 * do_it handler context (instead wl_cfgvendor_send_cmd_reply should
 * be used).
 */
int wl_cfgvendor_send_async_event(struct wiphy *wiphy,
	struct net_device *dev, int event_id, const void  *data, int len)
{
	gfp_t kflags;
	struct sk_buff *skb;

	kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;

	/* Alloc the SKB for vendor_event */
#if (defined(CONFIG_ARCH_MSM) && defined(SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC)) || \
	LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
	skb = cfg80211_vendor_event_alloc(wiphy, ndev_to_wdev(dev), len, event_id, kflags);
#else
	skb = cfg80211_vendor_event_alloc(wiphy, len, event_id, kflags);
#endif /* (defined(CONFIG_ARCH_MSM) && defined(SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC)) || */
		/* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0) */
	if (!skb) {
		WL_ERR(("skb alloc failed"));
		return -ENOMEM;
	}

	/* Push the data to the skb */
	nla_put_nohdr(skb, len, data);

	cfg80211_vendor_event(skb, kflags);

	return 0;
}

static int
wl_cfgvendor_send_cmd_reply(struct wiphy *wiphy,
	const void  *data, int len)
{
	struct sk_buff *skb;
	int err;

	/* Alloc the SKB for vendor_event */
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, len);
	if (unlikely(!skb)) {
		WL_ERR(("skb alloc failed"));
		err = -ENOMEM;
		goto exit;
	}

	/* Push the data to the skb */
	nla_put_nohdr(skb, len, data);
	err = cfg80211_vendor_cmd_reply(skb);
exit:
	WL_DBG(("wl_cfgvendor_send_cmd_reply status %d", err));
	return err;
}

static int
wl_cfgvendor_get_feature_set(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int reply;

	reply = dhd_dev_get_feature_set(bcmcfg_to_prmry_ndev(cfg));

	err =  wl_cfgvendor_send_cmd_reply(wiphy, &reply, sizeof(int));
	if (unlikely(err))
		WL_ERR(("Vendor Command reply failed ret:%d \n", err));

	return err;
}

static int
wl_cfgvendor_get_feature_set_matrix(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct sk_buff *skb;
	int reply;
	int mem_needed, i;

	mem_needed = VENDOR_REPLY_OVERHEAD +
		(ATTRIBUTE_U32_LEN * MAX_FEATURE_SET_CONCURRRENT_GROUPS) + ATTRIBUTE_U32_LEN;

	/* Alloc the SKB for vendor_event */
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, mem_needed);
	if (unlikely(!skb)) {
		WL_ERR(("skb alloc failed"));
		err = -ENOMEM;
		goto exit;
	}

	err = nla_put_u32(skb, ANDR_WIFI_ATTRIBUTE_NUM_FEATURE_SET,
		MAX_FEATURE_SET_CONCURRRENT_GROUPS);
	if (unlikely(err)) {
		kfree_skb(skb);
		goto exit;
	}
	for (i = 0; i < MAX_FEATURE_SET_CONCURRRENT_GROUPS; i++) {
		reply = dhd_dev_get_feature_set_matrix(bcmcfg_to_prmry_ndev(cfg), i);
		if (reply != WIFI_FEATURE_INVALID) {
			err = nla_put_u32(skb, ANDR_WIFI_ATTRIBUTE_FEATURE_SET,
				reply);
			if (unlikely(err)) {
				kfree_skb(skb);
				goto exit;
			}
		}
	}

	err =  cfg80211_vendor_cmd_reply(skb);

	if (unlikely(err)) {
		WL_ERR(("Vendor Command reply failed ret:%d \n", err));
	}
exit:
	return err;
}

static int
wl_cfgvendor_set_rand_mac_oui(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = -EINVAL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int type;

	if (!data) {
		WL_ERR(("data is not available\n"));
		goto exit;
	}

	if (len <= 0) {
		WL_ERR(("invalid len %d\n", len));
		goto exit;
	}

	type = nla_type(data);

	if (type == ANDR_WIFI_ATTRIBUTE_RANDOM_MAC_OUI) {
		if (nla_len(data) != DOT11_OUI_LEN) {
			WL_ERR(("nla_len not matched.\n"));
			goto exit;
		}
		err = dhd_dev_cfg_rand_mac_oui(bcmcfg_to_prmry_ndev(cfg), nla_data(data));

		if (unlikely(err))
			WL_ERR(("Bad OUI, could not set:%d \n", err));
	}
exit:
	return err;
}
#ifdef CUSTOM_FORCE_NODFS_FLAG
static int
wl_cfgvendor_set_nodfs_flag(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int len)
{
	int err = -EINVAL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int type;
	u32 nodfs;

	if (!data) {
		WL_ERR(("data is not available\n"));
		return -EINVAL;
	}

	if (len <= 0) {
		WL_ERR(("invalid len %d\n", len));
		return -EINVAL;
	}

	type = nla_type(data);
	if (type == ANDR_WIFI_ATTRIBUTE_NODFS_SET) {
		nodfs = nla_get_u32(data);
		err = dhd_dev_set_nodfs(bcmcfg_to_prmry_ndev(cfg), nodfs);
	}

	return err;
}
#endif /* CUSTOM_FORCE_NODFS_FLAG */

#ifdef WL_AUTO_COUNTRY
static int
wl_config_autocountry(struct bcm_cfg80211 *cfg,
		struct net_device *ndev, char *country_code)
{
	bool val = FALSE;
	s32 err;

	if (!country_code) {
		return -EINVAL;
	}

	if ((country_code[0] == '0') && (country_code[1] == '0')) {
		/* Enable auto country for world domain (00) */
		val = TRUE;
	}

	err = wldev_iovar_setint(ndev, "autocountry", val);
	if (err) {
		WL_ERR(("Failed to config auto country (%d) ret:%d\n", val, err));
		return err;
	}

	return err;
}
#endif /* WL_AUTO_COUNTRY */

static int
wl_cfgvendor_set_country(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int len)
{
	int err = BCME_ERROR, rem, type;
	char country_code[WLC_CNTRY_BUF_SZ] = {0};
	const struct nlattr *iter;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct net_device *primary_ndev = bcmcfg_to_prmry_ndev(cfg);

	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		switch (type) {
			case ANDR_WIFI_ATTRIBUTE_COUNTRY:
				err = memcpy_s(country_code, WLC_CNTRY_BUF_SZ,
					nla_data(iter), nla_len(iter));
				if (err) {
					WL_ERR(("Failed to copy country code: %d\n", err));
					return err;
				}
				break;
			default:
				WL_ERR(("Unknown type: %d\n", type));
				return err;
		}
	}

#ifdef WL_AUTO_COUNTRY
	err = wl_config_autocountry(cfg, primary_ndev, country_code);
	if (err) {
		return err;
	}
#endif /* WL_AUTO_COUNTRY */

	/* country code is unique for dongle..hence using primary interface. */
	err = wl_cfg80211_set_country_code(primary_ndev, country_code, true, true, 0);
	if (err < 0) {
		WL_ERR(("Set country failed ret:%d\n", err));
	}

	return err;
}

#ifdef GSCAN_SUPPORT
int
wl_cfgvendor_send_hotlist_event(struct wiphy *wiphy,
	struct net_device *dev, void  *data, int len, wl_vendor_event_t event)
{
	gfp_t kflags;
	const void *ptr;
	struct sk_buff *skb;
	int malloc_len, total, iter_cnt_to_send, cnt;
	gscan_results_cache_t *cache = (gscan_results_cache_t *)data;

	total = len/sizeof(wifi_gscan_result_t);
	while (total > 0) {
		malloc_len = (total * sizeof(wifi_gscan_result_t)) + VENDOR_DATA_OVERHEAD;
		if (malloc_len > NLMSG_DEFAULT_SIZE) {
			malloc_len = NLMSG_DEFAULT_SIZE;
		}
		iter_cnt_to_send =
		   (malloc_len - VENDOR_DATA_OVERHEAD)/sizeof(wifi_gscan_result_t);
		total = total - iter_cnt_to_send;

		kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;

		/* Alloc the SKB for vendor_event */
#if (defined(CONFIG_ARCH_MSM) && defined(SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC)) || \
	LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
		skb = cfg80211_vendor_event_alloc(wiphy, ndev_to_wdev(dev),
		malloc_len, event, kflags);
#else
		skb = cfg80211_vendor_event_alloc(wiphy, malloc_len, event, kflags);
#endif /* (defined(CONFIG_ARCH_MSM) && defined(SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC)) || */
		/* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0) */
		if (!skb) {
			WL_ERR(("skb alloc failed"));
			return -ENOMEM;
		}

		while (cache && iter_cnt_to_send) {
			ptr = (const void *) &cache->results[cache->tot_consumed];

			if (iter_cnt_to_send < (cache->tot_count - cache->tot_consumed)) {
				cnt = iter_cnt_to_send;
			} else {
				cnt = (cache->tot_count - cache->tot_consumed);
			}

			iter_cnt_to_send -= cnt;
			cache->tot_consumed += cnt;
			/* Push the data to the skb */
			nla_append(skb, cnt * sizeof(wifi_gscan_result_t), ptr);
			if (cache->tot_consumed == cache->tot_count) {
				cache = cache->next;
			}

		}

		cfg80211_vendor_event(skb, kflags);
	}

	return 0;
}

static int
wl_cfgvendor_gscan_get_capabilities(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	dhd_pno_gscan_capabilities_t *reply = NULL;
	uint32 reply_len = 0;

	reply = dhd_dev_pno_get_gscan(bcmcfg_to_prmry_ndev(cfg),
	   DHD_PNO_GET_CAPABILITIES, NULL, &reply_len);
	if (!reply) {
		WL_ERR(("Could not get capabilities\n"));
		err = -EINVAL;
		return err;
	}

	err =  wl_cfgvendor_send_cmd_reply(wiphy, reply, reply_len);
	if (unlikely(err)) {
		WL_ERR(("Vendor Command reply failed ret:%d \n", err));
	}

	MFREE(cfg->osh, reply, reply_len);
	return err;
}

static int
wl_cfgvendor_gscan_get_batch_results(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	gscan_results_cache_t *results, *iter;
	uint32 reply_len, is_done = 1;
	int32 mem_needed, num_results_iter;
	wifi_gscan_result_t *ptr;
	uint16 num_scan_ids, num_results;
	struct sk_buff *skb;
	struct nlattr *scan_hdr, *complete_flag;

	err = dhd_dev_wait_batch_results_complete(bcmcfg_to_prmry_ndev(cfg));
	if (err != BCME_OK)
		return -EBUSY;

	err = dhd_dev_pno_lock_access_batch_results(bcmcfg_to_prmry_ndev(cfg));
	if (err != BCME_OK) {
		WL_ERR(("Can't obtain lock to access batch results %d\n", err));
		return -EBUSY;
	}
	results = dhd_dev_pno_get_gscan(bcmcfg_to_prmry_ndev(cfg),
	             DHD_PNO_GET_BATCH_RESULTS, NULL, &reply_len);

	if (!results) {
		WL_ERR(("No results to send %d\n", err));
		err =  wl_cfgvendor_send_cmd_reply(wiphy, results, 0);

		if (unlikely(err))
			WL_ERR(("Vendor Command reply failed ret:%d \n", err));
		dhd_dev_pno_unlock_access_batch_results(bcmcfg_to_prmry_ndev(cfg));
		return err;
	}
	num_scan_ids = reply_len & 0xFFFF;
	num_results = (reply_len & 0xFFFF0000) >> 16;
	mem_needed = (num_results * sizeof(wifi_gscan_result_t)) +
	             (num_scan_ids * GSCAN_BATCH_RESULT_HDR_LEN) +
	             VENDOR_REPLY_OVERHEAD + SCAN_RESULTS_COMPLETE_FLAG_LEN;

	if (mem_needed > (int32)NLMSG_DEFAULT_SIZE) {
		mem_needed = (int32)NLMSG_DEFAULT_SIZE;
	}

	WL_TRACE(("is_done %d mem_needed %d max_mem %d\n", is_done, mem_needed,
		(int)NLMSG_DEFAULT_SIZE));
	/* Alloc the SKB for vendor_event */
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, mem_needed);
	if (unlikely(!skb)) {
		WL_ERR(("skb alloc failed"));
		dhd_dev_pno_unlock_access_batch_results(bcmcfg_to_prmry_ndev(cfg));
		return -ENOMEM;
	}
	iter = results;
	complete_flag = nla_reserve(skb, GSCAN_ATTRIBUTE_SCAN_RESULTS_COMPLETE,
	                    sizeof(is_done));

	if (unlikely(!complete_flag)) {
		WL_ERR(("complete_flag could not be reserved"));
		kfree_skb(skb);
		dhd_dev_pno_unlock_access_batch_results(bcmcfg_to_prmry_ndev(cfg));
		return -ENOMEM;
	}
	mem_needed = mem_needed - (SCAN_RESULTS_COMPLETE_FLAG_LEN + VENDOR_REPLY_OVERHEAD);

	while (iter) {
		num_results_iter = (mem_needed - (int32)GSCAN_BATCH_RESULT_HDR_LEN);
		num_results_iter /= (int32)sizeof(wifi_gscan_result_t);
		if (num_results_iter <= 0 ||
		    ((iter->tot_count - iter->tot_consumed) > num_results_iter)) {
			break;
		}
		scan_hdr = nla_nest_start(skb, GSCAN_ATTRIBUTE_SCAN_RESULTS);
		/* no more room? we are done then (for now) */
		if (scan_hdr == NULL) {
			is_done = 0;
			break;
		}
		err = nla_put_u32(skb, GSCAN_ATTRIBUTE_SCAN_ID, iter->scan_id);
		if (unlikely(err)) {
			goto fail;
		}
		err = nla_put_u8(skb, GSCAN_ATTRIBUTE_SCAN_FLAGS, iter->flag);
		if (unlikely(err)) {
			goto fail;
		}
		err = nla_put_u32(skb, GSCAN_ATTRIBUTE_CH_BUCKET_BITMASK, iter->scan_ch_bucket);
		if (unlikely(err)) {
			goto fail;
		}
		num_results_iter = iter->tot_count - iter->tot_consumed;

		err = nla_put_u32(skb, GSCAN_ATTRIBUTE_NUM_OF_RESULTS, num_results_iter);
		if (unlikely(err)) {
			goto fail;
		}
		if (num_results_iter) {
			ptr = &iter->results[iter->tot_consumed];
			err = nla_put(skb, GSCAN_ATTRIBUTE_SCAN_RESULTS,
			 num_results_iter * sizeof(wifi_gscan_result_t), ptr);
			if (unlikely(err)) {
				goto fail;
			}
			iter->tot_consumed += num_results_iter;
		}
		nla_nest_end(skb, scan_hdr);
		mem_needed -= GSCAN_BATCH_RESULT_HDR_LEN +
		    (num_results_iter * sizeof(wifi_gscan_result_t));
		iter = iter->next;
	}
	/* Cleans up consumed results and returns TRUE if all results are consumed */
	is_done = dhd_dev_gscan_batch_cache_cleanup(bcmcfg_to_prmry_ndev(cfg));
	memcpy(nla_data(complete_flag), &is_done, sizeof(is_done));
	dhd_dev_pno_unlock_access_batch_results(bcmcfg_to_prmry_ndev(cfg));
	return cfg80211_vendor_cmd_reply(skb);
fail:
	/* Free up consumed results which will now not be sent */
	(void)dhd_dev_gscan_batch_cache_cleanup(bcmcfg_to_prmry_ndev(cfg));
	kfree_skb(skb);
	dhd_dev_pno_unlock_access_batch_results(bcmcfg_to_prmry_ndev(cfg));
	return err;
}

static int
wl_cfgvendor_initiate_gscan(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int type, tmp = len;
	int run = 0xFF;
	int flush = 0;
	const struct nlattr *iter;

	nla_for_each_attr(iter, data, len, tmp) {
		type = nla_type(iter);
		if (type == GSCAN_ATTRIBUTE_ENABLE_FEATURE)
			run = nla_get_u32(iter);
		else if (type == GSCAN_ATTRIBUTE_FLUSH_FEATURE)
			flush = nla_get_u32(iter);
	}

	if (run != 0xFF) {
		err = dhd_dev_pno_run_gscan(bcmcfg_to_prmry_ndev(cfg), run, flush);

		if (unlikely(err)) {
			WL_ERR(("Could not run gscan:%d \n", err));
		}
		return err;
	} else {
		return -EINVAL;
	}

}

static int
wl_cfgvendor_enable_full_scan_result(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int type;
	bool real_time = FALSE;

	if (!data) {
		WL_ERR(("data is not available\n"));
		return -EINVAL;
	}

	if (len <= 0) {
		WL_ERR(("invalid len %d\n", len));
		return -EINVAL;
	}

	type = nla_type(data);

	if (type == GSCAN_ATTRIBUTE_ENABLE_FULL_SCAN_RESULTS) {
		real_time = nla_get_u32(data);

		err = dhd_dev_pno_enable_full_scan_result(bcmcfg_to_prmry_ndev(cfg), real_time);

		if (unlikely(err)) {
			WL_ERR(("Could not run gscan:%d \n", err));
		}

	} else {
		err = -EINVAL;
	}

	return err;
}

static int
wl_cfgvendor_set_scan_cfg_bucket(const struct nlattr *prev,
	gscan_scan_params_t *scan_param, int num)
{
	struct dhd_pno_gscan_channel_bucket  *ch_bucket;
	int k = 0;
	int type, err = 0, rem;
	const struct nlattr *cur, *next;

	nla_for_each_nested(cur, prev, rem) {
		type = nla_type(cur);
		ch_bucket = scan_param->channel_bucket;
		switch (type) {
		case GSCAN_ATTRIBUTE_BUCKET_ID:
			break;
		case GSCAN_ATTRIBUTE_BUCKET_PERIOD:
			if (nla_len(cur) != sizeof(uint32)) {
				err = -EINVAL;
				goto exit;
			}

			ch_bucket[num].bucket_freq_multiple =
				nla_get_u32(cur) / MSEC_PER_SEC;
			break;
		case GSCAN_ATTRIBUTE_BUCKET_NUM_CHANNELS:
			if (nla_len(cur) != sizeof(uint32)) {
				err = -EINVAL;
				goto exit;
			}
			ch_bucket[num].num_channels = nla_get_u32(cur);
			if (ch_bucket[num].num_channels >
				GSCAN_MAX_CHANNELS_IN_BUCKET) {
				WL_ERR(("channel range:%d,bucket:%d\n",
					ch_bucket[num].num_channels,
					num));
				err = -EINVAL;
				goto exit;
			}
			break;
		case GSCAN_ATTRIBUTE_BUCKET_CHANNELS:
			nla_for_each_nested(next, cur, rem) {
				if (k >= GSCAN_MAX_CHANNELS_IN_BUCKET)
					break;
				if (nla_len(next) != sizeof(uint32)) {
					err = -EINVAL;
					goto exit;
				}
				ch_bucket[num].chan_list[k] = nla_get_u32(next);
				k++;
			}
			break;
		case GSCAN_ATTRIBUTE_BUCKETS_BAND:
			if (nla_len(cur) != sizeof(uint32)) {
				err = -EINVAL;
				goto exit;
			}
			ch_bucket[num].band = (uint16)nla_get_u32(cur);
			break;
		case GSCAN_ATTRIBUTE_REPORT_EVENTS:
			if (nla_len(cur) != sizeof(uint32)) {
				err = -EINVAL;
				goto exit;
			}
			ch_bucket[num].report_flag = (uint8)nla_get_u32(cur);
			break;
		case GSCAN_ATTRIBUTE_BUCKET_STEP_COUNT:
			if (nla_len(cur) != sizeof(uint32)) {
				err = -EINVAL;
				goto exit;
			}
			ch_bucket[num].repeat = (uint16)nla_get_u32(cur);
			break;
		case GSCAN_ATTRIBUTE_BUCKET_MAX_PERIOD:
			if (nla_len(cur) != sizeof(uint32)) {
				err = -EINVAL;
				goto exit;
			}
			ch_bucket[num].bucket_max_multiple =
				nla_get_u32(cur) / MSEC_PER_SEC;
			break;
		default:
			WL_ERR(("unknown attr type:%d\n", type));
			err = -EINVAL;
			goto exit;
		}
	}

exit:
	return err;
}

static int
wl_cfgvendor_set_scan_cfg(struct wiphy *wiphy, struct wireless_dev *wdev,
	const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	gscan_scan_params_t *scan_param;
	int j = 0;
	int type, tmp;
	const struct nlattr *iter;

	scan_param = (gscan_scan_params_t *)MALLOCZ(cfg->osh,
		sizeof(gscan_scan_params_t));
	if (!scan_param) {
		WL_ERR(("Could not set GSCAN scan cfg, mem alloc failure\n"));
		err = -EINVAL;
		return err;

	}

	scan_param->scan_fr = PNO_SCAN_MIN_FW_SEC;
	nla_for_each_attr(iter, data, len, tmp) {
		type = nla_type(iter);

		if (j >= GSCAN_MAX_CH_BUCKETS) {
			break;
		}

		switch (type) {
			case GSCAN_ATTRIBUTE_BASE_PERIOD:
				if (nla_len(iter) != sizeof(uint32)) {
					err = -EINVAL;
					goto exit;
				}
				scan_param->scan_fr = nla_get_u32(iter) / MSEC_PER_SEC;
				break;
			case GSCAN_ATTRIBUTE_NUM_BUCKETS:
				if (nla_len(iter) != sizeof(uint32)) {
					err = -EINVAL;
					goto exit;
				}
				scan_param->nchannel_buckets = nla_get_u32(iter);
				if (scan_param->nchannel_buckets >=
				    GSCAN_MAX_CH_BUCKETS) {
					WL_ERR(("ncha_buck out of range %d\n",
					scan_param->nchannel_buckets));
					err = -EINVAL;
					goto exit;
				}
				break;
			case GSCAN_ATTRIBUTE_CH_BUCKET_1:
			case GSCAN_ATTRIBUTE_CH_BUCKET_2:
			case GSCAN_ATTRIBUTE_CH_BUCKET_3:
			case GSCAN_ATTRIBUTE_CH_BUCKET_4:
			case GSCAN_ATTRIBUTE_CH_BUCKET_5:
			case GSCAN_ATTRIBUTE_CH_BUCKET_6:
			case GSCAN_ATTRIBUTE_CH_BUCKET_7:
				err = wl_cfgvendor_set_scan_cfg_bucket(iter, scan_param, j);
				if (err < 0) {
					WL_ERR(("set_scan_cfg_buck error:%d\n", err));
					goto exit;
				}
				j++;
				break;
			default:
				WL_ERR(("Unknown type %d\n", type));
				err = -EINVAL;
				goto exit;
		}
	}

	err = dhd_dev_pno_set_cfg_gscan(bcmcfg_to_prmry_ndev(cfg),
	     DHD_PNO_SCAN_CFG_ID, scan_param, FALSE);

	if (err < 0) {
		WL_ERR(("Could not set GSCAN scan cfg\n"));
		err = -EINVAL;
	}

exit:
	MFREE(cfg->osh, scan_param, sizeof(gscan_scan_params_t));
	return err;

}

static int
wl_cfgvendor_hotlist_cfg(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	gscan_hotlist_scan_params_t *hotlist_params;
	int tmp, tmp1, tmp2, type, j = 0, dummy;
	const struct nlattr *outer, *inner = NULL, *iter;
	bool flush = FALSE;
	struct bssid_t *pbssid;

	BCM_REFERENCE(dummy);

	if (len < sizeof(*hotlist_params) || len >= WLC_IOCTL_MAXLEN) {
		WL_ERR(("buffer length :%d wrong - bail out.\n", len));
		return -EINVAL;
	}

	hotlist_params = (gscan_hotlist_scan_params_t *)MALLOCZ(cfg->osh,
		sizeof(*hotlist_params)
		+ (sizeof(struct bssid_t) * (PFN_SWC_MAX_NUM_APS - 1)));

	if (!hotlist_params) {
		WL_ERR(("Cannot Malloc memory.\n"));
		return -ENOMEM;
	}

	hotlist_params->lost_ap_window = GSCAN_LOST_AP_WINDOW_DEFAULT;

	nla_for_each_attr(iter, data, len, tmp2) {
		type = nla_type(iter);
		switch (type) {
		case GSCAN_ATTRIBUTE_HOTLIST_BSSID_COUNT:
			if (nla_len(iter) != sizeof(uint32)) {
				WL_DBG(("type:%d length:%d not matching.\n",
					type, nla_len(iter)));
				err = -EINVAL;
				goto exit;
			}
			hotlist_params->nbssid = (uint16)nla_get_u32(iter);
			if ((hotlist_params->nbssid == 0) ||
			    (hotlist_params->nbssid > PFN_SWC_MAX_NUM_APS)) {
				WL_ERR(("nbssid:%d exceed limit.\n",
					hotlist_params->nbssid));
				err = -EINVAL;
				goto exit;
			}
			break;
		case GSCAN_ATTRIBUTE_HOTLIST_BSSIDS:
			if (hotlist_params->nbssid == 0) {
				WL_ERR(("nbssid not retrieved.\n"));
				err = -EINVAL;
				goto exit;
			}
			pbssid = hotlist_params->bssid;
			nla_for_each_nested(outer, iter, tmp) {
				if (j >= hotlist_params->nbssid)
					break;
				nla_for_each_nested(inner, outer, tmp1) {
					type = nla_type(inner);

					switch (type) {
					case GSCAN_ATTRIBUTE_BSSID:
						if (nla_len(inner) != sizeof(pbssid[j].macaddr)) {
							WL_ERR(("type:%d length:%d not matching.\n",
								type, nla_len(inner)));
							err = -EINVAL;
							goto exit;
						}
						memcpy(
							&(pbssid[j].macaddr),
							nla_data(inner),
							sizeof(pbssid[j].macaddr));
						break;
					case GSCAN_ATTRIBUTE_RSSI_LOW:
						if (nla_len(inner) != sizeof(uint8)) {
							WL_ERR(("type:%d length:%d not matching.\n",
								type, nla_len(inner)));
							err = -EINVAL;
							goto exit;
						}
						pbssid[j].rssi_reporting_threshold =
							(int8)nla_get_u8(inner);
						break;
					case GSCAN_ATTRIBUTE_RSSI_HIGH:
						if (nla_len(inner) != sizeof(uint8)) {
							WL_ERR(("type:%d length:%d not matching.\n",
								type, nla_len(inner)));
							err = -EINVAL;
							goto exit;
						}
						dummy = (int8)nla_get_u8(inner);
						WL_DBG(("dummy %d\n", dummy));
						break;
					default:
						WL_ERR(("ATTR unknown %d\n", type));
						err = -EINVAL;
						goto exit;
					}
				}
				j++;
			}
			if (j != hotlist_params->nbssid) {
				WL_ERR(("bssid_cnt:%d != nbssid:%d.\n", j,
					hotlist_params->nbssid));
				err = -EINVAL;
				goto exit;
			}
			break;
		case GSCAN_ATTRIBUTE_HOTLIST_FLUSH:
			if (nla_len(iter) != sizeof(uint8)) {
				WL_ERR(("type:%d length:%d not matching.\n",
					type, nla_len(iter)));
				err = -EINVAL;
				goto exit;
			}
			flush = nla_get_u8(iter);
			break;
		case GSCAN_ATTRIBUTE_LOST_AP_SAMPLE_SIZE:
			if (nla_len(iter) != sizeof(uint32)) {
				WL_ERR(("type:%d length:%d not matching.\n",
					type, nla_len(iter)));
				err = -EINVAL;
				goto exit;
			}
			hotlist_params->lost_ap_window = (uint16)nla_get_u32(iter);
			break;
		default:
			WL_ERR(("Unknown type %d\n", type));
			err = -EINVAL;
			goto exit;
		}

	}

	if (dhd_dev_pno_set_cfg_gscan(bcmcfg_to_prmry_ndev(cfg),
	      DHD_PNO_GEOFENCE_SCAN_CFG_ID, hotlist_params, flush) < 0) {
		WL_ERR(("Could not set GSCAN HOTLIST cfg error: %d\n", err));
		err = -EINVAL;
		goto exit;
	}
exit:
	MFREE(cfg->osh, hotlist_params, sizeof(*hotlist_params)
		+ (sizeof(struct bssid_t) * (PFN_SWC_MAX_NUM_APS - 1)));
	return err;
}

static int wl_cfgvendor_epno_cfg(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	dhd_pno_ssid_t *ssid_elem = NULL;
	int tmp, tmp1, tmp2, type = 0, num = 0;
	const struct nlattr *outer, *inner, *iter;
	uint8 flush = FALSE, i = 0;
	wl_ssid_ext_params_t params;

	nla_for_each_attr(iter, data, len, tmp2) {
		type = nla_type(iter);
		switch (type) {
			case GSCAN_ATTRIBUTE_EPNO_SSID_LIST:
				nla_for_each_nested(outer, iter, tmp) {
					ssid_elem = (dhd_pno_ssid_t *)
						dhd_dev_pno_get_gscan(bcmcfg_to_prmry_ndev(cfg),
						DHD_PNO_GET_NEW_EPNO_SSID_ELEM,
						NULL, &num);
					if (!ssid_elem) {
						WL_ERR(("Failed to get SSID LIST buffer\n"));
						err = -ENOMEM;
						goto exit;
					}
					i++;
					nla_for_each_nested(inner, outer, tmp1) {
						type = nla_type(inner);

						switch (type) {
							case GSCAN_ATTRIBUTE_EPNO_SSID:
								memcpy(ssid_elem->SSID,
								  nla_data(inner),
								  DOT11_MAX_SSID_LEN);
								break;
							case GSCAN_ATTRIBUTE_EPNO_SSID_LEN:
								ssid_elem->SSID_len =
									nla_get_u32(inner);
								if (ssid_elem->SSID_len >
									DOT11_MAX_SSID_LEN) {
									WL_ERR(("SSID too"
									"long %d\n",
									ssid_elem->SSID_len));
									err = -EINVAL;
									MFREE(cfg->osh, ssid_elem,
										num);
									goto exit;
								}
								break;
							case GSCAN_ATTRIBUTE_EPNO_FLAGS:
								ssid_elem->flags =
									nla_get_u32(inner);
								ssid_elem->hidden =
									((ssid_elem->flags &
									DHD_EPNO_HIDDEN_SSID) != 0);
								break;
							case GSCAN_ATTRIBUTE_EPNO_AUTH:
								ssid_elem->wpa_auth =
								        nla_get_u32(inner);
								break;
						}
					}
					if (!ssid_elem->SSID_len) {
						WL_ERR(("Broadcast SSID is illegal for ePNO\n"));
						err = -EINVAL;
						MFREE(cfg->osh, ssid_elem, num);
						goto exit;
					}
					dhd_pno_translate_epno_fw_flags(&ssid_elem->flags);
					dhd_pno_set_epno_auth_flag(&ssid_elem->wpa_auth);
					MFREE(cfg->osh, ssid_elem, num);
				}
				break;
			case GSCAN_ATTRIBUTE_EPNO_SSID_NUM:
				num = nla_get_u8(iter);
				break;
			case GSCAN_ATTRIBUTE_EPNO_FLUSH:
				flush = (bool)nla_get_u32(iter);
				/* Flush attribute is expected before any ssid attribute */
				if (i && flush) {
					WL_ERR(("Bad attributes\n"));
					err = -EINVAL;
					goto exit;
				}
				/* Need to flush driver and FW cfg */
				dhd_dev_pno_set_cfg_gscan(bcmcfg_to_prmry_ndev(cfg),
				DHD_PNO_EPNO_CFG_ID, NULL, flush);
				dhd_dev_flush_fw_epno(bcmcfg_to_prmry_ndev(cfg));
				break;
			case GSCAN_ATTRIBUTE_EPNO_5G_RSSI_THR:
				params.min5G_rssi = nla_get_s8(iter);
				break;
			case GSCAN_ATTRIBUTE_EPNO_2G_RSSI_THR:
				params.min2G_rssi = nla_get_s8(iter);
				break;
			case GSCAN_ATTRIBUTE_EPNO_INIT_SCORE_MAX:
				params.init_score_max = nla_get_s16(iter);
				break;
			case GSCAN_ATTRIBUTE_EPNO_CUR_CONN_BONUS:
				params.cur_bssid_bonus = nla_get_s16(iter);
				break;
			case GSCAN_ATTRIBUTE_EPNO_SAME_NETWORK_BONUS:
				params.same_ssid_bonus = nla_get_s16(iter);
				break;
			case GSCAN_ATTRIBUTE_EPNO_SECURE_BONUS:
				params.secure_bonus = nla_get_s16(iter);
				break;
			case GSCAN_ATTRIBUTE_EPNO_5G_BONUS:
				params.band_5g_bonus = nla_get_s16(iter);
				break;
			default:
				WL_ERR(("%s: No such attribute %d\n", __FUNCTION__, type));
				err = -EINVAL;
				goto exit;
			}
	}
	if (i != num) {
		WL_ERR(("%s: num_ssid %d does not match ssids sent %d\n", __FUNCTION__,
		     num, i));
		err = -EINVAL;
	}
exit:
	/* Flush all configs if error condition */
	if (err < 0) {
		dhd_dev_pno_set_cfg_gscan(bcmcfg_to_prmry_ndev(cfg),
		   DHD_PNO_EPNO_CFG_ID, NULL, TRUE);
		dhd_dev_flush_fw_epno(bcmcfg_to_prmry_ndev(cfg));
	} else if (type != GSCAN_ATTRIBUTE_EPNO_FLUSH) {
		/* If the last attribute was FLUSH, nothing else to do */
		dhd_dev_pno_set_cfg_gscan(bcmcfg_to_prmry_ndev(cfg),
		DHD_PNO_EPNO_PARAMS_ID, &params, FALSE);
		err = dhd_dev_set_epno(bcmcfg_to_prmry_ndev(cfg));
	}
	return err;
}

static int
wl_cfgvendor_set_batch_scan_cfg(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0, tmp, type;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	gscan_batch_params_t batch_param;
	const struct nlattr *iter;

	batch_param.mscan = batch_param.bestn = 0;
	batch_param.buffer_threshold = GSCAN_BATCH_NO_THR_SET;

	nla_for_each_attr(iter, data, len, tmp) {
		type = nla_type(iter);

		switch (type) {
			case GSCAN_ATTRIBUTE_NUM_AP_PER_SCAN:
				batch_param.bestn = nla_get_u32(iter);
				break;
			case GSCAN_ATTRIBUTE_NUM_SCANS_TO_CACHE:
				batch_param.mscan = nla_get_u32(iter);
				break;
			case GSCAN_ATTRIBUTE_REPORT_THRESHOLD:
				batch_param.buffer_threshold = nla_get_u32(iter);
				break;
			default:
				WL_ERR(("Unknown type %d\n", type));
				break;
		}
	}

	if (dhd_dev_pno_set_cfg_gscan(bcmcfg_to_prmry_ndev(cfg),
	       DHD_PNO_BATCH_SCAN_CFG_ID, &batch_param, FALSE) < 0) {
		WL_ERR(("Could not set batch cfg\n"));
		err = -EINVAL;
		return err;
	}

	return err;
}

#endif /* GSCAN_SUPPORT */
#if defined(GSCAN_SUPPORT) || defined(DHD_GET_VALID_CHANNELS)
static int
wl_cfgvendor_gscan_get_channel_list(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0, type, band;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	uint16 *reply = NULL;
	uint32 reply_len = 0, num_channels, mem_needed;
	struct sk_buff *skb;
	dhd_pub_t *dhdp;
	struct net_device *ndev = wdev->netdev;

	if (!ndev) {
		WL_ERR(("ndev null\n"));
		return -EINVAL;
	}

	dhdp = wl_cfg80211_get_dhdp(ndev);
	if (!dhdp) {
		WL_ERR(("dhdp null\n"));
		return -EINVAL;
	}

	if (!data) {
		WL_ERR(("data is not available\n"));
		return -EINVAL;
	}

	if (len <= 0) {
		WL_ERR(("invalid len %d\n", len));
		return -EINVAL;
	}

	type = nla_type(data);
	if (type == GSCAN_ATTRIBUTE_BAND) {
		band = nla_get_u32(data);
	} else {
		return -EINVAL;
	}

	reply = MALLOCZ(cfg->osh, CHANINFO_LIST_BUF_SIZE);
	if (reply == NULL) {
		WL_ERR(("failed to allocate chanspec buffer\n"));
		return -ENOMEM;
	}
	err = wl_cfgscan_get_band_freq_list(cfg, band, reply, &num_channels);
	if (err != BCME_OK && err != BCME_UNSUPPORTED) {
		WL_ERR(("%s: failed to get valid channel list\n",
				__FUNCTION__));
		err = -EINVAL;
		goto exit;
	} else if (err == BCME_OK) {
		reply_len = (num_channels * sizeof(uint32));
	} else if (err == BCME_UNSUPPORTED) {
		reply = dhd_pno_get_gscan(dhdp,
			DHD_PNO_GET_CHANNEL_LIST, &band, &reply_len);
		if (!reply) {
			WL_ERR(("Could not get channel list\n"));
			err = -EINVAL;
			return err;
		}
		num_channels =  reply_len/sizeof(uint32);
	}
	mem_needed = reply_len + VENDOR_REPLY_OVERHEAD + (ATTRIBUTE_U32_LEN * 2);

	/* Alloc the SKB for vendor_event */
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, mem_needed);
	if (unlikely(!skb)) {
		WL_ERR(("skb alloc failed"));
		err = -ENOMEM;
		goto exit;
	}

	nla_put_u32(skb, GSCAN_ATTRIBUTE_NUM_CHANNELS, num_channels);
	nla_put(skb, GSCAN_ATTRIBUTE_CHANNEL_LIST, reply_len, reply);

	err =  cfg80211_vendor_cmd_reply(skb);

	if (unlikely(err)) {
		WL_ERR(("Vendor Command reply failed ret:%d \n", err));
	}
exit:
	MFREE(cfg->osh, reply, CHANINFO_LIST_BUF_SIZE);
	return err;
}
#endif	/* GSCAN_SUPPORT || DHD_GET_VALID_CHANNELS */

#ifdef RSSI_MONITOR_SUPPORT
static int wl_cfgvendor_set_rssi_monitor(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0, tmp, type, start = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int8 max_rssi = 0, min_rssi = 0;
	const struct nlattr *iter;

	if (!wl_get_drv_status(cfg, CONNECTED, wdev_to_ndev(wdev))) {
		WL_ERR(("Sta is not connected to an AP, rssi monitoring is not allowed\n"));
		return -EINVAL;
	}

	nla_for_each_attr(iter, data, len, tmp) {
		type = nla_type(iter);
		switch (type) {
			case RSSI_MONITOR_ATTRIBUTE_MAX_RSSI:
				max_rssi = (int8) nla_get_u32(iter);
				break;
			case RSSI_MONITOR_ATTRIBUTE_MIN_RSSI:
				min_rssi = (int8) nla_get_u32(iter);
				break;
			case RSSI_MONITOR_ATTRIBUTE_START:
				start = nla_get_u32(iter);
		}
	}

	if (dhd_dev_set_rssi_monitor_cfg(bcmcfg_to_prmry_ndev(cfg),
	       start, max_rssi, min_rssi) < 0) {
		WL_ERR(("Could not set rssi monitor cfg\n"));
		err = -EINVAL;
	}
	return err;
}
#endif /* RSSI_MONITOR_SUPPORT */

#ifdef DHD_WAKE_STATUS
static int
wl_cfgvendor_get_wake_reason_stats(struct wiphy *wiphy,
        struct wireless_dev *wdev, const void *data, int len)
{
	struct net_device *ndev = wdev_to_ndev(wdev);
	wake_counts_t *pwake_count_info;
	int ret, mem_needed;
#if defined(DHD_DEBUG) && defined(DHD_WAKE_EVENT_STATUS)
	int flowid;
#ifdef CUSTOM_WAKE_REASON_STATS
	int tmp_rc_event[MAX_WAKE_REASON_STATS];
	int rc_event_used_cnt = 0;
	int front = 0;
#endif /* CUSTOM_WAKE_REASON_STATS */
#endif /* DHD_DEBUG && DHD_WAKE_EVENT_STATUS */
	struct sk_buff *skb = NULL;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(ndev);

	WL_DBG(("Recv get wake status info cmd.\n"));

	pwake_count_info = dhd_get_wakecount(dhdp);
	mem_needed =  VENDOR_REPLY_OVERHEAD + (ATTRIBUTE_U32_LEN * 20) +
		(WLC_E_LAST * sizeof(uint));

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, mem_needed);
	if (unlikely(!skb)) {
		WL_ERR(("%s: can't allocate %d bytes\n", __FUNCTION__, mem_needed));
		ret = -ENOMEM;
		goto exit;
	}
#ifdef DHD_WAKE_EVENT_STATUS
	WL_ERR(("pwake_count_info->rcwake %d\n", pwake_count_info->rcwake));
#ifdef CUSTOM_WAKE_REASON_STATS
	ret = nla_put_u32(skb, WAKE_STAT_ATTRIBUTE_TOTAL_DRIVER_FW, 0);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put total count of driver fw\n"));
		goto exit;
	}
#endif /* CUSTOM_WAKE_REASON_STATS */
	ret = nla_put_u32(skb, WAKE_STAT_ATTRIBUTE_TOTAL_CMD_EVENT, pwake_count_info->rcwake);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put Total count of CMD event, ret=%d\n", ret));
		goto exit;
	}
#ifdef CUSTOM_WAKE_REASON_STATS
	front = pwake_count_info->rc_event_idx - 1;
	for (flowid = 0; flowid < MAX_WAKE_REASON_STATS; flowid++) {
		/* Reorder the rc_event, the latest event in the header index */
		if (front < 0) {
			front = MAX_WAKE_REASON_STATS - 1;
		}
		if (pwake_count_info->rc_event[front] != -1) {
			rc_event_used_cnt++;
		}
		tmp_rc_event[flowid] = pwake_count_info->rc_event[front];
		front--;
	}

	ret = nla_put_u32(skb, WAKE_STAT_ATTRIBUTE_CMD_EVENT_COUNT_USED, rc_event_used_cnt);
#endif /* CUSTOM_WAKE_REASON_STATS */
	if (unlikely(ret)) {
		WL_ERR(("Failed to put Max count of event used, ret=%d\n", ret));
		goto exit;
	}
#ifdef CUSTOM_WAKE_REASON_STATS
	ret = nla_put(skb, WAKE_STAT_ATTRIBUTE_CMD_EVENT_WAKE,
		(MAX_WAKE_REASON_STATS * sizeof(int)), tmp_rc_event);
#else
	ret = nla_put(skb, WAKE_STAT_ATTRIBUTE_CMD_EVENT_WAKE, (WLC_E_LAST * sizeof(uint)),
		pwake_count_info->rc_event);
#endif /* CUSTOM_WAKE_REASON_STATS */
	if (unlikely(ret)) {
		WL_ERR(("Failed to put Event wake data, ret=%d\n", ret));
		goto exit;
	}

#ifdef DHD_DEBUG
#ifdef CUSTOM_WAKE_REASON_STATS
	for (flowid = 0; flowid < MAX_WAKE_REASON_STATS; flowid++) {
		if (pwake_count_info->rc_event[flowid] != -1) {
			WL_INFORM(("Event ID %u = %s\n", pwake_count_info->rc_event[flowid],
				bcmevent_get_name(pwake_count_info->rc_event[flowid])));
		}
	}
#else
	for (flowid = 0; flowid < WLC_E_LAST; flowid++) {
		if (pwake_count_info->rc_event[flowid] != 0) {
			WL_ERR((" %s = %u\n", bcmevent_get_name(flowid),
				pwake_count_info->rc_event[flowid]));
		}
	}
#endif /* CUSTOM_WAKE_REASON_STATS */
#endif /* DHD_DEBUG */
#endif /* DHD_WAKE_EVENT_STATUS */
#ifdef DHD_WAKE_RX_STATUS
	WL_ERR(("pwake_count_info->rxwake %d\n", pwake_count_info->rxwake));
	ret = nla_put_u32(skb, WAKE_STAT_ATTRIBUTE_TOTAL_RX_DATA_WAKE, pwake_count_info->rxwake);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put Total Wake due RX data, ret=%d\n", ret));
		goto exit;
	}
	ret = nla_put_u32(skb, WAKE_STAT_ATTRIBUTE_RX_UNICAST_COUNT, pwake_count_info->rx_ucast);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put Total wake due to RX unicast, ret=%d\n", ret));
		goto exit;
	}
	ret = nla_put_u32(skb, WAKE_STAT_ATTRIBUTE_RX_MULTICAST_COUNT, pwake_count_info->rx_mcast);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put Total wake due RX multicast, ret=%d\n", ret));
		goto exit;
	}
	ret = nla_put_u32(skb, WAKE_STAT_ATTRIBUTE_RX_BROADCAST_COUNT, pwake_count_info->rx_bcast);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put Total wake due to RX broadcast, ret=%d\n", ret));
		goto exit;
	}
	ret = nla_put_u32(skb, WAKE_STAT_ATTRIBUTE_RX_ICMP_PKT, pwake_count_info->rx_arp);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put Total wake due to ICMP pkt, ret=%d\n", ret));
		goto exit;
	}
	ret = nla_put_u32(skb, WAKE_STAT_ATTRIBUTE_RX_ICMP6_PKT, pwake_count_info->rx_icmpv6);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put Total wake due ICMPV6 pkt, ret=%d\n", ret));
		goto exit;
	}
	ret = nla_put_u32(skb, WAKE_STAT_ATTRIBUTE_RX_ICMP6_RA, pwake_count_info->rx_icmpv6_ra);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put Total wake due to ICMPV6_RA, ret=%d\n", ret));
		goto exit;
	}
	ret = nla_put_u32(skb, WAKE_STAT_ATTRIBUTE_RX_ICMP6_NA, pwake_count_info->rx_icmpv6_na);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put Total wake due to ICMPV6_NA, ret=%d\n", ret));
		goto exit;
	}
	ret = nla_put_u32(skb, WAKE_STAT_ATTRIBUTE_RX_ICMP6_NS, pwake_count_info->rx_icmpv6_ns);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put Total wake due to ICMPV6_NS, ret=%d\n", ret));
		goto exit;
	}
	ret = nla_put_u32(skb, WAKE_STAT_ATTRIBUTE_IPV4_RX_MULTICAST_ADD_CNT,
		pwake_count_info->rx_multi_ipv4);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put Total wake due to RX IPV4 MULTICAST, ret=%d\n", ret));
		goto exit;
	}
	ret = nla_put_u32(skb, WAKE_STAT_ATTRIBUTE_IPV6_RX_MULTICAST_ADD_CNT,
		pwake_count_info->rx_multi_ipv6);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put Total wake due to RX IPV6 MULTICAST, ret=%d\n", ret));
		goto exit;
	}
	ret = nla_put_u32(skb, WAKE_STAT_ATTRIBUTE_OTHER_RX_MULTICAST_ADD_CNT,
		pwake_count_info->rx_multi_other);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put Total wake due to Other RX Multicast, ret=%d\n", ret));
		goto exit;
	}
#endif /* #ifdef DHD_WAKE_RX_STATUS */
	ret = cfg80211_vendor_cmd_reply(skb);
	if (unlikely(ret)) {
		WL_ERR(("Vendor cmd reply for -get wake status failed:%d \n", ret));
	}
	/* On cfg80211_vendor_cmd_reply() skb is consumed and freed in case of success or failure */
	return ret;

exit:
	/* Free skb memory */
	if (skb) {
		kfree_skb(skb);
	}
	return ret;
}
#endif /* DHD_WAKE_STATUS */

#ifdef DHDTCPACK_SUPPRESS
static int
wl_cfgvendor_set_tcpack_sup_mode(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int len)
{
	int err = BCME_OK, type;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct net_device *ndev = wdev_to_wlc_ndev(wdev, cfg);
	uint8 enable = 0;

	if (!data) {
		WL_ERR(("data is not available\n"));
		err = BCME_BADARG;
		goto exit;
	}

	if (len <= 0) {
		WL_ERR(("Length of the nlattr is not valid len : %d\n", len));
		err = BCME_BADARG;
		goto exit;
	}

	type = nla_type(data);
	if (type == ANDR_WIFI_ATTRIBUTE_TCPACK_SUP_VALUE) {
		enable = (uint8) nla_get_u32(data);
		err = dhd_dev_set_tcpack_sup_mode_cfg(ndev, enable);
		if (unlikely(err)) {
			WL_ERR(("Could not set TCP Ack Suppress mode cfg: %d\n", err));
		}
	} else {
		err = BCME_BADARG;
	}

exit:
	return err;
}
#endif /* DHDTCPACK_SUPPRESS */

#if defined(WL_CFG80211) && defined(DHD_FILE_DUMP_EVENT)
static int
wl_cfgvendor_notify_dump_completion(struct wiphy *wiphy,
        struct wireless_dev *wdev, const void *data, int len)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	dhd_pub_t *dhd_pub = cfg->pub;
	unsigned long flags = 0;

	WL_INFORM(("%s, [DUMP] received file dump notification from HAL\n", __FUNCTION__));

	DHD_GENERAL_LOCK(dhd_pub, flags);
	/* call wmb() to synchronize with the previous memory operations */
	OSL_SMP_WMB();
	DHD_BUS_BUSY_CLEAR_IN_HALDUMP(dhd_pub);
	dhd_set_dump_status(dhd_pub, DUMP_READY);
	/* Call another wmb() to make sure wait_for_dump_completion value
	 * gets updated before waking up waiting context.
	 */
	OSL_SMP_WMB();
	dhd_os_busbusy_wake(dhd_pub);
	DHD_GENERAL_UNLOCK(dhd_pub, flags);

	return BCME_OK;
}
#endif /* WL_CFG80211 && DHD_FILE_DUMP_EVENT */

#if defined(WL_CFG80211)
static int
wl_cfgvendor_set_hal_pid(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void  *data, int len)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int ret = BCME_OK;
	uint32 type;
	if (!data) {
		WL_DBG(("%s,data is not available\n", __FUNCTION__));
	} else {
		if (len > 0) {
			type = nla_type(data);
			if (type == SET_HAL_START_ATTRIBUTE_EVENT_SOCK_PID) {
				if (nla_len(data)) {
					WL_DBG(("HAL PID = %u\n", nla_get_u32(data)));
					cfg->halpid = nla_get_u32(data);
				}
			}
		} else {
			WL_ERR(("invalid len %d\n", len));
			ret = BCME_ERROR;
		}
	}
	return ret;
}

static int
wl_cfgvendor_set_hal_started(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void  *data, int len)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
#ifdef WL_STA_ASSOC_RAND
	struct ether_addr primary_mac;
	dhd_pub_t *dhd = (dhd_pub_t *)(cfg->pub);
#endif /* WL_STA_ASSOC_RAND */
	int ret = BCME_OK;
#if defined(WIFI_TURNON_USE_HALINIT)
	struct net_device *ndev = wdev_to_wlc_ndev(wdev, cfg);
	uint32 type;

	if (!data) {
		WL_DBG(("%s,data is not available\n", __FUNCTION__));
	} else {
		if (len > 0) {
			type = nla_type(data);
			WL_INFORM(("%s,type: %xh\n", __FUNCTION__, type));
			if (type == SET_HAL_START_ATTRIBUTE_PRE_INIT) {
				if (nla_len(data)) {
					WL_INFORM(("%s, HAL version: %s\n", __FUNCTION__,
							(char*)nla_data(data)));
				}
				WL_INFORM(("%s, dhd_open start\n", __FUNCTION__));
				ret = dhd_open(ndev);
				if (ret != BCME_OK) {
					WL_INFORM(("%s, dhd_open failed\n", __FUNCTION__));
					return ret;
				} else {
					WL_INFORM(("%s, dhd_open succeeded\n", __FUNCTION__));
				}
				return ret;
			}
		} else {
			WL_ERR(("invalid len %d\n", len));
		}
	}
#endif /* WIFI_TURNON_USE_HALINIT */
	RETURN_EIO_IF_NOT_UP(cfg);
	WL_INFORM(("%s,[DUMP] HAL STARTED\n", __FUNCTION__));

	cfg->hal_started = true;
#ifdef DHD_FILE_DUMP_EVENT
	dhd_set_dump_status(dhd, DUMP_READY);
#endif /* DHD_FILE_DUMP_EVENT */
#ifdef WL_STA_ASSOC_RAND
	/* If mac randomization is enabled and primary macaddress is not
	 * randomized, randomize it from HAL init context
	 */
	get_primary_mac(cfg, &primary_mac);
	if ((!ETHER_IS_LOCALADDR(&primary_mac)) &&
		(!wl_get_drv_status(cfg, CONNECTED, wdev_to_ndev(wdev)))) {
		WL_DBG_MEM(("%s, Local admin bit not set, randomize"
			"STA MAC address \n", __FUNCTION__));
		if ((ret = dhd_update_rand_mac_addr(dhd)) < 0) {
			WL_ERR(("%s: failed to set macaddress, ret = %d\n", __FUNCTION__, ret));
			return ret;
		}
	}
#endif /* WL_STA_ASSOC_RAND */
	return ret;
}

static int
wl_cfgvendor_stop_hal(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void  *data, int len)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
#ifdef DHD_FILE_DUMP_EVENT
	dhd_pub_t *dhd = (dhd_pub_t *)(cfg->pub);
#endif /* DHD_FILE_DUMP_EVENT */

	WL_INFORM(("%s,[DUMP] HAL STOPPED\n", __FUNCTION__));

	cfg->hal_started = false;
#ifdef DHD_FILE_DUMP_EVENT
	dhd_set_dump_status(dhd, DUMP_NOT_READY);
#endif /* DHD_FILE_DUMP_EVENT */
	return BCME_OK;
}
#endif /* WL_CFG80211 */

#ifdef RTT_SUPPORT
void
wl_cfgvendor_rtt_evt(void *ctx, void *rtt_data)
{
	struct wireless_dev *wdev = (struct wireless_dev *)ctx;
	struct wiphy *wiphy;
	struct sk_buff *skb = NULL;
	uint32 evt_complete = 0;
	gfp_t kflags;
	rtt_result_t *rtt_result;
	rtt_results_header_t *rtt_header;
	struct list_head *rtt_cache_list;
	struct nlattr *rtt_nl_hdr;
	int ret = BCME_OK;
	wiphy = wdev->wiphy;

	WL_DBG(("In\n"));
	/* Push the data to the skb */
	if (!rtt_data) {
		WL_ERR(("rtt_data is NULL\n"));
		return;
	}
	rtt_cache_list = (struct list_head *)rtt_data;
	kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	if (list_empty(rtt_cache_list)) {
#if (defined(CONFIG_ARCH_MSM) && defined(SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC)) || \
	LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
		skb = cfg80211_vendor_event_alloc(wiphy, NULL, 100,
			GOOGLE_RTT_COMPLETE_EVENT, kflags);
#else
		skb = cfg80211_vendor_event_alloc(wiphy, 100, GOOGLE_RTT_COMPLETE_EVENT, kflags);
#endif /* (defined(CONFIG_ARCH_MSM) && defined(SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC)) || */
		/* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0) */
		if (!skb) {
			WL_ERR(("skb alloc failed"));
			return;
		}
		evt_complete = 1;
		ret = nla_put_u32(skb, RTT_ATTRIBUTE_RESULTS_COMPLETE, evt_complete);
		if (ret < 0) {
			WL_ERR(("Failed to put RTT_ATTRIBUTE_RESULTS_COMPLETE\n"));
			goto free_mem;
		}
		cfg80211_vendor_event(skb, kflags);
		return;
	}
	GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST();
	list_for_each_entry(rtt_header, rtt_cache_list, list) {
		/* Alloc the SKB for vendor_event */
#if (defined(CONFIG_ARCH_MSM) && defined(SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC)) || \
	LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
		skb = cfg80211_vendor_event_alloc(wiphy, NULL, rtt_header->result_tot_len + 100,
			GOOGLE_RTT_COMPLETE_EVENT, kflags);
#else
		skb = cfg80211_vendor_event_alloc(wiphy, rtt_header->result_tot_len + 100,
			GOOGLE_RTT_COMPLETE_EVENT, kflags);
#endif /* (defined(CONFIG_ARCH_MSM) && defined(SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC)) || */
		/* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0) */
		if (!skb) {
			WL_ERR(("skb alloc failed"));
			return;
		}
		if (list_is_last(&rtt_header->list, rtt_cache_list)) {
			evt_complete = 1;
		}
		ret = nla_put_u32(skb, RTT_ATTRIBUTE_RESULTS_COMPLETE, evt_complete);
		if (ret < 0) {
			WL_ERR(("Failed to put RTT_ATTRIBUTE_RESULTS_COMPLETE\n"));
			goto free_mem;
		}
		rtt_nl_hdr = nla_nest_start(skb, RTT_ATTRIBUTE_RESULTS_PER_TARGET);
		if (!rtt_nl_hdr) {
			WL_ERR(("rtt_nl_hdr is NULL\n"));
		        dev_kfree_skb_any(skb);
			break;
		}
		ret = nla_put(skb, RTT_ATTRIBUTE_TARGET_MAC, ETHER_ADDR_LEN,
				&rtt_header->peer_mac);
		if (ret < 0) {
			WL_ERR(("Failed to put RTT_ATTRIBUTE_TARGET_MAC, ret:%d\n", ret));
			goto free_mem;
		}
		ret = nla_put_u32(skb, RTT_ATTRIBUTE_RESULT_CNT, rtt_header->result_cnt);
		if (ret < 0) {
			WL_ERR(("Failed to put RTT_ATTRIBUTE_RESULT_CNT, ret:%d\n", ret));
			goto free_mem;
		}
		list_for_each_entry(rtt_result, &rtt_header->result_list, list) {
#ifdef WL_RTT_LCI
			WL_DBG(("rtt_result report len=%d(%lu)\n",
				rtt_result->report_len, RTT_REPORT_SIZE));
			if (rtt_result->report_len > RTT_REPORT_SIZE) {
				struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
				int msize = rtt_result->report_len;
				char *mbuf;
				int plen;
				char *pdata;

				mbuf = (char *)MALLOCZ(cfg->osh, msize);
				if (!mbuf) {
					WL_ERR(("Failed to malloc buf for LCI/LCR, len(%d)\n",
						msize));
					goto free_mem;
				}
				pdata = mbuf;
				plen = msize;
				(void) memcpy_s(pdata, plen, &rtt_result->report, RTT_REPORT_SIZE);
				pdata += RTT_REPORT_SIZE;
				plen -= RTT_REPORT_SIZE;
				if (rtt_result->report.LCI) {
					bcm_tlv_t *tlv = rtt_result->report.LCI;
					int tlv_size = BCM_TLV_SIZE(tlv);
					if (tlv_size > plen) {
						WL_ERR(("LCI IE size error, len %d(%d)\n",
							tlv_size, plen));
					} else {
						(void) memcpy_s(pdata, plen, (char*)tlv, tlv_size);
						pdata += tlv_size;
						plen -= tlv_size;
						WL_INFORM_MEM(("copy LCI, len=%d\n", tlv_size));
					}
				}
				if (rtt_result->report.LCR) {
					bcm_tlv_t *tlv = rtt_result->report.LCR;
					int tlv_size = BCM_TLV_SIZE(tlv);
					if (tlv_size > plen) {
						WL_ERR(("LCR IE size error, len %d(%d)\n",
							tlv_size, plen));
					} else {
						(void) memcpy_s(pdata, plen, (char*)tlv, tlv_size);
						pdata += tlv_size;
						plen -= tlv_size;
						WL_INFORM_MEM(("copy LCR, len=%d\n", tlv_size));
					}
				}
				ret = nla_put(skb, RTT_ATTRIBUTE_RESULT, msize - plen, mbuf);
				MFREE(cfg->osh, mbuf, msize);
			} else
#endif /* WL_RTT_LCI */
			{
				ret = nla_put(skb, RTT_ATTRIBUTE_RESULT,
					rtt_result->report_len, &rtt_result->report);
			}

			if (ret < 0) {
				WL_ERR(("Failed to put RTT_ATTRIBUTE_RESULT, ret:%d\n", ret));
				goto free_mem;
			}
			ret = nla_put(skb, RTT_ATTRIBUTE_RESULT_DETAIL,
				rtt_result->detail_len, &rtt_result->rtt_detail);
			if (ret < 0) {
				WL_ERR(("Failed to put RTT_ATTRIBUTE_RESULT_DETAIL, ret:%d\n",
					ret));
				goto free_mem;
			}
		}
		nla_nest_end(skb, rtt_nl_hdr);
		cfg80211_vendor_event(skb, kflags);
	}
	GCC_DIAGNOSTIC_POP();

	return;

free_mem:
	/* Free skb memory */
	if (skb) {
		kfree_skb(skb);
	}
}

static int
wl_cfgvendor_rtt_set_config(struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int len) {
	int err = 0, rem, rem1, rem2, type;
	int target_cnt = 0;
	rtt_config_params_t rtt_param;
	rtt_target_info_t* rtt_target = NULL;
	const struct nlattr *iter, *iter1, *iter2;
	int8 eabuf[ETHER_ADDR_STR_LEN];
	int8 chanbuf[CHANSPEC_STR_LEN];
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	rtt_capabilities_t capability;

	bzero(&rtt_param, sizeof(rtt_param));

	WL_DBG(("In\n"));
	err = dhd_dev_rtt_register_noti_callback(wdev->netdev, wdev, wl_cfgvendor_rtt_evt);
	if (err < 0) {
		WL_ERR(("failed to register rtt_noti_callback\n"));
		goto exit;
	}
	err = dhd_dev_rtt_capability(bcmcfg_to_prmry_ndev(cfg), &capability);
	if (err < 0) {
		WL_ERR(("failed to get the capability\n"));
		goto exit;
	}

	if (len <= 0) {
		WL_ERR(("Length of the nlattr is not valid len : %d\n", len));
		err = BCME_ERROR;
		goto exit;
	}
	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		switch (type) {
		case RTT_ATTRIBUTE_TARGET_CNT:
			if (target_cnt != 0) {
				WL_ERR(("attempt to overwrite target_cnt"));
				err = -EINVAL;
				goto exit;
			}
			target_cnt = nla_get_u8(iter);
			if ((target_cnt <= 0) || (target_cnt > RTT_MAX_TARGET_CNT)) {
				WL_ERR(("target_cnt is not valid : %d\n",
					target_cnt));
				err = BCME_RANGE;
				goto exit;
			}
			rtt_param.rtt_target_cnt = target_cnt;

			rtt_param.target_info = (rtt_target_info_t *)MALLOCZ(cfg->osh,
				TARGET_INFO_SIZE(target_cnt));
			if (rtt_param.target_info == NULL) {
				WL_ERR(("failed to allocate target info for (%d)\n", target_cnt));
				err = BCME_NOMEM;
				goto exit;
			}
			break;
		case RTT_ATTRIBUTE_TARGET_INFO:
			/* Added this variable for safe check to avoid crash
			 * incase the caller did not respect the order
			 */
			if (rtt_param.target_info == NULL) {
				WL_ERR(("rtt_target_info is NULL\n"));
				err = BCME_NOMEM;
				goto exit;
			}
			rtt_target = rtt_param.target_info;
			nla_for_each_nested(iter1, iter, rem1) {
				if ((uint8 *)rtt_target >= ((uint8 *)rtt_param.target_info +
					TARGET_INFO_SIZE(target_cnt))) {
					WL_ERR(("rtt_target increased over its max size"));
					err = -EINVAL;
					goto exit;
				}
				nla_for_each_nested(iter2, iter1, rem2) {
					type = nla_type(iter2);
					switch (type) {
					case RTT_ATTRIBUTE_TARGET_MAC:
						if (nla_len(iter2) != ETHER_ADDR_LEN) {
							WL_ERR(("mac_addr length not match\n"));
							err = -EINVAL;
							goto exit;
						}
						memcpy(&rtt_target->addr, nla_data(iter2),
							ETHER_ADDR_LEN);
						break;
					case RTT_ATTRIBUTE_TARGET_TYPE:
						rtt_target->type = nla_get_u8(iter2);
						if (rtt_target->type == RTT_INVALID ||
							(rtt_target->type == RTT_ONE_WAY &&
							!capability.rtt_one_sided_supported)) {
							WL_ERR(("doesn't support RTT type"
								" : %d\n",
								rtt_target->type));
							err = -EINVAL;
							goto exit;
						}
						break;
					case RTT_ATTRIBUTE_TARGET_PEER:
						rtt_target->peer = nla_get_u8(iter2);
						break;
					case RTT_ATTRIBUTE_TARGET_CHAN:
						memcpy(&rtt_target->channel, nla_data(iter2),
							sizeof(rtt_target->channel));
						break;
					case RTT_ATTRIBUTE_TARGET_PERIOD:
						rtt_target->burst_period = nla_get_u32(iter2);
						if (rtt_target->burst_period < 32) {
							/* 100ms unit */
							rtt_target->burst_period *= 100;
						} else {
							WL_ERR(("%d value must in (0-31)\n",
								rtt_target->burst_period));
							err = EINVAL;
							goto exit;
						}
						break;
					case RTT_ATTRIBUTE_TARGET_NUM_BURST:
						rtt_target->num_burst = nla_get_u32(iter2);
						if (rtt_target->num_burst > 16) {
							WL_ERR(("%d value must in (0-15)\n",
								rtt_target->num_burst));
							err = -EINVAL;
							goto exit;
						}
						rtt_target->num_burst = BIT(rtt_target->num_burst);
						break;
					case RTT_ATTRIBUTE_TARGET_NUM_FTM_BURST:
						rtt_target->num_frames_per_burst =
						nla_get_u32(iter2);
						break;
					case RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTM:
						rtt_target->num_retries_per_ftm =
						nla_get_u32(iter2);
						break;
					case RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTMR:
						rtt_target->num_retries_per_ftmr =
						nla_get_u32(iter2);
						if (rtt_target->num_retries_per_ftmr > 3) {
							WL_ERR(("%d value must in (0-3)\n",
								rtt_target->num_retries_per_ftmr));
							err = -EINVAL;
							goto exit;
						}
						break;
					case RTT_ATTRIBUTE_TARGET_LCI:
						rtt_target->LCI_request = nla_get_u8(iter2);
						break;
					case RTT_ATTRIBUTE_TARGET_LCR:
						rtt_target->LCR_request = nla_get_u8(iter2);
						break;
					case RTT_ATTRIBUTE_TARGET_BURST_DURATION:
						if ((nla_get_u32(iter2) > 1 &&
							nla_get_u32(iter2) < 12)) {
							rtt_target->burst_duration =
							dhd_rtt_idx_to_burst_duration(
								nla_get_u32(iter2));
						} else if (nla_get_u32(iter2) == 15) {
							/* use default value */
							rtt_target->burst_duration = 0;
						} else {
							WL_ERR(("%d value must in (2-11) or 15\n",
								nla_get_u32(iter2)));
							err = -EINVAL;
							goto exit;
						}
						break;
					case RTT_ATTRIBUTE_TARGET_BW:
						rtt_target->bw = nla_get_u8(iter2);
						break;
					case RTT_ATTRIBUTE_TARGET_PREAMBLE:
						rtt_target->preamble = nla_get_u8(iter2);
						break;
					}
				}
				/* convert to chanspec value */
				rtt_target->chanspec =
					dhd_rtt_convert_to_chspec(rtt_target->channel);
				if (rtt_target->chanspec == 0) {
					WL_ERR(("Channel is not valid \n"));
					err = -EINVAL;
					goto exit;
				}
				WL_INFORM_MEM(("Target addr %s, Channel : %s for RTT \n",
					bcm_ether_ntoa((const struct ether_addr *)&rtt_target->addr,
					eabuf),
					wf_chspec_ntoa(rtt_target->chanspec, chanbuf)));
				rtt_target++;
			}
			break;
		}
	}
	WL_DBG(("leave :target_cnt : %d\n", rtt_param.rtt_target_cnt));
	if (dhd_dev_rtt_set_cfg(bcmcfg_to_prmry_ndev(cfg), &rtt_param) < 0) {
		WL_ERR(("Could not set RTT configuration\n"));
		err = -EINVAL;
	}
exit:
	/* free the target info list */
	if (rtt_param.target_info) {
		MFREE(cfg->osh, rtt_param.target_info,
			TARGET_INFO_SIZE(target_cnt));
	}
	return err;
}

static int
wl_cfgvendor_rtt_cancel_config(struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int len)
{
	int err = 0, rem, type, target_cnt = 0;
	int target_idx = 0;
	const struct nlattr *iter;
	struct ether_addr *mac_list = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);

	if (len <= 0) {
		WL_ERR(("Length of nlattr is not valid len : %d\n", len));
		err = -EINVAL;
		goto exit;
	}
	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		switch (type) {
		case RTT_ATTRIBUTE_TARGET_CNT:
			if (mac_list != NULL) {
				WL_ERR(("mac_list is not NULL\n"));
				err = -EINVAL;
				goto exit;
			}
			target_cnt = nla_get_u8(iter);
			if ((target_cnt > 0) && (target_cnt < RTT_MAX_TARGET_CNT)) {
				mac_list = (struct ether_addr *)MALLOCZ(cfg->osh,
					target_cnt * ETHER_ADDR_LEN);
				if (mac_list == NULL) {
					WL_ERR(("failed to allocate mem for mac list\n"));
					err = -EINVAL;
					goto exit;
				}
			} else {
				/* cancel the current whole RTT process */
				goto cancel;
			}
			break;
		case RTT_ATTRIBUTE_TARGET_MAC:
			if (mac_list == NULL) {
				WL_ERR(("ATTRIBUTE_TARGET_CNT not found before "
						" ATTRIBUTE_TARGET_MAC\n"));
				err = -EINVAL;
				goto exit;
			}

			if (target_idx >= target_cnt) {
				WL_ERR(("More TARGET_MAC entries found, "
						"expected TARGET_CNT:%d\n", target_cnt));
				err = -EINVAL;
				goto exit;
			}

			if (nla_len(iter) != ETHER_ADDR_LEN) {
				WL_ERR(("Invalid TARGET_MAC ATTR len :%d\n", nla_len(iter)));
				err = -EINVAL;
				goto exit;
			}

			memcpy(&mac_list[target_idx], nla_data(iter), ETHER_ADDR_LEN);
			target_idx++;

			break;
		default:
			WL_ERR(("Uknown type : %d\n", type));
			err = -EINVAL;
			goto exit;
		}
	}
cancel:
	if (mac_list && dhd_dev_rtt_cancel_cfg(
		bcmcfg_to_prmry_ndev(cfg), mac_list, target_cnt) < 0) {
		WL_ERR(("Could not cancel RTT configuration\n"));
		err = -EINVAL;
	}

exit:
	if (mac_list) {
		MFREE(cfg->osh, mac_list, target_cnt * ETHER_ADDR_LEN);
	}
	return err;
}

static int
wl_cfgvendor_rtt_get_capability(struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	rtt_capabilities_t capability;

	err = dhd_dev_rtt_capability(bcmcfg_to_prmry_ndev(cfg), &capability);
	if (unlikely(err)) {
		WL_ERR(("Vendor Command reply failed ret:%d \n", err));
		goto exit;
	}
	err =  wl_cfgvendor_send_cmd_reply(wiphy, &capability, sizeof(capability));

	if (unlikely(err)) {
		WL_ERR(("Vendor Command reply failed ret:%d \n", err));
	}
exit:
	return err;
}
static int
get_responder_info(struct bcm_cfg80211 *cfg,
	struct wifi_rtt_responder *responder_info)
{
	int err = 0;
	rtt_capabilities_t capability;
	err = dhd_dev_rtt_capability(bcmcfg_to_prmry_ndev(cfg), &capability);
	if (unlikely(err)) {
		WL_ERR(("Could not get responder capability:%d \n", err));
		return err;
	}
	if (capability.preamble_support & RTT_PREAMBLE_VHT) {
		responder_info->preamble = RTT_PREAMBLE_VHT;
	} else if (capability.preamble_support & RTT_PREAMBLE_HT) {
		responder_info->preamble = RTT_PREAMBLE_HT;
	} else {
		responder_info->preamble = RTT_PREAMBLE_LEGACY;
	}
	err = dhd_dev_rtt_avail_channel(bcmcfg_to_prmry_ndev(cfg), &(responder_info->channel));
	if (unlikely(err)) {
		WL_ERR(("Could not get available channel:%d \n", err));
		return err;
	}
	return err;
}
static int
wl_cfgvendor_rtt_get_responder_info(struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	wifi_rtt_responder_t responder_info;

	WL_DBG(("Recv -get_avail_ch command \n"));

	bzero(&responder_info, sizeof(responder_info));
	err = get_responder_info(cfg, &responder_info);
	if (unlikely(err)) {
		WL_ERR(("Failed to get responder info:%d \n", err));
		return err;
	}
	err =  wl_cfgvendor_send_cmd_reply(wiphy, &responder_info, sizeof(responder_info));
	if (unlikely(err)) {
		WL_ERR(("Vendor cmd reply for -get_avail_ch failed ret:%d \n", err));
	}
	return err;
}

static int
wl_cfgvendor_rtt_set_responder(struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct net_device *ndev = wdev_to_wlc_ndev(wdev, cfg);
	wifi_rtt_responder_t responder_info;

	WL_DBG(("Recv rtt -enable_resp cmd.\n"));

	bzero(&responder_info, sizeof(responder_info));

	/*
	*Passing channel as NULL until implementation
	*to get chan info from upper layers is donex
	*/
	err = dhd_dev_rtt_enable_responder(ndev, NULL);
	if (unlikely(err)) {
		WL_ERR(("Could not enable responder ret:%d \n", err));
		goto done;
	}
	err = get_responder_info(cfg, &responder_info);
	if (unlikely(err)) {
		WL_ERR(("Failed to get responder info:%d \n", err));
		dhd_dev_rtt_cancel_responder(ndev);
		goto done;
	}
done:
	err =  wl_cfgvendor_send_cmd_reply(wiphy, &responder_info, sizeof(responder_info));
	if (unlikely(err)) {
		WL_ERR(("Vendor cmd reply for -enable_resp failed ret:%d \n", err));
	}
	return err;
}

static int
wl_cfgvendor_rtt_cancel_responder(struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);

	WL_DBG(("Recv rtt -cancel_resp cmd \n"));

	err = dhd_dev_rtt_cancel_responder(bcmcfg_to_prmry_ndev(cfg));
	if (unlikely(err)) {
		WL_ERR(("Vendor cmd -cancel_resp failed ret:%d \n", err));
	}
	return err;
}
#endif /* RTT_SUPPORT */

#ifdef GSCAN_SUPPORT
static int wl_cfgvendor_enable_lazy_roam(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = -EINVAL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int type;
	uint32 lazy_roam_enable_flag;

	if (!data) {
		WL_ERR(("data is not available\n"));
		return -EINVAL;
	}

	if (len <= 0) {
		WL_ERR(("invaild len %d\n", len));
		return -EINVAL;
	}

	type = nla_type(data);

	if (type == GSCAN_ATTRIBUTE_LAZY_ROAM_ENABLE) {
		lazy_roam_enable_flag = nla_get_u32(data);

		err = dhd_dev_lazy_roam_enable(bcmcfg_to_prmry_ndev(cfg),
		           lazy_roam_enable_flag);
		if (unlikely(err))
			WL_ERR(("Could not enable lazy roam:%d \n", err));
	}

	return err;
}

static int wl_cfgvendor_set_lazy_roam_cfg(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0, tmp, type;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	wlc_roam_exp_params_t roam_param;
	const struct nlattr *iter;

	bzero(&roam_param, sizeof(roam_param));

	nla_for_each_attr(iter, data, len, tmp) {
		type = nla_type(iter);
		switch (type) {
			case GSCAN_ATTRIBUTE_A_BAND_BOOST_THRESHOLD:
				roam_param.a_band_boost_threshold = nla_get_u32(iter);
				break;
			case GSCAN_ATTRIBUTE_A_BAND_PENALTY_THRESHOLD:
				roam_param.a_band_penalty_threshold = nla_get_u32(iter);
				break;
			case GSCAN_ATTRIBUTE_A_BAND_BOOST_FACTOR:
				roam_param.a_band_boost_factor = nla_get_u32(iter);
				break;
			case GSCAN_ATTRIBUTE_A_BAND_PENALTY_FACTOR:
				roam_param.a_band_penalty_factor = nla_get_u32(iter);
				break;
			case GSCAN_ATTRIBUTE_A_BAND_MAX_BOOST:
				roam_param.a_band_max_boost = nla_get_u32(iter);
				break;
			case GSCAN_ATTRIBUTE_LAZY_ROAM_HYSTERESIS:
				roam_param.cur_bssid_boost = nla_get_u32(iter);
				break;
			case GSCAN_ATTRIBUTE_ALERT_ROAM_RSSI_TRIGGER:
				roam_param.alert_roam_trigger_threshold = nla_get_u32(iter);
				break;
		}
	}

	if (dhd_dev_set_lazy_roam_cfg(bcmcfg_to_prmry_ndev(cfg), &roam_param) < 0) {
		WL_ERR(("Could not set batch cfg\n"));
		err = -EINVAL;
	}
	return err;
}

/* small helper function */
static wl_bssid_pref_cfg_t *
create_bssid_pref_cfg(struct bcm_cfg80211 *cfg, uint32 num, uint32 *buf_len)
{
	wl_bssid_pref_cfg_t *bssid_pref;

	*buf_len = sizeof(wl_bssid_pref_cfg_t);
	if (num) {
		*buf_len += (num - 1) * sizeof(wl_bssid_pref_list_t);
	}
	bssid_pref = (wl_bssid_pref_cfg_t *)MALLOC(cfg->osh, *buf_len);

	return bssid_pref;
}

static int
wl_cfgvendor_set_bssid_pref(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	wl_bssid_pref_cfg_t *bssid_pref = NULL;
	wl_bssid_pref_list_t *bssids;
	int tmp, tmp1, tmp2, type;
	const struct nlattr *outer, *inner, *iter;
	uint32 flush = 0, num = 0, buf_len = 0;
	uint8 bssid_found = 0, rssi_found = 0;

	/* Assumption: NUM attribute must come first */
	nla_for_each_attr(iter, data, len, tmp2) {
		type = nla_type(iter);
		switch (type) {
			case GSCAN_ATTRIBUTE_NUM_BSSID:
				if (num) {
					WL_ERR(("attempt overide bssid num.\n"));
					err = -EINVAL;
					goto exit;
				}
				if (nla_len(iter) != sizeof(uint32)) {
					WL_ERR(("nla_len not match\n"));
					err = -EINVAL;
					goto exit;
				}
				num = nla_get_u32(iter);
				if (num == 0 || num > MAX_BSSID_PREF_LIST_NUM) {
					WL_ERR(("wrong BSSID num:%d\n", num));
					err = -EINVAL;
					goto exit;
				}
				if ((bssid_pref = create_bssid_pref_cfg(cfg, num, &buf_len))
							== NULL) {
					WL_ERR(("Can't malloc memory\n"));
					err = -ENOMEM;
					goto exit;
				}
				break;
			case GSCAN_ATTRIBUTE_BSSID_PREF_FLUSH:
				if (nla_len(iter) != sizeof(uint32)) {
					WL_ERR(("nla_len not match\n"));
					err = -EINVAL;
					goto exit;
				}
				flush = nla_get_u32(iter);
				if (flush != 1) {
					WL_ERR(("wrong flush value\n"));
					err = -EINVAL;
					goto exit;
				}
				break;
			case GSCAN_ATTRIBUTE_BSSID_PREF_LIST:
				if (!num || !bssid_pref) {
					WL_ERR(("bssid list count not set\n"));
					err = -EINVAL;
					goto exit;
				}
				bssid_pref->count = 0;
				bssids = bssid_pref->bssids;
				nla_for_each_nested(outer, iter, tmp) {
					if (bssid_pref->count >= num) {
						WL_ERR(("too many bssid list\n"));
						err = -EINVAL;
						goto exit;
					}
					bssid_found = 0;
					rssi_found = 0;
					nla_for_each_nested(inner, outer, tmp1) {
						type = nla_type(inner);
						switch (type) {
						case GSCAN_ATTRIBUTE_BSSID_PREF:
							if (nla_len(inner) != ETHER_ADDR_LEN) {
								WL_ERR(("nla_len not match.\n"));
								err = -EINVAL;
								goto exit;
							}
							memcpy(&(bssids[bssid_pref->count].bssid),
							  nla_data(inner), ETHER_ADDR_LEN);
							/* not used for now */
							bssids[bssid_pref->count].flags = 0;
							bssid_found = 1;
							break;
						case GSCAN_ATTRIBUTE_RSSI_MODIFIER:
							if (nla_len(inner) != sizeof(uint32)) {
								WL_ERR(("nla_len not match.\n"));
								err = -EINVAL;
								goto exit;
							}
							bssids[bssid_pref->count].rssi_factor =
							       (int8) nla_get_u32(inner);
							rssi_found = 1;
							break;
						default:
							WL_ERR(("wrong type:%d\n", type));
							err = -EINVAL;
							goto exit;
						}
						if (bssid_found && rssi_found) {
							break;
						}
					}
					bssid_pref->count++;
				}
				break;
			default:
				WL_ERR(("%s: No such attribute %d\n", __FUNCTION__, type));
				break;
			}
	}

	if (!bssid_pref) {
		/* What if only flush is desired? */
		if (flush) {
			if ((bssid_pref = create_bssid_pref_cfg(cfg, 0, &buf_len)) == NULL) {
				WL_ERR(("%s: Can't malloc memory\n", __FUNCTION__));
				err = -ENOMEM;
				goto exit;
			}
			bssid_pref->count = 0;
		} else {
			err = -EINVAL;
			goto exit;
		}
	}
	err = dhd_dev_set_lazy_roam_bssid_pref(bcmcfg_to_prmry_ndev(cfg),
	          bssid_pref, flush);
exit:
	if (bssid_pref) {
		MFREE(cfg->osh, bssid_pref, buf_len);
	}
	return err;
}
#endif /* GSCAN_SUPPORT */
#if defined(GSCAN_SUPPORT) || defined(ROAMEXP_SUPPORT)
static int
wl_cfgvendor_set_bssid_blacklist(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	maclist_t *blacklist = NULL;
	int err = 0;
	int type, tmp;
	const struct nlattr *iter;
	uint32 mem_needed = 0, flush = 0, num = 0;

	/* Assumption: NUM attribute must come first */
	nla_for_each_attr(iter, data, len, tmp) {
		type = nla_type(iter);
		switch (type) {
			case GSCAN_ATTRIBUTE_NUM_BSSID:
				if (num != 0) {
					WL_ERR(("attempt to change BSSID num\n"));
					err = -EINVAL;
					goto exit;
				}
				if (nla_len(iter) != sizeof(uint32)) {
					WL_ERR(("not matching nla_len.\n"));
					err = -EINVAL;
					goto exit;
				}
				num = nla_get_u32(iter);
				if (num == 0 || num > MAX_BSSID_BLACKLIST_NUM) {
					WL_ERR(("wrong BSSID count:%d\n", num));
					err = -EINVAL;
					goto exit;
				}
				if (!blacklist) {
					mem_needed = (uint32) (OFFSETOF(maclist_t, ea) +
						sizeof(struct ether_addr) * (num));
					blacklist = (maclist_t *)
						MALLOCZ(cfg->osh, mem_needed);
					if (!blacklist) {
						WL_ERR(("MALLOCZ failed.\n"));
						err = -ENOMEM;
						goto exit;
					}
				}
				break;
			case GSCAN_ATTRIBUTE_BSSID_BLACKLIST_FLUSH:
				if (nla_len(iter) != sizeof(uint32)) {
					WL_ERR(("not matching nla_len.\n"));
					err = -EINVAL;
					goto exit;
				}
				flush = nla_get_u32(iter);
				if (flush != 1) {
					WL_ERR(("flush arg is worng:%d\n", flush));
					err = -EINVAL;
					goto exit;
				}
				break;
			case GSCAN_ATTRIBUTE_BLACKLIST_BSSID:
				if (num == 0 || !blacklist) {
					WL_ERR(("number of BSSIDs not received.\n"));
					err = -EINVAL;
					goto exit;
				}
				if (nla_len(iter) != ETHER_ADDR_LEN) {
					WL_ERR(("not matching nla_len.\n"));
					err = -EINVAL;
					goto exit;
				}
				if (blacklist->count >= num) {
					WL_ERR(("too many BSSIDs than expected:%d\n",
						blacklist->count));
					err = -EINVAL;
					goto exit;
				}
				memcpy(&(blacklist->ea[blacklist->count]), nla_data(iter),
						ETHER_ADDR_LEN);
				blacklist->count++;
				break;
		default:
			WL_ERR(("No such attribute:%d\n", type));
			break;
		}
	}

	if (blacklist && (blacklist->count != num)) {
		WL_ERR(("not matching bssid count:%d to expected:%d\n",
				blacklist->count, num));
		err = -EINVAL;
		goto exit;
	}

	err = dhd_dev_set_blacklist_bssid(bcmcfg_to_prmry_ndev(cfg),
	          blacklist, mem_needed, flush);
exit:
	MFREE(cfg->osh, blacklist, mem_needed);
	return err;
}

static int
wl_cfgvendor_set_ssid_whitelist(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	wl_ssid_whitelist_t *ssid_whitelist = NULL;
	wlc_ssid_t *ssid_elem;
	int tmp, tmp1, mem_needed = 0, type;
	const struct nlattr *iter, *iter1;
	uint32 flush = 0, num = 0;
	int ssid_found = 0;

	/* Assumption: NUM attribute must come first */
	nla_for_each_attr(iter, data, len, tmp) {
		type = nla_type(iter);
		switch (type) {
		case GSCAN_ATTRIBUTE_NUM_WL_SSID:
			if (num != 0) {
				WL_ERR(("try to change SSID num\n"));
				err = -EINVAL;
				goto exit;
			}
			if (nla_len(iter) != sizeof(uint32)) {
				WL_ERR(("not matching nla_len.\n"));
				err = -EINVAL;
				goto exit;
			}
			num = nla_get_u32(iter);
			if (num == 0 || num > MAX_SSID_WHITELIST_NUM) {
				WL_ERR(("wrong SSID count:%d\n", num));
				err = -EINVAL;
				goto exit;
			}
			mem_needed = sizeof(wl_ssid_whitelist_t) +
				sizeof(wlc_ssid_t) * num;
			ssid_whitelist = (wl_ssid_whitelist_t *)
				MALLOCZ(cfg->osh, mem_needed);
			if (ssid_whitelist == NULL) {
				WL_ERR(("failed to alloc mem\n"));
				err = -ENOMEM;
				goto exit;
			}
			break;
		case GSCAN_ATTRIBUTE_WL_SSID_FLUSH:
			if (nla_len(iter) != sizeof(uint32)) {
				WL_ERR(("not matching nla_len.\n"));
				err = -EINVAL;
				goto exit;
			}
			flush = nla_get_u32(iter);
			if (flush != 1) {
				WL_ERR(("flush arg worng:%d\n", flush));
				err = -EINVAL;
				goto exit;
			}
			break;
		case GSCAN_ATTRIBUTE_WHITELIST_SSID_ELEM:
			if (!num || !ssid_whitelist) {
				WL_ERR(("num ssid is not set!\n"));
				err = -EINVAL;
				goto exit;
			}
			if (ssid_whitelist->ssid_count >= num) {
				WL_ERR(("too many SSIDs:%d\n",
					ssid_whitelist->ssid_count));
				err = -EINVAL;
				goto exit;
			}

			ssid_elem = &ssid_whitelist->ssids[
					ssid_whitelist->ssid_count];
			ssid_found = 0;
			nla_for_each_nested(iter1, iter, tmp1) {
				type = nla_type(iter1);
				switch (type) {
				case GSCAN_ATTRIBUTE_WL_SSID_LEN:
					if (nla_len(iter1) != sizeof(uint32)) {
						WL_ERR(("not match nla_len\n"));
						err = -EINVAL;
						goto exit;
					}
					ssid_elem->SSID_len = nla_get_u32(iter1);
					if (ssid_elem->SSID_len >
							DOT11_MAX_SSID_LEN) {
						WL_ERR(("wrong SSID len:%d\n",
							ssid_elem->SSID_len));
						err = -EINVAL;
						goto exit;
					}
					break;
				case GSCAN_ATTRIBUTE_WHITELIST_SSID:
					if (ssid_elem->SSID_len == 0) {
						WL_ERR(("SSID_len not received\n"));
						err = -EINVAL;
						goto exit;
					}
					if (nla_len(iter1) != ssid_elem->SSID_len) {
						WL_ERR(("not match nla_len\n"));
						err = -EINVAL;
						goto exit;
					}
					memcpy(ssid_elem->SSID, nla_data(iter1),
							ssid_elem->SSID_len);
					ssid_found = 1;
					break;
				}
				if (ssid_found) {
					ssid_whitelist->ssid_count++;
					break;
				}
			}
			break;
		default:
			WL_ERR(("No such attribute: %d\n", type));
			break;
		}
	}

	if (ssid_whitelist && (ssid_whitelist->ssid_count != num)) {
		WL_ERR(("not matching ssid count:%d to expected:%d\n",
				ssid_whitelist->ssid_count, num));
		err = -EINVAL;
		goto exit;
	}
	err = dhd_dev_set_whitelist_ssid(bcmcfg_to_prmry_ndev(cfg),
	          ssid_whitelist, mem_needed, flush);
	if (err == BCME_UNSUPPORTED) {
		/* If firmware doesn't support feature, ignore the error
		 * Android framework doesn't populate/use whitelist ssids
		 * as of now, but invokes whitelist as part of roam config
		 * API. so this handler cannot be compiled out. but its
		 * safe to ignore.
		 */
		WL_ERR(("whilelist ssid not supported. Ignore."));
		err = BCME_OK;
	}
exit:
	MFREE(cfg->osh, ssid_whitelist, mem_needed);
	return err;
}
#endif /* GSCAN_SUPPORT || ROAMEXP_SUPPORT */

#ifdef ROAMEXP_SUPPORT
typedef enum {
	FW_ROAMING_ENABLE = 1,
	FW_ROAMING_DISABLE,
	FW_ROAMING_PAUSE,
	FW_ROAMING_RESUME
} fw_roaming_state_t;

static int
wl_cfgvendor_set_fw_roaming_state(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	fw_roaming_state_t requested_roaming_state;
	int type;
	int err = 0;

	if (!data) {
		WL_ERR(("data is not available\n"));
		return -EINVAL;
	}

	if (len <= 0) {
		WL_ERR(("invalid len %d\n", len));
		return -EINVAL;
	}

	/* Get the requested fw roaming state */
	type = nla_type(data);
	if (type != GSCAN_ATTRIBUTE_ROAM_STATE_SET) {
		WL_ERR(("%s: Invalid attribute %d\n", __FUNCTION__, type));
		return -EINVAL;
	}

	requested_roaming_state = nla_get_u32(data);
	WL_INFORM(("setting FW roaming state to %d\n", requested_roaming_state));

	if ((requested_roaming_state == FW_ROAMING_ENABLE) ||
		(requested_roaming_state == FW_ROAMING_RESUME)) {
		err = wldev_iovar_setint(wdev_to_ndev(wdev), "roam_off", FALSE);
		ROAMOFF_DBG_SAVE(wdev_to_ndev(wdev), SET_ROAM_VNDR_POLICY, FALSE);
	} else if ((requested_roaming_state == FW_ROAMING_DISABLE) ||
		(requested_roaming_state == FW_ROAMING_PAUSE)) {
		err = wldev_iovar_setint(wdev_to_ndev(wdev), "roam_off", TRUE);
		ROAMOFF_DBG_SAVE(wdev_to_ndev(wdev), SET_ROAM_VNDR_POLICY, TRUE);
	} else {
		err = -EINVAL;
	}

	return err;
}

static int
wl_cfgvendor_fw_roam_get_capability(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0;
	wifi_roaming_capabilities_t roaming_capability;

	/* Update max number of blacklist bssids supported */
	roaming_capability.max_blacklist_size = MAX_BSSID_BLACKLIST_NUM;
	roaming_capability.max_whitelist_size = MAX_SSID_WHITELIST_NUM;
	err =  wl_cfgvendor_send_cmd_reply(wiphy, &roaming_capability,
		sizeof(roaming_capability));
	if (unlikely(err)) {
		WL_ERR(("Vendor cmd reply for fw roam capability failed ret:%d \n", err));
	}

	return err;
}
#endif /* ROAMEXP_SUPPORT */

static int
wl_cfgvendor_priv_string_handler(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int ret = 0;
	int ret_len = 0, payload = 0, msglen;
	const struct bcm_nlmsg_hdr *nlioc = data;
	void *buf = NULL, *cur;
	int maxmsglen = PAGE_SIZE - 0x100;
	struct sk_buff *reply;

	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(wdev->netdev);

	/* send to dongle only if we are not waiting for reload already */
	if (dhdp && dhdp->hang_was_sent) {
		WL_INFORM(("Bus down. HANG was sent up earlier\n"));
		DHD_OS_WAKE_LOCK_CTRL_TIMEOUT_ENABLE(dhdp, DHD_EVENT_TIMEOUT_MS);
		DHD_OS_WAKE_UNLOCK(dhdp);
		return OSL_ERROR(BCME_DONGLE_DOWN);
	}

	if (!data) {
		WL_ERR(("data is not available\n"));
		return BCME_BADARG;
	}

	if (len <= sizeof(struct bcm_nlmsg_hdr)) {
		WL_ERR(("invalid len %d\n", len));
		return BCME_BADARG;
	}

	WL_DBG(("entry: cmd = %d\n", nlioc->cmd));

	if (nlioc->offset != sizeof(struct bcm_nlmsg_hdr)) {
		WL_ERR(("invalid offset %d\n", nlioc->offset));
		return BCME_BADARG;
	}
	len -= sizeof(struct bcm_nlmsg_hdr);
	ret_len = nlioc->len;
	if (ret_len > 0 || len > 0) {
		if (len >= DHD_IOCTL_MAXLEN) {
			WL_ERR(("oversize input buffer %d\n", len));
			len = DHD_IOCTL_MAXLEN - 1;
		}
		if (ret_len >= DHD_IOCTL_MAXLEN) {
			WL_ERR(("oversize return buffer %d\n", ret_len));
			ret_len = DHD_IOCTL_MAXLEN - 1;
		}

		payload = max(ret_len, len) + 1;
		buf = vzalloc(payload);
		if (!buf) {
			return -ENOMEM;
		}
		GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST();
		memcpy(buf, (void *)((char *)nlioc + nlioc->offset), len);
		GCC_DIAGNOSTIC_POP();
		*((char *)buf + len) = '\0';
	}

	ret = dhd_cfgvendor_priv_string_handler(cfg, wdev, nlioc, buf);
	if (ret) {
		WL_ERR(("dhd_cfgvendor returned error %d", ret));
		vfree(buf);
		return ret;
	}
	cur = buf;
	while (ret_len > 0) {
		msglen = ret_len > maxmsglen ? maxmsglen : ret_len;
		ret_len -= msglen;
		payload = msglen + sizeof(msglen);
		reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, payload);
		if (!reply) {
			WL_ERR(("Failed to allocate reply msg\n"));
			ret = -ENOMEM;
			break;
		}

		if (nla_put(reply, BCM_NLATTR_DATA, msglen, cur) ||
			nla_put_u16(reply, BCM_NLATTR_LEN, msglen)) {
			kfree_skb(reply);
			ret = -ENOBUFS;
			break;
		}

		ret = cfg80211_vendor_cmd_reply(reply);
		if (ret) {
			WL_ERR(("testmode reply failed:%d\n", ret));
			break;
		}
		cur = (void *)((char *)cur + msglen);
	}

	return ret;
}

struct net_device *
wl_cfgvendor_get_ndev(struct bcm_cfg80211 *cfg, struct wireless_dev *wdev,
	const char *data, unsigned long int *out_addr)
{
	char *pos, *pos1;
	char ifname[IFNAMSIZ + 1] = {0};
	struct net_info *iter, *next;
	struct net_device *ndev = NULL;
	ulong ifname_len;
	*out_addr = (unsigned long int) data; /* point to command str by default */

	/* check whether ifname=<ifname> is provided in the command */
	pos = strstr(data, "ifname=");
	if (pos) {
		pos += strlen("ifname=");
		pos1 = strstr(pos, " ");
		if (!pos1) {
			WL_ERR(("command format error \n"));
			return NULL;
		}

		ifname_len = pos1 - pos;
		if (memcpy_s(ifname, (sizeof(ifname) - 1), pos, ifname_len) != BCME_OK) {
			WL_ERR(("Failed to copy data. len: %ld\n", ifname_len));
			return NULL;
		}
		GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST();
		for_each_ndev(cfg, iter, next) {
			if (iter->ndev) {
				if (strncmp(iter->ndev->name, ifname,
					strlen(iter->ndev->name)) == 0) {
					/* matching ifname found */
					WL_DBG(("matching interface (%s) found ndev:%p \n",
						iter->ndev->name, iter->ndev));
					*out_addr = (unsigned long int)(pos1 + 1);
					/* Returns the command portion after ifname=<name> */
					return iter->ndev;
				}
			}
		}
		GCC_DIAGNOSTIC_POP();
		WL_ERR(("Couldn't find ifname:%s in the netinfo list \n",
			ifname));
		return NULL;
	}

	/* If ifname=<name> arg is not provided, use default ndev */
	ndev = wdev->netdev ? wdev->netdev : bcmcfg_to_prmry_ndev(cfg);
	WL_DBG(("Using default ndev (%s) \n", ndev->name));
	return ndev;
}

#ifdef WL_SAE
static int wl_cfgvendor_map_supp_sae_pwe_to_fw(u32 sup_value, u32 *sae_pwe)
{
	s32 ret = BCME_OK;
	switch (sup_value) {
		case SUPP_SAE_PWE_LOOP:
			*sae_pwe = SAE_PWE_LOOP;
			break;
		case SUPP_SAE_PWE_H2E:
			*sae_pwe = SAE_PWE_H2E;
			break;
		case SUPP_SAE_PWE_TRANS:
			*sae_pwe = SAE_PWE_LOOP | SAE_PWE_H2E;
			break;
		default:
			ret = BCME_BADARG;
	}
	return ret;
}
#endif /* WL_SAE */

int
wl_cfgvendor_connect_params_handler(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	struct net_device *net = wdev->netdev;
	int ret = BCME_OK;
	int attr_type;
	int rem = len;
	const struct nlattr *iter;

	BCM_REFERENCE(net);

	nla_for_each_attr(iter, data, len, rem) {
		attr_type = nla_type(iter);
		WL_DBG(("attr type: (%u)\n", attr_type));

		switch (attr_type) {
#ifdef WL_SAE
		case BRCM_ATTR_SAE_PWE: {
			u32 sae_pwe = 0;
			if (nla_len(iter) != sizeof(uint32)) {
				WL_ERR(("Invalid value of sae_pwe\n"));
				ret = -EINVAL;
				break;
			}
			ret = wl_cfgvendor_map_supp_sae_pwe_to_fw(nla_get_u32(iter), &sae_pwe);
			if (unlikely(ret)) {
				WL_ERR(("Invalid sae_pwe\n"));
				break;
			}
			ret = wl_cfg80211_set_wsec_info(net, &sae_pwe,
				sizeof(sae_pwe), WL_WSEC_INFO_BSS_SAE_PWE);
			if (unlikely(ret)) {
				WL_ERR(("set wsec_info_sae_pwe failed \n"));
			}
			break;
		}
#endif /* WL_SAE */
		/* Add new attributes here */
		default:
			WL_DBG(("%s: Unknown type, %d\n", __FUNCTION__, attr_type));
		}
	}

	return ret;
}

int
wl_cfgvendor_start_ap_params_handler(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	struct net_device *net = wdev->netdev;
	int ret = BCME_OK;
	int attr_type;
	int rem = len;
	const struct nlattr *iter;

	BCM_REFERENCE(net);

	nla_for_each_attr(iter, data, len, rem) {
		attr_type = nla_type(iter);
		WL_DBG(("attr type: (%u)\n", attr_type));

		switch (attr_type) {
#ifdef WL_SAE
		case BRCM_ATTR_SAE_PWE: {
			u32 sae_pwe = 0;
			if (nla_len(iter) != sizeof(uint32)) {
				WL_ERR(("Invalid value of sae_pwe\n"));
				ret = -EINVAL;
				break;
			}
			ret = wl_cfgvendor_map_supp_sae_pwe_to_fw(nla_get_u32(iter), &sae_pwe);
			if (unlikely(ret)) {
				WL_ERR(("Invalid sae_pwe\n"));
				break;
			}
			ret = wl_cfg80211_set_wsec_info(net, &sae_pwe,
				sizeof(sae_pwe), WL_WSEC_INFO_BSS_SAE_PWE);
			if (unlikely(ret)) {
				WL_ERR(("set wsec_info_sae_pwe failed \n"));
			}
			break;
		}
#endif /* WL_SAE */
		/* Add new attributes here */
		default:
			WL_DBG(("%s: Unknown type, %d\n", __FUNCTION__, attr_type));
		}
	}

	return ret;
}

#if defined(WL_SAE) || defined(WL_CLIENT_SAE)
static int
wl_cfgvendor_set_sae_password(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = BCME_OK;
	struct net_device *net = wdev->netdev;
	struct bcm_cfg80211 *cfg = wl_get_cfg(net);
	wsec_pmk_t pmk;
	s32 bssidx;

/* This api not needed for wpa_supplicant based sae authentication */
#ifdef WL_CLIENT_SAE
	WL_INFORM_MEM(("Ignore for external sae auth\n"));
	return BCME_OK;
#endif /* WL_CLIENT_SAE */

	/* clear the content of pmk structure before usage */
	(void)memset_s(&pmk, sizeof(wsec_pmk_t), 0x0, sizeof(wsec_pmk_t));

	if ((bssidx = wl_get_bssidx_by_wdev(cfg, net->ieee80211_ptr)) < 0) {
		WL_ERR(("Find p2p index from wdev(%p) failed\n", net->ieee80211_ptr));
		return BCME_ERROR;
	}

	if ((len < 1) || (len > WSEC_MAX_PASSPHRASE_LEN)) {
		WL_ERR(("Invalid passphrase length %d..should be >= 1 and <= 256\n",
			len));
		err = BCME_BADLEN;
		goto done;
	}
	/* Set AUTH to SAE */
	err = wldev_iovar_setint_bsscfg(net, "wpa_auth", WPA3_AUTH_SAE_PSK, bssidx);
	if (unlikely(err)) {
		WL_ERR(("could not set wpa_auth (0x%x)\n", err));
		goto done;
	}
	pmk.key_len = htod16(len);
	bcopy((const u8*)data, pmk.key, len);
	pmk.flags = htod16(WSEC_PASSPHRASE);

	err = wldev_ioctl_set(net, WLC_SET_WSEC_PMK, &pmk, sizeof(pmk));
	if (err) {
		WL_ERR(("\n failed to set pmk %d\n", err));
		goto done;
	} else {
		WL_INFORM_MEM(("sae passphrase set successfully\n"));
	}
done:
	return err;
}
#endif /* WL_SAE || WL_CLIENT_SAE */

#ifdef BCM_PRIV_CMD_SUPPORT
/* strlen("ifname=") + IFNAMESIZE + strlen(" ") + '\0' */
#define ANDROID_PRIV_CMD_IF_PREFIX_LEN	(7 + IFNAMSIZ + 2)
/* Max length for the reply buffer. For BRCM_ATTR_DRIVER_CMD, the reply
 * would be a formatted string and reply buf would be the size of the
 * string.
 */
#define WL_DRIVER_PRIV_CMD_LEN 512
static int
wl_cfgvendor_priv_bcm_handler(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	const struct nlattr *iter;
	int err = 0;
	int data_len = 0, cmd_len = 0, tmp = 0, type = 0;
	struct net_device *ndev = wdev->netdev;
	char *cmd = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int bytes_written;
	struct net_device *net = NULL;
	unsigned long int cmd_out = 0;

#if defined(WL_ANDROID_PRIV_CMD_OVER_NL80211)
	u32 cmd_buf_len = WL_DRIVER_PRIV_CMD_LEN;
	char cmd_prefix[ANDROID_PRIV_CMD_IF_PREFIX_LEN + 1] = {0};
	char *cmd_buf = NULL;
	char *current_pos;
	u32 cmd_offset;
#endif /* WL_ANDROID_PRIV_CMD_OVER_NL80211 && OEM_ANDROID */

	WL_DBG(("%s: Enter \n", __func__));

	/* hold wake lock */
	net_os_wake_lock(ndev);

	nla_for_each_attr(iter, data, len, tmp) {
		type = nla_type(iter);
		cmd = nla_data(iter);
		cmd_len = nla_len(iter);

		WL_DBG(("%s: type: %d cmd_len:%d cmd_ptr:%p \n", __func__, type, cmd_len, cmd));
		if (!cmd || !cmd_len) {
			WL_ERR(("Invalid cmd data \n"));
			err = -EINVAL;
			goto exit;
		}

#if defined(WL_ANDROID_PRIV_CMD_OVER_NL80211)
		if (type == BRCM_ATTR_DRIVER_CMD) {
			if ((cmd_len >= WL_DRIVER_PRIV_CMD_LEN) ||
				(cmd_len < ANDROID_PRIV_CMD_IF_PREFIX_LEN)) {
				WL_ERR(("Unexpected command length (%u)."
					"Ignore the command\n", cmd_len));
				err = -EINVAL;
				goto exit;
			}

			/* check whether there is any ifname prefix provided */
			if (memcpy_s(cmd_prefix, (sizeof(cmd_prefix) - 1),
					cmd, ANDROID_PRIV_CMD_IF_PREFIX_LEN) != BCME_OK) {
				WL_ERR(("memcpy failed for cmd buffer. len:%d\n", cmd_len));
				err = -ENOMEM;
				goto exit;
			}

			net = wl_cfgvendor_get_ndev(cfg, wdev, cmd_prefix, &cmd_out);
			if (!cmd_out || !net) {
				WL_ERR(("ndev not found\n"));
				err = -ENODEV;
				goto exit;
			}

			/* find offset of the command */
			current_pos = (char *)cmd_out;
			cmd_offset = current_pos - cmd_prefix;

			if (!current_pos || (cmd_offset) > ANDROID_PRIV_CMD_IF_PREFIX_LEN) {
				WL_ERR(("Invalid len cmd_offset: %u \n", cmd_offset));
				err = -EINVAL;
				goto exit;
			}

			/* Private command data in expected to be in str format. To ensure that
			 * the data is null terminated, copy to a local buffer before use
			 */
			cmd_buf = (char *)MALLOCZ(cfg->osh, cmd_buf_len);
			if (!cmd_buf) {
				WL_ERR(("memory alloc failed for %u \n", cmd_buf_len));
				err = -ENOMEM;
				goto exit;
			}

			/* Point to the start of command */
			if (memcpy_s(cmd_buf, (WL_DRIVER_PRIV_CMD_LEN - 1),
				(const void *)(cmd + cmd_offset),
				(cmd_len - cmd_offset - 1)) != BCME_OK) {
				WL_ERR(("memcpy failed for cmd buffer. len:%d\n", cmd_len));
				err = -ENOMEM;
				goto exit;
			}
			cmd_buf[WL_DRIVER_PRIV_CMD_LEN - 1] = '\0';

			WL_DBG(("vendor_command: %s len: %u \n", cmd_buf, cmd_buf_len));
			bytes_written = wl_handle_private_cmd(net, cmd_buf, cmd_buf_len);
			WL_DBG(("bytes_written: %d \n", bytes_written));
			if (bytes_written == 0) {
				snprintf(cmd_buf, cmd_buf_len, "%s", "OK");
				data_len = sizeof("OK");
			} else if (bytes_written > 0) {
				if (bytes_written >= (cmd_buf_len - 1)) {
					/* Not expected */
					ASSERT(0);
					err = -EINVAL;
					goto exit;
				}
				data_len = bytes_written;
			} else {
				/* -ve return value. Propagate the error back */
				err = bytes_written;
				goto exit;
			}
			if ((data_len > 0) && (data_len < (cmd_buf_len - 1)) && cmd_buf) {
				err =  wl_cfgvendor_send_cmd_reply(wiphy, cmd_buf, data_len);
				if (unlikely(err)) {
					WL_ERR(("Vendor Command reply failed ret:%d \n", err));
				} else {
					WL_DBG(("Vendor Command reply sent successfully!\n"));
				}
			} else {
				/* No data to be sent back as reply */
				WL_ERR(("Vendor_cmd: No reply expected. data_len:%u cmd_buf %p \n",
					data_len, cmd_buf));
			}
			break;
		}
#endif /* WL_ANDROID_PRIV_CMD_OVER_NL80211 && OEM_ANDROID */

	}

exit:

#if defined(WL_ANDROID_PRIV_CMD_OVER_NL80211)
	if (cmd_buf) {
		MFREE(cfg->osh, cmd_buf, cmd_buf_len);
	}
#endif /* WL_ANDROID_PRIV_CMD_OVER_NL80211 && OEM_ANDROID */

	net_os_wake_unlock(ndev);
	return err;
}
#endif /* BCM_PRIV_CMD_SUPPORT */

#ifdef WL_NAN
static const char *
nan_attr_to_str(u16 cmd)
{
	const char *id2str;

	switch (cmd) {
	C2S(NAN_ATTRIBUTE_HEADER);
		break;
	C2S(NAN_ATTRIBUTE_HANDLE);
		break;
	C2S(NAN_ATTRIBUTE_TRANSAC_ID);
		break;
	C2S(NAN_ATTRIBUTE_2G_SUPPORT);
		break;
	C2S(NAN_ATTRIBUTE_SDF_2G_SUPPORT);
		break;
	C2S(NAN_ATTRIBUTE_SDF_5G_SUPPORT);
		break;
	C2S(NAN_ATTRIBUTE_5G_SUPPORT);
		break;
	C2S(NAN_ATTRIBUTE_SYNC_DISC_2G_BEACON);
		break;
	C2S(NAN_ATTRIBUTE_SYNC_DISC_5G_BEACON);
		break;
	C2S(NAN_ATTRIBUTE_CLUSTER_LOW);
		break;
	C2S(NAN_ATTRIBUTE_CLUSTER_HIGH);
		break;
	C2S(NAN_ATTRIBUTE_SID_BEACON);
		break;
	C2S(NAN_ATTRIBUTE_RSSI_CLOSE);
		break;
	C2S(NAN_ATTRIBUTE_RSSI_MIDDLE);
		break;
	C2S(NAN_ATTRIBUTE_RSSI_PROXIMITY);
		break;
	C2S(NAN_ATTRIBUTE_RSSI_CLOSE_5G);
		break;
	C2S(NAN_ATTRIBUTE_RSSI_MIDDLE_5G);
		break;
	C2S(NAN_ATTRIBUTE_RSSI_PROXIMITY_5G);
		break;
	C2S(NAN_ATTRIBUTE_HOP_COUNT_LIMIT);
		break;
	C2S(NAN_ATTRIBUTE_RANDOM_TIME);
		break;
	C2S(NAN_ATTRIBUTE_MASTER_PREF);
		break;
	C2S(NAN_ATTRIBUTE_PERIODIC_SCAN_INTERVAL);
		break;
	C2S(NAN_ATTRIBUTE_PUBLISH_ID);
		break;
	C2S(NAN_ATTRIBUTE_TTL);
		break;
	C2S(NAN_ATTRIBUTE_PERIOD);
		break;
	C2S(NAN_ATTRIBUTE_REPLIED_EVENT_FLAG);
		break;
	C2S(NAN_ATTRIBUTE_PUBLISH_TYPE);
		break;
	C2S(NAN_ATTRIBUTE_TX_TYPE);
		break;
	C2S(NAN_ATTRIBUTE_PUBLISH_COUNT);
		break;
	C2S(NAN_ATTRIBUTE_SERVICE_NAME_LEN);
		break;
	C2S(NAN_ATTRIBUTE_SERVICE_NAME);
		break;
	C2S(NAN_ATTRIBUTE_SERVICE_SPECIFIC_INFO_LEN);
		break;
	C2S(NAN_ATTRIBUTE_SERVICE_SPECIFIC_INFO);
		break;
	C2S(NAN_ATTRIBUTE_RX_MATCH_FILTER_LEN);
		break;
	C2S(NAN_ATTRIBUTE_RX_MATCH_FILTER);
		break;
	C2S(NAN_ATTRIBUTE_TX_MATCH_FILTER_LEN);
		break;
	C2S(NAN_ATTRIBUTE_TX_MATCH_FILTER);
		break;
	C2S(NAN_ATTRIBUTE_SUBSCRIBE_ID);
		break;
	C2S(NAN_ATTRIBUTE_SUBSCRIBE_TYPE);
		break;
	C2S(NAN_ATTRIBUTE_SERVICERESPONSEFILTER);
		break;
	C2S(NAN_ATTRIBUTE_SERVICERESPONSEINCLUDE);
		break;
	C2S(NAN_ATTRIBUTE_USESERVICERESPONSEFILTER);
		break;
	C2S(NAN_ATTRIBUTE_SSIREQUIREDFORMATCHINDICATION);
		break;
	C2S(NAN_ATTRIBUTE_SUBSCRIBE_MATCH);
		break;
	C2S(NAN_ATTRIBUTE_SUBSCRIBE_COUNT);
		break;
	C2S(NAN_ATTRIBUTE_MAC_ADDR);
		break;
	C2S(NAN_ATTRIBUTE_MAC_ADDR_LIST);
		break;
	C2S(NAN_ATTRIBUTE_MAC_ADDR_LIST_NUM_ENTRIES);
		break;
	C2S(NAN_ATTRIBUTE_PUBLISH_MATCH);
		break;
	C2S(NAN_ATTRIBUTE_ENABLE_STATUS);
		break;
	C2S(NAN_ATTRIBUTE_JOIN_STATUS);
		break;
	C2S(NAN_ATTRIBUTE_ROLE);
		break;
	C2S(NAN_ATTRIBUTE_MASTER_RANK);
		break;
	C2S(NAN_ATTRIBUTE_ANCHOR_MASTER_RANK);
		break;
	C2S(NAN_ATTRIBUTE_CNT_PEND_TXFRM);
		break;
	C2S(NAN_ATTRIBUTE_CNT_BCN_TX);
		break;
	C2S(NAN_ATTRIBUTE_CNT_BCN_RX);
		break;
	C2S(NAN_ATTRIBUTE_CNT_SVC_DISC_TX);
		break;
	C2S(NAN_ATTRIBUTE_CNT_SVC_DISC_RX);
		break;
	C2S(NAN_ATTRIBUTE_AMBTT);
		break;
	C2S(NAN_ATTRIBUTE_CLUSTER_ID);
		break;
	C2S(NAN_ATTRIBUTE_INST_ID);
		break;
	C2S(NAN_ATTRIBUTE_OUI);
		break;
	C2S(NAN_ATTRIBUTE_STATUS);
		break;
	C2S(NAN_ATTRIBUTE_DE_EVENT_TYPE);
		break;
	C2S(NAN_ATTRIBUTE_MERGE);
		break;
	C2S(NAN_ATTRIBUTE_IFACE);
		break;
	C2S(NAN_ATTRIBUTE_CHANNEL);
		break;
	C2S(NAN_ATTRIBUTE_24G_CHANNEL);
		break;
	C2S(NAN_ATTRIBUTE_5G_CHANNEL);
		break;
	C2S(NAN_ATTRIBUTE_PEER_ID);
		break;
	C2S(NAN_ATTRIBUTE_NDP_ID);
		break;
	C2S(NAN_ATTRIBUTE_SECURITY);
		break;
	C2S(NAN_ATTRIBUTE_QOS);
		break;
	C2S(NAN_ATTRIBUTE_RSP_CODE);
		break;
	C2S(NAN_ATTRIBUTE_INST_COUNT);
		break;
	C2S(NAN_ATTRIBUTE_PEER_DISC_MAC_ADDR);
		break;
	C2S(NAN_ATTRIBUTE_PEER_NDI_MAC_ADDR);
		break;
	C2S(NAN_ATTRIBUTE_IF_ADDR);
		break;
	C2S(NAN_ATTRIBUTE_WARMUP_TIME);
		break;
	C2S(NAN_ATTRIBUTE_RECV_IND_CFG);
		break;
	C2S(NAN_ATTRIBUTE_CONNMAP);
		break;
	C2S(NAN_ATTRIBUTE_DWELL_TIME);
		break;
	C2S(NAN_ATTRIBUTE_SCAN_PERIOD);
		break;
	C2S(NAN_ATTRIBUTE_RSSI_WINDOW_SIZE);
		break;
	C2S(NAN_ATTRIBUTE_CONF_CLUSTER_VAL);
		break;
	C2S(NAN_ATTRIBUTE_CIPHER_SUITE_TYPE);
		break;
	C2S(NAN_ATTRIBUTE_KEY_TYPE);
		break;
	C2S(NAN_ATTRIBUTE_KEY_LEN);
		break;
	C2S(NAN_ATTRIBUTE_SCID);
		break;
	C2S(NAN_ATTRIBUTE_SCID_LEN);
		break;
	C2S(NAN_ATTRIBUTE_SDE_CONTROL_CONFIG_DP);
		break;
	C2S(NAN_ATTRIBUTE_SDE_CONTROL_SECURITY);
		break;
	C2S(NAN_ATTRIBUTE_SDE_CONTROL_DP_TYPE);
		break;
	C2S(NAN_ATTRIBUTE_SDE_CONTROL_RANGE_SUPPORT);
		break;
	C2S(NAN_ATTRIBUTE_NO_CONFIG_AVAIL);
		break;
	C2S(NAN_ATTRIBUTE_2G_AWAKE_DW);
		break;
	C2S(NAN_ATTRIBUTE_5G_AWAKE_DW);
		break;
	C2S(NAN_ATTRIBUTE_RSSI_THRESHOLD_FLAG);
		break;
	C2S(NAN_ATTRIBUTE_KEY_DATA);
		break;
	C2S(NAN_ATTRIBUTE_SDEA_SERVICE_SPECIFIC_INFO_LEN);
		break;
	C2S(NAN_ATTRIBUTE_SDEA_SERVICE_SPECIFIC_INFO);
		break;
	C2S(NAN_ATTRIBUTE_REASON);
		break;
	C2S(NAN_ATTRIBUTE_DISC_IND_CFG);
		break;
	C2S(NAN_ATTRIBUTE_DWELL_TIME_5G);
		break;
	C2S(NAN_ATTRIBUTE_SCAN_PERIOD_5G);
		break;
	C2S(NAN_ATTRIBUTE_SVC_RESPONDER_POLICY);
		break;
	C2S(NAN_ATTRIBUTE_EVENT_MASK);
		break;
	C2S(NAN_ATTRIBUTE_SUB_SID_BEACON);
		break;
	C2S(NAN_ATTRIBUTE_RANDOMIZATION_INTERVAL);
		break;
	C2S(NAN_ATTRIBUTE_CMD_RESP_DATA);
		break;
	C2S(NAN_ATTRIBUTE_CMD_USE_NDPE);
		break;
	C2S(NAN_ATTRIBUTE_ENABLE_MERGE);
		break;
	C2S(NAN_ATTRIBUTE_DISCOVERY_BEACON_INTERVAL);
		break;
	C2S(NAN_ATTRIBUTE_NSS);
		break;
	C2S(NAN_ATTRIBUTE_ENABLE_RANGING);
		break;
	C2S(NAN_ATTRIBUTE_DW_EARLY_TERM);
		break;
	default:
		id2str = "NAN_ATTRIBUTE_UNKNOWN";
	}

	return id2str;
}

nan_hal_status_t nan_status_reasonstr_map[] = {
	{NAN_STATUS_SUCCESS, "NAN status success"},
	{NAN_STATUS_INTERNAL_FAILURE, "NAN Discovery engine failure"},
	{NAN_STATUS_PROTOCOL_FAILURE, "protocol failure"},
	{NAN_STATUS_INVALID_PUBLISH_SUBSCRIBE_ID, "invalid pub_sub ID"},
	{NAN_STATUS_NO_RESOURCE_AVAILABLE, "No space available"},
	{NAN_STATUS_INVALID_PARAM, "invalid param"},
	{NAN_STATUS_INVALID_REQUESTOR_INSTANCE_ID, "invalid req inst id"},
	{NAN_STATUS_INVALID_NDP_ID, "invalid ndp id"},
	{NAN_STATUS_NAN_NOT_ALLOWED, "Nan not allowed"},
	{NAN_STATUS_NO_OTA_ACK, "No OTA ack"},
	{NAN_STATUS_ALREADY_ENABLED, "NAN is Already enabled"},
	{NAN_STATUS_FOLLOWUP_QUEUE_FULL, "Follow-up queue full"},
	{NAN_STATUS_UNSUPPORTED_CONCURRENCY_NAN_DISABLED, "unsupported concurrency"},
};

void
wl_cfgvendor_add_nan_reason_str(nan_status_type_t status, nan_hal_resp_t *nan_req_resp)
{
	int i = 0;
	int num = (int)(sizeof(nan_status_reasonstr_map)/sizeof(nan_status_reasonstr_map[0]));
	for (i = 0; i < num; i++) {
		if (nan_status_reasonstr_map[i].status == status) {
			strlcpy(nan_req_resp->nan_reason, nan_status_reasonstr_map[i].nan_reason,
				sizeof(nan_status_reasonstr_map[i].nan_reason));
			break;
		}
	}
}

nan_status_type_t
wl_cfgvendor_brcm_to_nanhal_status(int32 vendor_status)
{
	nan_status_type_t hal_status;
	switch (vendor_status) {
		case BCME_OK:
			hal_status = NAN_STATUS_SUCCESS;
			break;
		case BCME_BUSY:
		case BCME_NOTREADY:
			hal_status = NAN_STATUS_NAN_NOT_ALLOWED;
			break;
		case BCME_BADLEN:
		case BCME_BADBAND:
		case BCME_UNSUPPORTED:
		case BCME_USAGE_ERROR:
		case BCME_BADARG:
		case BCME_NOTENABLED:
			hal_status = NAN_STATUS_INVALID_PARAM;
			break;
		case BCME_NOMEM:
		case BCME_NORESOURCE:
		case WL_NAN_E_SVC_SUB_LIST_FULL:
			hal_status = NAN_STATUS_NO_RESOURCE_AVAILABLE;
			break;
		case WL_NAN_E_SD_TX_LIST_FULL:
			hal_status = NAN_STATUS_FOLLOWUP_QUEUE_FULL;
			break;
		case WL_NAN_E_BAD_INSTANCE:
			hal_status = NAN_STATUS_INVALID_PUBLISH_SUBSCRIBE_ID;
			break;
		default:
			WL_ERR(("%s Unknown vendor status, status = %d\n",
					__func__, vendor_status));
			/* Generic error */
			hal_status = NAN_STATUS_INTERNAL_FAILURE;
	}
	return hal_status;
}

static int
wl_cfgvendor_nan_cmd_reply(struct wiphy *wiphy, int nan_cmd,
	nan_hal_resp_t *nan_req_resp, int ret, int nan_cmd_status)
{
	int err;
	int nan_reply;
	nan_req_resp->subcmd = nan_cmd;
	if (ret == BCME_OK) {
		nan_reply = nan_cmd_status;
	} else {
		nan_reply = ret;
	}
	nan_req_resp->status = wl_cfgvendor_brcm_to_nanhal_status(nan_reply);
	nan_req_resp->value = ret;
	err = wl_cfgvendor_send_cmd_reply(wiphy, nan_req_resp,
		sizeof(*nan_req_resp));
	/* giving more prio to ret than err */
	return (ret == 0) ? err : ret;
}

static void
wl_cfgvendor_free_disc_cmd_data(struct bcm_cfg80211 *cfg,
	nan_discover_cmd_data_t *cmd_data)
{
	if (!cmd_data) {
		WL_ERR(("Cmd_data is null\n"));
		return;
	}
	if (cmd_data->svc_info.data) {
		MFREE(cfg->osh, cmd_data->svc_info.data, cmd_data->svc_info.dlen);
	}
	if (cmd_data->svc_hash.data) {
		MFREE(cfg->osh, cmd_data->svc_hash.data, cmd_data->svc_hash.dlen);
	}
	if (cmd_data->rx_match.data) {
		MFREE(cfg->osh, cmd_data->rx_match.data, cmd_data->rx_match.dlen);
	}
	if (cmd_data->tx_match.data) {
		MFREE(cfg->osh, cmd_data->tx_match.data, cmd_data->tx_match.dlen);
	}
	if (cmd_data->mac_list.list) {
		MFREE(cfg->osh, cmd_data->mac_list.list,
			cmd_data->mac_list.num_mac_addr * ETHER_ADDR_LEN);
	}
	if (cmd_data->key.data) {
		MFREE(cfg->osh, cmd_data->key.data, NAN_MAX_PMK_LEN);
	}
	if (cmd_data->sde_svc_info.data) {
		MFREE(cfg->osh, cmd_data->sde_svc_info.data, cmd_data->sde_svc_info.dlen);
	}
	MFREE(cfg->osh, cmd_data, sizeof(*cmd_data));
}

static void
wl_cfgvendor_free_dp_cmd_data(struct bcm_cfg80211 *cfg,
	nan_datapath_cmd_data_t *cmd_data)
{
	if (!cmd_data) {
		WL_ERR(("Cmd_data is null\n"));
		return;
	}
	if (cmd_data->svc_hash.data) {
		MFREE(cfg->osh, cmd_data->svc_hash.data, cmd_data->svc_hash.dlen);
	}
	if (cmd_data->svc_info.data) {
		MFREE(cfg->osh, cmd_data->svc_info.data, cmd_data->svc_info.dlen);
	}
	if (cmd_data->key.data) {
		MFREE(cfg->osh, cmd_data->key.data, NAN_MAX_PMK_LEN);
	}
	MFREE(cfg->osh, cmd_data, sizeof(*cmd_data));
}

#define WL_NAN_EVENT_MAX_BUF 256
#ifdef WL_NAN_DISC_CACHE
static int
wl_cfgvendor_nan_parse_dp_sec_info_args(struct wiphy *wiphy,
	const void *buf, int len, nan_datapath_sec_info_cmd_data_t *cmd_data)
{
	int ret = BCME_OK;
	int attr_type;
	int rem = len;
	const struct nlattr *iter;

	NAN_DBG_ENTER();

	nla_for_each_attr(iter, buf, len, rem) {
		attr_type = nla_type(iter);
		WL_TRACE(("attr: %s (%u)\n", nan_attr_to_str(attr_type), attr_type));

		switch (attr_type) {
		case NAN_ATTRIBUTE_MAC_ADDR:
			ret = memcpy_s((char*)&cmd_data->mac_addr, ETHER_ADDR_LEN,
				(char*)nla_data(iter), nla_len(iter));
			if (ret != BCME_OK) {
				WL_ERR(("Failed to copy mac addr\n"));
				return ret;
			}
			break;
		case NAN_ATTRIBUTE_PUBLISH_ID:
			cmd_data->pub_id = nla_get_u32(iter);
			break;
		case NAN_ATTRIBUTE_NDP_ID:
			cmd_data->ndp_instance_id = nla_get_u32(iter);
			break;
		default:
			WL_ERR(("%s: Unknown type, %d\n", __FUNCTION__, attr_type));
			ret = BCME_BADARG;
			break;
		}
	}
	/* We need to call set_config_handler b/f calling start enable TBD */
	NAN_DBG_EXIT();
	return ret;
}
#endif /* WL_NAN_DISC_CACHE */

int8 chanbuf[CHANSPEC_STR_LEN];
static int
wl_cfgvendor_nan_parse_datapath_args(struct wiphy *wiphy,
	const void *buf, int len, nan_datapath_cmd_data_t *cmd_data)
{
	int ret = BCME_OK;
	int attr_type;
	int rem = len;
	const struct nlattr *iter;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int chan;

	NAN_DBG_ENTER();

	nla_for_each_attr(iter, buf, len, rem) {
		attr_type = nla_type(iter);
		WL_TRACE(("attr: %s (%u)\n", nan_attr_to_str(attr_type), attr_type));

		switch (attr_type) {
		case NAN_ATTRIBUTE_NDP_ID:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->ndp_instance_id = nla_get_u32(iter);
			break;
		case NAN_ATTRIBUTE_IFACE:
			if (nla_len(iter) >= sizeof(cmd_data->ndp_iface)) {
				WL_ERR(("iface_name len wrong:%d\n", nla_len(iter)));
				ret = -EINVAL;
				goto exit;
			}
			strlcpy((char *)cmd_data->ndp_iface, (char *)nla_data(iter),
				nla_len(iter));
			break;
		case NAN_ATTRIBUTE_SECURITY:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->ndp_cfg.security_cfg = nla_get_u8(iter);
			break;
		case NAN_ATTRIBUTE_QOS:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->ndp_cfg.qos_cfg = nla_get_u8(iter);
			break;
		case NAN_ATTRIBUTE_RSP_CODE:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->rsp_code = nla_get_u8(iter);
			break;
		case NAN_ATTRIBUTE_INST_COUNT:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->num_ndp_instances = nla_get_u8(iter);
			break;
		case NAN_ATTRIBUTE_PEER_DISC_MAC_ADDR:
			if (nla_len(iter) != ETHER_ADDR_LEN) {
				ret = -EINVAL;
				goto exit;
			}
			ret = memcpy_s((char*)&cmd_data->peer_disc_mac_addr,
				ETHER_ADDR_LEN,	(char*)nla_data(iter), nla_len(iter));
			if (ret != BCME_OK) {
				WL_ERR(("Failed to copy peer_disc_mac_addr\n"));
				goto exit;
			}
			break;
		case NAN_ATTRIBUTE_PEER_NDI_MAC_ADDR:
			if (nla_len(iter) != ETHER_ADDR_LEN) {
				ret = -EINVAL;
				goto exit;
			}
			ret = memcpy_s((char*)&cmd_data->peer_ndi_mac_addr,
				ETHER_ADDR_LEN,	(char*)nla_data(iter), nla_len(iter));
			if (ret != BCME_OK) {
				WL_ERR(("Failed to copy peer_ndi_mac_addr\n"));
				goto exit;
			}
			break;
		case NAN_ATTRIBUTE_MAC_ADDR:
			if (nla_len(iter) != ETHER_ADDR_LEN) {
				ret = -EINVAL;
				goto exit;
			}
			ret = memcpy_s((char*)&cmd_data->mac_addr, ETHER_ADDR_LEN,
					(char*)nla_data(iter), nla_len(iter));
			if (ret != BCME_OK) {
				WL_ERR(("Failed to copy mac_addr\n"));
				goto exit;
			}
			break;
		case NAN_ATTRIBUTE_IF_ADDR:
			if (nla_len(iter) != ETHER_ADDR_LEN) {
				ret = -EINVAL;
				goto exit;
			}
			ret = memcpy_s((char*)&cmd_data->if_addr, ETHER_ADDR_LEN,
					(char*)nla_data(iter), nla_len(iter));
			if (ret != BCME_OK) {
				WL_ERR(("Failed to copy if_addr\n"));
				goto exit;
			}
			break;
		case NAN_ATTRIBUTE_ENTRY_CONTROL:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->avail_params.duration = nla_get_u8(iter);
			break;
		case NAN_ATTRIBUTE_AVAIL_BIT_MAP:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->avail_params.bmap = nla_get_u32(iter);
			break;
		case NAN_ATTRIBUTE_CHANNEL: {
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			/* take the default channel start_factor frequency */
			chan = wf_mhz2channel((uint)nla_get_u32(iter), 0);
			if (chan <= CH_MAX_2G_CHANNEL) {
				cmd_data->avail_params.chanspec[0] =
					wf_channel2chspec(chan, WL_CHANSPEC_BW_20);
			} else {
				cmd_data->avail_params.chanspec[0] =
					wf_channel2chspec(chan, WL_CHANSPEC_BW_80);
			}
			if (cmd_data->avail_params.chanspec[0] == 0) {
				WL_ERR(("Channel is not valid \n"));
				ret = -EINVAL;
				goto exit;
			}
			WL_TRACE(("valid chanspec, chanspec = 0x%04x \n",
				cmd_data->avail_params.chanspec[0]));
			break;
		}
		case NAN_ATTRIBUTE_NO_CONFIG_AVAIL:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->avail_params.no_config_avail = (bool)nla_get_u8(iter);
			break;
		case NAN_ATTRIBUTE_SERVICE_NAME_LEN: {
			if (nla_len(iter) != sizeof(uint16)) {
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->svc_hash.dlen) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->svc_hash.dlen = nla_get_u16(iter);
			if (cmd_data->svc_hash.dlen != WL_NAN_SVC_HASH_LEN) {
				WL_ERR(("invalid svc_hash length = %u\n", cmd_data->svc_hash.dlen));
				ret = -EINVAL;
				goto exit;
			}
			break;
		}
		case NAN_ATTRIBUTE_SERVICE_NAME:
			if ((!cmd_data->svc_hash.dlen) ||
				(nla_len(iter) != cmd_data->svc_hash.dlen)) {
				WL_ERR(("invalid svc_hash length = %d,%d\n",
					cmd_data->svc_hash.dlen, nla_len(iter)));
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->svc_hash.data) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->svc_hash.data =
				MALLOCZ(cfg->osh, cmd_data->svc_hash.dlen);
			if (!cmd_data->svc_hash.data) {
				WL_ERR(("failed to allocate svc_hash data, len=%d\n",
					cmd_data->svc_hash.dlen));
				ret = -ENOMEM;
				goto exit;
			}
			ret = memcpy_s(cmd_data->svc_hash.data, cmd_data->svc_hash.dlen,
					nla_data(iter), nla_len(iter));
			if (ret != BCME_OK) {
				WL_ERR(("Failed to copy svc hash data\n"));
				goto exit;
			}
			break;
		case NAN_ATTRIBUTE_SERVICE_SPECIFIC_INFO_LEN:
			if (nla_len(iter) != sizeof(uint16)) {
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->svc_info.dlen) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->svc_info.dlen = nla_get_u16(iter);
			if (cmd_data->svc_info.dlen > MAX_APP_INFO_LEN) {
				WL_ERR_RLMT(("Not allowed beyond :%d\n", MAX_APP_INFO_LEN));
				ret = -EINVAL;
				goto exit;
			}
			break;
		case NAN_ATTRIBUTE_SERVICE_SPECIFIC_INFO:
			if ((!cmd_data->svc_info.dlen) ||
				(nla_len(iter) != cmd_data->svc_info.dlen)) {
				WL_ERR(("failed to allocate svc info by invalid len=%d,%d\n",
					cmd_data->svc_info.dlen, nla_len(iter)));
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->svc_info.data) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->svc_info.data = MALLOCZ(cfg->osh, cmd_data->svc_info.dlen);
			if (cmd_data->svc_info.data == NULL) {
				WL_ERR(("failed to allocate svc info data, len=%d\n",
					cmd_data->svc_info.dlen));
				ret = -ENOMEM;
				goto exit;
			}
			ret = memcpy_s(cmd_data->svc_info.data, cmd_data->svc_info.dlen,
					nla_data(iter), nla_len(iter));
			if (ret != BCME_OK) {
				WL_ERR(("Failed to copy svc info\n"));
				goto exit;
			}
			break;
		case NAN_ATTRIBUTE_PUBLISH_ID:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->pub_id = nla_get_u32(iter);
			break;
		case NAN_ATTRIBUTE_CIPHER_SUITE_TYPE:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->csid = nla_get_u8(iter);
			WL_TRACE(("CSID = %u\n", cmd_data->csid));
			break;
		case NAN_ATTRIBUTE_KEY_TYPE:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->key_type = nla_get_u8(iter);
			WL_TRACE(("Key Type = %u\n", cmd_data->key_type));
			break;
		case NAN_ATTRIBUTE_KEY_LEN:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->key.dlen) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->key.dlen = nla_get_u32(iter);
			if ((!cmd_data->key.dlen) || (cmd_data->key.dlen > WL_NAN_NCS_SK_PMK_LEN)) {
				WL_ERR(("invalid key length = %u\n", cmd_data->key.dlen));
				ret = -EINVAL;
				goto exit;
			}
			WL_TRACE(("valid key length = %u\n", cmd_data->key.dlen));
			break;
		case NAN_ATTRIBUTE_KEY_DATA:
			if ((!cmd_data->key.dlen) ||
				(nla_len(iter) != cmd_data->key.dlen)) {
				WL_ERR(("failed to allocate key data by invalid len=%d,%d\n",
					cmd_data->key.dlen, nla_len(iter)));
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->key.data) {
				WL_ERR(("trying to overwrite key data.\n"));
				ret = -EINVAL;
				goto exit;
			}

			cmd_data->key.data = MALLOCZ(cfg->osh, NAN_MAX_PMK_LEN);
			if (cmd_data->key.data == NULL) {
				WL_ERR(("failed to allocate key data, len=%d\n",
					cmd_data->key.dlen));
				ret = -ENOMEM;
				goto exit;
			}
			ret = memcpy_s(cmd_data->key.data, NAN_MAX_PMK_LEN,
					nla_data(iter), nla_len(iter));
			if (ret != BCME_OK) {
				WL_ERR(("Failed to key data\n"));
				goto exit;
			}
			break;

		default:
			WL_ERR(("Unknown type, %d\n", attr_type));
			ret = -EINVAL;
			goto exit;
		}
	}
exit:
	/* We need to call set_config_handler b/f calling start enable TBD */
	NAN_DBG_EXIT();
	return ret;
}

static int
wl_cfgvendor_nan_parse_discover_args(struct wiphy *wiphy,
	const void *buf, int len, nan_discover_cmd_data_t *cmd_data)
{
	int ret = BCME_OK;
	int attr_type;
	int rem = len;
	const struct nlattr *iter;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	u8 val_u8;
	u32 bit_flag;
	u8 flag_match;

	NAN_DBG_ENTER();

	nla_for_each_attr(iter, buf, len, rem) {
		attr_type = nla_type(iter);
		WL_TRACE(("attr: %s (%u)\n", nan_attr_to_str(attr_type), attr_type));

		switch (attr_type) {
		case NAN_ATTRIBUTE_TRANSAC_ID:
			if (nla_len(iter) != sizeof(uint16)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->token = nla_get_u16(iter);
			break;
		case NAN_ATTRIBUTE_PERIODIC_SCAN_INTERVAL:
			break;

		/* Nan Publish/Subscribe request Attributes */
		case NAN_ATTRIBUTE_PUBLISH_ID:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->pub_id = nla_get_u32(iter);
			cmd_data->local_id = cmd_data->pub_id;
			break;
		case NAN_ATTRIBUTE_MAC_ADDR:
			if (nla_len(iter) != ETHER_ADDR_LEN) {
				ret = -EINVAL;
				goto exit;
			}
			ret = memcpy_s((char*)&cmd_data->mac_addr, ETHER_ADDR_LEN,
					(char*)nla_data(iter), nla_len(iter));
			if (ret != BCME_OK) {
				WL_ERR(("Failed to copy mac addr\n"));
				return ret;
			}
			break;
		case NAN_ATTRIBUTE_SERVICE_SPECIFIC_INFO_LEN:
			if (nla_len(iter) != sizeof(uint16)) {
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->svc_info.dlen) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->svc_info.dlen = nla_get_u16(iter);
			if (cmd_data->svc_info.dlen > NAN_MAX_SERVICE_SPECIFIC_INFO_LEN) {
				WL_ERR_RLMT(("Not allowed beyond :%d\n",
					NAN_MAX_SERVICE_SPECIFIC_INFO_LEN));
				ret = -EINVAL;
				goto exit;
			}
			break;
		case NAN_ATTRIBUTE_SERVICE_SPECIFIC_INFO:
			if ((!cmd_data->svc_info.dlen) ||
				(nla_len(iter) != cmd_data->svc_info.dlen)) {
				WL_ERR(("failed to allocate svc info by invalid len=%d,%d\n",
					cmd_data->svc_info.dlen, nla_len(iter)));
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->svc_info.data) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}

			cmd_data->svc_info.data = MALLOCZ(cfg->osh, cmd_data->svc_info.dlen);
			if (cmd_data->svc_info.data == NULL) {
				WL_ERR(("failed to allocate svc info data, len=%d\n",
					cmd_data->svc_info.dlen));
				ret = -ENOMEM;
				goto exit;
			}
			ret = memcpy_s(cmd_data->svc_info.data, cmd_data->svc_info.dlen,
					nla_data(iter), nla_len(iter));
			if (ret != BCME_OK) {
				WL_ERR(("Failed to copy svc info\n"));
				return ret;
			}
			break;
		case NAN_ATTRIBUTE_SUBSCRIBE_ID:
			if (nla_len(iter) != sizeof(uint16)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->sub_id = nla_get_u16(iter);
			cmd_data->local_id = cmd_data->sub_id;
			break;
		case NAN_ATTRIBUTE_SUBSCRIBE_TYPE:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->flags |= nla_get_u8(iter) ? WL_NAN_SUB_ACTIVE : 0;
			break;
		case NAN_ATTRIBUTE_PUBLISH_COUNT:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->life_count = nla_get_u8(iter);
			break;
		case NAN_ATTRIBUTE_PUBLISH_TYPE: {
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			val_u8 = nla_get_u8(iter);
			if (val_u8 == 0) {
				cmd_data->flags |= WL_NAN_PUB_UNSOLICIT;
			} else if (val_u8 == 1) {
				cmd_data->flags |= WL_NAN_PUB_SOLICIT;
			} else {
				cmd_data->flags |= WL_NAN_PUB_BOTH;
			}
			break;
		}
		case NAN_ATTRIBUTE_PERIOD: {
			if (nla_len(iter) != sizeof(uint16)) {
				ret = -EINVAL;
				goto exit;
			}
			if (nla_get_u16(iter) > NAN_MAX_AWAKE_DW_INTERVAL) {
				WL_ERR(("Invalid/Out of bound value = %u\n", nla_get_u16(iter)));
				ret = BCME_BADARG;
				break;
			}
			if (nla_get_u16(iter)) {
				cmd_data->period = 1 << (nla_get_u16(iter)-1);
			}
			break;
		}
		case NAN_ATTRIBUTE_REPLIED_EVENT_FLAG:
			break;
		case NAN_ATTRIBUTE_TTL:
			if (nla_len(iter) != sizeof(uint16)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->ttl = nla_get_u16(iter);
			break;
		case NAN_ATTRIBUTE_SERVICE_NAME_LEN: {
			if (nla_len(iter) != sizeof(uint16)) {
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->svc_hash.dlen) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}

			cmd_data->svc_hash.dlen = nla_get_u16(iter);
			if (cmd_data->svc_hash.dlen != WL_NAN_SVC_HASH_LEN) {
				WL_ERR(("invalid svc_hash length = %u\n", cmd_data->svc_hash.dlen));
				ret = -EINVAL;
				goto exit;
			}
			break;
		}
		case NAN_ATTRIBUTE_SERVICE_NAME:
			if ((!cmd_data->svc_hash.dlen) ||
				(nla_len(iter) != cmd_data->svc_hash.dlen)) {
				WL_ERR(("invalid svc_hash length = %d,%d\n",
					cmd_data->svc_hash.dlen, nla_len(iter)));
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->svc_hash.data) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}

			cmd_data->svc_hash.data =
				MALLOCZ(cfg->osh, cmd_data->svc_hash.dlen);
			if (!cmd_data->svc_hash.data) {
				WL_ERR(("failed to allocate svc_hash data, len=%d\n",
					cmd_data->svc_hash.dlen));
				ret = -ENOMEM;
				goto exit;
			}
			ret = memcpy_s(cmd_data->svc_hash.data, cmd_data->svc_hash.dlen,
					nla_data(iter), nla_len(iter));
			if (ret != BCME_OK) {
				WL_ERR(("Failed to copy svc hash data\n"));
				return ret;
			}
			break;
		case NAN_ATTRIBUTE_PEER_ID:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->remote_id = nla_get_u32(iter);
			break;
		case NAN_ATTRIBUTE_INST_ID:
			if (nla_len(iter) != sizeof(uint16)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->local_id = nla_get_u16(iter);
			break;
		case NAN_ATTRIBUTE_SUBSCRIBE_COUNT:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->life_count = nla_get_u8(iter);
			break;
		case NAN_ATTRIBUTE_SSIREQUIREDFORMATCHINDICATION: {
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			bit_flag = (u32)nla_get_u8(iter);
			cmd_data->flags |=
				bit_flag ? WL_NAN_SUB_MATCH_IF_SVC_INFO : 0;
			break;
		}
		case NAN_ATTRIBUTE_SUBSCRIBE_MATCH:
		case NAN_ATTRIBUTE_PUBLISH_MATCH: {
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			flag_match = nla_get_u8(iter);

			switch (flag_match) {
			case NAN_MATCH_ALG_MATCH_CONTINUOUS:
				/* Default fw behaviour, no need to set explicitly */
				break;
			case NAN_MATCH_ALG_MATCH_ONCE:
				cmd_data->flags |= WL_NAN_MATCH_ONCE;
				break;
			case NAN_MATCH_ALG_MATCH_NEVER:
				cmd_data->flags |= WL_NAN_MATCH_NEVER;
				break;
			default:
				WL_ERR(("invalid nan match alg = %u\n", flag_match));
				ret = -EINVAL;
				goto exit;
			}
			break;
		}
		case NAN_ATTRIBUTE_SERVICERESPONSEFILTER:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->srf_type = nla_get_u8(iter);
			break;
		case NAN_ATTRIBUTE_SERVICERESPONSEINCLUDE:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->srf_include = nla_get_u8(iter);
			break;
		case NAN_ATTRIBUTE_USESERVICERESPONSEFILTER:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->use_srf = nla_get_u8(iter);
			break;
		case NAN_ATTRIBUTE_RX_MATCH_FILTER_LEN:
			if (nla_len(iter) != sizeof(uint16)) {
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->rx_match.dlen) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->rx_match.dlen = nla_get_u16(iter);
			if (cmd_data->rx_match.dlen > MAX_MATCH_FILTER_LEN) {
				ret = -EINVAL;
				WL_ERR_RLMT(("Not allowed beyond %d\n", MAX_MATCH_FILTER_LEN));
				goto exit;
			}
			break;
		case NAN_ATTRIBUTE_RX_MATCH_FILTER:
			if ((!cmd_data->rx_match.dlen) ||
			    (nla_len(iter) != cmd_data->rx_match.dlen)) {
				WL_ERR(("RX match filter len wrong:%d,%d\n",
					cmd_data->rx_match.dlen, nla_len(iter)));
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->rx_match.data) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->rx_match.data =
				MALLOCZ(cfg->osh, cmd_data->rx_match.dlen);
			if (cmd_data->rx_match.data == NULL) {
				WL_ERR(("failed to allocate LEN=[%u]\n",
					cmd_data->rx_match.dlen));
				ret = -ENOMEM;
				goto exit;
			}
			ret = memcpy_s(cmd_data->rx_match.data, cmd_data->rx_match.dlen,
					nla_data(iter), nla_len(iter));
			if (ret != BCME_OK) {
				WL_ERR(("Failed to copy rx match data\n"));
				return ret;
			}
			break;
		case NAN_ATTRIBUTE_TX_MATCH_FILTER_LEN:
			if (nla_len(iter) != sizeof(uint16)) {
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->tx_match.dlen) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->tx_match.dlen = nla_get_u16(iter);
			if (cmd_data->tx_match.dlen > MAX_MATCH_FILTER_LEN) {
				ret = -EINVAL;
				WL_ERR_RLMT(("Not allowed beyond %d\n", MAX_MATCH_FILTER_LEN));
				goto exit;
			}
			break;
		case NAN_ATTRIBUTE_TX_MATCH_FILTER:
			if ((!cmd_data->tx_match.dlen) ||
			    (nla_len(iter) != cmd_data->tx_match.dlen)) {
				WL_ERR(("TX match filter len wrong:%d,%d\n",
					cmd_data->tx_match.dlen, nla_len(iter)));
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->tx_match.data) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->tx_match.data =
				MALLOCZ(cfg->osh, cmd_data->tx_match.dlen);
			if (cmd_data->tx_match.data == NULL) {
				WL_ERR(("failed to allocate LEN=[%u]\n",
					cmd_data->tx_match.dlen));
				ret = -EINVAL;
				goto exit;
			}
			ret = memcpy_s(cmd_data->tx_match.data, cmd_data->tx_match.dlen,
					nla_data(iter), nla_len(iter));
			if (ret != BCME_OK) {
				WL_ERR(("Failed to copy tx match data\n"));
				return ret;
			}
			break;
		case NAN_ATTRIBUTE_MAC_ADDR_LIST_NUM_ENTRIES:
			if (nla_len(iter) != sizeof(uint16)) {
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->mac_list.num_mac_addr) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->mac_list.num_mac_addr = nla_get_u16(iter);
			if (cmd_data->mac_list.num_mac_addr >= NAN_SRF_MAX_MAC) {
				WL_ERR(("trying to overflow num :%d\n",
					cmd_data->mac_list.num_mac_addr));
				cmd_data->mac_list.num_mac_addr = 0;
				ret = -EINVAL;
				goto exit;
			}
			break;
		case NAN_ATTRIBUTE_MAC_ADDR_LIST:
			if ((!cmd_data->mac_list.num_mac_addr) ||
			    (nla_len(iter) != (cmd_data->mac_list.num_mac_addr * ETHER_ADDR_LEN))) {
				WL_ERR(("wrong mac list len:%d,%d\n",
					cmd_data->mac_list.num_mac_addr, nla_len(iter)));
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->mac_list.list) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->mac_list.list =
				MALLOCZ(cfg->osh, (cmd_data->mac_list.num_mac_addr
						* ETHER_ADDR_LEN));
			if (cmd_data->mac_list.list == NULL) {
				WL_ERR(("failed to allocate LEN=[%u]\n",
				(cmd_data->mac_list.num_mac_addr * ETHER_ADDR_LEN)));
				ret = -ENOMEM;
				goto exit;
			}
			ret = memcpy_s(cmd_data->mac_list.list,
				(cmd_data->mac_list.num_mac_addr * ETHER_ADDR_LEN),
				nla_data(iter), nla_len(iter));
			if (ret != BCME_OK) {
				WL_ERR(("Failed to copy list of mac addresses\n"));
				return ret;
			}
			break;
		case NAN_ATTRIBUTE_TX_TYPE:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			val_u8 =  nla_get_u8(iter);
			if (val_u8 == 0) {
				cmd_data->flags |= WL_NAN_PUB_BCAST;
				WL_TRACE(("NAN_ATTRIBUTE_TX_TYPE: flags=NAN_PUB_BCAST\n"));
			}
			break;
		case NAN_ATTRIBUTE_SDE_CONTROL_CONFIG_DP:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			if (nla_get_u8(iter) == 1) {
				cmd_data->sde_control_flag
					|= NAN_SDE_CF_DP_REQUIRED;
				break;
			}
			break;
		case NAN_ATTRIBUTE_SDE_CONTROL_RANGE_SUPPORT:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->sde_control_config = TRUE;
			if (nla_get_u8(iter) == 1) {
				cmd_data->sde_control_flag
					|= NAN_SDE_CF_RANGING_REQUIRED;
				break;
			}
			break;
		case NAN_ATTRIBUTE_SDE_CONTROL_DP_TYPE:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			if (nla_get_u8(iter) == 1) {
				cmd_data->sde_control_flag
					|= NAN_SDE_CF_MULTICAST_TYPE;
				break;
			}
			break;
		case NAN_ATTRIBUTE_SDE_CONTROL_SECURITY:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			if (nla_get_u8(iter) == 1) {
				cmd_data->sde_control_flag
					|= NAN_SDE_CF_SECURITY_REQUIRED;
				break;
			}
			break;
		case NAN_ATTRIBUTE_RECV_IND_CFG:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->recv_ind_flag = nla_get_u8(iter);
			break;
		case NAN_ATTRIBUTE_CIPHER_SUITE_TYPE:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->csid = nla_get_u8(iter);
			WL_TRACE(("CSID = %u\n", cmd_data->csid));
			break;
		case NAN_ATTRIBUTE_KEY_TYPE:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->key_type = nla_get_u8(iter);
			WL_TRACE(("Key Type = %u\n", cmd_data->key_type));
			break;
		case NAN_ATTRIBUTE_KEY_LEN:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->key.dlen) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->key.dlen = nla_get_u32(iter);
			if ((!cmd_data->key.dlen) || (cmd_data->key.dlen > WL_NAN_NCS_SK_PMK_LEN)) {
				WL_ERR(("invalid key length = %u\n",
					cmd_data->key.dlen));
				break;
			}
			WL_TRACE(("valid key length = %u\n", cmd_data->key.dlen));
			break;
		case NAN_ATTRIBUTE_KEY_DATA:
			if (!cmd_data->key.dlen ||
			    (nla_len(iter) != cmd_data->key.dlen)) {
				WL_ERR(("failed to allocate key data by invalid len=%d,%d\n",
					cmd_data->key.dlen, nla_len(iter)));
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->key.data) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}

			cmd_data->key.data = MALLOCZ(cfg->osh, NAN_MAX_PMK_LEN);
			if (cmd_data->key.data == NULL) {
				WL_ERR(("failed to allocate key data, len=%d\n",
					cmd_data->key.dlen));
				ret = -ENOMEM;
				goto exit;
			}
			ret = memcpy_s(cmd_data->key.data, NAN_MAX_PMK_LEN,
					nla_data(iter), nla_len(iter));
			if (ret != BCME_OK) {
				WL_ERR(("Failed to key data\n"));
				return ret;
			}
			break;
		case NAN_ATTRIBUTE_RSSI_THRESHOLD_FLAG:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			if (nla_get_u8(iter) == 1) {
				cmd_data->flags |=
					WL_NAN_RANGE_LIMITED;
				break;
			}
			break;
		case NAN_ATTRIBUTE_DISC_IND_CFG:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->disc_ind_cfg = nla_get_u8(iter);
			break;
		case NAN_ATTRIBUTE_SDEA_SERVICE_SPECIFIC_INFO_LEN:
			if (nla_len(iter) != sizeof(uint16)) {
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->sde_svc_info.dlen) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->sde_svc_info.dlen = nla_get_u16(iter);
			if (cmd_data->sde_svc_info.dlen > MAX_SDEA_SVC_INFO_LEN) {
				ret = -EINVAL;
				WL_ERR_RLMT(("Not allowed beyond %d\n", MAX_SDEA_SVC_INFO_LEN));
				goto exit;
			}
			break;
		case NAN_ATTRIBUTE_SDEA_SERVICE_SPECIFIC_INFO:
			if ((!cmd_data->sde_svc_info.dlen) ||
			    (nla_len(iter) != cmd_data->sde_svc_info.dlen)) {
				WL_ERR(("wrong sdea info len:%d,%d\n",
					cmd_data->sde_svc_info.dlen, nla_len(iter)));
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->sde_svc_info.data) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->sde_svc_info.data = MALLOCZ(cfg->osh,
				cmd_data->sde_svc_info.dlen);
			if (cmd_data->sde_svc_info.data == NULL) {
				WL_ERR(("failed to allocate svc info data, len=%d\n",
					cmd_data->sde_svc_info.dlen));
				ret = -ENOMEM;
				goto exit;
			}
			ret = memcpy_s(cmd_data->sde_svc_info.data,
					cmd_data->sde_svc_info.dlen,
					nla_data(iter), nla_len(iter));
			if (ret != BCME_OK) {
				WL_ERR(("Failed to sdea info data\n"));
				return ret;
			}
			break;
		case NAN_ATTRIBUTE_SECURITY:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->ndp_cfg.security_cfg = nla_get_u8(iter);
			break;
		case NAN_ATTRIBUTE_RANGING_INTERVAL:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->ranging_intvl_msec = nla_get_u32(iter);
			break;
		case NAN_ATTRIBUTE_RANGING_INGRESS_LIMIT:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->ingress_limit = nla_get_u32(iter);
			break;
		case NAN_ATTRIBUTE_RANGING_EGRESS_LIMIT:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->egress_limit = nla_get_u32(iter);
			break;
		case NAN_ATTRIBUTE_RANGING_INDICATION:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->ranging_indication = nla_get_u32(iter);
			break;
		/* Nan accept policy: Per service basis policy
		 * Based on this policy(ALL/NONE), responder side
		 * will send ACCEPT/REJECT
		 */
		case NAN_ATTRIBUTE_SVC_RESPONDER_POLICY:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->service_responder_policy = nla_get_u8(iter);
			break;
		default:
			WL_ERR(("Unknown type, %d\n", attr_type));
			ret = -EINVAL;
			goto exit;
		}
	}
exit:
	/* We need to call set_config_handler b/f calling start enable TBD */
	NAN_DBG_EXIT();
	return ret;
}

static int
wl_cfgvendor_nan_parse_args(struct wiphy *wiphy, const void *buf,
	int len, nan_config_cmd_data_t *cmd_data, uint32 *nan_attr_mask)
{
	int ret = BCME_OK;
	int attr_type;
	int rem = len;
	const struct nlattr *iter;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int chan;
	u8 sid_beacon = 0, sub_sid_beacon = 0;

	NAN_DBG_ENTER();

	nla_for_each_attr(iter, buf, len, rem) {
		attr_type = nla_type(iter);
		WL_TRACE(("attr: %s (%u)\n", nan_attr_to_str(attr_type), attr_type));

		switch (attr_type) {
		/* NAN Enable request attributes */
		case NAN_ATTRIBUTE_2G_SUPPORT:{
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->support_2g = nla_get_u8(iter);
			if (cmd_data->support_2g == 0) {
				WL_ERR((" 2.4GHz support is not set \n"));
				cmd_data->status = BCME_BADARG;
				goto exit;
			}
			*nan_attr_mask |= NAN_ATTR_SUPPORT_2G_CONFIG;
			break;
		}
		case NAN_ATTRIBUTE_5G_SUPPORT:{
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->support_5g = nla_get_u8(iter);
			*nan_attr_mask |= NAN_ATTR_SUPPORT_5G_CONFIG;
			break;
		}
		case NAN_ATTRIBUTE_CLUSTER_LOW: {
			if (nla_len(iter) != sizeof(uint16)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->cluster_low = nla_get_u16(iter);
			break;
		}
		case NAN_ATTRIBUTE_CLUSTER_HIGH: {
			if (nla_len(iter) != sizeof(uint16)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->cluster_high = nla_get_u16(iter);
			break;
		}
		case NAN_ATTRIBUTE_SID_BEACON: {
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			sid_beacon = nla_get_u8(iter);
			cmd_data->sid_beacon.sid_enable = (sid_beacon & 0x01);
			if (cmd_data->sid_beacon.sid_enable) {
				cmd_data->sid_beacon.sid_count = (sid_beacon >> 1);
				*nan_attr_mask |= NAN_ATTR_SID_BEACON_CONFIG;
			} else {
				WL_ERR((" sid beacon is not valid \n"));
				cmd_data->status = BCME_BADARG;
				goto exit;
			}

			break;
		}
		case NAN_ATTRIBUTE_SUB_SID_BEACON: {
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			sub_sid_beacon = nla_get_u8(iter);
			cmd_data->sid_beacon.sub_sid_enable = (sub_sid_beacon & 0x01);
			if (cmd_data->sid_beacon.sub_sid_enable) {
				cmd_data->sid_beacon.sub_sid_count = (sub_sid_beacon >> 1);
				*nan_attr_mask |= NAN_ATTR_SUB_SID_BEACON_CONFIG;
			} else {
				WL_ERR((" sub sid beacon is not valid \n"));
				cmd_data->status = BCME_BADARG;
				goto exit;
			}
			break;
		}
		case NAN_ATTRIBUTE_SYNC_DISC_2G_BEACON:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->beacon_2g_val = nla_get_u8(iter);
			*nan_attr_mask |= NAN_ATTR_SYNC_DISC_2G_BEACON_CONFIG;
			break;
		case NAN_ATTRIBUTE_SYNC_DISC_5G_BEACON:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->beacon_5g_val = nla_get_u8(iter);
			*nan_attr_mask |= NAN_ATTR_SYNC_DISC_5G_BEACON_CONFIG;
			break;
		case NAN_ATTRIBUTE_SDF_2G_SUPPORT:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->sdf_2g_val = nla_get_u8(iter);
			*nan_attr_mask |= NAN_ATTR_SDF_2G_SUPPORT_CONFIG;
			break;
		case NAN_ATTRIBUTE_SDF_5G_SUPPORT:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->sdf_5g_val = nla_get_u8(iter);
			*nan_attr_mask |= NAN_ATTR_SDF_5G_SUPPORT_CONFIG;
			break;
		case NAN_ATTRIBUTE_HOP_COUNT_LIMIT:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->hop_count_limit = nla_get_u8(iter);
			if (cmd_data->hop_count_limit == 0) {
				WL_ERR((" hop count limit is not valid \n"));
				cmd_data->status = BCME_BADARG;
				goto exit;
			}
			*nan_attr_mask |= NAN_ATTR_HOP_COUNT_LIMIT_CONFIG;
			break;
		case NAN_ATTRIBUTE_RANDOM_TIME:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->metrics.random_factor = nla_get_u8(iter);
			*nan_attr_mask |= NAN_ATTR_RAND_FACTOR_CONFIG;
			break;
		case NAN_ATTRIBUTE_MASTER_PREF:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->metrics.master_pref = nla_get_u8(iter);
			break;
		case NAN_ATTRIBUTE_OUI:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->nan_oui = nla_get_u32(iter);
			*nan_attr_mask |= NAN_ATTR_OUI_CONFIG;
			WL_TRACE(("nan_oui=%d\n", cmd_data->nan_oui));
			break;
		case NAN_ATTRIBUTE_WARMUP_TIME:
			if (nla_len(iter) != sizeof(uint16)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->warmup_time = nla_get_u16(iter);
			break;
		case NAN_ATTRIBUTE_AMBTT:
		case NAN_ATTRIBUTE_MASTER_RANK:
			WL_DBG(("Unhandled attribute, %d\n", attr_type));
			break;
		case NAN_ATTRIBUTE_CHANNEL: {
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			/* take the default channel start_factor frequency */
			chan = wf_mhz2channel((uint)nla_get_u32(iter), 0);
			if (chan <= CH_MAX_2G_CHANNEL) {
				cmd_data->chanspec[0] = wf_channel2chspec(chan, WL_CHANSPEC_BW_20);
			} else {
				cmd_data->chanspec[0] = wf_channel2chspec(chan, WL_CHANSPEC_BW_80);
			}
			if (cmd_data->chanspec[0] == 0) {
				WL_ERR(("Channel is not valid \n"));
				ret = -EINVAL;
				goto exit;
			}
			WL_TRACE(("valid chanspec, chanspec = 0x%04x \n",
				cmd_data->chanspec[0]));
			break;
		}
		case NAN_ATTRIBUTE_24G_CHANNEL: {
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			/* take the default channel start_factor frequency */
			chan = wf_mhz2channel((uint)nla_get_u32(iter), 0);
			/* 20MHz as BW */
			cmd_data->chanspec[1] = wf_channel2chspec(chan, WL_CHANSPEC_BW_20);
			if (cmd_data->chanspec[1] == 0) {
				WL_ERR((" 2.4GHz Channel is not valid \n"));
				ret = -EINVAL;
				break;
			}
			*nan_attr_mask |= NAN_ATTR_2G_CHAN_CONFIG;
			WL_TRACE(("valid 2.4GHz chanspec, chanspec = 0x%04x \n",
				cmd_data->chanspec[1]));
			break;
		}
		case NAN_ATTRIBUTE_5G_CHANNEL: {
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			/* take the default channel start_factor frequency */
			chan = wf_mhz2channel((uint)nla_get_u32(iter), 0);
			/* 20MHz as BW */
			cmd_data->chanspec[2] = wf_channel2chspec(chan, WL_CHANSPEC_BW_20);
			if (cmd_data->chanspec[2] == 0) {
				WL_ERR((" 5GHz Channel is not valid \n"));
				ret = -EINVAL;
				break;
			}
			*nan_attr_mask |= NAN_ATTR_5G_CHAN_CONFIG;
			WL_TRACE(("valid 5GHz chanspec, chanspec = 0x%04x \n",
				cmd_data->chanspec[2]));
			break;
		}
		case NAN_ATTRIBUTE_CONF_CLUSTER_VAL:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->config_cluster_val = nla_get_u8(iter);
			*nan_attr_mask |= NAN_ATTR_CLUSTER_VAL_CONFIG;
			break;
		case NAN_ATTRIBUTE_DWELL_TIME:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->dwell_time[0] = nla_get_u8(iter);
			if (cmd_data->dwell_time[0] == 0) {
				WL_ERR((" 2.4GHz dwell time is not valid \n"));
				cmd_data->status = BCME_BADARG;
				goto exit;
			}
			*nan_attr_mask |= NAN_ATTR_2G_DWELL_TIME_CONFIG;
			break;
		case NAN_ATTRIBUTE_SCAN_PERIOD:
			if (nla_len(iter) != sizeof(uint16)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->scan_period[0] = nla_get_u16(iter);
			if (cmd_data->scan_period[0] == 0) {
				WL_ERR((" 2.4GHz scan period is not valid \n"));
				cmd_data->status = BCME_BADARG;
				goto exit;
			}
			*nan_attr_mask |= NAN_ATTR_2G_SCAN_PERIOD_CONFIG;
			break;
		case NAN_ATTRIBUTE_DWELL_TIME_5G:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->dwell_time[1] = nla_get_u8(iter);
			if (cmd_data->dwell_time[1] == 0) {
				WL_ERR((" 5GHz dwell time is not valid \n"));
				cmd_data->status = BCME_BADARG;
				goto exit;
			}
			*nan_attr_mask |= NAN_ATTR_5G_DWELL_TIME_CONFIG;
			break;
		case NAN_ATTRIBUTE_SCAN_PERIOD_5G:
			if (nla_len(iter) != sizeof(uint16)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->scan_period[1] = nla_get_u16(iter);
			if (cmd_data->scan_period[1] == 0) {
				WL_ERR((" 5GHz scan period is not valid \n"));
				cmd_data->status = BCME_BADARG;
				goto exit;
			}
			*nan_attr_mask |= NAN_ATTR_5G_SCAN_PERIOD_CONFIG;
			break;
		case NAN_ATTRIBUTE_AVAIL_BIT_MAP:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->bmap = nla_get_u32(iter);
			break;
		case NAN_ATTRIBUTE_ENTRY_CONTROL:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->avail_params.duration = nla_get_u8(iter);
			break;
		case NAN_ATTRIBUTE_RSSI_CLOSE:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->rssi_attr.rssi_close_2dot4g_val = nla_get_s8(iter);
			if (cmd_data->rssi_attr.rssi_close_2dot4g_val == 0) {
				WL_ERR((" 2.4GHz rssi close is not valid \n"));
				cmd_data->status = BCME_BADARG;
				goto exit;
			}
			*nan_attr_mask |= NAN_ATTR_RSSI_CLOSE_CONFIG;
			break;
		case NAN_ATTRIBUTE_RSSI_MIDDLE:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->rssi_attr.rssi_middle_2dot4g_val = nla_get_s8(iter);
			if (cmd_data->rssi_attr.rssi_middle_2dot4g_val == 0) {
				WL_ERR((" 2.4GHz rssi middle is not valid \n"));
				cmd_data->status = BCME_BADARG;
				goto exit;
			}
			*nan_attr_mask |= NAN_ATTR_RSSI_MIDDLE_2G_CONFIG;
			break;
		case NAN_ATTRIBUTE_RSSI_PROXIMITY:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->rssi_attr.rssi_proximity_2dot4g_val = nla_get_s8(iter);
			if (cmd_data->rssi_attr.rssi_proximity_2dot4g_val == 0) {
				WL_ERR((" 2.4GHz rssi proximity is not valid \n"));
				cmd_data->status = BCME_BADARG;
				goto exit;
			}
			*nan_attr_mask |= NAN_ATTR_RSSI_PROXIMITY_2G_CONFIG;
			break;
		case NAN_ATTRIBUTE_RSSI_CLOSE_5G:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->rssi_attr.rssi_close_5g_val = nla_get_s8(iter);
			if (cmd_data->rssi_attr.rssi_close_5g_val == 0) {
				WL_ERR((" 5GHz rssi close is not valid \n"));
				cmd_data->status = BCME_BADARG;
				goto exit;
			}
			*nan_attr_mask |= NAN_ATTR_RSSI_CLOSE_5G_CONFIG;
			break;
		case NAN_ATTRIBUTE_RSSI_MIDDLE_5G:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->rssi_attr.rssi_middle_5g_val = nla_get_s8(iter);
			if (cmd_data->rssi_attr.rssi_middle_5g_val == 0) {
				WL_ERR((" 5Hz rssi middle is not valid \n"));
				cmd_data->status = BCME_BADARG;
				goto exit;
			}
			*nan_attr_mask |= NAN_ATTR_RSSI_MIDDLE_5G_CONFIG;
			break;
		case NAN_ATTRIBUTE_RSSI_PROXIMITY_5G:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->rssi_attr.rssi_proximity_5g_val = nla_get_s8(iter);
			if (cmd_data->rssi_attr.rssi_proximity_5g_val == 0) {
				WL_ERR((" 5GHz rssi proximity is not valid \n"));
				cmd_data->status = BCME_BADARG;
				goto exit;
			}
			*nan_attr_mask |= NAN_ATTR_RSSI_PROXIMITY_5G_CONFIG;
			break;
		case NAN_ATTRIBUTE_RSSI_WINDOW_SIZE:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->rssi_attr.rssi_window_size = nla_get_u8(iter);
			if (cmd_data->rssi_attr.rssi_window_size == 0) {
				WL_ERR((" rssi window size is not valid \n"));
				cmd_data->status = BCME_BADARG;
				goto exit;
			}
			*nan_attr_mask |= NAN_ATTR_RSSI_WINDOW_SIZE_CONFIG;
			break;
		case NAN_ATTRIBUTE_CIPHER_SUITE_TYPE:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->csid = nla_get_u8(iter);
			WL_TRACE(("CSID = %u\n", cmd_data->csid));
			break;
		case NAN_ATTRIBUTE_SCID_LEN:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->scid.dlen) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->scid.dlen = nla_get_u32(iter);
			if (cmd_data->scid.dlen > MAX_SCID_LEN) {
				ret = -EINVAL;
				WL_ERR_RLMT(("Not allowed beyond %d\n", MAX_SCID_LEN));
				goto exit;
			}
			WL_TRACE(("valid scid length = %u\n", cmd_data->scid.dlen));
			break;
		case NAN_ATTRIBUTE_SCID:
			if (!cmd_data->scid.dlen || (nla_len(iter) != cmd_data->scid.dlen)) {
				WL_ERR(("wrong scid len:%d,%d\n", cmd_data->scid.dlen,
					nla_len(iter)));
				ret = -EINVAL;
				goto exit;
			}
			if (cmd_data->scid.data) {
				WL_ERR(("trying to overwrite:%d\n", attr_type));
				ret = -EINVAL;
				goto exit;
			}

			cmd_data->scid.data = MALLOCZ(cfg->osh, cmd_data->scid.dlen);
			if (cmd_data->scid.data == NULL) {
				WL_ERR(("failed to allocate scid, len=%d\n",
					cmd_data->scid.dlen));
				ret = -ENOMEM;
				goto exit;
			}
			ret = memcpy_s(cmd_data->scid.data, cmd_data->scid.dlen,
					nla_data(iter), nla_len(iter));
			if (ret != BCME_OK) {
				WL_ERR(("Failed to scid data\n"));
				return ret;
			}
			break;
		case NAN_ATTRIBUTE_2G_AWAKE_DW:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			if (nla_get_u32(iter) > NAN_MAX_AWAKE_DW_INTERVAL) {
				WL_ERR(("%s: Invalid/Out of bound value = %u\n",
						__FUNCTION__, nla_get_u32(iter)));
				ret = -EINVAL;
				goto exit;
			}
			if (nla_get_u32(iter)) {
				cmd_data->awake_dws.dw_interval_2g =
					1 << (nla_get_u32(iter)-1);
			}
			*nan_attr_mask |= NAN_ATTR_2G_DW_CONFIG;
			break;
		case NAN_ATTRIBUTE_5G_AWAKE_DW:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			if (nla_get_u32(iter) > NAN_MAX_AWAKE_DW_INTERVAL) {
				WL_ERR(("%s: Invalid/Out of bound value = %u\n",
						__FUNCTION__, nla_get_u32(iter)));
				ret = BCME_BADARG;
				break;
			}
			if (nla_get_u32(iter)) {
				cmd_data->awake_dws.dw_interval_5g =
					1 << (nla_get_u32(iter)-1);
			}
			*nan_attr_mask |= NAN_ATTR_5G_DW_CONFIG;
			break;
		case NAN_ATTRIBUTE_DISC_IND_CFG:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->disc_ind_cfg = nla_get_u8(iter);
			break;
		case NAN_ATTRIBUTE_MAC_ADDR:
			if (nla_len(iter) != ETHER_ADDR_LEN) {
				ret = -EINVAL;
				goto exit;
			}
			ret = memcpy_s((char*)&cmd_data->mac_addr, ETHER_ADDR_LEN,
					(char*)nla_data(iter), nla_len(iter));
			if (ret != BCME_OK) {
				WL_ERR(("Failed to copy mac addr\n"));
				return ret;
			}
			break;
		case NAN_ATTRIBUTE_RANDOMIZATION_INTERVAL:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			/* run time nmi rand not supported as of now.
			* Only during nan enable/iface-create rand mac is used
			*/
			cmd_data->nmi_rand_intvl = nla_get_u32(iter);
			if (cmd_data->nmi_rand_intvl > 0) {
				cfg->nancfg->mac_rand = true;
			} else {
				cfg->nancfg->mac_rand = false;
			}
			break;
		case NAN_ATTRIBUTE_CMD_USE_NDPE:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->use_ndpe_attr = nla_get_u32(iter);
			break;
		case NAN_ATTRIBUTE_ENABLE_MERGE:
			if (nla_len(iter) != sizeof(uint8)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->enable_merge = nla_get_u8(iter);
			break;
		case NAN_ATTRIBUTE_DISCOVERY_BEACON_INTERVAL:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->disc_bcn_interval = nla_get_u32(iter);
			*nan_attr_mask |= NAN_ATTR_DISC_BEACON_INTERVAL;
			break;
		case NAN_ATTRIBUTE_NSS:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			/* FW handles it internally,
			* nothing to do as per the value rxed from framework, ignore.
			*/
			break;
		case NAN_ATTRIBUTE_ENABLE_RANGING:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			cfg->nancfg->ranging_enable = nla_get_u32(iter);
			break;
		case NAN_ATTRIBUTE_DW_EARLY_TERM:
			if (nla_len(iter) != sizeof(uint32)) {
				ret = -EINVAL;
				goto exit;
			}
			cmd_data->dw_early_termination = nla_get_u32(iter);
			break;
		default:
			WL_ERR(("%s: Unknown type, %d\n", __FUNCTION__, attr_type));
			ret = -EINVAL;
			goto exit;
		}
	}

exit:
	/* We need to call set_config_handler b/f calling start enable TBD */
	NAN_DBG_EXIT();
	if (ret) {
		WL_ERR(("%s: Failed to parse attribute %d ret %d",
			__FUNCTION__, attr_type, ret));
	}
	return ret;

}

static int
wl_cfgvendor_nan_dp_estb_event_data_filler(struct sk_buff *msg,
	nan_event_data_t *event_data) {
	int ret = BCME_OK;
	ret = nla_put_u32(msg, NAN_ATTRIBUTE_NDP_ID, event_data->ndp_id);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put NDP ID, ret=%d\n", ret));
		goto fail;
	}
	/*
	 * NDI mac address of the peer
	 * (required to derive target ipv6 address)
	 */
	ret = nla_put(msg, NAN_ATTRIBUTE_PEER_NDI_MAC_ADDR, ETH_ALEN,
			event_data->responder_ndi.octet);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put resp ndi, ret=%d\n", ret));
		goto fail;
	}
	ret = nla_put_u8(msg, NAN_ATTRIBUTE_RSP_CODE, event_data->status);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put response code, ret=%d\n", ret));
		goto fail;
	}
	if (event_data->svc_info.dlen && event_data->svc_info.data) {
		ret = nla_put_u16(msg, NAN_ATTRIBUTE_SERVICE_SPECIFIC_INFO_LEN,
				event_data->svc_info.dlen);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put svc info len, ret=%d\n", ret));
			goto fail;
		}
		ret = nla_put(msg, NAN_ATTRIBUTE_SERVICE_SPECIFIC_INFO,
				event_data->svc_info.dlen, event_data->svc_info.data);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put svc info, ret=%d\n", ret));
			goto fail;
		}
	}

fail:
	return ret;
}
static int
wl_cfgvendor_nan_dp_ind_event_data_filler(struct sk_buff *msg,
		nan_event_data_t *event_data) {
	int ret = BCME_OK;

	ret = nla_put_u32(msg, NAN_ATTRIBUTE_PUBLISH_ID,
			event_data->pub_id);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put pub ID, ret=%d\n", ret));
		goto fail;
	}
	ret = nla_put_u32(msg, NAN_ATTRIBUTE_NDP_ID, event_data->ndp_id);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put NDP ID, ret=%d\n", ret));
		goto fail;
	}
	/* Discovery MAC addr of the peer/initiator */
	ret = nla_put(msg, NAN_ATTRIBUTE_MAC_ADDR, ETH_ALEN,
			event_data->remote_nmi.octet);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put remote NMI, ret=%d\n", ret));
		goto fail;
	}
	ret = nla_put_u8(msg, NAN_ATTRIBUTE_SECURITY, event_data->security);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put security, ret=%d\n", ret));
		goto fail;
	}
	if (event_data->svc_info.dlen && event_data->svc_info.data) {
		ret = nla_put_u16(msg, NAN_ATTRIBUTE_SERVICE_SPECIFIC_INFO_LEN,
				event_data->svc_info.dlen);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put svc info len, ret=%d\n", ret));
			goto fail;
		}
		ret = nla_put(msg, NAN_ATTRIBUTE_SERVICE_SPECIFIC_INFO,
				event_data->svc_info.dlen, event_data->svc_info.data);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put svc info, ret=%d\n", ret));
			goto fail;
		}
	}

fail:
	return ret;
}

static int
wl_cfgvendor_nan_tx_followup_ind_event_data_filler(struct sk_buff *msg,
	nan_event_data_t *event_data) {
	int ret = BCME_OK;
	ret = nla_put_u16(msg, NAN_ATTRIBUTE_TRANSAC_ID, event_data->token);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put transaction id, ret=%d\n", ret));
		goto fail;
	}
	ret = nla_put_u8(msg, NAN_ATTRIBUTE_HANDLE, event_data->local_inst_id);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put handle, ret=%d\n", ret));
		goto fail;
	}
	ret = nla_put_u16(msg, NAN_ATTRIBUTE_STATUS, event_data->status);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put nan status, ret=%d\n", ret));
		goto fail;
	}
	if (event_data->status == NAN_STATUS_SUCCESS) {
		ret = nla_put(msg, NAN_ATTRIBUTE_REASON,
				strlen("NAN_STATUS_SUCCESS"), event_data->nan_reason);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put nan reason, ret=%d\n", ret));
			goto fail;
		}
	} else {
		ret = nla_put(msg, NAN_ATTRIBUTE_REASON,
				strlen("NAN_STATUS_NO_OTA_ACK"), event_data->nan_reason);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put nan reason, ret=%d\n", ret));
			goto fail;
		}
	}
fail:
	return ret;
}

static int
wl_cfgvendor_nan_svc_terminate_event_filler(struct sk_buff *msg,
	struct bcm_cfg80211 *cfg, int event_id, nan_event_data_t *event_data) {
	int ret = BCME_OK;
	ret = nla_put_u8(msg, NAN_ATTRIBUTE_HANDLE, event_data->local_inst_id);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put handle, ret=%d\n", ret));
		goto fail;
	}

	if (event_id == GOOGLE_NAN_EVENT_SUBSCRIBE_TERMINATED) {
		ret = nla_put_u16(msg, NAN_ATTRIBUTE_SUBSCRIBE_ID,
				event_data->local_inst_id);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put local inst id, ret=%d\n", ret));
			goto fail;
		}
	} else {
		ret = nla_put_u32(msg, NAN_ATTRIBUTE_PUBLISH_ID,
				event_data->local_inst_id);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put local inst id, ret=%d\n", ret));
			goto fail;
		}
	}
	ret = nla_put_u16(msg, NAN_ATTRIBUTE_STATUS, event_data->status);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put status, ret=%d\n", ret));
		goto fail;
	}
	if (event_data->status == NAN_STATUS_SUCCESS) {
		ret = nla_put(msg, NAN_ATTRIBUTE_REASON,
				strlen("NAN_STATUS_SUCCESS"), event_data->nan_reason);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put nan reason, ret=%d\n", ret));
			goto fail;
		}
	} else {
		ret = nla_put(msg, NAN_ATTRIBUTE_REASON,
				strlen("NAN_STATUS_INTERNAL_FAILURE"), event_data->nan_reason);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put nan reason, ret=%d\n", ret));
			goto fail;
		}
	}

	ret = wl_cfgnan_remove_inst_id(cfg, event_data->local_inst_id);
	if (ret) {
		WL_ERR(("failed to free svc instance-id[%d], ret=%d, event_id = %d\n",
				event_data->local_inst_id, ret, event_id));
		goto fail;
	}
fail:
	return ret;
}

static int
wl_cfgvendor_nan_opt_params_filler(struct sk_buff *msg,
	nan_event_data_t *event_data) {
	int ret = BCME_OK;
	/* service specific info data */
	if (event_data->svc_info.dlen && event_data->svc_info.data) {
		ret = nla_put_u16(msg, NAN_ATTRIBUTE_SERVICE_SPECIFIC_INFO_LEN,
				event_data->svc_info.dlen);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put svc info len, ret=%d\n", ret));
			goto fail;
		}
		ret = nla_put(msg, NAN_ATTRIBUTE_SERVICE_SPECIFIC_INFO,
				event_data->svc_info.dlen, event_data->svc_info.data);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put svc info, ret=%d\n", ret));
			goto fail;
		}
		WL_TRACE(("svc info len = %d\n", event_data->svc_info.dlen));
	}

	/* sdea service specific info data */
	if (event_data->sde_svc_info.dlen && event_data->sde_svc_info.data) {
		ret = nla_put_u16(msg, NAN_ATTRIBUTE_SDEA_SERVICE_SPECIFIC_INFO_LEN,
				event_data->sde_svc_info.dlen);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put sdea svc info len, ret=%d\n", ret));
			goto fail;
		}
		ret = nla_put(msg, NAN_ATTRIBUTE_SDEA_SERVICE_SPECIFIC_INFO,
				event_data->sde_svc_info.dlen,
				event_data->sde_svc_info.data);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put sdea svc info, ret=%d\n", ret));
			goto fail;
		}
		WL_TRACE(("sdea svc info len = %d\n", event_data->sde_svc_info.dlen));
	}
	/* service control discovery range limit */
	/* TODO: */

	/* service control binding bitmap */
	/* TODO: */
fail:
	return ret;
}

static int
wl_cfgvendor_nan_match_expiry_event_filler(struct sk_buff *msg,
		nan_event_data_t *event_data) {
	int ret = BCME_OK;

	WL_DBG(("sub id (local id)=%d, pub id (remote id)=%d\n",
		event_data->sub_id, event_data->pub_id));
	ret = nla_put_u16(msg, NAN_ATTRIBUTE_SUBSCRIBE_ID, event_data->sub_id);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put Sub Id, ret=%d\n", ret));
		goto fail;
	}
	ret = nla_put_u32(msg, NAN_ATTRIBUTE_PUBLISH_ID, event_data->pub_id);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put pub id, ret=%d\n", ret));
		goto fail;
	}
fail:
	return ret;
}

static int
wl_cfgvendor_nan_tx_followup_event_filler(struct sk_buff *msg,
		nan_event_data_t *event_data) {
	int ret = BCME_OK;
	/* In followup pkt, instance id and requestor instance id are configured
	 * from the transmitter perspective. As the event is processed with the
	 * role of receiver, the local handle should use requestor instance
	 * id (peer_inst_id)
	 */
	WL_TRACE(("handle=%d\n", event_data->requestor_id));
	WL_TRACE(("inst id (local id)=%d\n", event_data->local_inst_id));
	WL_TRACE(("peer id (remote id)=%d\n", event_data->requestor_id));
	WL_TRACE(("peer mac addr=" MACDBG "\n",
			MAC2STRDBG(event_data->remote_nmi.octet)));
	WL_TRACE(("peer rssi: %d\n", event_data->fup_rssi));
	WL_TRACE(("attribute no: %d\n", event_data->attr_num));
	WL_TRACE(("attribute len: %d\n", event_data->attr_list_len));

	ret = nla_put_u8(msg, NAN_ATTRIBUTE_HANDLE, event_data->requestor_id);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put handle, ret=%d\n", ret));
		goto fail;
	}
	ret = nla_put_u32(msg, NAN_ATTRIBUTE_INST_ID, event_data->local_inst_id);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put local inst id, ret=%d\n", ret));
		goto fail;
	}
	ret = nla_put_u16(msg, NAN_ATTRIBUTE_PEER_ID, event_data->requestor_id);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put requestor inst id, ret=%d\n", ret));
		goto fail;
	}
	ret = nla_put(msg, NAN_ATTRIBUTE_MAC_ADDR, ETHER_ADDR_LEN,
			event_data->remote_nmi.octet);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put remote nmi, ret=%d\n", ret));
		goto fail;
	}
	ret = nla_put_s8(msg, NAN_ATTRIBUTE_RSSI_PROXIMITY,
			event_data->fup_rssi);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put fup rssi, ret=%d\n", ret));
		goto fail;
	}
fail:
	return ret;
}

static int
wl_cfgvendor_nan_sub_match_event_filler(struct sk_buff *msg,
	nan_event_data_t *event_data) {
	int ret = BCME_OK;
	WL_TRACE(("handle (sub_id)=%d\n", event_data->sub_id));
	WL_TRACE(("pub id=%d\n", event_data->pub_id));
	WL_TRACE(("sub id=%d\n", event_data->sub_id));
	WL_TRACE(("pub mac addr=" MACDBG "\n",
			MAC2STRDBG(event_data->remote_nmi.octet)));
	WL_TRACE(("attr no: %d\n", event_data->attr_num));
	WL_TRACE(("attr len: %d\n", event_data->attr_list_len));

	ret = nla_put_u8(msg, NAN_ATTRIBUTE_HANDLE, event_data->sub_id);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put handle, ret=%d\n", ret));
		goto fail;
	}
	ret = nla_put_u32(msg, NAN_ATTRIBUTE_PUBLISH_ID, event_data->pub_id);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put pub id, ret=%d\n", ret));
		goto fail;
	}
	ret = nla_put_u16(msg, NAN_ATTRIBUTE_SUBSCRIBE_ID, event_data->sub_id);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put Sub Id, ret=%d\n", ret));
		goto fail;
	}
	ret = nla_put(msg, NAN_ATTRIBUTE_MAC_ADDR, ETHER_ADDR_LEN,
			event_data->remote_nmi.octet);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put remote NMI, ret=%d\n", ret));
		goto fail;
	}
	if (event_data->publish_rssi) {
		event_data->publish_rssi = -event_data->publish_rssi;
		ret = nla_put_u8(msg, NAN_ATTRIBUTE_RSSI_PROXIMITY,
				event_data->publish_rssi);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put publish rssi, ret=%d\n", ret));
			goto fail;
		}
	}
	if (event_data->ranging_result_present) {
		ret = nla_put_u32(msg, NAN_ATTRIBUTE_RANGING_INDICATION,
				event_data->ranging_ind);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put ranging ind, ret=%d\n", ret));
			goto fail;
		}
		ret = nla_put_u32(msg, NAN_ATTRIBUTE_RANGING_RESULT,
				event_data->range_measurement_cm);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put range measurement cm, ret=%d\n",
					ret));
			goto fail;
		}
	}
	/*
	 * handling optional service control, service response filter
	 */
	if (event_data->tx_match_filter.dlen && event_data->tx_match_filter.data) {
		ret = nla_put_u16(msg, NAN_ATTRIBUTE_TX_MATCH_FILTER_LEN,
				event_data->tx_match_filter.dlen);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put tx match filter len, ret=%d\n",
					ret));
			goto fail;
		}
		ret = nla_put(msg, NAN_ATTRIBUTE_TX_MATCH_FILTER,
				event_data->tx_match_filter.dlen,
				event_data->tx_match_filter.data);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put tx match filter data, ret=%d\n",
					ret));
			goto fail;
		}
		WL_TRACE(("tx matching filter (%d):\n",
				event_data->tx_match_filter.dlen));
	}

fail:
	return ret;
}

static int
wl_cfgvendor_nan_de_event_filler(struct sk_buff *msg, nan_event_data_t *event_data)
{
	int ret = BCME_OK;
	ret = nla_put_u8(msg, NAN_ATTRIBUTE_ENABLE_STATUS, event_data->enabled);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put event_data->enabled, ret=%d\n", ret));
		goto fail;
	}
	ret = nla_put_u8(msg, NAN_ATTRIBUTE_DE_EVENT_TYPE,
			event_data->nan_de_evt_type);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put nan_de_evt_type, ret=%d\n", ret));
		goto fail;
	}
	ret = nla_put(msg, NAN_ATTRIBUTE_CLUSTER_ID, ETH_ALEN,
			event_data->clus_id.octet);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put clust id, ret=%d\n", ret));
		goto fail;
	}
	/* OOB tests requires local nmi */
	ret = nla_put(msg, NAN_ATTRIBUTE_MAC_ADDR, ETH_ALEN,
			event_data->local_nmi.octet);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put NMI, ret=%d\n", ret));
		goto fail;
	}
fail:
	return ret;
}

#ifdef RTT_SUPPORT
s32
wl_cfgvendor_send_as_rtt_legacy_event(struct wiphy *wiphy, struct net_device *dev,
	wl_nan_ev_rng_rpt_ind_t *range_res, uint32 status)
{
	s32 ret = BCME_OK;
	gfp_t kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	rtt_report_t *report = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct sk_buff *msg = NULL;
	struct nlattr *rtt_nl_hdr;

	NAN_DBG_ENTER();

	report = MALLOCZ(cfg->osh, sizeof(*report));
	if (!report) {
		WL_ERR(("%s: memory allocation failed\n", __func__));
		ret = BCME_NOMEM;
		goto exit;
	}
	if (range_res) {
		report->distance = range_res->dist_mm/10;
		ret = memcpy_s(&report->addr, ETHER_ADDR_LEN,
				&range_res->peer_m_addr, ETHER_ADDR_LEN);
		if (ret != BCME_OK) {
			WL_ERR(("Failed to copy peer_m_addr\n"));
			goto exit;
		}
	}
	report->status = (rtt_reason_t)status;
	report->type   = RTT_TWO_WAY;

#if (defined(CONFIG_ARCH_MSM) && defined(SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC)) || \
	LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
	msg = cfg80211_vendor_event_alloc(wiphy, NULL, 100,
			GOOGLE_RTT_COMPLETE_EVENT, kflags);
#else
	msg = cfg80211_vendor_event_alloc(wiphy, 100, GOOGLE_RTT_COMPLETE_EVENT, kflags);
#endif /* (defined(CONFIG_ARCH_MSM) && defined(SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC)) || */
	/* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0) */
	if (!msg) {
		WL_ERR(("%s: fail to allocate skb for vendor event\n", __FUNCTION__));
		ret = BCME_NOMEM;
		goto exit;
	}

	ret = nla_put_u32(msg, RTT_ATTRIBUTE_RESULTS_COMPLETE, 1);
	if (ret < 0) {
		WL_ERR(("Failed to put RTT_ATTRIBUTE_RESULTS_COMPLETE\n"));
		goto exit;
	}
	rtt_nl_hdr = nla_nest_start(msg, RTT_ATTRIBUTE_RESULTS_PER_TARGET);
	if (!rtt_nl_hdr) {
		WL_ERR(("rtt_nl_hdr is NULL\n"));
		ret = BCME_NOMEM;
		goto exit;
	}
	ret = nla_put(msg, RTT_ATTRIBUTE_TARGET_MAC, ETHER_ADDR_LEN, &report->addr);
	if (ret < 0) {
		WL_ERR(("Failed to put RTT_ATTRIBUTE_TARGET_MAC\n"));
		goto exit;
	}
	ret = nla_put_u32(msg, RTT_ATTRIBUTE_RESULT_CNT, 1);
	if (ret < 0) {
		WL_ERR(("Failed to put RTT_ATTRIBUTE_RESULT_CNT\n"));
		goto exit;
	}
	ret = nla_put(msg, RTT_ATTRIBUTE_RESULT,
			sizeof(*report), report);
	if (ret < 0) {
		WL_ERR(("Failed to put RTT_ATTRIBUTE_RESULTS\n"));
		goto exit;
	}
	nla_nest_end(msg, rtt_nl_hdr);
	cfg80211_vendor_event(msg, kflags);
	if (report) {
		MFREE(cfg->osh, report, sizeof(*report));
	}

	return ret;
exit:
	if (msg)
		dev_kfree_skb_any(msg);
	WL_ERR(("Failed to send event GOOGLE_RTT_COMPLETE_EVENT,"
				" -- Free skb, ret = %d\n", ret));
	if (report)
		MFREE(cfg->osh, report, sizeof(*report));
	NAN_DBG_EXIT();
	return ret;
}
#endif /* RTT_SUPPORT */

static int
wl_cfgvendor_send_nan_async_resp(struct wiphy *wiphy, struct wireless_dev *wdev,
	int event_id, u8* nan_req_resp, u16 len)
{
	int ret = BCME_OK;
	int buf_len = NAN_EVENT_BUFFER_SIZE_LARGE;
	gfp_t kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;

	struct sk_buff *msg;

	NAN_DBG_ENTER();

	/* Allocate the skb for vendor event */
	msg = CFG80211_VENDOR_EVENT_ALLOC(wiphy, wdev, buf_len,
		event_id, kflags);
	if (!msg) {
		WL_ERR(("%s: fail to allocate skb for vendor event\n", __FUNCTION__));
		return -ENOMEM;
	}

	ret = nla_put(msg, NAN_ATTRIBUTE_CMD_RESP_DATA,
			len, (u8*)nan_req_resp);
	if (unlikely(ret)) {
		WL_ERR(("Failed to put resp data, ret=%d\n",
				ret));
		goto fail;
	}
	WL_DBG(("Event sent up to hal, event_id = %d, ret = %d\n",
		event_id, ret));
	cfg80211_vendor_event(msg, kflags);
	NAN_DBG_EXIT();
	return ret;

fail:
	dev_kfree_skb_any(msg);
	WL_ERR(("Event not implemented or unknown -- Free skb, event_id = %d, ret = %d\n",
		event_id, ret));
	NAN_DBG_EXIT();
	return ret;
}

int
wl_cfgvendor_nan_send_async_disable_resp(struct wireless_dev *wdev)
{
	int ret = BCME_OK;
	struct wiphy *wiphy = wdev->wiphy;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	nan_hal_resp_t nan_req_resp;
	bzero(&nan_req_resp, sizeof(nan_req_resp));
	nan_req_resp.status = NAN_STATUS_SUCCESS;
	nan_req_resp.value = BCME_OK;
	nan_req_resp.subcmd = NAN_WIFI_SUBCMD_DISABLE;
	WL_INFORM_MEM(("Send NAN_ASYNC_RESPONSE_DISABLED\n"));
	ret = wl_cfgvendor_send_nan_async_resp(wiphy, wdev,
		NAN_ASYNC_RESPONSE_DISABLED, (u8*)&nan_req_resp, sizeof(nan_req_resp));
	cfg->nancfg->notify_user = false;
	return ret;
}

int
wl_cfgvendor_send_nan_event(struct wiphy *wiphy, struct net_device *dev,
	int event_id, nan_event_data_t *event_data)
{
	int ret = BCME_OK;
	int buf_len = NAN_EVENT_BUFFER_SIZE_LARGE;
	gfp_t kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;

	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct sk_buff *msg;

	NAN_DBG_ENTER();

	/* Allocate the skb for vendor event */
	msg = CFG80211_VENDOR_EVENT_ALLOC(wiphy, ndev_to_wdev(dev), buf_len,
		event_id, kflags);
	if (!msg) {
		WL_ERR(("%s: fail to allocate skb for vendor event\n", __FUNCTION__));
		return -ENOMEM;
	}

	switch (event_id) {
	case GOOGLE_NAN_EVENT_DE_EVENT: {
		WL_DBG(("[NAN] GOOGLE_NAN_DE_EVENT cluster id=" MACDBG "nmi= " MACDBG "\n",
			MAC2STRDBG(event_data->clus_id.octet),
			MAC2STRDBG(event_data->local_nmi.octet)));
		ret = wl_cfgvendor_nan_de_event_filler(msg, event_data);
		if (unlikely(ret)) {
			WL_ERR(("Failed to fill de event data, ret=%d\n", ret));
			goto fail;
		}
		break;
	}
	case GOOGLE_NAN_EVENT_SUBSCRIBE_MATCH:
	case GOOGLE_NAN_EVENT_MATCH_EXPIRY:
	case GOOGLE_NAN_EVENT_FOLLOWUP: {
		if (event_id == GOOGLE_NAN_EVENT_SUBSCRIBE_MATCH) {
			WL_DBG(("GOOGLE_NAN_EVENT_SUBSCRIBE_MATCH\n"));
			ret = wl_cfgvendor_nan_sub_match_event_filler(msg, event_data);
			if (unlikely(ret)) {
				WL_ERR(("Failed to fill sub match event data, ret=%d\n", ret));
				goto fail;
			}
		} else if (event_id == GOOGLE_NAN_EVENT_FOLLOWUP) {
			WL_DBG(("GOOGLE_NAN_EVENT_FOLLOWUP\n"));
			ret = wl_cfgvendor_nan_tx_followup_event_filler(msg, event_data);
			if (unlikely(ret)) {
				WL_ERR(("Failed to fill tx follow up event data, ret=%d\n", ret));
				goto fail;
			}
		} else if (event_id == GOOGLE_NAN_EVENT_MATCH_EXPIRY) {
			WL_DBG(("GOOGLE_NAN_EVENT_MATCH_EXPIRY\n"));
			ret = wl_cfgvendor_nan_match_expiry_event_filler(msg, event_data);
			if (unlikely(ret)) {
				WL_ERR(("Failed to fill match expiry event data, ret=%d\n", ret));
				goto fail;
			}
		}

		ret = wl_cfgvendor_nan_opt_params_filler(msg, event_data);
		if (unlikely(ret)) {
			WL_ERR(("Failed to fill sub match event data, ret=%d\n", ret));
			goto fail;
		}
		break;
	}

	case GOOGLE_NAN_EVENT_DISABLED: {
		WL_INFORM_MEM(("[NAN] GOOGLE_NAN_EVENT_DISABLED\n"));
		ret = nla_put_u8(msg, NAN_ATTRIBUTE_HANDLE, 0);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put handle, ret=%d\n", ret));
			goto fail;
		}
		ret = nla_put_u16(msg, NAN_ATTRIBUTE_STATUS, event_data->status);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put status, ret=%d\n", ret));
			goto fail;
		}
		ret = nla_put(msg, NAN_ATTRIBUTE_REASON,
			strlen("NAN_STATUS_SUCCESS"), event_data->nan_reason);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put reason code, ret=%d\n", ret));
			goto fail;
		}
		break;
	}

	case GOOGLE_NAN_EVENT_SUBSCRIBE_TERMINATED:
	case GOOGLE_NAN_EVENT_PUBLISH_TERMINATED: {
		WL_DBG(("GOOGLE_NAN_SVC_TERMINATED, %d\n", event_id));
		ret = wl_cfgvendor_nan_svc_terminate_event_filler(msg, cfg, event_id, event_data);
		if (unlikely(ret)) {
			WL_ERR(("Failed to fill svc terminate event data, ret=%d\n", ret));
			goto fail;
		}
		break;
	}

	case GOOGLE_NAN_EVENT_TRANSMIT_FOLLOWUP_IND: {
		WL_DBG(("GOOGLE_NAN_EVENT_TRANSMIT_FOLLOWUP_IND %d\n",
			GOOGLE_NAN_EVENT_TRANSMIT_FOLLOWUP_IND));
		ret = wl_cfgvendor_nan_tx_followup_ind_event_data_filler(msg, event_data);
		if (unlikely(ret)) {
			WL_ERR(("Failed to fill tx follow up ind event data, ret=%d\n", ret));
			goto fail;
		}

		break;
	}

	case GOOGLE_NAN_EVENT_DATA_REQUEST: {
		WL_INFORM_MEM(("[NAN] GOOGLE_NAN_EVENT_DATA_REQUEST\n"));
		ret = wl_cfgvendor_nan_dp_ind_event_data_filler(msg, event_data);
		if (unlikely(ret)) {
			WL_ERR(("Failed to fill dp ind event data, ret=%d\n", ret));
			goto fail;
		}
		break;
	}

	case GOOGLE_NAN_EVENT_DATA_CONFIRMATION: {
		WL_INFORM_MEM(("[NAN] GOOGLE_NAN_EVENT_DATA_CONFIRMATION\n"));

		ret = wl_cfgvendor_nan_dp_estb_event_data_filler(msg, event_data);
		if (unlikely(ret)) {
			WL_ERR(("Failed to fill dp estb event data, ret=%d\n", ret));
			goto fail;
		}
		break;
	}

	case GOOGLE_NAN_EVENT_DATA_END: {
		WL_INFORM_MEM(("[NAN] GOOGLE_NAN_EVENT_DATA_END\n"));
		ret = nla_put_u8(msg, NAN_ATTRIBUTE_INST_COUNT, 1);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put inst count, ret=%d\n", ret));
			goto fail;
		}
		ret = nla_put_u32(msg, NAN_ATTRIBUTE_NDP_ID, event_data->ndp_id);
		if (unlikely(ret)) {
			WL_ERR(("Failed to put ndp id, ret=%d\n", ret));
			goto fail;
		}
		break;
	}

	default:
		goto fail;
	}

	cfg80211_vendor_event(msg, kflags);
	NAN_DBG_EXIT();
	return ret;

fail:
	dev_kfree_skb_any(msg);
	WL_ERR(("Event not implemented or unknown -- Free skb, event_id = %d, ret = %d\n",
			event_id, ret));
	NAN_DBG_EXIT();
	return ret;
}

static int
wl_cfgvendor_nan_req_subscribe(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void * data, int len)
{
	int ret = 0;
	nan_discover_cmd_data_t *cmd_data = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	nan_hal_resp_t nan_req_resp;

	NAN_DBG_ENTER();
	/* Blocking Subscribe if NAN is not enable */
	if (!cfg->nancfg->nan_enable) {
		WL_ERR(("nan is not enabled, subscribe blocked\n"));
		ret = BCME_ERROR;
		goto exit;
	}
	cmd_data = (nan_discover_cmd_data_t *)MALLOCZ(cfg->osh, sizeof(*cmd_data));
	if (!cmd_data) {
		WL_ERR(("%s: memory allocation failed\n", __func__));
		ret = BCME_NOMEM;
		goto exit;
	}

	bzero(&nan_req_resp, sizeof(nan_req_resp));
	ret = wl_cfgvendor_nan_parse_discover_args(wiphy, data, len, cmd_data);
	if (ret) {
		WL_ERR(("failed to parse nan disc vendor args, ret = %d\n", ret));
		goto exit;
	}

	if (cmd_data->sub_id == 0) {
		ret = wl_cfgnan_generate_inst_id(cfg, &cmd_data->sub_id);
		if (ret) {
			WL_ERR(("failed to generate instance-id for subscribe\n"));
			goto exit;
		}
	} else {
		cmd_data->svc_update = true;
	}

	ret = wl_cfgnan_subscribe_handler(wdev->netdev, cfg, cmd_data);
	if (unlikely(ret) || unlikely(cmd_data->status)) {
		WL_ERR(("failed to subscribe error[%d], status = [%d]\n",
				ret, cmd_data->status));
		wl_cfgnan_remove_inst_id(cfg, cmd_data->sub_id);
		goto exit;
	}

	WL_DBG(("subscriber instance id=%d\n", cmd_data->sub_id));

	if (cmd_data->status == WL_NAN_E_OK) {
		nan_req_resp.instance_id = cmd_data->sub_id;
	} else {
		nan_req_resp.instance_id = 0;
	}
exit:
	ret = wl_cfgvendor_nan_cmd_reply(wiphy, NAN_WIFI_SUBCMD_REQUEST_SUBSCRIBE,
		&nan_req_resp, ret, cmd_data ? cmd_data->status : BCME_OK);
	wl_cfgvendor_free_disc_cmd_data(cfg, cmd_data);
	NAN_DBG_EXIT();
	return ret;
}

static int
wl_cfgvendor_nan_req_publish(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void * data, int len)
{
	int ret = 0;
	nan_discover_cmd_data_t *cmd_data = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	nan_hal_resp_t nan_req_resp;
	NAN_DBG_ENTER();

	/* Blocking Publish if NAN is not enable */
	if (!cfg->nancfg->nan_enable) {
		WL_ERR(("nan is not enabled publish blocked\n"));
		ret = BCME_ERROR;
		goto exit;
	}
	cmd_data = (nan_discover_cmd_data_t *)MALLOCZ(cfg->osh, sizeof(*cmd_data));
	if (!cmd_data) {
		WL_ERR(("%s: memory allocation failed\n", __func__));
		ret = BCME_NOMEM;
		goto exit;
	}

	bzero(&nan_req_resp, sizeof(nan_req_resp));
	ret = wl_cfgvendor_nan_parse_discover_args(wiphy, data, len, cmd_data);
	if (ret) {
		WL_ERR(("failed to parse nan disc vendor args, ret = %d\n", ret));
		goto exit;
	}

	if (cmd_data->pub_id == 0) {
		ret = wl_cfgnan_generate_inst_id(cfg, &cmd_data->pub_id);
		if (ret) {
			WL_ERR(("failed to generate instance-id for publisher\n"));
			goto exit;
		}
	} else {
		cmd_data->svc_update = true;
	}

	ret = wl_cfgnan_publish_handler(wdev->netdev, cfg, cmd_data);
	if (unlikely(ret) || unlikely(cmd_data->status)) {
		WL_ERR(("failed to publish error[%d], status[%d]\n",
				ret, cmd_data->status));
		wl_cfgnan_remove_inst_id(cfg, cmd_data->pub_id);
		goto exit;
	}

	WL_DBG(("publisher instance id=%d\n", cmd_data->pub_id));

	if (cmd_data->status == WL_NAN_E_OK) {
		nan_req_resp.instance_id = cmd_data->pub_id;
	} else {
		nan_req_resp.instance_id = 0;
	}
exit:
	ret = wl_cfgvendor_nan_cmd_reply(wiphy, NAN_WIFI_SUBCMD_REQUEST_PUBLISH,
		&nan_req_resp, ret, cmd_data ? cmd_data->status : BCME_OK);
	wl_cfgvendor_free_disc_cmd_data(cfg, cmd_data);
	NAN_DBG_EXIT();
	return ret;
}

static int
wl_cfgvendor_nan_start_handler(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int ret = 0;
	nan_config_cmd_data_t *cmd_data;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	nan_hal_resp_t nan_req_resp;
	uint32 nan_attr_mask = 0;

	cmd_data = (nan_config_cmd_data_t *)MALLOCZ(cfg->osh, sizeof(*cmd_data));
	if (!cmd_data) {
		WL_ERR(("%s: memory allocation failed\n", __func__));
		ret = BCME_NOMEM;
		goto exit;
	}
	NAN_DBG_ENTER();

	ret = wl_cfgnan_check_nan_disable_pending(cfg, false, true);
	if (ret != BCME_OK) {
		WL_ERR(("failed to disable nan, error[%d]\n", ret));
		goto exit;
	}

	if (cfg->nancfg->nan_enable) {
		WL_ERR(("nan is already enabled\n"));
		ret = BCME_OK;
		goto exit;
	}
	bzero(&nan_req_resp, sizeof(nan_req_resp));

	cmd_data->sid_beacon.sid_enable = NAN_SID_ENABLE_FLAG_INVALID; /* Setting to some default */
	cmd_data->sid_beacon.sid_count = NAN_SID_BEACON_COUNT_INVALID; /* Setting to some default */

	ret = wl_cfgvendor_nan_parse_args(wiphy, data, len, cmd_data, &nan_attr_mask);
	if (ret) {
		WL_ERR(("failed to parse nan vendor args, ret %d\n", ret));
		goto exit;
	}
	if (cmd_data->status == BCME_BADARG) {
		WL_ERR(("nan vendor args is invalid\n"));
		goto exit;
	}

	ret = wl_cfgnan_start_handler(wdev->netdev, cfg, cmd_data, nan_attr_mask);
	if (ret) {
		WL_ERR(("failed to start nan error[%d]\n", ret));
		goto exit;
	}
	/* Initializing Instance Id List */
	bzero(cfg->nancfg->nan_inst_ctrl, NAN_ID_CTRL_SIZE * sizeof(nan_svc_inst_t));
exit:
	ret = wl_cfgvendor_nan_cmd_reply(wiphy, NAN_WIFI_SUBCMD_ENABLE,
		&nan_req_resp, ret, cmd_data ? cmd_data->status : BCME_OK);
	if (cmd_data) {
		if (cmd_data->scid.data) {
			MFREE(cfg->osh, cmd_data->scid.data, cmd_data->scid.dlen);
			cmd_data->scid.dlen = 0;
		}
		MFREE(cfg->osh, cmd_data, sizeof(*cmd_data));
	}
	NAN_DBG_EXIT();
	return ret;
}

static int
wl_cfgvendor_terminate_dp_rng_sessions(struct bcm_cfg80211 *cfg,
	struct wireless_dev *wdev, bool *ssn_exists)
{
	int ret = 0;
	uint8 i = 0;
	int status = BCME_ERROR;
	wl_nancfg_t *nancfg = cfg->nancfg;
	dhd_pub_t *dhdp;
#ifdef RTT_SUPPORT
	nan_ranging_inst_t *ranging_inst = NULL;
#endif /* RTT_SUPPORT */

	*ssn_exists = false;
	dhdp = wl_cfg80211_get_dhdp(wdev->netdev);
	/* Cleanup active Data Paths If any */
	for (i = 0; i < NAN_MAX_NDP_PEER; i++) {
		if (nancfg->ndp_id[i]) {
			WL_DBG(("Found entry of ndp id = [%d], end dp associated to it\n",
					nancfg->ndp_id[i]));
			ret = wl_cfgnan_data_path_end_handler(wdev->netdev, cfg,
					nancfg->ndp_id[i], &status);
			if ((ret == BCME_OK) && cfg->nancfg->nan_enable &&
				dhdp->up) {
				*ssn_exists = true;
			}
		}
	}

#ifdef RTT_SUPPORT
	/* Cancel ranging sessiosns */
	for (i = 0; i < NAN_MAX_RANGING_INST; i++) {
		ranging_inst = &nancfg->nan_ranging_info[i];
		if (ranging_inst->in_use &&
				(NAN_RANGING_IS_IN_PROG(ranging_inst->range_status))) {
			ret = wl_cfgnan_cancel_ranging(bcmcfg_to_prmry_ndev(cfg), cfg,
					&ranging_inst->range_id,
					NAN_RNG_TERM_FLAG_IMMEDIATE, &status);
			if (unlikely(ret) || unlikely(status)) {
				WL_ERR(("nan range cancel failed ret = %d status = %d\n",
					ret, status));
			} else {
				*ssn_exists = true;
			}
		}
	}
#endif /* RTT_SUPPORT */
	return ret;
}

static int
wl_cfgvendor_nan_stop_handler(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void * data, int len)
{
	int ret = BCME_OK;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	bool ssn_exists = false;
	uint32 delay_ms = 0;
	wl_nancfg_t *nancfg = cfg->nancfg;

	NAN_DBG_ENTER();
	mutex_lock(&cfg->if_sync);

	if (nancfg->nan_init_state == false) {
		WL_INFORM_MEM(("nan is not initialized/nmi doesnt exists\n"));
		goto exit;
	}
	if (nancfg->nan_enable == false) {
		WL_INFORM_MEM(("nan is in disabled state\n"));
	} else {
		nancfg->notify_user = true;
		wl_cfgvendor_terminate_dp_rng_sessions(cfg, wdev, &ssn_exists);
		if (ssn_exists == true) {
			/*
			* Schedule nan disable with NAN_DISABLE_CMD_DELAY
			* delay to make sure
			* fw cleans any active Data paths and
			* notifies the peer about the dp session terminations
			*/
			WL_INFORM_MEM(("Schedule Nan Disable Req with NAN_DISABLE_CMD_DELAY\n"));
			delay_ms = NAN_DISABLE_CMD_DELAY;
			DHD_NAN_WAKE_LOCK_TIMEOUT(cfg->pub, NAN_WAKELOCK_TIMEOUT);
		} else {
			delay_ms = 0;
		}
		schedule_delayed_work(&nancfg->nan_disable,
			msecs_to_jiffies(delay_ms));
	}
exit:
	mutex_unlock(&cfg->if_sync);
	NAN_DBG_EXIT();
	return ret;
}

static int
wl_cfgvendor_nan_config_handler(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int ret = 0;
	nan_config_cmd_data_t *cmd_data;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	nan_hal_resp_t nan_req_resp;
	uint32 nan_attr_mask = 0;

	cmd_data = MALLOCZ(cfg->osh, sizeof(*cmd_data));
	if (!cmd_data) {
		WL_ERR(("%s: memory allocation failed\n", __func__));
		ret = BCME_NOMEM;
		goto exit;
	}
	NAN_DBG_ENTER();

	bzero(&nan_req_resp, sizeof(nan_req_resp));

	cmd_data->avail_params.duration = NAN_BAND_INVALID;  /* Setting to some default */
	cmd_data->sid_beacon.sid_enable = NAN_SID_ENABLE_FLAG_INVALID; /* Setting to some default */
	cmd_data->sid_beacon.sid_count = NAN_SID_BEACON_COUNT_INVALID; /* Setting to some default */

	ret = wl_cfgvendor_nan_parse_args(wiphy, data, len, cmd_data, &nan_attr_mask);
	if (ret) {
		WL_ERR(("failed to parse nan vendor args, ret = %d\n", ret));
		goto exit;
	}
	if (cmd_data->status == BCME_BADARG) {
		WL_ERR(("nan vendor args is invalid\n"));
		goto exit;
	}

	ret = wl_cfgnan_config_handler(wdev->netdev, cfg, cmd_data, nan_attr_mask);
	if (ret) {
		WL_ERR(("failed in config request, nan error[%d]\n", ret));
		goto exit;
	}
exit:
	ret = wl_cfgvendor_nan_cmd_reply(wiphy, NAN_WIFI_SUBCMD_CONFIG,
		&nan_req_resp, ret, cmd_data ? cmd_data->status : BCME_OK);
	if (cmd_data) {
		if (cmd_data->scid.data) {
			MFREE(cfg->osh, cmd_data->scid.data, cmd_data->scid.dlen);
			cmd_data->scid.dlen = 0;
		}
		MFREE(cfg->osh, cmd_data, sizeof(*cmd_data));
	}
	NAN_DBG_EXIT();
	return ret;
}

static int
wl_cfgvendor_nan_cancel_publish(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void * data, int len)
{
	int ret = 0;
	nan_discover_cmd_data_t *cmd_data = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	nan_hal_resp_t nan_req_resp;

	/* Blocking Cancel_Publish if NAN is not enable */
	if (!cfg->nancfg->nan_enable) {
		WL_ERR(("nan is not enabled, cancel publish blocked\n"));
		ret = BCME_ERROR;
		goto exit;
	}
	cmd_data = (nan_discover_cmd_data_t *)MALLOCZ(cfg->osh, sizeof(*cmd_data));
	if (!cmd_data) {
		WL_ERR(("%s: memory allocation failed\n", __func__));
		ret = BCME_NOMEM;
		goto exit;
	}
	NAN_DBG_ENTER();

	bzero(&nan_req_resp, sizeof(nan_req_resp));

	ret = wl_cfgvendor_nan_parse_discover_args(wiphy, data, len, cmd_data);
	if (ret) {
		WL_ERR(("failed to parse nan disc vendor args, ret= %d\n", ret));
		goto exit;
	}
	nan_req_resp.instance_id = cmd_data->pub_id;
	WL_INFORM_MEM(("[NAN] cancel publish instance_id=%d\n", cmd_data->pub_id));

	ret = wl_cfgnan_cancel_pub_handler(wdev->netdev, cfg, cmd_data);
	if (ret) {
		WL_ERR(("failed to cancel publish nan instance-id[%d] error[%d]\n",
			cmd_data->pub_id, ret));
		goto exit;
	}
exit:
	ret = wl_cfgvendor_nan_cmd_reply(wiphy, NAN_WIFI_SUBCMD_CANCEL_PUBLISH,
		&nan_req_resp, ret, cmd_data ? cmd_data->status : BCME_OK);
	wl_cfgvendor_free_disc_cmd_data(cfg, cmd_data);
	NAN_DBG_EXIT();
	return ret;
}

static int
wl_cfgvendor_nan_cancel_subscribe(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void * data, int len)
{
	int ret = 0;
	nan_discover_cmd_data_t *cmd_data = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	nan_hal_resp_t nan_req_resp;

	/* Blocking Cancel_Subscribe if NAN is not enableb */
	if (!cfg->nancfg->nan_enable) {
		WL_ERR(("nan is not enabled, cancel subscribe blocked\n"));
		ret = BCME_ERROR;
		goto exit;
	}
	cmd_data = MALLOCZ(cfg->osh, sizeof(*cmd_data));
	if (!cmd_data) {
		WL_ERR(("%s: memory allocation failed\n", __func__));
		ret = BCME_NOMEM;
		goto exit;
	}
	NAN_DBG_ENTER();

	bzero(&nan_req_resp, sizeof(nan_req_resp));

	ret = wl_cfgvendor_nan_parse_discover_args(wiphy, data, len, cmd_data);
	if (ret) {
		WL_ERR(("failed to parse nan disc vendor args, ret= %d\n", ret));
		goto exit;
	}
	nan_req_resp.instance_id = cmd_data->sub_id;
	WL_INFORM_MEM(("[NAN] cancel subscribe instance_id=%d\n", cmd_data->sub_id));

	ret = wl_cfgnan_cancel_sub_handler(wdev->netdev, cfg, cmd_data);
	if (ret) {
		WL_ERR(("failed to cancel subscribe nan instance-id[%d] error[%d]\n",
			cmd_data->sub_id, ret));
		goto exit;
	}
exit:
	ret = wl_cfgvendor_nan_cmd_reply(wiphy, NAN_WIFI_SUBCMD_CANCEL_SUBSCRIBE,
		&nan_req_resp, ret, cmd_data ? cmd_data->status : BCME_OK);
	wl_cfgvendor_free_disc_cmd_data(cfg, cmd_data);
	NAN_DBG_EXIT();
	return ret;
}

static int
wl_cfgvendor_nan_transmit(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void * data, int len)
{
	int ret = 0;
	nan_discover_cmd_data_t *cmd_data = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	nan_hal_resp_t nan_req_resp;

	/* Blocking Transmit if NAN is not enable */
	if (!cfg->nancfg->nan_enable) {
		WL_ERR(("nan is not enabled, transmit blocked\n"));
		ret = BCME_ERROR;
		goto exit;
	}
	cmd_data = (nan_discover_cmd_data_t *)MALLOCZ(cfg->osh, sizeof(*cmd_data));
	if (!cmd_data) {
		WL_ERR(("%s: memory allocation failed\n", __func__));
		ret = BCME_NOMEM;
		goto exit;
	}
	NAN_DBG_ENTER();

	bzero(&nan_req_resp, sizeof(nan_req_resp));

	ret = wl_cfgvendor_nan_parse_discover_args(wiphy, data, len, cmd_data);
	if (ret) {
		WL_ERR(("failed to parse nan disc vendor args, ret= %d\n", ret));
		goto exit;
	}
	nan_req_resp.instance_id = cmd_data->local_id;
	ret = wl_cfgnan_transmit_handler(wdev->netdev, cfg, cmd_data);
	if (ret) {
		WL_ERR(("failed to transmit-followup nan error[%d]\n", ret));
		goto exit;
	}
exit:
	ret = wl_cfgvendor_nan_cmd_reply(wiphy, NAN_WIFI_SUBCMD_TRANSMIT,
		&nan_req_resp, ret, cmd_data ? cmd_data->status : BCME_OK);
	wl_cfgvendor_free_disc_cmd_data(cfg, cmd_data);
	NAN_DBG_EXIT();
	return ret;
}

static int
wl_cfgvendor_nan_get_capablities(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void * data, int len)
{
	int ret = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	nan_hal_resp_t nan_req_resp;

	NAN_DBG_ENTER();

	bzero(&nan_req_resp, sizeof(nan_req_resp));
	ret = wl_cfgnan_get_capablities_handler(wdev->netdev, cfg, &nan_req_resp.capabilities);
	if (ret) {
		WL_ERR(("Could not get capabilities\n"));
		ret = -EINVAL;
		goto exit;
	}
exit:
	ret = wl_cfgvendor_nan_cmd_reply(wiphy, NAN_WIFI_SUBCMD_GET_CAPABILITIES,
		&nan_req_resp, ret, BCME_OK);
	NAN_DBG_EXIT();
	return ret;
}

static int
wl_cfgvendor_nan_data_path_iface_create(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void * data, int len)
{
	int ret = 0;
	nan_datapath_cmd_data_t *cmd_data = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	nan_hal_resp_t nan_req_resp;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(wdev->netdev);

	if (!cfg->nancfg->nan_init_state) {
		WL_ERR(("%s: NAN is not inited or Device doesn't support NAN \n", __func__));
		ret = -ENODEV;
		goto exit;
	}

	cmd_data = (nan_datapath_cmd_data_t *)MALLOCZ(cfg->osh, sizeof(*cmd_data));
	if (!cmd_data) {
		WL_ERR(("%s: memory allocation failed\n", __func__));
		ret = BCME_NOMEM;
		goto exit;
	}
	NAN_DBG_ENTER();

	bzero(&nan_req_resp, sizeof(nan_req_resp));

	ret = wl_cfgvendor_nan_parse_datapath_args(wiphy, data, len, cmd_data);
	if (ret) {
		WL_ERR(("failed to parse nan datapath vendor args, ret = %d\n", ret));
		goto exit;
	}

	if (cfg->nancfg->nan_enable) { /* new framework Impl, iface create called after nan enab */
		ret = wl_cfgnan_data_path_iface_create_delete_handler(wdev->netdev,
			cfg, cmd_data->ndp_iface,
			NAN_WIFI_SUBCMD_DATA_PATH_IFACE_CREATE, dhdp->up);
		if (ret != BCME_OK) {
			WL_ERR(("failed to create iface, ret = %d\n", ret));
			goto exit;
		}
	}
exit:
	ret = wl_cfgvendor_nan_cmd_reply(wiphy, NAN_WIFI_SUBCMD_DATA_PATH_IFACE_CREATE,
		&nan_req_resp, ret, cmd_data ? cmd_data->status : BCME_OK);
	wl_cfgvendor_free_dp_cmd_data(cfg, cmd_data);
	NAN_DBG_EXIT();
	return ret;
}

static int
wl_cfgvendor_nan_data_path_iface_delete(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void * data, int len)
{
	int ret = 0;
	nan_datapath_cmd_data_t *cmd_data = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	nan_hal_resp_t nan_req_resp;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(wdev->netdev);

	if (cfg->nancfg->nan_init_state == false) {
		WL_ERR(("%s: NAN is not inited or Device doesn't support NAN \n", __func__));
		/* Deinit has taken care of cleaing the virtual iface */
		ret = BCME_OK;
		goto exit;
	}

	NAN_DBG_ENTER();
	cmd_data = (nan_datapath_cmd_data_t *)MALLOCZ(cfg->osh, sizeof(*cmd_data));
	if (!cmd_data) {
		WL_ERR(("%s: memory allocation failed\n", __func__));
		ret = BCME_NOMEM;
		goto exit;
	}
	bzero(&nan_req_resp, sizeof(nan_req_resp));
	ret = wl_cfgvendor_nan_parse_datapath_args(wiphy, data, len, cmd_data);
	if (ret) {
		WL_ERR(("failed to parse nan datapath vendor args, ret = %d\n", ret));
		goto exit;
	}

	ret = wl_cfgnan_data_path_iface_create_delete_handler(wdev->netdev, cfg,
		(char*)cmd_data->ndp_iface,
		NAN_WIFI_SUBCMD_DATA_PATH_IFACE_DELETE, dhdp->up);
	if (ret) {
		WL_ERR(("failed to delete ndp iface [%d]\n", ret));
		goto exit;
	}
exit:
	ret = wl_cfgvendor_nan_cmd_reply(wiphy, NAN_WIFI_SUBCMD_DATA_PATH_IFACE_DELETE,
		&nan_req_resp, ret, cmd_data ? cmd_data->status : BCME_OK);
	wl_cfgvendor_free_dp_cmd_data(cfg, cmd_data);
	NAN_DBG_EXIT();
	return ret;
}

static int
wl_cfgvendor_nan_data_path_request(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void * data, int len)
{
	int ret = 0;
	nan_datapath_cmd_data_t *cmd_data = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	nan_hal_resp_t nan_req_resp;
	uint8 ndp_instance_id = 0;

	if (!cfg->nancfg->nan_enable) {
		WL_ERR(("nan is not enabled, nan data path request blocked\n"));
		ret = BCME_ERROR;
		goto exit;
	}

	NAN_DBG_ENTER();
	cmd_data = (nan_datapath_cmd_data_t *)MALLOCZ(cfg->osh, sizeof(*cmd_data));
	if (!cmd_data) {
		WL_ERR(("%s: memory allocation failed\n", __func__));
		ret = BCME_NOMEM;
		goto exit;
	}

	bzero(&nan_req_resp, sizeof(nan_req_resp));
	ret = wl_cfgvendor_nan_parse_datapath_args(wiphy, data, len, cmd_data);
	if (ret) {
		WL_ERR(("failed to parse nan datapath vendor args, ret = %d\n", ret));
		goto exit;
	}

	ret = wl_cfgnan_data_path_request_handler(wdev->netdev, cfg,
			cmd_data, &ndp_instance_id);
	if (ret) {
		WL_ERR(("failed to request nan data path [%d]\n", ret));
		goto exit;
	}

	if (cmd_data->status == BCME_OK) {
		nan_req_resp.ndp_instance_id = cmd_data->ndp_instance_id;
	} else {
		nan_req_resp.ndp_instance_id = 0;
	}
exit:
	ret = wl_cfgvendor_nan_cmd_reply(wiphy, NAN_WIFI_SUBCMD_DATA_PATH_REQUEST,
		&nan_req_resp, ret, cmd_data ? cmd_data->status : BCME_OK);
	wl_cfgvendor_free_dp_cmd_data(cfg, cmd_data);
	NAN_DBG_EXIT();
	return ret;
}

static int
wl_cfgvendor_nan_data_path_response(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void * data, int len)
{
	int ret = 0;
	nan_datapath_cmd_data_t *cmd_data = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	nan_hal_resp_t nan_req_resp;

	if (!cfg->nancfg->nan_enable) {
		WL_ERR(("nan is not enabled, nan data path response blocked\n"));
		ret = BCME_ERROR;
		goto exit;
	}
	NAN_DBG_ENTER();
	cmd_data = (nan_datapath_cmd_data_t *)MALLOCZ(cfg->osh, sizeof(*cmd_data));
	if (!cmd_data) {
		WL_ERR(("%s: memory allocation failed\n", __func__));
		ret = BCME_NOMEM;
		goto exit;
	}

	bzero(&nan_req_resp, sizeof(nan_req_resp));
	ret = wl_cfgvendor_nan_parse_datapath_args(wiphy, data, len, cmd_data);
	if (ret) {
		WL_ERR(("failed to parse nan datapath vendor args, ret = %d\n", ret));
		goto exit;
	}
	ret = wl_cfgnan_data_path_response_handler(wdev->netdev, cfg, cmd_data);
	if (ret) {
		WL_ERR(("failed to response nan data path [%d]\n", ret));
		goto exit;
	}
exit:
	ret = wl_cfgvendor_nan_cmd_reply(wiphy, NAN_WIFI_SUBCMD_DATA_PATH_RESPONSE,
		&nan_req_resp, ret, cmd_data ? cmd_data->status : BCME_OK);
	wl_cfgvendor_free_dp_cmd_data(cfg, cmd_data);
	NAN_DBG_EXIT();
	return ret;
}

static int
wl_cfgvendor_nan_data_path_end(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void * data, int len)
{
	int ret = 0;
	nan_datapath_cmd_data_t *cmd_data = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	nan_hal_resp_t nan_req_resp;
	int status = BCME_ERROR;

	NAN_DBG_ENTER();
	if (!cfg->nancfg->nan_enable) {
		WL_ERR(("nan is not enabled, nan data path end blocked\n"));
		ret = BCME_OK;
		goto exit;
	}
	cmd_data = (nan_datapath_cmd_data_t *)MALLOCZ(cfg->osh, sizeof(*cmd_data));
	if (!cmd_data) {
		WL_ERR(("%s: memory allocation failed\n", __func__));
		ret = BCME_NOMEM;
		goto exit;
	}

	bzero(&nan_req_resp, sizeof(nan_req_resp));
	ret = wl_cfgvendor_nan_parse_datapath_args(wiphy, data, len, cmd_data);
	if (ret) {
		WL_ERR(("failed to parse nan datapath vendor args, ret = %d\n", ret));
		goto exit;
	}
	ret = wl_cfgnan_data_path_end_handler(wdev->netdev, cfg,
			cmd_data->ndp_instance_id, &status);
	if (ret) {
		WL_ERR(("failed to end nan data path [%d]\n", ret));
		goto exit;
	}
exit:
	ret = wl_cfgvendor_nan_cmd_reply(wiphy, NAN_WIFI_SUBCMD_DATA_PATH_END,
		&nan_req_resp, ret, cmd_data ? status : BCME_OK);
	wl_cfgvendor_free_dp_cmd_data(cfg, cmd_data);
	NAN_DBG_EXIT();
	return ret;
}

#ifdef WL_NAN_DISC_CACHE
static int
wl_cfgvendor_nan_data_path_sec_info(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int len)
{
	int ret = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	nan_hal_resp_t nan_req_resp;
	nan_datapath_sec_info_cmd_data_t *cmd_data = NULL;
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(wdev->netdev);

	NAN_DBG_ENTER();
	if (!cfg->nancfg->nan_enable) {
		WL_ERR(("nan is not enabled\n"));
		ret = BCME_UNSUPPORTED;
		goto exit;
	}
	cmd_data = MALLOCZ(dhdp->osh, sizeof(*cmd_data));
	if (!cmd_data) {
		WL_ERR(("%s: memory allocation failed\n", __func__));
		ret = BCME_NOMEM;
		goto exit;
	}

	ret = wl_cfgvendor_nan_parse_dp_sec_info_args(wiphy, data, len, cmd_data);
	if (ret) {
		WL_ERR(("failed to parse sec info args\n"));
		goto exit;
	}

	bzero(&nan_req_resp, sizeof(nan_req_resp));
	ret = wl_cfgnan_sec_info_handler(cfg, cmd_data, &nan_req_resp);
	if (ret) {
		WL_ERR(("failed to retrieve svc hash/pub nmi error[%d]\n", ret));
		goto exit;
	}
exit:
	ret = wl_cfgvendor_nan_cmd_reply(wiphy, NAN_WIFI_SUBCMD_DATA_PATH_SEC_INFO,
		&nan_req_resp, ret, BCME_OK);
	if (cmd_data) {
		MFREE(dhdp->osh, cmd_data, sizeof(*cmd_data));
	}
	NAN_DBG_EXIT();
	return ret;
}
#endif /* WL_NAN_DISC_CACHE */

static int
wl_cfgvendor_nan_version_info(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int len)
{
	int ret = BCME_OK;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	uint32 version = NAN_HAL_VERSION_1;

	BCM_REFERENCE(cfg);
	WL_DBG(("Enter %s version %d\n", __FUNCTION__, version));
	ret = wl_cfgvendor_send_cmd_reply(wiphy, &version, sizeof(version));
	return ret;
}

static int
wl_cfgvendor_nan_enable_merge(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void * data, int len)
{
	int ret = 0;
	nan_config_cmd_data_t *cmd_data = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int status = BCME_OK;
	uint32 nan_attr_mask = 0;

	BCM_REFERENCE(nan_attr_mask);
	NAN_DBG_ENTER();
	cmd_data = (nan_config_cmd_data_t *)MALLOCZ(cfg->osh, sizeof(*cmd_data));
	if (!cmd_data) {
		WL_ERR(("%s: memory allocation failed\n", __func__));
		ret = BCME_NOMEM;
		goto exit;
	}

	ret = wl_cfgvendor_nan_parse_args(wiphy, data, len, cmd_data, &nan_attr_mask);
	if (ret) {
		WL_ERR((" Enable merge: failed to parse nan config vendor args, ret = %d\n", ret));
		goto exit;
	}
	ret = wl_cfgnan_set_enable_merge(wdev->netdev, cfg, cmd_data->enable_merge, &status);
	if (unlikely(ret) || unlikely(status)) {
		WL_ERR(("Enable merge: failed to set config request  [%d]\n", ret));
		/* As there is no cmd_reply, return status if error is in status else return ret */
		if (status) {
			ret = status;
		}
		goto exit;
	}
exit:
	if (cmd_data) {
		if (cmd_data->scid.data) {
			MFREE(cfg->osh, cmd_data->scid.data, cmd_data->scid.dlen);
			cmd_data->scid.dlen = 0;
		}
		MFREE(cfg->osh, cmd_data, sizeof(*cmd_data));
	}
	NAN_DBG_EXIT();
	return ret;
}
#endif /* WL_NAN */

#ifdef LINKSTAT_SUPPORT

#define NUM_RATE 32
#define NUM_PEER 1
#define NUM_CHAN 11
#define HEADER_SIZE sizeof(ver_len)

#define NUM_PNO_SCANS 8
#define NUM_CCA_SAMPLING_SECS 1

static int wl_cfgvendor_lstats_get_bcn_mbss(char *buf, uint32 *rxbeaconmbss)
{
	wl_cnt_info_t *cbuf = (wl_cnt_info_t *)buf;
	const void *cnt;

	if ((cnt = (const void *)bcm_get_data_from_xtlv_buf(cbuf->data, cbuf->datalen,
			WL_CNT_XTLV_CNTV_LE10_UCODE, NULL, BCM_XTLV_OPTION_ALIGN32)) != NULL) {
		*rxbeaconmbss = ((const wl_cnt_v_le10_mcst_t *)cnt)->rxbeaconmbss;
	} else if ((cnt = (const void *)bcm_get_data_from_xtlv_buf(cbuf->data, cbuf->datalen,
			WL_CNT_XTLV_LT40_UCODE_V1, NULL, BCM_XTLV_OPTION_ALIGN32)) != NULL) {
		*rxbeaconmbss = ((const wl_cnt_lt40mcst_v1_t *)cnt)->rxbeaconmbss;
	} else if ((cnt = (const void *)bcm_get_data_from_xtlv_buf(cbuf->data, cbuf->datalen,
			WL_CNT_XTLV_GE40_UCODE_V1, NULL, BCM_XTLV_OPTION_ALIGN32)) != NULL) {
		*rxbeaconmbss = ((const wl_cnt_ge40mcst_v1_t *)cnt)->rxbeaconmbss;
	} else if ((cnt = (const void *)bcm_get_data_from_xtlv_buf(cbuf->data, cbuf->datalen,
			WL_CNT_XTLV_GE80_UCODE_V1, NULL, BCM_XTLV_OPTION_ALIGN32)) != NULL) {
		*rxbeaconmbss = ((const wl_cnt_ge80mcst_v1_t *)cnt)->rxbeaconmbss;
	} else {
		*rxbeaconmbss = 0;
		return BCME_NOTFOUND;
	}

	return BCME_OK;
}

static void fill_chanspec_to_channel_info(chanspec_t cur_chanspec,
	wifi_channel_info *channel, int *cur_band)
{
	int band;
	channel->width = WIFI_CHAN_WIDTH_INVALID;

	if (CHSPEC_IS20(cur_chanspec)) {
		channel->width = WIFI_CHAN_WIDTH_20;
	} else if (CHSPEC_IS40(cur_chanspec)) {
		channel->width = WIFI_CHAN_WIDTH_40;
	} else if (CHSPEC_IS80(cur_chanspec)) {
		channel->width = WIFI_CHAN_WIDTH_80;
	} else if (CHSPEC_IS160(cur_chanspec)) {
		channel->width = WIFI_CHAN_WIDTH_160;
	} else if (CHSPEC_IS8080(cur_chanspec)) {
		channel->width = WIFI_CHAN_WIDTH_80P80;
	}

	band = *cur_band = CHSPEC_BAND(cur_chanspec);
	channel->center_freq =
		wl_channel_to_frequency(wf_chspec_primary20_chan(cur_chanspec),
			band);

	if (CHSPEC_IS160(cur_chanspec) || CHSPEC_IS8080(cur_chanspec)) {
		channel->center_freq0 =
			wl_channel_to_frequency(wf_chspec_primary80_channel(cur_chanspec),
				band);
		channel->center_freq1 =
			wl_channel_to_frequency(wf_chspec_secondary80_channel(cur_chanspec),
				band);
	} else {
		channel->center_freq0 =
			wl_channel_to_frequency(CHSPEC_CHANNEL(cur_chanspec),
				band);
		channel->center_freq1 = 0;
	}
}

static int wl_cfgvendor_lstats_get_info(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	static char iovar_buf[WLC_IOCTL_MAXLEN];
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int err = 0, ret = 0, i;
	wifi_radio_stat *radio;
	wifi_radio_stat_h radio_h;
	wifi_channel_stat *chan_stats = NULL;
	uint chan_stats_size = 0;
#ifdef CHAN_STATS_SUPPORT
	wifi_channel_stat *all_chan_stats = NULL;
	cca_congest_ext_channel_req_v2_t *per_chspec_stats = NULL;
	uint per_chspec_stats_size = 0;
	cca_congest_ext_channel_req_v3_t *all_chan_results;
	cca_congest_ext_channel_req_v3_t all_chan_req;
#else
	/* cca_get_stats_ext iovar for Wifi channel statics */
	cca_congest_ext_channel_req_v2_t *cca_v2_results = NULL;
	cca_congest_ext_channel_req_v3_t cca_v3_req;
	cca_congest_ext_channel_req_v2_t cca_v2_req;
	uint16 cca_ver;
#endif /* CHAN_STATS_SUPPORT  */
	const wl_cnt_wlc_t *wlc_cnt;
	scb_val_t scbval;
	char *output = NULL;
	char *outdata = NULL;
	wifi_rate_stat_v1 *p_wifi_rate_stat_v1 = NULL;
	wifi_rate_stat *p_wifi_rate_stat = NULL;
	uint total_len = 0;
	uint32 rxbeaconmbss;
	wlc_rev_info_t revinfo;
	wl_if_stats_t *if_stats = NULL;
	dhd_pub_t *dhdp = (dhd_pub_t *)(cfg->pub);
	wl_pwrstats_query_t scan_query;
	wl_pwrstats_t *pwrstats;
	wl_pwr_scan_stats_t scan_stats;
	int scan_time_len;
	uint32 tot_pno_dur = 0;
	wifi_channel_stat cur_channel_stat;
	cca_congest_channel_req_t *cca_result;
	cca_congest_channel_req_t cca_req;
	uint32 cca_busy_time = 0;
	int cur_chansp, cur_band;
	chanspec_t cur_chanspec;

	COMPAT_STRUCT_IFACE(wifi_iface_stat, iface);

	WL_TRACE(("%s: Enter \n", __func__));
	RETURN_EIO_IF_NOT_UP(cfg);

	BCM_REFERENCE(if_stats);
	BCM_REFERENCE(dhdp);
	/* Get the device rev info */
	bzero(&revinfo, sizeof(revinfo));
	err = wldev_ioctl_get(bcmcfg_to_prmry_ndev(cfg), WLC_GET_REVINFO, &revinfo,
			sizeof(revinfo));
	if (err != BCME_OK) {
		goto exit;
	}

	outdata = (void *)MALLOCZ(cfg->osh, WLC_IOCTL_MAXLEN);
	if (outdata == NULL) {
		WL_ERR(("outdata alloc failed\n"));
		return BCME_NOMEM;
	}

	bzero(&scbval, sizeof(scb_val_t));
	bzero(outdata, WLC_IOCTL_MAXLEN);
	output = outdata;

	err = wldev_iovar_getbuf(bcmcfg_to_prmry_ndev(cfg), "radiostat", NULL, 0,
		iovar_buf, WLC_IOCTL_MAXLEN, NULL);
	if (err != BCME_OK && err != BCME_UNSUPPORTED) {
		WL_ERR(("error (%d) - size = %zu\n", err, sizeof(wifi_radio_stat)));
		goto exit;
	}
	radio = (wifi_radio_stat *)iovar_buf;

	bzero(&radio_h, sizeof(wifi_radio_stat_h));
	radio_h.on_time = radio->on_time;
	radio_h.tx_time = radio->tx_time;
	radio_h.rx_time = radio->rx_time;
	radio_h.on_time_scan = radio->on_time_scan;
	radio_h.on_time_nbd = radio->on_time_nbd;
	radio_h.on_time_gscan = radio->on_time_gscan;
	radio_h.on_time_roam_scan = radio->on_time_roam_scan;
	radio_h.on_time_pno_scan = radio->on_time_pno_scan;
	radio_h.on_time_hs20 = radio->on_time_hs20;
	radio_h.num_channels = NUM_PEER;

	scan_query.length = 1;
	scan_query.type[0] = WL_PWRSTATS_TYPE_SCAN;

	err = wldev_iovar_getbuf(bcmcfg_to_prmry_ndev(cfg), "pwrstats", &scan_query,
		sizeof(scan_query), iovar_buf, WLC_IOCTL_MAXLEN, NULL);
	if (err != BCME_OK && err != BCME_UNSUPPORTED) {
		WL_ERR(("error (%d) - size = %zu\n", err, sizeof(wl_pwrstats_t)));
		goto exit;
	}

	pwrstats = (wl_pwrstats_t *) iovar_buf;

	if (dtoh16(pwrstats->version) != WL_PWRSTATS_VERSION) {
		WL_ERR(("PWRSTATS Version mismatch\n"));
		err = BCME_ERROR;
		goto exit;
	}

	scan_time_len = dtoh16(((uint16 *)pwrstats->data)[1]);

	if (scan_time_len < sizeof(wl_pwr_scan_stats_t)) {
		WL_ERR(("WL_PWRSTATS_TYPE_SCAN IOVAR info short len :  %d < %d\n",
			scan_time_len, (int)sizeof(wl_pwr_scan_stats_t)));
		err = BCME_ERROR;
		goto exit;
	}

	(void) memcpy_s(&scan_stats, sizeof(wl_pwr_scan_stats_t), pwrstats->data, scan_time_len);

	/* wl_pwr_scan_stats structure has the array of pno_scans.
	 * scan_data_t pno_scans[8];
	 * The number of array is 8 : For future PNO bucketing (BSSID, SSID, etc)
	 * FW sets the number as harcoded.
	 * If the hardcoded number (8) is changed,
	 * the loop condition or NUM_PNO_SCANS has to be changed
	 */

	for (i = 0; i < NUM_PNO_SCANS; i++) {
		tot_pno_dur += dtoh32(scan_stats.pno_scans[i].dur);
	}

	/*  Android Framework defines the total scan time in ms.
	 *  But FW sends each scan time in us except for roam scan time.
	 *  So we need to scale the times in ms.
	 */

	radio_h.on_time_scan = (uint32)((tot_pno_dur +
		dtoh32(scan_stats.user_scans.dur) +
		dtoh32(scan_stats.assoc_scans.dur) +
		dtoh32(scan_stats.other_scans.dur)) / 1000);

	radio_h.on_time_scan += dtoh32(scan_stats.roam_scans.dur);
	radio_h.on_time_roam_scan = dtoh32(scan_stats.roam_scans.dur);
	radio_h.on_time_pno_scan = (uint32)(tot_pno_dur / 1000);

	WL_TRACE(("pwr_scan_stats : %u %u %u %u %u %u\n",
		radio_h.on_time_scan,
		dtoh32(scan_stats.user_scans.dur),
		dtoh32(scan_stats.assoc_scans.dur),
		dtoh32(scan_stats.roam_scans.dur),
		tot_pno_dur,
		dtoh32(scan_stats.other_scans.dur)));

	err = wldev_iovar_getint(bcmcfg_to_prmry_ndev(cfg), "chanspec", (int*)&cur_chansp);
	if (err != BCME_OK) {
		WL_ERR(("error (%d) \n", err));
		goto exit;
	}

	cur_chanspec = wl_chspec_driver_to_host(cur_chansp);

	if (!wf_chspec_valid(cur_chanspec)) {
		WL_ERR(("Invalid chanspec : %x\n", cur_chanspec));
		err = BCME_ERROR;
		goto exit;
	}

	fill_chanspec_to_channel_info(cur_chanspec, &cur_channel_stat.channel, &cur_band);
	WL_TRACE(("chanspec : %x, BW : %d, Cur Band : %x, freq : %d, freq0 :%d, freq1 : %d\n",
		cur_chanspec,
		cur_channel_stat.channel.width,
		cur_band,
		cur_channel_stat.channel.center_freq,
		cur_channel_stat.channel.center_freq0,
		cur_channel_stat.channel.center_freq1));

	chan_stats_size = sizeof(wifi_channel_stat);
	chan_stats = &cur_channel_stat;

#ifdef CHAN_STATS_SUPPORT
	/* Option to get all channel statistics */
	all_chan_req.num_of_entries = 0;
	all_chan_req.ver = WL_CCA_EXT_REQ_VER_V3;
	err = wldev_iovar_getbuf(bcmcfg_to_prmry_ndev(cfg), "cca_get_stats_ext",
		&all_chan_req, sizeof(all_chan_req), iovar_buf, WLC_IOCTL_MAXLEN, NULL);

	if (err != BCME_OK && err != BCME_UNSUPPORTED) {
		WL_ERR(("cca_get_stats_ext iovar err = %d\n", err));
		goto exit;
	}

	all_chan_results = (cca_congest_ext_channel_req_v3_t *) iovar_buf;
	if ((err == BCME_OK) &&
			(dtoh16(all_chan_results->ver) == WL_CCA_EXT_REQ_VER_V3)) {
		int i = 0, num_channels;

		num_channels = dtoh16(all_chan_results->num_of_entries);
		radio_h.num_channels = num_channels;

		chan_stats_size = sizeof(wifi_channel_stat) * num_channels;
		chan_stats = (wifi_channel_stat*)MALLOCZ(cfg->osh, chan_stats_size);
		if (chan_stats == NULL) {
			WL_ERR(("chan_stats alloc failed\n"));
			goto exit;
		}
		bzero(chan_stats, chan_stats_size);
		all_chan_stats = chan_stats;

		per_chspec_stats_size =
			sizeof(cca_congest_ext_channel_req_v2_t) * num_channels;
		per_chspec_stats = (cca_congest_ext_channel_req_v2_t *)
			MALLOCZ(cfg->osh, per_chspec_stats_size);
		if (per_chspec_stats == NULL) {
			WL_ERR(("per_chspec_stats alloc failed\n"));
			goto exit;
		}
		(void) memcpy_s(per_chspec_stats, per_chspec_stats_size,
			&all_chan_results->per_chan_stats, per_chspec_stats_size);

		WL_TRACE(("** Per channel CCA entries ** \n"));

		for (i = 0; i < num_channels; i++, all_chan_stats++) {
			if (per_chspec_stats[i].num_secs != 1) {
				WL_ERR(("Bogus num of seconds returned %d\n",
					per_chspec_stats[i].num_secs));
				goto exit;
			}

			fill_chanspec_to_channel_info(per_chspec_stats[i].chanspec,
				&all_chan_stats->channel, &cur_band);

			all_chan_stats->on_time =
				per_chspec_stats[i].secs[0].radio_on_time;
			all_chan_stats->cca_busy_time =
				per_chspec_stats[i].secs[0].cca_busy_time;

			WL_TRACE(("chanspec %x num_sec %d radio_on_time %d cca_busytime %d \n",
				per_chspec_stats[i].chanspec, per_chspec_stats[i].num_secs,
				per_chspec_stats[i].secs[0].radio_on_time,
				per_chspec_stats[i].secs[0].cca_busy_time));
		}
		all_chan_stats = chan_stats;
#else
	cca_v3_req.num_of_entries = 1;
	cca_v3_req.ver = WL_CCA_EXT_REQ_VER_V3;
	cca_v3_req.per_chan_stats->chanspec =
		wl_chspec_host_to_driver(wf_chspec_primary20_chspec(cur_chanspec));

	err = wldev_iovar_getbuf(bcmcfg_to_prmry_ndev(cfg), "cca_get_stats_ext", &cca_v3_req,
		sizeof(cca_v3_req), iovar_buf, WLC_IOCTL_MAXLEN, NULL);

	if (err != BCME_OK && err != BCME_UNSUPPORTED) {
		WL_ERR(("cca_get_stats_ext iovar err = %d\n", err));
		goto exit;
	}

	cca_ver = ((cca_congest_ext_channel_req_v3_t *)iovar_buf)->ver;

	/* Check the verison for cca_get_stats_ext iovar */
	if ((err == BCME_OK) &&
			(dtoh16(cca_ver) == WL_CCA_EXT_REQ_VER_V3)) {

		cca_v2_results =
			((cca_congest_ext_channel_req_v3_t *)iovar_buf)->per_chan_stats;

		/* the accumulated time for the current channel */
		cur_channel_stat.on_time = dtoh32(cca_v2_results->secs[0].radio_on_time);
		cur_channel_stat.cca_busy_time = dtoh32(cca_v2_results->secs[0].cca_busy_time);

		WL_TRACE(("wifi chan statics ver.3 - on_time : %u, cca_busy_time : %u\n",
			cur_channel_stat.on_time, cur_channel_stat.cca_busy_time));
	} else if ((err == BCME_OK) &&
			(dtoh16(cca_ver) == WL_CCA_EXT_REQ_VER_V2)) {

		cca_v2_req.chanspec =
			wl_chspec_host_to_driver(wf_chspec_primary20_chspec(cur_chanspec));

		err = wldev_iovar_getbuf(bcmcfg_to_prmry_ndev(cfg), "cca_get_stats_ext",
			&cca_v2_req, sizeof(cca_v2_req), iovar_buf, WLC_IOCTL_MAXLEN, NULL);

		if (err != BCME_OK) {
			WL_ERR(("cca_get_stats_ext iovar err = %d\n", err));
			goto exit;
		}

		cca_v2_results = (cca_congest_ext_channel_req_v2_t *) iovar_buf;

		/* the accumulated time for the current channel */
		cur_channel_stat.on_time = dtoh32(cca_v2_results->secs[0].radio_on_time);
		cur_channel_stat.cca_busy_time = dtoh32(cca_v2_results->secs[0].cca_busy_time);

		WL_TRACE(("wifi chan statics ver.2 - on_time : %u, cca_busy_time : %u\n",
			cur_channel_stat.on_time, cur_channel_stat.cca_busy_time));
#endif /* CHAN_STATS_SUPPORT  */
	} else {
		/* To get fine-grained cca result,
		* you can increase num_secs because num_secs is the time to get samples.
		* Also if the time is increased,
		* it is necessary to use a loop to add the times of cca_result->sec[].
		* For simplicity, the sampling time is set to 1sec.
		*/
		WL_TRACE(("cca_get_stats_ext unsupported or version mismatch\n"));

		cca_req.num_secs = NUM_CCA_SAMPLING_SECS;
		cca_req.chanspec = wl_chspec_host_to_driver(cur_chanspec);

		err = wldev_iovar_getbuf(bcmcfg_to_prmry_ndev(cfg), "cca_get_stats", &cca_req,
			sizeof(cca_req), iovar_buf, WLC_IOCTL_MAXLEN, NULL);

		if (err != BCME_OK && err != BCME_UNSUPPORTED) {
			WL_ERR(("error (%d) - size = %zu\n",
				err, sizeof(cca_congest_channel_req_t)));
			goto exit;
		}

		cur_channel_stat.on_time = radio_h.on_time;

		if (err == BCME_OK) {
			cca_result = (cca_congest_channel_req_t *) iovar_buf;
			cca_busy_time = dtoh32(cca_result->secs[0].congest_ibss) +
				dtoh32(cca_result->secs[0].congest_obss) +
				dtoh32(cca_result->secs[0].interference);

			WL_TRACE(("wifi stats : %u, %u, %u, %u, %u\n", cur_channel_stat.on_time,
				cca_busy_time,
				dtoh32(cca_result->secs[0].congest_ibss),
				dtoh32(cca_result->secs[0].congest_obss),
				dtoh32(cca_result->secs[0].interference)));
		} else {
			WL_ERR(("cca_get_stats is unsupported \n"));
		}

		/* If cca_get_stats is unsupported, cca_busy_time has zero value as initial value */
		cur_channel_stat.cca_busy_time = cca_busy_time;
	}

	ret = memcpy_s(output, WLC_IOCTL_MAXLEN, &radio_h, sizeof(wifi_radio_stat_h));
	if (ret) {
		WL_ERR(("Failed to copy wifi_radio_stat_h: %d\n", ret));
		goto exit;
	}
	output += sizeof(wifi_radio_stat_h);

	ret = memcpy_s(output, (WLC_IOCTL_MAXLEN - sizeof(wifi_radio_stat_h)),
		chan_stats, chan_stats_size);
	if (ret) {
		WL_ERR(("Failed to copy wifi_channel_stat: %d\n", ret));
		goto exit;
	}
	output += chan_stats_size;

	COMPAT_BZERO_IFACE(wifi_iface_stat, iface);
	COMPAT_ASSIGN_VALUE(iface, ac[WIFI_AC_VO].ac, WIFI_AC_VO);
	COMPAT_ASSIGN_VALUE(iface, ac[WIFI_AC_VI].ac, WIFI_AC_VI);
	COMPAT_ASSIGN_VALUE(iface, ac[WIFI_AC_BE].ac, WIFI_AC_BE);
	COMPAT_ASSIGN_VALUE(iface, ac[WIFI_AC_BK].ac, WIFI_AC_BK);

	err = wldev_iovar_getbuf(bcmcfg_to_prmry_ndev(cfg), "counters", NULL, 0,
		iovar_buf, WLC_IOCTL_MAXLEN, NULL);
	if (unlikely(err)) {
		WL_ERR(("error (%d) - size = %zu\n", err, sizeof(wl_cnt_wlc_t)));
		goto exit;
	}

	CHK_CNTBUF_DATALEN(iovar_buf, WLC_IOCTL_MAXLEN);
	/* Translate traditional (ver <= 10) counters struct to new xtlv type struct */
	/* traditional(ver<=10)counters will use WL_CNT_XTLV_CNTV_LE10_UCODE.
	 * Other cases will use its xtlv type accroding to corerev
	 */
	err = wl_cntbuf_to_xtlv_format(NULL, iovar_buf, WLC_IOCTL_MAXLEN, revinfo.corerev);
	if (err != BCME_OK) {
		WL_ERR(("wl_cntbuf_to_xtlv_format ERR %d\n", err));
		goto exit;
	}

	if (!(wlc_cnt = GET_WLCCNT_FROM_CNTBUF(iovar_buf))) {
		WL_ERR(("wlc_cnt NULL!\n"));
		err = BCME_ERROR;
		goto exit;
	}

#ifndef DISABLE_IF_COUNTERS
	if_stats = (wl_if_stats_t *)MALLOCZ(cfg->osh, sizeof(wl_if_stats_t));
	if (!if_stats) {
	    WL_ERR(("MALLOCZ failed\n"));
	    err = BCME_NOMEM;
	    goto exit;
	}

	if (FW_SUPPORTED(dhdp, ifst)) {
		err = wl_cfg80211_ifstats_counters(bcmcfg_to_prmry_ndev(cfg), if_stats);
	} else {
		err = wldev_iovar_getbuf(bcmcfg_to_prmry_ndev(cfg), "if_counters",
			NULL, 0, (char *)if_stats, sizeof(*if_stats), NULL);
	}

	if (!err) {
		/* Populate from if_stats */
		if (dtoh16(if_stats->version) > WL_IF_STATS_T_VERSION) {
			WL_ERR(("incorrect version of wl_if_stats_t,"
				" expected=%u got=%u\n", WL_IF_STATS_T_VERSION,
				if_stats->version));
			goto exit;
		}
		COMPAT_ASSIGN_VALUE(iface, ac[WIFI_AC_BE].tx_mpdu, (uint32)if_stats->txframe);
		COMPAT_ASSIGN_VALUE(iface, ac[WIFI_AC_BE].rx_mpdu,
			(uint32)(if_stats->rxframe - if_stats->rxmulti));
		COMPAT_ASSIGN_VALUE(iface, ac[WIFI_AC_BE].mpdu_lost,
				(uint32)if_stats->txfail + wlc_cnt->tx_toss_cnt);
		COMPAT_ASSIGN_VALUE(iface, ac[WIFI_AC_BE].retries, (uint32)if_stats->txretrans);
	} else
#endif /* !DISABLE_IF_COUNTERS */
	{
		COMPAT_ASSIGN_VALUE(iface, ac[WIFI_AC_BE].tx_mpdu,
			(wlc_cnt->txfrmsnt - wlc_cnt->txmulti));
		COMPAT_ASSIGN_VALUE(iface, ac[WIFI_AC_BE].rx_mpdu, wlc_cnt->rxframe);
		COMPAT_ASSIGN_VALUE(iface, ac[WIFI_AC_BE].mpdu_lost,
				wlc_cnt->txfail + wlc_cnt->tx_toss_cnt);
		COMPAT_ASSIGN_VALUE(iface, ac[WIFI_AC_BE].retries, wlc_cnt->txretrans);
	}

	err = wl_cfgvendor_lstats_get_bcn_mbss(iovar_buf, &rxbeaconmbss);
	if (unlikely(err)) {
		WL_ERR(("get_bcn_mbss error (%d)\n", err));
		goto exit;
	}

	err = wldev_get_rssi(bcmcfg_to_prmry_ndev(cfg), &scbval);
	if (unlikely(err)) {
		WL_ERR(("get_rssi error (%d)\n", err));
		goto exit;
	}

	COMPAT_ASSIGN_VALUE(iface, beacon_rx, rxbeaconmbss);
	COMPAT_ASSIGN_VALUE(iface, rssi_mgmt, scbval.val);
	COMPAT_ASSIGN_VALUE(iface, num_peers, NUM_PEER);
	COMPAT_ASSIGN_VALUE(iface, peer_info->num_rate, NUM_RATE);

	COMPAT_MEMCOPY_IFACE(output, total_len, wifi_iface_stat, iface, wifi_rate_stat);

	err = wldev_iovar_getbuf(bcmcfg_to_prmry_ndev(cfg), "ratestat", NULL, 0,
		iovar_buf, WLC_IOCTL_MAXLEN, NULL);
	if (err != BCME_OK && err != BCME_UNSUPPORTED) {
		WL_ERR(("error (%d) - size = %zu\n", err, NUM_RATE*sizeof(wifi_rate_stat)));
		goto exit;
	}
	for (i = 0; i < NUM_RATE; i++) {
		p_wifi_rate_stat =
			(wifi_rate_stat *)(iovar_buf + i*sizeof(wifi_rate_stat));
		p_wifi_rate_stat_v1 = (wifi_rate_stat_v1 *)output;
		p_wifi_rate_stat_v1->rate.preamble = p_wifi_rate_stat->rate.preamble;
		p_wifi_rate_stat_v1->rate.nss = p_wifi_rate_stat->rate.nss;
		p_wifi_rate_stat_v1->rate.bw = p_wifi_rate_stat->rate.bw;
		p_wifi_rate_stat_v1->rate.rateMcsIdx = p_wifi_rate_stat->rate.rateMcsIdx;
		p_wifi_rate_stat_v1->rate.reserved = p_wifi_rate_stat->rate.reserved;
		p_wifi_rate_stat_v1->rate.bitrate = p_wifi_rate_stat->rate.bitrate;
		p_wifi_rate_stat_v1->tx_mpdu = p_wifi_rate_stat->tx_mpdu;
		p_wifi_rate_stat_v1->rx_mpdu = p_wifi_rate_stat->rx_mpdu;
		p_wifi_rate_stat_v1->mpdu_lost = p_wifi_rate_stat->mpdu_lost;
		p_wifi_rate_stat_v1->retries = p_wifi_rate_stat->retries;
		p_wifi_rate_stat_v1->retries_short = p_wifi_rate_stat->retries_short;
		p_wifi_rate_stat_v1->retries_long = p_wifi_rate_stat->retries_long;
		output = (char *) &(p_wifi_rate_stat_v1->retries_long);
		output += sizeof(p_wifi_rate_stat_v1->retries_long);
	}

	total_len = sizeof(wifi_radio_stat_h) + chan_stats_size;
	total_len = total_len - sizeof(wifi_peer_info) +
		NUM_PEER * (sizeof(wifi_peer_info) - sizeof(wifi_rate_stat_v1) +
			NUM_RATE * sizeof(wifi_rate_stat_v1));

	if (total_len > WLC_IOCTL_MAXLEN) {
		WL_ERR(("Error! total_len:%d is unexpected value\n", total_len));
		err = BCME_BADLEN;
		goto exit;
	}
	err =  wl_cfgvendor_send_cmd_reply(wiphy, outdata, total_len);

	if (unlikely(err))
		WL_ERR(("Vendor Command reply failed ret:%d \n", err));

exit:
	if (outdata) {
		MFREE(cfg->osh, outdata, WLC_IOCTL_MAXLEN);
	}
	if (if_stats) {
		MFREE(cfg->osh, if_stats, sizeof(wl_if_stats_t));
	}
#ifdef CHAN_STATS_SUPPORT
	if (all_chan_stats) {
		MFREE(cfg->osh, all_chan_stats, chan_stats_size);
	}
	if (per_chspec_stats) {
		MFREE(cfg->osh, per_chspec_stats, per_chspec_stats_size);
	}
#endif /* CHAN_STATS_SUPPORT */
	return err;
}
#endif /* LINKSTAT_SUPPORT */

#ifdef DHD_LOG_DUMP
static int
wl_cfgvendor_get_buf_data(const struct nlattr *iter, struct buf_data *buf)
{
	int ret = BCME_OK;
#ifdef CONFIG_COMPAT
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0))
	if (in_compat_syscall()) {
#else
	if (is_compat_task()) {
#endif /* LINUX_VER >= 4.6 */
		struct compat_buf_data *compat_buf = (struct compat_buf_data *)nla_data(iter);

		if (nla_len(iter) != sizeof(struct compat_buf_data)) {
			WL_ERR(("Invalid len : %d\n", nla_len(iter)));
			ret = BCME_BADLEN;
		}

		buf->ver = compat_buf->ver;
		buf->len = compat_buf->len;
		buf->buf_threshold = compat_buf->buf_threshold;
		buf->data_buf[0] = (const void *)compat_ptr(compat_buf->data_buf);
	}
	else
#endif /* CONFIG_COMPAT */
	{
		if (nla_len(iter) != sizeof(struct buf_data)) {
			WL_ERR(("Invalid len : %d\n", nla_len(iter)));
			ret = BCME_BADLEN;
		}
		ret = memcpy_s(buf, sizeof(struct buf_data), (void *)nla_data(iter), nla_len(iter));
		if (ret) {
			WL_ERR(("Can't get buf data\n"));
			goto exit;
		}
	}

	if ((buf->len <= 0) || !buf->data_buf[0]) {
		WL_ERR(("Invalid buffer\n"));
		ret = BCME_ERROR;
	}
exit:
	return ret;

}

static int
wl_cfgvendor_dbg_file_dump(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void *data, int len)
{
	int ret = BCME_OK, rem, type = 0;
	const struct nlattr *iter;
	char *mem_buf = NULL;
	struct sk_buff *skb = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct buf_data *buf;
	struct buf_data data_from_hal;
	int pos = 0;

	/* Alloc the SKB for vendor_event */
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, CFG80211_VENDOR_CMD_REPLY_SKB_SZ);
	if (!skb) {
		WL_ERR(("skb allocation is failed\n"));
		ret = BCME_NOMEM;
		goto exit;
	}
	WL_ERR(("%s\n", __FUNCTION__));

	memset_s(&data_from_hal, sizeof(data_from_hal),  0, sizeof(data_from_hal));
	buf = &data_from_hal;
	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		ret = wl_cfgvendor_get_buf_data(iter, buf);
		if (ret)
			goto exit;
		switch (type) {
			case DUMP_BUF_ATTR_MEMDUMP:
				ret = dhd_os_get_socram_dump(bcmcfg_to_prmry_ndev(cfg), &mem_buf,
					(uint32 *)(&(buf->len)));
				if (ret) {
					WL_ERR(("failed to get_socram_dump : %d\n", ret));
					goto exit;
				}
				ret = dhd_export_debug_data(mem_buf, NULL, buf->data_buf[0],
					(int)buf->len, &pos);
				break;

			case DUMP_BUF_ATTR_TIMESTAMP :
				ret = dhd_print_time_str(buf->data_buf[0], NULL,
					(uint32)buf->len, &pos);
				break;
#ifdef EWP_ECNTRS_LOGGING
			case DUMP_BUF_ATTR_ECNTRS :
				ret = dhd_print_ecntrs_data(bcmcfg_to_prmry_ndev(cfg), NULL,
					buf->data_buf[0], NULL, (uint32)buf->len, &pos);
				break;
#endif /* EWP_ECNTRS_LOGGING */
#ifdef DHD_STATUS_LOGGING
			case DUMP_BUF_ATTR_STATUS_LOG :
				ret = dhd_print_status_log_data(bcmcfg_to_prmry_ndev(cfg), NULL,
					buf->data_buf[0], NULL, (uint32)buf->len, &pos);
				break;
#endif /* DHD_STATUS_LOGGING */
#ifdef EWP_RTT_LOGGING
			case DUMP_BUF_ATTR_RTT_LOG :
				ret = dhd_print_rtt_data(bcmcfg_to_prmry_ndev(cfg), NULL,
					buf->data_buf[0], NULL, (uint32)buf->len, &pos);
				break;
#endif /* EWP_RTT_LOGGING */
			case DUMP_BUF_ATTR_DHD_DUMP :
				ret = dhd_print_dump_data(bcmcfg_to_prmry_ndev(cfg), NULL,
					buf->data_buf[0], NULL, (uint32)buf->len, &pos);
				break;
#if defined(BCMPCIE)
			case DUMP_BUF_ATTR_EXT_TRAP :
				ret = dhd_print_ext_trap_data(bcmcfg_to_prmry_ndev(cfg), NULL,
					buf->data_buf[0], NULL, (uint32)buf->len, &pos);
				break;
#endif /* BCMPCIE */
#if defined(DHD_FW_COREDUMP) && defined(DNGL_EVENT_SUPPORT)
			case DUMP_BUF_ATTR_HEALTH_CHK :
				ret = dhd_print_health_chk_data(bcmcfg_to_prmry_ndev(cfg), NULL,
					buf->data_buf[0], NULL, (uint32)buf->len, &pos);
				break;
#endif
			case DUMP_BUF_ATTR_COOKIE :
				ret = dhd_print_cookie_data(bcmcfg_to_prmry_ndev(cfg), NULL,
					buf->data_buf[0], NULL, (uint32)buf->len, &pos);
				break;
#ifdef DHD_DUMP_PCIE_RINGS
			case DUMP_BUF_ATTR_FLOWRING_DUMP :
				ret = dhd_print_flowring_data(bcmcfg_to_prmry_ndev(cfg), NULL,
					buf->data_buf[0], NULL, (uint32)buf->len, &pos);
				break;
#endif
			case DUMP_BUF_ATTR_GENERAL_LOG :
				ret = dhd_get_dld_log_dump(bcmcfg_to_prmry_ndev(cfg), NULL,
					buf->data_buf[0], NULL, (uint32)buf->len,
					DLD_BUF_TYPE_GENERAL, &pos);
				break;

			case DUMP_BUF_ATTR_PRESERVE_LOG :
				ret = dhd_get_dld_log_dump(bcmcfg_to_prmry_ndev(cfg), NULL,
					buf->data_buf[0], NULL, (uint32)buf->len,
					DLD_BUF_TYPE_PRESERVE, &pos);
				break;

			case DUMP_BUF_ATTR_SPECIAL_LOG :
				ret = dhd_get_dld_log_dump(bcmcfg_to_prmry_ndev(cfg), NULL,
					buf->data_buf[0], NULL, (uint32)buf->len,
					DLD_BUF_TYPE_SPECIAL, &pos);
				break;
#ifdef DHD_MAP_PKTID_LOGGING
			case DUMP_BUF_ATTR_PKTID_MAP_LOG:
				ret = dhd_print_pktid_map_log_data(bcmcfg_to_prmry_ndev(cfg), NULL,
					buf->data_buf[0], NULL, (uint32)buf->len, &pos, TRUE);
				break;

			case DUMP_BUF_ATTR_PKTID_UNMAP_LOG:
				ret = dhd_print_pktid_map_log_data(bcmcfg_to_prmry_ndev(cfg), NULL,
					buf->data_buf[0], NULL, (uint32)buf->len, &pos, FALSE);
				break;
#endif /* DHD_MAP_PKTID_LOGGIN */

#ifdef DHD_SSSR_DUMP
#ifdef DHD_SSSR_DUMP_BEFORE_SR
			case DUMP_BUF_ATTR_SSSR_C0_D11_BEFORE :
				ret = dhd_sssr_dump_d11_buf_before(bcmcfg_to_prmry_ndev(cfg),
					buf->data_buf[0], (uint32)buf->len, 0);
				break;
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

			case DUMP_BUF_ATTR_SSSR_C0_D11_AFTER :
				ret = dhd_sssr_dump_d11_buf_after(bcmcfg_to_prmry_ndev(cfg),
					buf->data_buf[0], (uint32)buf->len, 0);
				break;

#ifdef DHD_SSSR_DUMP_BEFORE_SR
			case DUMP_BUF_ATTR_SSSR_C1_D11_BEFORE :
				ret = dhd_sssr_dump_d11_buf_before(bcmcfg_to_prmry_ndev(cfg),
					buf->data_buf[0], (uint32)buf->len, 1);
				break;
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

			case DUMP_BUF_ATTR_SSSR_C1_D11_AFTER :
				ret = dhd_sssr_dump_d11_buf_after(bcmcfg_to_prmry_ndev(cfg),
					buf->data_buf[0], (uint32)buf->len, 1);
				break;

#ifdef DHD_SSSR_DUMP_BEFORE_SR
			case DUMP_BUF_ATTR_SSSR_C2_D11_BEFORE :
				ret = dhd_sssr_dump_d11_buf_before(bcmcfg_to_prmry_ndev(cfg),
					buf->data_buf[0], (uint32)buf->len, 2);
				break;
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

			case DUMP_BUF_ATTR_SSSR_C2_D11_AFTER :
				ret = dhd_sssr_dump_d11_buf_after(bcmcfg_to_prmry_ndev(cfg),
					buf->data_buf[0], (uint32)buf->len, 2);
				break;

#ifdef DHD_SSSR_DUMP_BEFORE_SR
			case DUMP_BUF_ATTR_SSSR_DIG_BEFORE :
				ret = dhd_sssr_dump_dig_buf_before(bcmcfg_to_prmry_ndev(cfg),
					buf->data_buf[0], (uint32)buf->len);
				break;
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

			case DUMP_BUF_ATTR_SSSR_DIG_AFTER :
				ret = dhd_sssr_dump_dig_buf_after(bcmcfg_to_prmry_ndev(cfg),
					buf->data_buf[0], (uint32)buf->len);
				break;
#endif /* DHD_SSSR_DUMP */
#ifdef DHD_PKT_LOGGING
			case DUMP_BUF_ATTR_PKTLOG:
				ret = dhd_os_get_pktlog_dump(bcmcfg_to_prmry_ndev(cfg),
					buf->data_buf[0], (uint32)buf->len);
				break;

			case DUMP_BUF_ATTR_PKTLOG_DEBUG:
				ret = dhd_os_get_pktlog_dump(bcmcfg_to_prmry_ndev(cfg),
					buf->data_buf[0], (uint32)buf->len);
				break;
#endif /* DHD_PKT_LOGGING */
#if defined(DNGL_AXI_ERROR_LOGGING) && defined(REPORT_AXI_ERROR)
			case DUMP_BUF_ATTR_AXI_ERROR:
				ret = dhd_os_get_axi_error_dump(bcmcfg_to_prmry_ndev(cfg),
					buf->data_buf[0], (uint32)buf->len);
				break;
#endif /* DNGL_AXI_ERROR_LOGGING && REPORT_AXI_ERROR */
			default:
				WL_ERR(("Unknown type: %d\n", type));
				ret = BCME_ERROR;
				goto exit;
		}
	}

	if (ret)
		goto exit;

	ret = nla_put_u32(skb, type, (uint32)(ret));
	if (ret < 0) {
		WL_ERR(("Failed to put type, ret:%d\n", ret));
		goto exit;
	}
	ret = cfg80211_vendor_cmd_reply(skb);
	if (ret) {
		WL_ERR(("Vendor Command reply failed ret:%d \n", ret));
	}
	return ret;
exit:
	if (skb) {
		/* Free skb memory */
		kfree_skb(skb);
	}
	return ret;
}
#endif /* DHD_LOG_DUMP */

#ifdef DEBUGABILITY
#ifndef DEBUGABILITY_DISABLE_MEMDUMP
static int
wl_cfgvendor_dbg_trigger_mem_dump(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void  *data, int len)
{
	int ret = BCME_OK;
	uint32 alloc_len;
	struct sk_buff *skb = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	dhd_pub_t *dhdp = (dhd_pub_t *)(cfg->pub);

	WL_ERR(("wl_cfgvendor_dbg_trigger_mem_dump %d\n", __LINE__));

	dhdp->memdump_type = DUMP_TYPE_CFG_VENDOR_TRIGGERED;
	ret = dhd_os_socram_dump(bcmcfg_to_prmry_ndev(cfg), &alloc_len);
	if (ret) {
		WL_ERR(("failed to call dhd_os_socram_dump : %d\n", ret));
		goto exit;
	}
	/* Alloc the SKB for vendor_event */
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, CFG80211_VENDOR_CMD_REPLY_SKB_SZ);
	if (!skb) {
		WL_ERR(("skb allocation is failed\n"));
		ret = BCME_NOMEM;
		goto exit;
	}
	ret = nla_put_u32(skb, DEBUG_ATTRIBUTE_FW_DUMP_LEN, alloc_len);

	if (unlikely(ret)) {
		WL_ERR(("Failed to put fw dump length, ret=%d\n", ret));
		goto exit;
	}

	ret = cfg80211_vendor_cmd_reply(skb);

	if (ret) {
		WL_ERR(("Vendor Command reply failed ret:%d \n", ret));
		goto exit;
	}
	return ret;
exit:
	/* Free skb memory */
	if (skb) {
		kfree_skb(skb);
	}
	return ret;
}

static int
wl_cfgvendor_dbg_get_mem_dump(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void *data, int len)
{
	int ret = BCME_OK, rem, type;
	int buf_len = 0;
	uintptr_t user_buf = (uintptr_t)NULL;
	const struct nlattr *iter;
	char *mem_buf = NULL;
	struct sk_buff *skb = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);

	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		switch (type) {
			case DEBUG_ATTRIBUTE_FW_DUMP_LEN:
				/* Check if the iter is valid and
				 * buffer length is not already initialized.
				 */
				if ((nla_len(iter) == sizeof(uint32)) &&
						!buf_len) {
					buf_len = nla_get_u32(iter);
					if (buf_len <= 0) {
						ret = BCME_ERROR;
						goto exit;
					}
				} else {
					ret = BCME_ERROR;
					goto exit;
				}
				break;
			case DEBUG_ATTRIBUTE_FW_DUMP_DATA:
				if (nla_len(iter) != sizeof(uint64)) {
					WL_ERR(("Invalid len\n"));
					ret = BCME_ERROR;
					goto exit;
				}
				user_buf = (uintptr_t)nla_get_u64(iter);
				if (!user_buf) {
					ret = BCME_ERROR;
					goto exit;
				}
				break;
			default:
				WL_ERR(("Unknown type: %d\n", type));
				ret = BCME_ERROR;
				goto exit;
		}
	}
	if (buf_len > 0 && user_buf) {
		mem_buf = vmalloc(buf_len);
		if (!mem_buf) {
			WL_ERR(("failed to allocate mem_buf with size : %d\n", buf_len));
			ret = BCME_NOMEM;
			goto exit;
		}
		ret = dhd_os_get_socram_dump(bcmcfg_to_prmry_ndev(cfg), &mem_buf, &buf_len);
		if (ret) {
			WL_ERR(("failed to get_socram_dump : %d\n", ret));
			goto free_mem;
		}
#ifdef CONFIG_COMPAT
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0))
		if (in_compat_syscall()) {
#else
		if (is_compat_task()) {
#endif /* LINUX_VER >= 4.6 */
			void * usr_ptr =  compat_ptr((uintptr_t) user_buf);
			ret = copy_to_user(usr_ptr, mem_buf, buf_len);
			if (ret) {
				WL_ERR(("failed to copy memdump into user buffer : %d\n", ret));
				goto free_mem;
			}
		}
		else
#endif /* CONFIG_COMPAT */
		{
			ret = copy_to_user((void*)user_buf, mem_buf, buf_len);
			if (ret) {
				WL_ERR(("failed to copy memdump into user buffer : %d\n", ret));
				goto free_mem;
			}
		}
		/* Alloc the SKB for vendor_event */
		skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, CFG80211_VENDOR_CMD_REPLY_SKB_SZ);
		if (!skb) {
			WL_ERR(("skb allocation is failed\n"));
			ret = BCME_NOMEM;
			goto free_mem;
		}
		/* Indicate the memdump is succesfully copied */
		ret = nla_put(skb, DEBUG_ATTRIBUTE_FW_DUMP_DATA, sizeof(ret), &ret);
		if (ret < 0) {
			WL_ERR(("Failed to put DEBUG_ATTRIBUTE_FW_DUMP_DATA, ret:%d\n", ret));
			goto free_mem;
		}

		ret = cfg80211_vendor_cmd_reply(skb);

		if (ret) {
			WL_ERR(("Vendor Command reply failed ret:%d \n", ret));
		}
		skb = NULL;
	}

free_mem:
	vfree(mem_buf);
	/* Free skb memory */
	if (skb) {
		kfree_skb(skb);
	}
exit:
	return ret;
}
#else
static int
wl_cfgvendor_dbg_trigger_mem_dump(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	return WIFI_ERROR_NOT_SUPPORTED;
}

static int
wl_cfgvendor_dbg_get_mem_dump(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int len)
{
	return WIFI_ERROR_NOT_SUPPORTED;
}
#endif /* !DEBUGABILITY_DISABLE_MEMDUMP */

static int wl_cfgvendor_dbg_start_logging(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int ret = BCME_OK, rem, type;
	char ring_name[DBGRING_NAME_MAX] = {0};
	int log_level = 0, flags = 0, time_intval = 0, threshold = 0;
	const struct nlattr *iter;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	dhd_pub_t *dhd_pub = cfg->pub;
	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		switch (type) {
			case DEBUG_ATTRIBUTE_RING_NAME:
				strncpy(ring_name, nla_data(iter),
					MIN(sizeof(ring_name) -1, nla_len(iter)));
				break;
			case DEBUG_ATTRIBUTE_LOG_LEVEL:
				log_level = nla_get_u32(iter);
				break;
			case DEBUG_ATTRIBUTE_RING_FLAGS:
				flags = nla_get_u32(iter);
				break;
			case DEBUG_ATTRIBUTE_LOG_TIME_INTVAL:
				time_intval = nla_get_u32(iter);
				break;
			case DEBUG_ATTRIBUTE_LOG_MIN_DATA_SIZE:
				threshold = nla_get_u32(iter);
				break;
			default:
				WL_ERR(("Unknown type: %d\n", type));
				ret = BCME_BADADDR;
				goto exit;
		}
	}

	ret = dhd_os_start_logging(dhd_pub, ring_name, log_level, flags, time_intval, threshold);
	if (ret < 0) {
		WL_ERR(("start_logging is failed ret: %d\n", ret));
	}
exit:
	return ret;
}

static int wl_cfgvendor_dbg_reset_logging(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int ret = BCME_OK;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	dhd_pub_t *dhd_pub = cfg->pub;

	ret = dhd_os_reset_logging(dhd_pub);
	if (ret < 0) {
		WL_ERR(("reset logging is failed ret: %d\n", ret));
	}

	return ret;
}

static int wl_cfgvendor_dbg_get_ring_status(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int ret = BCME_OK;
	int ring_id, i;
	int ring_cnt;
	struct sk_buff *skb;
	dhd_dbg_ring_status_t dbg_ring_status[DEBUG_RING_ID_MAX];
	dhd_dbg_ring_status_t ring_status;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	dhd_pub_t *dhd_pub = cfg->pub;
	bzero(dbg_ring_status, DBG_RING_STATUS_SIZE * DEBUG_RING_ID_MAX);
	ring_cnt = 0;
	for (ring_id = DEBUG_RING_ID_INVALID + 1; ring_id < DEBUG_RING_ID_MAX; ring_id++) {
		ret = dhd_os_get_ring_status(dhd_pub, ring_id, &ring_status);
		if (ret == BCME_NOTFOUND) {
			WL_DBG(("The ring (%d) is not found \n", ring_id));
		} else if (ret == BCME_OK) {
			dbg_ring_status[ring_cnt++] = ring_status;
		}
	}
	/* Alloc the SKB for vendor_event */
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
		nla_total_size(DBG_RING_STATUS_SIZE) * ring_cnt + nla_total_size(sizeof(ring_cnt)));
	if (!skb) {
		WL_ERR(("skb allocation is failed\n"));
		ret = BCME_NOMEM;
		goto exit;
	}

	/* Ignore return of nla_put_u32 and nla_put since the skb allocated
	 * above has a requested size for all payload
	 */
	(void)nla_put_u32(skb, DEBUG_ATTRIBUTE_RING_NUM, ring_cnt);
	for (i = 0; i < ring_cnt; i++) {
		(void)nla_put(skb, DEBUG_ATTRIBUTE_RING_STATUS, DBG_RING_STATUS_SIZE,
				&dbg_ring_status[i]);
	}
	ret = cfg80211_vendor_cmd_reply(skb);

	if (ret) {
		WL_ERR(("Vendor Command reply failed ret:%d \n", ret));
	}
exit:
	return ret;
}

static int wl_cfgvendor_dbg_get_ring_data(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int ret = BCME_OK, rem, type;
	char ring_name[DBGRING_NAME_MAX] = {0};
	const struct nlattr *iter;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	dhd_pub_t *dhd_pub = cfg->pub;

	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		switch (type) {
			case DEBUG_ATTRIBUTE_RING_NAME:
				strlcpy(ring_name, nla_data(iter), sizeof(ring_name));
				break;
			default:
				WL_ERR(("Unknown type: %d\n", type));
				return ret;
		}
	}

	ret = dhd_os_trigger_get_ring_data(dhd_pub, ring_name);
	if (ret < 0) {
		WL_ERR(("trigger_get_data failed ret:%d\n", ret));
	}

	return ret;
}
#endif /* DEBUGABILITY */

static int wl_cfgvendor_dbg_get_feature(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int ret = BCME_OK;
	u32 supported_features = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	dhd_pub_t *dhd_pub = cfg->pub;

	ret = dhd_os_dbg_get_feature(dhd_pub, &supported_features);
	if (ret < 0) {
		WL_ERR(("dbg_get_feature failed ret:%d\n", ret));
		goto exit;
	}
	ret = wl_cfgvendor_send_cmd_reply(wiphy, &supported_features,
		sizeof(supported_features));
exit:
	return ret;
}

#ifdef DEBUGABILITY
static void wl_cfgvendor_dbg_ring_send_evt(void *ctx,
	const int ring_id, const void *data, const uint32 len,
	const dhd_dbg_ring_status_t ring_status)
{
	struct net_device *ndev = ctx;
	struct wiphy *wiphy;
	gfp_t kflags;
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	struct bcm_cfg80211 *cfg;
	if (!ndev) {
		WL_ERR(("ndev is NULL\n"));
		return;
	}
	kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	wiphy = ndev->ieee80211_ptr->wiphy;
	cfg = wiphy_priv(wiphy);

	/* If wifi hal is not start, don't send event to wifi hal */
	if (!cfg->hal_started) {
		WL_ERR(("Hal is not started\n"));
		return;
	}
	/* Alloc the SKB for vendor_event */
#if (defined(CONFIG_ARCH_MSM) && defined(SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC)) || \
	LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
	skb = cfg80211_vendor_event_alloc(wiphy, NULL, len + CFG80211_VENDOR_EVT_SKB_SZ,
			GOOGLE_DEBUG_RING_EVENT, kflags);
#else
	skb = cfg80211_vendor_event_alloc(wiphy, len + CFG80211_VENDOR_EVT_SKB_SZ,
			GOOGLE_DEBUG_RING_EVENT, kflags);
#endif /* (defined(CONFIG_ARCH_MSM) && defined(SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC)) || */
		/* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0) */
	if (!skb) {
		WL_ERR(("skb alloc failed"));
		return;
	}
	/* Set halpid for sending unicast event to wifi hal */
	nlh = (struct nlmsghdr*)skb->data;
	nlh->nlmsg_pid = cfg->halpid;
	nla_put(skb, DEBUG_ATTRIBUTE_RING_STATUS, sizeof(ring_status), &ring_status);
	nla_put(skb, DEBUG_ATTRIBUTE_RING_DATA, len, data);
	cfg80211_vendor_event(skb, kflags);
}
#endif /* DEBUGABILITY */

#ifdef DHD_LOG_DUMP
#ifdef DHD_SSSR_DUMP
#define DUMP_SSSR_DUMP_MAX_COUNT	8
static int wl_cfgvendor_nla_put_sssr_dump_data(struct sk_buff *skb,
		struct net_device *ndev)
{
	int ret = BCME_OK;
#ifdef DHD_SSSR_DUMP
	uint32 arr_len[DUMP_SSSR_DUMP_MAX_COUNT];
#endif /* DHD_SSSR_DUMP */
	char memdump_path[MEMDUMP_PATH_LEN];
	dhd_pub_t *dhdp = wl_cfg80211_get_dhdp(ndev);

#ifdef DHD_SSSR_DUMP_BEFORE_SR
	dhd_get_memdump_filename(ndev, memdump_path, MEMDUMP_PATH_LEN,
		"sssr_dump_core_0_before_SR");
	ret = nla_put_string(skb, DUMP_FILENAME_ATTR_SSSR_CORE_0_BEFORE_DUMP, memdump_path);
	if (unlikely(ret)) {
		WL_ERR(("Failed to nla put sssr core 0 before dump path, ret=%d\n", ret));
		goto exit;
	}
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

	dhd_get_memdump_filename(ndev, memdump_path, MEMDUMP_PATH_LEN,
		"sssr_dump_core_0_after_SR");
	ret = nla_put_string(skb, DUMP_FILENAME_ATTR_SSSR_CORE_0_AFTER_DUMP, memdump_path);
	if (unlikely(ret)) {
		WL_ERR(("Failed to nla put sssr core 1 after dump path, ret=%d\n", ret));
		goto exit;
	}

#ifdef DHD_SSSR_DUMP_BEFORE_SR
	dhd_get_memdump_filename(ndev, memdump_path, MEMDUMP_PATH_LEN,
		"sssr_dump_core_1_before_SR");
	ret = nla_put_string(skb, DUMP_FILENAME_ATTR_SSSR_CORE_1_BEFORE_DUMP, memdump_path);
	if (unlikely(ret)) {
		WL_ERR(("Failed to nla put sssr core 1 before dump path, ret=%d\n", ret));
		goto exit;
	}
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

	dhd_get_memdump_filename(ndev, memdump_path, MEMDUMP_PATH_LEN,
		"sssr_dump_core_1_after_SR");
	ret = nla_put_string(skb, DUMP_FILENAME_ATTR_SSSR_CORE_1_AFTER_DUMP, memdump_path);
	if (unlikely(ret)) {
		WL_ERR(("Failed to nla put sssr core 1 after dump path, ret=%d\n", ret));
		goto exit;
	}

	if (dhdp->sssr_d11_outofreset[2]) {
#ifdef DHD_SSSR_DUMP_BEFORE_SR
		dhd_get_memdump_filename(ndev, memdump_path, MEMDUMP_PATH_LEN,
			"sssr_dump_core_2_before_SR");
		ret = nla_put_string(skb, DUMP_FILENAME_ATTR_SSSR_CORE_2_BEFORE_DUMP,
			memdump_path);
		if (unlikely(ret)) {
			WL_ERR(("Failed to nla put sssr core 2 before dump path, ret=%d\n",
				ret));
			goto exit;
		}
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

		dhd_get_memdump_filename(ndev, memdump_path, MEMDUMP_PATH_LEN,
			"sssr_dump_core_2_after_SR");
		ret = nla_put_string(skb, DUMP_FILENAME_ATTR_SSSR_CORE_2_AFTER_DUMP,
			memdump_path);
		if (unlikely(ret)) {
			WL_ERR(("Failed to nla put sssr core 2 after dump path, ret=%d\n",
				ret));
			goto exit;
		}
	}

#ifdef DHD_SSSR_DUMP_BEFORE_SR
	dhd_get_memdump_filename(ndev, memdump_path, MEMDUMP_PATH_LEN,
		"sssr_dump_dig_before_SR");
	ret = nla_put_string(skb, DUMP_FILENAME_ATTR_SSSR_DIG_BEFORE_DUMP, memdump_path);
	if (unlikely(ret)) {
		WL_ERR(("Failed to nla put sssr dig before dump path, ret=%d\n", ret));
		goto exit;
	}
#endif /* DHD_SSSR_DUMP_BEFORE_SR */

	dhd_get_memdump_filename(ndev, memdump_path, MEMDUMP_PATH_LEN,
		"sssr_dump_dig_after_SR");
	ret = nla_put_string(skb, DUMP_FILENAME_ATTR_SSSR_DIG_AFTER_DUMP, memdump_path);
	if (unlikely(ret)) {
		WL_ERR(("Failed to nla put sssr dig after dump path, ret=%d\n", ret));
		goto exit;
	}

#ifdef DHD_SSSR_DUMP
	memset(arr_len, 0, sizeof(arr_len));
	dhd_nla_put_sssr_dump_len(ndev, arr_len);
#ifdef DHD_SSSR_DUMP_BEFORE_SR
	ret |= nla_put_u32(skb, DUMP_LEN_ATTR_SSSR_C0_D11_BEFORE, arr_len[0]);
	ret |= nla_put_u32(skb, DUMP_LEN_ATTR_SSSR_C1_D11_BEFORE, arr_len[2]);
	ret |= nla_put_u32(skb, DUMP_LEN_ATTR_SSSR_C2_D11_BEFORE, arr_len[4]);
	ret |= nla_put_u32(skb, DUMP_LEN_ATTR_SSSR_DIG_BEFORE, arr_len[6]);
#endif /* DHD_SSSR_DUMP_BEFORE_SR */
	ret |= nla_put_u32(skb, DUMP_LEN_ATTR_SSSR_C0_D11_AFTER, arr_len[1]);
	ret |= nla_put_u32(skb, DUMP_LEN_ATTR_SSSR_C1_D11_AFTER, arr_len[3]);
	ret |= nla_put_u32(skb, DUMP_LEN_ATTR_SSSR_C2_D11_AFTER, arr_len[5]);
	ret |= nla_put_u32(skb, DUMP_LEN_ATTR_SSSR_DIG_AFTER, arr_len[7]);
	if (unlikely(ret)) {
		WL_ERR(("Failed to nla put sssr dump len, ret=%d\n", ret));
		goto exit;
	}
#endif /* DHD_SSSR_DUMP */

exit:
	return ret;
}
#else
static int wl_cfgvendor_nla_put_sssr_dump_data(struct sk_buff *skb,
		struct net_device *ndev)
{
	return BCME_OK;
}
#endif /* DHD_SSSR_DUMP */

static int wl_cfgvendor_nla_put_debug_dump_data(struct sk_buff *skb,
		struct net_device *ndev)
{
	int ret = BCME_OK;
	uint32 len = 0;
	char dump_path[128];

	ret = dhd_get_debug_dump_file_name(ndev, NULL, dump_path, sizeof(dump_path));
	if (ret < 0) {
		WL_ERR(("%s: Failed to get debug dump filename\n", __FUNCTION__));
		goto exit;
	}
	ret = nla_put_string(skb, DUMP_FILENAME_ATTR_DEBUG_DUMP, dump_path);
	if (unlikely(ret)) {
		WL_ERR(("Failed to nla put debug dump path, ret=%d\n", ret));
		goto exit;
	}
	WL_ERR(("debug_dump path = %s%s\n", dump_path, FILE_NAME_HAL_TAG));
	wl_print_verinfo(wl_get_cfg(ndev));

	len = dhd_get_time_str_len();
	if (len) {
		ret = nla_put_u32(skb, DUMP_LEN_ATTR_TIMESTAMP, len);
		if (unlikely(ret)) {
			WL_ERR(("Failed to nla put time stamp length, ret=%d\n", ret));
			goto exit;
		}
	}

	len = dhd_get_dld_len(DLD_BUF_TYPE_GENERAL);
	if (len) {
		ret = nla_put_u32(skb, DUMP_LEN_ATTR_GENERAL_LOG, len);
		if (unlikely(ret)) {
			WL_ERR(("Failed to nla put general log length, ret=%d\n", ret));
			goto exit;
		}
	}
#ifdef EWP_ECNTRS_LOGGING
	len = dhd_get_ecntrs_len(ndev, NULL);
	if (len) {
		ret = nla_put_u32(skb, DUMP_LEN_ATTR_ECNTRS, len);
		if (unlikely(ret)) {
			WL_ERR(("Failed to nla put ecntrs length, ret=%d\n", ret));
			goto exit;
		}
	}
#endif /* EWP_ECNTRS_LOGGING */
	len = dhd_get_dld_len(DLD_BUF_TYPE_SPECIAL);
	if (len) {
		ret = nla_put_u32(skb, DUMP_LEN_ATTR_SPECIAL_LOG, len);
		if (unlikely(ret)) {
			WL_ERR(("Failed to nla put special log length, ret=%d\n", ret));
			goto exit;
		}
	}
	len = dhd_get_dhd_dump_len(ndev, NULL);
	if (len) {
		ret = nla_put_u32(skb, DUMP_LEN_ATTR_DHD_DUMP, len);
		if (unlikely(ret)) {
			WL_ERR(("Failed to nla put dhd dump length, ret=%d\n", ret));
			goto exit;
		}
	}

#if defined(BCMPCIE)
	len = dhd_get_ext_trap_len(ndev, NULL);
	if (len) {
		ret = nla_put_u32(skb, DUMP_LEN_ATTR_EXT_TRAP, len);
		if (unlikely(ret)) {
			WL_ERR(("Failed to nla put ext trap length, ret=%d\n", ret));
			goto exit;
		}
	}
#endif /* BCMPCIE */

#if defined(DHD_FW_COREDUMP) && defined(DNGL_EVENT_SUPPORT)
	len = dhd_get_health_chk_len(ndev, NULL);
	if (len) {
		ret = nla_put_u32(skb, DUMP_LEN_ATTR_HEALTH_CHK, len);
		if (unlikely(ret)) {
			WL_ERR(("Failed to nla put health check length, ret=%d\n", ret));
			goto exit;
		}
	}
#endif

	len = dhd_get_dld_len(DLD_BUF_TYPE_PRESERVE);
	if (len) {
		ret = nla_put_u32(skb, DUMP_LEN_ATTR_PRESERVE_LOG, len);
		if (unlikely(ret)) {
			WL_ERR(("Failed to nla put preserve log length, ret=%d\n", ret));
			goto exit;
		}
	}

	len = dhd_get_cookie_log_len(ndev, NULL);
	if (len) {
		ret = nla_put_u32(skb, DUMP_LEN_ATTR_COOKIE, len);
		if (unlikely(ret)) {
			WL_ERR(("Failed to nla put cookie length, ret=%d\n", ret));
			goto exit;
		}
	}
#ifdef DHD_DUMP_PCIE_RINGS
	len = dhd_get_flowring_len(ndev, NULL);
	if (len) {
		ret = nla_put_u32(skb, DUMP_LEN_ATTR_FLOWRING_DUMP, len);
		if (unlikely(ret)) {
			WL_ERR(("Failed to nla put flowring dump length, ret=%d\n", ret));
			goto exit;
		}
	}
#endif
#ifdef DHD_STATUS_LOGGING
	len = dhd_get_status_log_len(ndev, NULL);
	if (len) {
		ret = nla_put_u32(skb, DUMP_LEN_ATTR_STATUS_LOG, len);
		if (unlikely(ret)) {
			WL_ERR(("Failed to nla put status log length, ret=%d\n", ret));
			goto exit;
		}
	}
#endif /* DHD_STATUS_LOGGING */
#ifdef EWP_RTT_LOGGING
	len = dhd_get_rtt_len(ndev, NULL);
	if (len) {
		ret = nla_put_u32(skb, DUMP_LEN_ATTR_RTT_LOG, len);
		if (unlikely(ret)) {
			WL_ERR(("Failed to nla put rtt log length, ret=%d\n", ret));
			goto exit;
		}
	}
#endif /* EWP_RTT_LOGGING */
#ifdef  DHD_MAP_PKTID_LOGGING
	len = dhd_get_pktid_map_logging_len(ndev, NULL, TRUE);
	if (len) {
		ret = nla_put_u32(skb, DUMP_LEN_ATTR_PKTID_MAP_LOG, len);
		if (unlikely(ret)) {
			WL_ERR(("Failed to nla put pktid log lenght, ret=%d", ret));
			goto exit;
		}
	}

	len = dhd_get_pktid_map_logging_len(ndev, NULL, FALSE);
	if (len) {
		ret = nla_put_u32(skb, DUMP_LEN_ATTR_PKTID_UNMAP_LOG, len);
		if (unlikely(ret)) {
			WL_ERR(("Failed to nla put pktid log lenght, ret=%d", ret));
			goto exit;
		}
	}
#endif /* DHD_MAP_PKTID_LOGGING */
exit:
	return ret;
}
#if defined(DNGL_AXI_ERROR_LOGGING) && defined(REPORT_AXI_ERROR)
static void wl_cfgvendor_nla_put_axi_error_data(struct sk_buff *skb,
		struct net_device *ndev)
{
	int ret = 0;
	char axierrordump_path[MEMDUMP_PATH_LEN];
	int dumpsize = dhd_os_get_axi_error_dump_size(ndev);
	if (dumpsize <= 0) {
		WL_ERR(("Failed to calcuate axi error dump len\n"));
		return;
	}
	dhd_os_get_axi_error_filename(ndev, axierrordump_path, MEMDUMP_PATH_LEN);
	ret = nla_put_string(skb, DUMP_FILENAME_ATTR_AXI_ERROR_DUMP, axierrordump_path);
	if (ret) {
		WL_ERR(("Failed to put filename\n"));
		return;
	}
	ret = nla_put_u32(skb, DUMP_LEN_ATTR_AXI_ERROR, dumpsize);
	if (ret) {
		WL_ERR(("Failed to put filesize\n"));
		return;
	}
}
#endif /* DNGL_AXI_ERROR_LOGGING && REPORT_AXI_ERROR */
#ifdef DHD_PKT_LOGGING
static int wl_cfgvendor_nla_put_pktlogdump_data(struct sk_buff *skb,
		struct net_device *ndev, bool pktlogdbg)
{
	int ret = BCME_OK;
	char pktlogdump_path[MEMDUMP_PATH_LEN];
	uint32 pktlog_dumpsize = dhd_os_get_pktlog_dump_size(ndev);
	if (pktlog_dumpsize == 0) {
		WL_ERR(("Failed to calcuate pktlog len\n"));
		return BCME_ERROR;
	}

	dhd_os_get_pktlogdump_filename(ndev, pktlogdump_path, MEMDUMP_PATH_LEN);

	if (pktlogdbg) {
		ret = nla_put_string(skb, DUMP_FILENAME_ATTR_PKTLOG_DEBUG_DUMP, pktlogdump_path);
		if (ret) {
			WL_ERR(("Failed to put filename\n"));
			return ret;
		}
		ret = nla_put_u32(skb, DUMP_LEN_ATTR_PKTLOG_DEBUG, pktlog_dumpsize);
		if (ret) {
			WL_ERR(("Failed to put filesize\n"));
			return ret;
		}
	} else {
		ret = nla_put_string(skb, DUMP_FILENAME_ATTR_PKTLOG_DUMP, pktlogdump_path);
		if (ret) {
			WL_ERR(("Failed to put filename\n"));
			return ret;
		}
		ret = nla_put_u32(skb, DUMP_LEN_ATTR_PKTLOG, pktlog_dumpsize);
		if (ret) {
			WL_ERR(("Failed to put filesize\n"));
			return ret;
		}
	}
	return ret;
}
#endif /* DHD_PKT_LOGGING */

static int wl_cfgvendor_nla_put_memdump_data(struct sk_buff *skb,
		struct net_device *ndev, const uint32 fw_len)
{
	char memdump_path[MEMDUMP_PATH_LEN];
	int ret = BCME_OK;

	dhd_get_memdump_filename(ndev, memdump_path, MEMDUMP_PATH_LEN, "mem_dump");
	ret = nla_put_string(skb, DUMP_FILENAME_ATTR_MEM_DUMP, memdump_path);
	if (unlikely(ret)) {
		WL_ERR(("Failed to nla put mem dump path, ret=%d\n", ret));
		goto exit;
	}
	ret = nla_put_u32(skb, DUMP_LEN_ATTR_MEMDUMP, fw_len);
	if (unlikely(ret)) {
		WL_ERR(("Failed to nla put mem dump length, ret=%d\n", ret));
		goto exit;
	}

exit:
	return ret;
}

static int wl_cfgvendor_nla_put_dump_data(dhd_pub_t *dhd_pub, struct sk_buff *skb,
		struct net_device *ndev, const uint32 fw_len)
{
	int ret = BCME_OK;

#if defined(DNGL_AXI_ERROR_LOGGING) && defined(REPORT_AXI_ERROR)
	if (dhd_pub->smmu_fault_occurred) {
		wl_cfgvendor_nla_put_axi_error_data(skb, ndev);
	}
#endif /* DNGL_AXI_ERROR_LOGGING && REPORT_AXI_ERROR */
	if (dhd_pub->memdump_enabled || (dhd_pub->memdump_type == DUMP_TYPE_BY_SYSDUMP)) {
		if (((ret = wl_cfgvendor_nla_put_debug_dump_data(skb, ndev)) < 0) ||
			((ret = wl_cfgvendor_nla_put_memdump_data(skb, ndev, fw_len)) < 0) ||
			((ret = wl_cfgvendor_nla_put_sssr_dump_data(skb, ndev)) < 0)) {
			goto done;
		}
#ifdef DHD_PKT_LOGGING
		if ((ret = wl_cfgvendor_nla_put_pktlogdump_data(skb, ndev, FALSE)) < 0) {
			goto done;
		}
#endif /* DHD_PKT_LOGGING */
	}
done:
	return ret;
}

static void wl_cfgvendor_dbg_send_file_dump_evt(void *ctx, const void *data,
	const uint32 len, const uint32 fw_len)
{
	struct net_device *ndev = ctx;
	struct wiphy *wiphy;
	gfp_t kflags;
	struct sk_buff *skb = NULL;
	struct bcm_cfg80211 *cfg;
	dhd_pub_t *dhd_pub;
	int ret = BCME_OK;

	if (!ndev) {
		WL_ERR(("ndev is NULL\n"));
		return;
	}

	kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	wiphy = ndev->ieee80211_ptr->wiphy;
	/* Alloc the SKB for vendor_event */
#if (defined(CONFIG_ARCH_MSM) && defined(SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC)) || \
	LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
	skb = cfg80211_vendor_event_alloc(wiphy, NULL, len + CFG80211_VENDOR_EVT_SKB_SZ,
			GOOGLE_FILE_DUMP_EVENT, kflags);
#else
	skb = cfg80211_vendor_event_alloc(wiphy, len + CFG80211_VENDOR_EVT_SKB_SZ,
			GOOGLE_FILE_DUMP_EVENT, kflags);
#endif /* (defined(CONFIG_ARCH_MSM) && defined(SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC)) || */
		/* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0) */
	if (!skb) {
		WL_ERR(("skb alloc failed"));
		return;
	}

	cfg = wiphy_priv(wiphy);
	dhd_pub = cfg->pub;

#ifdef DHD_PKT_LOGGING
	if (dhd_pub->pktlog_debug) {
		if ((ret = wl_cfgvendor_nla_put_pktlogdump_data(skb, ndev, TRUE)) < 0) {
			WL_ERR(("nla put failed\n"));
			goto done;
		}
		dhd_pub->pktlog_debug = FALSE;
	} else
#endif /* DHD_PKT_LOGGING */
	{
		if ((ret = wl_cfgvendor_nla_put_dump_data(dhd_pub, skb, ndev, fw_len)) < 0) {
			WL_ERR(("nla put failed\n"));
			goto done;
		}
	}
	/* TODO : Similar to above function add for debug_dump, sssr_dump, and pktlog also. */
	cfg80211_vendor_event(skb, kflags);
	return;
done:
	if (skb) {
		dev_kfree_skb_any(skb);
	}
}
#endif /* DHD_LOG_DUMP */

static int wl_cfgvendor_dbg_get_version(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int len)
{
	int ret = BCME_OK, rem, type;
	int buf_len = 1024;
	bool dhd_ver = FALSE;
	char *buf_ptr, *ver, *p;
	const struct nlattr *iter;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);

	buf_ptr = (char *)MALLOCZ(cfg->osh, buf_len);
	if (!buf_ptr) {
		WL_ERR(("failed to allocate the buffer for version n"));
		ret = BCME_NOMEM;
		goto exit;
	}
	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		switch (type) {
			case DEBUG_ATTRIBUTE_GET_DRIVER:
				dhd_ver = TRUE;
				break;
			case DEBUG_ATTRIBUTE_GET_FW:
				dhd_ver = FALSE;
				break;
			default:
				WL_ERR(("Unknown type: %d\n", type));
				ret = BCME_ERROR;
				goto exit;
		}
	}
	ret = dhd_os_get_version(bcmcfg_to_prmry_ndev(cfg), dhd_ver, &buf_ptr, buf_len);
	if (ret < 0) {
		WL_ERR(("failed to get the version %d\n", ret));
		goto exit;
	}
	ver = strstr(buf_ptr, "version ");
	if (!ver) {
		WL_ERR(("failed to locate the version\n"));
		goto exit;
	}
	ver += strlen("version ");
	/* Adjust version format to fit in android sys property */
	for (p = ver; (*p != ' ') && (*p != '\n') && (*p != 0); p++) {
		;
	}
	ret = wl_cfgvendor_send_cmd_reply(wiphy, ver, p - ver);
exit:
	MFREE(cfg->osh, buf_ptr, buf_len);
	return ret;
}

#ifdef DBG_PKT_MON
static int wl_cfgvendor_dbg_start_pkt_fate_monitoring(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int len)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	dhd_pub_t *dhd_pub = cfg->pub;
	int ret;

	ret = dhd_os_dbg_attach_pkt_monitor(dhd_pub);
	if (unlikely(ret)) {
		WL_ERR(("failed to start pkt fate monitoring, ret=%d", ret));
	}

	return ret;
}

typedef int (*dbg_mon_get_pkts_t) (dhd_pub_t *dhdp, void __user *user_buf,
	uint16 req_count, uint16 *resp_count);

static int __wl_cfgvendor_dbg_get_pkt_fates(struct wiphy *wiphy,
	const void *data, int len, dbg_mon_get_pkts_t dbg_mon_get_pkts)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	dhd_pub_t *dhd_pub = cfg->pub;
	struct sk_buff *skb = NULL;
	const struct nlattr *iter;
	void __user *user_buf = NULL;
	uint16 req_count = 0, resp_count = 0;
	int ret, tmp, type, mem_needed;

	nla_for_each_attr(iter, data, len, tmp) {
		type = nla_type(iter);
		switch (type) {
			case DEBUG_ATTRIBUTE_PKT_FATE_NUM:
				req_count = nla_get_u32(iter);
				break;
			case DEBUG_ATTRIBUTE_PKT_FATE_DATA:
				user_buf = (void __user *)(unsigned long) nla_get_u64(iter);
				break;
			default:
				WL_ERR(("%s: no such attribute %d\n", __FUNCTION__, type));
				ret = -EINVAL;
				goto exit;
		}
	}

	if (!req_count || !user_buf) {
		WL_ERR(("%s: invalid request, user_buf=%p, req_count=%u\n",
			__FUNCTION__, user_buf, req_count));
		ret = -EINVAL;
		goto exit;
	}

	ret = dbg_mon_get_pkts(dhd_pub, user_buf, req_count, &resp_count);
	if (unlikely(ret)) {
		WL_ERR(("failed to get packets, ret:%d \n", ret));
		goto exit;
	}

	mem_needed = VENDOR_REPLY_OVERHEAD + ATTRIBUTE_U32_LEN;
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, mem_needed);
	if (unlikely(!skb)) {
		WL_ERR(("skb alloc failed"));
		ret = -ENOMEM;
		goto exit;
	}

	ret = nla_put_u32(skb, DEBUG_ATTRIBUTE_PKT_FATE_NUM, resp_count);
	if (ret < 0) {
		WL_ERR(("Failed to put DEBUG_ATTRIBUTE_PKT_FATE_NUM, ret:%d\n", ret));
		goto exit;
	}

	ret = cfg80211_vendor_cmd_reply(skb);
	if (unlikely(ret)) {
		WL_ERR(("vendor Command reply failed ret:%d \n", ret));
	}
	return ret;

exit:
	/* Free skb memory */
	if (skb) {
		kfree_skb(skb);
	}
	return ret;
}

static int wl_cfgvendor_dbg_get_tx_pkt_fates(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int ret;

	ret = __wl_cfgvendor_dbg_get_pkt_fates(wiphy, data, len,
			dhd_os_dbg_monitor_get_tx_pkts);
	if (unlikely(ret)) {
		WL_ERR(("failed to get tx packets, ret:%d \n", ret));
	}

	return ret;
}

static int wl_cfgvendor_dbg_get_rx_pkt_fates(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int ret;

	ret = __wl_cfgvendor_dbg_get_pkt_fates(wiphy, data, len,
			dhd_os_dbg_monitor_get_rx_pkts);
	if (unlikely(ret)) {
		WL_ERR(("failed to get rx packets, ret:%d \n", ret));
	}

	return ret;
}
#endif /* DBG_PKT_MON */

#ifdef KEEP_ALIVE
/* max size of IP packet for keep alive */
#define MKEEP_ALIVE_IP_PKT_MAX 256

static int wl_cfgvendor_start_mkeep_alive(struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int len)
{
	int ret = BCME_OK, rem, type;
	uint8 mkeep_alive_id = 0;
	uint8 *ip_pkt = NULL;
	uint16 ip_pkt_len = 0;
	uint16 ether_type = ETHERTYPE_IP;
	uint8 src_mac[ETHER_ADDR_LEN];
	uint8 dst_mac[ETHER_ADDR_LEN];
	uint32 period_msec = 0;
	const struct nlattr *iter;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);

	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		switch (type) {
			case MKEEP_ALIVE_ATTRIBUTE_ID:
				mkeep_alive_id = nla_get_u8(iter);
				break;
			case MKEEP_ALIVE_ATTRIBUTE_IP_PKT_LEN:
				ip_pkt_len = nla_get_u16(iter);
				if (ip_pkt_len > MKEEP_ALIVE_IP_PKT_MAX) {
					ret = BCME_BADARG;
					goto exit;
				}
				break;
			case MKEEP_ALIVE_ATTRIBUTE_IP_PKT:
				if (ip_pkt) {
					ret = BCME_BADARG;
					WL_ERR(("ip_pkt already allocated\n"));
					goto exit;
				}
				if (!ip_pkt_len) {
					ret = BCME_BADARG;
					WL_ERR(("ip packet length is 0\n"));
					goto exit;
				}
				ip_pkt = (u8 *)MALLOCZ(cfg->osh, ip_pkt_len);
				if (ip_pkt == NULL) {
					ret = BCME_NOMEM;
					WL_ERR(("Failed to allocate mem for ip packet\n"));
					goto exit;
				}
				memcpy(ip_pkt, (u8*)nla_data(iter), ip_pkt_len);
				break;
			case MKEEP_ALIVE_ATTRIBUTE_SRC_MAC_ADDR:
				memcpy(src_mac, nla_data(iter), ETHER_ADDR_LEN);
				break;
			case MKEEP_ALIVE_ATTRIBUTE_DST_MAC_ADDR:
				memcpy(dst_mac, nla_data(iter), ETHER_ADDR_LEN);
				break;
			case MKEEP_ALIVE_ATTRIBUTE_PERIOD_MSEC:
				period_msec = nla_get_u32(iter);
				break;
			case MKEEP_ALIVE_ATTRIBUTE_ETHER_TYPE:
				ether_type = nla_get_u16(iter);
				if (!((ether_type == ETHERTYPE_IP) ||
						(ether_type == ETHERTYPE_IPV6))) {
					WL_ERR(("Invalid ether type, %2x\n", ether_type));
					ret = BCME_BADARG;
					goto exit;
				}
				break;
			default:
				WL_ERR(("Unknown type: %d\n", type));
				ret = BCME_BADARG;
				goto exit;
		}
	}

	if (ip_pkt == NULL) {
		ret = BCME_BADARG;
		WL_ERR(("ip packet is NULL\n"));
		goto exit;
	}

	ret = wl_cfg80211_start_mkeep_alive(cfg, mkeep_alive_id,
		ether_type, ip_pkt, ip_pkt_len, src_mac, dst_mac, period_msec);
	if (ret < 0) {
		WL_ERR(("start_mkeep_alive is failed ret: %d\n", ret));
	}
#ifdef DHD_CLEANUP_KEEP_ALIVE
	else if (ret == BCME_OK) {
		setbit(&cfg->mkeep_alive_avail, mkeep_alive_id);
	}
#endif /* DHD_CLEANUP_KEEP_ALIVE */

exit:
	if (ip_pkt) {
		MFREE(cfg->osh, ip_pkt, ip_pkt_len);
	}

	return ret;
}

static int wl_cfgvendor_stop_mkeep_alive(struct wiphy *wiphy, struct wireless_dev *wdev,
	const void *data, int len)
{
	int ret = BCME_OK, rem, type;
	uint8 mkeep_alive_id = 0;
	const struct nlattr *iter;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);

	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		switch (type) {
			case MKEEP_ALIVE_ATTRIBUTE_ID:
				mkeep_alive_id = nla_get_u8(iter);
				break;
			default:
				WL_ERR(("Unknown type: %d\n", type));
				ret = BCME_BADARG;
				break;
		}
	}

	ret = wl_cfg80211_stop_mkeep_alive(cfg, mkeep_alive_id);
	if (ret < 0) {
		WL_ERR(("stop_mkeep_alive is failed ret: %d\n", ret));
	}
#ifdef DHD_CLEANUP_KEEP_ALIVE
	else if (ret == BCME_OK) {
		clrbit(&cfg->mkeep_alive_avail, mkeep_alive_id);
	}
#endif /* DHD_CLEANUP_KEEP_ALIVE */

	return ret;
}
#endif /* KEEP_ALIVE */

#if defined(APF)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
const struct nla_policy apf_atrribute_policy[APF_ATTRIBUTE_MAX] = {
	[APF_ATTRIBUTE_VERSION] = { .type = NLA_U32 },
	[APF_ATTRIBUTE_MAX_LEN] = { .type = NLA_U32 },
	[APF_ATTRIBUTE_PROGRAM] = { .type = NLA_BINARY },
	[APF_ATTRIBUTE_PROGRAM_LEN] = { .type = NLA_U32 },
};
#endif /* LINUX_VERSION >= 5.3 */

static int
wl_cfgvendor_apf_get_capabilities(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int len)
{
	struct net_device *ndev = wdev_to_ndev(wdev);
	struct sk_buff *skb = NULL;
	int ret, ver, max_len, mem_needed;

	/* APF version */
	ver = 0;
	ret = dhd_dev_apf_get_version(ndev, &ver);
	if (unlikely(ret)) {
		WL_ERR(("APF get version failed, ret=%d\n", ret));
		goto fail;
	}

	/* APF memory size limit */
	max_len = 0;
	ret = dhd_dev_apf_get_max_len(ndev, &max_len);
	if (unlikely(ret)) {
		WL_ERR(("APF get maximum length failed, ret=%d\n", ret));
		goto fail;
	}

	mem_needed = VENDOR_REPLY_OVERHEAD + (ATTRIBUTE_U32_LEN * 2);

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, mem_needed);
	if (unlikely(!skb)) {
		WL_ERR(("%s: can't allocate %d bytes\n", __FUNCTION__, mem_needed));
		ret = -ENOMEM;
		goto fail;
	}

	ret = nla_put_u32(skb, APF_ATTRIBUTE_VERSION, ver);
	if (ret < 0) {
		WL_ERR(("Failed to put APF_ATTRIBUTE_VERSION, ret:%d\n", ret));
		goto fail;
	}
	ret = nla_put_u32(skb, APF_ATTRIBUTE_MAX_LEN, max_len);
	if (ret < 0) {
		WL_ERR(("Failed to put APF_ATTRIBUTE_MAX_LEN, ret:%d\n", ret));
		goto fail;
	}

	ret = cfg80211_vendor_cmd_reply(skb);
	if (unlikely(ret)) {
		WL_ERR(("vendor command reply failed, ret=%d\n", ret));
	}

	return ret;

fail:
	if (skb) {
		/* Free skb memory */
		kfree_skb(skb);
	}

	return ret;
}

static int
wl_cfgvendor_apf_set_filter(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	struct net_device *ndev = wdev_to_ndev(wdev);
	const struct nlattr *iter;
	u8 *program = NULL;
	u32 program_len = 0;
	int ret, tmp, type, max_len;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);

	if (len <= 0) {
		WL_ERR(("Invalid len: %d\n", len));
		ret = -EINVAL;
		goto exit;
	}

	/* APF memory size limit */
	max_len = 0;
	ret = dhd_dev_apf_get_max_len(ndev, &max_len);
	if (unlikely(ret)) {
		WL_ERR(("APF get maximum length failed, ret=%d\n", ret));
		return ret;
	}

	nla_for_each_attr(iter, data, len, tmp) {
		type = nla_type(iter);
		switch (type) {
			case APF_ATTRIBUTE_PROGRAM_LEN:
				/* check if the iter value is valid and program_len
				 * is not already initialized.
				 */
				if (nla_len(iter) == sizeof(uint32) && !program_len) {
					program_len = nla_get_u32(iter);
				} else {
					ret = -EINVAL;
					goto exit;
				}

				if (program_len > max_len) {
					WL_ERR(("program len is more than expected len\n"));
					ret = -EINVAL;
					goto exit;
				}

				if (unlikely(!program_len)) {
					WL_ERR(("zero program length\n"));
					ret = -EINVAL;
					goto exit;
				}
				break;
			case APF_ATTRIBUTE_PROGRAM:
				if (unlikely(program)) {
					WL_ERR(("program already allocated\n"));
					ret = -EINVAL;
					goto exit;
				}
				if (unlikely(!program_len)) {
					WL_ERR(("program len is not set\n"));
					ret = -EINVAL;
					goto exit;
				}
				if (nla_len(iter) != program_len) {
					WL_ERR(("program_len is not same\n"));
					ret = -EINVAL;
					goto exit;
				}
				program = MALLOCZ(cfg->osh, program_len);
				if (unlikely(!program)) {
					WL_ERR(("%s: can't allocate %d bytes\n",
					      __FUNCTION__, program_len));
					ret = -ENOMEM;
					goto exit;
				}
				memcpy(program, (u8*)nla_data(iter), program_len);
				break;
			default:
				WL_ERR(("%s: no such attribute %d\n", __FUNCTION__, type));
				ret = -EINVAL;
				goto exit;
		}
	}

	ret = dhd_dev_apf_add_filter(ndev, program, program_len);

exit:
	if (program) {
		MFREE(cfg->osh, program, program_len);
	}
	return ret;
}

static int
wl_cfgvendor_apf_read_filter_data(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int len)
{
	struct net_device *ndev = wdev_to_ndev(wdev);
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct sk_buff *skb = NULL;
	uint8 *buf = NULL;
	int ret, buf_len, max_len, mem_needed;

	/* Get APF memory size limit */
	max_len = 0;
	ret = dhd_dev_apf_get_max_len(ndev, &max_len);
	if (unlikely(ret)) {
		WL_ERR(("APF get maximum length failed, ret=%d\n", ret));
		goto fail;
	}

	/* As the IOVar returns the data in 'wl_apf_program_t' structure format, account for this
	 * also along with the queried APF buffer size for the IOvar buffer.
	 */
	buf_len = WL_APF_PROGRAM_FIXED_LEN + max_len;

	/* Get APF filter data */
	buf = MALLOCZ(cfg->osh, buf_len);
	if (unlikely(!buf)) {
		WL_ERR(("%s: can't allocate %d bytes\n", __FUNCTION__, buf_len));
		ret = -ENOMEM;
		goto fail;
	}
	ret = dhd_dev_apf_read_filter_data(ndev, buf, buf_len);
	if (unlikely(ret)) {
		WL_ERR(("APF get filter data failed, ret=%d\n", ret));
		goto fail;
	}

	mem_needed = VENDOR_REPLY_OVERHEAD + ATTRIBUTE_U32_LEN + max_len;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, mem_needed);
	if (unlikely(!skb)) {
		WL_ERR(("%s: can't allocate %d bytes\n", __FUNCTION__, mem_needed));
		ret = -ENOMEM;
		goto fail;
	}

	ret = nla_put_u32(skb, APF_ATTRIBUTE_PROGRAM_LEN, max_len);
	if (ret < 0) {
		WL_ERR(("Failed to put APF_ATTRIBUTE_MAX_LEN, ret=%d\n", ret));
		goto fail;
	}

	/* Skip the initial fixed portion of 'wl_apf_program_t' and copy only the data portion */
	ret = nla_put(skb, APF_ATTRIBUTE_PROGRAM, max_len, (buf + WL_APF_PROGRAM_FIXED_LEN));
	if (ret < 0) {
		WL_ERR(("Failed to put APF_ATTRIBUTE_MAX_LEN, ret=%d\n", ret));
		goto fail;
	}

	ret = cfg80211_vendor_cmd_reply(skb);
	if (unlikely(ret)) {
		WL_ERR(("vendor command reply failed, ret=%d\n", ret));
	}

	if (buf) {
		MFREE(cfg->osh, buf, buf_len);
	}

	return ret;

fail:
	if (buf) {
		MFREE(cfg->osh, buf, buf_len);
	}

	if (skb) {
		/* Free skb memory */
		kfree_skb(skb);
	}

	return ret;
}
#endif /* APF */

#ifdef NDO_CONFIG_SUPPORT
static int wl_cfgvendor_configure_nd_offload(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	const struct nlattr *iter;
	int ret = BCME_OK, rem, type;
	u8 enable = 0;

	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		switch (type) {
			case ANDR_WIFI_ATTRIBUTE_ND_OFFLOAD_VALUE:
				enable = nla_get_u8(iter);
				break;
			default:
				WL_ERR(("Unknown type: %d\n", type));
				ret = BCME_BADARG;
				goto exit;
		}
	}

	ret = dhd_dev_ndo_cfg(bcmcfg_to_prmry_ndev(cfg), enable);
	if (ret < 0) {
		WL_ERR(("dhd_dev_ndo_cfg() failed: %d\n", ret));
	}

exit:
	return ret;
}
#endif /* NDO_CONFIG_SUPPORT */

#if !defined(BCMSUP_4WAY_HANDSHAKE) || (LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0))
static int wl_cfgvendor_set_pmk(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int len)
{
	int ret = 0;
	wsec_pmk_t pmk;
	const struct nlattr *iter;
	int rem, type;
	struct net_device *ndev = wdev_to_ndev(wdev);
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct wl_security *sec;

	bzero(&pmk, sizeof(pmk));
	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		switch (type) {
			case BRCM_ATTR_DRIVER_KEY_PMK:
				pmk.flags = 0;
				pmk.key_len = htod16(nla_len(iter));
				ret = memcpy_s(pmk.key, sizeof(pmk.key),
					(uint8 *)nla_data(iter), nla_len(iter));
				if (ret) {
					WL_ERR(("Failed to copy pmk: %d\n", ret));
					ret = -EINVAL;
					goto exit;
				}
				break;
			default:
				WL_ERR(("Unknown type: %d\n", type));
				ret = BCME_BADARG;
				goto exit;
		}
	}

	sec = wl_read_prof(cfg, ndev, WL_PROF_SEC);
	if ((sec->wpa_auth == WLAN_AKM_SUITE_8021X) ||
		(sec->wpa_auth == WL_AKM_SUITE_SHA256_1X)) {
		ret = wldev_iovar_setbuf(ndev, "okc_info_pmk", pmk.key, pmk.key_len, cfg->ioctl_buf,
			WLC_IOCTL_SMLEN,  &cfg->ioctl_buf_sync);
		if (ret) {
			/* could fail in case that 'okc' is not supported */
			WL_INFORM_MEM(("okc_info_pmk failed, err=%d (ignore)\n", ret));
		}
	}

	ret = wldev_ioctl_set(ndev, WLC_SET_WSEC_PMK, &pmk, sizeof(pmk));
	WL_INFORM_MEM(("IOVAR set_pmk ret:%d", ret));
exit:
	return ret;
}
#endif /* !BCMSUP_4WAY_HANDSHAKE || LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0) */

static int wl_cfgvendor_get_driver_feature(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int ret = BCME_OK;
	u8 supported[(BRCM_WLAN_VENDOR_FEATURES_MAX / 8) + 1] = {0};
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	dhd_pub_t *dhd_pub = cfg->pub;
	struct sk_buff *skb;
	int32 mem_needed;

	mem_needed = VENDOR_REPLY_OVERHEAD + NLA_HDRLEN + sizeof(supported);

	BCM_REFERENCE(dhd_pub);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0))
	if (FW_SUPPORTED(dhd_pub, idsup)) {
		ret = wl_features_set(supported, sizeof(supported),
				BRCM_WLAN_VENDOR_FEATURE_KEY_MGMT_OFFLOAD);
	}
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0) */

	/* Alloc the SKB for vendor_event */
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, mem_needed);
	if (unlikely(!skb)) {
		WL_ERR(("skb alloc failed"));
		ret = BCME_NOMEM;
		goto exit;
	}

	ret = nla_put(skb, BRCM_ATTR_DRIVER_FEATURE_FLAGS, sizeof(supported), supported);
	if (ret) {
		kfree_skb(skb);
		goto exit;
	}
	ret = cfg80211_vendor_cmd_reply(skb);
exit:
	return ret;
}

#ifdef WL_P2P_RAND
static int
wl_cfgvendor_set_p2p_rand_mac(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void  *data, int len)
{
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	int type;
	WL_DBG(("%s, wdev->iftype = %d\n", __FUNCTION__, wdev->iftype));
	WL_INFORM_MEM(("randomized p2p_dev_addr - "MACDBG"\n", MAC2STRDBG(nla_data(data))));

	BCM_REFERENCE(cfg);

	type = nla_type(data);

	if (type == BRCM_ATTR_DRIVER_RAND_MAC) {
		if (nla_len(data) != ETHER_ADDR_LEN) {
			WL_ERR(("nla_len not matched.\n"));
			err = -EINVAL;
			goto exit;
		}

		if (wdev->iftype != NL80211_IFTYPE_P2P_DEVICE) {
			WL_ERR(("wrong interface type , wdev->iftype=%d\n", wdev->iftype));
			err = -EINVAL;
			goto exit;
		}
		(void)memcpy_s(wl_to_p2p_bss_macaddr(cfg, P2PAPI_BSSCFG_DEVICE), ETHER_ADDR_LEN,
				nla_data(data), ETHER_ADDR_LEN);
		(void)memcpy_s(wdev->address, ETHER_ADDR_LEN, nla_data(data), ETHER_ADDR_LEN);

		err = wl_cfgp2p_disable_discovery(cfg);
		if (unlikely(err < 0)) {
			WL_ERR(("P2P disable discovery failed, ret=%d\n", err));
			goto exit;
		}

		err = wl_cfgp2p_set_firm_p2p(cfg);
		if (unlikely(err < 0)) {
			WL_ERR(("Set P2P address in firmware failed, ret=%d\n", err));
			goto exit;
		}

		err = wl_cfgp2p_enable_discovery(cfg, bcmcfg_to_prmry_ndev(cfg), NULL, 0);
		if (unlikely(err < 0)) {
			WL_ERR(("P2P enable discovery failed, ret=%d\n", err));
			goto exit;
		}
	} else {
		WL_ERR(("unexpected attrib type:%d\n", type));
		err = -EINVAL;
	}
exit:
	return err;
}
#endif /* WL_P2P_RAND */

#ifdef WL_THERMAL_MITIGATION
static int
wl_cfgvendor_thermal_mitigation(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = BCME_ERROR, rem, type;
	wifi_thermal_mode set_thermal_mode = WIFI_MITIGATION_NONE;
	struct net_device *net = wdev->netdev;
	struct bcm_cfg80211 *cfg = wl_get_cfg(net);
	const struct nlattr *iter;
	wl_tvpm_req_t* tvpm_req = NULL;
	size_t reqlen = sizeof(wl_tvpm_req_t) + sizeof(wl_tvpm_status_t);
	s32 bssidx;
	u32 duty_cycle = DUTY_CYCLE_NONE;
	/* delay_win
	 * Deadline (in milliseconds) to complete this request, value 0 implies apply
	 * immediately. Deadline is basically a relaxed limit and allows
	 * vendors to apply the mitigation within the window (if it cannot
	 * apply immediately)
	 * current chip can apply immediately, so will not use this value currently
	 */
	u32 delay_win = 0;

	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		if (type == ANDR_WIFI_ATTRIBUTE_THERMAL_MITIGATION) {
			set_thermal_mode = nla_get_u32(iter);
			WL_INFORM_MEM(("got thermal mode = %d\n", set_thermal_mode));
		} else if (type == ANDR_WIFI_ATTRIBUTE_THERMAL_COMPLETION_WINDOW) {
			delay_win = nla_get_u32(iter);
			WL_INFORM_MEM(("got delay_win = %d\n", delay_win));
		} else {
			WL_ERR(("Unknown attr type: %d\n", type));
			err = -EINVAL;
			goto exit;
		}
	}
	/* If thermal mode is already configured, no need to set it again */
	if (cfg->thermal_mode == set_thermal_mode) {
		WL_INFORM_MEM(("%s, thermal_mode %d is already set\n",
				__FUNCTION__, set_thermal_mode));
		err = BCME_OK;
		goto exit;
	}

	/* Map Android TX power modes to internal power mode */
	switch (set_thermal_mode) {
		case WIFI_MITIGATION_LIGHT:
			duty_cycle = DUTY_CYCLE_LIGHT;
			break;
		case WIFI_MITIGATION_MODERATE:
			duty_cycle = DUTY_CYCLE_MODERATE;
			break;
		case WIFI_MITIGATION_SEVERE:
			duty_cycle = DUTY_CYCLE_SEVERE;
			break;
		case WIFI_MITIGATION_CRITICAL:
			duty_cycle = DUTY_CYCLE_CRITICAL;
			break;
		case WIFI_MITIGATION_EMERGENCY:
			duty_cycle = DUTY_CYCLE_EMERGENCY;
			break;
		case WIFI_MITIGATION_NONE:
		/* intentional fall-through */
		default:
			duty_cycle = DUTY_CYCLE_NONE;
			break;
	}
	WL_DBG(("%s, duty_cycle %d thermal_mode %d\n", __FUNCTION__,
			duty_cycle, set_thermal_mode));

	tvpm_req = MALLOCZ(cfg->osh, reqlen);
	if (tvpm_req == NULL) {
		WL_ERR(("%s: can't allocate %ld bytes\n",
				__FUNCTION__, reqlen));
		err = -ENOMEM;
		goto exit;
	}
	tvpm_req->version = TVPM_REQ_CURRENT_VERSION;
	tvpm_req->length = reqlen;
	tvpm_req->req_type = WL_TVPM_REQ_CLTM_INDEX;
	*(int32*)(tvpm_req->value) = duty_cycle;

	if ((bssidx = wl_get_bssidx_by_wdev(cfg, net->ieee80211_ptr)) < 0) {
		WL_ERR(("Find index failed\n"));
		err = BCME_ERROR;
		goto exit;
	}

	err = wldev_iovar_setbuf_bsscfg(net, "tvpm",
		tvpm_req, reqlen, cfg->ioctl_buf,
		WLC_IOCTL_MEDLEN, bssidx, &cfg->ioctl_buf_sync);
	if (unlikely(err)) {
		WL_ERR(("tvpm duty cycle failed with error %d\n", err));
		goto exit;
	}

	/* Cache the thermal mode sent by the hal */
	cfg->thermal_mode = set_thermal_mode;

exit:
	if (tvpm_req) {
		MFREE(cfg->osh, tvpm_req, reqlen);
	}

	return err;
}
#endif /* WL_THERMAL_MITIGATION */

#ifdef WL_SAR_TX_POWER
static int
wl_cfgvendor_tx_power_scenario(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void  *data, int len)
{
	int err = BCME_ERROR, rem, type;
	wifi_power_scenario wifi_tx_power_mode = WIFI_POWER_SCENARIO_INVALID;
	struct bcm_cfg80211 *cfg = wl_get_cfg(wdev_to_ndev(wdev));
	const struct nlattr *iter;
	sar_advance_modes sar_tx_power_val = SAR_DISABLE;
	int airplane_mode = 0;

	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		if (type == ANDR_WIFI_ATTRIBUTE_TX_POWER_SCENARIO) {
			wifi_tx_power_mode = nla_get_s8(iter);
		} else {
			WL_ERR(("Unknown attr type: %d\n", type));
			err = -EINVAL;
			goto exit;
		}
	}
	/* If sar tx power is already configured, no need to set it again */
	if (cfg->wifi_tx_power_mode == wifi_tx_power_mode) {
		WL_INFORM_MEM(("%s, tx_power_mode %d is already set\n",
			__FUNCTION__, wifi_tx_power_mode));
		err = BCME_OK;
		goto exit;
	}

	/* Map Android TX power modes to Brcm power mode */
	switch (wifi_tx_power_mode) {
		case WIFI_POWER_SCENARIO_VOICE_CALL:
		case WIFI_POWER_SCENARIO_DEFAULT:
			/* SAR disabled */
			sar_tx_power_val = SAR_DISABLE;
			airplane_mode = 0;
			break;
		case WIFI_POWER_SCENARIO_ON_HEAD_CELL_OFF:
			/* HEAD mode, Airplane */
			sar_tx_power_val = SAR_HEAD;
			airplane_mode = 1;
			break;
		case WIFI_POWER_SCENARIO_ON_BODY_CELL_OFF:
			/* GRIP mode, Airplane */
			sar_tx_power_val = SAR_GRIP;
			airplane_mode = 1;
			break;
		case WIFI_POWER_SCENARIO_ON_BODY_BT:
			/* BT mode, Airplane */
			sar_tx_power_val = SAR_BT;
			airplane_mode = 1;
			break;
		case WIFI_POWER_SCENARIO_ON_HEAD_CELL_ON:
			/* HEAD mode, Normal */
			sar_tx_power_val = SAR_HEAD;
			airplane_mode = 0;
			break;
		case WIFI_POWER_SCENARIO_ON_BODY_CELL_ON:
			/* GRIP mode, Normal */
			sar_tx_power_val = SAR_GRIP;
			airplane_mode = 0;
			break;
		default:
			WL_ERR(("invalid wifi tx power scenario = %d\n",
				sar_tx_power_val));
			err = -EINVAL;
			goto exit;
	}

	WL_DBG(("%s, sar_mode %d airplane_mode %d\n", __FUNCTION__,
		sar_tx_power_val, airplane_mode));
	err = wldev_iovar_setint(wdev_to_ndev(wdev), "fccpwrlimit2g", airplane_mode);
	if (unlikely(err)) {
		WL_ERR(("%s: Failed to set airplane_mode - error (%d)\n", __FUNCTION__, err));
		goto exit;
	}
	err = wldev_iovar_setint(wdev_to_ndev(wdev), "sar_enable", sar_tx_power_val);
	if (unlikely(err)) {
		WL_ERR(("%s: Failed to set sar_enable - error (%d)\n", __FUNCTION__, err));
		goto exit;
	}
	/* Cache the tx power mode sent by the hal */
	cfg->wifi_tx_power_mode = wifi_tx_power_mode;
exit:
	return err;
}
#endif /* WL_SAR_TX_POWER */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
const struct nla_policy andr_wifi_attr_policy[ANDR_WIFI_ATTRIBUTE_MAX] = {
	[ANDR_WIFI_ATTRIBUTE_NUM_FEATURE_SET] = { .type = NLA_U32 },
	[ANDR_WIFI_ATTRIBUTE_FEATURE_SET] = { .type = NLA_U32 },
	[ANDR_WIFI_ATTRIBUTE_RANDOM_MAC_OUI] = { .type = NLA_NUL_STRING, .len = 3 },
	[ANDR_WIFI_ATTRIBUTE_NODFS_SET] = { .type = NLA_U32 },
	[ANDR_WIFI_ATTRIBUTE_COUNTRY] = { .type = NLA_NUL_STRING, .len = 3 },
	[ANDR_WIFI_ATTRIBUTE_ND_OFFLOAD_VALUE] = { .type = NLA_U8 },
	[ANDR_WIFI_ATTRIBUTE_TCPACK_SUP_VALUE] = { .type = NLA_U32 },
	[ANDR_WIFI_ATTRIBUTE_LATENCY_MODE] = { .type = NLA_U32 },
	/* Dont see ANDR_WIFI_ATTRIBUTE_RANDOM_MAC being used currently */
	[ANDR_WIFI_ATTRIBUTE_RANDOM_MAC] = { .type = NLA_U32 },
	[ANDR_WIFI_ATTRIBUTE_TX_POWER_SCENARIO] = { .type = NLA_S8 },
	[ANDR_WIFI_ATTRIBUTE_THERMAL_MITIGATION] = { .type = NLA_S8 },
	[ANDR_WIFI_ATTRIBUTE_THERMAL_COMPLETION_WINDOW] = { .type = NLA_U32 },
};

const struct nla_policy dump_buf_policy[DUMP_BUF_ATTR_MAX] = {
	[DUMP_BUF_ATTR_MEMDUMP] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_SSSR_C0_D11_BEFORE] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_SSSR_C0_D11_AFTER] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_SSSR_C1_D11_BEFORE] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_SSSR_C1_D11_AFTER] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_SSSR_C2_D11_BEFORE] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_SSSR_C2_D11_AFTER] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_SSSR_DIG_BEFORE] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_SSSR_DIG_AFTER] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_TIMESTAMP] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_GENERAL_LOG] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_ECNTRS] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_SPECIAL_LOG] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_DHD_DUMP] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_EXT_TRAP] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_HEALTH_CHK] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_PRESERVE_LOG] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_COOKIE] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_FLOWRING_DUMP] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_PKTLOG] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_PKTLOG_DEBUG] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_STATUS_LOG] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_AXI_ERROR] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_RTT_LOG] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_PKTID_MAP_LOG] = { .type = NLA_BINARY },
	[DUMP_BUF_ATTR_PKTID_UNMAP_LOG] = { .type = NLA_BINARY },
};

const struct nla_policy andr_dbg_policy[DEBUG_ATTRIBUTE_MAX] = {
	[DEBUG_ATTRIBUTE_GET_DRIVER] = { .type = NLA_BINARY },
	[DEBUG_ATTRIBUTE_GET_FW] = { .type = NLA_BINARY },
	[DEBUG_ATTRIBUTE_RING_ID] = { .type = NLA_U32 },
	[DEBUG_ATTRIBUTE_RING_NAME] = { .type = NLA_NUL_STRING },
	[DEBUG_ATTRIBUTE_RING_FLAGS] = { .type = NLA_U32 },
	[DEBUG_ATTRIBUTE_LOG_LEVEL] = { .type = NLA_U32 },
	[DEBUG_ATTRIBUTE_LOG_TIME_INTVAL] = { .type = NLA_U32 },
	[DEBUG_ATTRIBUTE_LOG_MIN_DATA_SIZE] = { .type = NLA_U32 },
	[DEBUG_ATTRIBUTE_FW_DUMP_LEN] = { .type = NLA_U32 },
	[DEBUG_ATTRIBUTE_FW_DUMP_DATA] = { .type = NLA_U64 },
	[DEBUG_ATTRIBUTE_FW_ERR_CODE] = { .type = NLA_U32 },
	[DEBUG_ATTRIBUTE_RING_DATA] = { .type = NLA_BINARY },
	[DEBUG_ATTRIBUTE_RING_STATUS] = { .type = NLA_BINARY },
	[DEBUG_ATTRIBUTE_RING_NUM] = { .type = NLA_U32 },
	[DEBUG_ATTRIBUTE_DRIVER_DUMP_LEN] = { .type = NLA_U32 },
	[DEBUG_ATTRIBUTE_DRIVER_DUMP_DATA] = { .type = NLA_BINARY },
	[DEBUG_ATTRIBUTE_PKT_FATE_NUM] = { .type = NLA_U32 },
	[DEBUG_ATTRIBUTE_PKT_FATE_DATA] = { .type = NLA_U64 },
	[DEBUG_ATTRIBUTE_HANG_REASON] = { .type = NLA_BINARY },
};

const struct nla_policy brcm_drv_attr_policy[BRCM_ATTR_DRIVER_MAX] = {
	[BRCM_ATTR_DRIVER_CMD] = { .type = NLA_NUL_STRING },
	[BRCM_ATTR_DRIVER_KEY_PMK] = { .type = NLA_BINARY, .len = WSEC_MAX_PASSPHRASE_LEN },
	[BRCM_ATTR_DRIVER_FEATURE_FLAGS] = { .type = NLA_BINARY, .len =
	((BRCM_WLAN_VENDOR_FEATURES_MAX / 8) + 1) },
	[BRCM_ATTR_DRIVER_RAND_MAC] = { .type = NLA_BINARY, .len = ETHER_ADDR_LEN },
	[BRCM_ATTR_SAE_PWE] = { .type = NLA_U32 },
};

const struct nla_policy rtt_attr_policy[RTT_ATTRIBUTE_MAX] = {
	[RTT_ATTRIBUTE_TARGET_CNT] = { .type = NLA_U8 },
	[RTT_ATTRIBUTE_TARGET_INFO] = { .type = NLA_NESTED },
	[RTT_ATTRIBUTE_TARGET_MAC] = { .type = NLA_BINARY, .len = ETHER_ADDR_LEN },
	[RTT_ATTRIBUTE_TARGET_TYPE] = { .type = NLA_U8 },
	[RTT_ATTRIBUTE_TARGET_PEER] = { .type = NLA_U8 },
	[RTT_ATTRIBUTE_TARGET_CHAN] = { .type = NLA_BINARY },
	[RTT_ATTRIBUTE_TARGET_PERIOD] = { .type = NLA_U32 },
	[RTT_ATTRIBUTE_TARGET_NUM_BURST] = { .type = NLA_U32 },
	[RTT_ATTRIBUTE_TARGET_NUM_FTM_BURST] = { .type = NLA_U32 },
	[RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTM] = { .type = NLA_U32 },
	[RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTMR] = { .type = NLA_U32 },
	[RTT_ATTRIBUTE_TARGET_LCI] = { .type = NLA_U8 },
	[RTT_ATTRIBUTE_TARGET_LCR] = { .type = NLA_U8 },
	[RTT_ATTRIBUTE_TARGET_BURST_DURATION] = { .type = NLA_U32 },
	[RTT_ATTRIBUTE_TARGET_PREAMBLE] = { .type = NLA_U8 },
	[RTT_ATTRIBUTE_TARGET_BW] = { .type = NLA_U8 },
	[RTT_ATTRIBUTE_RESULTS_COMPLETE] = { .type = NLA_U32 },
	[RTT_ATTRIBUTE_RESULTS_PER_TARGET] = { .type = NLA_NESTED },
	[RTT_ATTRIBUTE_RESULT_CNT] = { .type = NLA_U32 },
	[RTT_ATTRIBUTE_RESULT] = { .type = NLA_BINARY, .len = sizeof(rtt_result_t) },
	[RTT_ATTRIBUTE_RESULT_DETAIL] = { .type = NLA_BINARY,
	.len = sizeof(struct rtt_result_detail) },
};

#ifdef RSSI_MONITOR_SUPPORT
const struct nla_policy rssi_monitor_attr_policy[RSSI_MONITOR_ATTRIBUTE_MAX] = {
	[RSSI_MONITOR_ATTRIBUTE_MAX_RSSI] = { .type = NLA_U32 },
	[RSSI_MONITOR_ATTRIBUTE_MIN_RSSI] = { .type = NLA_U32 },
	[RSSI_MONITOR_ATTRIBUTE_START] = { .type = NLA_U32 }
};
#endif /* RSSI_MONITOR_SUPPORT */

#ifdef KEEP_ALIVE
const struct nla_policy mkeep_alive_attr_policy[MKEEP_ALIVE_ATTRIBUTE_MAX] = {
	[MKEEP_ALIVE_ATTRIBUTE_ID] = { .type = NLA_U8 },
	[MKEEP_ALIVE_ATTRIBUTE_IP_PKT] = { .type = NLA_BINARY, .len = MKEEP_ALIVE_IP_PKT_MAX },
	[MKEEP_ALIVE_ATTRIBUTE_IP_PKT_LEN] = { .type = NLA_U16 },
	[MKEEP_ALIVE_ATTRIBUTE_SRC_MAC_ADDR] = { .type = NLA_BINARY, .len = ETHER_ADDR_LEN },
	[MKEEP_ALIVE_ATTRIBUTE_DST_MAC_ADDR] = { .type = NLA_BINARY, .len = ETHER_ADDR_LEN },
	[MKEEP_ALIVE_ATTRIBUTE_PERIOD_MSEC] = { .type = NLA_U32 },
	[MKEEP_ALIVE_ATTRIBUTE_ETHER_TYPE] = { .type = NLA_U16 }
};
#endif /* KEEP_ALIVE */
#ifdef WL_NAN
const struct nla_policy nan_attr_policy[NAN_ATTRIBUTE_MAX] = {
	[NAN_ATTRIBUTE_2G_SUPPORT] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_5G_SUPPORT] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_CLUSTER_LOW] = { .type = NLA_U16, .len = sizeof(uint16) },
	[NAN_ATTRIBUTE_CLUSTER_HIGH] = { .type = NLA_U16, .len = sizeof(uint16) },
	[NAN_ATTRIBUTE_SID_BEACON] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_SUB_SID_BEACON] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_SYNC_DISC_2G_BEACON] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_SYNC_DISC_5G_BEACON] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_SDF_2G_SUPPORT] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_SDF_5G_SUPPORT] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_HOP_COUNT_LIMIT] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_RANDOM_TIME] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_MASTER_PREF] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_OUI] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_WARMUP_TIME] = { .type = NLA_U16, .len = sizeof(uint16) },
	[NAN_ATTRIBUTE_CHANNEL] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_24G_CHANNEL] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_5G_CHANNEL] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_CONF_CLUSTER_VAL] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_DWELL_TIME] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_SCAN_PERIOD] = { .type = NLA_U16, .len = sizeof(uint16) },
	[NAN_ATTRIBUTE_DWELL_TIME_5G] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_SCAN_PERIOD_5G] = { .type = NLA_U16, .len = sizeof(uint16) },
	[NAN_ATTRIBUTE_AVAIL_BIT_MAP] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_ENTRY_CONTROL] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_RSSI_CLOSE] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_RSSI_MIDDLE] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_RSSI_PROXIMITY] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_RSSI_CLOSE_5G] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_RSSI_MIDDLE_5G] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_RSSI_PROXIMITY_5G] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_RSSI_WINDOW_SIZE] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_CIPHER_SUITE_TYPE] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_SCID_LEN] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_SCID] = { .type = NLA_BINARY, .len = MAX_SCID_LEN },
	[NAN_ATTRIBUTE_2G_AWAKE_DW] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_5G_AWAKE_DW] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_DISC_IND_CFG] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_MAC_ADDR] = { .type = NLA_BINARY, .len = ETHER_ADDR_LEN },
	[NAN_ATTRIBUTE_RANDOMIZATION_INTERVAL] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_CMD_USE_NDPE] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_ENABLE_MERGE] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_DISCOVERY_BEACON_INTERVAL] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_NSS] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_ENABLE_RANGING] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_DW_EARLY_TERM] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_TRANSAC_ID] = { .type = NLA_U16, .len = sizeof(uint16) },
	[NAN_ATTRIBUTE_PUBLISH_ID] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_SERVICE_SPECIFIC_INFO_LEN] = { .type = NLA_U16, .len = sizeof(uint16) },
	[NAN_ATTRIBUTE_SERVICE_SPECIFIC_INFO] = { .type = NLA_BINARY, .len =
	NAN_MAX_SERVICE_SPECIFIC_INFO_LEN },
	[NAN_ATTRIBUTE_SUBSCRIBE_ID] = { .type = NLA_U16, .len = sizeof(uint16) },
	[NAN_ATTRIBUTE_SUBSCRIBE_TYPE] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_PUBLISH_COUNT] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_PUBLISH_TYPE] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_PERIOD] = { .type = NLA_U16, .len = sizeof(uint16) },
	[NAN_ATTRIBUTE_TTL] = { .type = NLA_U16, .len = sizeof(uint16) },
	[NAN_ATTRIBUTE_SERVICE_NAME_LEN] = { .type = NLA_U16, .len = sizeof(uint16) },
	[NAN_ATTRIBUTE_SERVICE_NAME] = { .type = NLA_BINARY, .len = WL_NAN_SVC_HASH_LEN },
	[NAN_ATTRIBUTE_PEER_ID] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_INST_ID] = { .type = NLA_U16, .len = sizeof(uint16) },
	[NAN_ATTRIBUTE_SUBSCRIBE_COUNT] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_SSIREQUIREDFORMATCHINDICATION] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_SUBSCRIBE_MATCH] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_PUBLISH_MATCH] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_SERVICERESPONSEFILTER] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_SERVICERESPONSEINCLUDE] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_USESERVICERESPONSEFILTER] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_RX_MATCH_FILTER_LEN] = { .type = NLA_U16, .len = sizeof(uint16) },
	[NAN_ATTRIBUTE_RX_MATCH_FILTER] = { .type = NLA_BINARY, .len = MAX_MATCH_FILTER_LEN },
	[NAN_ATTRIBUTE_TX_MATCH_FILTER_LEN] = { .type = NLA_U16, .len = sizeof(uint16) },
	[NAN_ATTRIBUTE_TX_MATCH_FILTER] = { .type = NLA_BINARY, .len = MAX_MATCH_FILTER_LEN },
	[NAN_ATTRIBUTE_MAC_ADDR_LIST_NUM_ENTRIES] = { .type = NLA_U16, .len = sizeof(uint16) },
	[NAN_ATTRIBUTE_MAC_ADDR_LIST] = { .type = NLA_BINARY, .len =
	(NAN_SRF_MAX_MAC*ETHER_ADDR_LEN) },
	[NAN_ATTRIBUTE_TX_TYPE] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_SDE_CONTROL_CONFIG_DP] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_SDE_CONTROL_RANGE_SUPPORT] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_SDE_CONTROL_DP_TYPE] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_SDE_CONTROL_SECURITY] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_RECV_IND_CFG] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_KEY_TYPE] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_KEY_LEN] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_KEY_DATA] = { .type = NLA_BINARY, .len = NAN_MAX_PMK_LEN },
	[NAN_ATTRIBUTE_RSSI_THRESHOLD_FLAG] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_SDEA_SERVICE_SPECIFIC_INFO_LEN] = { .type = NLA_U16, .len =
	sizeof(uint16) },
	[NAN_ATTRIBUTE_SDEA_SERVICE_SPECIFIC_INFO] = { .type = NLA_BINARY, .len =
	MAX_SDEA_SVC_INFO_LEN },
	[NAN_ATTRIBUTE_SECURITY] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_RANGING_INTERVAL] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_RANGING_INGRESS_LIMIT] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_RANGING_EGRESS_LIMIT] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_RANGING_INDICATION] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_SVC_RESPONDER_POLICY] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_NDP_ID] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_IFACE] = { .type = NLA_BINARY, .len = IFNAMSIZ+1 },
	[NAN_ATTRIBUTE_QOS] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_RSP_CODE] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_INST_COUNT] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_PEER_DISC_MAC_ADDR] = { .type = NLA_BINARY, .len = ETHER_ADDR_LEN },
	[NAN_ATTRIBUTE_PEER_NDI_MAC_ADDR] = { .type = NLA_BINARY, .len = ETHER_ADDR_LEN },
	[NAN_ATTRIBUTE_IF_ADDR] = { .type = NLA_BINARY, .len = ETHER_ADDR_LEN },
	[NAN_ATTRIBUTE_ENTRY_CONTROL] = { .type = NLA_U8, .len = sizeof(uint8) },
	[NAN_ATTRIBUTE_AVAIL_BIT_MAP] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_CHANNEL] = { .type = NLA_U32, .len = sizeof(uint32) },
	[NAN_ATTRIBUTE_NO_CONFIG_AVAIL] = { .type = NLA_U8, .len = sizeof(uint8) },
};
#endif /* WL_NAN */

const struct nla_policy gscan_attr_policy[GSCAN_ATTRIBUTE_MAX] = {
	[GSCAN_ATTRIBUTE_BAND] = { .type = NLA_U32 },
	[GSCAN_ATTRIBUTE_NUM_CHANNELS] = { .type = NLA_U32 },
	[GSCAN_ATTRIBUTE_CHANNEL_LIST] = { .type = NLA_BINARY },
	[GSCAN_ATTRIBUTE_WHITELIST_SSID] = { .type = NLA_BINARY, .len = IEEE80211_MAX_SSID_LEN },
	[GSCAN_ATTRIBUTE_NUM_WL_SSID] = { .type = NLA_U32 },
	[GSCAN_ATTRIBUTE_WL_SSID_LEN] = { .type = NLA_U32 },
	[GSCAN_ATTRIBUTE_WL_SSID_FLUSH] = { .type = NLA_U32 },
	[GSCAN_ATTRIBUTE_WHITELIST_SSID_ELEM] = { .type = NLA_NESTED },
	/* length is sizeof(wl_ssid_whitelist_t) * MAX_SSID_WHITELIST_NUM */
	[GSCAN_ATTRIBUTE_NUM_BSSID] = { .type = NLA_U32 },
	[GSCAN_ATTRIBUTE_BSSID_PREF_LIST] = { .type = NLA_NESTED },
	/* length is sizeof(wl_bssid_pref_list_t) * MAX_BSSID_PREF_LIST_NUM */
	[GSCAN_ATTRIBUTE_BSSID_PREF_FLUSH] = { .type = NLA_U32 },
	[GSCAN_ATTRIBUTE_BSSID_PREF] = { .type = NLA_BINARY, .len = ETH_ALEN },
	[GSCAN_ATTRIBUTE_RSSI_MODIFIER] = { .type = NLA_U32 },
	[GSCAN_ATTRIBUTE_BSSID_BLACKLIST_FLUSH] = { .type = NLA_U32 },
	[GSCAN_ATTRIBUTE_BLACKLIST_BSSID] = { .type = NLA_BINARY, .len = ETH_ALEN },
	[GSCAN_ATTRIBUTE_ROAM_STATE_SET] = { .type = NLA_U32 },
};

const struct nla_policy wake_stat_attr_policy[WAKE_STAT_ATTRIBUTE_MAX] = {
	[WAKE_STAT_ATTRIBUTE_TOTAL_CMD_EVENT] = { .type = NLA_U32 },
#ifdef CUSTOM_WAKE_REASON_STATS
	[WAKE_STAT_ATTRIBUTE_CMD_EVENT_WAKE] = { .type = NLA_BINARY,
	.len = (MAX_WAKE_REASON_STATS * sizeof(int))},
#else
	[WAKE_STAT_ATTRIBUTE_CMD_EVENT_WAKE] = { .type = NLA_BINARY,
	.len = (WLC_E_LAST * sizeof(uint))},
#endif /* CUSTOM_WAKE_REASON_STATS */
	[WAKE_STAT_ATTRIBUTE_CMD_EVENT_COUNT] = { .type = NLA_U32 },
	[WAKE_STAT_ATTRIBUTE_CMD_EVENT_COUNT_USED] = { .type = NLA_U32 },
	[WAKE_STAT_ATTRIBUTE_TOTAL_DRIVER_FW] = { .type = NLA_U32 },
	[WAKE_STAT_ATTRIBUTE_DRIVER_FW_WAKE] = { .type = NLA_U32 },
	[WAKE_STAT_ATTRIBUTE_DRIVER_FW_COUNT] = { .type = NLA_U32 },
	[WAKE_STAT_ATTRIBUTE_DRIVER_FW_COUNT_USED] = { .type = NLA_U32 },
	[WAKE_STAT_ATTRIBUTE_TOTAL_RX_DATA_WAKE] = { .type = NLA_U32 },
	[WAKE_STAT_ATTRIBUTE_RX_UNICAST_COUNT] = { .type = NLA_U32 },
	[WAKE_STAT_ATTRIBUTE_RX_MULTICAST_COUNT] = { .type = NLA_U32 },
	[WAKE_STAT_ATTRIBUTE_RX_BROADCAST_COUNT] = { .type = NLA_U32 },
	[WAKE_STAT_ATTRIBUTE_RX_ICMP_PKT] = { .type = NLA_U32 },
	[WAKE_STAT_ATTRIBUTE_RX_ICMP6_PKT] = { .type = NLA_U32 },
	[WAKE_STAT_ATTRIBUTE_RX_ICMP6_RA] = { .type = NLA_U32 },
	[WAKE_STAT_ATTRIBUTE_RX_ICMP6_NA] = { .type = NLA_U32 },
	[WAKE_STAT_ATTRIBUTE_RX_ICMP6_NS] = { .type = NLA_U32 },
	[WAKE_STAT_ATTRIBUTE_IPV4_RX_MULTICAST_ADD_CNT] = { .type = NLA_U32 },
	[WAKE_STAT_ATTRIBUTE_IPV6_RX_MULTICAST_ADD_CNT] = { .type = NLA_U32 },
	[WAKE_STAT_ATTRIBUTE_OTHER_RX_MULTICAST_ADD_CNT] = { .type = NLA_U32 },
};

#ifdef WL_SOFTAP_ACS
const struct nla_policy acs_attr_policy[BRCM_VENDOR_ATTR_ACS_LAST] = {
	[BRCM_VENDOR_ATTR_ACS_CHANNEL_INVALID] = { .type = NLA_U8 },
	[BRCM_VENDOR_ATTR_ACS_PRIMARY_FREQ] = { .type = NLA_U32 },
	[BRCM_VENDOR_ATTR_ACS_SECONDARY_FREQ] = { .type = NLA_U32 },
	[BRCM_VENDOR_ATTR_ACS_VHT_SEG0_CENTER_CHANNEL] = { .type = NLA_U8 },
	[BRCM_VENDOR_ATTR_ACS_VHT_SEG1_CENTER_CHANNEL] = { .type = NLA_U8 },
	[BRCM_VENDOR_ATTR_ACS_HW_MODE] = { .type = NLA_U8 },
	[BRCM_VENDOR_ATTR_ACS_HT_ENABLED] = { .type = NLA_U8 },
	[BRCM_VENDOR_ATTR_ACS_HT40_ENABLED] = { .type = NLA_U8 },
	[BRCM_VENDOR_ATTR_ACS_VHT_ENABLED] = { .type = NLA_U8 },
	[BRCM_VENDOR_ATTR_ACS_CHWIDTH] = { .type = NLA_U16 },
	[BRCM_VENDOR_ATTR_ACS_CH_LIST] = { .type = NLA_BINARY },
	[BRCM_VENDOR_ATTR_ACS_FREQ_LIST] = { .type = NLA_BINARY },
};
#endif /* WL_SOFTAP_ACS */

const struct nla_policy hal_start_attr_policy[SET_HAL_START_ATTRIBUTE_MAX] = {
	[0] = { .strict_start_type = 0 },
	[SET_HAL_START_ATTRIBUTE_DEINIT] = { .type = NLA_UNSPEC },
	[SET_HAL_START_ATTRIBUTE_PRE_INIT] = { .type = NLA_NUL_STRING },
	[SET_HAL_START_ATTRIBUTE_EVENT_SOCK_PID] = { .type = NLA_U32 },
};
#ifdef TPUT_DEBUG_DUMP
const struct nla_policy tput_debug_dump_attr_policy[TPUT_DEBUG_ATTRIBUTE_MAX] = {
	[0] = { .strict_start_type = 0 },
	[TPUT_DEBUG_ATTRIBUTE_CMD_STR ] = { .type = NLA_NUL_STRING },
	[TPUT_DEBUG_ATTRIBUTE_SUB_CMD_STR_AMPDU] = { .type = NLA_NUL_STRING },
	[TPUT_DEBUG_ATTRIBUTE_SUB_CMD_STR_CLEAR] = { .type = NLA_NUL_STRING },
};
#endif /* TPUT_DEBUG_DUMP */
#endif /* LINUX_VERSION >= 5.3 */

static struct wiphy_vendor_command wl_vendor_cmds [] = {
	{
		{
			.vendor_id = OUI_BRCM,
			.subcmd = BRCM_VENDOR_SCMD_PRIV_STR
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_priv_string_handler,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = brcm_drv_attr_policy,
		.maxattr = BRCM_ATTR_DRIVER_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#ifdef BCM_PRIV_CMD_SUPPORT
	{
		{
			.vendor_id = OUI_BRCM,
			.subcmd = BRCM_VENDOR_SCMD_BCM_STR
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_priv_bcm_handler,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = brcm_drv_attr_policy,
		.maxattr = BRCM_ATTR_DRIVER_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#endif /* BCM_PRIV_CMD_SUPPORT */
#if defined(WL_SAE) || defined(WL_CLIENT_SAE)
	{
		{
			.vendor_id = OUI_BRCM,
			.subcmd = BRCM_VENDOR_SCMD_BCM_PSK
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_set_sae_password
	},
#endif /* WL_SAE || WL_CLIENT_SAE */
	{
		{
			.vendor_id = OUI_BRCM,
			.subcmd = BRCM_VENDOR_SCMD_SET_CONNECT_PARAMS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_connect_params_handler,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = brcm_drv_attr_policy,
		.maxattr = BRCM_ATTR_DRIVER_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_BRCM,
			.subcmd = BRCM_VENDOR_SCMD_SET_START_AP_PARAMS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_start_ap_params_handler,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = brcm_drv_attr_policy,
		.maxattr = BRCM_ATTR_DRIVER_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#ifdef GSCAN_SUPPORT
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = GSCAN_SUBCMD_GET_CAPABILITIES
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_gscan_get_capabilities
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = GSCAN_SUBCMD_SET_CONFIG
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_set_scan_cfg
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = GSCAN_SUBCMD_SET_SCAN_CONFIG
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_set_batch_scan_cfg
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = GSCAN_SUBCMD_ENABLE_GSCAN
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_initiate_gscan
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = GSCAN_SUBCMD_ENABLE_FULL_SCAN_RESULTS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_enable_full_scan_result
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = GSCAN_SUBCMD_SET_HOTLIST
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_hotlist_cfg
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = GSCAN_SUBCMD_GET_SCAN_RESULTS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_gscan_get_batch_results
	},
#endif /* GSCAN_SUPPORT */
#if defined(GSCAN_SUPPORT) || defined(DHD_GET_VALID_CHANNELS)
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = GSCAN_SUBCMD_GET_CHANNEL_LIST
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_gscan_get_channel_list,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = gscan_attr_policy,
		.maxattr = GSCAN_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#endif /* GSCAN_SUPPORT || DHD_GET_VALID_CHANNELS */
#ifdef RTT_SUPPORT
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = RTT_SUBCMD_SET_CONFIG
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_rtt_set_config,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = rtt_attr_policy,
		.maxattr = RTT_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = RTT_SUBCMD_CANCEL_CONFIG
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_rtt_cancel_config,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = rtt_attr_policy,
		.maxattr = RTT_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = RTT_SUBCMD_GETCAPABILITY
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_rtt_get_capability,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = rtt_attr_policy,
		.maxattr = RTT_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = RTT_SUBCMD_GETAVAILCHANNEL
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_rtt_get_responder_info,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = rtt_attr_policy,
		.maxattr = RTT_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = RTT_SUBCMD_SET_RESPONDER
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_rtt_set_responder,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = rtt_attr_policy,
		.maxattr = RTT_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = RTT_SUBCMD_CANCEL_RESPONDER
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_rtt_cancel_responder,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = rtt_attr_policy,
		.maxattr = RTT_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#endif /* RTT_SUPPORT */
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = ANDR_WIFI_SUBCMD_GET_FEATURE_SET
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_get_feature_set,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = andr_wifi_attr_policy,
		.maxattr = ANDR_WIFI_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = ANDR_WIFI_SUBCMD_GET_FEATURE_SET_MATRIX
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_get_feature_set_matrix,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = andr_wifi_attr_policy,
		.maxattr = ANDR_WIFI_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = ANDR_WIFI_RANDOM_MAC_OUI
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_set_rand_mac_oui,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = andr_wifi_attr_policy,
		.maxattr = ANDR_WIFI_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#ifdef CUSTOM_FORCE_NODFS_FLAG
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = ANDR_WIFI_NODFS_CHANNELS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_set_nodfs_flag,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = andr_wifi_attr_policy,
		.maxattr = ANDR_WIFI_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */

	},
#endif /* CUSTOM_FORCE_NODFS_FLAG */
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = ANDR_WIFI_SET_COUNTRY
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_set_country,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = andr_wifi_attr_policy,
		.maxattr = ANDR_WIFI_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#ifdef LINKSTAT_SUPPORT
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = LSTATS_SUBCMD_GET_INFO
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_lstats_get_info
	},
#endif /* LINKSTAT_SUPPORT */

#ifdef GSCAN_SUPPORT
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = GSCAN_SUBCMD_SET_EPNO_SSID
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_epno_cfg

	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = WIFI_SUBCMD_SET_LAZY_ROAM_PARAMS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_set_lazy_roam_cfg

	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = WIFI_SUBCMD_ENABLE_LAZY_ROAM
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_enable_lazy_roam

	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = WIFI_SUBCMD_SET_BSSID_PREF
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_set_bssid_pref

	},
#endif /* GSCAN_SUPPORT */
#if defined(GSCAN_SUPPORT) || defined(ROAMEXP_SUPPORT)
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = WIFI_SUBCMD_SET_SSID_WHITELIST
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_set_ssid_whitelist,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = gscan_attr_policy,
		.maxattr = GSCAN_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */

	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = WIFI_SUBCMD_SET_BSSID_BLACKLIST
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_set_bssid_blacklist,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = gscan_attr_policy,
		.maxattr = GSCAN_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#endif /* GSCAN_SUPPORT || ROAMEXP_SUPPORT */
#ifdef ROAMEXP_SUPPORT
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = WIFI_SUBCMD_FW_ROAM_POLICY
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_set_fw_roaming_state,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = gscan_attr_policy,
		.maxattr = GSCAN_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = WIFI_SUBCMD_ROAM_CAPABILITY
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_fw_roam_get_capability
	},
#endif /* ROAMEXP_SUPPORT */
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = DEBUG_GET_VER
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_dbg_get_version,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = andr_dbg_policy,
		.maxattr = DEBUG_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#ifdef DHD_LOG_DUMP
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = DEBUG_GET_FILE_DUMP_BUF
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_dbg_file_dump,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = dump_buf_policy,
		.maxattr = DUMP_BUF_ATTR_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#endif /* DHD_LOG_DUMP */

#ifdef DEBUGABILITY
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = DEBUG_TRIGGER_MEM_DUMP
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_dbg_trigger_mem_dump
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = DEBUG_GET_MEM_DUMP
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_dbg_get_mem_dump,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = andr_dbg_policy,
		.maxattr = DEBUG_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = DEBUG_START_LOGGING
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_dbg_start_logging,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = andr_dbg_policy,
		.maxattr = DEBUG_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = DEBUG_RESET_LOGGING
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_dbg_reset_logging
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = DEBUG_GET_RING_STATUS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_dbg_get_ring_status,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = andr_dbg_policy,
		.maxattr = DEBUG_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = DEBUG_GET_RING_DATA
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_dbg_get_ring_data,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = andr_dbg_policy,
		.maxattr = DEBUG_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */

	},
#endif /* DEBUGABILITY */
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = DEBUG_GET_FEATURE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_dbg_get_feature
	},
#ifdef DBG_PKT_MON
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = DEBUG_START_PKT_FATE_MONITORING
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_dbg_start_pkt_fate_monitoring
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = DEBUG_GET_TX_PKT_FATES
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_dbg_get_tx_pkt_fates,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = andr_dbg_policy,
		.maxattr = DEBUG_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = DEBUG_GET_RX_PKT_FATES
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_dbg_get_rx_pkt_fates,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = andr_dbg_policy,
		.maxattr = DEBUG_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#endif /* DBG_PKT_MON */
#ifdef KEEP_ALIVE
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = WIFI_OFFLOAD_SUBCMD_START_MKEEP_ALIVE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_start_mkeep_alive,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = mkeep_alive_attr_policy,
		.maxattr = MKEEP_ALIVE_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = WIFI_OFFLOAD_SUBCMD_STOP_MKEEP_ALIVE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_stop_mkeep_alive,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = mkeep_alive_attr_policy,
		.maxattr = MKEEP_ALIVE_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#endif /* KEEP_ALIVE */
#ifdef WL_NAN
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = NAN_WIFI_SUBCMD_ENABLE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_nan_start_handler,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = nan_attr_policy,
		.maxattr = NAN_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = NAN_WIFI_SUBCMD_DISABLE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_nan_stop_handler,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = nan_attr_policy,
		.maxattr = NAN_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = NAN_WIFI_SUBCMD_CONFIG
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_nan_config_handler,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = nan_attr_policy,
		.maxattr = NAN_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = NAN_WIFI_SUBCMD_REQUEST_PUBLISH
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_nan_req_publish,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = nan_attr_policy,
		.maxattr = NAN_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = NAN_WIFI_SUBCMD_REQUEST_SUBSCRIBE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_nan_req_subscribe,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = nan_attr_policy,
		.maxattr = NAN_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = NAN_WIFI_SUBCMD_CANCEL_PUBLISH
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_nan_cancel_publish,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = nan_attr_policy,
		.maxattr = NAN_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = NAN_WIFI_SUBCMD_CANCEL_SUBSCRIBE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_nan_cancel_subscribe,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = nan_attr_policy,
		.maxattr = NAN_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = NAN_WIFI_SUBCMD_TRANSMIT
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_nan_transmit,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = nan_attr_policy,
		.maxattr = NAN_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = NAN_WIFI_SUBCMD_GET_CAPABILITIES
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_nan_get_capablities
	},

	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = NAN_WIFI_SUBCMD_DATA_PATH_IFACE_CREATE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_nan_data_path_iface_create,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = nan_attr_policy,
		.maxattr = NAN_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = NAN_WIFI_SUBCMD_DATA_PATH_IFACE_DELETE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_nan_data_path_iface_delete,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = nan_attr_policy,
		.maxattr = NAN_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = NAN_WIFI_SUBCMD_DATA_PATH_REQUEST
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_nan_data_path_request,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = nan_attr_policy,
		.maxattr = NAN_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = NAN_WIFI_SUBCMD_DATA_PATH_RESPONSE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_nan_data_path_response,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = nan_attr_policy,
		.maxattr = NAN_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = NAN_WIFI_SUBCMD_DATA_PATH_END
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_nan_data_path_end,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = nan_attr_policy,
		.maxattr = NAN_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#ifdef WL_NAN_DISC_CACHE
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = NAN_WIFI_SUBCMD_DATA_PATH_SEC_INFO
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_nan_data_path_sec_info,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = nan_attr_policy,
		.maxattr = NAN_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#endif /* WL_NAN_DISC_CACHE */
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = NAN_WIFI_SUBCMD_VERSION_INFO
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_nan_version_info
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = NAN_WIFI_SUBCMD_ENABLE_MERGE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_nan_enable_merge,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = nan_attr_policy,
		.maxattr = NAN_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#endif /* WL_NAN */
#if defined(APF)
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = APF_SUBCMD_GET_CAPABILITIES
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_apf_get_capabilities,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = apf_atrribute_policy,
		.maxattr = APF_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = APF_SUBCMD_SET_FILTER
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_apf_set_filter,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = apf_atrribute_policy,
		.maxattr = APF_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = APF_SUBCMD_READ_FILTER_DATA
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_apf_read_filter_data,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = apf_atrribute_policy,
		.maxattr = APF_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#endif /* APF */
#ifdef NDO_CONFIG_SUPPORT
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = WIFI_SUBCMD_CONFIG_ND_OFFLOAD
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_configure_nd_offload,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = andr_wifi_attr_policy,
		.maxattr = ANDR_WIFI_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#endif /* NDO_CONFIG_SUPPORT */
#ifdef RSSI_MONITOR_SUPPORT
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = WIFI_SUBCMD_SET_RSSI_MONITOR
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_set_rssi_monitor,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = rssi_monitor_attr_policy,
		.maxattr = RSSI_MONITOR_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#endif /* RSSI_MONITOR_SUPPORT */
#ifdef DHD_WAKE_STATUS
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = DEBUG_GET_WAKE_REASON_STATS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_get_wake_reason_stats,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = wake_stat_attr_policy,
		.maxattr = WAKE_STAT_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#endif /* DHD_WAKE_STATUS */
#ifdef DHDTCPACK_SUPPRESS
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = WIFI_SUBCMD_CONFIG_TCPACK_SUP
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_set_tcpack_sup_mode,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = andr_wifi_attr_policy,
		.maxattr = ANDR_WIFI_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#endif /* DHDTCPACK_SUPPRESS */
#if !defined(BCMSUP_4WAY_HANDSHAKE) || (LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0))
	{
		{
			.vendor_id = OUI_BRCM,
			.subcmd = BRCM_VENDOR_SCMD_SET_PMK
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_set_pmk,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = brcm_drv_attr_policy,
		.maxattr = BRCM_ATTR_DRIVER_MAX
#endif /* LINUX_VERSION >= 5.3 */

	},
#endif /* !BCMSUP_4WAY_HANDSHAKE || LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0) */
	{
		{
			.vendor_id = OUI_BRCM,
			.subcmd = BRCM_VENDOR_SCMD_GET_FEATURES
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_get_driver_feature,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = brcm_drv_attr_policy,
		.maxattr = BRCM_ATTR_DRIVER_MAX
#endif /* LINUX_VERSION >= 5.3 */

	},
#if defined(WL_CFG80211) && defined(DHD_FILE_DUMP_EVENT)
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = DEBUG_FILE_DUMP_DONE_IND
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_notify_dump_completion
	},
#endif /* WL_CFG80211 && DHD_FILE_DUMP_EVENT */
#if defined(WL_CFG80211)
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = DEBUG_SET_HAL_START
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_set_hal_started,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = hal_start_attr_policy,
		.maxattr = SET_HAL_START_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = DEBUG_SET_HAL_STOP
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_stop_hal
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = DEBUG_SET_HAL_PID
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_set_hal_pid,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = hal_start_attr_policy,
		.maxattr = SET_HAL_START_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#endif /* WL_CFG80211 */
#ifdef WL_P2P_RAND
	{
		{
			.vendor_id = OUI_BRCM,
			.subcmd = BRCM_VENDOR_SCMD_SET_MAC
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
		.doit = wl_cfgvendor_set_p2p_rand_mac,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = brcm_drv_attr_policy,
		.maxattr = BRCM_ATTR_DRIVER_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#endif /* WL_P2P_RAND */
#ifdef WL_THERMAL_MITIGATION
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = WIFI_SUBCMD_THERMAL_MITIGATION
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgvendor_thermal_mitigation,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = andr_wifi_attr_policy,
		.maxattr = ANDR_WIFI_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#endif /* WL_THERMAL_MITIGATION */
#ifdef WL_SAR_TX_POWER
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = WIFI_SUBCMD_TX_POWER_SCENARIO
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV,
		.doit = wl_cfgvendor_tx_power_scenario,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = andr_wifi_attr_policy,
		.maxattr = ANDR_WIFI_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#endif /* WL_SAR_TX_POWER */
#ifdef WL_SOFTAP_ACS
	{
		{
			.vendor_id = OUI_BRCM,
			.subcmd = BRCM_VENDOR_SCMD_ACS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgscan_acs,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = acs_attr_policy,
		.maxattr = BRCM_VENDOR_ATTR_ACS_LAST
#endif /* LINUX_VERSION >= 5.3 */

	},
#endif /* WL_SOFTAP_ACS */
#ifdef TPUT_DEBUG_DUMP
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = DEBUG_SET_TPUT_DEBUG_DUMP_CMD
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wl_cfgdbg_tput_debug_get_cmd,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
		.policy = tput_debug_dump_attr_policy,
		.maxattr = TPUT_DEBUG_ATTRIBUTE_MAX
#endif /* LINUX_VERSION >= 5.3 */
	},
#endif /* TPUT_DEBUG_DUMP */
};

static const struct  nl80211_vendor_cmd_info wl_vendor_events [] = {
		{ OUI_BRCM, BRCM_VENDOR_EVENT_UNSPEC },
		{ OUI_BRCM, BRCM_VENDOR_EVENT_PRIV_STR },
		{ OUI_GOOGLE, GOOGLE_GSCAN_SIGNIFICANT_EVENT },
		{ OUI_GOOGLE, GOOGLE_GSCAN_GEOFENCE_FOUND_EVENT },
		{ OUI_GOOGLE, GOOGLE_GSCAN_BATCH_SCAN_EVENT },
		{ OUI_GOOGLE, GOOGLE_SCAN_FULL_RESULTS_EVENT },
		{ OUI_GOOGLE, GOOGLE_RTT_COMPLETE_EVENT },
		{ OUI_GOOGLE, GOOGLE_SCAN_COMPLETE_EVENT },
		{ OUI_GOOGLE, GOOGLE_GSCAN_GEOFENCE_LOST_EVENT },
		{ OUI_GOOGLE, GOOGLE_SCAN_EPNO_EVENT },
		{ OUI_GOOGLE, GOOGLE_DEBUG_RING_EVENT },
		{ OUI_GOOGLE, GOOGLE_FW_DUMP_EVENT },
		{ OUI_GOOGLE, GOOGLE_PNO_HOTSPOT_FOUND_EVENT },
		{ OUI_GOOGLE, GOOGLE_RSSI_MONITOR_EVENT },
		{ OUI_GOOGLE, GOOGLE_MKEEP_ALIVE_EVENT },
		{ OUI_GOOGLE, GOOGLE_NAN_EVENT_ENABLED},
		{ OUI_GOOGLE, GOOGLE_NAN_EVENT_DISABLED},
		{ OUI_GOOGLE, GOOGLE_NAN_EVENT_SUBSCRIBE_MATCH},
		{ OUI_GOOGLE, GOOGLE_NAN_EVENT_REPLIED},
		{ OUI_GOOGLE, GOOGLE_NAN_EVENT_PUBLISH_TERMINATED},
		{ OUI_GOOGLE, GOOGLE_NAN_EVENT_SUBSCRIBE_TERMINATED},
		{ OUI_GOOGLE, GOOGLE_NAN_EVENT_DE_EVENT},
		{ OUI_GOOGLE, GOOGLE_NAN_EVENT_FOLLOWUP},
		{ OUI_GOOGLE, GOOGLE_NAN_EVENT_TRANSMIT_FOLLOWUP_IND},
		{ OUI_GOOGLE, GOOGLE_NAN_EVENT_DATA_REQUEST},
		{ OUI_GOOGLE, GOOGLE_NAN_EVENT_DATA_CONFIRMATION},
		{ OUI_GOOGLE, GOOGLE_NAN_EVENT_DATA_END},
		{ OUI_GOOGLE, GOOGLE_NAN_EVENT_BEACON},
		{ OUI_GOOGLE, GOOGLE_NAN_EVENT_SDF},
		{ OUI_GOOGLE, GOOGLE_NAN_EVENT_TCA},
		{ OUI_GOOGLE, GOOGLE_NAN_EVENT_SUBSCRIBE_UNMATCH},
		{ OUI_GOOGLE, GOOGLE_NAN_EVENT_UNKNOWN},
		{ OUI_GOOGLE, GOOGLE_ROAM_EVENT_START},
		{ OUI_BRCM, BRCM_VENDOR_EVENT_HANGED},
		{ OUI_BRCM, BRCM_VENDOR_EVENT_SAE_KEY},
		{ OUI_BRCM, BRCM_VENDOR_EVENT_BEACON_RECV},
		{ OUI_BRCM, BRCM_VENDOR_EVENT_PORT_AUTHORIZED},
		{ OUI_GOOGLE, GOOGLE_FILE_DUMP_EVENT },
		{ OUI_BRCM, BRCM_VENDOR_EVENT_CU},
		{ OUI_BRCM, BRCM_VENDOR_EVENT_WIPS},
		{ OUI_GOOGLE, NAN_ASYNC_RESPONSE_DISABLED},
		{ OUI_BRCM, BRCM_VENDOR_EVENT_RCC_INFO},
		{ OUI_BRCM, BRCM_VENDOR_EVENT_ACS},
		{ OUI_BRCM, BRCM_VENDOR_EVENT_TWT},
		{ OUI_GOOGLE, BRCM_VENDOR_EVENT_TPUT_DUMP},
		{ OUI_GOOGLE, GOOGLE_NAN_EVENT_MATCH_EXPIRY},
		{ OUI_BRCM, BRCM_VENDOR_EVENT_RCC_FREQ_INFO},
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
static void
wl_cfgvendor_apply_cmd_policy(struct wiphy *wiphy)
{
	int i;
	u32 n_cmds = wiphy->n_vendor_commands;

	WL_INFORM(("Apply CMD_RAW_DATA policy\n"));
	for (i = 0; i < n_cmds; i++) {
		if (wl_vendor_cmds[i].policy == NULL) {
			wl_vendor_cmds[i].policy = VENDOR_CMD_RAW_DATA;
		}
	}
}
#endif /* LINUX VER >= 5.3 */

int wl_cfgvendor_attach(struct wiphy *wiphy, dhd_pub_t *dhd)
{

	WL_INFORM_MEM(("Vendor: Register BRCM cfg80211 vendor cmd(0x%x) interface \n",
		NL80211_CMD_VENDOR));

	wiphy->vendor_commands	= wl_vendor_cmds;
	wiphy->n_vendor_commands = ARRAY_SIZE(wl_vendor_cmds);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
	wl_cfgvendor_apply_cmd_policy(wiphy);
#endif /* LINUX VER >= 5.3 */

	wiphy->vendor_events	= wl_vendor_events;
	wiphy->n_vendor_events	= ARRAY_SIZE(wl_vendor_events);

#ifdef DEBUGABILITY
	dhd_os_dbg_register_callback(FW_VERBOSE_RING_ID, wl_cfgvendor_dbg_ring_send_evt);
	dhd_os_dbg_register_callback(DHD_EVENT_RING_ID, wl_cfgvendor_dbg_ring_send_evt);
#ifdef DHD_DEBUGABILITY_LOG_DUMP_RING
	dhd_os_dbg_register_callback(DRIVER_LOG_RING_ID, wl_cfgvendor_dbg_ring_send_evt);
	dhd_os_dbg_register_callback(ROAM_STATS_RING_ID, wl_cfgvendor_dbg_ring_send_evt);
#endif /* DHD_DEBUGABILITY_LOG_DUMP_RING */
#endif /* DEBUGABILITY */
#ifdef DHD_LOG_DUMP
	dhd_os_dbg_register_urgent_notifier(dhd, wl_cfgvendor_dbg_send_file_dump_evt);
#endif /* DHD_LOG_DUMP */

	return 0;
}

int wl_cfgvendor_detach(struct wiphy *wiphy)
{
	WL_INFORM_MEM(("Vendor: Unregister BRCM cfg80211 vendor interface \n"));

	wiphy->vendor_commands  = NULL;
	wiphy->vendor_events    = NULL;
	wiphy->n_vendor_commands = 0;
	wiphy->n_vendor_events  = 0;

	return 0;
}
#endif /* (LINUX_VERSION_CODE > KERNEL_VERSION(3, 13, 0)) || defined(WL_VENDOR_EXT_SUPPORT) */

#ifdef WL_CFGVENDOR_SEND_HANG_EVENT
void
wl_cfgvendor_send_hang_event(struct net_device *dev, u16 reason, char *string, int hang_info_cnt)
{
	struct bcm_cfg80211 *cfg = wl_get_cfg(dev);
	struct wiphy *wiphy;
	char *hang_info;
	int len = 0;
	int bytes_written;
	uint32 dummy_data = 0;
	int reason_hang_info = 0;
	int cnt = 0;
	dhd_pub_t *dhd;
	int hang_reason_mismatch = FALSE;

	if (!cfg || !cfg->wdev) {
		WL_ERR(("cfg=%p wdev=%p\n", cfg, (cfg ? cfg->wdev : NULL)));
		return;
	}

	wiphy = cfg->wdev->wiphy;

	if (!wiphy) {
		WL_ERR(("wiphy is NULL\n"));
		return;
	}

	hang_info = MALLOCZ(cfg->osh, VENDOR_SEND_HANG_EXT_INFO_LEN);
	if (hang_info == NULL) {
		WL_ERR(("alloc hang_info failed\n"));
		return;
	}

	dhd = (dhd_pub_t *)(cfg->pub);

#ifdef WL_BCNRECV
	/* check fakeapscan in progress then stop scan */
	if (cfg->bcnrecv_info.bcnrecv_state == BEACON_RECV_STARTED) {
		wl_android_bcnrecv_stop(dev, WL_BCNRECV_HANG);
	}
#endif /* WL_BCNRECV */
	sscanf(string, "%d", &reason_hang_info);
	bytes_written = 0;
	len = VENDOR_SEND_HANG_EXT_INFO_LEN - bytes_written;
	if (strlen(string) == 0 || (reason_hang_info != reason)) {
		WL_ERR(("hang reason mismatch: string len %d reason_hang_info %d\n",
			(int)strlen(string), reason_hang_info));
		hang_reason_mismatch = TRUE;
		if (dhd) {
			get_debug_dump_time(dhd->debug_dump_time_hang_str);
			copy_debug_dump_time(dhd->debug_dump_time_str,
					dhd->debug_dump_time_hang_str);
		}
		/* Fill bigdata key with */
		bytes_written += scnprintf(&hang_info[bytes_written], len,
				"%d %d %s %08x %08x %08x %08x %08x %08x %08x",
				reason, VENDOR_SEND_HANG_EXT_INFO_VER,
				dhd->debug_dump_time_hang_str,
				0, 0, 0, 0, 0, 0, 0);
		if (dhd) {
			clear_debug_dump_time(dhd->debug_dump_time_hang_str);
		}
	} else {
		bytes_written += scnprintf(&hang_info[bytes_written], len, "%s", string);
	}

	WL_ERR(("hang reason: %d info cnt: %d\n", reason, hang_info_cnt));

	if (hang_reason_mismatch == FALSE) {
		cnt = hang_info_cnt;
	} else {
		cnt = HANG_FIELD_MISMATCH_CNT;
	}

	while (cnt < HANG_FIELD_CNT_MAX) {
		len = VENDOR_SEND_HANG_EXT_INFO_LEN - bytes_written;
		if (len <= 0) {
			break;
		}
		bytes_written += scnprintf(&hang_info[bytes_written], len,
				"%c%08x", HANG_RAW_DEL, dummy_data);
		cnt++;
	}

	WL_ERR(("hang info cnt: %d len: %d\n", cnt, (int)strlen(hang_info)));
	WL_ERR(("hang info data: %s\n", hang_info));

	wl_cfgvendor_send_async_event(wiphy,
			bcmcfg_to_prmry_ndev(cfg), BRCM_VENDOR_EVENT_HANGED,
			hang_info, (int)strlen(hang_info));

	memset(string, 0, VENDOR_SEND_HANG_EXT_INFO_LEN);

	if (hang_info) {
		MFREE(cfg->osh, hang_info, VENDOR_SEND_HANG_EXT_INFO_LEN);
	}

#ifdef DHD_LOG_DUMP
	dhd_logdump_cookie_save(dhd, dhd->debug_dump_time_hang_str, "HANG");
#endif /*  DHD_LOG_DUMP */

	if (dhd) {
		clear_debug_dump_time(dhd->debug_dump_time_str);
	}
}

void
wl_cfgvendor_simple_hang_event(struct net_device *dev, u16 reason)
{
	struct bcm_cfg80211 *cfg;
	struct wiphy *wiphy;
	struct sk_buff *msg;
	gfp_t kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	int hang_event_len = 0;
#ifdef DHD_COREDUMP
	dhd_pub_t *dhd;
#endif
	WL_ERR(("0x%x\n", reason));

	cfg = wl_cfg80211_get_bcmcfg();
	if (!cfg || !cfg->wdev) {
		WL_ERR(("fw dump evt invalid arg\n"));
		return;
	}

	wiphy = bcmcfg_to_wiphy(cfg);
	if (!wiphy) {
		WL_ERR(("wiphy is NULL\n"));
		return;
	}

#ifdef DHD_COREDUMP
	hang_event_len = DHD_MEMDUMP_LONGSTR_LEN;
#endif

	/* Allocate the skb for vendor event */
	msg = CFG80211_VENDOR_EVENT_ALLOC(wiphy, ndev_to_wdev(dev),
			hang_event_len, BRCM_VENDOR_EVENT_HANGED, kflags);
	if (!msg) {
		WL_ERR(("%s: fail to allocate skb for vendor event\n", __FUNCTION__));
		return;
	}

#ifdef DHD_COREDUMP
	dhd = (dhd_pub_t *)(cfg->pub);

	WL_ERR(("hang reason: %s\n", dhd->memdump_str));
	nla_put(msg, DEBUG_ATTRIBUTE_HANG_REASON, DHD_MEMDUMP_LONGSTR_LEN, dhd->memdump_str);
#endif

	cfg80211_vendor_event(msg, kflags);
	return;
}

void
wl_copy_hang_info_if_falure(struct net_device *dev, u16 reason, s32 ret)
{
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dhd;
	s32 err = 0;
	char ioctl_buf[WLC_IOCTL_SMLEN];
	memuse_info_t mu;
	int bytes_written = 0;
	int remain_len = 0;

	if (!dev) {
		WL_ERR(("dev is null"));
		return;

	}

	cfg = wl_get_cfg(dev);
	if (!cfg) {
		WL_ERR(("dev=%p cfg=%p\n", dev, cfg));
		return;
	}

	dhd = (dhd_pub_t *)(cfg->pub);

	if (!dhd || !dhd->hang_info) {
		WL_ERR(("%s dhd=%p hang_info=%p\n", __FUNCTION__,
			dhd, (dhd ? dhd->hang_info : NULL)));
		return;
	}

	err = wldev_iovar_getbuf_bsscfg(dev, "memuse",
			NULL, 0, ioctl_buf, WLC_IOCTL_SMLEN, 0, NULL);
	if (unlikely(err)) {
		WL_ERR(("error (%d)\n", err));
		return;
	}

	memcpy(&mu, ioctl_buf, sizeof(memuse_info_t));

	if (mu.len >= sizeof(memuse_info_t)) {
		WL_ERR(("Heap Total: %d(%dK)\n", mu.arena_size, KB(mu.arena_size)));
		WL_ERR(("Free: %d(%dK), LWM: %d(%dK)\n",
			mu.arena_free, KB(mu.arena_free),
			mu.free_lwm, KB(mu.free_lwm)));
		WL_ERR(("In use: %d(%dK), HWM: %d(%dK)\n",
			mu.inuse_size, KB(mu.inuse_size),
			mu.inuse_hwm, KB(mu.inuse_hwm)));
		WL_ERR(("Malloc failure count: %d\n", mu.mf_count));
	}

	memset(dhd->hang_info, 0, VENDOR_SEND_HANG_EXT_INFO_LEN);
	remain_len = VENDOR_SEND_HANG_EXT_INFO_LEN - bytes_written;

	get_debug_dump_time(dhd->debug_dump_time_hang_str);
	copy_debug_dump_time(dhd->debug_dump_time_str, dhd->debug_dump_time_hang_str);

	bytes_written += scnprintf(&dhd->hang_info[bytes_written], remain_len,
			"%d %d %s %d %d %d %d %d %08x %08x",
			reason, VENDOR_SEND_HANG_EXT_INFO_VER,
			dhd->debug_dump_time_hang_str,
			ret, mu.arena_size, mu.arena_free, mu.inuse_size, mu.mf_count, 0, 0);

	dhd->hang_info_cnt = HANG_FIELD_IF_FAILURE_CNT;

	clear_debug_dump_time(dhd->debug_dump_time_hang_str);

	return;
}
#endif /* WL_CFGVENDOR_SEND_HANG_EVENT */
#ifdef WL_CFGVENDOR_SEND_ALERT_EVENT
void
wl_cfgvendor_send_alert_event(struct net_device *dev, uint32 reason)
{
	struct bcm_cfg80211 *cfg;
	struct wiphy *wiphy;
	struct sk_buff *msg;
	gfp_t kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;

	WL_DBG(("wl_cfgvendor_send_fw_dump_event %d\n", reason));

	cfg = wl_cfg80211_get_bcmcfg();
	if (!cfg || !cfg->wdev) {
		WL_ERR(("fw dump evt invalid arg\n"));
		return;
	}

	wiphy = bcmcfg_to_wiphy(cfg);
	if (!wiphy) {
		WL_ERR(("wiphy is NULL\n"));
		return;
	}

	/* Allocate the skb for vendor event */
	msg = CFG80211_VENDOR_EVENT_ALLOC(wiphy, ndev_to_wdev(dev), sizeof(uint32),
		GOOGLE_FW_DUMP_EVENT, kflags);
	if (!msg) {
		WL_ERR(("%s: fail to allocate skb for vendor event\n", __FUNCTION__));
		return;
	}

	nla_put_u32(msg, DEBUG_ATTRIBUTE_FW_ERR_CODE, reason);

	cfg80211_vendor_event(msg, kflags);
	return;
}
#endif /* WL_CFGVENDOR_SEND_ALERT_EVENT */
