/*****************************************************************************
 * kate.c : a decoder for the kate bitstream format
 *****************************************************************************
 * Copyright (C) 2000-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Penquerc'h <ogg.k.ogg.k@googlemail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc_input.h>
#include <vlc_codec.h>

#include <kate/kate.h>

#include "vlc_osd.h"

/* #define ENABLE_PACKETIZER */
/* #define ENABLE_FORMATTING */
/* #define ENABLE_BITMAPS */

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
#ifdef ENABLE_PACKETIZER
    /* Module mode */
    bool b_packetizer;
#endif

    /*
     * Input properties
     */
    int i_num_headers;
    int i_headers;

    /*
     * Kate properties
     */
    bool           b_ready;
    kate_info      ki;
    kate_comment   kc;
    kate_state     k;

    /*
     * Common properties
     */
    mtime_t i_pts;

    /*
     * Options
     */
#ifdef ENABLE_FORMATTING
    bool   b_formatted;
#endif
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );
#ifdef ENABLE_PACKETIZER
static int OpenPacketizer( vlc_object_t *p_this );
#endif

static subpicture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block );
static int ProcessHeaders( decoder_t *p_dec );
static subpicture_t *ProcessPacket( decoder_t *p_dec, kate_packet *p_kp,
                            block_t **pp_block );
static subpicture_t *DecodePacket( decoder_t *p_dec, kate_packet *p_kp,
                            block_t *p_block );
static void ParseKateComments( decoder_t * );

#define DEFAULT_NAME "Default"
#define MAX_LINE 8192

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/

#ifdef ENABLE_FORMATTING
#define FORMAT_TEXT N_("Formatted Subtitles")
#define FORMAT_LONGTEXT N_("Kate streams allow for text formatting. " \
 "VLC partly implements this, but you can choose to disable all formatting.")
#endif


vlc_module_begin();
    set_shortname( _("Kate"));
    set_description( _("Kate text subtitles decoder") );
    set_capability( "decoder", 50 );
    set_callbacks( OpenDecoder, CloseDecoder );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_SCODEC );
    add_shortcut( "kate" );

#ifdef ENABLE_PACKETIZER
    add_submodule();
    set_description( _("Kate text subtitles packetizer") );
    set_capability( "packetizer", 100 );
    set_callbacks( OpenPacketizer, CloseDecoder );
    add_shortcut( "kate" );
#endif

#ifdef ENABLE_FORMATTING
    add_bool( "kate-formatted", true, NULL, FORMAT_TEXT, FORMAT_LONGTEXT,
                 false );
#endif
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to chose.
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    msg_Dbg( p_dec, "kate: OpenDecoder");

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('k','a','t','e') )
    {
        return VLC_EGENERIC;
    }

    /* Set callbacks */
    p_dec->pf_decode_sub = (subpicture_t *(*)(decoder_t *, block_t **))
        DecodeBlock;
    p_dec->pf_packetize    = (block_t *(*)(decoder_t *, block_t **))
        DecodeBlock;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return VLC_ENOMEM;
    }

    /* init of p_sys */
#ifdef ENABLE_PACKETIZER
    p_sys->b_packetizer = false;
#endif
    p_sys->b_ready = false;
    p_sys->i_pts = 0;

    kate_comment_init( &p_sys->kc );
    kate_info_init( &p_sys->ki );

    p_sys->i_num_headers = 0;
    p_sys->i_headers = 0;

    /* retrieve options */
#ifdef ENABLE_FORMATTING
    p_sys->b_formatted = var_CreateGetBool( p_dec, "kate-formatted" );
#endif

    return VLC_SUCCESS;
}

#ifdef ENABLE_PACKETIZER
static int OpenPacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    int i_ret = OpenDecoder( p_this );

    if( i_ret == VLC_SUCCESS )
    {
        p_dec->p_sys->b_packetizer = true;
        p_dec->fmt_out.i_codec = VLC_FOURCC( 'k', 'a', 't', 'e' );
    }

    return i_ret;
}
#endif

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with kate packets.
 ****************************************************************************/
