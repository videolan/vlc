/*****************************************************************************
 * dvbsub.c : DVB subtitles decoder
 *            DVB subtitles encoder (developed for Anevia, www.anevia.com)
 *****************************************************************************
 * Copyright (C) 2003 ANEVIA
 * Copyright (C) 2003-2004 VideoLAN
 * $Id$
 *
 * Authors: Damien LUCAS <damien.lucas@anevia.com>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>
#include <vlc/sout.h>

#include "vlc_bits.h"

//#define DEBUG_DVBSUB 1

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );
static subpicture_t *Decode( decoder_t *, block_t ** );

static int OpenEncoder  ( vlc_object_t * );
static void CloseEncoder( vlc_object_t * );
static block_t *Encode  ( encoder_t *, subpicture_t * );

vlc_module_begin();
    set_description( _("DVB subtitles decoder") );
    set_capability( "decoder", 50 );
    set_callbacks( Open, Close );

#   define ENC_CFG_PREFIX "sout-dvbsub-"
    add_submodule();
    set_description( _("DVB subtitles encoder") );
    set_capability( "encoder", 100 );
    set_callbacks( OpenEncoder, CloseEncoder );
vlc_module_end();

static const char *ppsz_enc_options[] = { NULL };

/****************************************************************************
 * Local structures
 ****************************************************************************
 * Those structures refer closely to the ETSI 300 743 Object model
 ****************************************************************************/

/* Storage of a RLE entry */
typedef struct dvbsub_rle_s
{
    uint16_t                i_num;
    int                     i_color_code;
    int                     i_bpp;
    uint8_t                 y;
    uint8_t                 cr;
    uint8_t                 cb;
    uint8_t                 t;
    struct dvbsub_rle_s     *p_next;

} dvbsub_rle_t;

/* A subpicture image is a list of codes
 * We need to store the length of each line since nothing specify in
 * the standard that all lines should have the same length
 * WARNING: We assume here that a spu is less than 576 lines high */
typedef struct
{
    uint16_t                i_rows;
    uint16_t                i_cols[576];
    dvbsub_rle_t            *p_last;
    dvbsub_rle_t            *p_codes;

} dvbsub_image_t;

/* The object definition gives the position of the object in a region */
typedef struct dvbsub_objectdef_s
{
    uint16_t                  i_id;
    uint8_t                   i_type;
    uint8_t                   i_provider;
    uint16_t                  i_x;
    uint16_t                  i_y;
    uint8_t                   i_fg_pc;
    uint8_t                   i_bg_pc;

} dvbsub_objectdef_t;

/* An object is constituted of 2 images (for interleaving) */
typedef struct dvbsub_object_s
{
    uint16_t                i_id;
    uint8_t                 i_version_number;
    uint8_t                 i_coding_method;
    vlc_bool_t              b_non_modify_color;
    dvbsub_image_t         *topfield;
    dvbsub_image_t         *bottomfield;
    struct dvbsub_object_s *p_next;

} dvbsub_object_t;

/* The object definition gives the position of the object in a region */
typedef struct dvbsub_regiondef_s
{
    uint16_t                  i_id;
    uint16_t                  i_x;
    uint16_t                  i_y;

} dvbsub_regiondef_t;

/* The Region is an aera on the image
 * with a list of the object definitions associated and a CLUT */
typedef struct dvbsub_region_s
{
    uint8_t                 i_id;
    uint8_t                 i_version_number;
    vlc_bool_t              b_fill;
    uint16_t                i_x;
    uint16_t                i_y;
    uint16_t                i_width;
    uint16_t                i_height;
    uint8_t                 i_level_comp;
    uint8_t                 i_depth;
    uint8_t                 i_clut;
    uint8_t                 i_8bp_code;
    uint8_t                 i_4bp_code;
    uint8_t                 i_2bp_code;

    int                     i_object_defs;
    dvbsub_objectdef_t      *p_object_defs;

    struct dvbsub_region_s  *p_next;

} dvbsub_region_t;

/* The page defines the list of regions */
typedef struct
{
    uint16_t              i_id;
    uint8_t               i_timeout;
    uint8_t               i_state;
    uint8_t               i_version_number;

    uint8_t               i_region_defs;
    dvbsub_regiondef_t    *p_region_defs;

} dvbsub_page_t;

/* The entry in the palette CLUT */
typedef struct
{
    uint8_t                 Y;
    uint8_t                 Cr;
    uint8_t                 Cb;
    uint8_t                 T;

} dvbsub_color_t;

/* */
typedef struct
{
    uint8_t                 i_id;
    uint8_t                 i_version_number;
    dvbsub_color_t          c_2b[4];
    dvbsub_color_t          c_4b[16];
    dvbsub_color_t          c_8b[256];

} dvbsub_clut_t;

struct decoder_sys_t
{
    bs_t            bs;

    /* Decoder internal data */
    int             i_id;
    int             i_ancillary_id;
    mtime_t         i_pts;

    dvbsub_page_t   *p_page;
    dvbsub_region_t *p_regions;
    dvbsub_object_t *p_objects;

    dvbsub_clut_t   *p_clut[256];
    dvbsub_clut_t   default_clut;
};


// List of different SEGMENT TYPES
// According to EN 300-743, table 2
#define DVBSUB_ST_PAGE_COMPOSITION      0x10
#define DVBSUB_ST_REGION_COMPOSITION    0x11
#define DVBSUB_ST_CLUT_DEFINITION       0x12
#define DVBSUB_ST_OBJECT_DATA           0x13
#define DVBSUB_ST_ENDOFDISPLAY          0x80
#define DVBSUB_ST_STUFFING              0xff
// List of different OBJECT TYPES
// According to EN 300-743, table 6
#define DVBSUB_OT_BASIC_BITMAP          0x00
#define DVBSUB_OT_BASIC_CHAR            0x01
#define DVBSUB_OT_COMPOSITE_STRING      0x02
// Pixel DATA TYPES
// According to EN 300-743, table 9
#define DVBSUB_DT_2BP_CODE_STRING       0x10
#define DVBSUB_DT_4BP_CODE_STRING       0x11
#define DVBSUB_DT_8BP_CODE_STRING       0x12
#define DVBSUB_DT_24_TABLE_DATA         0x20
#define DVBSUB_DT_28_TABLE_DATA         0x21
#define DVBSUB_DT_48_TABLE_DATA         0x22
#define DVBSUB_DT_END_LINE              0xf0
// List of different Page Composition Segment state
// According to EN 300-743, 7.2.1 table 3
#define DVBSUB_PCS_STATE_ACQUISITION    0x01
#define DVBSUB_PCS_STATE_CHANGE         0x10

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void decode_segment( decoder_t *, bs_t * );
static void decode_page_composition( decoder_t *, bs_t * );
static void decode_region_composition( decoder_t *, bs_t * );
static void decode_object( decoder_t *, bs_t * );
static void decode_clut( decoder_t *, bs_t * );

static void free_objects( decoder_t * );
static void free_all( decoder_t * );

static subpicture_t *render( decoder_t * );

static void default_clut_init( decoder_t * );

