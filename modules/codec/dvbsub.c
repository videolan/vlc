/*****************************************************************************
 * dvbsub.c : DVB subtitles decoder thread
 *****************************************************************************
 * Copyright (C) 2003 ANEVIA
 * Copyright (C) 2003 VideoLAN
 * $Id: dvbsub.c,v 1.6 2003/11/24 02:35:50 fenrir Exp $
 *
 * Authors: Damien LUCAS <damien.lucas@anevia.com>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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

#include "vlc_bits.h"

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  Open ( vlc_object_t *p_this );
static void Close( vlc_object_t *p_this );

vlc_module_begin();
    add_category_hint( N_("subtitles"), NULL, VLC_TRUE );
    set_description( _("subtitles decoder") );
    set_capability( "decoder", 50 );
    set_callbacks( Open, Close );
vlc_module_end();


// Wow, that's ugly but very usefull for a memory leak track
// so I just keep it
#if 0
static long long unsigned int trox_malloc_nb = 0;
static long long unsigned int trox_free_nb = 0;

static void* trox_malloc (size_t size)
{ ++trox_malloc_nb; return malloc (size); }

static void trox_free (void* ptr)
{ ++trox_free_nb; free(ptr); return; }

static void trox_call ()
{
  fprintf(stderr, "dvbbsub -- Memory usage:  %llu mallocs %llu frees (%llu)\n",
                  trox_malloc_nb,
                  trox_free_nb,
                  trox_malloc_nb - trox_free_nb);
  return;
}
#else
# define trox_malloc malloc
# define trox_free free
# define trox_call()
#endif
/****************************************************************************
 * Local structures
 ****************************************************************************
 * Those structures refer closely to the ETSI 300 743 Object model
 ****************************************************************************/

/* Storage of a RLE entry */
typedef struct dvbsub_rle_s
{
    uint16_t                 i_num;
    uint8_t                 i_color_code;
    uint8_t                 y;
    uint8_t                 cr;
    uint8_t                 cb;
    uint8_t                 t;
    struct dvbsub_rle_s*    p_next;
} dvbsub_rle_t;

/* A subpicture image is a list of codes
 * We need to store the length of each line since nothing specify in
 * the standard that all lines shoudl have the same length
 * WARNING: We assume here that a spu is less than 576 lines high */
typedef struct
{
    uint16_t                        i_rows;
    uint16_t                        i_cols[576];
    dvbsub_rle_t*                   p_last;
    dvbsub_rle_t*                   p_codes;
} dvbsub_image_t;

/* The object definition gives the position of the object in a region */
typedef struct dvbsub_objectdef_s
{
    uint16_t                    i_id;
    uint8_t                     i_type;
    uint8_t                     i_provider;
    uint16_t                    i_xoffset;
    uint16_t                    i_yoffset;
    uint8_t                     i_fg_pc;
    uint8_t                     i_bg_pc;
    struct dvbsub_objectdef_s*  p_next;
} dvbsub_objectdef_t;

/* The Region is an aera on the image
 * with a list of the object definitions associated
 * and a CLUT */
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
    dvbsub_objectdef_t*    p_object;
} dvbsub_region_t;

/* The page defines the list of regions */
typedef struct
{
    uint16_t              i_id;
    uint8_t               i_timeout;
    uint8_t               i_state;
    uint8_t               i_version_number;
    uint8_t               i_regions_number;
    dvbsub_region_t*      regions;
} dvbsub_page_t;

/* An object is constituted of 2 images (for interleaving) */
typedef struct dvbsub_object_s
{
    uint16_t                i_id;
    uint8_t                 i_version_number;
    uint8_t                 i_coding_method;
    vlc_bool_t              b_non_modify_color;
    dvbsub_image_t*         topfield;
    dvbsub_image_t*         bottomfield;
    struct dvbsub_object_s* p_next;
} dvbsub_object_t;

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
    dvbsub_color_t          c_2b[0xff];
    dvbsub_color_t          c_4b[0xff];
    dvbsub_color_t          c_8b[0xff];
} dvbsub_clut_t;

typedef struct
{
    uint8_t                 i_x;
    uint16_t                i_y;
    dvbsub_image_t*         p_rle_top;
    dvbsub_image_t*         p_rle_bot;
} dvbsub_render_t;

typedef struct
{
    int i_id;

    mtime_t i_pts;

    dvbsub_clut_t*          p_clut[0xff];
    dvbsub_page_t*          p_page;
    dvbsub_object_t*        p_objects;
    subpicture_t*           p_spu[16];
} dvbsub_all_t;

struct subpicture_sys_t
{
    mtime_t         i_pts;
    void *          p_data;                          /* rle datas are stored */
    vlc_object_t*   p_input;                            /* Link to the input */
    vlc_bool_t      b_obsolete;
};

struct decoder_sys_t
{
    vout_thread_t *p_vout;

    bs_t          bs;

