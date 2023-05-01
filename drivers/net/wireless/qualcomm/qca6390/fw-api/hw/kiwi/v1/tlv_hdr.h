
/*
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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








#ifndef _TLV_HDR_H_
#define _TLV_HDR_H_
#if !defined(__ASSEMBLER__)
#endif

#define _TLV_USERID_WIDTH_      6
#define _TLV_DATA_WIDTH_        32
#define _TLV_TAG_WIDTH_         9

#define _TLV_MRV_EN_LEN_WIDTH_  9
#define _TLV_MRV_DIS_LEN_WIDTH_ 12

#define _TLV_16_DATA_WIDTH_     16
#define _TLV_16_TAG_WIDTH_      5
#define _TLV_16_LEN_WIDTH_      4
#define _TLV_CTAG_WIDTH_        5
#define _TLV_44_DATA_WIDTH_     44
#define _TLV_64_DATA_WIDTH_     64
#define _TLV_76_DATA_WIDTH_     64
#define _TLV_CDATA_WIDTH_       32
#define _TLV_CDATA_76_WIDTH_    64

struct tlv_usr_16_tlword_t {
	   uint16_t             tlv_cflg_reserved   :   1,
				tlv_tag             :   _TLV_16_TAG_WIDTH_,
				tlv_len             :   _TLV_16_LEN_WIDTH_,
				tlv_usrid           :   _TLV_USERID_WIDTH_;
};

struct tlv_16_tlword_t {
	   uint16_t             tlv_cflg_reserved   :   1,
				tlv_len             :   _TLV_16_LEN_WIDTH_,
				tlv_tag             :   _TLV_16_TAG_WIDTH_,
				tlv_reserved        :   6;
};







struct tlv_mlo_usr_32_tlword_t {
	   uint32_t             tlv_cflg_reserved   :   1,
				tlv_tag             :   _TLV_TAG_WIDTH_,
				tlv_len             :   _TLV_MRV_EN_LEN_WIDTH_,
				tlv_dst_linkid      :   3,
				tlv_src_linkid      :   3,
				tlv_mrv             :   1,
				tlv_usrid           :   _TLV_USERID_WIDTH_;
};

struct tlv_mlo_32_tlword_t {
	   uint32_t             tlv_cflg_reserved   :   1,
				tlv_tag             :   _TLV_TAG_WIDTH_,
				tlv_len             :   _TLV_MRV_EN_LEN_WIDTH_,
				tlv_dst_linkid      :   3,
				tlv_src_linkid      :   3,
				tlv_mrv             :   1,
				tlv_reserved        :   6;
};

struct tlv_mlo_usr_64_tlword_t {
	   uint64_t             tlv_cflg_reserved   :   1,
				tlv_tag             :   _TLV_TAG_WIDTH_,
				tlv_len             :   _TLV_MRV_EN_LEN_WIDTH_,
				tlv_dst_linkid      :   3,
				tlv_src_linkid      :   3,
				tlv_mrv             :   1,
				tlv_usrid           :   _TLV_USERID_WIDTH_,
				tlv_reserved        :   32;
};

struct tlv_mlo_64_tlword_t {
	   uint64_t             tlv_cflg_reserved   :   1,
				tlv_tag             :   _TLV_TAG_WIDTH_,
				tlv_len             :   _TLV_MRV_EN_LEN_WIDTH_,
				tlv_dst_linkid      :   3,
				tlv_src_linkid      :   3,
				tlv_mrv             :   1,
				tlv_reserved        :   38;
};

struct tlv_mlo_usr_44_tlword_t {
	   uint64_t             tlv_compression     :   1,
				tlv_tag             :   _TLV_TAG_WIDTH_,
				tlv_len             :   _TLV_MRV_EN_LEN_WIDTH_,
				tlv_dst_linkid      :   3,
				tlv_src_linkid      :   3,
				tlv_mrv             :   1,
				tlv_usrid           :   _TLV_USERID_WIDTH_,
				tlv_reserved        :   10,
				pad_44to64_bit      :   22;
};

struct tlv_mlo_44_tlword_t {
	   uint64_t             tlv_compression     :   1,
				tlv_tag             :   _TLV_TAG_WIDTH_,
				tlv_len             :   _TLV_MRV_EN_LEN_WIDTH_,
				tlv_dst_linkid      :   3,
				tlv_src_linkid      :   3,
				tlv_mrv             :   1,
				tlv_reserved        :   16,
				pad_44to64_bit      :   22;
};

struct tlv_mlo_usr_76_tlword_t {
	   uint64_t             tlv_compression     :   1,
				tlv_tag             :   _TLV_TAG_WIDTH_,
				tlv_len             :   _TLV_MRV_EN_LEN_WIDTH_,
				tlv_dst_linkid      :   3,
				tlv_src_linkid      :   3,
				tlv_mrv             :   1,
				tlv_usrid           :   _TLV_USERID_WIDTH_,
				tlv_reserved        :   32;
	   uint64_t             pad_64to128_bit     :   64;
};

struct tlv_mlo_76_tlword_t {
	   uint64_t             tlv_compression     :   1,
				tlv_tag             :   _TLV_TAG_WIDTH_,
				tlv_len             :   _TLV_MRV_EN_LEN_WIDTH_,
				tlv_dst_linkid      :   3,
				tlv_src_linkid      :   3,
				tlv_mrv             :   1,
				tlv_reserved        :   38;
	   uint64_t             pad_64to128_bit     :   64;
};






struct tlv_mac_usr_32_tlword_t {
	   uint32_t             tlv_cflg_reserved   :   1,
				tlv_tag             :   _TLV_TAG_WIDTH_,
				tlv_len             :   _TLV_MRV_DIS_LEN_WIDTH_,
				tlv_src_linkid      :   3,
				tlv_mrv             :   1,
				tlv_usrid           :   _TLV_USERID_WIDTH_;
};

struct tlv_mac_32_tlword_t {
	   uint32_t             tlv_cflg_reserved   :   1,
				tlv_tag             :   _TLV_TAG_WIDTH_,
				tlv_len             :   _TLV_MRV_DIS_LEN_WIDTH_,
				tlv_src_linkid      :   3,
				tlv_mrv             :   1,
				tlv_reserved        :   6;
};

struct tlv_mac_usr_64_tlword_t {
	   uint64_t             tlv_cflg_reserved   :   1,
				tlv_tag             :   _TLV_TAG_WIDTH_,
				tlv_len             :   _TLV_MRV_DIS_LEN_WIDTH_,
				tlv_src_linkid      :   3,
				tlv_mrv             :   1,
				tlv_usrid           :   _TLV_USERID_WIDTH_,
				tlv_reserved        :   32;
};

struct tlv_mac_64_tlword_t {
	   uint64_t             tlv_cflg_reserved   :   1,
				tlv_tag             :   _TLV_TAG_WIDTH_,
				tlv_len             :   _TLV_MRV_DIS_LEN_WIDTH_,
				tlv_src_linkid      :   3,
				tlv_mrv             :   1,
				tlv_reserved        :   38;
};

struct tlv_mac_usr_44_tlword_t {
	   uint64_t             tlv_compression     :   1,
				tlv_tag             :   _TLV_TAG_WIDTH_,
				tlv_len             :   _TLV_MRV_DIS_LEN_WIDTH_,
				tlv_src_linkid      :   3,
				tlv_mrv             :   1,
				tlv_usrid           :   _TLV_USERID_WIDTH_,
				tlv_reserved        :   10,
				pad_44to64_bit      :   22;
};

struct tlv_mac_44_tlword_t {
	   uint64_t             tlv_compression     :   1,
				tlv_tag             :   _TLV_TAG_WIDTH_,
				tlv_len             :   _TLV_MRV_DIS_LEN_WIDTH_,
				tlv_src_linkid      :   3,
				tlv_mrv             :   1,
				tlv_reserved        :   16,
				pad_44to64_bit      :   22;
};

struct tlv_mac_usr_76_tlword_t {
	   uint64_t             tlv_compression     :   1,
				tlv_tag             :   _TLV_TAG_WIDTH_,
				tlv_len             :   _TLV_MRV_DIS_LEN_WIDTH_,
				tlv_src_linkid      :   3,
				tlv_mrv             :   1,
				tlv_usrid           :   _TLV_USERID_WIDTH_,
				tlv_reserved        :   32;
	   uint64_t             pad_64to128_bit     :   64;
};

struct tlv_mac_76_tlword_t {
	   uint64_t             tlv_compression     :   1,
				tlv_tag             :   _TLV_TAG_WIDTH_,
				tlv_len             :   _TLV_MRV_DIS_LEN_WIDTH_,
				tlv_src_linkid      :   3,
				tlv_mrv             :   1,
				tlv_reserved        :   38;
	   uint64_t             pad_64to128_bit     :   64;
};





struct tlv_usr_c_44_tlword_t {
	   uint64_t             tlv_compression     :   1,
				tlv_ctag            :   _TLV_CTAG_WIDTH_,
				tlv_usrid           :   _TLV_USERID_WIDTH_,
				tlv_cdata           :   _TLV_CDATA_WIDTH_,
				pad_44to64_bit      :   20;
};

struct tlv_usr_c_76_tlword_t {
	   uint64_t             tlv_compression     :   1,
				tlv_ctag            :   _TLV_CTAG_WIDTH_,
				tlv_usrid           :   _TLV_USERID_WIDTH_,
				tlv_cdata_lower_52  :   52;
	   uint64_t             tlv_cdata_upper_12  :   12,
				pad_76to128_bit     :   52;
};







struct tlv_usr_32_hdr {
	   uint64_t             tlv_cflg_reserved   :   1,
				tlv_tag             :   _TLV_TAG_WIDTH_,
				tlv_len             :   _TLV_MRV_DIS_LEN_WIDTH_,
				tlv_src_linkid      :   3,
				tlv_mrv             :   1,
				tlv_usrid           :   _TLV_USERID_WIDTH_,
				tlv_reserved        :   32;
};

struct tlv_32_hdr {
	   uint64_t             tlv_cflg_reserved   :   1,
				tlv_tag             :   _TLV_TAG_WIDTH_,
				tlv_len             :   _TLV_MRV_DIS_LEN_WIDTH_,
				tlv_src_linkid      :   3,
				tlv_mrv             :   1,
				tlv_reserved        :   38;
};



#endif
