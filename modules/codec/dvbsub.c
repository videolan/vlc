/*****************************************************************************
 * dvbsub.c : DVB subtitles decoder
 *            DVB subtitles encoder (developed for Anevia, www.anevia.com)
 *****************************************************************************
 * Copyright (C) 2003 ANEVIA
 * Copyright (C) 2003-2009 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Damien LUCAS <damien.lucas@anevia.com>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
 *          Derk-Jan Hartman <hartman #at# videolan dot org>
 *          Simon Hailes <simon _a_ screen.subtitling.com>
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

/*****************************************************************************
 * Preamble
 *
 * FIXME:
 * DVB subtitles coded as strings of characters are not handled correctly.
 * The character codes in the string should actually be indexes referring to a
 * character table identified in the subtitle descriptor.
 *
 * The spec is quite vague in this area, but what is meant is perhaps that it
 * refers to the character index in the codepage belonging to the language
 * specified in the subtitle descriptor. Potentially it's designed for widechar
 * (but not for UTF-*) codepages.
 *****************************************************************************
 *
 *****************************************************************************
 * Notes on DDS (Display Definition Segment)
 * -----------------------------------------
 * DDS (Display Definition Segment) tells the decoder how the subtitle image
 * relates to the video image.
 * For SD, the subtitle image is always considered to be for display at
 * 720x576 (although it's assumed that for NTSC, this is 720x480, this
 * is not documented well) Also, for SD, the subtitle image is drawn 'on
 * the glass' (i.e. after video scaling, letterbox, etc.)
 * For 'HD' (subs marked type 0x14/0x24 in PSI), a DDS must be present,
 * and the subs area is drawn onto the video area (scales if necessary).
 * The DDS tells the decoder what resolution the subtitle images were
 * intended for, and hence how to scale the subtitle images for a
 * particular video size
 * i.e. if HD video is presented as letterbox, the subs will be in the
 * same place on the video as if the video was presented on an HD set
 * indeed, if the HD video was pillarboxed by the decoder, the subs may
 * be cut off as well as the video. The intent here is that the subs can
 * be placed accurately on the video - something which was missed in the
 * original spec.
 *
 * A DDS may also specify a window - this is where the subs images are moved so that the (0,0)
 * origin of decode is offset.
 ********************************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_sout.h>

#include <vlc_bits.h>

/* #define DEBUG_DVBSUB 1 */

#define POSX_TEXT N_("Decoding X coordinate")
#define POSX_LONGTEXT N_("X coordinate of the rendered subtitle")

#define POSY_TEXT N_("Decoding Y coordinate")
#define POSY_LONGTEXT N_("Y coordinate of the rendered subtitle")

#define POS_TEXT N_("Subpicture position")
#define POS_LONGTEXT N_( \
  "You can enforce the subpicture position on the video " \
  "(0=center, 1=left, 2=right, 4=top, 8=bottom, you can " \
  "also use combinations of these values, e.g. 6=top-right).")

#define ENC_POSX_TEXT N_("Encoding X coordinate")
#define ENC_POSX_LONGTEXT N_("X coordinate of the encoded subtitle" )
#define ENC_POSY_TEXT N_("Encoding Y coordinate")
#define ENC_POSY_LONGTEXT N_("Y coordinate of the encoded subtitle" )

static const int pi_pos_values[] = { 0, 1, 2, 4, 8, 5, 6, 9, 10 };
static const char *const ppsz_pos_descriptions[] =
{ N_("Center"), N_("Left"), N_("Right"), N_("Top"), N_("Bottom"),
  N_("Top-Left"), N_("Top-Right"), N_("Bottom-Left"), N_("Bottom-Right") };

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );
static int Decode( decoder_t *, block_t * );
static void Flush( decoder_t * );

#ifdef ENABLE_SOUT
static int OpenEncoder  ( vlc_object_t * );
static void CloseEncoder( vlc_object_t * );
static block_t *Encode  ( encoder_t *, subpicture_t * );
#endif

vlc_module_begin ()
#   define DVBSUB_CFG_PREFIX "dvbsub-"
    set_description( N_("DVB subtitles decoder") )
    set_shortname( N_("DVB subtitles") )
    set_capability( "spu decoder", 80 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )
    set_callbacks( Open, Close )

    add_integer( DVBSUB_CFG_PREFIX "position", 8, POS_TEXT, POS_LONGTEXT, true )
        change_integer_list( pi_pos_values, ppsz_pos_descriptions )
    add_integer( DVBSUB_CFG_PREFIX "x", -1, POSX_TEXT, POSX_LONGTEXT, false )
    add_integer( DVBSUB_CFG_PREFIX "y", -1, POSY_TEXT, POSY_LONGTEXT, false )

#ifdef ENABLE_SOUT
#   define ENC_CFG_PREFIX "sout-dvbsub-"
    add_submodule ()
    set_description( N_("DVB subtitles encoder") )
    set_capability( "encoder", 100 )
    set_callbacks( OpenEncoder, CloseEncoder )

    add_integer( ENC_CFG_PREFIX "x", -1, ENC_POSX_TEXT, ENC_POSX_LONGTEXT, false )
    add_integer( ENC_CFG_PREFIX "y", -1, ENC_POSY_TEXT, ENC_POSY_LONGTEXT, false )
#endif
vlc_module_end ()

static const char *const ppsz_enc_options[] = { "x", "y", NULL };

/****************************************************************************
 * Local structures
 ****************************************************************************
 * Those structures refer closely to the ETSI 300 743 Object model
 ****************************************************************************/

/* The object definition gives the position of the object in a region [7.2.5] */
typedef struct dvbsub_objectdef_s
{
    int i_id;
    int i_type;
    int i_x;
    int i_y;
    int i_fg_pc;
    int i_bg_pc;
    char *psz_text; /* for string of characters objects */

} dvbsub_objectdef_t;

/* The entry in the palette CLUT */
typedef struct
{
    uint8_t                 Y;
    uint8_t                 Cr;
    uint8_t                 Cb;
    uint8_t                 T;

} dvbsub_color_t;

/* The displays dimensions [7.2.1] */
typedef struct dvbsub_display_s
{
    uint8_t                 i_id;
    uint8_t                 i_version;

    int                     i_width;
    int                     i_height;

    bool                    b_windowed;
    /* these values are only relevant if windowed */
    int                     i_x;
    int                     i_y;
    int                     i_max_x;
    int                     i_max_y;

} dvbsub_display_t;

/* [7.2.4] */
typedef struct dvbsub_clut_s
{
    uint8_t                 i_id;
    uint8_t                 i_version;
    dvbsub_color_t          c_2b[4];
    dvbsub_color_t          c_4b[16];
    dvbsub_color_t          c_8b[256];

    struct dvbsub_clut_s    *p_next;

} dvbsub_clut_t;

/* The Region is an aera on the image [7.2.3]
 * with a list of the object definitions associated and a CLUT */
typedef struct dvbsub_region_s
{
    int i_id;
    int i_version;
    int i_x;
    int i_y;
    int i_width;
    int i_height;
    int i_level_comp;
    int i_depth;
    int i_clut;

    uint8_t *p_pixbuf;

    int                    i_object_defs;
    dvbsub_objectdef_t     *p_object_defs;

    struct dvbsub_region_s *p_next;

} dvbsub_region_t;

/* The object definition gives the position of the object in a region */
typedef struct dvbsub_regiondef_s
{
    int i_id;
    int i_x;
    int i_y;

} dvbsub_regiondef_t;

/* The page defines the list of regions [7.2.2] */
typedef struct
{
    int i_id;
    int i_timeout; /* in seconds */
    int i_state;
    int i_version;

    int                i_region_defs;
    dvbsub_regiondef_t *p_region_defs;

} dvbsub_page_t;

typedef struct
{
    bs_t               bs;

    /* Decoder internal data */
    int                i_id;
    int                i_ancillary_id;
    vlc_tick_t         i_pts;

    bool               b_absolute;
    int                i_spu_position;
    int                i_spu_x;
    int                i_spu_y;

    bool               b_page;
    dvbsub_page_t      *p_page;
    dvbsub_region_t    *p_regions;
    dvbsub_clut_t      *p_cluts;
    /* this is very small, so keep forever */
    dvbsub_display_t   display;
    dvbsub_clut_t      default_clut;
} decoder_sys_t;


/* List of different SEGMENT TYPES */
/* According to EN 300-743, table 2 */
#define DVBSUB_ST_PAGE_COMPOSITION      0x10
#define DVBSUB_ST_REGION_COMPOSITION    0x11
#define DVBSUB_ST_CLUT_DEFINITION       0x12
#define DVBSUB_ST_OBJECT_DATA           0x13
#define DVBSUB_ST_DISPLAY_DEFINITION    0x14
#define DVBSUB_ST_ENDOFDISPLAY          0x80
#define DVBSUB_ST_STUFFING              0xff
/* List of different OBJECT TYPES */
/* According to EN 300-743, table 6 */
#define DVBSUB_OT_BASIC_BITMAP          0x00
#define DVBSUB_OT_BASIC_CHAR            0x01
#define DVBSUB_OT_COMPOSITE_STRING      0x02
/* Pixel DATA TYPES */
/* According to EN 300-743, table 9 */
#define DVBSUB_DT_2BP_CODE_STRING       0x10
#define DVBSUB_DT_4BP_CODE_STRING       0x11
#define DVBSUB_DT_8BP_CODE_STRING       0x12
#define DVBSUB_DT_24_TABLE_DATA         0x20
#define DVBSUB_DT_28_TABLE_DATA         0x21
#define DVBSUB_DT_48_TABLE_DATA         0x22
#define DVBSUB_DT_END_LINE              0xf0
/* List of different Page Composition Segment state */
/* According to EN 300-743, 7.2.1 table 3 */
#define DVBSUB_PCS_STATE_ACQUISITION    0x01
#define DVBSUB_PCS_STATE_CHANGE         0x02

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void decode_segment( decoder_t *, bs_t * );
static void decode_page_composition( decoder_t *, bs_t *, uint16_t );
static void decode_region_composition( decoder_t *, bs_t *, uint16_t );
static void decode_object( decoder_t *, bs_t *, uint16_t );
static void decode_display_definition( decoder_t *, bs_t *, uint16_t );
static void decode_clut( decoder_t *, bs_t *, uint16_t );
static void free_all( decoder_t * );