    dvbsub_all_t dvbsub;
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

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void Decode   ( decoder_t *, block_t ** );

static vout_thread_t *FindVout( decoder_t * );

static int  dvbsub_init( dvbsub_all_t *, int  );
static void dvbsub_decode_segment( dvbsub_all_t *p_dvbsub, bs_t *s );
static void dvbsub_render( dvbsub_all_t *, vout_thread_t * );
static void dvbsub_clean( dvbsub_all_t * );


/*****************************************************************************
 * Open: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to chose.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*) p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('d','v','b','s') )
    {
        return VLC_EGENERIC;
    }

    p_dec->pf_decode_sub = Decode;
    p_sys = p_dec->p_sys = malloc( sizeof( decoder_sys_t ) );

    p_sys->p_vout = NULL;

    dvbsub_init( &p_sys->dvbsub, p_dec->fmt_in.subs.dvb.i_id );

    es_format_Init( &p_dec->fmt_out, SPU_ES, VLC_FOURCC( 'd','v','b','s' ) );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*) p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_vout && p_sys->p_vout->p_subpicture != NULL )
    {
        subpicture_t *  p_subpic;
        int i_subpic;
        for( i_subpic = 0; i_subpic < VOUT_MAX_SUBPICTURES; i_subpic++ )
        {
            p_subpic = &p_sys->p_vout->p_subpicture[i_subpic];
            if( p_subpic != NULL &&
              ( ( p_subpic->i_status == RESERVED_SUBPICTURE ) ||
                ( p_subpic->i_status == READY_SUBPICTURE ) ) )
            {
                vout_DestroySubPicture( p_sys->p_vout, p_subpic );
            }
        }
    }

    dvbsub_clean( &p_sys->dvbsub );

    free( p_sys );
}

/*****************************************************************************
 * Decode:
 *****************************************************************************/
static void Decode( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t       *p_block;

    if( pp_block == NULL || *pp_block == NULL )
    {
        return;
    }
    p_block = *pp_block;
    *pp_block = NULL;

    p_sys->dvbsub.i_pts = p_block->i_pts;
    if( p_sys->dvbsub.i_pts <= 0 )
    {
        msg_Warn( p_dec, "non dated subtitle" );
        block_Release( p_block );
        return;
    }

    if( ( p_sys->p_vout = FindVout( p_dec ) ) )
    {
        int i_data_identifier;
        int i_subtitle_stream_id;
        int i_end_data_marker;

        bs_init( &p_sys->bs, p_block->p_buffer, p_block->i_buffer );

        i_data_identifier = bs_read( &p_sys->bs, 8 );
        i_subtitle_stream_id = bs_read( &p_sys->bs, 8 );

        for( ;; )
        {
            if( bs_show( &p_sys->bs, 8 ) != 0x0f )
            {
                break;
            }
            dvbsub_decode_segment( &p_sys->dvbsub, &p_sys->bs );
        }
        i_end_data_marker = bs_read( &p_sys->bs, 8 );

        /* Check if the page is to be displayed */
        if( p_sys->dvbsub.p_page && p_sys->dvbsub.p_objects )
        {
            dvbsub_render( &p_sys->dvbsub, p_sys->p_vout );
        }

        vlc_object_release( p_sys->p_vout );
    }

    block_Release( p_block );
}

/* following functions are local */
/*****************************************************************************
 * FindVout: Find a vout or wait for one to be created.
 *****************************************************************************/
static vout_thread_t *FindVout( decoder_t *p_dec )
{
    for( ;; )
    {
        vout_thread_t *p_vout;

        if( p_dec->b_die || p_dec->b_error )
        {
            return NULL;
        }
        p_vout = vlc_object_find( p_dec, VLC_OBJECT_VOUT, FIND_ANYWHERE );
        if( p_vout )
        {
            return p_vout;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }
}


static int dvbsub_init( dvbsub_all_t *p_dvbsub, int i_id )
{
    int i;

    memset( p_dvbsub, 0, sizeof( dvbsub_all_t ) );

    p_dvbsub->i_pts     = 0;
    p_dvbsub->i_id      = i_id;
    p_dvbsub->p_page     = NULL;
    p_dvbsub->p_objects  = NULL;
    for( i = 0; i < 255; i++ )
    {
        p_dvbsub->p_clut[i] = NULL;
    }
    for( i = 0; i < 16; i++ )
    {
        p_dvbsub->p_spu[i] = NULL;
    }
    return 0;
}

static void free_all( dvbsub_all_t * );

static void dvbsub_clean( dvbsub_all_t *p_dvbsub )
{
    free_all( p_dvbsub );
    trox_call() ;
}


static void dvbsub_decode_clut( dvbsub_all_t *p_dvbsub, bs_t *s );
static void dvbsub_decode_page_composition( dvbsub_all_t *p_dvbsub, bs_t *s);
static void dvbsub_decode_region_composition( dvbsub_all_t *p_dvbsub, bs_t *s );
static void dvbsub_stop_display( dvbsub_all_t* p_dvbsub );
static void dvbsub_decode_object( dvbsub_all_t *p_dvbsub, bs_t *s );

static void free_page (dvbsub_page_t* p_p);

static void dvbsub_decode_segment( dvbsub_all_t *p_dvbspu, bs_t *s )
{
    int i_type;
    int i_page_id;
    int i_size;

    /* sync_byte */
    bs_skip( s, 8 );

    /* segment type */
    i_type = bs_read( s, 8 );

    /* page id */
    i_page_id = bs_read( s, 16 );

    /* segment size */
    i_size = bs_show( s, 16 );

    if( i_page_id != p_dvbspu->i_id )
    {
        bs_skip( s,  8 * ( 2 + i_size ) );
        return;
    }

    switch( i_type )
    {
        case DVBSUB_ST_CLUT_DEFINITION:
            dvbsub_decode_clut( p_dvbspu, s );
            break;
         case DVBSUB_ST_PAGE_COMPOSITION:
            dvbsub_decode_page_composition( p_dvbspu, s );
            break;
        case DVBSUB_ST_REGION_COMPOSITION:
            dvbsub_decode_region_composition( p_dvbspu, s );
            break;
        case DVBSUB_ST_OBJECT_DATA:
            dvbsub_decode_object( p_dvbspu, s );
            break;
        case DVBSUB_ST_ENDOFDISPLAY:
            dvbsub_stop_display( p_dvbspu );
            break;
        case DVBSUB_ST_STUFFING:
        default:
            fprintf( stderr, "*** DVBSUB - Unsupported segment type ! (%04x)",
                      i_type );
            bs_skip( s,  8 * ( 2 + i_size ) );
            break;
    }
}

static void dvbsub_stop_display( dvbsub_all_t *p_dvbsub )
{
    int i;

    for( i = 0; p_dvbsub->p_spu[i] != NULL; i++ )
    {
        p_dvbsub->p_spu[i]->i_stop = p_dvbsub->i_pts;
    }
}

static void dvbsub_decode_clut( dvbsub_all_t *p_dvbsub, bs_t *s )
{
    uint16_t         i_segment_length;
    uint16_t         i_processed_length;
    dvbsub_clut_t*   clut;
    uint8_t          i_clut_id;
    uint8_t          i_version_number;

    i_segment_length = bs_read( s, 16 );
    i_clut_id        = bs_read( s, 8 );
    i_version_number = bs_read( s, 4 );

    // Check that this id doesn't not already exist
    // with the same version number
    // And allocate memory if necessary
    if( p_dvbsub->p_clut[i_clut_id] != NULL)
    {
        if( p_dvbsub->p_clut[i_clut_id]->i_version_number == i_version_number )
        {
            //TODO skip the right number of bits
            return;
        }
        else
        {
            memset( p_dvbsub->p_clut[i_clut_id], 0, sizeof(dvbsub_clut_t) );
        }
    }
    else
    {
        p_dvbsub->p_clut[i_clut_id] = trox_malloc( sizeof(dvbsub_clut_t) );
    }
    clut = p_dvbsub->p_clut[i_clut_id];

    /* We don't have this version of the CLUT:
     * Parse it                                 */
    clut->i_version_number = i_version_number;
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
            y  = bs_read( s, 6 );
            cr = bs_read( s, 4 );
            cb = bs_read( s, 4 );
            t  = bs_read( s, 2 );
            i_processed_length += 4;
        }

        /* According to EN 300-743 section 7.2.3 note 1, type should
         * not have more than 1 bit set to one
           But, some strams don't respect this note. */

        if( i_type&0x04)
        {
            clut->c_2b[i_id].Y = y;
            clut->c_2b[i_id].Cr = cr;
            clut->c_2b[i_id].Cb = cb;
            clut->c_2b[i_id].T = t;
        }
        if( i_type&0x02)
        {
            clut->c_4b[i_id].Y = y;
            clut->c_4b[i_id].Cr = cr;
            clut->c_4b[i_id].Cb = cb;
            clut->c_4b[i_id].T = t;
        }
        if( i_type & 0x01)
        {
            clut->c_8b[i_id].Y = y;
            clut->c_8b[i_id].Cr = cr;
            clut->c_8b[i_id].Cb = cb;
            clut->c_8b[i_id].T = t;
        }
    }
}

