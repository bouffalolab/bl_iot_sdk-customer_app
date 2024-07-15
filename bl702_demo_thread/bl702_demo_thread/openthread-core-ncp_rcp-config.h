/*
 *  Copyright (c) 2016, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef OPENTHREAD_CORE_NCP_CONFIG_H
#define OPENTHREAD_CORE_NCP_CONFIG_H

#define OT_TASK_SIZE 1024
#define OT_TASK_PRORITY 20

#define PACKAGE_NAME "OpenThread"
#define PACKAGE_VERSION "7e32165be"

#define OPENTHREAD_CONFIG_PLATFORM_INFO "BL702"

// #define OPENTHREAD_CONFIG_LOG_LEVEL OT_LOG_LEVEL_DEBG
#define OPENTHREAD_CONFIG_LOG_OUTPUT OPENTHREAD_CONFIG_LOG_OUTPUT_APP
#define OPENTHREAD_CONFIG_PLATFORM_ASSERT_MANAGEMENT 1

#define OPENTHREAD_CONFIG_NCP_HDLC_ENABLE 1
#define OPENTHREAD_ENABLE_NCP_SPINEL_ENCRYPTER 0
#if (OPENTHREAD_FTD) || (OPENTHREAD_MTD)
#define OPENTHREAD_SPINEL_CONFIG_OPENTHREAD_MESSAGE_ENABLE 1
#else
#define OPENTHREAD_SPINEL_CONFIG_OPENTHREAD_MESSAGE_ENABLE 0
#endif

#define OPENTHREAD_CONFIG_PING_SENDER_ENABLE 1

#define OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE 1
#define OPENTHREAD_CONFIG_MLE_LINK_METRICS_INITIATOR_ENABLE 1

#define OPENTHREAD_CONFIG_TMF_PROXY_MLR_ENABLE 1
#define OPENTHREAD_CONFIG_DUA_ENABLE 1
#define OPENTHREAD_CONFIG_MLR_ENABLE 1

#define OPENTHREAD_CONFIG_IP6_FRAGMENTATION_ENABLE 1

#define OPENTHREAD_CONFIG_TMF_NETDIAG_CLIENT_ENABLE 1
#define OPENTHREAD_CONFIG_TMF_NETDATA_SERVICE_ENABLE 1
#define OPENTHREAD_CONFIG_TMF_ANYCAST_LOCATOR_ENABLE 1
#define OPENTHREAD_CONFIG_DATASET_UPDATER_ENABLE 1

#define OPENTHREAD_CONFIG_COAP_API_ENABLE 1
#define OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE 1
#define OPENTHREAD_CONFIG_COAP_BLOCKWISE_TRANSFER_ENABLE 1

#define OPENTHREAD_CONFIG_IP6_SLAAC_ENABLE 1
#define OPENTHREAD_CONFIG_DHCP6_CLIENT_ENABLE 1
#define OPENTHREAD_CONFIG_DNS_CLIENT_ENABLE 1
#define OPENTHREAD_CONFIG_SNTP_CLIENT_ENABLE 1

#define OPENTHREAD_CONFIG_NEIGHBOR_DISCOVERY_AGENT_ENABLE 1

#define OPENTHREAD_CONFIG_ECDSA_ENABLE 1
#define OPENTHREAD_CONFIG_SRP_CLIENT_ENABLE 1
#define OPENTHREAD_CONFIG_SRP_CLIENT_AUTO_START_API_ENABLE 1

// #define OPENTHREAD_CONFIG_MAC_BEACON_PAYLOAD_PARSING_ENABLE 1
// #define OPENTHREAD_CONFIG_MAC_OUTGOING_BEACON_PAYLOAD_ENABLE 1

#define OPENTHREAD_CONFIG_SRP_SERVER_ENABLE 0
#define OPENTHREAD_CONFIG_BORDER_ROUTER_ENABLE 0
#define OPENTHREAD_CONFIG_BACKBONE_ROUTER_ENABLE 0

#define OPENTHREAD_CONFIG_MAC_SOFTWARE_ACK_TIMEOUT_ENABLE 0
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_RETRANSMIT_ENABLE 1
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_CSMA_BACKOFF_ENABLE 0
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_TX_SECURITY_ENABLE 0
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_TX_TIMING_ENABLE 0
#define OPENTHREAD_CONFIG_MAC_SOFTWARE_ENERGY_SCAN_ENABLE 1

#define OPENTHREAD_CONFIG_PLATFORM_USEC_TIMER_ENABLE 1
#define OPENTHREAD_CONFIG_HEAP_EXTERNAL_ENABLE 1

#define OPENTHREAD_EXAMPLES_SIMULATION 0
#define OPENTHREAD_PLATFORM_POSIX 0
#define OPENTHREAD_ENABLE_VENDOR_EXTENSION 0

#define IEEE802_15_4_RADIO_RECEIVE_SENSITIVITY -105
#define OPENTHREAD_CONFIG_PLATFORM_XTAL_ACCURACY 40

#endif // OPENTHREAD_CORE_PROJ_CONFIG_H