static void default_clut_init( decoder_t * );
static void default_dds_init( decoder_t * );

static subpicture_t *render( decoder_t * );

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
    int i_posx, i_posy;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_DVBS )
    {
        return VLC_EGENERIC;
    }

    p_dec->pf_decode = Decode;
    p_dec->pf_flush  = Flush;
    p_sys = p_dec->p_sys = calloc( 1, sizeof(decoder_sys_t) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->i_pts          = VLC_TICK_INVALID;
    p_sys->i_id           = p_dec->fmt_in.subs.dvb.i_id & 0xFFFF;
    p_sys->i_ancillary_id = p_dec->fmt_in.subs.dvb.i_id >> 16;

    p_sys->p_regions      = NULL;
    p_sys->p_cluts        = NULL;
    p_sys->p_page         = NULL;

    /* configure for SD res in case DDS is not present */
    default_dds_init( p_dec );

    p_sys->i_spu_position = var_CreateGetInteger( p_this,
                                    DVBSUB_CFG_PREFIX "position" );
    i_posx = var_CreateGetInteger( p_this, DVBSUB_CFG_PREFIX "x" );
    i_posy = var_CreateGetInteger( p_this, DVBSUB_CFG_PREFIX "y" );

    /* Check if subpicture position was overridden */
    p_sys->b_absolute = true;
    p_sys->i_spu_x = p_sys->i_spu_y = 0;

    if( ( i_posx >= 0 ) && ( i_posy >= 0 ) )
    {
        p_sys->b_absolute = true;
        p_sys->i_spu_x = i_posx;
        p_sys->i_spu_y = i_posy;
    }

    p_dec->fmt_out.i_codec = 0;

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

    var_Destroy( p_this, DVBSUB_CFG_PREFIX "x" );
    var_Destroy( p_this, DVBSUB_CFG_PREFIX "y" );
    var_Destroy( p_this, DVBSUB_CFG_PREFIX "position" );

    free_all( p_dec );
    free( p_sys );
}

/*****************************************************************************
 * Flush:
 *****************************************************************************/
static void Flush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    p_sys->i_pts = VLC_TICK_INVALID;
}

/*****************************************************************************
 * Decode:
 *****************************************************************************/
static int Decode( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

    if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY | BLOCK_FLAG_CORRUPTED) )
    {
        Flush( p_dec );
        if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
        {
            block_Release( p_block );
            return VLCDEC_SUCCESS;
        }
    }

    /* configure for SD res in case DDS is not present */
    /* a change of PTS is a good indication we must get a new DDS */
    if( p_sys->i_pts != p_block->i_pts )
        default_dds_init( p_dec );

    p_sys->i_pts = p_block->i_pts;
    if( p_sys->i_pts == VLC_TICK_INVALID )
    {
#ifdef DEBUG_DVBSUB
        /* Some DVB channels send stuffing segments in non-dated packets so
         * don't complain too loudly. */
        msg_Warn( p_dec, "non dated subtitle" );
#endif
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    bs_init( &p_sys->bs, p_block->p_buffer, p_block->i_buffer );

    if( bs_read( &p_sys->bs, 8 ) != 0x20 ) /* Data identifier */
    {
        msg_Dbg( p_dec, "invalid data identifier" );
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    if( bs_read( &p_sys->bs, 8 ) ) /* Subtitle stream id */
    {
        msg_Dbg( p_dec, "invalid subtitle stream id" );
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

#ifdef DEBUG_DVBSUB
    msg_Dbg( p_dec, "subtitle packet received: %"PRId64, p_sys->i_pts );
#endif

    p_sys->b_page = false;

    uint8_t i_sync_byte = bs_read( &p_sys->bs, 8 );
    while( i_sync_byte == 0x0f ) /* Sync byte */
    {
        decode_segment( p_dec, &p_sys->bs );
        i_sync_byte = bs_read( &p_sys->bs, 8 );
    }

    if( ( i_sync_byte & 0x3f ) != 0x3f ) /* End marker */
    {
        msg_Warn( p_dec, "end marker not found (corrupted subtitle ?)" );
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    /* Check if the page is to be displayed */
    if( p_sys->p_page && p_sys->b_page )
    {
        subpicture_t *p_spu = render( p_dec );
        if( p_spu != NULL )
            decoder_QueueSub( p_dec, p_spu );
    }

    block_Release( p_block );

    return VLCDEC_SUCCESS;
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
        p_sys->default_clut.c_2b[i].Cb = RGB_TO_V(R,G,B);
        p_sys->default_clut.c_2b[i].Cr = RGB_TO_U(R,G,B);
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
        p_sys->default_clut.c_4b[i].Cr = RGB_TO_V(R,G,B);
        p_sys->default_clut.c_4b[i].Cb = RGB_TO_U(R,G,B);
        p_sys->default_clut.c_4b[i].T = T;
    }

    /* 256 entries CLUT */
    memset( p_sys->default_clut.c_8b, 0xFF, 256 * sizeof(dvbsub_color_t) );
}

static void decode_segment( decoder_t *p_dec, bs_t *s )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_type;
    int i_page_id;
    int i_size;

    /* sync_byte (already checked) */
    //bs_skip( s, 8 );

    /* segment type */
    i_type = bs_read( s, 8 );

    /* page id */
    i_page_id = bs_read( s, 16 );

    /* segment size */
    i_size = bs_read( s, 16 );

    if( ( i_page_id != p_sys->i_id ) &&
        ( i_page_id != p_sys->i_ancillary_id ) )
    {
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "subtitle skipped (page id: %i, %i)",
                 i_page_id, p_sys->i_id );
#endif
        bs_skip( s,  8 * i_size );
        return;
    }

    if( ( p_sys->i_ancillary_id != p_sys->i_id ) &&
        ( i_type == DVBSUB_ST_PAGE_COMPOSITION ) &&
        ( i_page_id == p_sys->i_ancillary_id ) )
    {
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "skipped invalid ancillary subtitle packet" );
#endif
        bs_skip( s,  8 * i_size );
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
        decode_page_composition( p_dec, s, i_size );
        break;

    case DVBSUB_ST_REGION_COMPOSITION:
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "decode_region_composition" );
#endif
        decode_region_composition( p_dec, s, i_size );
        break;

    case DVBSUB_ST_CLUT_DEFINITION:
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "decode_clut" );
#endif
        decode_clut( p_dec, s, i_size );
        break;

    case DVBSUB_ST_OBJECT_DATA:
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "decode_object" );
#endif
        decode_object( p_dec, s, i_size );
        break;

    case DVBSUB_ST_DISPLAY_DEFINITION:
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "decode_display_definition" );
#endif
        decode_display_definition( p_dec, s, i_size );
        break;

    case DVBSUB_ST_ENDOFDISPLAY:
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "end of display" );
#endif
        bs_skip( s,  8 * i_size );
        break;

    case DVBSUB_ST_STUFFING:
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "skip stuffing" );
#endif
        bs_skip( s,  8 * i_size );
        break;

    default:
        msg_Warn( p_dec, "unsupported segment type: (%04x)", i_type );
        bs_skip( s,  8 * i_size );
        break;
    }
}

static void decode_clut( decoder_t *p_dec, bs_t *s, uint16_t i_segment_length )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint16_t      i_processed_length;
    dvbsub_clut_t *p_clut, *p_next;
    int           i_id, i_version;

    i_id             = bs_read( s, 8 );
    i_version        = bs_read( s, 4 );

    /* Check if we already have this clut */
    for( p_clut = p_sys->p_cluts; p_clut != NULL; p_clut = p_clut->p_next )
    {
        if( p_clut->i_id == i_id ) break;
    }

    /* Check version number */
    if( p_clut && ( p_clut->i_version == i_version ) )
    {
        /* Nothing to do */
        bs_skip( s, 8 * i_segment_length - 12 );
        return;
    }

    if( !p_clut )
    {
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "new clut: %i", i_id );
#endif
        p_clut = malloc( sizeof( dvbsub_clut_t ) );
        if( !p_clut )
            return;
        p_clut->p_next = p_sys->p_cluts;
        p_sys->p_cluts = p_clut;
    }

    /* Initialize to default clut */
    p_next = p_clut->p_next;
    *p_clut = p_sys->default_clut;
    p_clut->p_next = p_next;

    /* We don't have this version of the CLUT: Parse it */
    p_clut->i_version = i_version;
    p_clut->i_id = i_id;
    bs_skip( s, 4 ); /* Reserved bits */
    i_processed_length = 2;
    while( i_processed_length < i_segment_length )
    {
        uint8_t y, cb, cr, t;
        uint_fast8_t cid = bs_read( s, 8 );
        uint_fast8_t type = bs_read( s, 3 );

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
        if( ( type & 0x04 ) && ( cid < 4 ) )
        {
            p_clut->c_2b[cid].Y = y;
            p_clut->c_2b[cid].Cr = cr;
            p_clut->c_2b[cid].Cb = cb;
            p_clut->c_2b[cid].T = t;
        }
        if( ( type & 0x02 ) && ( cid < 16 ) )
        {
            p_clut->c_4b[cid].Y = y;
            p_clut->c_4b[cid].Cr = cr;
            p_clut->c_4b[cid].Cb = cb;
            p_clut->c_4b[cid].T = t;
        }
        if( type & 0x01 )
        {
            p_clut->c_8b[cid].Y = y;
            p_clut->c_8b[cid].Cr = cr;
            p_clut->c_8b[cid].Cb = cb;
            p_clut->c_8b[cid].T = t;
        }
    }
}

static void decode_page_composition( decoder_t *p_dec, bs_t *s, uint16_t i_segment_length )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_version, i_state, i_timeout, i;

    /* A page is composed by 0 or more region */
    i_timeout = bs_read( s, 8 );
    i_version = bs_read( s, 4 );
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
    else if( !p_sys->p_page && ( i_state != DVBSUB_PCS_STATE_ACQUISITION ) &&
             ( i_state != DVBSUB_PCS_STATE_CHANGE ) )
    {
        /* Not a full PCS, we need to wait for one */
        msg_Dbg( p_dec, "didn't receive an acquisition page yet" );

#if 0
        /* Try to start decoding even without an acquisition page */
        bs_skip( s,  8 * (i_segment_length - 2) );
        return;
#endif
    }

