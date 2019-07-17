/*****************************************************************************
 * substx3gsub.c : MP4 tx3g subtitles decoder
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_sout.h>
#include <vlc_charset.h>

#include "substext.h"
#include "../demux/mp4/minibox.h"

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int OpenDecoder ( vlc_object_t * );
static void CloseDecoder ( vlc_object_t * );
static int Decode( decoder_t *, block_t * );
#ifdef ENABLE_SOUT
static int OpenEncoder ( vlc_object_t * );
static block_t * Encode( encoder_t *, subpicture_t * );
#endif

vlc_module_begin ()
    set_description( N_("tx3g subtitles decoder") )
    set_shortname( N_("tx3g subtitles") )
    set_capability( "spu decoder", 100 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )
    set_callbacks( OpenDecoder, CloseDecoder )
#ifdef ENABLE_SOUT
    add_submodule ()
        set_description( N_("tx3g subtitles encoder") )
        set_shortname( N_("tx3g subtitles encoder") )
        set_capability( "encoder", 101 )
        set_callback( OpenEncoder )
#endif
vlc_module_end ()

/****************************************************************************
 * Local structs
 ****************************************************************************/

/*****************************************************************************
 * Local:
 *****************************************************************************/

#define FONT_FACE_BOLD      0x1
#define FONT_FACE_ITALIC    0x2
#define FONT_FACE_UNDERLINE 0x4

static int ConvertToVLCFlags( int i_atomflags )
{
    int i_vlcstyles_flags = 0;
    if ( i_atomflags & FONT_FACE_BOLD )
        i_vlcstyles_flags |= STYLE_BOLD;
    if ( i_atomflags & FONT_FACE_ITALIC )
        i_vlcstyles_flags |= STYLE_ITALIC;
    if ( i_atomflags & FONT_FACE_UNDERLINE )
        i_vlcstyles_flags |= STYLE_UNDERLINE;
    return i_vlcstyles_flags;
}

static size_t str8len( const char *psz_string )
{
    const char *psz_tmp = psz_string;
    size_t i=0;
    while ( *psz_tmp )
    {
        if ( (*psz_tmp & 0xC0) != 0x80 ) i++;
        psz_tmp++;
    }
    return i;
}

static char * str8indup( const char *psz_string, size_t i_skip, size_t n )
{
    while( i_skip && *psz_string )
    {
        if ( (*psz_string & 0xC0) != 0x80 ) i_skip--;
        psz_string++;
    }
    if ( ! *psz_string || i_skip ) return NULL;

    const char *psz_tmp = psz_string;
    while( n && *psz_tmp )
    {
        if ( (*psz_tmp & 0xC0) != 0x80 ) n--;
        psz_tmp++;
    }
    return strndup( psz_string, psz_tmp - psz_string );
}

typedef struct tx3g_segment_t tx3g_segment_t;

struct tx3g_segment_t
{
    text_segment_t *s;
    size_t i_size;
    tx3g_segment_t *p_next3g;
};

static tx3g_segment_t * tx3g_segment_New( const char *psz_string )
{
    tx3g_segment_t *p_seg = malloc( sizeof(tx3g_segment_t) );
    if( p_seg )
    {
        p_seg->i_size = 0;
        p_seg->p_next3g = NULL;
        p_seg->s = text_segment_New( psz_string );
        if( !p_seg->s )
        {
            free( p_seg );
            p_seg = NULL;
        }
    }
    return p_seg;
}