static void dvbsub_decode_page_composition( dvbsub_all_t *p_dvbsub, bs_t *s )
{
    unsigned int i_version_number;
    unsigned int i_state;
    unsigned int i_segment_length;
    uint8_t i_timeout;
    unsigned int i;

    i_segment_length = bs_read( s, 16 );

    /* A page is composed by one or more region: */
    i_timeout =  bs_read( s, 8 );
    i_version_number =  bs_read( s, 4 );
    i_state =  bs_read( s, 2 );

    /* TODO We assume it is a new page (i_state) */
    if( p_dvbsub->p_page ) free_page( p_dvbsub->p_page );

    bs_skip( s, 2 ); /* Reserved */

    /* Allocate a new page */
    p_dvbsub->p_page = trox_malloc( sizeof(dvbsub_page_t) );
    p_dvbsub->p_page->i_timeout = i_timeout;

    /* Number of regions: */
    p_dvbsub->p_page->i_regions_number = (i_segment_length-2) / 6;

    /* Special workaround for CAVENA encoders
     * a page with no regions is sent instead of a 0x80 packet (End Of Display) */
    if( p_dvbsub->p_page->i_regions_number == 0 )
    {
        dvbsub_stop_display( p_dvbsub );
    }
    /* /Special workaround */

    p_dvbsub->p_page->regions =
               trox_malloc(p_dvbsub->p_page->i_regions_number*sizeof(dvbsub_region_t));
    for( i = 0; i < p_dvbsub->p_page->i_regions_number; i++ )
    {
        p_dvbsub->p_page->regions[i].i_id = bs_read( s, 8 );
        bs_skip( s, 8 ); /* Reserved */
        p_dvbsub->p_page->regions[i].i_x = bs_read( s, 16 );
        p_dvbsub->p_page->regions[i].i_y = bs_read( s, 16 );
        p_dvbsub->p_page->regions[i].p_object = NULL;
    }
}