#ifdef DEBUG_DVBSUB
    if( i_state == DVBSUB_PCS_STATE_ACQUISITION )
        msg_Dbg( p_dec, "acquisition page composition" );
#endif

    /* Check version number */
    if( p_sys->p_page && ( p_sys->p_page->i_version == i_version ) )
    {
        bs_skip( s,  8 * (i_segment_length - 2) );
        return;
    }
    else if( p_sys->p_page )
    {
        if( p_sys->p_page->i_region_defs )
            free( p_sys->p_page->p_region_defs );
        p_sys->p_page->p_region_defs = NULL;
        p_sys->p_page->i_region_defs = 0;
    }

    if( !p_sys->p_page )
    {
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "new page" );
#endif
        /* Allocate a new page */
        p_sys->p_page = malloc( sizeof(dvbsub_page_t) );
        if( !p_sys->p_page )
            return;
    }

    p_sys->p_page->i_version = i_version;
    p_sys->p_page->i_timeout = i_timeout;
    p_sys->b_page = true;

    /* Number of regions */
    p_sys->p_page->i_region_defs = (i_segment_length - 2) / 6;

    if( p_sys->p_page->i_region_defs == 0 ) return;

    p_sys->p_page->p_region_defs =
        vlc_alloc( p_sys->p_page->i_region_defs, sizeof(dvbsub_regiondef_t) );
    if( p_sys->p_page->p_region_defs )
    {
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
}

static void decode_region_composition( decoder_t *p_dec, bs_t *s, uint16_t i_segment_length )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    dvbsub_region_t *p_region, **pp_region = &p_sys->p_regions;
    int i_processed_length, i_id, i_version;
    int i_width, i_height, i_level_comp, i_depth, i_clut;
    int i_8_bg, i_4_bg, i_2_bg;
    bool b_fill;

    i_id = bs_read( s, 8 );
    i_version = bs_read( s, 4 );

    /* Check if we already have this region */
    for( p_region = p_sys->p_regions; p_region != NULL;
         p_region = p_region->p_next )
    {
        pp_region = &p_region->p_next;
        if( p_region->i_id == i_id ) break;
    }

    /* Check version number */
    if( p_region && ( p_region->i_version == i_version ) )
    {
        bs_skip( s, 8 * (i_segment_length - 1) - 4 );
        return;
    }

    if( !p_region )
    {
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "new region: %i", i_id );
#endif
        p_region = *pp_region = calloc( 1, sizeof(dvbsub_region_t) );
        if( !p_region )
            return;
        p_region->p_object_defs = NULL;
        p_region->p_pixbuf = NULL;
        p_region->p_next = NULL;
    }

    /* Region attributes */
    p_region->i_id = i_id;
    p_region->i_version = i_version;
    b_fill = bs_read( s, 1 );
    bs_skip( s, 3 ); /* Reserved */

    i_width = bs_read( s, 16 );
    i_height = bs_read( s, 16 );
#ifdef DEBUG_DVBSUB
    msg_Dbg( p_dec, " width=%d height=%d", i_width, i_height );
#endif
    i_level_comp = bs_read( s, 3 );
    i_depth = bs_read( s, 3 );
    bs_skip( s, 2 ); /* Reserved */
    i_clut = bs_read( s, 8 );

    i_8_bg = bs_read( s, 8 );
    i_4_bg = bs_read( s, 4 );
    i_2_bg = bs_read( s, 2 );
    bs_skip( s, 2 ); /* Reserved */

    /* Free old object defs */
    while( p_region->i_object_defs )
        free( p_region->p_object_defs[--p_region->i_object_defs].psz_text );

    free( p_region->p_object_defs );
    p_region->p_object_defs = NULL;

    /* Extra sanity checks */
    if( ( p_region->i_width != i_width ) ||
        ( p_region->i_height != i_height ) )
    {
        if( p_region->p_pixbuf )
        {
            msg_Dbg( p_dec, "region size changed (%dx%d->%dx%d)",
                     p_region->i_width, p_region->i_height, i_width, i_height );
            free( p_region->p_pixbuf );
        }

        p_region->p_pixbuf = xmalloc( i_height * i_width );
        p_region->i_depth = 0;
        b_fill = true;
    }
    if( p_region->i_depth &&
        ( ( p_region->i_depth != i_depth ) ||
          ( p_region->i_level_comp != i_level_comp ) ||
          ( p_region->i_clut != i_clut) ) )
    {
        msg_Dbg( p_dec, "region parameters changed (not allowed)" );
    }

    /* Erase background of region */
    if( b_fill )
    {
        int i_background = ( i_depth == 1 ) ? i_2_bg :
            ( ( i_depth == 2 ) ? i_4_bg : i_8_bg );
        memset( p_region->p_pixbuf, i_background, i_width * i_height );
    }

    p_region->i_width = i_width;
    p_region->i_height = i_height;
    p_region->i_level_comp = i_level_comp;
    p_region->i_depth = i_depth;
    p_region->i_clut = i_clut;

    /* List of objects in the region */
    i_processed_length = 10;
    while( i_processed_length < i_segment_length )
    {
        dvbsub_objectdef_t *p_obj;

        /* We create a new object */
        p_region->i_object_defs++;
        p_region->p_object_defs = xrealloc( p_region->p_object_defs,
                     sizeof(dvbsub_objectdef_t) * p_region->i_object_defs );

        /* We parse object properties */
        p_obj = &p_region->p_object_defs[p_region->i_object_defs - 1];
        p_obj->i_id         = bs_read( s, 16 );
        p_obj->i_type       = bs_read( s, 2 );
        bs_skip( s, 2 ); /* Provider */
        p_obj->i_x          = bs_read( s, 12 );
        bs_skip( s, 4 ); /* Reserved */
        p_obj->i_y          = bs_read( s, 12 );
        p_obj->psz_text     = NULL;

        i_processed_length += 6;

        if( ( p_obj->i_type == DVBSUB_OT_BASIC_CHAR ) ||
            ( p_obj->i_type == DVBSUB_OT_COMPOSITE_STRING ) )
        {
            p_obj->i_fg_pc =  bs_read( s, 8 );
            p_obj->i_bg_pc =  bs_read( s, 8 );
            i_processed_length += 2;
        }
    }
}

/* ETSI 300 743 [7.2.1] */
static void decode_display_definition( decoder_t *p_dec, bs_t *s, uint16_t i_segment_length )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint16_t      i_processed_length = 40;
    int           i_version;

    i_version        = bs_read( s, 4 );

    /* Check version number */
    if( p_sys->display.i_version == i_version )
    {
        /* The definition did not change */
        bs_skip( s, 8*i_segment_length - 4 );
        return;
    }

#ifdef DEBUG_DVBSUB
    msg_Dbg( p_dec, "new display definition: %i", i_version );
#endif

    /* We don't have this version of the display definition: Parse it */
    p_sys->display.i_version = i_version;
    p_sys->display.b_windowed = bs_read( s, 1 );
    bs_skip( s, 3 ); /* Reserved bits */
    p_sys->display.i_width = bs_read( s, 16 )+1;
    p_sys->display.i_height = bs_read( s, 16 )+1;

    if( p_sys->display.b_windowed )
    {
#ifdef DEBUG_DVBSUB
        msg_Dbg( p_dec, "display definition with offsets (windowed)" );
#endif
        /* Coordinates are measured from the top left corner */
        p_sys->display.i_x     = bs_read( s, 16 );
        p_sys->display.i_max_x = bs_read( s, 16 );
        p_sys->display.i_y     = bs_read( s, 16 );
        p_sys->display.i_max_y = bs_read( s, 16 );
        i_processed_length += 64;
    }
    else
    {
        /* if not windowed, setup the window variables to good defaults */
        /* not necessary, but to avoid future confusion.. */
        p_sys->display.i_x     = 0;
        p_sys->display.i_max_x = p_sys->display.i_width-1;
        p_sys->display.i_y     = 0;
        p_sys->display.i_max_y = p_sys->display.i_height-1;
    }

    if( i_processed_length != i_segment_length*8 )
    {
        msg_Err( p_dec, "processed length %d bytes != segment length %d bytes",
                 i_processed_length / 8 , i_segment_length );
    }

#ifdef DEBUG_DVBSUB
    msg_Dbg( p_dec, "version: %d, width: %d, height: %d",
             p_sys->display.i_version, p_sys->display.i_width, p_sys->display.i_height );
    if( p_sys->display.b_windowed )
        msg_Dbg( p_dec, "xmin: %d, xmax: %d, ymin: %d, ymax: %d",
                 p_sys->display.i_x, p_sys->display.i_max_x, p_sys->display.i_y, p_sys->display.i_max_y );
#endif
}

static void dvbsub_render_pdata( decoder_t *, dvbsub_region_t *, int, int,
                                 uint8_t *, int );
static void dvbsub_pdata2bpp( bs_t *, uint8_t *, int, int * );
static void dvbsub_pdata4bpp( bs_t *, uint8_t *, int, int * );
static void dvbsub_pdata8bpp( bs_t *, uint8_t *, int, int * );

static void decode_object( decoder_t *p_dec, bs_t *s, uint16_t i_segment_length )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    dvbsub_region_t *p_region;
    int i_coding_method, i_id, i;

    /* ETSI 300-743 paragraph 7.2.4
     * sync_byte, segment_type and page_id have already been processed.
     */
    i_id             = bs_read( s, 16 );
    bs_skip( s, 4 ); /* version */
    i_coding_method  = bs_read( s, 2 );

    if( i_coding_method > 1 )
    {
        msg_Dbg( p_dec, "unknown DVB subtitling coding %d is not handled!", i_coding_method );
        bs_skip( s, 8 * (i_segment_length - 2) - 6 );
        return;
    }

    /* Check if the object needs to be rendered in at least one
     * of the regions */
    for( p_region = p_sys->p_regions; p_region != NULL;
         p_region = p_region->p_next )
    {
        for( i = 0; i < p_region->i_object_defs; i++ )
            if( p_region->p_object_defs[i].i_id == i_id ) break;

        if( i != p_region->i_object_defs ) break;
    }
    if( !p_region )
    {
        bs_skip( s, 8 * (i_segment_length - 2) - 6 );
        return;
    }

