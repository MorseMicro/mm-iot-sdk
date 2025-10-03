/*
 * hostapd / IEEE 802.11ah S1G
 * Copyright (c) 2022, Morse Micro
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "hostapd.h"
#include "ap_config.h"
#include "sta_info.h"
#include "beacon.h"
#include "ieee802_11.h"


static inline void dot11_prim_1mhz_channel_idx_to_loc_and_pri(
	int op_bw_mhz, int pr_bw_mhz, int chan_centre_freq_num, int idx,
	int *pr_chan_num, int *chan_loc)
{
	*pr_chan_num = -1;
	*chan_loc = -1;

	switch (op_bw_mhz)
	{
	case 1:
		*chan_loc = 0;
		*pr_chan_num = chan_centre_freq_num;
		break;

	case 2:
		if (pr_bw_mhz == 2)
		{
			*chan_loc = idx;
			*pr_chan_num = chan_centre_freq_num;
		}
		else
		{
			*chan_loc = 0;
			*pr_chan_num = chan_centre_freq_num + (idx * 2 - 1);
		}

		break;

	case 4:
		if (chan_centre_freq_num == 36)
		{
			// Japan 4 MHz Ch 36 special case
			switch (idx)
			{
			case 0:
				if (pr_bw_mhz == 1)
				{
					*pr_chan_num = 13;
					*chan_loc = 0;
				}
				else
				{
					*pr_chan_num = 2;
					*chan_loc = 0;
				}
				break;
			case 1:
				if (pr_bw_mhz == 1)
				{
					*pr_chan_num = 15;
					*chan_loc = 0;
				}
				else
				{
					*pr_chan_num = 2;
					*chan_loc = 0;
				}
				break;
			case 2:
				if (pr_bw_mhz == 1)
				{
					*pr_chan_num = 17;
					*chan_loc = 0;
				}
				else
				{
					*pr_chan_num = 6;
					*chan_loc = 0;
				}
				break;
			case 3:
				if (pr_bw_mhz == 1)
				{
					*pr_chan_num = 19;
					*chan_loc = 0;
				}
				else
				{
					*pr_chan_num = 6;
					*chan_loc = 0;
				}
				break;
			default:
				wpa_printf(MSG_ERROR,"Unsupported ch idx: %d\n", idx);
				break;
			}
		}
		else if (chan_centre_freq_num == 38)
		{
			// Japan 4 MHz Ch 38 special case
			switch (idx)
			{
			case 0:
				if (pr_bw_mhz == 1)
				{
					*pr_chan_num = 15;
				}
				else
				{
					*pr_chan_num = 4;
				}
				*chan_loc = 0;
				break;
			case 1:
				if (pr_bw_mhz == 1)
				{
					*pr_chan_num = 17;
					*chan_loc = 0;
				}
				else
				{
					*pr_chan_num = 4;
					*chan_loc = 1;
				}
				break;
			case 2:
				if (pr_bw_mhz == 1)
				{
					*pr_chan_num = 19;
					*chan_loc = 0;
				}
				else
				{
					*pr_chan_num = 8;
					*chan_loc = 0;
				}
				break;
			case 3:
				if (pr_bw_mhz == 1)
				{
					*pr_chan_num = 21;
					*chan_loc = 0;
				}
				else
				{
					*pr_chan_num = 8;
					*chan_loc = 1;
				}
				break;
			default:
				wpa_printf(MSG_ERROR, "Unsupported ch idx: %d\n", idx);
				break;
			}
		}
		else if (pr_bw_mhz == 1)
		{
			int pr_chan_offset = (idx / pr_bw_mhz) * 2;
			*pr_chan_num = chan_centre_freq_num - 3 + pr_chan_offset;
			*chan_loc = 0;
		}
		else if (pr_bw_mhz == 2)
		{
			int pr_chan_offset = (idx / pr_bw_mhz) * 4;
			*pr_chan_num = chan_centre_freq_num - 2 + pr_chan_offset;
			*chan_loc = idx % pr_bw_mhz;
		}
		else
		{
			wpa_printf(MSG_ERROR, "Invalid pr_bw_mhz %d\n", pr_bw_mhz);
		}
		break;

	case 8:
		if (pr_bw_mhz == 1)
		{
			int pr_chan_offset = (idx / pr_bw_mhz) * 2;
			*pr_chan_num = chan_centre_freq_num - 7 + pr_chan_offset;
			*chan_loc = 0;
		}
		else if (pr_bw_mhz == 2)
		{
			int pr_chan_offset = (idx / pr_bw_mhz) * 4;
			*pr_chan_num = chan_centre_freq_num - 6 + pr_chan_offset;
			*chan_loc = idx % pr_bw_mhz;
		}
		else
		{
			wpa_printf(MSG_ERROR, "Invalid pr_bw_mhz %d\n", pr_bw_mhz);
		}
		break;

	default:
		wpa_printf(MSG_ERROR, "Unsupported BW: %d\n", op_bw_mhz);
		break;
	}
}

u8 * hostapd_eid_s1g_oper(struct hostapd_data *hapd, u8 *eid)
{
	struct ieee80211_s1g_operation *cap;
	u8 max_mcs_nss_set;
	u8 oper_chwidth_mhz = 1;
	u8 s1g_supp_oper_chwidth_mask = 0;
	u8 s1g_prim_chwidth = 0;
	u16 s1g_basic_min_max_mcs_nss_ie = 0;
	u8 *pos = eid;

	*pos++ = WLAN_EID_S1G_OPERATION;
	*pos++ = sizeof(*cap);

	cap = (struct ieee80211_s1g_operation *) pos;
	os_memset(cap, 0, sizeof(*cap));

	switch(hapd->iconf->s1g_oper_chwidth) {
	case S1G_OPER_CHWIDTH_1:
		oper_chwidth_mhz = 1;
		break;
	case S1G_OPER_CHWIDTH_2:
		oper_chwidth_mhz = 2;
		break;
	case S1G_OPER_CHWIDTH_4:
		oper_chwidth_mhz = 4;
		break;
	case S1G_OPER_CHWIDTH_8:
		oper_chwidth_mhz = 8;
		break;
	case S1G_OPER_CHWIDTH_16:
		oper_chwidth_mhz = 16;
		break;
	default:
		wpa_printf(MSG_ERROR, "Invalid s1g_oper_chwidth %d\n", hapd->iconf->s1g_oper_chwidth);
		break;
	}

	s1g_supp_oper_chwidth_mask = oper_chwidth_mhz - 1;

	int pri_chan_num = -1;
	int pri_chan_loc = -1;

	dot11_prim_1mhz_channel_idx_to_loc_and_pri(
		oper_chwidth_mhz, hapd->iconf->s1g_prim_chwidth,
		hapd->iconf->channel, hapd->iconf->s1g_prim_1mhz_chan_index,
		&pri_chan_num, &pri_chan_loc);
	if (pri_chan_num < 0 || pri_chan_loc < 0) {
		pri_chan_num = 0;
		pri_chan_loc = 0;
	}

	cap->primary_ch = pri_chan_num;

	cap->ch_width = 0;
	cap->ch_width |= (hapd->iconf->s1g_prim_chwidth == 1) ? S1G_OPER_IE_CHANWIDTH_PRIM_CH_MASK : 0;
	cap->ch_width |= ((s1g_supp_oper_chwidth_mask << 1) & S1G_OPER_IE_CHANWIDTH_OPER_CH_MASK);
	cap->ch_width |= pri_chan_loc ?  S1G_OPER_IE_CHANWIDTH_PRIM_OFFSET : 0;

	cap->oper_class = hapd->iconf->s1g_op_class;

	cap->oper_ch = hapd->iconf->channel;

	max_mcs_nss_set = hapd->iconf->s1g_basic_mcs_nss_set;

	s1g_basic_min_max_mcs_nss_ie = (max_mcs_nss_set & 0x3) <<
					S1G_OPER_IE_MAX_MCS_1_NSS_SHIFT |
						((max_mcs_nss_set >> 2) & 0x3) <<
					S1G_OPER_IE_MAX_MCS_2_NSS_SHIFT |
						((max_mcs_nss_set >> 4) & 0x3) <<
					S1G_OPER_IE_MAX_MCS_3_NSS_SHIFT |
						((max_mcs_nss_set >> 6) & 0x3) <<
					S1G_OPER_IE_MAX_MCS_4_NSS_SHIFT;
	cap->basic_mcs_nss |= s1g_basic_min_max_mcs_nss_ie & 0xff;
	cap->basic_mcs_nss |= (s1g_basic_min_max_mcs_nss_ie >> 8);

	pos += sizeof(*cap);
	return pos;
}

u8 * hostapd_eid_s1g_capab(struct hostapd_data *hapd, u8 *eid)
{
	struct ieee80211_s1g_capabilities *cap;
	u8 *pos = eid;

	*pos++ = WLAN_EID_S1G_CAPABILITIES;
	*pos++ = sizeof(*cap);

	cap = (struct ieee80211_s1g_capabilities *) pos;
	os_memset(cap, 0, sizeof(*cap));

	os_memcpy(cap->capab_info,
		  hapd->iface->current_mode->s1g_capab,
		  sizeof(cap->capab_info));
	os_memcpy(cap->supp_mcs_nss,
		  hapd->iface->current_mode->s1g_mcs,
		  sizeof(cap->supp_mcs_nss));

	pos += sizeof(*cap);
	return pos;
}


u8 * hostapd_eid_s1g_beacon_compat(struct hostapd_data *hapd, u8 *eid)
{
	u8 *pos = eid;
	u16 beacon_int;

	beacon_int = hapd->conf->short_beacon_int * hapd->iconf->beacon_int;

	*pos++ = WLAN_EID_S1G_BCN_COMPAT;
	*pos++ = 8;
	*pos++ = WLAN_CAPABILITY_ESS | WLAN_CAPABILITY_PRIVACY;;
	*pos++ = 0;

	*pos++ = beacon_int & 0xff;
	*pos++ = (beacon_int >> 8) & 0xff;

	/* Last 4 octets will be filled in by the driver to contain the 4
	 MSB of the TSF timer at time of generation */
	*pos++ = 0;
	*pos++ = 0;
	*pos++ = 0;
	*pos++ = 0;
	return pos;
}


