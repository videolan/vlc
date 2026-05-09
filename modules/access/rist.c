/*****************************************************************************
 * rist.c: RIST (Reliable Internet Stream Transport) input module
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_interrupt.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_block.h>

#define RIST_CFG_PREFIX "rist-"
#include "rist.h"

#define NACK_FMT_RANGE 0
#define NACK_FMT_BITMASK 1

static const int nack_type_values[] = {
    NACK_FMT_RANGE, NACK_FMT_BITMASK,
};

static const char *const nack_type_names[] = {
    N_("Range"), N_("Bitmask"),
};

struct stream_sys_t
{
    struct       rist_ctx *receiver_ctx;
    int          gre_filter_dst_port;
    uint32_t     cumulative_loss;
    uint32_t     flow_id;
    bool         eof;
    int          i_recovery_buffer;
    int          i_maximum_jitter;
    struct       rist_logging_settings logging_settings;
    vlc_mutex_t  lock;
    struct       rist_data_block *rist_items[RIST_MAX_QUEUE_BUFFERS];
};

static int cb_stats(void *arg, const struct rist_stats *stats_container)
{
    stream_t *p_access = (stream_t*)arg;
    stream_sys_t *p_sys = p_access->p_sys;

    msg_Dbg(p_access, "[RIST-STATS]: %s", stats_container->stats_json);

    const struct rist_stats_receiver_flow *stats_receiver_flow = &stats_container->stats.receiver_flow;

    p_sys->cumulative_loss += stats_receiver_flow->lost;
    msg_Dbg(p_access, "[RIST-STATS]: received %"PRIu64", missing %"PRIu32", reordered %"PRIu32", recovered %"PRIu32", lost %"PRIu32", Q %.2f, max jitter (us) %"PRIu64", rtt %"PRIu32"ms, cumulative loss %"PRIu32"", 
    stats_receiver_flow->received,
    stats_receiver_flow->missing,
    stats_receiver_flow->reordered,
    stats_receiver_flow->recovered,
    stats_receiver_flow->lost,
    stats_receiver_flow->quality,
    stats_receiver_flow->max_inter_packet_spacing,
    stats_receiver_flow->rtt,
    p_sys->cumulative_loss
    );

    if ((int)stats_receiver_flow->max_inter_packet_spacing > p_sys->i_recovery_buffer * 1000)
    {
        msg_Err(p_access, "The IP network jitter exceeded your recovery buffer size, %d > %d us, you should increase the recovery buffer size or fix your source/network jitter",
            (int)stats_receiver_flow->max_inter_packet_spacing, p_sys->i_recovery_buffer *1000);
    }

    if ((int)stats_receiver_flow->rtt > (p_sys->i_recovery_buffer))
    {
        msg_Err(p_access, "The RTT between us and the sender is higher than the configured recovery buffer size, %"PRIu32" > %d ms, you should increase the recovery buffer size",
            stats_receiver_flow->rtt, p_sys->i_recovery_buffer);
    }


    vlc_mutex_lock( &p_sys->lock );
    /* Trigger the appropriate response when there is no more data */
    /* status of 1 is no data for one buffer length */
    /* status of 2 is no data for 60 seconds, i.e. session timeout */
    if (p_sys->flow_id == stats_receiver_flow->flow_id && stats_receiver_flow->status == 2) {
        p_sys->eof = true;
    }
    vlc_mutex_unlock( &p_sys->lock );

    rist_stats_free(stats_container);
    return 0;
}