#ifdef DEBUG_DVBSUB
    msg_Dbg( p_dec, "new object: %i", i_id );
#endif

    bs_skip( s, 1 ); /* non_modify_color */
    bs_skip( s, 1 ); /* Reserved */

    if( i_coding_method == 0x00 )
    {
        int i_topfield, i_bottomfield;
        uint8_t *p_topfield, *p_bottomfield;

        i_topfield    = bs_read( s, 16 );
        i_bottomfield = bs_read( s, 16 );
        p_topfield    = s->p_start + bs_pos( s ) / 8;
        p_bottomfield = p_topfield + i_topfield;

        bs_skip( s, 8 * (i_segment_length - 7) );

        /* Sanity check */
        if( ( i_segment_length < ( i_topfield + i_bottomfield + 7 ) ) ||
            ( ( p_topfield + i_topfield + i_bottomfield ) > s->p_end ) )
        {
            msg_Dbg( p_dec, "corrupted object data" );
            return;
        }

        for( p_region = p_sys->p_regions; p_region != NULL;
             p_region = p_region->p_next )
        {
            for( i = 0; i < p_region->i_object_defs; i++ )
            {
                if( p_region->p_object_defs[i].i_id != i_id ) continue;

                dvbsub_render_pdata( p_dec, p_region,
                                     p_region->p_object_defs[i].i_x,
                                     p_region->p_object_defs[i].i_y,
                                     p_topfield, i_topfield );

                if( i_bottomfield )
                {
                    dvbsub_render_pdata( p_dec, p_region,
                                         p_region->p_object_defs[i].i_x,
                                         p_region->p_object_defs[i].i_y + 1,
                                         p_bottomfield, i_bottomfield );
                }
                else
                {
                    /* Duplicate the top field */
                    dvbsub_render_pdata( p_dec, p_region,
                                         p_region->p_object_defs[i].i_x,
                                         p_region->p_object_defs[i].i_y + 1,
                                         p_topfield, i_topfield );
                }
            }
        }
    }
    else
    {
        /* DVB subtitling as characters */
        int i_number_of_codes = bs_read( s, 8 );
        uint8_t* p_start = s->p_start + bs_pos( s ) / 8;

        /* Sanity check */
        if( ( i_segment_length < ( i_number_of_codes*2 + 4 ) ) ||
            ( ( p_start + i_number_of_codes*2 ) > s->p_end ) )
        {
            msg_Dbg( p_dec, "corrupted object data" );
            return;
        }

        for( p_region = p_sys->p_regions; p_region != NULL;
             p_region = p_region->p_next )
        {
            for( i = 0; i < p_region->i_object_defs; i++ )
            {
                int j;

                if( p_region->p_object_defs[i].i_id != i_id ) continue;

                p_region->p_object_defs[i].psz_text =
                    xrealloc( p_region->p_object_defs[i].psz_text,
                             i_number_of_codes + 1 );

                /* FIXME 16bits -> char ??? See Preamble */
                for( j = 0; j < i_number_of_codes; j++ )
                {
                    p_region->p_object_defs[i].psz_text[j] = (char)(bs_read( s, 16 ) & 0xFF);
                }
                /* Null terminate the string */
                p_region->p_object_defs[i].psz_text[j] = 0;
            }
        }
    }

#ifdef DEBUG_DVBSUB
    msg_Dbg( p_dec, "end object: %i", i_id );
#endif
}

static void dvbsub_render_pdata( decoder_t *p_dec, dvbsub_region_t *p_region,
                                 int i_x, int i_y,
                                 uint8_t *p_field, int i_field )
{
    uint8_t *p_pixbuf;
    int i_offset = 0;
    bs_t bs;

    /* Sanity check */
    if( !p_region->p_pixbuf )
    {
        msg_Err( p_dec, "region %i has no pixel buffer!", p_region->i_id );
        return;
    }
    if( i_y < 0 || i_x < 0 || i_y >= p_region->i_height ||
        i_x >= p_region->i_width )
    {
        msg_Dbg( p_dec, "invalid offset (%i,%i)", i_x, i_y );
        return;
    }

    p_pixbuf = p_region->p_pixbuf + i_y * p_region->i_width;
    bs_init( &bs, p_field, i_field );

    while( !bs_eof( &bs ) )
    {
        /* Sanity check */
        if( i_y >= p_region->i_height ) return;

        switch( bs_read( &bs, 8 ) )
        {
        case 0x10:
            dvbsub_pdata2bpp( &bs, p_pixbuf + i_x, p_region->i_width - i_x,
                              &i_offset );
            break;

        case 0x11:
            dvbsub_pdata4bpp( &bs, p_pixbuf + i_x, p_region->i_width - i_x,
                              &i_offset );
            break;

        case 0x12:
            dvbsub_pdata8bpp( &bs, p_pixbuf + i_x, p_region->i_width - i_x,
                              &i_offset );
            break;

        case 0x20:
        case 0x21:
        case 0x22:
            /* We don't use map tables */
            break;

        case 0xf0: /* End of line code */
            p_pixbuf += 2*p_region->i_width;
            i_offset = 0; i_y += 2;
            break;
        }
    }
}

static void dvbsub_pdata2bpp( bs_t *s, uint8_t *p, int i_width, int *pi_off )
{
    bool b_stop = false;

    while( !b_stop && !bs_eof( s ) )
    {
        int i_count = 0, i_color = 0;

        i_color = bs_read( s, 2 );
        if( i_color != 0x00 )
        {
            i_count = 1;
        }
        else
        {
            if( bs_read( s, 1 ) == 0x01 )         // Switch1
            {
                i_count = 3 + bs_read( s, 3 );
                i_color = bs_read( s, 2 );
            }
            else
            {
                if( bs_read( s, 1 ) == 0x00 )     //Switch2
                {
                    switch( bs_read( s, 2 ) )     //Switch3
                    {
                    case 0x00:
                        b_stop = true;
                        break;
                    case 0x01:
                        i_count = 2;
                        break;
                    case 0x02:
                        i_count =  12 + bs_read( s, 4 );
                        i_color = bs_read( s, 2 );
                        break;
                    case 0x03:
                        i_count =  29 + bs_read( s, 8 );
                        i_color = bs_read( s, 2 );
                        break;
                    default:
                        break;
                    }
                }
                else
                {
                    /* 1 pixel color 0 */
                    i_count = 1;
                }
            }
        }

        if( !i_count ) continue;

        /* Sanity check */
        if( ( i_count + *pi_off ) > i_width ) break;

        if( i_count == 1 ) p[*pi_off] = i_color;
        else memset( ( p + *pi_off ), i_color, i_count );

        (*pi_off) += i_count;
    }

    bs_align( s );
}

static void dvbsub_pdata4bpp( bs_t *s, uint8_t *p, int i_width, int *pi_off )
{
    bool b_stop = false;

    while( !b_stop && !bs_eof( s ) )
    {
        int i_count = 0, i_color = 0;

        i_color = bs_read( s, 4 );
        if( i_color != 0x00 )
        {
            /* Add 1 pixel */
            i_count = 1;
        }
        else
        {
            if( bs_read( s, 1 ) == 0x00 )           // Switch1
            {
                i_count = bs_read( s, 3 );
                if( i_count != 0x00 )
                {
                    i_count += 2;
                }
                else b_stop = true;
            }
            else
            {
                if( bs_read( s, 1 ) == 0x00)        //Switch2
                {
                    i_count =  4 + bs_read( s, 2 );
                    i_color = bs_read( s, 4 );
                }
                else
                {
                    switch ( bs_read( s, 2 ) )     //Switch3
                    {
                    case 0x0:
                        i_count = 1;
                        break;
                    case 0x1:
                        i_count = 2;
                        break;
                    case 0x2:
                        i_count = 9 + bs_read( s, 4 );
                        i_color = bs_read( s, 4 );
                        break;
                    case 0x3:
                        i_count= 25 + bs_read( s, 8 );
                        i_color = bs_read( s, 4 );
                        break;
                    }
                }
            }
        }

        if( !i_count ) continue;

        /* Sanity check */
        if( ( i_count + *pi_off ) > i_width ) break;

        if( i_count == 1 ) p[*pi_off] = i_color;
        else memset( ( p + *pi_off ), i_color, i_count );

        (*pi_off) += i_count;
    }

    bs_align( s );
}

static void dvbsub_pdata8bpp( bs_t *s, uint8_t *p, int i_width, int *pi_off )
{
    bool b_stop = false;

    while( !b_stop && !bs_eof( s ) )
    {
        int i_count = 0, i_color = 0;

        i_color = bs_read( s, 8 );
        if( i_color != 0x00 )
        {
            /* Add 1 pixel */
            i_count = 1;
        }
        else
        {
            if( bs_read( s, 1 ) == 0x00 )           // Switch1
            {
                i_count = bs_read( s, 7 );
                if( i_count == 0x00 )
                    b_stop = true;
            }
            else
            {
                i_count = bs_read( s, 7 );
                i_color = bs_read( s, 8 );
            }
        }

        if( !i_count ) continue;

        /* Sanity check */
        if( ( i_count + *pi_off ) > i_width ) break;

        if( i_count == 1 ) p[*pi_off] = i_color;
        else memset( ( p + *pi_off ), i_color, i_count );

        (*pi_off) += i_count;
    }

    bs_align( s );
}

