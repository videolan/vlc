/*****************************************************************************
 * aribsub.c : ARIB subtitles decoder
 *****************************************************************************
 * Copyright (C) 2012 Naohiro KORIYAMA
 *
 * Authors:  Naohiro KORIYAMA <nkoriyama@gmail.com>
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
#include <vlc_plugin.h>
#include <vlc_codec.h>

#ifdef HAVE_ARIBB24
 #include "substext.h"
 #include <aribb24/parser.h>
 #include <aribb24/decoder.h>
#endif

//#define DEBUG_ARIBSUB 1

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );
static int Decode( decoder_t *, block_t * );

#define IGNORE_RUBY_TEXT N_("Ignore ruby (furigana)")
#define IGNORE_RUBY_LONGTEXT N_("Ignore ruby (furigana) in the subtitle.")
#define USE_CORETEXT_TEXT N_("Use Core Text renderer")
#define USE_CORETEXT_LONGTEXT N_("Use Core Text renderer in the subtitle.")

vlc_module_begin ()
#   define ARIBSUB_CFG_PREFIX "aribsub-"
    set_description( N_("ARIB subtitles decoder") )
    set_shortname( N_("ARIB subtitles") )
    set_capability( "spu decoder", 50 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )
    set_callbacks( Open, Close )

    add_bool( ARIBSUB_CFG_PREFIX "ignore-ruby", false, IGNORE_RUBY_TEXT, IGNORE_RUBY_LONGTEXT, true )
    add_bool( ARIBSUB_CFG_PREFIX "use-coretext", false, USE_CORETEXT_TEXT, USE_CORETEXT_LONGTEXT, true )
vlc_module_end ()


/****************************************************************************
 * Local structures
 ****************************************************************************/

typedef struct
{
    bool              b_a_profile;
    bool              b_ignore_ruby;
    bool              b_use_coretext;
    bool              b_ignore_position_adjustment;

    arib_instance_t  *p_arib_instance;
    char             *psz_arib_base_dir;
} decoder_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static subpicture_t *render( decoder_t *, arib_parser_t *,
                             arib_decoder_t *, block_t * );

static char* get_arib_base_dir( void );
static void messages_callback_handler( void *, const char *psz_message );

/*****************************************************************************
 * Open: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to choose.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t *) p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_ARIB_A &&
        p_dec->fmt_in.i_codec != VLC_CODEC_ARIB_C )
    {
        return VLC_EGENERIC;
    }

    p_sys = (decoder_sys_t*) calloc( 1, sizeof(decoder_sys_t) );
    if( p_sys == NULL )
    {
        return VLC_ENOMEM;
    }

    p_sys->p_arib_instance = arib_instance_new( (void *) p_this );
    if ( !p_sys->p_arib_instance )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_dec->p_sys = p_sys;
    p_dec->pf_decode = Decode;
    p_dec->fmt_out.i_codec = 0;

    p_sys->b_a_profile = ( p_dec->fmt_in.i_codec == VLC_CODEC_ARIB_A );

    p_sys->b_ignore_ruby =
        var_InheritBool( p_this, ARIBSUB_CFG_PREFIX "ignore-ruby" );
    p_sys->b_use_coretext =
        var_InheritBool( p_this, ARIBSUB_CFG_PREFIX "use-coretext" );
    p_sys->b_ignore_position_adjustment = p_sys->b_use_coretext;
    p_sys->p_arib_instance->b_use_private_conv = p_sys->b_use_coretext;
    p_sys->p_arib_instance->b_replace_ellipsis = p_sys->b_use_coretext;

    char *psz_basedir = get_arib_base_dir();
    arib_set_base_path( p_sys->p_arib_instance, psz_basedir );
    free( psz_basedir );

    arib_register_messages_callback( p_sys->p_arib_instance,
                                     messages_callback_handler );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*) p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    arib_instance_destroy( p_sys->p_arib_instance );
    free( p_sys->psz_arib_base_dir );

    var_Destroy( p_this, ARIBSUB_CFG_PREFIX "ignore-ruby" );
    var_Destroy( p_this, ARIBSUB_CFG_PREFIX "use-coretext" );

    free( p_sys );
}

/*****************************************************************************
 * Decode:
 *****************************************************************************/