static void dvbsub_decode_region_composition( dvbsub_all_t *p_dvbsub, bs_t *s )
{
    dvbsub_region_t* p_region = NULL;
    unsigned int i_segment_length;
    unsigned int i_processed_length;
    unsigned int i_region_id;
    unsigned int i;

    i_segment_length = bs_read( s, 16 );

    /* Get region id: */
    i_region_id = bs_read( s, 8 );
    for( i = 0; i < p_dvbsub->p_page->i_regions_number; i++ )
    {
        if( p_dvbsub->p_page->regions[i].i_id ==  i_region_id )
        {
            p_region = &(p_dvbsub->p_page->regions[i]);
        }
    }

    if( p_region == NULL )
    {
        /* TODO
         * The region has never been declared before
         * Internal error */
        fprintf( stderr, "Decoding of undeclared region N/A...\n" );
        return;
    }

    /* Skip version number and fill flag */
    if( bs_show( s, 4 ) == p_region->i_version_number )
    {
        fprintf( stderr, "Skipping already known region N/A ...\n" );
        /* TODO Skip the right number of bits */
    }

    /* Region attributes */
    p_region->i_version_number = bs_read( s, 4 );
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

    /* List of objects in the region: */
    /* We already skipped 10 bytes */

    i_processed_length = 10;
    while( i_processed_length < i_segment_length )
    {
        /* We create a new object */
        dvbsub_objectdef_t *p_obj = trox_malloc(sizeof(dvbsub_objectdef_t));

        /* We parse object properties */
        p_obj->p_next       = NULL;
        p_obj->i_id         = bs_read( s, 16 );
        p_obj->i_type       = bs_read( s, 2 );
        p_obj->i_provider   = bs_read( s, 2 );
        p_obj->i_xoffset    = bs_read( s, 12 );
        bs_skip( s, 4 ); /* Reserved */
        p_obj->i_yoffset    = bs_read( s, 12 );

        i_processed_length += 6;

        if( p_obj->i_type == DVBSUB_OT_BASIC_CHAR ||
            p_obj->i_type == DVBSUB_OT_COMPOSITE_STRING )
        {
            p_obj->i_fg_pc =  bs_read( s, 8 );
            p_obj->i_bg_pc =  bs_read( s, 8 );
            i_processed_length += 2;
        }

        /* we append it */
        if( p_region->p_object )
        {
            dvbsub_objectdef_t *p_o;
            for( p_o = p_region->p_object; ; p_o = p_o->p_next )
            {
                if( p_o->p_next == NULL )
                {
                    break;
                }
            }
            p_o->p_next = p_obj;
        }
        else
        {
            p_region->p_object = p_obj;
        }
    }
}

static dvbsub_image_t* dvbsub_parse_pdata( dvbsub_all_t *p_dvbsub, bs_t *s, uint16_t length );
static uint16_t dvbsub_count0x11( bs_t *s, uint16_t* p, dvbsub_image_t* p_image);

