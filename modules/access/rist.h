/*****************************************************************************
 * rist.h: RIST (Reliable Internet Stream Transport) helper
 *****************************************************************************
 * Copyright (C) 2023, SipRadius LLC
 *
 * Authors: Sergio Ammirata <sergio@ammirata.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef RIST_VLC_COMMON_H
#define RIST_VLC_COMMON_H 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <librist/librist.h>

/* RIST max fifo count (private RIST_DATAOUT_QUEUE_BUFFERS in librist) */
#define RIST_MAX_QUEUE_BUFFERS (1024)

/* RIST parameter names */
#define RIST_CFG_MAX_PACKET_SIZE   "packet-size"
#define RIST_CFG_URL2              "peer-url2"
#define RIST_CFG_URL3              "peer-url3"
#define RIST_CFG_URL4              "peer-url4"
#define RIST_CFG_RETRY_INTERVAL    "retry-interval"
#define RIST_CFG_MAX_RETRIES       "max-retries"
#define RIST_CFG_LATENCY           "latency"

static const char *rist_log_label[9] = {"DISABLED", "", "", "ERROR", "WARN", "", "INFO", "DEBUG", "SIMULATE"};

/* RIST parameter descriptions */
#define RIST_URL2_TEXT N_("Second peer URL")
#define RIST_URL3_TEXT N_("Third peer URL")
#define RIST_URL4_TEXT N_("Fourth peer URL")
#define RIST_MAX_BITRATE_TEXT N_("Max bitrate in Kbps")
#define RIST_MAX_BITRATE_LONGTEXT N_( \
    "Use this value to guarantee that data+retries bitrate never exceeds your pipe size. " \
    "Default value is 100000 Kbps (100 Mbps)" )
#define RIST_RETRY_INTERVAL_TEXT N_("RIST nack minimum retry interval (ms)")
#define RIST_REORDER_BUFFER_TEXT N_("RIST reorder buffer size (ms)")
#define RIST_MAX_RETRIES_TEXT N_("RIST maximum retry count")
#define BUFFER_TEXT N_("RIST retry-buffer queue size (ms)")
#define BUFFER_LONGTEXT N_( \
    "This must match the buffer size (latency) configured on the other side. If you " \
    "are not sure, leave it blank and it will use 1000ms" )
#define RIST_SHARED_SECRET_TEXT N_("Shared Secret")
#define RIST_SHARED_SECRET_LONGTEXT N_( \
    "This shared secret is a passphare shared between sender and receiver. The AES key " \
    "is derived from it" )
#define RIST_CNAME_TEXT N_("Peer cname")
#define RIST_CNAME_LONGTEXT N_( \
    "This name will be sent using the rist RTCP channel and uniquely identifies us" )
#define RIST_PACKET_SIZE_TEXT N_("RIST maximum packet size (bytes)")
#define RIST_SRP_USERNAME_TEXT N_("Username used for stream authentication")
#define RIST_SRP_PASSWORD_TEXT N_("Password used for stream authentication")
/* Profile selection */
#define RIST_PROFILE_TEXT N_("Rist Profile")
#define RIST_PROFILE_LONGTEXT N_( "Select the rist profile to use" )
static const int rist_profile[] = { 0, 1, 2 };
static const char *const rist_profile_names[] = {
    N_("Simple Profile"), N_("Main Profile"), N_("Advanced Profile"),
};
/* Encryption type */
#define RIST_ENCRYPTION_TYPE_TEXT N_("Encryption type")
#define RIST_DEFAULT_ENCRYPTION_TPE 128
static const int rist_encryption_type[] = { 0, 128, 256, };
static const char *const rist_encryption_type_names[] = {
    N_("Disabled"), N_("AES 128 bits"), N_("AES 256 bits"),
};
/* Timing Mode */
#define RIST_TIMING_MODE_TEXT N_("Timing mode")
static const int rist_timing_mode_type[] = { 0, 1, 2, };
static const char *const rist_timing_mode_names[] = {
    N_("Use Source Time"), N_("Use Arrival Time"), N_("Use RTC"),
};
/* Verbose level */
#define RIST_VERBOSE_LEVEL_TEXT N_("Verbose level")
#define RIST_VERBOSE_LEVEL_LONGTEXT N_("This controls how much log data the library will output over stderr")
static const int verbose_level_type[] = { RIST_LOG_DISABLE, RIST_LOG_ERROR, RIST_LOG_WARN, RIST_LOG_INFO, RIST_LOG_DEBUG, RIST_LOG_SIMULATE };
static const char *const verbose_level_type_names[] = {
    N_("Quiet"), N_("Errors"), N_("Warnings"), N_("Info"), N_("Debug"), N_("Simulate-Loss"),
};

/* Max number of peers */
// This will restrict the use of the library to the configured maximum packet size
#define RIST_MAX_PACKET_SIZE (10000)

#define RIST_MAX_LOG_BUFFER 512


static inline int log_cb(void *arg, enum rist_log_level log_level, const char *msg)
{
    vlc_object_t *p_access = (vlc_object_t *)arg;
    int label_index = 0;
    if (log_level > 8)
        label_index = 8;
    else if (log_level > 0)
        label_index = log_level;

    if (log_level == RIST_LOG_ERROR)
        msg_Err(p_access, "[RIST-%s]: %.*s", rist_log_label[label_index], (int)strlen(msg) - 1, msg);
    else if (log_level == RIST_LOG_WARN)
        msg_Warn(p_access, "[RIST-%s]: %.*s", rist_log_label[label_index], (int)strlen(msg) - 1, msg);
    else if (log_level == RIST_LOG_INFO)
        //libRIST follows Syslog log priorities and assigns INFO a low prio, VLC gives Info the highest
        //so make it warn (debug would be too verbose)
        msg_Warn(p_access, "[RIST-%s]: %.*s", rist_log_label[label_index], (int)strlen(msg) - 1, msg);
    else if (log_level > RIST_LOG_DEBUG)
        msg_Dbg(p_access, "[RIST-%s]: %.*s", rist_log_label[label_index], (int)strlen(msg) - 1, msg);

    return 0;
}

