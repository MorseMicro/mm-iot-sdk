/*
 * Copyright 2021-2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmhal_wlan.h"
#include "mmlog.h"
#include "mmosal.h"
#include "mmutils.h"
#include "main.h"

#ifndef EXCLUDE_MMCONFIG
#include "mmconfig.h"
#endif

/**
 * Generate a stable, device-unique MAC address based on the MCU UID. This address is not globally
 * unique, but is consistent across boots on the same device and marked as locally administered
 * (0x02 prefix).
 *
 * @param mac_addr Location where the MAC address will be stored.
 */
static void generate_stable_mac_addr_from_uid(uint8_t *mac_addr)
{
    /* Shorten the UID */
    uint32_t uid = LL_GetUID_Word0() ^ LL_GetUID_Word1() ^ LL_GetUID_Word2();

    /* Set MAC address as locally administered */
    mac_addr[0] = 0x02;
    mac_addr[1] = 0x00;
    memcpy(&mac_addr[2], &uid, sizeof(uid));
    MMOSAL_DEV_ASSERT(mm_mac_addr_is_valid_sta(mac_addr));
}

void mmhal_read_mac_addr(uint8_t *mac_addr)
{
    /*
     * MAC address is determined using the following precedence:
     *
     * 1. The value of the `wlan.macaddr` setting in persistent storage, if present and valid.
     *
     * 2. The MAC address in transceiver OTP (i.e., the value of mac_addr passed into this function,
     *    if non-zero).
     *
     * 3. A stable MAC address generated from the MCU’s hardware UID. This value is consistent
     *    across boots for the same device, but unique to each MCU.
     *
     * 4. Failing all of the above, the value of mac_addr will remain zero on return from this
     *    function, in which case the driver will generate a random MAC address.
     */
#ifndef EXCLUDE_MMCONFIG
    /* Read from mmconfig into a temporary as we only override the value in mac_addr once the
     * mmconfig MAC has been validated in case mac_addr already contains a MAC address. */
    uint8_t macval[MMWLAN_MAC_ADDR_LEN];
    if (mmconfig_read_mac_addr("wlan.macaddr", macval) == MMCONFIG_OK)
    {
        if (mm_mac_addr_is_valid_sta(macval))
        {
            memcpy(mac_addr, macval, MMWLAN_MAC_ADDR_LEN);
            return;
        }
        MMLOG_ERR("Rejecting invalid MAC address from mmconfig wlan.macaddr: " MM_MAC_ADDR_FMT "\n",
                  MM_MAC_ADDR_VAL(macval));
    }
#endif

    if (mm_mac_addr_is_valid_sta(mac_addr))
    {
        return;
    }

    generate_stable_mac_addr_from_uid(mac_addr);
}