static void dvbsub_decode_object( dvbsub_all_t *p_dvbsub, bs_t *s )
{
    dvbsub_object_t *p_obj;
    uint16_t        i_segment_length;

    /* Memory Allocation */
    p_obj = trox_malloc ( sizeof ( dvbsub_object_t ) );
    p_obj->p_next = NULL;

    i_segment_length = bs_read( s, 16 );

    /* TODO Check we don't already have this object / this version */
    p_obj->i_id              =  bs_read( s, 16 );
    p_obj->i_version_number  = bs_read( s, 4 );
    p_obj->i_coding_method   = bs_read( s, 2 );
    p_obj->b_non_modify_color= bs_read( s, 1 );
    bs_skip( s, 1 ); /* Reserved */

    if( p_obj->i_coding_method == 0x00 )
    {
        uint16_t    i_topfield_length;
        uint16_t    i_bottomfield_length;

        i_topfield_length   = bs_read( s, 16 );
        i_bottomfield_length= bs_read( s, 16 );

        p_obj->topfield    = dvbsub_parse_pdata( p_dvbsub, s, i_topfield_length );
        p_obj->bottomfield = dvbsub_parse_pdata( p_dvbsub, s, i_bottomfield_length );
    }
    else
    {
        bs_skip( s, (i_segment_length - 3 ) * 8 );
        /*TODO
         * DVB subtitling as characters */
    }

    /* Add this object to the list of the page */
    p_obj->p_next = p_dvbsub->p_objects;
    p_dvbsub->p_objects = p_obj;
}

static dvbsub_image_t* dvbsub_parse_pdata( dvbsub_all_t *p_dvbsub, bs_t *s, uint16_t length )
{
    dvbsub_image_t* p_image;
    uint16_t i_processed_length=0;
    uint16_t i_lines=0;
    uint16_t i_cols_last=0;

    p_image = trox_malloc ( sizeof ( dvbsub_image_t) );
    p_image->p_last=NULL;

    memset(p_image->i_cols, 0, 576*sizeof(uint16_t));

    /* Let's parse it a first time to determine the size of the buffer */
    while( i_processed_length < length)
    {
        switch( bs_read( s, 8 ) )
        {
            case 0x10:
                fprintf(stderr, "0x10 N/A\n");
                break;
            case 0x11:
                i_processed_length +=
                    1 + dvbsub_count0x11( s, &(p_image->i_cols[i_lines]),
                                          p_image );
                break;
            case 0x12:
                fprintf(stderr, "0x12 N/A\n");
                break;
            case 0x20:
                fprintf(stderr, "0x20 N/A\n");
                break;
            case 0x21:
                fprintf(stderr, "0x21 N/A\n");
                break;
            case 0x22:
                fprintf(stderr, "0x22 N/A\n");
                break;
            case 0xf0:
                i_processed_length++;
                i_lines++;
                break;
        }
    }

    p_image->i_rows =  i_lines;
    p_image->i_cols[i_lines] = i_cols_last;

    /* Check word-aligned bits */
    if( bs_show( s, 8 ) == 0x00 )
    {
        bs_skip( s, 8 );
    }

    return p_image;
}



static void add_rle_code( dvbsub_image_t *p, uint16_t num, uint8_t color )
{
    if(p->p_last != NULL)
    {
        p->p_last->p_next = trox_malloc (sizeof (dvbsub_rle_t));
        p->p_last = p->p_last->p_next;
    }
    else
    {
        p->p_codes =  trox_malloc (sizeof (dvbsub_rle_t));
        p->p_last = p->p_codes;
    }
    p->p_last->i_num = num;
    p->p_last->i_color_code = color;
    p->p_last->p_next = NULL;
}


static uint16_t dvbsub_count0x11( bs_t *s, uint16_t* p, dvbsub_image_t* p_image )
{
    uint16_t i_processed=0;
    vlc_bool_t b_stop=0;
    uint16_t i_count = 0;
    uint8_t i_color =0;

    while (!b_stop)
    {
        if( (i_color = bs_read( s, 4 )) != 0x00 )
        {
            (*p)++;
            i_processed+=4;

            /* 1 pixel of color code '0000' */
            add_rle_code( p_image, 1, i_color );
        }
        else
        {
            if( bs_read( s, 1 ) == 0x00 )           // Switch1
            {
                if( bs_show( s, 3 ) != 0x00 )
                {
                    i_count = 2 + bs_read( s, 3 );
                    (*p) += i_count ;
                    add_rle_code( p_image, i_count, 0x00 );
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
                    add_rle_code( p_image, i_count, i_color );
                }
                else
                {
                    switch ( bs_read( s, 2 ) )     //Switch3
                    {
                        case 0x0:
                            (*p)++;
                            i_processed += 8;
                            add_rle_code( p_image, 1, 0x00 );
                            break;
                        case 0x1:
                            (*p)+=2;
                            i_processed += 8;
                            add_rle_code( p_image, 2, 0x00 );
                            break;
                        case 0x2:
                             i_count = 9 + bs_read( s, 4 );
                             i_color = bs_read( s, 4 );
                             (*p)+= i_count;
                             i_processed += 16;
                             add_rle_code( p_image, i_count, i_color );
                             break;
                        case 0x3:
                             i_count= 25 + bs_read( s, 8 );
                             i_color = bs_read( s, 4 );
                             (*p)+= i_count;
                             i_processed += 20;
                             add_rle_code( p_image, i_count, i_color );
                             break;
                    }
                }
            }
        }
    }

    bs_align( s );

    return ( i_processed + 7 ) / 8 ;
}