static inline int rist_get_max_packet_size(vlc_object_t *p_this)
{
    int i_max_packet_size = var_InheritInteger( p_this, RIST_CFG_PREFIX RIST_CFG_MAX_PACKET_SIZE );
    if (i_max_packet_size > RIST_MAX_PACKET_SIZE)
    {
        msg_Err(p_this, "The maximum packet size configured is bigger than what the library allows %d > %d, using %d instead",
            i_max_packet_size, RIST_MAX_PACKET_SIZE, RIST_MAX_PACKET_SIZE);
        i_max_packet_size = RIST_MAX_PACKET_SIZE;
    }
    return i_max_packet_size;
}

static inline bool rist_add_peers(vlc_object_t *p_this, struct rist_ctx *ctx, char *psz_url, int i_multipeer_mode, int virt_dst_port, int i_recovery_length)
{


    int i_rist_reorder_buffer = var_InheritInteger( p_this, RIST_CFG_PREFIX RIST_URL_PARAM_REORDER_BUFFER );
    int i_rist_retry_interval = var_InheritInteger( p_this, RIST_CFG_PREFIX RIST_CFG_RETRY_INTERVAL );
    int i_rist_max_retries = var_InheritInteger( p_this, RIST_CFG_PREFIX RIST_CFG_MAX_RETRIES );
    int i_rist_max_bitrate = var_InheritInteger(p_this, RIST_CFG_PREFIX RIST_URL_PARAM_BANDWIDTH);

    char *psz_stream_name = NULL;
    psz_stream_name = var_InheritString( p_this, RIST_CFG_PREFIX RIST_URL_PARAM_CNAME );

    char *psz_shared_secret = NULL;
    psz_shared_secret = var_InheritString( p_this, RIST_CFG_PREFIX RIST_URL_PARAM_SECRET );

    int i_key_size = var_InheritInteger(p_this, RIST_CFG_PREFIX RIST_URL_PARAM_AES_TYPE);

    char *psz_srp_username = NULL;
    psz_srp_username = var_InheritString( p_this, RIST_CFG_PREFIX RIST_URL_PARAM_SRP_USERNAME );
    char *psz_srp_password = NULL;
    psz_srp_password = var_InheritString( p_this, RIST_CFG_PREFIX RIST_URL_PARAM_SRP_PASSWORD );

    int recovery_mode = RIST_RECOVERY_MODE_TIME;

    msg_Info( p_this, "Setting retry buffer to %d ms", i_recovery_length );

    char *addr[] = {
        strdup(psz_url),
        var_InheritString( p_this, RIST_CFG_PREFIX RIST_CFG_URL2 ),
        var_InheritString( p_this, RIST_CFG_PREFIX RIST_CFG_URL3 ),
        var_InheritString( p_this, RIST_CFG_PREFIX RIST_CFG_URL4 )
    };

    bool b_peer_alive = false;
    for (size_t i = 0; i < ARRAY_SIZE(addr); i++) {
        if (addr[i] == NULL) {
            continue;
        }
        else if (addr[i][0] == '\0') {
            free(addr[i]);
            continue;
        }

        struct rist_peer_config app_peer_config = {
            .version = RIST_PEER_CONFIG_VERSION,
            .virt_dst_port = virt_dst_port,
            .recovery_mode = recovery_mode,
            .recovery_maxbitrate = (uint32_t)i_rist_max_bitrate,
            .recovery_maxbitrate_return = 0,
            .recovery_length_min = (uint32_t)i_recovery_length,
            .recovery_length_max = (uint32_t)i_recovery_length,
            .recovery_reorder_buffer = (uint32_t)i_rist_reorder_buffer,
            .recovery_rtt_min = (uint32_t)i_rist_retry_interval,
            .recovery_rtt_max = (uint32_t)10*i_rist_retry_interval,
            .weight = (uint32_t)i_multipeer_mode,
            .congestion_control_mode = RIST_CONGESTION_CONTROL_MODE_NORMAL,
            .min_retries = 6,
            .max_retries = (uint32_t)i_rist_max_retries,
            .key_size = i_key_size
        };

        if (psz_shared_secret != NULL && psz_shared_secret[0] != '\0') {
            strlcpy(app_peer_config.secret, psz_shared_secret, sizeof(app_peer_config.secret));
        }

        if ( psz_stream_name != NULL && psz_stream_name[0] != '\0') {
            strlcpy(app_peer_config.cname, psz_stream_name, sizeof(app_peer_config.cname));
        }

        // URL overrides (also cleans up the URL)
        struct rist_peer_config *peer_config = &app_peer_config;
        if (rist_parse_address2(addr[i], &peer_config))
        {
            msg_Err( p_this, "Could not parse peer options for sender #%d\n", (int)(i + 1));
            free(addr[i]);
            continue;
        }

        struct rist_peer *peer;
        if (rist_peer_create(ctx, &peer, peer_config)) {
            msg_Err( p_this, "Could not init rist sender #%i at %s",(int)(i + 1), addr[i] );
            free(addr[i]);
            continue;
        }
        else {
            b_peer_alive = true;
        }
        free(addr[i]);
    }

    free(psz_shared_secret);
    free(psz_stream_name);
    free(psz_srp_username);
    free(psz_srp_password);

    return b_peer_alive;

}

#endif