/*****************************************************************************
 * Open: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to chose.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t *) p_this;
    decoder_sys_t *p_sys;
    int i;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('d','v','b','s') )
    {
        return VLC_EGENERIC;
    }

    p_dec->pf_decode_sub = Decode;
    p_sys = p_dec->p_sys = malloc( sizeof(decoder_sys_t) );

    p_sys->i_pts          = 0;
    p_sys->i_id           = p_dec->fmt_in.subs.dvb.i_id & 0xFFFF;
    p_sys->i_ancillary_id = p_dec->fmt_in.subs.dvb.i_id >> 16;
    p_sys->p_page         = NULL;
    p_sys->p_regions      = NULL;
    p_sys->p_objects      = NULL;
    for( i = 0; i < 256; i++ ) p_sys->p_clut[i] = NULL;

    es_format_Init( &p_dec->fmt_out, SPU_ES, VLC_FOURCC( 'd','v','b','s' ) );

    default_clut_init( p_dec );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*) p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    free_all( p_dec );
    free( p_sys );
}

/*****************************************************************************
 * Decode:
 *****************************************************************************/
static subpicture_t *Decode( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t       *p_block;
    subpicture_t  *p_spu = NULL;

    if( pp_block == NULL || *pp_block == NULL ) return NULL;
    p_block = *pp_block;
    *pp_block = NULL;

    p_sys->i_pts = p_block->i_pts;
    if( p_sys->i_pts <= 0 )
    {
#ifdef DEBUG_DVBSUB
        /* Some DVB channels send stuffing segments in non-dated packets so
         * don't complain too loudly. */
        msg_Warn( p_dec, "non dated subtitle" );
#endif
        block_Release( p_block );
        return NULL;
    }

    bs_init( &p_sys->bs, p_block->p_buffer, p_block->i_buffer );

    if( bs_read( &p_sys->bs, 8 ) != 0x20 ) /* Data identifier */
    {
        msg_Dbg( p_dec, "invalid data identifier" );
        block_Release( p_block );
        return NULL;
    }

    if( bs_read( &p_sys->bs, 8 ) ) /* Subtitle stream id */
    {
        msg_Dbg( p_dec, "invalid subtitle stream id" );
        block_Release( p_block );
        return NULL;
    }

#ifdef DEBUG_DVBSUB
    msg_Dbg( p_dec, "subtitle packet received: "I64Fd, p_sys->i_pts );
#endif

    while( bs_show( &p_sys->bs, 8 ) == 0x0f ) /* Sync byte */
    {
        decode_segment( p_dec, &p_sys->bs );
    }

    if( bs_read( &p_sys->bs, 8 ) != 0xff ) /* End marker */
    {
        msg_Warn( p_dec, "end marker not found (corrupted subtitle ?)" );
        block_Release( p_block );
        return NULL;
    }

    /* Check if the page is to be displayed */
    if( p_sys->p_page ) p_spu = render( p_dec );

    block_Release( p_block );

    return p_spu;
}

/* following functions are local */

/*****************************************************************************
 * default_clut_init: default clut as defined in EN 300-743 section 10
 *****************************************************************************/
static void default_clut_init( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t i;

#define RGB_TO_Y(r, g, b) ((int16_t) 77 * r + 150 * g + 29 * b) / 256;
#define RGB_TO_U(r, g, b) ((int16_t) -44 * r - 87 * g + 131 * b) / 256;
#define RGB_TO_V(r, g, b) ((int16_t) 131 * r - 110 * g - 21 * b) / 256;

    /* 4 entries CLUT */
    for( i = 0; i < 4; i++ )
    {
        uint8_t R = 0, G = 0, B = 0, T = 0;

        if( !(i & 0x2) && !(i & 0x1) ) T = 0xFF;
        else if( !(i & 0x2) && (i & 0x1) ) R = G = B = 0xFF;
        else if( (i & 0x2) && !(i & 0x1) ) R = G = B = 0;
        else R = G = B = 0x7F;

        p_sys->default_clut.c_2b[i].Y = RGB_TO_Y(R,G,B);
        p_sys->default_clut.c_2b[i].Cr = RGB_TO_U(R,G,B);
        p_sys->default_clut.c_2b[i].Cb = RGB_TO_V(R,G,B);
        p_sys->default_clut.c_2b[i].T = T;
    }

    /* 16 entries CLUT */
    for( i = 0; i < 16; i++ )
    {
        uint8_t R = 0, G = 0, B = 0, T = 0;

        if( !(i & 0x8) )
        {
            if( !(i & 0x4) && !(i & 0x2) && !(i & 0x1) )
            {
                T = 0xFF;
            }
            else
            {
                R = (i & 0x1) ? 0xFF : 0;
                G = (i & 0x2) ? 0xFF : 0;
                B = (i & 0x4) ? 0xFF : 0;
            }
        }
        else
        {
            R = (i & 0x1) ? 0x7F : 0;
            G = (i & 0x2) ? 0x7F : 0;
            B = (i & 0x4) ? 0x7F : 0;
        }

        p_sys->default_clut.c_4b[i].Y = RGB_TO_Y(R,G,B);
        p_sys->default_clut.c_4b[i].Cr = RGB_TO_U(R,G,B);
        p_sys->default_clut.c_4b[i].Cb = RGB_TO_V(R,G,B);
        p_sys->default_clut.c_4b[i].T = T;
    }

    /* 256 entries CLUT (TODO) */
    memset( p_sys->default_clut.c_8b, 0xFF, 256 * sizeof(dvbsub_color_t) );
}

static void decode_segment( decoder_t *p_dec, bs_t *s )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_type;
    int i_page_id;
    int i_size;

    /* sync_byte (already checked) */
    bs_skip( s, 8 );

    /* segment type */
    i_type = bs_read( s, 8 );

    /* page id */
    i_page_id = bs_read( s, 16 );

    /* segment size */
    i_size = bs_show( s, 16 );

    if( i_page_id != p_sys->i_id && i_page_id != p_sys->i_ancillary_id )
    {
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "subtitle skipped (page id: %i, %i)",
                 i_page_id, p_sys->i_id );
#endif
        bs_skip( s,  8 * ( 2 + i_size ) );
        return;
    }

#ifdef DEBUG_DVBSUB
    if( i_page_id == p_sys->i_id )
        msg_Dbg( p_dec, "segment (id: %i)", i_page_id );
    else
        msg_Dbg( p_dec, "ancillary segment (id: %i)", i_page_id );
#endif

    switch( i_type )
    {
    case DVBSUB_ST_PAGE_COMPOSITION:
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "decode_page_composition" );
#endif
        decode_page_composition( p_dec, s );
        break;

    case DVBSUB_ST_REGION_COMPOSITION:
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "decode_region_composition" );
#endif
        decode_region_composition( p_dec, s );
        break;

    case DVBSUB_ST_CLUT_DEFINITION:
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "decode_clut" );
#endif
        decode_clut( p_dec, s );
        break;

    case DVBSUB_ST_OBJECT_DATA:
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "decode_object" );
#endif
        decode_object( p_dec, s );
        break;

    case DVBSUB_ST_ENDOFDISPLAY:
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "end of display" );
#endif
        bs_skip( s,  8 * ( 2 + i_size ) );
        break;

    case DVBSUB_ST_STUFFING:
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "skip stuffing" );
#endif
        bs_skip( s,  8 * ( 2 + i_size ) );
        break;

    default:
        msg_Warn( p_dec, "unsupported segment type: (%04x)", i_type );
        bs_skip( s,  8 * ( 2 + i_size ) );
        break;
    }
}