static void free_image (dvbsub_image_t* p_i)
{
    dvbsub_rle_t* p1;
    dvbsub_rle_t* p2=NULL;

    for( p1 = p_i->p_codes; p1 != NULL; p1=p2)
    {
        p2=p1->p_next;
        trox_free(p1);
        p1=NULL;
    }

    trox_free(p_i);
}

static void free_object (dvbsub_object_t* p_o)
{
    trox_free(p_o);
}

static void free_objectdefs ( dvbsub_objectdef_t* p_o)
{
    dvbsub_objectdef_t* p1;
    dvbsub_objectdef_t* p2=NULL;

    for( p1 = p_o; p1 != NULL; p1=p2)
    {
        p2=p1->p_next;
        trox_free(p1);
        p1=NULL;
    }
}

static void free_regions (dvbsub_region_t* p_r, uint8_t nb)
{
    unsigned int i;

    for (i = 0; i<nb; i++) free_objectdefs ( p_r[i].p_object );
    trox_free (p_r);
    p_r = NULL;
}

static void free_objects (dvbsub_object_t* p_o)
{
    dvbsub_object_t* p1;
    dvbsub_object_t* p2=NULL;

    for( p1 = p_o; p1 != NULL; p1=p2)
    {
        p2=p1->p_next;
        free_image (p1->topfield);
        free_image (p1->bottomfield);
        free_object(p1);
    }
}

static void free_clut ( dvbsub_clut_t* p_c )
{
    trox_free(p_c);
}

static void free_page (dvbsub_page_t* p_p)
{
    free_regions (p_p->regions, p_p->i_regions_number);
    trox_free(p_p);
    p_p = NULL;
}

static void free_spu( subpicture_t *p_spu )
{
    if ( p_spu->p_sys )
    {
        free_image(((dvbsub_render_t *)p_spu->p_sys->p_data)->p_rle_top);
        free_image(((dvbsub_render_t *)p_spu->p_sys->p_data)->p_rle_bot);
        trox_free(p_spu->p_sys->p_data);
        trox_free( p_spu->p_sys );
        p_spu->p_sys = NULL;
    }
}

static void free_all ( dvbsub_all_t* p_a )
{
    unsigned int i;

    for(i=0; i<0xff; i++) if (p_a->p_clut[i]) free_clut ( p_a->p_clut[i] );
    for(i=0; i<16; i++) if (p_a->p_spu[i]) free_spu ( p_a->p_spu[i] );
    if(p_a->p_page) free_page( p_a->p_page );
    free_objects (p_a->p_objects);
}

static void RenderYUY2( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_spu )
{
    /* Common variables */
    uint8_t  *p_desty;
    uint16_t i,j;
    uint16_t i_cnt;
    uint16_t x, y;
    dvbsub_rle_t* p_c;
    dvbsub_render_t* p_r = ((dvbsub_render_t *)p_spu->p_sys->p_data);
    dvbsub_image_t* p_im = p_r->p_rle_top;
    i=0;
    j=0;
    p_desty = p_pic->Y_PIXELS;
    //let's render the 1st frame
    for(p_c = p_im->p_codes; p_c->p_next != NULL; p_c=p_c->p_next)
    {
//        if( p_c->y != 0  && p_c->t < 0x20)
        if( p_c->y != 0  && p_c->t < 0x20)
        {
            x = j+ p_r->i_x;
            y = 2*i+p_r->i_y;
            //memset(p_desty+ y*p_pic->Y_PITCH + x, p_c->y, p_c->i_num);
            // In YUY2 we have to set pixel per pixel
            for( i_cnt = 0; i_cnt < p_c->i_num; i_cnt+=2 )
            {
                memset(p_desty+ y*p_pic->Y_PITCH + 2*x + i_cnt, p_c->y, 1);
           //     memset(p_desty+ y*p_pic->Y_PITCH + 2*x + i_cnt+1, p_c->cr, 1);
          //      memset(p_desty+ y*p_pic->Y_PITCH + 2*x + i_cnt+2, p_c->y, 1);
           //     memset(p_desty+ y*p_pic->Y_PITCH + 2*x + i_cnt+3, p_c->cb, 1);
            }
        }
        j += p_c->i_num;
        if(j >= p_im->i_cols[i])
        {
            i++; j=0;
        }
        if( i>= p_im->i_rows) break;
    }
    //idem for the second frame
    p_im = p_r->p_rle_bot; i=0; j=0;
    for(p_c = p_im->p_codes; p_c->p_next != NULL; p_c=p_c->p_next)
    {
        if( p_c->y != 0 && p_c->t < 0x20)
        {
            x = j+ p_r->i_x;
            y = 2*i+1+p_r->i_y;
            //memset(p_desty+ y*p_pic->Y_PITCH + x, p_c->y, p_c->i_num);
            // In YUY2 we have to set pixel per pixel
            for( i_cnt = 0; i_cnt < p_c->i_num; i_cnt+=2 )
            {
                memset(p_desty+ y*p_pic->Y_PITCH + 2*x + i_cnt, p_c->y, 1);
           //     memset(p_desty+ y*p_pic->Y_PITCH + 2*x + i_cnt+1, p_c->cr, 1);
           //     memset(p_desty+ y*p_pic->Y_PITCH + 2*x + i_cnt+2, p_c->y, 1);
           //     memset(p_desty+ y*p_pic->Y_PITCH + 2*x + i_cnt+3, p_c->cb, 1);
           }
        }
        j += p_c->i_num;
        if(j >= p_im->i_cols[i])
        {
            i++; j=0;
        }
        if( i>= p_im->i_rows) break;
    }
}


