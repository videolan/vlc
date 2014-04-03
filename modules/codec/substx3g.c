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

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static subpicture_t *Decode( decoder_t *, block_t ** );

vlc_module_begin ()
    set_description( N_("tx3g subtitles decoder") )
    set_shortname( N_("tx3g subtitles") )
    set_capability( "decoder", 100 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )
    set_callbacks( Open, NULL )
vlc_module_end ()

/****************************************************************************
 * Local structs
 ****************************************************************************/

/*****************************************************************************
 * Open: probe the decoder and return score
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t *) p_this;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_TX3G )
        return VLC_EGENERIC;

    p_dec->pf_decode_sub = Decode;

    p_dec->fmt_out.i_cat = SPU_ES;
    p_dec->fmt_out.i_codec = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Local:
 *****************************************************************************/

#define FONT_FACE_BOLD      0x1
#define FONT_FACE_ITALIC    0x2
#define FONT_FACE_UNDERLINE 0x4

static int ConvertFlags( int i_atomflags )
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

static void SegmentDoSplit( segment_t *p_segment, uint16_t i_start, uint16_t i_end,
                            segment_t **pp_segment_left,
                            segment_t **pp_segment_middle,
                            segment_t **pp_segment_right )
{
    segment_t *p_segment_left = *pp_segment_left;
    segment_t *p_segment_right = *pp_segment_right;
    segment_t *p_segment_middle = *pp_segment_middle;
    p_segment_left = p_segment_middle = p_segment_right = NULL;

    if ( (p_segment->i_size - i_start < 1) || (p_segment->i_size - i_end < 1) )
        return;

    if ( i_start > 0 )
    {
        p_segment_left = calloc( 1, sizeof(segment_t) );
        if ( !p_segment_left ) goto error;
        memcpy( &p_segment_left->styles, &p_segment->styles, sizeof(segment_style_t) );
        p_segment_left->psz_string = str8indup( p_segment->psz_string, 0, i_start );
        p_segment_left->i_size = str8len( p_segment_left->psz_string );
    }

    p_segment_middle = calloc( 1, sizeof(segment_t) );
    if ( !p_segment_middle ) goto error;
    memcpy( &p_segment_middle->styles, &p_segment->styles, sizeof(segment_style_t) );
    p_segment_middle->psz_string = str8indup( p_segment->psz_string, i_start, i_end - i_start + 1 );
    p_segment_middle->i_size = str8len( p_segment_middle->psz_string );

    if ( i_end < (p_segment->i_size - 1) )
    {
        p_segment_right = calloc( 1, sizeof(segment_t) );
        if ( !p_segment_right ) goto error;
        memcpy( &p_segment_right->styles, &p_segment->styles, sizeof(segment_style_t) );
        p_segment_right->psz_string = str8indup( p_segment->psz_string, i_end + 1, p_segment->i_size - i_end - 1 );
        p_segment_right->i_size = str8len( p_segment_right->psz_string );
    }

    if ( p_segment_left ) p_segment_left->p_next = p_segment_middle;
    if ( p_segment_right ) p_segment_middle->p_next = p_segment_right;

    *pp_segment_left = p_segment_left;
    *pp_segment_middle = p_segment_middle;
    *pp_segment_right = p_segment_right;

    return;

error:
    SegmentFree( p_segment_left );
    SegmentFree( p_segment_middle );
    SegmentFree( p_segment_right );
}

static bool SegmentSplit( segment_t *p_prev, segment_t **pp_segment,
                          const uint16_t i_start, const uint16_t i_end,
                          const segment_style_t *p_styles )
{
    segment_t *p_segment_left = NULL, *p_segment_middle = NULL, *p_segment_right = NULL;

    if ( (*pp_segment)->i_size == 0 ) return false;
    if ( i_start > i_end ) return false;
    if ( (size_t)(i_end - i_start) > (*pp_segment)->i_size - 1 ) return false;
    if ( i_end > (*pp_segment)->i_size - 1 ) return false;

    SegmentDoSplit( *pp_segment, i_start, i_end, &p_segment_left, &p_segment_middle, &p_segment_right );
    if ( !p_segment_middle )
    {
        /* Failed */
        SegmentFree( p_segment_left );
        SegmentFree( p_segment_right );
        return false;
    }

    segment_t *p_next = (*pp_segment)->p_next;
    SegmentFree( *pp_segment );
    *pp_segment = ( p_segment_left ) ? p_segment_left : p_segment_middle ;
    if ( p_prev ) p_prev->p_next = *pp_segment;

    if ( p_segment_right )
        p_segment_right->p_next = p_next;
    else
        p_segment_middle->p_next = p_next;

    p_segment_middle->styles = *p_styles;

    return true;
}

/* Creates a new segment using the given style and split existing ones according
   to the start & end offsets */
static void ApplySegmentStyle( segment_t **pp_segment, const uint16_t i_absstart,
                               const uint16_t i_absend, const segment_style_t *p_styles )
{
    /* find the matching segment */
    uint16_t i_curstart = 0;
    segment_t *p_prev = NULL;
    segment_t *p_cur = *pp_segment;
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
            p_cur = p_cur->p_next;
        }
    }
}

/*****************************************************************************
 * Decode:
 *****************************************************************************/