static void decode_clut( decoder_t *p_dec, bs_t *s )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint16_t      i_segment_length;
    uint16_t      i_processed_length;
    dvbsub_clut_t *p_clut;
    uint8_t       i_clut_id;
    uint8_t       i_version_number;

    i_segment_length = bs_read( s, 16 );
    i_clut_id        = bs_read( s, 8 );
    i_version_number = bs_read( s, 4 );

    /* Check that this id doesn't not already exist with the same version
     * number and allocate memory if necessary */
    if( p_sys->p_clut[i_clut_id] != NULL &&
        p_sys->p_clut[i_clut_id]->i_version_number == i_version_number )
    {
        /* Nothing to do */
        bs_skip( s,  8 * i_segment_length - 12 );
        return;
    }

    if( !p_sys->p_clut[i_clut_id] )
    {
        p_sys->p_clut[i_clut_id] = malloc( sizeof(dvbsub_clut_t) );
    }

    p_clut = p_sys->p_clut[i_clut_id];

    /* We don't have this version of the CLUT: Parse it */
    p_clut->i_version_number = i_version_number;
    bs_skip( s, 4 ); /* Reserved bits */
    i_processed_length = 2;
    while( i_processed_length < i_segment_length )
    {
        uint8_t y, cb, cr, t;
        uint8_t i_id;
        uint8_t i_type;

        i_id = bs_read( s, 8 );
        i_type = bs_read( s, 3 );

        bs_skip( s, 4 );

        if( bs_read( s, 1 ) )
        {
            y  = bs_read( s, 8 );
            cr = bs_read( s, 8 );
            cb = bs_read( s, 8 );
            t  = bs_read( s, 8 );
            i_processed_length += 6;
        }
        else
        {
            y  = bs_read( s, 6 ) << 2;
            cr = bs_read( s, 4 ) << 4;
            cb = bs_read( s, 4 ) << 4;
            t  = bs_read( s, 2 ) << 6;
            i_processed_length += 4;
        }

        /* We are not entirely compliant here as full transparency is indicated
         * with a luma value of zero, not a transparency value of 0xff
         * (full transparency would actually be 0xff + 1). */

        if( y == 0 )
        {
            cr = cb = 0;
            t  = 0xff;
        }

        /* According to EN 300-743 section 7.2.3 note 1, type should
         * not have more than 1 bit set to one, but some streams don't
         * respect this note. */

        if( i_type & 0x04)
        {
            p_clut->c_2b[i_id].Y = y;
            p_clut->c_2b[i_id].Cr = cr;
            p_clut->c_2b[i_id].Cb = cb;
            p_clut->c_2b[i_id].T = t;
        }
        if( i_type & 0x02)
        {
            p_clut->c_4b[i_id].Y = y;
            p_clut->c_4b[i_id].Cr = cr;
            p_clut->c_4b[i_id].Cb = cb;
            p_clut->c_4b[i_id].T = t;
        }
        if( i_type & 0x01)
        {
            p_clut->c_8b[i_id].Y = y;
            p_clut->c_8b[i_id].Cr = cr;
            p_clut->c_8b[i_id].Cb = cb;
            p_clut->c_8b[i_id].T = t;
        }
    }
}

static void decode_page_composition( decoder_t *p_dec, bs_t *s )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    unsigned int i_version_number;
    unsigned int i_state;
    unsigned int i_segment_length;
    uint8_t i_timeout;
    unsigned int i;

    /* A page is composed by one or more region */

    i_segment_length = bs_read( s, 16 );
    i_timeout = bs_read( s, 8 );
    i_version_number = bs_read( s, 4 );
    i_state = bs_read( s, 2 );
    bs_skip( s, 2 ); /* Reserved */

    if( i_state == DVBSUB_PCS_STATE_CHANGE )
    {
        /* End of an epoch, reset decoder buffer */
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "page composition mode change" );
#endif
        free_all( p_dec );
    }
    else if( !p_sys->p_page && i_state != DVBSUB_PCS_STATE_ACQUISITION )
    {
        /* Not a full PCS, we need to wait for one */
        bs_skip( s,  8 * (i_segment_length - 2) );
        return;
    }

    if( i_state == DVBSUB_PCS_STATE_ACQUISITION )
    {
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "acquisition page composition" );
#endif
        /* Make sure we clean up regularly our objects list.
         * Is it the best place to do this ? */
        free_objects( p_dec );
    }

    /* Check version number */
    if( p_sys->p_page &&
        p_sys->p_page->i_version_number == i_version_number )
    {
        bs_skip( s,  8 * (i_segment_length - 2) );
        return;
    }
    else if( p_sys->p_page )
    {
        if( p_sys->p_page->i_region_defs )
            free( p_sys->p_page->p_region_defs );
        p_sys->p_page->i_region_defs = 0;
    }

    if( !p_sys->p_page )
    {
        /* Allocate a new page */
        p_sys->p_page = malloc( sizeof(dvbsub_page_t) );
    }

    p_sys->p_page->i_version_number = i_version_number;
    p_sys->p_page->i_timeout = i_timeout;

    /* Number of regions */
    p_sys->p_page->i_region_defs = (i_segment_length - 2) / 6;

    if( p_sys->p_page->i_region_defs == 0 ) return;

    p_sys->p_page->p_region_defs =
        malloc( p_sys->p_page->i_region_defs * sizeof(dvbsub_region_t) );
    for( i = 0; i < p_sys->p_page->i_region_defs; i++ )
    {
        p_sys->p_page->p_region_defs[i].i_id = bs_read( s, 8 );
        bs_skip( s, 8 ); /* Reserved */
        p_sys->p_page->p_region_defs[i].i_x = bs_read( s, 16 );
        p_sys->p_page->p_region_defs[i].i_y = bs_read( s, 16 );

#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "page_composition, region %i (%i,%i)",
                 i, p_sys->p_page->p_region_defs[i].i_x,
                 p_sys->p_page->p_region_defs[i].i_y );
#endif
    }
}