static void free_all( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    dvbsub_region_t *p_reg, *p_reg_next;
    dvbsub_clut_t *p_clut, *p_clut_next;

    /*free( p_sys->p_display ); No longer malloced */

    for( p_clut = p_sys->p_cluts; p_clut != NULL; p_clut = p_clut_next )
    {
        p_clut_next = p_clut->p_next;
        free( p_clut );
    }
    p_sys->p_cluts = NULL;

    for( p_reg = p_sys->p_regions; p_reg != NULL; p_reg = p_reg_next )
    {
        p_reg_next = p_reg->p_next;
        for( int i = 0; i < p_reg->i_object_defs; i++ )
            free( p_reg->p_object_defs[i].psz_text );
        if( p_reg->i_object_defs ) free( p_reg->p_object_defs );
        free( p_reg->p_pixbuf );
        free( p_reg );
    }
    p_sys->p_regions = NULL;

    if( p_sys->p_page )
    {
        if( p_sys->p_page->i_region_defs )
            free( p_sys->p_page->p_region_defs );
        free( p_sys->p_page );
    }
    p_sys->p_page = NULL;
}

static subpicture_t *render( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    subpicture_t *p_spu;
    subpicture_region_t **pp_spu_region;
    int i, j;
    int i_base_x;
    int i_base_y;

    /* Allocate the subpicture internal data. */
    p_spu = decoder_NewSubpicture( p_dec, NULL );
    if( !p_spu )
        return NULL;

    p_spu->b_absolute = p_sys->b_absolute;
    /* Set the pf_render callback */
    p_spu->i_start = p_sys->i_pts;
    //p_spu->i_stop = (vlc_tick_t) 0;
    p_spu->b_ephemer = true;
    //p_spu->b_fade = true;
    //p_spu->i_stop = p_spu->i_start + (vlc_tick_t) (i_timeout * 1000000);
    p_spu->b_subtitle = true;

    /* Correct positioning of SPU */
    i_base_x = p_sys->i_spu_x;
    i_base_y = p_sys->i_spu_y;

    p_spu->i_original_picture_width = p_sys->display.i_width;
    p_spu->i_original_picture_height = p_sys->display.i_height;

    if( p_sys->display.b_windowed )
    {
        /* From en_300743v01 - */
        /* the DDS is there to indicate intended size/position of text */
        /* the intended video area is ->i_width/height */
        /* the window is within this... SO... we should leave i_original_picture_width etc. as is */
        /* and ONLY change i_base_x.  effectively i_max_x/y are only there to limit memory requirements*/
        /* we COULD use the upper limits to limit rendering to within these? */

        /* see notes on DDS at the top of the file */
        i_base_x += p_sys->display.i_x;
        i_base_y += p_sys->display.i_y;
    }

    pp_spu_region = &p_spu->p_region;

    /* Loop on region definitions */
#ifdef DEBUG_DVBSUB
    if( p_sys->p_page )
        msg_Dbg( p_dec, "rendering %i regions", p_sys->p_page->i_region_defs );
#endif

    for( i = 0; p_sys->p_page && ( i < p_sys->p_page->i_region_defs ); i++ )
    {
        dvbsub_region_t     *p_region;
        dvbsub_regiondef_t  *p_regiondef;
        dvbsub_clut_t       *p_clut;
        dvbsub_color_t      *p_color;
        subpicture_region_t *p_spu_region;
        uint8_t *p_src, *p_dst;
        video_format_t fmt;
        video_palette_t palette;
        int i_pitch;

        p_regiondef = &p_sys->p_page->p_region_defs[i];

        /* Find associated region */
        for( p_region = p_sys->p_regions; p_region != NULL;
             p_region = p_region->p_next )
        {
            if( p_regiondef->i_id == p_region->i_id ) break;
        }

#ifdef DEBUG_DVBSUB
        /* if a region exists, then print it's size */
        if (p_region)
        {
                msg_Dbg( p_dec, "rendering region %i (%i,%i) to (%i,%i)", i,
                        p_regiondef->i_x, p_regiondef->i_y,
                p_regiondef->i_x + p_region->i_width,
                p_regiondef->i_y + p_region->i_height );
        }
        else
        {
                msg_Dbg( p_dec, "rendering region %i (%i,%i) (no region matched to render)", i,
                      p_regiondef->i_x, p_regiondef->i_y );
        }
#endif

        if( !p_region )
        {
            msg_Dbg( p_dec, "region %i not found", p_regiondef->i_id );
            continue;
        }

        /* Find associated CLUT */
        for( p_clut = p_sys->p_cluts; p_clut != NULL; p_clut = p_clut->p_next )
        {
            if( p_region->i_clut == p_clut->i_id ) break;
        }
        if( !p_clut )
        {
            msg_Dbg( p_dec, "clut %i not found", p_region->i_clut );
            continue;
        }

        /* FIXME: don't create a subpicture region with VLC CODEC YUVP
         * when it actually is a TEXT region */

        /* Create new SPU region */
        video_format_Init( &fmt, VLC_CODEC_YUVP );
        fmt.i_sar_num = 0; /* 0 means use aspect ratio of background video */
        fmt.i_sar_den = 1;
        fmt.i_width = fmt.i_visible_width = p_region->i_width;
        fmt.i_height = fmt.i_visible_height = p_region->i_height;
        fmt.i_x_offset = fmt.i_y_offset = 0;
        fmt.p_palette = &palette;
        fmt.p_palette->i_entries = ( p_region->i_depth == 1 ) ? 4 :
            ( ( p_region->i_depth == 2 ) ? 16 : 256 );
        p_color = ( p_region->i_depth == 1 ) ? p_clut->c_2b :
            ( ( p_region->i_depth == 2 ) ? p_clut->c_4b : p_clut->c_8b );
        for( j = 0; j < fmt.p_palette->i_entries; j++ )
        {
            fmt.p_palette->palette[j][0] = p_color[j].Y;
            fmt.p_palette->palette[j][1] = p_color[j].Cb; /* U == Cb */
            fmt.p_palette->palette[j][2] = p_color[j].Cr; /* V == Cr */
            fmt.p_palette->palette[j][3] = 0xff - p_color[j].T;
        }

        p_spu_region = subpicture_region_New( &fmt );
        fmt.p_palette = NULL; /* was stack var */
        video_format_Clean( &fmt );
        if( !p_spu_region )
        {
            msg_Err( p_dec, "cannot allocate SPU region" );
            continue;
        }
        p_spu_region->i_x = i_base_x + p_regiondef->i_x;
        p_spu_region->i_y = i_base_y + p_regiondef->i_y;
        p_spu_region->i_align = p_sys->i_spu_position;
        *pp_spu_region = p_spu_region;
        pp_spu_region = &p_spu_region->p_next;

        p_src = p_region->p_pixbuf;
        p_dst = p_spu_region->p_picture->Y_PIXELS;
        i_pitch = p_spu_region->p_picture->Y_PITCH;

        /* Copy pixel buffer */
        for( j = 0; j < p_region->i_height; j++ )
        {
            memcpy( p_dst, p_src, p_region->i_width );
            p_src += p_region->i_width;
            p_dst += i_pitch;
        }

        /* Check subtitles encoded as strings of characters
         * (since there are not rendered in the pixbuffer) */
        for( j = 0; j < p_region->i_object_defs; j++ )
        {
            dvbsub_objectdef_t *p_object_def = &p_region->p_object_defs[j];

            if( ( p_object_def->i_type != 1 ) || !p_object_def->psz_text )
                continue;

            /* Create new SPU region */
            video_format_Init( &fmt, VLC_CODEC_TEXT );
            fmt.i_sar_num = 1;
            fmt.i_sar_den = 1;
            fmt.i_width = fmt.i_visible_width = p_region->i_width;
            fmt.i_height = fmt.i_visible_height = p_region->i_height;
            fmt.i_x_offset = fmt.i_y_offset = 0;
            p_spu_region = subpicture_region_New( &fmt );
            video_format_Clean( &fmt );

            p_spu_region->p_text = text_segment_New( p_object_def->psz_text );
            p_spu_region->i_x = i_base_x + p_regiondef->i_x + p_object_def->i_x;
            p_spu_region->i_y = i_base_y + p_regiondef->i_y + p_object_def->i_y;
            p_spu_region->i_align = p_sys->i_spu_position;
            *pp_spu_region = p_spu_region;
            pp_spu_region = &p_spu_region->p_next;
        }
    }

    return p_spu;
}

/*****************************************************************************
 * encoder_sys_t : encoder descriptor
 *****************************************************************************/
typedef struct encoder_region_t
{
    int i_width;
    int i_height;

} encoder_region_t;

typedef struct
{
    unsigned int i_page_ver;
    unsigned int i_region_ver;
    unsigned int i_clut_ver;

    int i_regions;
    encoder_region_t *p_regions;

    vlc_tick_t i_pts;

    /* subpicture positioning */
    int i_offset_x;
    int i_offset_y;
} encoder_sys_t;