static subpicture_t *Decode( decoder_t *p_dec, block_t **pp_block )
{
    block_t       *p_block;
    subpicture_t  *p_spu = NULL;

    if( ( pp_block == NULL ) || ( *pp_block == NULL ) ) return NULL;
    p_block = *pp_block;
    *pp_block = NULL;

    if( ( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) ) ||
          p_block->i_buffer < sizeof(uint16_t) )
    {
        block_Release( p_block );
        return NULL;
    }

    uint8_t *p_buf = p_block->p_buffer;

    /* Read our raw string and create the styled segment for HTML */
    uint16_t i_psz_bytelength = GetWBE( p_buf );
    const uint8_t *p_pszstart = p_block->p_buffer + sizeof(uint16_t);
    char *psz_subtitle;
    if ( i_psz_bytelength > 2 &&
         ( !memcmp( p_pszstart, "\xFE\xFF", 2 ) || !memcmp( p_pszstart, "\xFF\xFE", 2 ) )
       )
    {
        psz_subtitle = FromCharset( "UTF-16", p_pszstart, i_psz_bytelength );
        if ( !psz_subtitle ) return NULL;
    }
    else
    {
        psz_subtitle = malloc( i_psz_bytelength + 1 );
        if ( !psz_subtitle ) return NULL;
        memcpy( psz_subtitle, p_pszstart, i_psz_bytelength );
        psz_subtitle[ i_psz_bytelength ] = '\0';
    }
    p_buf += i_psz_bytelength + sizeof(uint16_t);

    for( uint16_t i=0; i < i_psz_bytelength; i++ )
     if ( psz_subtitle[i] == '\r' ) psz_subtitle[i] = '\n';

    segment_t *p_segment = calloc( 1, sizeof(segment_t) );
    if ( !p_segment )
    {
        free( psz_subtitle );
        return NULL;
    }
    p_segment->psz_string = strdup( psz_subtitle );
    p_segment->i_size = str8len( psz_subtitle );
    if ( p_dec->fmt_in.subs.p_style )
    {
        p_segment->styles.i_color = p_dec->fmt_in.subs.p_style->i_font_color;
        p_segment->styles.i_color |= p_dec->fmt_in.subs.p_style->i_font_alpha << 24;
        if ( p_dec->fmt_in.subs.p_style->i_style_flags )
            p_segment->styles.i_flags = p_dec->fmt_in.subs.p_style->i_style_flags;
        p_segment->styles.i_fontsize = p_dec->fmt_in.subs.p_style->i_font_size;
    }

    if ( !p_segment->psz_string )
    {
        SegmentFree( p_segment );
        free( psz_subtitle );
        return NULL;
    }

    /* Create the subpicture unit */
    p_spu = decoder_NewSubpictureText( p_dec );
    if( !p_spu )
    {
        free( psz_subtitle );
        SegmentFree( p_segment );
        return NULL;
    }
    subpicture_updater_sys_t *p_spu_sys = p_spu->updater.p_sys;

    /* Parse our styles */
    while( (size_t)(p_buf - p_block->p_buffer) + 8 < p_block->i_buffer )
    {
        uint32_t i_atomsize = GetDWBE( p_buf );
        vlc_fourcc_t i_atomtype = VLC_FOURCC(p_buf[4],p_buf[5],p_buf[6],p_buf[7]);
        p_buf += 8;
        switch( i_atomtype )
        {

        case VLC_FOURCC('s','t','y','l'):
        {
            if ( (size_t)(p_buf - p_block->p_buffer) < 14 ) break;
            uint16_t i_nbrecords = GetWBE(p_buf);
            uint16_t i_cur_record = 0;
            p_buf += 2;
            while( i_cur_record++ < i_nbrecords )
            {
                if ( (size_t)(p_buf - p_block->p_buffer) < 12 ) break;
                uint16_t i_start = __MIN( GetWBE(p_buf), i_psz_bytelength - 1 );
                uint16_t i_end =  __MIN( GetWBE(p_buf + 2), i_psz_bytelength - 1 );

                segment_style_t style;
                style.i_flags = ConvertFlags( p_buf[6] );
                style.i_fontsize = p_buf[7];
                style.i_color = GetDWBE(p_buf+8) >> 8;// RGBA -> ARGB
                style.i_color |= (GetDWBE(p_buf+8) & 0xFF) << 24;
                ApplySegmentStyle( &p_segment, i_start, i_end, &style );

                if ( i_nbrecords == 1 )
                {
                    if ( p_buf[6] )
                    {
                        p_spu_sys->style_flags.i_value = ConvertFlags( p_buf[6] );
                        p_spu_sys->style_flags.b_set = true;
                    }
                    p_spu_sys->i_font_height_abs_to_src = p_buf[7];
                    p_spu_sys->font_color.i_value = GetDWBE(p_buf+8) >> 8;// RGBA -> ARGB
                    p_spu_sys->font_color.i_value |= (GetDWBE(p_buf+8) & 0xFF) << 24;
                    p_spu_sys->font_color.b_set = true;
                }

                p_buf += 12;
            }
        }   break;

        case VLC_FOURCC('d','r','p','o'):
            if ( (size_t)(p_buf - p_block->p_buffer) < 4 ) break;
            p_spu_sys->i_drop_shadow = __MAX( GetWBE(p_buf), GetWBE(p_buf+2) );
            break;

        case VLC_FOURCC('d','r','p','t'):
            if ( (size_t)(p_buf - p_block->p_buffer) < 2 ) break;
            p_spu_sys->i_drop_shadow_alpha = GetWBE(p_buf);
            break;

        default:
            break;

        }
        p_buf += i_atomsize;
    }

    p_spu->i_start    = p_block->i_pts;
    p_spu->i_stop     = p_block->i_pts + p_block->i_length;
    p_spu->b_ephemer  = (p_block->i_length == 0);
    p_spu->b_absolute = false;

    p_spu_sys->align = SUBPICTURE_ALIGN_BOTTOM;
    p_spu_sys->text  = psz_subtitle;
    p_spu_sys->p_htmlsegments = p_segment;

    block_Release( p_block );

    return p_spu;
}