u8 * hostapd_eid_s1g_short_beacon_int(struct hostapd_data *hapd, u8 *eid)
{
	u8 *pos = eid;

	*pos++ = WLAN_EID_S1G_SHORT_BCN_INTERVAL;
	*pos++ = 2;
	*pos++ = hapd->iconf->beacon_int & 0xff;
	*pos++ = (hapd->iconf->beacon_int >> 8) & 0xff;

	return pos;
}


static u8 hostapd_s1g_get_oper_class_s1g(u16 s1g_op,
					 struct hostapd_iface *iface) {
	enum s1g_oper_chwidth bw;
	unsigned start_freq;

	/* S1G op class conversions to channel start frequency (kHz) and
	 * channel bw (MHz) retrieved from IEEE Std 802.11-2020: Table E-5 */
	switch (s1g_op) {
	case 1:
		bw = S1G_OPER_CHWIDTH_1;
		start_freq = 902000;
		break;
	case 2:
		bw = S1G_OPER_CHWIDTH_2;
		start_freq = 902000;
		break;
	case 3:
		bw = S1G_OPER_CHWIDTH_4;
		start_freq = 902000;
		break;
	case 4:
		bw = S1G_OPER_CHWIDTH_8;
		start_freq = 902000;
		break;
	case 5:
		bw = S1G_OPER_CHWIDTH_16;
		start_freq = 902000;
		break;
	case 6:
		bw = S1G_OPER_CHWIDTH_1;
		start_freq = 863000;
		break;
	case 8:
		bw = S1G_OPER_CHWIDTH_1;
		start_freq = 916500;
		break;
	case 14:
		bw = S1G_OPER_CHWIDTH_1;
		start_freq = 917500;
		break;
	case 15:
		bw = S1G_OPER_CHWIDTH_2;
		start_freq = 917500;
		break;
	case 16:
		bw = S1G_OPER_CHWIDTH_4;
		start_freq = 917500;
		break;
	case 17:
		bw = S1G_OPER_CHWIDTH_1;
		start_freq = 863000;
		break;
	case 18:
		bw = S1G_OPER_CHWIDTH_1;
		start_freq = 902000;
		break;
	case 19:
		bw = S1G_OPER_CHWIDTH_2;
		start_freq = 863000;
		break;
	case 20:
		bw = S1G_OPER_CHWIDTH_2;
		start_freq = 902000;
		break;
	case 21:
		bw = S1G_OPER_CHWIDTH_4;
		start_freq = 902000;
		break;
	case 22:
		bw = S1G_OPER_CHWIDTH_1;
		start_freq = 902000;
		break;
	case 23:
		bw = S1G_OPER_CHWIDTH_2;
		start_freq = 902000;
		break;
	case 24:
		bw = S1G_OPER_CHWIDTH_4;
		start_freq = 902000;
		break;
	case 25:
		bw = S1G_OPER_CHWIDTH_8;
		start_freq = 902000;
		break;
	case 26:
		bw = S1G_OPER_CHWIDTH_1;
		start_freq = 902000;
		break;
	case 27:
		bw = S1G_OPER_CHWIDTH_2;
		start_freq = 902000;
		break;
	case 28:
		bw = S1G_OPER_CHWIDTH_4;
		start_freq = 902000;
		break;
	case 29:
		bw = S1G_OPER_CHWIDTH_8;
		start_freq = 902000;
		break;
	case 30:
		bw = S1G_OPER_CHWIDTH_1;
		start_freq = 901400;
		break;
	default:
		return -1;
	}

	iface->conf->s1g_oper_chwidth = bw;
	iface->conf->s1g_op_class = s1g_op;
	/*
	 * From IEEE Std 802.11-2020: 23.3.14 - Channelization.
	 * ChannelCenterFrequency = ChannelStartingFrequency + separation *
	 * ChannelCenterFrequencyIndex
	 */
	iface->freq_khz = start_freq + 500 * iface->conf->channel;
	return 0;
}