static void decode_region_composition( decoder_t *p_dec, bs_t *s )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    dvbsub_region_t *p_region, **pp_region = &p_sys->p_regions;
    int i_segment_length;
    int i_processed_length;
    int i_region_id;
    int i_version_number;

    i_segment_length = bs_read( s, 16 );
    i_region_id = bs_read( s, 8 );
    i_version_number = bs_read( s, 4 );

    /* Check if we already have this region */
    for( p_region = p_sys->p_regions; p_region != NULL;
         p_region = p_region->p_next )
    {
        pp_region = &p_region->p_next;
        if( p_region->i_id == i_region_id ) break;
    }

    /* Check version number */
    if( p_region &&
        p_region->i_version_number == i_version_number )
    {
        bs_skip( s,  8 * (i_segment_length - 1) - 4 );
        return;
    }
    else if( p_region )
    {
        if( p_region->i_object_defs )
            free( p_region->p_object_defs );
    }

    if( !p_region )
    {
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "new region: %i", i_region_id );
#endif
        p_region = *pp_region = malloc( sizeof(dvbsub_region_t) );
        p_region->p_next = NULL;
    }

    /* Region attributes */
    p_region->i_id = i_region_id;
    p_region->i_version_number = i_version_number;
    p_region->b_fill           = bs_read( s, 1 );
    bs_skip( s, 3 ); /* Reserved */
    p_region->i_width          = bs_read( s, 16 );
    p_region->i_height         = bs_read( s, 16 );
    p_region->i_level_comp     = bs_read( s, 3 );
    p_region->i_depth          = bs_read( s, 3 );
    bs_skip( s, 2 ); /* Reserved */
    p_region->i_clut           = bs_read( s, 8 );
    p_region->i_8bp_code       = bs_read( s, 8 );
    p_region->i_4bp_code       = bs_read( s, 4 );
    p_region->i_2bp_code       = bs_read( s, 2 );
    bs_skip( s, 2 ); /* Reserved */
    p_region->p_object_defs    = NULL;
    p_region->i_object_defs    = 0;

    /* List of objects in the region */
    i_processed_length = 10;
    while( i_processed_length < i_segment_length )
    {
        dvbsub_objectdef_t *p_obj;

        /* We create a new object */
        p_region->i_object_defs++;
        p_region->p_object_defs =
            realloc( p_region->p_object_defs,
                     sizeof(dvbsub_objectdef_t) * p_region->i_object_defs );

        /* We parse object properties */
        p_obj = &p_region->p_object_defs[p_region->i_object_defs - 1];
        p_obj->i_id         = bs_read( s, 16 );
        p_obj->i_type       = bs_read( s, 2 );
        p_obj->i_provider   = bs_read( s, 2 );
        p_obj->i_x          = bs_read( s, 12 );
        bs_skip( s, 4 ); /* Reserved */
        p_obj->i_y          = bs_read( s, 12 );

        i_processed_length += 6;

        if( p_obj->i_type == DVBSUB_OT_BASIC_CHAR ||
            p_obj->i_type == DVBSUB_OT_COMPOSITE_STRING )
        {
            p_obj->i_fg_pc =  bs_read( s, 8 );
            p_obj->i_bg_pc =  bs_read( s, 8 );
            i_processed_length += 2;
        }
    }
}

static dvbsub_image_t *dvbsub_parse_pdata( decoder_t *, bs_t *, uint16_t );
static uint16_t dvbsub_pdata2bpp( bs_t *, uint16_t *, dvbsub_image_t *, int );
static uint16_t dvbsub_pdata4bpp( bs_t *, uint16_t *, dvbsub_image_t *, int );
static uint16_t dvbsub_pdata8bpp( bs_t *, uint16_t *, dvbsub_image_t *, int );

static void decode_object( decoder_t *p_dec, bs_t *s )
{
    decoder_sys_t   *p_sys = p_dec->p_sys;
    dvbsub_object_t *p_obj, **pp_obj = &p_sys->p_objects;
    int i_segment_length;
    int i_version_number;
    int i_coding_method;
    int i_obj_id;

    i_segment_length   = bs_read( s, 16 );
    i_obj_id           = bs_read( s, 16 );
    i_version_number   = bs_read( s, 4 );
    i_coding_method    = bs_read( s, 2 );

    if( i_coding_method )
    {
        /* TODO: DVB subtitling as characters */
        msg_Dbg( p_dec, "DVB subtitling as characters is not handled!" );
        bs_skip( s,  8 * (i_segment_length - 2) - 6 );
        return;
    }

    /* Check if we already have this region */
    for( p_obj = p_sys->p_objects; p_obj != NULL; p_obj = p_obj->p_next )
    {
        pp_obj = &p_obj->p_next;
        if( p_obj->i_id == i_obj_id ) break;
    }

    /* Check version number */
    if( p_obj && p_obj->i_version_number == i_version_number )
    {
        bs_skip( s,  8 * (i_segment_length - 2) - 6 );
        return;
    }
    else if( p_obj )
    {
        /* Clean structure */
    }

    if( !p_obj )
    {
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "new object: %i", i_obj_id );
#endif
        p_obj = *pp_obj = malloc( sizeof(dvbsub_object_t) );
        p_obj->p_next = NULL;
    }

    p_obj->i_id               = i_obj_id;
    p_obj->i_version_number   = i_version_number;
    p_obj->i_coding_method    = i_coding_method;
    p_obj->b_non_modify_color = bs_read( s, 1 );
    bs_skip( s, 1 ); /* Reserved */

    if( p_obj->i_coding_method == 0x00 )
    {
        uint16_t i_topfield_length;
        uint16_t i_bottomfield_length;

        i_topfield_length    = bs_read( s, 16 );
        i_bottomfield_length = bs_read( s, 16 );

        p_obj->topfield =
            dvbsub_parse_pdata( p_dec, s, i_topfield_length );
        p_obj->bottomfield =
            dvbsub_parse_pdata( p_dec, s, i_bottomfield_length );

        /* Check word-alignement */
        bs_align( s );
        if( bs_pos( s ) % 16 ) bs_skip( s, 8 );
    }
    else
    {
        /* TODO: DVB subtitling as characters */
    }

#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "end object: %i", i_obj_id );
#endif
}

static dvbsub_image_t* dvbsub_parse_pdata( decoder_t *p_dec, bs_t *s,
                                           uint16_t length )
{
    dvbsub_image_t* p_image;
    uint16_t i_processed_length = 0;
    uint16_t i_lines = 0;
    uint16_t i_cols_last = 0;

    p_image = malloc( sizeof(dvbsub_image_t) );
    p_image->p_last = p_image->p_codes = NULL;

    memset( p_image->i_cols, 0, 576 * sizeof(uint16_t) );

    /* Let's parse it a first time to determine the size of the buffer */
    while( i_processed_length < length )
    {
        i_processed_length++;

        switch( bs_read( s, 8 ) )
        {
            case 0x10:
                i_processed_length +=
                    dvbsub_pdata2bpp( s, &p_image->i_cols[i_lines],
                                      p_image, length - i_processed_length );
                break;
            case 0x11:
                i_processed_length +=
                    dvbsub_pdata4bpp( s, &p_image->i_cols[i_lines],
                                      p_image, length - i_processed_length );
                break;
            case 0x12:
                i_processed_length +=
                    dvbsub_pdata8bpp( s, &p_image->i_cols[i_lines],
                                      p_image, length - i_processed_length );
                break;
            case 0x20:
            case 0x21:
            case 0x22:
                /* We don't use map tables */
                break;
            case 0xf0:
                i_lines++; /* End of line code */
                break;
        }
    }

    p_image->i_rows = i_lines;
    p_image->i_cols[i_lines] = i_cols_last;

    return p_image;
}

static void add_rle_code( dvbsub_image_t *p, uint16_t num, uint8_t color,
                          int i_bpp )
{
    if( p->p_last != NULL )
    {
        p->p_last->p_next = malloc( sizeof(dvbsub_rle_t) );
        p->p_last = p->p_last->p_next;
    }
    else
    {
        p->p_codes = malloc( sizeof(dvbsub_rle_t) );
        p->p_last = p->p_codes;
    }
    p->p_last->i_num = num;

    p->p_last->i_color_code = color;
    p->p_last->i_bpp = i_bpp;
    p->p_last->p_next = NULL;
}

