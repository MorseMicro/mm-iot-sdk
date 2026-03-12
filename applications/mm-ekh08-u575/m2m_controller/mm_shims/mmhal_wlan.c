/*
 * Copyright 2021-2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmhal_wlan.h"
#include "mmosal.h"
#include "mmconfig.h"
#include "mmutils.h"
#include "main.h"

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
}

/**
 * Attempts to read a MAC address from the "wlan.macaddr" key in mmconfig persistent configuration.
 *
 * @param mac_addr Location where the MAC address will be stored if there is a valid MAC address in
 *                 mmconfig persistent storage.
 */
static void get_mmconfig_mac_addr(uint8_t *mac_addr)
{
    char strval[32];
    if (mmconfig_read_string("wlan.macaddr", strval, sizeof(strval)) > 0)
    {
        /* Need to provide an array of ints to sscanf otherwise it will overflow */
        int temp[MMWLAN_MAC_ADDR_LEN];
        uint8_t validated_mac[MMWLAN_MAC_ADDR_LEN];
        int i;

        int ret = sscanf(strval, "%x:%x:%x:%x:%x:%x",
                         &temp[0], &temp[1], &temp[2],
                         &temp[3], &temp[4], &temp[5]);
        if (ret == MMWLAN_MAC_ADDR_LEN)
        {
            for (i = 0; i < MMWLAN_MAC_ADDR_LEN; i++)
            {
                if (temp[i] > UINT8_MAX || temp[i] < 0)
                {
                    /* Invalid value, ignore and exit without updating mac_addr */
                    printf("Invalid MAC address found in [wlan.macaddr], rejecting!\n");
                    return;
                }
                validated_mac[i] = (uint8_t)temp[i];
            }
            /* We only override the value in mac_addr once the entire mmconfig MAC has been
             * validated in case mac_addr already contains a MAC address. */
            memcpy(mac_addr, validated_mac, MMWLAN_MAC_ADDR_LEN);
        }
    }
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

    get_mmconfig_mac_addr(mac_addr);

    if (!mm_mac_addr_is_zero(mac_addr))
    {
        return;
    }

    generate_stable_mac_addr_from_uid(mac_addr);
}