#ifdef ENABLE_SOUT
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

    if( ( p_enc->fmt_out.i_codec != VLC_CODEC_DVBS ) &&
        !p_enc->obj.force )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_sys = (encoder_sys_t *)malloc(sizeof(encoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;
    p_enc->p_sys = p_sys;

    p_enc->pf_encode_sub = Encode;
    p_enc->fmt_out.i_codec = VLC_CODEC_DVBS;
    p_enc->fmt_out.subs.dvb.i_id  = 1 << 16 | 1;

    config_ChainParse( p_enc, ENC_CFG_PREFIX, ppsz_enc_options, p_enc->p_cfg );

    p_sys->i_page_ver = 0;
    p_sys->i_region_ver = 0;
    p_sys->i_clut_ver = 0;
    p_sys->i_regions = 0;
    p_sys->p_regions = 0;

    p_sys->i_offset_x = var_CreateGetInteger( p_this, ENC_CFG_PREFIX "x" );
    p_sys->i_offset_y = var_CreateGetInteger( p_this, ENC_CFG_PREFIX "y" );

    return VLC_SUCCESS;
}

/* FIXME: this routine is a hack to convert VLC_CODEC_YUVA
 *        into VLC_CODEC_YUVP
 */
static subpicture_t *YuvaYuvp( subpicture_t *p_subpic )
{
    subpicture_region_t *p_region = NULL;

    if( !p_subpic ) return NULL;

    for( p_region = p_subpic->p_region; p_region; p_region = p_region->p_next )
    {
        video_format_t *p_fmt = &p_region->fmt;
        int i = 0, j = 0, n = 0, p = 0;
        int i_max_entries = 256;

#ifdef RANDOM_DITHERING
        int i_seed = 0xdeadbeef; /* random seed */
#else
        int *pi_delta;
#endif
        int i_pixels = p_region->p_picture->p[0].i_visible_lines
                        * p_region->p_picture->p[0].i_pitch;
        int i_iterator = p_region->p_picture->p[0].i_visible_lines * 3 / 4
                            * p_region->p_picture->p[0].i_pitch
                        + p_region->p_picture->p[0].i_pitch * 1 / 3;
        int i_tolerance = 0;

#ifdef DEBUG_DVBSUB1
        /* p_enc not valid here */
        msg_Dbg( p_enc, "YuvaYuvp: i_pixels=%d, i_iterator=%d", i_pixels, i_iterator );
#endif
        p_fmt->i_chroma = VLC_CODEC_YUVP;
        p_fmt->p_palette = (video_palette_t *) malloc( sizeof( video_palette_t ) );
        if( !p_fmt->p_palette ) break;
        p_fmt->p_palette->i_entries = 0;

        /* Find best iterator using Euclide’s algorithm */
        for( ; i_iterator > 1 ; i_iterator-- )
        {
            int a = i_pixels;
            int b = i_iterator;
            int c;

            while( b )
            {
                c = a % b;
                a = b;
                b = c;
            }

            if( a == 1 )
            {
                break;
            }
        }

        /* Count colors, build best palette */
        for( i_tolerance = 0; i_tolerance < 128; i_tolerance++ )
        {
            bool b_success = true;
            p_fmt->p_palette->i_entries = 0;

            for( i = 0; i < i_pixels ; )
            {
                uint8_t y, u, v, a;
                y = p_region->p_picture->p[0].p_pixels[i];
                u = p_region->p_picture->p[1].p_pixels[i];
                v = p_region->p_picture->p[2].p_pixels[i];
                a = p_region->p_picture->p[3].p_pixels[i];
                for( j = 0; j < p_fmt->p_palette->i_entries; j++ )
                {
                    if( abs((int)p_fmt->p_palette->palette[j][0] - (int)y) <= i_tolerance &&
                        abs((int)p_fmt->p_palette->palette[j][1] - (int)u) <= i_tolerance &&
                        abs((int)p_fmt->p_palette->palette[j][2] - (int)v) <= i_tolerance &&
                        abs((int)p_fmt->p_palette->palette[j][3] - (int)a) <= i_tolerance / 2 )
                    {
                        break;
                    }
                }
                if( j == p_fmt->p_palette->i_entries )
                {
                    p_fmt->p_palette->palette[j][0] = y;
                    p_fmt->p_palette->palette[j][1] = u;
                    p_fmt->p_palette->palette[j][2] = v;
                    p_fmt->p_palette->palette[j][3] = a;
                    p_fmt->p_palette->i_entries++;
                }
                if( p_fmt->p_palette->i_entries >= i_max_entries )
                {
                    b_success = false;
                    break;
                }
                i += i_iterator;
                if( i > i_pixels )
                {
                    i -= i_pixels;
                }
            }

            if( b_success )
            {
                break;
            }
        }

#ifdef DEBUG_DVBSUB1
        /* p_enc not valid here */
        msg_Dbg( p_enc, "best palette has %d colors", p_fmt->p_palette->i_entries );
#endif

#ifndef RANDOM_DITHERING
        pi_delta = xmalloc( ( p_region->p_picture->p[0].i_pitch + 1 )
                            * sizeof(int) * 4  );
        for( i = 0; i < (p_region->p_picture->p[0].i_pitch + 1) * 4 ; i++ )
        {
            pi_delta[ i ] = 0;
        }
#endif

        /* Fill image with our new colours */
        for( p = 0; p < p_region->p_picture->p[0].i_visible_lines ; p++ )
        {
            int i_ydelta = 0, i_udelta = 0, i_vdelta = 0, i_adelta = 0;

            for( n = 0; n < p_region->p_picture->p[0].i_pitch ; n++ )
            {
                int i_offset = p * p_region->p_picture->p[0].i_pitch + n;
                int y, u, v, a;
                int i_mindist, i_best;

                y = (int)p_region->p_picture->p[0].p_pixels[i_offset];
                u = (int)p_region->p_picture->p[1].p_pixels[i_offset];
                v = (int)p_region->p_picture->p[2].p_pixels[i_offset];
                a = (int)p_region->p_picture->p[3].p_pixels[i_offset];

                /* Add dithering compensation */
#ifdef RANDOM_DITHERING
                y += ((i_seed & 0xff) - 0x80) * i_tolerance / 0x80;
                u += (((i_seed >> 8) & 0xff) - 0x80) * i_tolerance / 0x80;
                v += (((i_seed >> 16) & 0xff) - 0x80) * i_tolerance / 0x80;
                a += (((i_seed >> 24) & 0xff) - 0x80) * i_tolerance / 0x80;
#else
                y += i_ydelta + pi_delta[ n * 4 ];
                u += i_udelta + pi_delta[ n * 4 + 1 ];
                v += i_vdelta + pi_delta[ n * 4 + 2 ];
                a += i_adelta + pi_delta[ n * 4 + 3 ];
#endif

                /* Find best colour in palette */
                for( i_mindist = 99999999, i_best = 0, j = 0; j < p_fmt->p_palette->i_entries; j++ )
                {
                    int i_dist = 0;

                    i_dist += abs((int)p_fmt->p_palette->palette[j][0] - y);
                    i_dist += abs((int)p_fmt->p_palette->palette[j][1] - u);
                    i_dist += abs((int)p_fmt->p_palette->palette[j][2] - v);
                    i_dist += 2 * abs((int)p_fmt->p_palette->palette[j][3] - a);

                    if( i_dist < i_mindist )
                    {
                        i_mindist = i_dist;
                        i_best = j;
                    }
                }

                /* Set pixel to best color */
                p_region->p_picture->p[0].p_pixels[i_offset] = i_best;

                /* Update dithering state */
#ifdef RANDOM_DITHERING
                i_seed = (i_seed * 0x1283837) ^ 0x789479 ^ (i_seed >> 13);
#else
                i_ydelta = y - (int)p_fmt->p_palette->palette[i_best][0];
                i_udelta = u - (int)p_fmt->p_palette->palette[i_best][1];
                i_vdelta = v - (int)p_fmt->p_palette->palette[i_best][2];
                i_adelta = a - (int)p_fmt->p_palette->palette[i_best][3];
                pi_delta[ n * 4 ] = i_ydelta * 3 / 8;
                pi_delta[ n * 4 + 1 ] = i_udelta * 3 / 8;
                pi_delta[ n * 4 + 2 ] = i_vdelta * 3 / 8;
                pi_delta[ n * 4 + 3 ] = i_adelta * 3 / 8;
                i_ydelta = i_ydelta * 5 / 8;
                i_udelta = i_udelta * 5 / 8;
                i_vdelta = i_vdelta * 5 / 8;
                i_adelta = i_adelta * 5 / 8;
#endif
            }
        }
#ifndef RANDOM_DITHERING
        free( pi_delta );
#endif

        /* pad palette */
        for( i = p_fmt->p_palette->i_entries; i < i_max_entries; i++ )
        {
            p_fmt->p_palette->palette[i][0] = 0;
            p_fmt->p_palette->palette[i][1] = 0;
            p_fmt->p_palette->palette[i][2] = 0;
            p_fmt->p_palette->palette[i][3] = 0;
        }
        p_fmt->p_palette->i_entries = i_max_entries;
#ifdef DEBUG_DVBSUB1
        /* p_enc not valid here */
        msg_Dbg( p_enc, "best palette has %d colors", p_fmt->p_palette->i_entries );
#endif
    }
    return p_subpic;
} /* End of hack */

/****************************************************************************
 * Encode: the whole thing
 ****************************************************************************/
static block_t *Encode( encoder_t *p_enc, subpicture_t *p_subpic )
{
    subpicture_t *p_temp = NULL;
    subpicture_region_t *p_region = NULL;
    bs_t bits, *s = &bits;
    block_t *p_block;

    if( !p_subpic || !p_subpic->p_region ) return NULL;

    /* FIXME: this is a hack to convert VLC_CODEC_YUVA into
     *  VLC_CODEC_YUVP
     */
    p_region = p_subpic->p_region;
    if( p_region->fmt.i_chroma == VLC_CODEC_YUVA )
    {
        p_temp = YuvaYuvp( p_subpic );
        if( !p_temp )
        {
            msg_Err( p_enc, "no picture in subpicture" );
            return NULL;
        }
        p_region = p_subpic->p_region;
    }

    /* Sanity check */
    if( !p_region ) return NULL;

    if( ( p_region->fmt.i_chroma != VLC_CODEC_TEXT ) &&
        ( p_region->fmt.i_chroma != VLC_CODEC_YUVP ) )
    {
        msg_Err( p_enc, "chroma %4.4s not supported", (char *)&p_region->fmt.i_chroma );
        return NULL;
    }

    if( p_region->fmt.p_palette )
    {
        switch( p_region->fmt.p_palette->i_entries )
        {
            case 0:
            case 4:
            case 16:
            case 256:
                break;
            default:
                msg_Err( p_enc, "subpicture palette (%d) not handled",
                            p_region->fmt.p_palette->i_entries );
                return NULL;
        }
    }
    /* End of hack */

#ifdef DEBUG_DVBSUB
    msg_Dbg( p_enc, "encoding subpicture" );
#endif
    p_block = block_Alloc( 64000 );
    bs_init( s, p_block->p_buffer, p_block->i_buffer );

    bs_write( s, 8, 0x20 ); /* Data identifier */
    bs_write( s, 8, 0x0 );  /* Subtitle stream id */

    encode_page_composition( p_enc, s, p_subpic );
    encode_region_composition( p_enc, s, p_subpic );
    encode_clut( p_enc, s, p_subpic );
    encode_object( p_enc, s, p_subpic );

    /* End of display */
    bs_write( s, 8, 0x0f ); /* Sync byte */
    bs_write( s, 8, DVBSUB_ST_ENDOFDISPLAY ); /* Segment type */
    bs_write( s, 16, 1 );  /* Page id */
    bs_write( s, 16, 0 );  /* Segment length */

    bs_write( s, 8, 0xff );/* End marker */
    p_block->i_buffer = bs_pos( s ) / 8;
    p_block->i_pts = p_block->i_dts = p_subpic->i_start;
    if( !p_subpic->b_ephemer && ( p_subpic->i_stop > p_subpic->i_start ) )
    {
        block_t *p_block_stop;

        p_block->i_length = p_subpic->i_stop - p_subpic->i_start;

        /* Send another (empty) subtitle to signal the end of display */
        p_block_stop = block_Alloc( 64000 );
        bs_init( s, p_block_stop->p_buffer, p_block_stop->i_buffer );
        bs_write( s, 8, 0x20 ); /* Data identifier */
        bs_write( s, 8, 0x0 );  /* Subtitle stream id */
        encode_page_composition( p_enc, s, 0 );
        bs_write( s, 8, 0x0f ); /* Sync byte */
        bs_write( s, 8, DVBSUB_ST_ENDOFDISPLAY ); /* Segment type */
        bs_write( s, 16, 1 );  /* Page id */
        bs_write( s, 16, 0 );  /* Segment length */
        bs_write( s, 8, 0xff );/* End marker */
        p_block_stop->i_buffer = bs_pos( s ) / 8;
        p_block_stop->i_pts = p_block_stop->i_dts = p_subpic->i_stop;
        block_ChainAppend( &p_block, p_block_stop );
        p_block_stop->i_length = VLC_TICK_FROM_MS(100); /* p_subpic->i_stop - p_subpic->i_start; */
    }
#ifdef DEBUG_DVBSUB
    msg_Dbg( p_enc, "subpicture encoded properly" );
#endif
    return p_block;
}

/*****************************************************************************
 * CloseEncoder: encoder destruction
 *****************************************************************************/
static void CloseEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    var_Destroy( p_this , ENC_CFG_PREFIX "x" );
    var_Destroy( p_this , ENC_CFG_PREFIX "y" );
    var_Destroy( p_this , ENC_CFG_PREFIX "timeout" );

    if( p_sys->i_regions ) free( p_sys->p_regions );
    free( p_sys );
}