static void SegmentDoSplit( tx3g_segment_t *p_segment, uint16_t i_start, uint16_t i_end,
                            tx3g_segment_t **pp_segment_left,
                            tx3g_segment_t **pp_segment_middle,
                            tx3g_segment_t **pp_segment_right )
{
    tx3g_segment_t *p_segment_left = NULL, *p_segment_right = NULL, *p_segment_middle = NULL;

    if ( (p_segment->i_size - i_start < 1) || (p_segment->i_size - i_end < 1) )
        goto error;

    if ( i_start > 0 )
    {
        char* psz_text = str8indup( p_segment->s->psz_text, 0, i_start );
        p_segment_left = tx3g_segment_New( psz_text );
        free( psz_text );
        if ( !p_segment_left ) goto error;
        p_segment_left->s->style = text_style_Duplicate( p_segment->s->style );
        p_segment_left->i_size = str8len( p_segment_left->s->psz_text );
    }

    char* psz_midtext = str8indup( p_segment->s->psz_text, i_start, i_end - i_start + 1 );
    p_segment_middle = tx3g_segment_New( psz_midtext );
    free( psz_midtext );
    if ( !p_segment_middle ) goto error;
    p_segment_middle->s->style = text_style_Duplicate( p_segment->s->style );
    p_segment_middle->i_size = str8len( p_segment_middle->s->psz_text );

    if ( i_end < (p_segment->i_size - 1) )
    {
        char* psz_text = str8indup( p_segment->s->psz_text, i_end + 1, p_segment->i_size - i_end - 1 );
        p_segment_right = tx3g_segment_New( psz_text );
        free( psz_text );
        if ( !p_segment_right ) goto error;
        p_segment_right->s->style = text_style_Duplicate( p_segment->s->style );
        p_segment_right->i_size = str8len( p_segment_right->s->psz_text );
    }

    if ( p_segment_left ) p_segment_left->p_next3g = p_segment_middle;
    if ( p_segment_right ) p_segment_middle->p_next3g = p_segment_right;

    *pp_segment_left = p_segment_left;
    *pp_segment_middle = p_segment_middle;
    *pp_segment_right = p_segment_right;

    return;

error:
    if( p_segment_middle )
    {
        text_segment_Delete( p_segment_middle->s );
        free( p_segment_middle );
    }
    if( p_segment_left )
    {
        text_segment_Delete( p_segment_left->s );
        free( p_segment_left );
    }
    *pp_segment_left = *pp_segment_middle = *pp_segment_right = NULL;
}

static bool SegmentSplit( tx3g_segment_t *p_prev, tx3g_segment_t **pp_segment,
                          const uint16_t i_start, const uint16_t i_end,
                          const text_style_t *p_styles )
{
    tx3g_segment_t *p_segment_left = NULL, *p_segment_middle = NULL, *p_segment_right = NULL;

    if ( (*pp_segment)->i_size == 0 ) return false;
    if ( i_start > i_end ) return false;
    if ( (size_t)(i_end - i_start) > (*pp_segment)->i_size - 1 ) return false;
    if ( i_end > (*pp_segment)->i_size - 1 ) return false;

    SegmentDoSplit( *pp_segment, i_start, i_end, &p_segment_left, &p_segment_middle, &p_segment_right );
    if ( !p_segment_middle )
    {
        /* Failed */
        text_segment_Delete( p_segment_left->s );
        free( p_segment_left );
        text_segment_Delete( p_segment_right->s );
        free( p_segment_right );
        return false;
    }

    tx3g_segment_t *p_next3g = (*pp_segment)->p_next3g;
    text_segment_Delete( (*pp_segment)->s );
    free( *pp_segment );
    *pp_segment = ( p_segment_left ) ? p_segment_left : p_segment_middle ;
    if ( p_prev ) p_prev->p_next3g = *pp_segment;

    if ( p_segment_right )
        p_segment_right->p_next3g = p_next3g;
    else
        p_segment_middle->p_next3g = p_next3g;

    if( p_segment_middle->s->style )
        text_style_Merge( p_segment_middle->s->style, p_styles, true );
    else
        p_segment_middle->s->style = text_style_Duplicate( p_styles );

    return true;
}

/* Creates a new segment using the given style and split existing ones according
   to the start & end offsets */
static void ApplySegmentStyle( tx3g_segment_t **pp_segment, const uint16_t i_absstart,
                               const uint16_t i_absend, const text_style_t *p_styles )
{
    /* find the matching segment */
    uint16_t i_curstart = 0;
    tx3g_segment_t *p_prev = NULL;
    tx3g_segment_t *p_cur = *pp_segment;
    while ( p_cur )
    {
        uint16_t i_curend = i_curstart + p_cur->i_size - 1;
        if ( (i_absstart >= i_curstart) && (i_absend <= i_curend) )
        {
            /* segment found */
            if ( !SegmentSplit( p_prev, &p_cur, i_absstart - i_curstart,
                                i_absend - i_curstart, p_styles ) )
                return;
            if ( !p_prev ) *pp_segment = p_cur;
            break;
        }
        else
        {
            i_curstart += p_cur->i_size;
            p_prev = p_cur;
            p_cur = p_cur->p_next3g;
        }
    }
}