static void RenderI42x( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_spu )
{
    /* Common variables */
    uint8_t  *p_desty;
    uint8_t  *p_destu;
    uint8_t  *p_destv;
    uint16_t i,j;
    uint16_t x, y;
    dvbsub_rle_t* p_c;
    dvbsub_render_t* p_r = ((dvbsub_render_t *)p_spu->p_sys->p_data);
    dvbsub_image_t* p_im = p_r->p_rle_top;
    i=0;
    j=0;
    p_desty = p_pic->Y_PIXELS;
    p_destu = p_pic->U_PIXELS;
    p_destv = p_pic->V_PIXELS;
    //let's render the 1st frame
    for(p_c = p_im->p_codes; p_c->p_next != NULL; p_c=p_c->p_next)
    {
        if( p_c->y != 0 )
        {
            x = j+ p_r->i_x;
            y = 2*i+p_r->i_y;
            //memset(p_dest+ y*p_pic->U_PITCH*2 + x, p_c->cr, p_c->i_num);
//            memset(p_desty+ (y)*p_pic->Y_PITCH + x, p_c->cr, p_c->i_num);
            //memset(p_dest+ y*p_pic->V_PITCH*2 + x, p_c->cb, p_c->i_num);
            //memset(p_destu+ (y)*p_pic->Y_PITCH + x, p_c->cb, p_c->i_num);
            memset(p_desty+ y*p_pic->Y_PITCH + x, p_c->y, p_c->i_num);
  //          memset(p_desty+ 2*y*p_pic->U_PITCH + x, p_c->cr, p_c->i_num);
  //          memset(p_desty+ 2*y*p_pic->V_PITCH + x, p_c->cb, p_c->i_num);
        }
        j += p_c->i_num;
        if(j >= p_im->i_cols[i])
        {
            i++; j=0;
        }
        if( i>= p_im->i_rows) break;
    }
    //idem for the second frame
    p_im = p_r->p_rle_bot; i=0; j=0;
    for(p_c = p_im->p_codes; p_c->p_next != NULL; p_c=p_c->p_next)
    {
        if( p_c->y != 0 && p_c->t < 0x20)
        {
            x = j+ p_r->i_x;
            y = 2*i+1+p_r->i_y;
//            memset(p_desty+ y*p_pic->U_PITCH*2 + x, p_c->cr, p_c->i_num);
//            memset(p_desty+ y*p_pic->V_PITCH*2 + x, p_c->cb, p_c->i_num);
            memset(p_desty+ y*p_pic->Y_PITCH + x, p_c->y, p_c->i_num);
//            memset(p_desty+ 2*y*p_pic->U_PITCH + x, p_c->cr, p_c->i_num);
//            memset(p_desty+ 2*y*p_pic->V_PITCH + x, p_c->cb, p_c->i_num);
        }
        j += p_c->i_num;
        if(j >= p_im->i_cols[i])
        {
            i++; j=0;
        }
        if( i>= p_im->i_rows) break;
    }
}

static void dvbsub_RenderDVBSUB( vout_thread_t *p_vout, picture_t *p_pic,
                                 const subpicture_t *p_spu )
{
    /* If we have changed the language on the fly */

    if( p_spu->p_sys == NULL || p_spu->p_sys->b_obsolete )
    {
        return;
    }

    switch( p_vout->output.i_chroma )
    {
        /* I420 target, no scaling */
        case VLC_FOURCC('I','4','2','2'):
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('I','Y','U','V'):
        case VLC_FOURCC('Y','V','1','2'):
            /* As long as we just use Y info, I422 and YV12 are just equivalent
             * to I420. Remember to change it the day we'll take into account
             * U and V info. */
            RenderI42x( p_vout, p_pic, p_spu );
            break;

        /* RV16 target, scaling */
        case VLC_FOURCC('R','V','1','6'):
            fprintf(stderr, "Not implemented chroma ! RV16)\n");
            /* RenderRV16( p_vout, p_pic, p_spu ); */
            break;

        /* RV32 target, scaling */
        case VLC_FOURCC('R','V','2','4'):
        case VLC_FOURCC('R','V','3','2'):
            fprintf(stderr, "Not implemented chroma ! RV32 \n");
            /* RenderRV32( p_vout, p_pic, p_spu ); */
            break;

        /* NVidia overlay, no scaling */
        case VLC_FOURCC('Y','U','Y','2'):
            RenderYUY2( p_vout, p_pic, p_spu );
            break;

        default:
            msg_Err( p_vout, "unknown chroma, can't render SPU" );
            break;
    }
}