static void encode_page_composition( encoder_t *p_enc, bs_t *s,
                                     subpicture_t *p_subpic )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    subpicture_region_t *p_region;
    bool b_mode_change = false;
    int i_regions, i_timeout;

    bs_write( s, 8, 0x0f ); /* Sync byte */
    bs_write( s, 8, DVBSUB_ST_PAGE_COMPOSITION ); /* Segment type */
    bs_write( s, 16, 1 ); /* Page id */

    for( i_regions = 0, p_region = p_subpic ? p_subpic->p_region : 0;
         p_region; p_region = p_region->p_next, i_regions++ )
    {
        if( i_regions >= p_sys->i_regions )
        {
            encoder_region_t region;
            region.i_width = region.i_height = 0;
            p_sys->p_regions = xrealloc( p_sys->p_regions,
                          sizeof(encoder_region_t) * (p_sys->i_regions + 1) );
            p_sys->p_regions[p_sys->i_regions++] = region;
        }

        if( ( p_sys->p_regions[i_regions].i_width <
              (int)p_region->fmt.i_visible_width ) ||
            ( p_sys->p_regions[i_regions].i_width >
              (int)p_region->fmt.i_visible_width ) )
        {
            b_mode_change = true;
            msg_Dbg( p_enc, "region %i width change: %i -> %i",
                     i_regions, p_sys->p_regions[i_regions].i_width,
                     p_region->fmt.i_visible_width );
            p_sys->p_regions[i_regions].i_width =
                p_region->fmt.i_visible_width;
        }
        if( p_sys->p_regions[i_regions].i_height <
             (int)p_region->fmt.i_visible_height )
        {
            b_mode_change = true;
            msg_Dbg( p_enc, "region %i height change: %i -> %i",
                     i_regions, p_sys->p_regions[i_regions].i_height,
                     p_region->fmt.i_visible_height );
            p_sys->p_regions[i_regions].i_height =
                p_region->fmt.i_visible_height;
        }
    }

    bs_write( s, 16, i_regions * 6 + 2 ); /* Segment length */

    i_timeout = 0;
    if( p_subpic && !p_subpic->b_ephemer &&
        ( p_subpic->i_stop > p_subpic->i_start ) )
    {
        i_timeout = SEC_FROM_VLC_TICK(p_subpic->i_stop - p_subpic->i_start);
    }

    bs_write( s, 8, i_timeout ); /* Timeout */
    bs_write( s, 4, p_sys->i_page_ver++ );
    bs_write( s, 2, b_mode_change ?
              DVBSUB_PCS_STATE_CHANGE : DVBSUB_PCS_STATE_ACQUISITION );
    bs_write( s, 2, 0 ); /* Reserved */

    for( i_regions = 0, p_region = p_subpic ? p_subpic->p_region : 0;
         p_region; p_region = p_region->p_next, i_regions++ )
    {
        bs_write( s, 8, i_regions );
        bs_write( s, 8, 0 ); /* Reserved */
        if( (p_sys->i_offset_x > 0) && (p_sys->i_offset_y > 0) )
        {
            bs_write( s, 16, p_sys->i_offset_x ); /* override x position */
            bs_write( s, 16, p_sys->i_offset_y ); /* override y position */
        }
        else
        {
            bs_write( s, 16, p_region->i_x );
            bs_write( s, 16, p_region->i_y );
        }
    }
}

static void encode_clut( encoder_t *p_enc, bs_t *s, subpicture_t *p_subpic )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    subpicture_region_t *p_region = p_subpic->p_region;
    video_palette_t *p_pal, pal;

    /* Sanity check */
    if( !p_region ) return;

    if( p_region->fmt.i_chroma == VLC_CODEC_YUVP )
    {
        p_pal = p_region->fmt.p_palette;
    }
    else
    {
        pal.i_entries = 4;
        for( int i = 0; i < 4; i++ )
        {
            pal.palette[i][0] = 0;
            pal.palette[i][1] = 0;
            pal.palette[i][2] = 0;
            pal.palette[i][3] = 0;
        }
        p_pal = &pal;
    }

    bs_write( s, 8, 0x0f ); /* Sync byte */
    bs_write( s, 8, DVBSUB_ST_CLUT_DEFINITION ); /* Segment type */
    bs_write( s, 16, 1 );  /* Page id */

    bs_write( s, 16, p_pal->i_entries * 6 + 2 ); /* Segment length */
    bs_write( s, 8, 1 ); /* Clut id */
    bs_write( s, 4, p_sys->i_clut_ver++ );
    bs_write( s, 4, 0 ); /* Reserved */

    for( int i = 0; i < p_pal->i_entries; i++ )
    {
        bs_write( s, 8, i ); /* Clut entry id */
        bs_write( s, 1, p_pal->i_entries == 4 );   /* 2bit/entry flag */
        bs_write( s, 1, p_pal->i_entries == 16 );  /* 4bit/entry flag */
        bs_write( s, 1, p_pal->i_entries == 256 ); /* 8bit/entry flag */
        bs_write( s, 4, 0 ); /* Reserved */
        bs_write( s, 1, 1 ); /* Full range flag */
        bs_write( s, 8, p_pal->palette[i][3] ?  /* Y value */
                  (p_pal->palette[i][0] ? p_pal->palette[i][0] : 16) : 0 );
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
    int i_region;

    for( i_region = 0, p_region = p_subpic->p_region; p_region;
         p_region = p_region->p_next, i_region++ )
    {
        int i_entries = 4, i_depth = 0x1, i_bg = 0;
        bool b_text =
            ( p_region->fmt.i_chroma == VLC_CODEC_TEXT );

        if( !b_text )
        {
            video_palette_t *p_pal = p_region->fmt.p_palette;

            if( !p_pal )
            {
                msg_Err( p_enc, "subpicture has no palette - ignoring it" );
                break;
            }

            i_entries = p_pal->i_entries;
            i_depth = i_entries == 4 ? 0x1 : i_entries == 16 ? 0x2 : 0x3;

            for( i_bg = 0; i_bg < p_pal->i_entries; i_bg++ )
            {
                if( !p_pal->palette[i_bg][3] ) break;
            }
        }

        bs_write( s, 8, 0x0f ); /* Sync byte */
        bs_write( s, 8, DVBSUB_ST_REGION_COMPOSITION ); /* Segment type */
        bs_write( s, 16, 1 );   /* Page id */

        bs_write( s, 16, 10 + 6 + (b_text ? 2 : 0) ); /* Segment length */
        bs_write( s, 8, i_region );
        bs_write( s, 4, p_sys->i_region_ver++ );

        /* Region attributes */
        bs_write( s, 1, i_bg < i_entries ); /* Fill */
        bs_write( s, 3, 0 ); /* Reserved */
        bs_write( s, 16, p_sys->p_regions[i_region].i_width );
        bs_write( s, 16, p_sys->p_regions[i_region].i_height );
        bs_write( s, 3, i_depth );  /* Region level of compatibility */
        bs_write( s, 3, i_depth  ); /* Region depth */
        bs_write( s, 2, 0 ); /* Reserved */
        bs_write( s, 8, 1 ); /* Clut id */
        bs_write( s, 8, i_bg ); /* region 8bit pixel code */
        bs_write( s, 4, i_bg ); /* region 4bit pixel code */
        bs_write( s, 2, i_bg ); /* region 2bit pixel code */
        bs_write( s, 2, 0 ); /* Reserved */

        /* In our implementation we only have 1 object per region */
        bs_write( s, 16, i_region );
        bs_write( s, 2, b_text ? DVBSUB_OT_BASIC_CHAR:DVBSUB_OT_BASIC_BITMAP );
        bs_write( s, 2, 0 ); /* object provider flag */
        bs_write( s, 12, 0 );/* object horizontal position */
        bs_write( s, 4, 0 ); /* Reserved */
        bs_write( s, 12, 0 );/* object vertical position */

        if( b_text )
        {
            bs_write( s, 8, 1 ); /* foreground pixel code */
            bs_write( s, 8, 0 ); /* background pixel code */
        }
    }
}

