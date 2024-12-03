/**
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: MIT
 *
 * @file
 * Contains the definitions for @ref sntp_client.c
 */

#pragma once

/**
 * Syncs system time to network time.
 *
 * This function sends an NTP request to a server from the pool of servers
 * returned by a DNS lookup of @c server_name. The servers is picked as the first DNS
 * record returned. If no response is received from the server in the specified timeout
 * then the operation fails.
 *
 * On success this function calls mmhal_set_time() to set the system time.
 *
 * @param  server_name  Name of the NTP server/pool to connect to.
 * @param  timeout_ms   Timeout in ms for NTP sync to complete.
 * @return Returns 0 if successfully synced the time.
 */
int sntp_sync(char * server_name, int timeout_ms);

/**
 * Syncs system time to network time, backing off and retrying on failure.
 *
 * This function sends an NTP request to a server from the pool of servers
 * returned by a DNS lookup of @c server_name. The servers is picked as the first DNS
 * record returned. This function blocks until either it is successful or max_attempts is reached.
 * On success this function calls mmhal_set_time() to set the system time.
 *
 * @param  server_name  Name of the NTP server/pool to connect to.
 * @param  timeout_ms   Timeout in ms for NTP sync to complete.
 * @param min_backoff   Minimum amount of time in ms to back off if NTP sync failed.
 *                      Most NTP servers expect you to back off 1 minute before they honor
 *                      further requests.
 * @param min_jitter    The minimum jitter to add to the back off in ms, the jitter is increased
 *                      exponentially on each attempt till it reaches max_jitter.
 * @param max_jitter    The maximum jitter to add to the back off in ms.
 * @param max_attempts  The maximum number of attempts to make.
 * @return Returns 0 if successfully synced the time.
 */
int sntp_sync_with_backoff(char * server_name, int timeout_ms, uint32_t min_backoff,
                           uint16_t min_jitter, uint16_t max_jitter, uint32_t max_attempts);