static void dvbsub_Destroy( subpicture_t *p_spu )
{
    free_spu( p_spu );
}

static void dvbsub_render( dvbsub_all_t *dvbsub, vout_thread_t *p_vout )
{
    dvbsub_region_t*     p_region;
    dvbsub_objectdef_t*  p_objectdef;
    dvbsub_object_t*     p_o;
    dvbsub_object_t*     p_object;
    dvbsub_object_t*     p_object_old;
    dvbsub_render_t*     p_render;
    dvbsub_rle_t*        p_c;
    uint8_t i,j;
    j=0;

    /* loop on regions */
    for( i = 0; i < dvbsub->p_page->i_regions_number; i++ )
    {
        p_region = &(dvbsub->p_page->regions[i]);

        /* loop on objects */
        for(p_objectdef = p_region->p_object; p_objectdef != NULL; p_objectdef = p_objectdef->p_next )
        {
            /* Look for the right object */
            p_object = dvbsub->p_objects;
            while((p_object!=NULL) && (p_object->i_id != p_objectdef->i_id))
            {
                p_object = p_object->p_next;
            }

            if(p_object==NULL)
            {
                fprintf(stderr, "Internal DvbSub decoder error\n");
                return;
            }

            /* Allocate the render structure */
            p_render = trox_malloc(sizeof(dvbsub_render_t));
            p_render->i_x = p_region->i_x + p_objectdef->i_xoffset;
            p_render->i_y = p_region->i_y + p_objectdef->i_yoffset;
            p_render->p_rle_top = p_object->topfield;
            p_render->p_rle_bot = p_object->bottomfield;

            // if we did not recieved the CLUT yet
            if ( !dvbsub->p_clut[p_region->i_clut] ) return;

            /* Compute the color datas according to the appropriate CLUT */
            for(p_c=p_render->p_rle_top->p_codes;p_c->p_next!=NULL; p_c=p_c->p_next)
            {
                //TODO We assume here we are working in 4bp
                p_c->y=dvbsub->p_clut[p_region->i_clut]->c_4b[p_c->i_color_code].Y;
                p_c->cr=dvbsub->p_clut[p_region->i_clut]->c_4b[p_c->i_color_code].Cr;
                p_c->cb=dvbsub->p_clut[p_region->i_clut]->c_4b[p_c->i_color_code].Cb;
                p_c->t=dvbsub->p_clut[p_region->i_clut]->c_4b[p_c->i_color_code].T;
            }
            for(p_c=p_render->p_rle_bot->p_codes;p_c->p_next!=NULL; p_c=p_c->p_next)
            {
                //TODO We assume here we are working in 4bp
                p_c->y=dvbsub->p_clut[p_region->i_clut]->c_4b[p_c->i_color_code].Y;
                p_c->cr=dvbsub->p_clut[p_region->i_clut]->c_4b[p_c->i_color_code].Cr;
                p_c->cb=dvbsub->p_clut[p_region->i_clut]->c_4b[p_c->i_color_code].Cb;
                p_c->t=dvbsub->p_clut[p_region->i_clut]->c_4b[p_c->i_color_code].T;
            }


            /* Allocate the subpicture internal data. */
            dvbsub->p_spu[j] = vout_CreateSubPicture( p_vout, MEMORY_SUBPICTURE );
            if( dvbsub->p_spu[j] == NULL )
            {
                fprintf(stderr, "Unable to allocate memory ... skipping\n");
                return;
            }
            /* Set the pf_render callback */
            dvbsub->p_spu[j]->pf_render = dvbsub_RenderDVBSUB;
            dvbsub->p_spu[j]->p_sys =  trox_malloc( sizeof( subpicture_sys_t ));
            dvbsub->p_spu[j]->p_sys->p_data = p_render;
            dvbsub->p_spu[j]->p_sys->b_obsolete=0;
            dvbsub->p_spu[j]->pf_destroy = dvbsub_Destroy;
            dvbsub->p_spu[j]->i_start = dvbsub->i_pts;
            dvbsub->p_spu[j]->i_stop =  dvbsub->p_spu[j]->i_start + dvbsub->p_page->i_timeout*1000000;
            dvbsub->p_spu[j]->b_ephemer = VLC_FALSE;

            // At this stage, we have all we need in p_render
            // We need to free the object
            //Remove this object from the list
            p_object_old = p_object;
            if(p_object == dvbsub->p_objects)
              dvbsub->p_objects = p_object->p_next;
            else
            {
             for(p_o = dvbsub->p_objects; p_o->p_next != p_object; p_o=p_o->p_next);
             p_o->p_next = p_object->p_next;
            }
            free_object(p_object_old);

            vout_DisplaySubPicture( p_vout, dvbsub->p_spu[j] );

            j++;
        }
    }
}