static void encode_pixel_data( encoder_t *p_enc, bs_t *s,
                               subpicture_region_t *p_region,
                               bool b_top );

static void encode_object( encoder_t *p_enc, bs_t *s, subpicture_t *p_subpic )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    subpicture_region_t *p_region;
    int i_region;

    int i_length_pos, i_update_pos, i_pixel_data_pos;

    for( i_region = 0, p_region = p_subpic->p_region; p_region;
         p_region = p_region->p_next, i_region++ )
    {
        bs_write( s, 8, 0x0f ); /* Sync byte */
        bs_write( s, 8, DVBSUB_ST_OBJECT_DATA ); /* Segment type */
        bs_write( s, 16, 1 ); /* Page id */

        i_length_pos = bs_pos( s );
        bs_write( s, 16, 0 ); /* Segment length */
        bs_write( s, 16, i_region ); /* Object id */
        bs_write( s, 4, p_sys->i_region_ver++ );

        /* object coding method */
        switch( p_region->fmt.i_chroma )
        {
        case VLC_CODEC_YUVP:
            bs_write( s, 2, 0 );
            break;
        case VLC_CODEC_TEXT:
            bs_write( s, 2, 1 );
            break;
        default:
            msg_Err( p_enc, "FOURCC %d not supported by encoder.", p_region->fmt.i_chroma );
            continue;
        }

        bs_write( s, 1, 0 ); /* non modifying color flag */
        bs_write( s, 1, 0 ); /* Reserved */

        if( p_region->fmt.i_chroma == VLC_CODEC_TEXT )
        {
            int i_size, i;

            if( !p_region->p_text ) continue;

            i_size = __MIN( strlen( p_region->p_text->psz_text ), 256 );

            bs_write( s, 8, i_size ); /* number of characters in string */
            for( i = 0; i < i_size; i++ )
            {
                bs_write( s, 16, p_region->p_text->psz_text[i] );
            }

            /* Update segment length */
            SetWBE( &s->p_start[i_length_pos/8],
                    (bs_pos(s) - i_length_pos)/8 -2 );
            continue;
        }

        /* Coding of a bitmap object */
        i_update_pos = bs_pos( s );
        bs_write( s, 16, 0 ); /* topfield data block length */
        bs_write( s, 16, 0 ); /* bottomfield data block length */

        /* Top field */
        i_pixel_data_pos = bs_pos( s );
        encode_pixel_data( p_enc, s, p_region, true );
        i_pixel_data_pos = ( bs_pos( s ) - i_pixel_data_pos ) / 8;
        SetWBE( &s->p_start[i_update_pos/8], i_pixel_data_pos );

        /* Bottom field */
        i_pixel_data_pos = bs_pos( s );
        encode_pixel_data( p_enc, s, p_region, false );
        i_pixel_data_pos = ( bs_pos( s ) - i_pixel_data_pos ) / 8;
        SetWBE( &s->p_start[i_update_pos/8+2], i_pixel_data_pos );

        /* Stuffing for word alignment */
        bs_align_0( s );
        if( bs_pos( s ) % 16 ) bs_write( s, 8, 0 );

        /* Update segment length */
        SetWBE( &s->p_start[i_length_pos/8], (bs_pos(s) - i_length_pos)/8 -2 );
    }
}

static void encode_pixel_line_2bp( bs_t *s, subpicture_region_t *p_region,
                                   int i_line );
static void encode_pixel_line_4bp( bs_t *s, subpicture_region_t *p_region,
                                   int i_line );
static void encode_pixel_line_8bp( bs_t *s, subpicture_region_t *p_region,
                                   int i_line );
static void encode_pixel_data( encoder_t *p_enc, bs_t *s,
                               subpicture_region_t *p_region,
                               bool b_top )
{
    unsigned int i_line;

    /* Sanity check */
    if( p_region->fmt.i_chroma != VLC_CODEC_YUVP ) return;

    /* Encode line by line */
    for( i_line = !b_top; i_line < p_region->fmt.i_visible_height;
         i_line += 2 )
    {
        switch( p_region->fmt.p_palette->i_entries )
        {
        case 0:
            break;

        case 4:
            bs_write( s, 8, 0x10 ); /* 2 bit/pixel code string */
            encode_pixel_line_2bp( s, p_region, i_line );
            break;

        case 16:
            bs_write( s, 8, 0x11 ); /* 4 bit/pixel code string */
            encode_pixel_line_4bp( s, p_region, i_line );
            break;

        case 256:
            bs_write( s, 8, 0x12 ); /* 8 bit/pixel code string */
            encode_pixel_line_8bp( s, p_region, i_line );
            break;

        default:
            msg_Err( p_enc, "subpicture palette (%i) not handled",
                     p_region->fmt.p_palette->i_entries );
            break;
        }

        bs_write( s, 8, 0xf0 ); /* End of object line code */
    }
}

static void encode_pixel_line_2bp( bs_t *s, subpicture_region_t *p_region,
                                   int i_line )
{
    unsigned int i, i_length = 0;
    int i_pitch = p_region->p_picture->p->i_pitch;
    uint8_t *p_data = &p_region->p_picture->p->p_pixels[ i_pitch * i_line ];
    int i_last_pixel = p_data[0];

    for( i = 0; i <= p_region->fmt.i_visible_width; i++ )
    {
        if( ( i != p_region->fmt.i_visible_width ) &&
            ( p_data[i] == i_last_pixel ) && ( i_length != 284 ) )
        {
            i_length++;
            continue;
        }

        if( ( i_length == 1 ) || ( i_length == 11 ) || ( i_length == 28 ) )
        {
            /* 2bit/pixel code */
            if( i_last_pixel )
                bs_write( s, 2, i_last_pixel );
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

static void encode_pixel_line_4bp( bs_t *s, subpicture_region_t *p_region,
                                   int i_line )
{
    unsigned int i, i_length = 0;
    int i_pitch = p_region->p_picture->p->i_pitch;
    uint8_t *p_data = &p_region->p_picture->p->p_pixels[ i_pitch * i_line ];
    int i_last_pixel = p_data[0];

    for( i = 0; i <= p_region->fmt.i_visible_width; i++ )
    {
        if( i != p_region->fmt.i_visible_width &&
            p_data[i] == i_last_pixel && i_length != 280 )
        {
            i_length++;
            continue;
        }

        if( ( i_length == 1 ) ||
            ( ( i_length == 3 ) && i_last_pixel ) ||
            ( i_length == 8 ) )
        {
            /* 4bit/pixel code */
            if( i_last_pixel )
                bs_write( s, 4, i_last_pixel );
            else
            {
                bs_write( s, 4, 0 );
                bs_write( s, 1, 1 );
                bs_write( s, 1, 1 );
                bs_write( s, 2, 0 ); /* pseudo color 0 */
            }
            i_length--;
        }

        if( i_length == 2 )
        {
            if( i_last_pixel )
            {
                bs_write( s, 4, i_last_pixel );
                bs_write( s, 4, i_last_pixel );
            }
            else
            {
                bs_write( s, 4, 0 );
                bs_write( s, 1, 1 );
                bs_write( s, 1, 1 );
                bs_write( s, 2, 1 ); /* 2 * pseudo color 0 */
            }
        }
        else if( !i_last_pixel && ( i_length >= 3 ) && ( i_length <= 9 ) )
        {
            bs_write( s, 4, 0 );
            bs_write( s, 1, 0 );
            bs_write( s, 3, i_length - 2 ); /* (i_length - 2) * color 0 */
        }
        else if( i_length > 2 )
        {
            bs_write( s, 4, 0 );
            bs_write( s, 1, 1 );

            if( i_length <= 7 )
            {
                bs_write( s, 1, 0 );
                bs_write( s, 2, i_length - 4 );
                bs_write( s, 4, i_last_pixel );
            }
            else
            {
                bs_write( s, 1, 1 );

                if( i_length <= 24 )
                {
                    bs_write( s, 2, 2 );
                    bs_write( s, 4, i_length - 9 );
                    bs_write( s, 4, i_last_pixel );
                }
                else
                {
                    bs_write( s, 2, 3 );
                    bs_write( s, 8, i_length - 25 );
                    bs_write( s, 4, i_last_pixel );
                }
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

static void encode_pixel_line_8bp( bs_t *s, subpicture_region_t *p_region,
                                   int i_line )
{
    unsigned int i, i_length = 0;
    int i_pitch = p_region->p_picture->p->i_pitch;
    uint8_t *p_data = &p_region->p_picture->p->p_pixels[ i_pitch * i_line ];
    int i_last_pixel = p_data[0];

    for( i = 0; i <= p_region->fmt.i_visible_width; i++ )
    {
        if( ( i != p_region->fmt.i_visible_width ) &&
            ( p_data[i] == i_last_pixel ) && ( i_length != 127 ) )
        {
            i_length++;
            continue;
        }

        if( ( i_length == 1 ) && i_last_pixel )
        {
            /* 8bit/pixel code */
            bs_write( s, 8, i_last_pixel );
        }
        else if( ( i_length == 2 ) && i_last_pixel )
        {
            /* 8bit/pixel code */
            bs_write( s, 8, i_last_pixel );
            bs_write( s, 8, i_last_pixel );
        }
        else if( i_length <= 127 )
        {
            bs_write( s, 8, 0 );

            if( !i_last_pixel )
            {
                bs_write( s, 1, 0 );
                bs_write( s, 7, i_length ); /* pseudo color 0 */
            }
            else
            {
                bs_write( s, 1, 1 );
                bs_write( s, 7, i_length );
                bs_write( s, 8, i_last_pixel );
            }
        }

        if( i == p_region->fmt.i_visible_width ) break;

        i_last_pixel = p_data[i];
        i_length = 1;
    }

    /* Stop */
    bs_write( s, 8, 0 );
    bs_write( s, 8, 0 );

    /* Stuffing */
    bs_align_0( s );
}

#endif

static void default_dds_init( decoder_t * p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* see notes on DDS at the top of the file */

    /* configure for SD res in case DDS is not present */
    p_sys->display.i_version = 0xff; /* an invalid version so it's always different */
    p_sys->display.i_width = 720;
    p_sys->display.i_height = 576;
    p_sys->display.b_windowed = false;
}