static uint16_t dvbsub_pdata2bpp( bs_t *s, uint16_t* p,
                                  dvbsub_image_t* p_image, int i_length )
{
    uint16_t i_processed = 0;
    vlc_bool_t b_stop = 0;
    uint16_t i_count = 0;
    uint8_t i_color = 0;

    while( !b_stop && i_processed/8 < i_length )
    {
        i_processed += 2;
        if( (i_color = bs_read( s, 2 )) != 0x00 )
        {
            (*p)++;

            /* Add 1 pixel */
            add_rle_code( p_image, 1, i_color, 2 );
        }
        else
        {
            i_processed++;
            if( bs_read( s, 1 ) == 0x01 )         // Switch1
            {
                i_count = 3 + bs_read( s, 3 );
                (*p) += i_count ;
                i_color = bs_read( s, 2 );
                add_rle_code( p_image, i_count, i_color, 2 );
                i_processed += 5;
            }
            else
            {
                i_processed++;
                if( bs_read( s, 1 ) == 0x00 )     //Switch2
                {
                    i_processed += 2;
                    switch( bs_read( s, 2 ) )     //Switch3
                    {
                    case 0x00:
                        b_stop=1;
                        break;
                    case 0x01:
                        (*p) += 2 ;
                        add_rle_code( p_image, 2, 0, 2 );
                        break;
                    case 0x02:
                        i_count =  12 + bs_read( s, 4 );
                        i_color = bs_read( s, 2 );
                        (*p) += i_count;
                        i_processed += 6;
                        add_rle_code( p_image, i_count, i_color, 2 );
                        break;
                    case 0x03:
                        i_count =  29 + bs_read( s, 8 );
                        i_color = bs_read( s, 2 );
                        (*p) += i_count;
                        i_processed += 10;
                        add_rle_code( p_image, i_count, i_color, 2 );
                        break;
                    default:
                        break;
                    }
                }
                else
                {
                    (*p)++;
                    add_rle_code( p_image, 1, 0, 2 ); /* 1 pixel color 0 */
                }
            }
        }
    }

    bs_align( s );

    return ( i_processed + 7 ) / 8;
}

static uint16_t dvbsub_pdata4bpp( bs_t *s, uint16_t* p,
                                  dvbsub_image_t* p_image, int i_length )
{
    uint16_t i_processed = 0;
    vlc_bool_t b_stop = 0;
    uint16_t i_count = 0;
    uint8_t i_color = 0;

    while( !b_stop && i_processed/8 < i_length )
    {
        if( (i_color = bs_read( s, 4 )) != 0x00 )
        {
            (*p)++;
            i_processed+=4;

            /* Add 1 pixel */
            add_rle_code( p_image, 1, i_color, 4 );
        }
        else
        {
            if( bs_read( s, 1 ) == 0x00 )           // Switch1
            {
                if( bs_show( s, 3 ) != 0x00 )
                {
                    i_count = 2 + bs_read( s, 3 );
                    (*p) += i_count ;
                    add_rle_code( p_image, i_count, 0x00, 4 );
                }
                else
                {
                    bs_skip( s, 3 );
                    b_stop=1;
                }
                i_processed += 8;
            }
            else
            {
                if( bs_read( s, 1 ) == 0x00)        //Switch2
                {
                    i_count =  4 + bs_read( s, 2 );
                    i_color = bs_read( s, 4 );
                    (*p) += i_count;
                    i_processed += 12;
                    add_rle_code( p_image, i_count, i_color, 4 );
                }
                else
                {
                    switch ( bs_read( s, 2 ) )     //Switch3
                    {
                        case 0x0:
                            (*p)++;
                            i_processed += 8;
                            add_rle_code( p_image, 1, 0x00, 4 );
                            break;
                        case 0x1:
                            (*p)+=2;
                            i_processed += 8;
                            add_rle_code( p_image, 2, 0x00, 4 );
                            break;
                        case 0x2:
                             i_count = 9 + bs_read( s, 4 );
                             i_color = bs_read( s, 4 );
                             (*p)+= i_count;
                             i_processed += 16;
                             add_rle_code( p_image, i_count, i_color, 4 );
                             break;
                        case 0x3:
                             i_count= 25 + bs_read( s, 8 );
                             i_color = bs_read( s, 4 );
                             (*p)+= i_count;
                             i_processed += 20;
                             add_rle_code( p_image, i_count, i_color, 4 );
                             break;
                    }
                }
            }
        }
    }

    bs_align( s );

    return ( i_processed + 7 ) / 8 ;
}

static uint16_t dvbsub_pdata8bpp( bs_t *s, uint16_t* p,
                                  dvbsub_image_t* p_image, int i_length )
{
    uint16_t i_processed = 0;
    vlc_bool_t b_stop = 0;
    uint16_t i_count = 0;
    uint8_t i_color = 0;

    while( !b_stop && i_processed/8 < i_length )
    {
        i_processed += 8;
        if( (i_color = bs_read( s, 8 )) != 0x00 )
        {
            (*p)++;

            /* Add 1 pixel */
            add_rle_code( p_image, 1, i_color, 8 );
        }
        else
        {
            i_processed++;
            if( bs_read( s, 1 ) == 0x00 )           // Switch1
            {
                if( bs_show( s, 7 ) != 0x00 )
                {
                    i_count = bs_read( s, 7 );
                    (*p) += i_count ;
                    add_rle_code( p_image, i_count, 0x00, 8 );
                }
                else
                {
                    bs_skip( s, 7 );
                    b_stop = 1;
                }
                i_processed += 7;
            }
            else
            {
                i_count = bs_read( s, 7 );
                (*p) += i_count ;
                i_color = bs_read( s, 8 );
                add_rle_code( p_image, i_count, i_color, 8 );
                i_processed += 15;
            }
        }
    }

    bs_align( s );

    return ( i_processed + 7 ) / 8 ;
}

static void free_image( dvbsub_image_t *p_i )
{
    dvbsub_rle_t *p1;
    dvbsub_rle_t *p2 = NULL;

    for( p1 = p_i->p_codes; p1 != NULL; p1 = p2 )
    {
        p2 = p1->p_next;
        free( p1 );
        p1 = NULL;
    }

    free( p_i );
}

static void free_objects( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    dvbsub_object_t *p_obj, *p_obj_next;

    for( p_obj = p_sys->p_objects; p_obj != NULL; p_obj = p_obj_next )
    {
        p_obj_next = p_obj->p_next;
        free_image( p_obj->topfield );
        free_image( p_obj->bottomfield );
        free( p_obj );
    }
    p_sys->p_objects = NULL;
}

static void free_all( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    dvbsub_region_t *p_reg, *p_reg_next;
    int i;

    for( i = 0; i < 256; i++ )
    {
        if( p_sys->p_clut[i] ) free( p_sys->p_clut[i] );
        p_sys->p_clut[i] = NULL;
    }

    if( p_sys->p_page )
    {
        if( p_sys->p_page->i_region_defs )
            free( p_sys->p_page->p_region_defs );
        free( p_sys->p_page );
        p_sys->p_page = NULL;
    }

    for( p_reg = p_sys->p_regions; p_reg != NULL; p_reg = p_reg_next )
    {
        p_reg_next = p_reg->p_next;
        if( p_reg->i_object_defs ) free( p_reg->p_object_defs );
        free( p_reg );
    }
    p_sys->p_regions = NULL;

    free_objects( p_dec );
}