static int Decode( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

    if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
    {
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    arib_parser_t *p_parser = arib_get_parser( p_sys->p_arib_instance );
    arib_decoder_t *p_decoder = arib_get_decoder( p_sys->p_arib_instance );
    if ( p_parser && p_decoder )
    {
        arib_parse_pes( p_parser, p_block->p_buffer, p_block->i_buffer );
        subpicture_t *p_spu = render( p_dec, p_parser, p_decoder, p_block );
        if( p_spu != NULL )
            decoder_QueueSub( p_dec, p_spu );
    }

    block_Release( p_block );
    return VLCDEC_SUCCESS;
}

/* following functions are local */

static void messages_callback_handler( void *p_opaque, const char *psz_message )
{
    decoder_t *p_dec = ( decoder_t * ) p_opaque;
    msg_Dbg( p_dec, "%s", psz_message );
}

static char* get_arib_base_dir()
{
    char *psz_data_dir = config_GetUserDir( VLC_USERDATA_DIR );
    if( psz_data_dir == NULL )
    {
        return NULL;
    }

    char *psz_arib_base_dir;
    if( asprintf( &psz_arib_base_dir, "%s"DIR_SEP"arib", psz_data_dir ) < 0 )
    {
        psz_arib_base_dir = NULL;
    }
    free( psz_data_dir );

    return psz_arib_base_dir;
}

static subpicture_t *render( decoder_t *p_dec, arib_parser_t *p_parser,
                             arib_decoder_t *p_arib_decoder, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    subpicture_t *p_spu = NULL;
    char *psz_subtitle = NULL;

    size_t i_data_size;
    const unsigned char *psz_data = arib_parser_get_data( p_parser, &i_data_size );
    if( !psz_data || !i_data_size )
        return NULL;

    size_t i_subtitle_size = i_data_size * 4;
    psz_subtitle = (char*) calloc( i_subtitle_size + 1, sizeof(*psz_subtitle) );
    if( psz_subtitle == NULL )
    {
        return NULL;
    }
    if( p_sys->b_a_profile )
        arib_initialize_decoder_a_profile( p_arib_decoder );
    else
        arib_initialize_decoder_c_profile( p_arib_decoder );

    i_subtitle_size = arib_decode_buffer( p_arib_decoder,
                                          psz_data,
                                          i_data_size,
                                          psz_subtitle,
                                          i_subtitle_size );
#ifdef DEBUG_ARIBSUB
    msg_Dbg( p_dec, "psz_subtitle [%s]", psz_subtitle );
    unsigned const char* start = psz_data;
    unsigned const char* end = psz_data + i_data_size;
    char* psz_subtitle_data_hex = (char*) calloc(
            i_data_size * 3 + 1, sizeof(char) );
    char* psz_subtitle_data_hex_idx = psz_subtitle_data_hex;
    while( start < end )
    {
        sprintf(psz_subtitle_data_hex_idx, "%02x ", *start++);
        psz_subtitle_data_hex_idx += 3;
    }
    msg_Dbg( p_dec, "psz_subtitle_data [%s]", psz_subtitle_data_hex);
    free( psz_subtitle_data_hex );
#endif

    p_spu = decoder_NewSubpictureText( p_dec );
    if( p_spu == NULL )
    {
        goto decoder_NewSubpictureText_failed;
    }

    p_spu->i_start = p_block->i_pts;
    p_spu->i_stop = p_block->i_pts + VLC_TICK_FROM_US(arib_decoder_get_time( p_arib_decoder ));
    p_spu->b_ephemer  = (p_spu->i_start == p_spu->i_stop);
    p_spu->b_absolute = true;

    arib_spu_updater_sys_t *p_spu_sys = p_spu->updater.p_sys;

    arib_text_region_t *p_region = p_spu_sys->p_region =
        (arib_text_region_t*) calloc( 1, sizeof(arib_text_region_t) );
    if( p_region == NULL )
    {
        goto malloc_failed;
    }
    for( const arib_buf_region_t *p_buf_region = arib_decoder_get_regions( p_arib_decoder );
         p_buf_region; p_buf_region = p_buf_region->p_next )
    {
        if( p_sys->b_ignore_ruby && p_buf_region->i_fontheight == 18 )
        {
            continue;
        }

        int i_size = p_buf_region->p_end - p_buf_region->p_start;
        char *psz_text = (char*) calloc( i_size + 1, sizeof(char) );
        if( psz_text == NULL )
        {
            goto malloc_failed;
        }
        strncpy(psz_text, p_buf_region->p_start, i_size);
        psz_text[i_size] = '\0';
#ifdef DEBUG_ARIBSUB
        msg_Dbg( p_dec, "psz_text [%s]", psz_text );
#endif

        p_region->psz_text = strdup( psz_text );
        free( psz_text );
        p_region->psz_fontname = NULL;
        p_region->i_font_color = p_buf_region->i_foreground_color;
        p_region->i_planewidth = p_buf_region->i_planewidth;
        p_region->i_planeheight = p_buf_region->i_planeheight;
        p_region->i_fontwidth = p_buf_region->i_fontwidth;
        p_region->i_fontheight = p_buf_region->i_fontheight;
        p_region->i_verint = p_buf_region->i_verint;
        p_region->i_horint = p_buf_region->i_horint;
        p_region->i_charleft = p_buf_region->i_charleft;
        p_region->i_charbottom = p_buf_region->i_charbottom;
        p_region->i_charleft_adj = 0;
        p_region->i_charbottom_adj = 0;
        if( !p_sys->b_ignore_position_adjustment )
        {
            p_region->i_charleft_adj = p_buf_region->i_horadj;
            p_region->i_charbottom_adj = p_buf_region->i_veradj;
        }
        p_region->p_next = NULL;
        if( p_buf_region->p_next != NULL )
        {
            p_region = p_region->p_next =
                (arib_text_region_t*) calloc( 1, sizeof(arib_text_region_t) );
            if( p_region == NULL )
            {
                goto malloc_failed;
            }
        }
    }

decoder_NewSubpictureText_failed:
malloc_failed:
    arib_finalize_decoder( p_arib_decoder );
    free( psz_subtitle );

    return p_spu;
}