static subpicture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;
    kate_packet kp;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;

    /* Block to Kate packet */
    kate_packet_wrap(&kp, p_block->i_buffer, p_block->p_buffer);

    if( p_sys->i_headers == 0 && p_dec->fmt_in.i_extra )
    {
        /* Headers already available as extra data */
        p_sys->i_num_headers = ((unsigned char*)p_dec->fmt_in.p_extra)[0];
        p_sys->i_headers = p_sys->i_num_headers;
    }
    else if( kp.nbytes && (p_sys->i_headers==0 || p_sys->i_headers < p_sys->ki.num_headers ))
    {
        /* Backup headers as extra data */
        uint8_t *p_extra;

        p_dec->fmt_in.p_extra =
            realloc( p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra + kp.nbytes + 2 );
        p_extra = (void*)(((unsigned char*)p_dec->fmt_in.p_extra) + p_dec->fmt_in.i_extra);
        *(p_extra++) = kp.nbytes >> 8;
        *(p_extra++) = kp.nbytes & 0xFF;

        memcpy( p_extra, kp.data, kp.nbytes );
        p_dec->fmt_in.i_extra += kp.nbytes + 2;

        block_Release( *pp_block );
        p_sys->i_num_headers = ((unsigned char*)p_dec->fmt_in.p_extra)[0];
        p_sys->i_headers++;
        return NULL;
    }

    if( p_sys->i_headers == p_sys->i_num_headers && p_sys->i_num_headers>0 )
    {
        if( ProcessHeaders( p_dec ) != VLC_SUCCESS )
        {
            p_sys->i_headers = 0;
            p_dec->fmt_in.i_extra = 0;
            block_Release( *pp_block );
            return NULL;
        }
        else p_sys->i_headers++;
    }

    return ProcessPacket( p_dec, &kp, pp_block );
}

/*****************************************************************************
 * ProcessHeaders: process Kate headers.
 *****************************************************************************/
