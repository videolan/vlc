/*****************************************************************************
 * rist.c: RIST (Reliable Internet Stream Transport) output module
 *****************************************************************************
 * Copyright (C) 2021, SipRadius LLC
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_interrupt.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_rand.h>
#include <sys/time.h>

#define RIST_CFG_PREFIX "sout-rist-"
#include "../access/rist.h"

static const char *const ppsz_sout_options[] = {
    RIST_CFG_MAX_PACKET_SIZE,
    RIST_URL_PARAM_VIRT_SRC_PORT,
    RIST_URL_PARAM_VIRT_DST_PORT,
    RIST_CFG_LATENCY,
    "multipeer-mode",
    RIST_CFG_URL2,
    RIST_CFG_URL3,
    RIST_CFG_URL4,
    RIST_URL_PARAM_BANDWIDTH,
    RIST_CFG_RETRY_INTERVAL,
    RIST_URL_PARAM_REORDER_BUFFER,
    RIST_CFG_MAX_RETRIES,
    RIST_URL_PARAM_VERBOSE_LEVEL,
    RIST_URL_PARAM_CNAME,
    RIST_URL_PARAM_PROFILE,
    RIST_URL_PARAM_SECRET,
    RIST_URL_PARAM_AES_TYPE,
    RIST_URL_PARAM_SRP_USERNAME,
    RIST_URL_PARAM_SRP_PASSWORD,
    NULL
};

typedef struct
{
    struct rist_ctx    *sender_ctx;
    int                gre_src_port;
    int                gre_dst_port;
    uint32_t           i_recovery_buffer;
    size_t             i_max_packet_size;
    struct rist_logging_settings logging_settings;
} sout_access_out_sys_t;

static int cb_stats(void *arg, const struct rist_stats *stats_container)
{
    sout_access_out_t *p_access = (sout_access_out_t*)arg;
    sout_access_out_sys_t *p_sys = p_access->p_sys;

    msg_Dbg(p_access, "[RIST-STATS]: %s", stats_container->stats_json);

    const struct rist_stats_sender_peer *stats_sender_peer = &stats_container->stats.sender_peer;
    msg_Dbg(p_access, "[RIST-STATS]: name %s, id %"PRIu32", bitrate %zu, sent %"PRIu64", received %"PRIu64", retransmitted %"PRIu64", Q %.2f, rtt %"PRIu32"ms",
        stats_sender_peer->cname,
        stats_sender_peer->peer_id,
        stats_sender_peer->bandwidth,
        stats_sender_peer->sent,
        stats_sender_peer->received,
        stats_sender_peer->retransmitted,
        stats_sender_peer->quality,
        stats_sender_peer->rtt
    );

    if (stats_sender_peer->rtt > p_sys->i_recovery_buffer)
    {
        msg_Err(p_access, "The RTT between us and the receiver is higher than the configured recovery buffer size, %"PRIu32" > %"PRIu32" ms, you should increase the recovery buffer size",
            stats_sender_peer->rtt, p_sys->i_recovery_buffer);
    }

    rist_stats_free(stats_container);
    return 0;
}

static ssize_t Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    sout_access_out_sys_t *p_sys = p_access->p_sys;
    int i_len = 0;

    struct rist_data_block rist_buffer = { 0 };
    rist_buffer.virt_src_port = p_sys->gre_src_port;
    rist_buffer.virt_dst_port = p_sys->gre_dst_port;

    while( p_buffer )
    {
        block_t *p_next;

        i_len += p_buffer->i_buffer;

        while( p_buffer->i_buffer )
        {
            size_t i_write = __MIN( p_buffer->i_buffer, p_sys->i_max_packet_size );
            rist_buffer.payload = p_buffer->p_buffer;
            rist_buffer.payload_len = p_buffer->i_buffer;
            rist_sender_data_write(p_sys->sender_ctx, &rist_buffer);
            p_buffer->p_buffer += i_write;
            p_buffer->i_buffer -= i_write;
        }

        p_next = p_buffer->p_next;
        block_Release( p_buffer );
        p_buffer = p_next;

    }
    return i_len;
}

static int Control( sout_access_out_t *p_access, int i_query, va_list args )
{
    VLC_UNUSED( p_access );

    int i_ret = VLC_SUCCESS;

    switch( i_query )
    {
        case ACCESS_OUT_CONTROLS_PACE:
            *va_arg( args, bool * ) = false;
            break;

        default:
            i_ret = VLC_EGENERIC;
            break;
    }

    return i_ret;
}

static void Close( vlc_object_t * p_this )
{
    sout_access_out_t     *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t *p_sys = p_access->p_sys;

    rist_destroy(p_sys->sender_ctx);
    p_sys->sender_ctx = NULL;
}

static int Open( vlc_object_t *p_this )
{
    sout_access_out_t       *p_access = (sout_access_out_t*)p_this;
    sout_access_out_sys_t   *p_sys = NULL;

    if (var_Create ( p_access, "dst-port", VLC_VAR_INTEGER )
     || var_Create ( p_access, "src-port", VLC_VAR_INTEGER )
     || var_Create ( p_access, "dst-addr", VLC_VAR_STRING )
     || var_Create ( p_access, "src-addr", VLC_VAR_STRING ) )
    {
        msg_Err( p_access, "Valid network information is required." );
        return VLC_EGENERIC;
    }

    config_ChainParse( p_access, RIST_CFG_PREFIX, ppsz_sout_options, p_access->p_cfg );

    p_sys = vlc_obj_calloc( p_this, 1, sizeof( *p_sys ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    p_access->p_sys = p_sys;


    p_sys->gre_src_port = var_InheritInteger(p_access, RIST_CFG_PREFIX RIST_URL_PARAM_VIRT_SRC_PORT);
    p_sys->gre_dst_port = var_InheritInteger(p_access, RIST_CFG_PREFIX RIST_URL_PARAM_VIRT_DST_PORT);
    if (p_sys->gre_dst_port % 2 != 0) {
        msg_Err( p_access, "Virtual destination port must be an even number." );
        return VLC_EGENERIC;
    }

    p_sys->i_max_packet_size = rist_get_max_packet_size(VLC_OBJECT(p_access));

    int i_rist_profile = var_InheritInteger(p_access, RIST_CFG_PREFIX RIST_URL_PARAM_PROFILE);
    int i_verbose_level = var_InheritInteger(p_access, RIST_CFG_PREFIX RIST_URL_PARAM_VERBOSE_LEVEL);


    //disable global logging: see comment in access/rist.c
    struct rist_logging_settings rist_global_logging_settings = LOGGING_SETTINGS_INITIALIZER;
    if (rist_logging_set_global(&rist_global_logging_settings) != 0) {
        msg_Err(p_access,"Could not set logging\n");
        return VLC_EGENERIC;
    }

    struct rist_logging_settings *logging_settings = &p_sys->logging_settings;
    logging_settings->log_socket = -1;
    logging_settings->log_stream = NULL;
    logging_settings->log_level = i_verbose_level;
    logging_settings->log_cb = log_cb;
    logging_settings->log_cb_arg = p_access;

    if (rist_sender_create(&p_sys->sender_ctx, i_rist_profile, 0, logging_settings) != 0) {
        msg_Err( p_access, "Could not create rist sender context\n");
        return VLC_EGENERIC;
    }

    // Enable stats data
    if (rist_stats_callback_set(p_sys->sender_ctx, 1000, cb_stats, (void *)p_access) == -1) {
        msg_Err(p_access, "Could not enable stats callback");
        goto failed;
    }

    int i_multipeer_mode = var_InheritInteger(p_access, RIST_CFG_PREFIX "multipeer-mode");
    int i_recovery_length = var_InheritInteger(p_access, RIST_CFG_PREFIX RIST_CFG_LATENCY);
    p_sys->i_recovery_buffer = i_recovery_length;

    if ( !rist_add_peers(VLC_OBJECT(p_access), p_sys->sender_ctx, p_access->psz_path, i_multipeer_mode, p_sys->gre_dst_port + 1, i_recovery_length) )
        goto failed;

    if (rist_start(p_sys->sender_ctx) == -1) {
        msg_Err( p_access, "Could not start rist sender\n");
        goto failed;
    }

    p_access->pf_write = Write;
    p_access->pf_control = Control;

    return VLC_SUCCESS;

failed:
    rist_destroy(p_sys->sender_ctx);
    p_sys->sender_ctx = NULL;
    return VLC_EGENERIC;
}

#define SRC_PORT_TEXT N_("Virtual Source Port")
#define SRC_PORT_LONGTEXT N_( \
    "Source port to be used inside the reduced-mode of the main profile" )

#define DST_PORT_TEXT N_("Virtual Destination Port")
#define DST_PORT_LONGTEXT N_( \
    "Destination port to be used inside the reduced-mode of the main profile" )

/* The default target payload size */
#define RIST_DEFAULT_TARGET_PAYLOAD_SIZE 1316