static subpicture_t *render( decoder_t *p_dec )
{
    decoder_sys_t   *p_sys = p_dec->p_sys;
    dvbsub_clut_t   *p_clut;
    dvbsub_rle_t    *p_c;
    subpicture_t    *p_spu;
    subpicture_region_t **pp_spu_region;
    int i, j = 0, i_timeout = 0;

    /* Allocate the subpicture internal data. */
    p_spu = p_dec->pf_spu_buffer_new( p_dec );
    if( !p_spu ) return NULL;

    pp_spu_region = &p_spu->p_region;

    /* Loop on region definitions */
#ifdef DEBUG_DVBSUB
    if( p_sys->p_page )
        msg_Dbg( p_dec, "rendering %i regions", p_sys->p_page->i_region_defs );
#endif

    for( i = 0; p_sys->p_page && i < p_sys->p_page->i_region_defs; i++ )
    {
        dvbsub_region_t     *p_region;
        dvbsub_regiondef_t  *p_regiondef;
        subpicture_region_t *p_spu_region;
        dvbsub_color_t      *p_color;
        video_format_t fmt;
        uint8_t *p_y;
        int i_pitch, i_background;

        i_timeout = p_sys->p_page->i_timeout;

        p_regiondef = &p_sys->p_page->p_region_defs[i];

#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "rendering region %i (%i,%i)", i,
                 p_regiondef->i_x, p_regiondef->i_y );
#endif

        /* Find associated region */
        for( p_region = p_sys->p_regions; p_region != NULL;
             p_region = p_region->p_next )
        {
            if( p_regiondef->i_id == p_region->i_id ) break;
        }

        if( !p_region )
        {
            msg_Err( p_dec, "no region founddddd!!!" );
            continue;
        }

        /* Create new SPU region */
        memset( &fmt, 0, sizeof(video_format_t) );
        fmt.i_chroma = VLC_FOURCC('Y','U','V','P');
        fmt.i_aspect = VOUT_ASPECT_FACTOR;
        fmt.i_width = fmt.i_visible_width = p_region->i_width;
        fmt.i_height = fmt.i_visible_height = p_region->i_height;
        fmt.i_x_offset = fmt.i_y_offset = 0;
        p_spu_region = p_spu->pf_create_region( VLC_OBJECT(p_dec), &fmt );
        if( !p_region )
        {
            msg_Err( p_dec, "cannot allocate SPU region" );
            continue;
        }
        p_spu_region->i_x = p_regiondef->i_x;
        p_spu_region->i_y = p_regiondef->i_y;
        *pp_spu_region = p_spu_region;
        pp_spu_region = &p_spu_region->p_next;

        /* Build palette */
        p_clut = p_sys->p_clut[p_region->i_clut];
        if( !p_clut ) p_clut = &p_sys->default_clut;
        fmt.p_palette->i_entries = p_region->i_depth == 1 ? 4 :
            p_region->i_depth == 2 ? 16 : 256;
        p_color = (p_region->i_depth == 1) ? p_clut->c_2b :
            (p_region->i_depth == 2) ? p_clut->c_4b : p_clut->c_8b;
        for( j = 0; j < fmt.p_palette->i_entries; j++ )
        {
            fmt.p_palette->palette[j][0] = p_color[j].Y;
            fmt.p_palette->palette[j][1] = p_color[j].Cr;
            fmt.p_palette->palette[j][2] = p_color[j].Cb;
            fmt.p_palette->palette[j][3] = 0xff - p_color[j].T;
        }

        p_y = p_spu_region->picture.Y_PIXELS;
        i_pitch = p_spu_region->picture.Y_PITCH;

        /* Erase region */
        i_background = (p_region->i_depth == 1) ? p_region->i_2bp_code :
            (p_region->i_depth == 2) ? p_region->i_4bp_code :
            p_region->i_8bp_code;
        memset( p_y, i_background, i_pitch * p_region->i_height );

        /* Loop on object definitions */
        for( j = 0; j < p_region->i_object_defs; j++ )
        {
            dvbsub_object_t    *p_object;
            dvbsub_objectdef_t *p_objectdef;
            uint16_t k, l, x, y;

            p_objectdef = &p_region->p_object_defs[j];

#ifdef DEBUG_DVBSUB
            msg_Dbg( p_dec, "rendering object %i (%i,%i)", p_objectdef->i_id,
                     p_objectdef->i_x, p_objectdef->i_y );
#endif

            /* Look for the right object */
            for( p_object = p_sys->p_objects; p_object != NULL;
                 p_object = p_object->p_next )
            {
                if( p_objectdef->i_id == p_object->i_id ) break;
            }

            if( !p_object )
            {
                msg_Err( p_dec, "no object founddddd!!!" );
                continue;
            }

            /* Draw SPU region */
            for( k = 0, l = 0, p_c = p_object->topfield->p_codes;
                 p_c; p_c = p_c->p_next )
            {
                x = l + p_objectdef->i_x;
                y = 2 * k + p_objectdef->i_y;
                memset( p_y + y * i_pitch + x, p_c->i_color_code, p_c->i_num );

                l += p_c->i_num;
                if( l >= p_object->topfield->i_cols[k] ) { k++; l = 0; }
                if( k >= p_object->topfield->i_rows) break;
            }

            for( k = 0, l = 0, p_c = p_object->bottomfield->p_codes;
                 p_c; p_c = p_c->p_next )
            {
                x = l + p_objectdef->i_x;
                y = 2 * k + 1 + p_objectdef->i_y;
                memset( p_y + y * i_pitch + x, p_c->i_color_code, p_c->i_num );

                l += p_c->i_num;
                if( l >= p_object->bottomfield->i_cols[k] ) { k++; l = 0; }
                if( k >= p_object->bottomfield->i_rows) break;

            }
        }
    }

    /* Set the pf_render callback */
    p_spu->i_start = p_sys->i_pts;
    p_spu->i_stop = p_spu->i_start + i_timeout * 1000000;
    p_spu->b_ephemer = VLC_TRUE;

    return p_spu;
}

/*****************************************************************************
 * encoder_sys_t : encoder descriptor
 *****************************************************************************/
struct encoder_sys_t
{
    unsigned int i_page_ver;
    unsigned int i_region_ver;
    unsigned int i_clut_ver;

    /*
     * Input properties
     */
    /*
     * Common properties
     */
    mtime_t i_pts;
};

static void encode_page_composition( encoder_t *, bs_t *, subpicture_t * );
static void encode_clut( encoder_t *, bs_t *, subpicture_t * );
static void encode_region_composition( encoder_t *, bs_t *, subpicture_t * );
static void encode_object( encoder_t *, bs_t *, subpicture_t * );

/*****************************************************************************
 * OpenEncoder: probe the encoder and return score
 *****************************************************************************/
static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;

    if( p_enc->fmt_out.i_codec != VLC_FOURCC('d','v','b','s') &&
        !p_enc->b_force )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_sys = (encoder_sys_t *)malloc(sizeof(encoder_sys_t)) ) == NULL )
    {
        msg_Err( p_enc, "out of memory" );
        return VLC_EGENERIC;
    }
    p_enc->p_sys = p_sys;

    p_enc->pf_encode_sub = Encode;
    p_enc->fmt_out.i_codec = VLC_FOURCC('d','v','b','s');
    p_enc->fmt_out.subs.dvb.i_id  = 1 << 16 | 1;

    sout_CfgParse( p_enc, ENC_CFG_PREFIX, ppsz_enc_options, p_enc->p_cfg );

    p_sys->i_page_ver = 0;
    p_sys->i_region_ver = 0;
    p_sys->i_clut_ver = 0;

    return VLC_SUCCESS;
}