static int ProcessHeaders( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    kate_packet kp;
    uint8_t *p_extra;
    int i_extra;
    int headeridx;
    int ret;

    if( !p_dec->fmt_in.i_extra ) return VLC_EGENERIC;

    p_extra = p_dec->fmt_in.p_extra;
    i_extra = p_dec->fmt_in.i_extra;

    /* skip number of headers */
    ++p_extra;
    --i_extra;

    /* Take care of the initial Kate header */
    kp.nbytes = *(p_extra++) << 8;
    kp.nbytes |= (*(p_extra++) & 0xFF);
    kp.data = p_extra;
    p_extra += kp.nbytes;
    i_extra -= (kp.nbytes + 2);
    if( i_extra < 0 )
    {
        msg_Err( p_dec, "header data corrupted");
        return VLC_EGENERIC;
    }

    ret = kate_decode_headerin( &p_sys->ki, &p_sys->kc, &kp );
    if( ret < 0 )
    {
        msg_Err( p_dec, "this bitstream does not contain Kate data (%d)", ret );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_dec, "%s %s text, granule rate %f, granule shift %d",
             p_sys->ki.language, p_sys->ki.category,
             (double)p_sys->ki.gps_numerator/p_sys->ki.gps_denominator,
             p_sys->ki.granule_shift);

    /* we want markup to be removed for now */
    kate_info_remove_markup( &p_sys->ki, 1 );

    /* parse all remaining header packets */
    for (headeridx=1; headeridx<p_sys->ki.num_headers; ++headeridx)
    {
        kp.nbytes = *(p_extra++) << 8;
        kp.nbytes |= (*(p_extra++) & 0xFF);
        kp.data = p_extra;
        p_extra += kp.nbytes;
        i_extra -= (kp.nbytes + 2);
        if( i_extra < 0 )
        {
            msg_Err( p_dec, "header %d data corrupted", headeridx);
            return VLC_EGENERIC;
        }

        ret = kate_decode_headerin( &p_sys->ki, &p_sys->kc, &kp );
        if( ret < 0 )
        {
            msg_Err( p_dec, "Kate header %d is corrupted: %d", headeridx, ret);
            return VLC_EGENERIC;
        }

        /* header 1 is comments */
        if( headeridx == 1 )
        {
            ParseKateComments( p_dec );
        }
    }

#ifdef ENABLE_PACKETIZER
    if( !p_sys->b_packetizer )
#endif
    {
        /* We have all the headers, initialize decoder */
        msg_Dbg( p_dec, "we have all headers, initialize libkate for decoding" );
        ret = kate_decode_init( &p_sys->k, &p_sys->ki );
        if (ret < 0)
        {
            msg_Err( p_dec, "Kate failed to initialize for decoding: %d", ret );
            return VLC_EGENERIC;
        }
        p_sys->b_ready = true;
    }
#ifdef ENABLE_PACKETIZER
    else
    {
        p_dec->fmt_out.i_extra = p_dec->fmt_in.i_extra;
        p_dec->fmt_out.p_extra =
            realloc( p_dec->fmt_out.p_extra, p_dec->fmt_out.i_extra );
        memcpy( p_dec->fmt_out.p_extra,
                p_dec->fmt_in.p_extra, p_dec->fmt_out.i_extra );
    }
#endif

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ProcessPacket: processes a kate packet.
 *****************************************************************************/
static subpicture_t *ProcessPacket( decoder_t *p_dec, kate_packet *p_kp,
                            block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block = *pp_block;
    subpicture_t *p_buf = NULL;

    /* Date management */
    if( p_block->i_pts > 0 && p_block->i_pts != p_sys->i_pts )
    {
        p_sys->i_pts = p_block->i_pts;
    }

    *pp_block = NULL; /* To avoid being fed the same packet again */

#ifdef ENABLE_PACKETIZER
    if( p_sys->b_packetizer )
    {
        /* Date management */
        p_block->i_dts = p_block->i_pts = p_sys->i_pts;

        if( p_sys->i_headers >= p_sys->i_num_headers )
            p_block->i_length = p_sys->i_pts - p_block->i_pts;
        else
            p_block->i_length = 0;

        p_buf = p_block;
    }
    else
#endif
    {
        if( p_sys->i_headers >= p_sys->i_num_headers )
            p_buf = DecodePacket( p_dec, p_kp, p_block );
        else
            p_buf = NULL;

        if( p_block ) block_Release( p_block );
    }

    return p_buf;
}

#ifdef ENABLE_BITMAPS
/* nicked off blend.c */
static inline void rgb_to_yuv( uint8_t *y, uint8_t *u, uint8_t *v,
                               int r, int g, int b )
{
    *y = ( ( (  66 * r + 129 * g +  25 * b + 128 ) >> 8 ) + 16 );
    *u =   ( ( -38 * r -  74 * g + 112 * b + 128 ) >> 8 ) + 128 ;
    *v =   ( ( 112 * r -  94 * g -  18 * b + 128 ) >> 8 ) + 128 ;
}
#endif

/*****************************************************************************
 * DecodePacket: decodes a Kate packet.
 *****************************************************************************/
static subpicture_t *DecodePacket( decoder_t *p_dec, kate_packet *p_kp, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    const kate_event *ev=NULL;
    subpicture_t *p_spu = NULL;
    subpicture_region_t *p_bitmap_region = NULL;
    int ret;
#ifdef ENABLE_BITMAPS
    size_t n, y;
    picture_t *pic = NULL;
#endif
    video_format_t fmt;
    kate_tracker kin;
    bool tracker_valid = false;

    ret = kate_decode_packetin( &p_sys->k, p_kp );
    if( ret < 0 )
    {
        msg_Err( p_dec, "Kate failed to decode packet: %d", ret );
        return NULL;
    }

    ret = kate_decode_eventout( &p_sys->k, &ev );
    if( ret < 0 )
    {
        msg_Err( p_dec, "Kate failed to retrieve event: %d", ret );
        return NULL;
    }
    if( ret > 0 )
    {
        /* no event to go with this packet, this is normal */
        return NULL;
    }

    /* we have an event */

    /* Get a new spu */
    p_spu = p_dec->pf_spu_buffer_new( p_dec );
    if( !p_spu )
    {
        msg_Err( p_dec, "Failed to allocate spu buffer" );
        return NULL;
    }

    p_spu->b_pausable = true;

#ifdef ENABLE_BITMAPS
    if (ev->bitmap && ev->palette) {
        /* create a separate region for the bitmap */
        memset( &fmt, 0, sizeof(video_format_t) );
        fmt.i_chroma = VLC_FOURCC('Y','U','V','P');
        fmt.i_aspect = 0;
        fmt.i_width = fmt.i_visible_width = ev->bitmap->width;
        fmt.i_height = fmt.i_visible_height = ev->bitmap->height;
        fmt.i_x_offset = fmt.i_y_offset = 0;

        p_bitmap_region = p_spu->pf_create_region( VLC_OBJECT(p_dec), &fmt );
        if( !p_bitmap_region )
        {
            msg_Err( p_dec, "cannot allocate SPU region" );
            p_dec->pf_spu_buffer_del( p_dec, p_spu );
            return NULL;
        }

        /* create the palette */
        fmt.p_palette->i_entries = ev->palette->ncolors;
        for (n=0; n<ev->palette->ncolors; ++n)
        {
            rgb_to_yuv(
                &fmt.p_palette->palette[n][0], &fmt.p_palette->palette[n][1], &fmt.p_palette->palette[n][2],
                ev->palette->colors[n].r, ev->palette->colors[n].g, ev->palette->colors[n].b
            );
            fmt.p_palette->palette[n][3] = ev->palette->colors[n].a;
        }

        /* create the bitmap */
        pic = &p_bitmap_region->picture;
        for (y=0; y<ev->bitmap->height; ++y) {
          uint8_t *dest=pic->Y_PIXELS+pic->Y_PITCH*y;
          const uint8_t *src=ev->bitmap->pixels+y*ev->bitmap->width;
          memcpy(dest, src, ev->bitmap->width);
        }

        msg_Dbg(p_dec, "Created bitmap, %dx%d, %d colors\n", ev->bitmap->width, ev->bitmap->height, ev->palette->ncolors);
    }
#endif

    /* Create a new subpicture region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC('T','E','X','T');
    fmt.i_aspect = 0;
    fmt.i_width = fmt.i_height = 0;
    fmt.i_x_offset = fmt.i_y_offset = 0;

#ifdef ENABLE_FORMATTING
    if (p_sys->b_formatted)
    {
        ret = kate_tracker_init( &kin, &p_sys->ki, ev);
        if( ret < 0)
        {
            msg_Err( p_dec, "failed to initialize kate tracker, event will be unformatted: %d", ret );
        }
        else
        {
            // TODO: get window/video sizes/pos - can't find where to get those !
            int w = 640;
            int h = 480;
            ret = kate_tracker_update(&kin, 0, w, h, 0, 0, w, h);
            if( ret < 0)
            {
                kate_tracker_clear(&kin);
                msg_Err( p_dec, "failed to update kate tracker, event will be unformatted: %d", ret );
            }
            else
            {
                if (kin.has.region)
                {
                    fmt.i_width = kin.region_w;
                    fmt.i_height = kin.region_h;
                }

                // TODO: parse tracker and set style, init fmt
                tracker_valid = true;
            }
        }
    }
#endif



    p_spu->p_region = p_spu->pf_create_region( VLC_OBJECT(p_dec), &fmt );
    if( !p_spu->p_region )
    {
        msg_Err( p_dec, "cannot allocate SPU region" );
        p_dec->pf_spu_buffer_del( p_dec, p_spu );
        return NULL;
    }

    p_spu->p_region->psz_text = strdup(ev->text); /* no leak, this actually gets killed by the core */
    p_spu->i_start = p_block->i_pts;
    p_spu->i_stop = p_block->i_pts + I64C(1000000)*ev->duration*p_sys->ki.gps_denominator/p_sys->ki.gps_numerator;
    p_spu->b_ephemer = (p_block->i_length == 0);
    p_spu->b_absolute = false;

    if (tracker_valid)
    {
        p_spu->i_flags = 0;
        if (kin.has.region)
        {
            p_spu->p_region->i_x = kin.region_x;
            p_spu->p_region->i_y = kin.region_y;
            if (p_bitmap_region) {
                p_bitmap_region->i_x = kin.region_x;
                p_bitmap_region->i_y = kin.region_y;
            }
        }

        kate_tracker_clear(&kin);
    }
    else
    {
        /* Normal text subs, easy markup */
        p_spu->p_region->i_align = SUBPICTURE_ALIGN_BOTTOM;
        if (p_bitmap_region) {
            p_bitmap_region->i_align = SUBPICTURE_ALIGN_BOTTOM;
        }
        p_spu->i_x = 0;
        p_spu->i_y = 10;
    }

#ifdef ENABLE_BITMAPS
    /* if we have a bitmap, chain it before the text */
    if (p_bitmap_region)
    {
        p_bitmap_region->p_next = p_spu->p_region;
        p_spu->p_region = p_bitmap_region;
    }
#endif

    return p_spu;
}

/*****************************************************************************
 * ParseKateComments: FIXME should be done in demuxer
 *****************************************************************************/
static void ParseKateComments( decoder_t *p_dec )
{
    input_thread_t *p_input = (input_thread_t *)p_dec->p_parent;
    char *psz_name, *psz_value, *psz_comment;
    int i = 0;

    if( p_input->i_object_type != VLC_OBJECT_INPUT ) return;

    while ( i < p_dec->p_sys->kc.comments )
    {
        psz_comment = strdup( p_dec->p_sys->kc.user_comments[i] );
        if( !psz_comment )
        {
            msg_Warn( p_dec, "out of memory" );
            break;
        }
        psz_name = psz_comment;
        psz_value = strchr( psz_comment, '=' );
        if( psz_value )
        {
            *psz_value = '\0';
            psz_value++;
            input_Control( p_input, INPUT_ADD_INFO, _("Kate comment"),
                           psz_name, "%s", psz_value );
        }
        free( psz_comment );
        i++;
    }
}

/*****************************************************************************
 * CloseDecoder: clean up the decoder
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (p_sys->b_ready)
        kate_clear( &p_sys->k );
    kate_info_clear( &p_sys->ki );
    kate_comment_clear( &p_sys->kc );

    free( p_sys );
}