/* Multipeer mode */
#define RIST_DEFAULT_MULTIPEER_MODE 0
#define RIST_MULTIPEER_MODE_TEXT N_("Multipeer mode")
#define RIST_MULTIPEER_MODE_LONGTEXT N_( \
    "This allows you to select between duplicate or load balanced modes when " \
    "sending data to multiple peers (several network paths)" )
static const int multipeer_mode_type[] = { 0, 5, };
static const char *const multipeer_mode_type_names[] = {
    N_("Duplicate"), N_("Load balanced"),
};

/* Module descriptor */
vlc_module_begin()

    set_shortname( N_("RIST") )
    set_description( N_("RIST stream output") )
    set_subcategory( SUBCAT_SOUT_ACO )

    add_integer( RIST_CFG_PREFIX RIST_CFG_MAX_PACKET_SIZE, RIST_DEFAULT_TARGET_PAYLOAD_SIZE,
            N_("RIST target payload size (bytes)"), NULL )
    add_integer( RIST_CFG_PREFIX RIST_URL_PARAM_VIRT_SRC_PORT, RIST_DEFAULT_VIRT_SRC_PORT,
            SRC_PORT_TEXT, SRC_PORT_LONGTEXT )
    add_integer( RIST_CFG_PREFIX RIST_URL_PARAM_VIRT_DST_PORT, RIST_DEFAULT_VIRT_DST_PORT,
            DST_PORT_TEXT, DST_PORT_LONGTEXT )
    add_integer( RIST_CFG_PREFIX "multipeer-mode", RIST_DEFAULT_MULTIPEER_MODE,
            RIST_MULTIPEER_MODE_TEXT, RIST_MULTIPEER_MODE_LONGTEXT )
        change_integer_list( multipeer_mode_type, multipeer_mode_type_names )
    add_string( RIST_CFG_PREFIX RIST_CFG_URL2, NULL, RIST_URL2_TEXT, NULL )
    add_string( RIST_CFG_PREFIX RIST_CFG_URL3, NULL, RIST_URL3_TEXT, NULL )
    add_string( RIST_CFG_PREFIX RIST_CFG_URL4, NULL, RIST_URL4_TEXT, NULL )
    add_integer( RIST_CFG_PREFIX RIST_URL_PARAM_BANDWIDTH, RIST_DEFAULT_RECOVERY_MAXBITRATE,
            RIST_MAX_BITRATE_TEXT, RIST_MAX_BITRATE_LONGTEXT )
    add_integer( RIST_CFG_PREFIX RIST_CFG_RETRY_INTERVAL, RIST_DEFAULT_RECOVERY_RTT_MIN, 
        RIST_RETRY_INTERVAL_TEXT, NULL )
    add_integer( RIST_CFG_PREFIX RIST_URL_PARAM_REORDER_BUFFER, RIST_DEFAULT_RECOVERY_REORDER_BUFFER, 
        RIST_REORDER_BUFFER_TEXT, NULL )
    add_integer( RIST_CFG_PREFIX RIST_CFG_MAX_RETRIES, RIST_DEFAULT_MAX_RETRIES, 
        RIST_MAX_RETRIES_TEXT, NULL )
    add_integer( RIST_CFG_PREFIX RIST_URL_PARAM_VERBOSE_LEVEL, RIST_DEFAULT_VERBOSE_LEVEL,
            RIST_VERBOSE_LEVEL_TEXT, RIST_VERBOSE_LEVEL_LONGTEXT )
        change_integer_list( verbose_level_type, verbose_level_type_names )
    add_integer( RIST_CFG_PREFIX RIST_CFG_LATENCY, RIST_DEFAULT_RECOVERY_LENGHT_MIN,
            BUFFER_TEXT, BUFFER_LONGTEXT )
    add_string( RIST_CFG_PREFIX RIST_URL_PARAM_CNAME, NULL, RIST_CNAME_TEXT, 
            RIST_CNAME_LONGTEXT )
    add_integer( RIST_CFG_PREFIX RIST_URL_PARAM_PROFILE, RIST_DEFAULT_PROFILE,
            RIST_PROFILE_TEXT, RIST_PROFILE_LONGTEXT )
    add_password( RIST_CFG_PREFIX RIST_URL_PARAM_SECRET, "",
            RIST_SHARED_SECRET_TEXT, NULL )
    add_integer( RIST_CFG_PREFIX RIST_URL_PARAM_AES_TYPE, 0,
            RIST_ENCRYPTION_TYPE_TEXT, NULL )
        change_integer_list( rist_encryption_type, rist_encryption_type_names )
    add_string( RIST_CFG_PREFIX RIST_URL_PARAM_SRP_USERNAME, "",
            RIST_SRP_USERNAME_TEXT, NULL )
    add_password( RIST_CFG_PREFIX RIST_URL_PARAM_SRP_PASSWORD, "",
            RIST_SRP_PASSWORD_TEXT, NULL )

    set_capability( "sout access", 10 )
    add_shortcut( "librist", "rist", "tr06" )

    set_callbacks( Open, Close )

vlc_module_end ()