/****************************************************************************
 * Encode: the whole thing
 ****************************************************************************/
static block_t *Encode( encoder_t *p_enc, subpicture_t *p_subpic )
{
    bs_t bits, *s = &bits;
    block_t *p_block;

    if( !p_subpic || !p_subpic->p_region ) return 0;

    msg_Dbg( p_enc, "encoding subpicture" );

    p_block = block_New( p_enc, 64000 );
    bs_init( s, p_block->p_buffer, p_block->i_buffer );

    bs_write( s, 8, 0x20 ); /* Data identifier */
    bs_write( s, 8, 0x0 ); /* Subtitle stream id */

    encode_page_composition( p_enc, s, p_subpic );
    encode_region_composition( p_enc, s, p_subpic );
    encode_clut( p_enc, s, p_subpic );
    encode_object( p_enc, s, p_subpic );

    /* End of display */
    bs_write( s, 8, 0x0f ); /* Sync byte */
    bs_write( s, 8, DVBSUB_ST_ENDOFDISPLAY ); /* Segment type */
    bs_write( s, 16, 1 ); /* Page id */
    bs_write( s, 16, 0 ); /* Segment length */

    bs_write( s, 8, 0xff ); /* End marker */
    p_block->i_buffer = bs_pos( s ) / 8;
    p_block->i_pts = p_block->i_dts = p_subpic->i_start;
    if( !p_subpic->b_ephemer && p_subpic->i_stop )
        p_block->i_length = p_subpic->i_stop - p_subpic->i_start;

    msg_Dbg( p_enc, "subpicture encoded properly" );

    return p_block;
}

/*****************************************************************************
 * CloseEncoder: encoder destruction
 *****************************************************************************/
static void CloseEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    free( p_sys );
}

static void encode_page_composition( encoder_t *p_enc, bs_t *s,
                                     subpicture_t *p_subpic )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    subpicture_region_t *p_region;
    int i_regions;

    bs_write( s, 8, 0x0f ); /* Sync byte */
    bs_write( s, 8, DVBSUB_ST_PAGE_COMPOSITION ); /* Segment type */
    bs_write( s, 16, 1 ); /* Page id */

    for( i_regions = 0, p_region = p_subpic->p_region; p_region;
         p_region = p_region->p_next, i_regions++ );

    bs_write( s, 16, i_regions * 6 + 2 ); /* Segment length */

    bs_write( s, 8, 5 ); /* Timeout */
    bs_write( s, 4, p_sys->i_page_ver++ );
    bs_write( s, 2, DVBSUB_PCS_STATE_ACQUISITION );
    bs_write( s, 2, 0 ); /* Reserved */

    for( i_regions = 0, p_region = p_subpic->p_region; p_region;
         p_region = p_region->p_next, i_regions++ )
    {
        bs_write( s, 8, i_regions );
        bs_write( s, 8, 0 ); /* Reserved */
        bs_write( s, 16, p_region->i_x );
        bs_write( s, 16, p_region->i_y );
    }
}

static void encode_clut( encoder_t *p_enc, bs_t *s, subpicture_t *p_subpic )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    subpicture_region_t *p_region = p_subpic->p_region;
    video_palette_t *p_pal;
    int i;

    /* Sanity check */
    if( !p_region || !p_region->fmt.p_palette ||
        p_region->fmt.i_chroma != VLC_FOURCC('Y','U','V','P') ) return;

    bs_write( s, 8, 0x0f ); /* Sync byte */
    bs_write( s, 8, DVBSUB_ST_CLUT_DEFINITION ); /* Segment type */
    bs_write( s, 16, 1 ); /* Page id */

    p_pal = p_region->fmt.p_palette;

    bs_write( s, 16, p_pal->i_entries * 6 + 2 ); /* Segment length */
    bs_write( s, 8, 1 ); /* Clut id */
    bs_write( s, 4, p_sys->i_clut_ver++ );
    bs_write( s, 4, 0 ); /* Reserved */

    for( i = 0; i < p_pal->i_entries; i++ )
    {
        bs_write( s, 8, i ); /* Clut entry id */
        bs_write( s, 1, p_pal->i_entries == 4 );   /* 2bit/entry flag */
        bs_write( s, 1, p_pal->i_entries == 16 );  /* 4bit/entry flag */
        bs_write( s, 1, p_pal->i_entries == 256 ); /* 8bit/entry flag */
        bs_write( s, 4, 0 ); /* Reserved */
        bs_write( s, 1, 1 ); /* Full range flag */
        bs_write( s, 8, p_pal->palette[i][0] ); /* Y value */
        bs_write( s, 8, p_pal->palette[i][1] ); /* Cr value */
        bs_write( s, 8, p_pal->palette[i][2] ); /* Cb value */
        bs_write( s, 8, 0xff - p_pal->palette[i][3] ); /* T value */
    }
}

static void encode_region_composition( encoder_t *p_enc, bs_t *s,
                                       subpicture_t *p_subpic )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    subpicture_region_t *p_region;
    int i_regions;

    for( i_regions = 0, p_region = p_subpic->p_region; p_region;
         p_region = p_region->p_next, i_regions++ )
    {
        video_palette_t *p_pal = p_region->fmt.p_palette;
        int i_depth = p_pal->i_entries == 4 ? 0x1 :
            p_pal->i_entries == 16 ? 0x2 : 0x3;

        bs_write( s, 8, 0x0f ); /* Sync byte */
        bs_write( s, 8, DVBSUB_ST_REGION_COMPOSITION ); /* Segment type */
        bs_write( s, 16, 1 ); /* Page id */

        bs_write( s, 16, 10 + 6 ); /* Segment length */
        bs_write( s, 8, i_regions );
        bs_write( s, 4, p_sys->i_region_ver++ );

        /* Region attributes */
        bs_write( s, 1, 0 ); /* Fill */
        bs_write( s, 3, 0 ); /* Reserved */
        bs_write( s, 16, p_region->fmt.i_visible_width );
        bs_write( s, 16, p_region->fmt.i_visible_height );
        bs_write( s, 3, i_depth );  /* Region level of compatibility */
        bs_write( s, 3, i_depth  ); /* Region depth */
        bs_write( s, 2, 0 ); /* Reserved */
        bs_write( s, 8, 1 ); /* Clut id */
        bs_write( s, 8, 0 ); /* region 8bit pixel code */
        bs_write( s, 4, 0 ); /* region 4bit pixel code */
        bs_write( s, 2, 0 ); /* region 2bit pixel code */
        bs_write( s, 2, 0 ); /* Reserved */

        /* In our implementation we only have 1 object per region */
        bs_write( s, 16, i_regions );
        bs_write( s, 2, DVBSUB_OT_BASIC_BITMAP );
        bs_write( s, 2, 0 ); /* object provider flag */
        bs_write( s, 12, 0 ); /* object horizontal position */
        bs_write( s, 4, 0 ); /* Reserved */
        bs_write( s, 12, 0 ); /* object vertical position */
    }
}

static void encode_pixel_data( encoder_t *p_enc, bs_t *s,
                               subpicture_region_t *p_region,
                               vlc_bool_t b_top );