/* Do relative size conversion using default style size (from stsd),
   as the line should always be 5%. Apply to each segment specific text size */
static void FontSizeConvert( const text_style_t *p_reference, text_style_t *p_style )
{
    if( unlikely(!p_style) )
    {
        return;
    }
    else if( unlikely(!p_reference) || p_reference->i_font_size == 0 )
    {
        p_style->i_font_size = 0;
        p_style->f_font_relsize = 5.0;
    }
    else
    {
        p_style->f_font_relsize = 5.0 * (float) p_style->i_font_size / p_reference->i_font_size;
        p_style->i_font_size = 0;
    }
}

/*****************************************************************************
 * Decode:
 *****************************************************************************/
static int Decode( decoder_t *p_dec, block_t *p_block )
{
    subpicture_t  *p_spu = NULL;

    if( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

    if( ( p_block->i_flags & (BLOCK_FLAG_CORRUPTED) ) ||
          p_block->i_buffer < sizeof(uint16_t) )
    {
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    uint8_t *p_buf = p_block->p_buffer;

    /* Read our raw string and create the styled segment for HTML */
    uint16_t i_psz_bytelength = GetWBE( p_buf );
    if( p_block->i_buffer < i_psz_bytelength + 2U )
    {
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    const uint8_t *p_pszstart = p_block->p_buffer + sizeof(uint16_t);
    char *psz_subtitle;
    if ( i_psz_bytelength > 2 &&
         ( !memcmp( p_pszstart, "\xFE\xFF", 2 ) || !memcmp( p_pszstart, "\xFF\xFE", 2 ) )
       )
    {
        psz_subtitle = FromCharset( "UTF-16", p_pszstart, i_psz_bytelength );
        if ( !psz_subtitle )
            return VLCDEC_SUCCESS;
    }
    else
    {
        psz_subtitle = strndup( (const char*) p_pszstart, i_psz_bytelength );
        if ( !psz_subtitle )
            return VLCDEC_SUCCESS;
    }
    p_buf += i_psz_bytelength + sizeof(uint16_t);

    for( uint16_t i=0; i < i_psz_bytelength; i++ )
     if ( psz_subtitle[i] == '\r' ) psz_subtitle[i] = '\n';

    tx3g_segment_t *p_segment3g = tx3g_segment_New( psz_subtitle );
    p_segment3g->i_size = str8len( psz_subtitle );
    free( psz_subtitle );

    if ( !p_segment3g->s->psz_text )
    {
        text_segment_Delete( p_segment3g->s );
        free( p_segment3g );
        return VLCDEC_SUCCESS;
    }

    /* Create the subpicture unit */
    p_spu = decoder_NewSubpictureText( p_dec );
    if( !p_spu )
    {
        text_segment_Delete( p_segment3g->s );
        free( p_segment3g );
        return VLCDEC_SUCCESS;
    }

    subtext_updater_sys_t *p_spu_sys = p_spu->updater.p_sys;
    const text_style_t *p_root_style = (text_style_t *) p_dec->p_sys;

    mp4_box_iterator_t it;
    mp4_box_iterator_Init( &it, p_buf,
                           p_block->i_buffer - (p_buf - p_block->p_buffer) );
    /* Parse our styles */
    if( p_dec->fmt_in.i_codec != VLC_CODEC_QTXT )
    while( mp4_box_iterator_Next( &it ) )
    {
        switch( it.i_type )
        {

        case VLC_FOURCC('s','t','y','l'):
        {
            if( it.i_payload < 14 )
                break;

            uint16_t i_nbrecords = GetWBE(it.p_payload);
            uint16_t i_cur_record = 0;

            it.p_payload += 2; it.i_payload -= 2;
            while( i_cur_record++ < i_nbrecords && it.i_payload >= 12 )
            {
                uint16_t i_start = __MIN( GetWBE(it.p_payload), i_psz_bytelength - 1 );
                uint16_t i_end =  GetWBE(it.p_payload + 2); /* index is past last char */
                if( i_start < i_end )
                {
                    i_end = VLC_CLIP( i_end - 1, i_start, i_psz_bytelength - 1 );

                    text_style_t *p_style = text_style_Create( STYLE_NO_DEFAULTS );
                    if( p_style )
                    {
                        if( (p_style->i_style_flags = ConvertToVLCFlags( it.p_payload[6] )) )
                            p_style->i_features |= STYLE_HAS_FLAGS;
                        p_style->i_font_size = it.p_payload[7];
                        p_style->i_font_color = GetDWBE(&it.p_payload[8]) >> 8;// RGBA -> RGB
                        p_style->i_font_alpha = GetDWBE(&it.p_payload[8]) & 0xFF;
                        p_style->i_features |= STYLE_HAS_FONT_COLOR | STYLE_HAS_FONT_ALPHA;
                        ApplySegmentStyle( &p_segment3g, i_start, i_end, p_style );
                        text_style_Delete( p_style );
                    }
                }

                it.p_payload += 12; it.i_payload -= 12;
            }
        }   break;


        default:
            break;

        }
    }

    p_spu->i_start    = p_block->i_pts;
    p_spu->i_stop     = p_block->i_pts + p_block->i_length;
    p_spu->b_ephemer  = (p_block->i_length == VLC_TICK_INVALID);
    p_spu->b_absolute = false;

    p_spu_sys->region.align = SUBPICTURE_ALIGN_BOTTOM;

    text_style_Merge( p_spu_sys->p_default_style, p_root_style, true );
    FontSizeConvert( p_root_style, p_spu_sys->p_default_style );

    /* Unwrap */
    text_segment_t *p_text_segments = p_segment3g->s;
    text_segment_t *p_cur = p_text_segments;
    while( p_segment3g )
    {
        FontSizeConvert( p_root_style, p_segment3g->s->style );

        tx3g_segment_t * p_old = p_segment3g;
        p_segment3g = p_segment3g->p_next3g;
        free( p_old );
        if( p_segment3g )
            p_cur->p_next = p_segment3g->s;
        p_cur = p_cur->p_next;
    }

    p_spu_sys->region.p_segments = p_text_segments;

    block_Release( p_block );

    decoder_QueueSub( p_dec, p_spu );
    return VLCDEC_SUCCESS;
}

/*****************************************************************************
 * Extradata Parsing
 *****************************************************************************/
static void ParseExtradataTx3g( decoder_t *p_dec )
{
    text_style_t *p_style = (text_style_t *) p_dec->p_sys;
    const uint8_t *p_extra = p_dec->fmt_in.p_extra;

    if( p_dec->fmt_in.i_extra < 32 )
        return;

    /* DF @0 */
    /* Just @4 */

    /* BGColor @6 */
    p_style->i_background_color = GetDWBE(&p_extra[6]) >> 8;
    p_style->i_background_alpha = p_extra[9];
    p_style->i_features |= STYLE_HAS_BACKGROUND_COLOR|STYLE_HAS_BACKGROUND_ALPHA;

    /* BoxRecord @10 */

    /* StyleRecord @18 */
    p_style->i_style_flags = ConvertToVLCFlags( p_extra[24] );
    if( p_style->i_style_flags )
        p_style->i_features |= STYLE_HAS_FLAGS;
    p_style->i_font_size = p_extra[25];
    p_style->i_font_color = GetDWBE(&p_extra[26]) >> 8;// RGBA -> RGB
    p_style->i_font_alpha = p_extra[29];
    p_style->i_features |= STYLE_HAS_FONT_COLOR | STYLE_HAS_FONT_ALPHA;

    /* FontTableBox @30 */
}

static void ParseExtradataTextMedia( decoder_t *p_dec )
{
    text_style_t *p_style = (text_style_t *) p_dec->p_sys;
    const uint8_t *p_extra = p_dec->fmt_in.p_extra;

    if( p_dec->fmt_in.i_extra < 44 )
        return;

    /* DF @0 */
    uint32_t i_flags = GetDWBE(p_extra);
    if(i_flags & 0x1000) /* drop shadow */
    {
        p_style->i_style_flags |= STYLE_SHADOW;
        p_style->i_features    |= STYLE_HAS_SHADOW_COLOR|STYLE_HAS_FLAGS|STYLE_HAS_SHADOW_ALPHA;
        p_style->i_shadow_color = 0xC0C0C0;
        p_style->i_shadow_alpha = STYLE_ALPHA_OPAQUE;
    }
    if(i_flags & 0x4000) /* key text*/
    {
        /*Controls background color. If this flag is set to 1, the text media handler does not display the
          background color, so that the text overlay background tracks.*/
        p_style->i_style_flags &= ~STYLE_BACKGROUND;
    }

    /* Just @4 */

    /* BGColor @8, read top of 16 bits */
    p_style->i_background_color = (p_extra[8]  << 16) |
                                  (p_extra[10] <<  8) |
                                   p_extra[12];
    p_style->i_features |= STYLE_HAS_BACKGROUND_COLOR | STYLE_HAS_BACKGROUND_ALPHA;
    p_style->i_background_alpha = STYLE_ALPHA_OPAQUE;

    /* BoxRecord @14 */
    /* Reserved 64 @22 */
    /* Font # @30 */

    /* Font Face @32 */
    p_style->i_style_flags |= ConvertToVLCFlags( GetWBE(&p_extra[32]) );
    if( p_style->i_style_flags )
        p_style->i_features |= STYLE_HAS_FLAGS;
    /* Reserved 8 @34 */
    /* Reserved 16 @35 */
    /* FGColor @37 */
    p_style->i_font_color = (p_extra[37] << 16) |
                            (p_extra[39] <<  8) |
                             p_extra[41];
    p_style->i_features |= STYLE_HAS_FONT_COLOR;

    /* FontName Pascal (8 + string) @43 */
}
/*****************************************************************************
 * Decoder entry/exit points
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t *) p_this;
    text_style_Delete( (text_style_t *) p_dec->p_sys );
}

static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t *) p_this;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_TX3G &&
        p_dec->fmt_in.i_codec != VLC_CODEC_QTXT )
        return VLC_EGENERIC;

    p_dec->pf_decode = Decode;

    p_dec->p_sys = text_style_Create( STYLE_NO_DEFAULTS );
    if( !p_dec->p_sys )
        return VLC_ENOMEM;

    text_style_t *p_default_style = p_dec->p_sys;
    p_default_style->i_style_flags |= STYLE_BACKGROUND;
    p_default_style->i_features |= STYLE_HAS_FLAGS;

    if( p_dec->fmt_in.i_codec == VLC_CODEC_TX3G )
        ParseExtradataTx3g( p_dec );
    else
        ParseExtradataTextMedia( p_dec );

    p_dec->fmt_out.i_codec = VLC_CODEC_TEXT;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Encoder entry/exit
 *****************************************************************************/
#ifdef ENABLE_SOUT
static void FillExtradataTx3g( void **pp_extra, int *pi_extra )
{
    size_t i_extra = 32 + 37;
    uint8_t *p_extra = calloc( 1, i_extra );
    if( p_extra )
    {
        p_extra[4] = 0x01;/* 1  center, horizontal */
        p_extra[5] = 0xFF;/* -1 bottom, vertical */
        SetDWBE( &p_extra[6],  0x000000FFU ); /* bgcolor */
        p_extra[25] = STYLE_DEFAULT_FONT_SIZE;
        SetDWBE( &p_extra[26], 0xFFFFFFFFU ); /* fgcolor */

        /* FontTableBox */
        SetDWBE(&p_extra[32], 8 + 2 + 6 + 11 + 10);
        memcpy(&p_extra[36], "ftab", 4);

        SetWBE(&p_extra[40], 3); /* entry count */
        /* Font Record 0 */
        p_extra[41] = 5;
        memcpy(&p_extra[42], "Serif", 5);
        /* Font Record 1 */
        p_extra[47] = 10;
        memcpy(&p_extra[48], "Sans-serif", 10);
        /* Font Record 2 */
        p_extra[58] = 9;
        memcpy(&p_extra[59], "Monospace", 9);

        *pp_extra = p_extra;
        *pi_extra = i_extra;
    }
}

static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;

    if( p_enc->fmt_out.i_codec != VLC_CODEC_TX3G )
        return VLC_EGENERIC;

    p_enc->fmt_in.i_codec = VLC_CODEC_TEXT;

    p_enc->p_sys = NULL;

    p_enc->pf_encode_sub = Encode;
    p_enc->fmt_out.i_cat = SPU_ES;

    if( !p_enc->fmt_out.i_extra )
        FillExtradataTx3g( &p_enc->fmt_out.p_extra, &p_enc->fmt_out.i_extra );

    return VLC_SUCCESS;
}

static int ConvertFromVLCFlags( const text_style_t *p_style )
{
    int i_atomflags = 0;
    if( p_style->i_features & STYLE_HAS_FLAGS )
    {
        if ( p_style->i_style_flags & STYLE_BOLD )
            i_atomflags |= FONT_FACE_BOLD;
        if ( p_style->i_style_flags & STYLE_ITALIC )
            i_atomflags |= FONT_FACE_ITALIC;
        if ( p_style->i_style_flags & STYLE_UNDERLINE )
            i_atomflags |= FONT_FACE_UNDERLINE;
    }
    return i_atomflags;
}

static uint32_t ConvertFromVLCColor( const text_style_t *p_style )
{
    uint32_t rgba = 0;
    if( p_style->i_features & STYLE_HAS_FONT_COLOR )
        rgba = ((uint32_t)p_style->i_font_color) << 8;
    else
        rgba = 0xFFFFFF00U;
    if( p_style->i_features & STYLE_HAS_FONT_ALPHA )
        rgba |= p_style->i_font_alpha;
    else
        rgba |= 0xFF;
    return rgba;
}

static bool NeedStyling( const text_segment_t *p_segment )
{
    const text_style_t *p_style = p_segment->style;
    if( !p_style )
        return false;

    if( p_style->i_features & STYLE_HAS_FLAGS )
    {
        if( p_style->i_style_flags & (STYLE_BOLD|STYLE_ITALIC|STYLE_UNDERLINE) )
            return true;
    }

    if( p_style->i_features & (STYLE_HAS_FONT_COLOR|STYLE_HAS_FONT_ALPHA) )
        return true;

    return false;
}

static block_t *GetStylBlock( const text_segment_t *p_segment, size_t i_styles )
{
    size_t i_start = 0;
    block_t *p_styl = block_Alloc( 10 + 12 * i_styles );
    if( p_styl )
    {
        SetDWBE( p_styl->p_buffer, p_styl->i_buffer );
        memcpy( &p_styl->p_buffer[4], "styl", 4 );
        SetWBE( &p_styl->p_buffer[8], i_styles );
        p_styl->i_buffer = 10;
        for( ; p_segment; p_segment = p_segment->p_next )
        {
            size_t i_len = str8len( p_segment->psz_text );
            if( NeedStyling( p_segment ) )
            {
                uint8_t *p = &p_styl->p_buffer[p_styl->i_buffer];
                SetWBE( &p[0], i_start );
                SetWBE( &p[2], i_start + i_len );
                SetWBE( &p[4], 0 );
                p[6] = ConvertFromVLCFlags( p_segment->style );
                p[7] = STYLE_DEFAULT_FONT_SIZE;
                SetDWBE(&p[8], ConvertFromVLCColor( p_segment->style ) );
                p_styl->i_buffer += 12;
            }
            i_start += i_len;
        }
    }
    return p_styl;
}

static block_t * Encode( encoder_t *p_enc, subpicture_t *p_spu )
{
    VLC_UNUSED(p_enc);
    const text_segment_t *p_segments = (p_spu->p_region)
                                     ? p_spu->p_region->p_text
                                     : NULL;
    size_t i_len = 0;
    size_t i_styles = 0;

    for(const text_segment_t  *p_segment = p_segments;
                               p_segment; p_segment = p_segment->p_next )
    {
        if( p_segment->style )
            i_styles++;
        i_len += strlen( p_segment->psz_text );
    }

    block_t *p_block = block_Alloc( i_len + 2 );
    if( !p_block )
        return NULL;

    SetWBE(p_block->p_buffer, i_len);
    p_block->i_buffer = 2;
    for(const text_segment_t  *p_segment = p_segments;
                               p_segment; p_segment = p_segment->p_next )
    {
        size_t i_seglen = strlen(p_segment->psz_text);
        memcpy(&p_block->p_buffer[p_block->i_buffer],
                p_segment->psz_text, i_seglen);
        p_block->i_buffer += i_seglen;
    }
    p_block->i_dts = p_block->i_pts = p_spu->i_start;
    p_block->i_length = p_spu->i_stop - p_spu->i_start;

    if( i_styles > 0 )
        p_block->p_next = GetStylBlock( p_segments, i_styles );

    return block_ChainGather( p_block );
}
#endif