static int Control(stream_t *p_access, int i_query, va_list args)
{
    switch( i_query )
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            *va_arg( args, bool * ) = false;
            break;

        case STREAM_GET_PTS_DELAY:
            *va_arg( args, int64_t * ) = INT64_C(1000)
                   * var_InheritInteger(p_access, "network-caching");
            break;

        default:
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static block_t *BlockRIST(stream_t *p_access, bool *restrict eof)
{
    stream_sys_t *p_sys = p_access->p_sys;
    block_t *pktout = NULL;
    struct rist_data_block *rist_buffer = NULL;
    size_t i_total_size, i_rist_items_index;
    int i_flags, ret;
    *eof = false;
    i_rist_items_index = i_flags = i_total_size = 0;
    int i_read_timeout_ms = p_sys->i_maximum_jitter;

    while ((ret = rist_receiver_data_read2(p_sys->receiver_ctx, &rist_buffer, i_read_timeout_ms)) > 0)
    {
        if (p_sys->gre_filter_dst_port > 0 && rist_buffer->virt_dst_port != p_sys->gre_filter_dst_port) {
            rist_receiver_data_block_free2(&rist_buffer);
            continue;
        }
        i_read_timeout_ms = 0;

        p_sys->rist_items[i_rist_items_index++] = rist_buffer;
        i_total_size += rist_buffer->payload_len;
        vlc_mutex_lock( &p_sys->lock );
        if (p_sys->flow_id != rist_buffer->flow_id ||
            rist_buffer->flags == RIST_DATA_FLAGS_DISCONTINUITY ||
            rist_buffer->flags == RIST_DATA_FLAGS_FLOW_BUFFER_START) {
            if (p_sys->flow_id != rist_buffer->flow_id) {
                msg_Info(p_access, "New flow detected with id %"PRIu32"", rist_buffer->flow_id);
                p_sys->flow_id = rist_buffer->flow_id;
            }
            i_flags = BLOCK_FLAG_DISCONTINUITY;
            vlc_mutex_unlock( &p_sys->lock );
            break;
        }
        vlc_mutex_unlock( &p_sys->lock );
        // Make sure we never read more than our array size
        if (i_rist_items_index == (RIST_MAX_QUEUE_BUFFERS -1))
            break;
    }

    if (ret > 50)
        msg_Dbg(p_access, "Falling behind reading rist buffer by %d packets", ret);

    if (ret < 0) {
        msg_Err(p_access, "Unrecoverable error %i while reading from rist, ending stream", ret);
        *eof = true;
        goto failed_cleanup;
    }

    if (i_total_size == 0) {
        return NULL;
    }

    // Prepare one large buffer (when we are behing in reading, otherwise it is the same size as what is being read)
    pktout = block_Alloc(i_total_size);
    if ( unlikely(pktout == NULL) ) {
        goto failed_cleanup;
    }

    size_t block_offset = 0;
    for(size_t i = 0; i < i_rist_items_index; ++i) {
        memcpy(pktout->p_buffer + block_offset,  p_sys->rist_items[i]->payload,  p_sys->rist_items[i]->payload_len);
        block_offset +=  p_sys->rist_items[i]->payload_len;
        rist_receiver_data_block_free2(& p_sys->rist_items[i]);
    }
    pktout->i_flags = i_flags;
    return pktout;

failed_cleanup:
    if (i_total_size > 0) {
        for (size_t i = 0; i < i_rist_items_index; i++) {
            rist_receiver_data_block_free2(&p_sys->rist_items[i]);
        }
    }
    return NULL;
}


static void Close(vlc_object_t *p_this)
{
    stream_t *p_access = (stream_t*)p_this;
    stream_sys_t *p_sys = p_access->p_sys;
    rist_destroy(p_sys->receiver_ctx);
}

static int Open(vlc_object_t *p_this)
{
    stream_t     *p_access = (stream_t*)p_this;
    stream_sys_t *p_sys = NULL;

    p_sys = vlc_obj_calloc( p_this, 1, sizeof( *p_sys ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    p_access->p_sys = p_sys;

    vlc_mutex_init( &p_sys->lock );

    int rist_profile = var_InheritInteger(p_access, RIST_CFG_PREFIX RIST_URL_PARAM_PROFILE);
    p_sys->i_maximum_jitter = var_InheritInteger(p_access, RIST_CFG_PREFIX "maximum-jitter");
    p_sys->gre_filter_dst_port = var_InheritInteger(p_access, RIST_CFG_PREFIX RIST_URL_PARAM_VIRT_DST_PORT);
    if (p_sys->gre_filter_dst_port % 2 != 0) {
        msg_Err(p_access, "Virtual destination port must be an even number.");
        return VLC_EGENERIC;
    }
    if (rist_profile == RIST_PROFILE_SIMPLE)
        p_sys->gre_filter_dst_port = 0;

    int i_recovery_length = var_InheritInteger(p_access, RIST_CFG_PREFIX RIST_CFG_LATENCY);
    if (i_recovery_length == 0) {
        // Auto-configure the recovery buffer
        i_recovery_length = 1000;//1 Second, libRIST default
    }
    p_sys->i_recovery_buffer = i_recovery_length;

    int i_verbose_level = var_InheritInteger( p_access, RIST_CFG_PREFIX RIST_URL_PARAM_VERBOSE_LEVEL );

    //This simply disables the global logs, which are only used by the udpsocket functions provided
    //by libRIST. When called by the library logs are anyway generated (though perhaps less accurate).
    //This prevents all sorts of complications wrt other rist modules running and other the like.
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

    if (rist_receiver_create(&p_sys->receiver_ctx, rist_profile, logging_settings) != 0) {
        msg_Err(p_access, "Could not create rist receiver context");
        return VLC_EGENERIC;
    }

    int nack_type = var_InheritInteger( p_access, RIST_CFG_PREFIX "nack-type" );
    if (rist_receiver_nack_type_set(p_sys->receiver_ctx, nack_type)) {
        msg_Err(p_access, "Could not set nack type");
        goto failed;
    }

    // Enable stats data
    if (rist_stats_callback_set(p_sys->receiver_ctx, 1000, cb_stats, (void *)p_access) == -1) {
        msg_Err(p_access, "Could not enable stats callback");
        goto failed;
    }

    if( !rist_add_peers(VLC_OBJECT(p_access), p_sys->receiver_ctx, p_access->psz_url, 0, RIST_DEFAULT_VIRT_DST_PORT, i_recovery_length) )
        goto failed;

    /* Start the rist protocol thread */
    if (rist_start(p_sys->receiver_ctx)) {
        msg_Err(p_access, "Could not start rist receiver");
        goto failed;
    }

    p_access->pf_block = BlockRIST;
    p_access->pf_control = Control;

    return VLC_SUCCESS;

failed:
    rist_destroy(p_sys->receiver_ctx);
    msg_Err(p_access, "Failed to open rist module");
    return VLC_EGENERIC;
}

#define DST_PORT_TEXT N_("Virtual Destination Port Filter")
#define DST_PORT_LONGTEXT N_( \
    "Destination port to be used inside the reduced-mode of the main profile "\
    "to filter incoming data. Use zero to allow all." )

/* Module descriptor */
vlc_module_begin ()

    set_shortname( N_("RIST") )
    set_description( N_("RIST input") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )

    add_integer( RIST_CFG_PREFIX "maximum-jitter", 5,
        N_("RIST demux/decode maximum jitter (default is 5ms)"),
        N_("This controls the maximum jitter that will be passed to the demux/decode chain. "
            "The lower the value, the more CPU cycles the module will consume"),
            true)
    add_integer( RIST_CFG_PREFIX "nack-type", NACK_FMT_RANGE,
            N_("RIST nack type, 0 = range, 1 = bitmask. Default is range"), NULL, true)
        change_integer_list( nack_type_values, nack_type_names )
    add_integer( RIST_CFG_PREFIX RIST_URL_PARAM_VIRT_DST_PORT, 0,
            DST_PORT_TEXT, DST_PORT_LONGTEXT, true )
    add_integer( RIST_CFG_PREFIX RIST_CFG_MAX_PACKET_SIZE, RIST_MAX_PACKET_SIZE,
            RIST_PACKET_SIZE_TEXT, NULL, true )
    add_string( RIST_CFG_PREFIX RIST_CFG_URL2, NULL, RIST_URL2_TEXT, NULL, true )
    add_string( RIST_CFG_PREFIX RIST_CFG_URL3, NULL, RIST_URL3_TEXT, NULL, true )
    add_string( RIST_CFG_PREFIX RIST_CFG_URL4, NULL, RIST_URL4_TEXT, NULL, true )
    add_integer( RIST_CFG_PREFIX RIST_URL_PARAM_BANDWIDTH, RIST_DEFAULT_RECOVERY_MAXBITRATE,
            RIST_MAX_BITRATE_TEXT, RIST_MAX_BITRATE_LONGTEXT, true )
    add_integer( RIST_CFG_PREFIX RIST_CFG_RETRY_INTERVAL, RIST_DEFAULT_RECOVERY_RTT_MIN, 
        RIST_RETRY_INTERVAL_TEXT, NULL, true )
    add_integer( RIST_CFG_PREFIX RIST_URL_PARAM_REORDER_BUFFER, RIST_DEFAULT_RECOVERY_REORDER_BUFFER, 
        RIST_REORDER_BUFFER_TEXT, NULL, true )
    add_integer( RIST_CFG_PREFIX RIST_CFG_MAX_RETRIES, RIST_DEFAULT_MAX_RETRIES, 
        RIST_MAX_RETRIES_TEXT, NULL, true )
    add_integer( RIST_CFG_PREFIX RIST_URL_PARAM_VERBOSE_LEVEL, RIST_DEFAULT_VERBOSE_LEVEL,
            RIST_VERBOSE_LEVEL_TEXT, RIST_VERBOSE_LEVEL_LONGTEXT, true )
        change_integer_list( verbose_level_type, verbose_level_type_names )
    add_integer( RIST_CFG_PREFIX RIST_CFG_LATENCY, 0,
            BUFFER_TEXT, BUFFER_LONGTEXT, true )
    add_string( RIST_CFG_PREFIX RIST_URL_PARAM_CNAME, NULL, RIST_CNAME_TEXT, 
            RIST_CNAME_LONGTEXT, true )
    add_integer( RIST_CFG_PREFIX RIST_URL_PARAM_PROFILE, RIST_DEFAULT_PROFILE,
            RIST_PROFILE_TEXT, RIST_PROFILE_LONGTEXT, true )
    add_password( RIST_CFG_PREFIX RIST_URL_PARAM_SECRET, "",
            RIST_SHARED_SECRET_TEXT, NULL, true )
    add_integer( RIST_CFG_PREFIX RIST_URL_PARAM_AES_TYPE, 0,
            RIST_ENCRYPTION_TYPE_TEXT, NULL, true )
        change_integer_list( rist_encryption_type, rist_encryption_type_names )
    add_integer( RIST_CFG_PREFIX RIST_URL_PARAM_TIMING_MODE, RIST_DEFAULT_TIMING_MODE,
            RIST_TIMING_MODE_TEXT, NULL, true )
        change_integer_list( rist_timing_mode_type, rist_timing_mode_names )
    add_string( RIST_CFG_PREFIX RIST_URL_PARAM_SRP_USERNAME, "",
            RIST_SRP_USERNAME_TEXT, NULL, true )
    add_password( RIST_CFG_PREFIX RIST_URL_PARAM_SRP_PASSWORD, "",
            RIST_SRP_PASSWORD_TEXT, NULL, true )

    set_capability( "access", 10 )
    add_shortcut( "librist", "rist", "tr06" )

    set_callbacks( Open, Close )

vlc_module_end ()