static u8 hostapd_s1g_get_oper_global(u16 glob_op,
					  char *cc,
					  struct hostapd_iface *iface) {
	/* Mappings of global operating class to S1G operating class
	 * retrieved from IEEE Std 802.11-2020: Table E-5 */
	switch(glob_op) {
	case 66:
		if (!strncmp(cc, "EU", 2)) {
			return hostapd_s1g_get_oper_class_s1g(6, iface);
		} else if (!strncmp(cc, "SG", 2)) {
			return hostapd_s1g_get_oper_class_s1g(17, iface);
		} else {
			return -1;
		}
	case 67:
		if (!strncmp(cc, "SG", 2)) {
			return hostapd_s1g_get_oper_class_s1g(19, iface);
		} else {
			return -1;
		}
	case 68:
		if (!strncmp(cc, "US", 2)) {
			return hostapd_s1g_get_oper_class_s1g(1, iface);
		} else if (!strncmp(cc, "SG", 2)) {
			return hostapd_s1g_get_oper_class_s1g(18, iface);
		} else if (!strncmp(cc, "AU", 2)) {
			return hostapd_s1g_get_oper_class_s1g(22, iface);
		} else if (!strncmp(cc, "NZ", 2)) {
			return hostapd_s1g_get_oper_class_s1g(26, iface);
		} else {
			return -1;
		}
	case 69:
		if (!strncmp(cc, "US", 2)) {
			return hostapd_s1g_get_oper_class_s1g(2, iface);
		} else if (!strncmp(cc, "SG", 2)) {
			return hostapd_s1g_get_oper_class_s1g(20, iface);
		} else if (!strncmp(cc, "AU", 2)) {
			return hostapd_s1g_get_oper_class_s1g(23, iface);
		} else if (!strncmp(cc, "NZ", 2)) {
			return hostapd_s1g_get_oper_class_s1g(27, iface);
		} else {
			return -1;
		}
	case 70:
		if (!strncmp(cc, "US", 2)) {
			return hostapd_s1g_get_oper_class_s1g(3, iface);
		} else if (!strncmp(cc, "SG", 2)) {
			return hostapd_s1g_get_oper_class_s1g(21, iface);
		} else if (!strncmp(cc, "AU", 2)) {
			return hostapd_s1g_get_oper_class_s1g(24, iface);
		} else if (!strncmp(cc, "NZ", 2)) {
			return hostapd_s1g_get_oper_class_s1g(28, iface);
		} else {
			return -1;
		}
	case 71:
		if (!strncmp(cc, "US", 2)) {
			return hostapd_s1g_get_oper_class_s1g(4, iface);
		} else if (!strncmp(cc, "AU", 2)) {
			return hostapd_s1g_get_oper_class_s1g(25, iface);
		} else if (!strncmp(cc, "NZ", 2)) {
			return hostapd_s1g_get_oper_class_s1g(29, iface);
		} else {
			return -1;
		}
	case 72:
		if (!strncmp(cc, "US", 2)) {
			return hostapd_s1g_get_oper_class_s1g(5, iface);
		} else {
			return -1;
		}
	case 73:
		if (!strncmp(cc, "JP", 2)) {
			return hostapd_s1g_get_oper_class_s1g(8, iface);
		} else {
			return -1;
		}
	case 74:
		if (!strncmp(cc, "KR", 2)) {
			return hostapd_s1g_get_oper_class_s1g(14, iface);
		} else {
			return -1;
		}
	case 75:
		if (!strncmp(cc, "KR", 2)) {
			return hostapd_s1g_get_oper_class_s1g(15, iface);
		} else {
			return -1;
		}
	case 76:
		if (!strncmp(cc, "KR", 2)) {
			return hostapd_s1g_get_oper_class_s1g(16, iface);
		} else {
			return -1;
		}
	case 77:
		if (!strncmp(cc, "EU", 2)) {
			return hostapd_s1g_get_oper_class_s1g(30, iface);
		} else {
			return -1;
		}
	}
	return 0;
}


u32 hostapd_s1g_get_oper_config(struct hostapd_iface *iface) {
	int ret;

	/* If channel is 0 we will do ACS */
	if (iface->conf->channel == 0)
		return 0;

	if (iface->conf->s1g_op_class) {
		ret = hostapd_s1g_get_oper_class_s1g(iface->conf->s1g_op_class,
							 iface);
		if (ret < 0)
			return -1;
	} else if (iface->conf->op_class && iface->conf->country) {
		ret = hostapd_s1g_get_oper_global(iface->conf->op_class,
						  iface->conf->country,
						  iface);
		if (ret < 0)
			return -1;
	} else {
		return -1;
	}
	return 0;
}