static void encode_object( encoder_t *p_enc, bs_t *s, subpicture_t *p_subpic )
{
    encoder_sys_t   *p_sys = p_enc->p_sys;
    subpicture_region_t *p_region;
    int i_regions;

    int i_length_pos, i_update_pos, i_pixel_data_pos;

    for( i_regions = 0, p_region = p_subpic->p_region; p_region;
         p_region = p_region->p_next, i_regions++ )
    {
        bs_write( s, 8, 0x0f ); /* Sync byte */
        bs_write( s, 8, DVBSUB_ST_OBJECT_DATA ); /* Segment type */
        bs_write( s, 16, 1 ); /* Page id */

        i_length_pos = bs_pos( s );
        bs_write( s, 16, 0 ); /* Segment length */
        bs_write( s, 16, i_regions ); /* Object id */
        bs_write( s, 4, p_sys->i_region_ver++ );
        bs_write( s, 2, 0 ); /* object coding method */

        bs_write( s, 1, 0 ); /* non modifying color flag */
        bs_write( s, 1, 0 ); /* Reserved */

        i_update_pos = bs_pos( s );
        bs_write( s, 16, 0 ); /* topfield data block length */
        bs_write( s, 16, 0 ); /* bottomfield data block length */

        /* Top field */
        i_pixel_data_pos = bs_pos( s );
        encode_pixel_data( p_enc, s, p_region, VLC_TRUE );
        i_pixel_data_pos = ( bs_pos( s ) - i_pixel_data_pos ) / 8;
        SetWBE( &s->p_start[i_update_pos/8], i_pixel_data_pos );

        /* Bottom field */
        i_pixel_data_pos = bs_pos( s );
        encode_pixel_data( p_enc, s, p_region, VLC_FALSE );
        i_pixel_data_pos = ( bs_pos( s ) - i_pixel_data_pos ) / 8;
        SetWBE( &s->p_start[i_update_pos/8+2], i_pixel_data_pos );

        /* Stuffing for word alignment */
        bs_align_0( s );
        if( bs_pos( s ) % 16 ) bs_write( s, 8, 0 );

        /* Update segment length */
        SetWBE( &s->p_start[i_length_pos/8], (bs_pos(s) - i_length_pos)/8 -2 );
    }
}

static void encode_pixel_line_2bp( encoder_t *p_enc, bs_t *s,
                                   subpicture_region_t *p_region,
                                   int i_line );
static void encode_pixel_line_4bp( encoder_t *p_enc, bs_t *s,
                                   subpicture_region_t *p_region,
                                   int i_line );
static void encode_pixel_data( encoder_t *p_enc, bs_t *s,
                               subpicture_region_t *p_region,
                               vlc_bool_t b_top )
{
    unsigned int i_line;

    /* Sanity check */
    if( p_region->fmt.i_chroma != VLC_FOURCC('Y','U','V','P') ) return;

    /* Encode line by line */
    for( i_line = !b_top; i_line < p_region->fmt.i_visible_height;
         i_line += 2 )
    {
        switch( p_region->fmt.p_palette->i_entries )
        {
        case 4:
            bs_write( s, 8, 0x10 ); /* 2 bit/pixel code string */
            encode_pixel_line_2bp( p_enc, s, p_region, i_line );
            break;

        case 16:
            bs_write( s, 8, 0x11 ); /* 4 bit/pixel code string */
            encode_pixel_line_4bp( p_enc, s, p_region, i_line );
            break;

        default:
            msg_Err( p_enc, "subpicture palette (%i) not handled",
                     p_region->fmt.p_palette->i_entries );
            break;
        }

        bs_write( s, 8, 0xf0 ); /* End of object line code */
    }
}

static void encode_pixel_line_2bp( encoder_t *p_enc, bs_t *s,
                                   subpicture_region_t *p_region,
                                   int i_line )
{
    unsigned int i, i_length = 0;
    int i_pitch = p_region->picture.p->i_pitch;
    uint8_t *p_data = &p_region->picture.p->p_pixels[ i_pitch * i_line ];
    int i_last_pixel = p_data[0];

    for( i = 0; i <= p_region->fmt.i_visible_width; i++ )
    {
        if( i != p_region->fmt.i_visible_width &&
            p_data[i] == i_last_pixel && i_length != 284 )
        {
            i_length++;
            continue;
        }

        if( i_length == 1 || i_length == 11 || i_length == 28 )
        {
            /* 2bit/pixel code */
            if( i_last_pixel ) bs_write( s, 2, i_last_pixel );
            else
            {
                bs_write( s, 2, 0 );
                bs_write( s, 1, 0 );
                bs_write( s, 1, 1 ); /* pseudo color 0 */
            }
            i_length--;
        }

        if( i_length == 2 )
        {
            if( i_last_pixel )
            {
                bs_write( s, 2, i_last_pixel );
                bs_write( s, 2, i_last_pixel );
            }
            else
            {
                bs_write( s, 2, 0 );
                bs_write( s, 1, 0 );
                bs_write( s, 1, 0 );
                bs_write( s, 2, 1 ); /* 2 * pseudo color 0 */
            }
        }
        else if( i_length > 2 )
        {
            bs_write( s, 2, 0 );
            if( i_length <= 10 )
            {
                bs_write( s, 1, 1 );
                bs_write( s, 3, i_length - 3 );
                bs_write( s, 2, i_last_pixel );
            }
            else
            {
                bs_write( s, 1, 0 );
                bs_write( s, 1, 0 );

                if( i_length <= 27 )
                {
                    bs_write( s, 2, 2 );
                    bs_write( s, 4, i_length - 12 );
                    bs_write( s, 2, i_last_pixel );
                }
                else
                {
                    bs_write( s, 2, 3 );
                    bs_write( s, 8, i_length - 29 );
                    bs_write( s, 2, i_last_pixel );
                }
            }
        }

        if( i == p_region->fmt.i_visible_width ) break;

        i_last_pixel = p_data[i];
        i_length = 1;
    }

    /* Stop */
    bs_write( s, 2, 0 );
    bs_write( s, 1, 0 );
    bs_write( s, 1, 0 );
    bs_write( s, 2, 0 );

    /* Stuffing */
    bs_align_0( s );
}

static void encode_pixel_line_4bp( encoder_t *p_enc, bs_t *s,
                                   subpicture_region_t *p_region,
                                   int i_line )
{
    unsigned int i, i_length = 0;
    int i_pitch = p_region->picture.p->i_pitch;
    uint8_t *p_data = &p_region->picture.p->p_pixels[ i_pitch * i_line ];
    int i_last_pixel = p_data[0];

    for( i = 0; i <= p_region->fmt.i_visible_width; i++ )
    {
        if( i != p_region->fmt.i_visible_width &&
            p_data[i] == i_last_pixel && i_length != 1 )
        {
            i_length++;
            continue;
        }

        if( i_length == 1 )
        {
            /* 4bit/pixel code */
            if( i_last_pixel ) bs_write( s, 4, i_last_pixel );
            else
            {
                bs_write( s, 4, 0 );
                bs_write( s, 1, 1 );
                bs_write( s, 1, 1 );
                bs_write( s, 2, 0 ); /* pseudo color 0 */
            }
        }

        if( i == p_region->fmt.i_visible_width ) break;

        i_last_pixel = p_data[i];
        i_length = 1;
    }

    /* Stop */
    bs_write( s, 8, 0 );

    /* Stuffing */
    bs_align_0( s );
}
