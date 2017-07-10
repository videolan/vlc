/*****************************************************************************
 * scte18.c : SCTE-18 EAS decoder
 *****************************************************************************
 * Copyright (C) 2016 - VideoLAN Authors
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

#include "atsc_a65.h"
#include "scte18.h"
#include "substext.h"

#include <time.h>

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin ()
    set_description(N_("SCTE-18 decoder"))
    set_shortname(N_("SCTE-18"))
    set_capability( "spu decoder", 51)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_SCODEC)
    set_callbacks(Open, Close)
vlc_module_end ()

struct decoder_sys_t
{
    atsc_a65_handle_t *p_handle;
};

//#define GPS_UTC_EPOCH_OFFSET 315964800
//#define GPS_CUR_UTC_LEAP_OFFSET  16 /* 1 Jul 2015 */

typedef struct scte18_cea_t
{
    uint16_t i_eas_event_id;
    char     rgc_eas_originator_code[3];
    char *   psz_eas_event_code;
    char *   psz_nature_of_activation;
    uint8_t  alert_message_time_remaining;
    uint32_t event_start_time;
    uint16_t event_duration;
    uint8_t  alert_priority;

    char *   psz_alert_text;

} scte18_cea_t;

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
#define BUF_ADVANCE(n) p_buffer += n; i_buffer -= n;

static inline scte18_cea_t * scte18_cea_New()
{
    return calloc( 1, sizeof(scte18_cea_t) );
}

static void scte18_cea_Free( scte18_cea_t *p_cea )
{
    free( p_cea->psz_alert_text );
    free( p_cea->psz_nature_of_activation );
    free( p_cea->psz_eas_event_code );
    free( p_cea );
}

static scte18_cea_t * scte18_cea_Decode( atsc_a65_handle_t *p_handle, const block_t *p_block )
{
    size_t len;
    scte18_cea_t *p_cea = scte18_cea_New();
    if( !p_cea )
        return NULL;

    const uint8_t *p_buffer = p_block->p_buffer;
    size_t i_buffer = p_block->i_buffer;

    if( i_buffer < 34 || p_buffer[0] != 0 )
        goto error;

    BUF_ADVANCE(1);

    p_cea->i_eas_event_id = GetWBE( p_buffer );
    BUF_ADVANCE(2);

    memcpy( p_cea->rgc_eas_originator_code, p_buffer, 3 );
    BUF_ADVANCE(3);

    len = p_buffer[0];
    if( i_buffer < 23 + len )
        goto error;
    p_cea->psz_eas_event_code = malloc( len + 1 );
    memcpy( p_cea->psz_eas_event_code, &p_buffer[1], len );
    p_cea->psz_eas_event_code[len] = 0;
    BUF_ADVANCE( len + 1 );

    len = p_buffer[0];
    if( i_buffer < len + 22 )
        goto error;
    p_cea->psz_nature_of_activation = atsc_a65_Decode_multiple_string( p_handle, &p_buffer[1], len );
    BUF_ADVANCE(1 + len);

    if( i_buffer < 21 )
        goto error;
    p_cea->alert_message_time_remaining = p_buffer[0];
    BUF_ADVANCE(1);

    p_cea->event_start_time = GetDWBE( p_buffer );
    BUF_ADVANCE(4);

    p_cea->event_duration = GetWBE( p_buffer );
    if( p_cea->event_duration != 0 && ( p_cea->event_duration < 15 || p_cea->event_duration > 6000 ) )
        goto error;
    BUF_ADVANCE(2);

    p_cea->alert_priority = p_buffer[1] & 0x0f;
    switch( p_cea->alert_priority )
    {
        case EAS_PRIORITY_TEST:
        case EAS_PRIORITY_LOW:
        case EAS_PRIORITY_MEDIUM:
        case EAS_PRIORITY_HIGH:
        case EAS_PRIORITY_MAX:
            break;
        default:
            goto error;
    }

    BUF_ADVANCE(2);

    BUF_ADVANCE(2); //OOB_ID

    BUF_ADVANCE(2); //
    BUF_ADVANCE(2); //

    BUF_ADVANCE(2); //audio_OOB_ID

    len = GetWBE( p_buffer );
    if( i_buffer < len + 2 )
        goto error;
    p_cea->psz_alert_text = atsc_a65_Decode_multiple_string( p_handle, &p_buffer[2], len );

    return p_cea;

error:
    scte18_cea_Free( p_cea );
    return NULL;
}

static int Decode( decoder_t *p_dec, block_t *p_block )
{
    if ( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;
    subpicture_t *p_spu = NULL;

    if (p_block->i_flags & (BLOCK_FLAG_CORRUPTED))
        goto exit;

    scte18_cea_t *p_cea = scte18_cea_Decode( p_dec->p_sys->p_handle, p_block );
    if( p_cea )
    {
        p_spu = decoder_NewSubpictureText( p_dec );
        if( p_spu )
        {
            subpicture_updater_sys_t *p_spu_sys = p_spu->updater.p_sys;

            p_spu->i_start = p_block->i_pts;
            if( p_cea->alert_message_time_remaining )
                p_spu->i_stop = p_spu->i_start + CLOCK_FREQ * p_cea->alert_message_time_remaining;
            else
                p_spu->i_stop = VLC_TS_INVALID;

            p_spu->b_ephemer  = true;
            p_spu->b_absolute = false;

            p_spu_sys->region.inner_align = SUBPICTURE_ALIGN_TOP;
            p_spu_sys->p_default_style->i_style_flags = STYLE_BOLD | STYLE_BACKGROUND;
            p_spu_sys->p_default_style->i_features |= STYLE_HAS_FLAGS;
            p_spu_sys->p_default_style->i_background_color = 0x000000;
            p_spu_sys->p_default_style->i_background_alpha = STYLE_ALPHA_OPAQUE;
            p_spu_sys->p_default_style->i_features |= STYLE_HAS_BACKGROUND_COLOR | STYLE_HAS_BACKGROUND_ALPHA;
            p_spu_sys->p_default_style->i_font_color = 0xFF0000;
            p_spu_sys->p_default_style->i_features |= STYLE_HAS_FONT_COLOR;

            p_spu_sys->region.p_segments = text_segment_New( p_cea->psz_alert_text );
            decoder_QueueSub( p_dec, p_spu );
        }
        msg_Info( p_dec, "Received %s", p_cea->psz_alert_text );
        scte18_cea_Free( p_cea );
    }

exit:
    block_Release( p_block );
    return VLCDEC_SUCCESS;
}

static int Open( vlc_object_t *object )
{
    decoder_t *dec = (decoder_t *)object;

    if ( dec->fmt_in.i_codec != VLC_CODEC_SCTE_18 )
        return VLC_EGENERIC;

    decoder_sys_t *p_sys = malloc( sizeof(decoder_sys_t) );
    if( unlikely(!p_sys) )
        return VLC_ENOMEM;

    p_sys->p_handle = atsc_a65_handle_New( NULL );
    if( !p_sys->p_handle )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    dec->p_sys = p_sys;
    dec->pf_decode = Decode;
    dec->fmt_out.i_codec = 0;

    return VLC_SUCCESS;
}

static void Close( vlc_object_t *p_object )
{
    decoder_t *p_dec = (decoder_t *)p_object;
    decoder_sys_t *p_sys = (decoder_sys_t *) p_dec->p_sys;
    atsc_a65_handle_Release( p_sys->p_handle );
    free( p_sys );
}

