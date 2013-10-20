/*****************************************************************************
 * dirac.c
 *****************************************************************************
 * Copyright (C) 2008 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: David Flynn <davidf@rd.bbc.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License
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

/* Dirac packetizer, formed of three parts:
 *  1) Bitstream synchroniser (dirac_DoSync)
 *      - Given an arbitrary sequence of bytes, extract whole Dirac Data Units
 *      - Maps timestamps in supplied block_t's to the extracted Data Unit
 *        A time stamp applies to the next Data Unit to commence at, or after
 *        the first byte of the block_t with the timestamp.
 *  2) Encapsulation Unit generation (dirac_BuildEncapsulationUnit)
 *      - Takes multiple well formed Dirac Data Units and forms them into a
 *        single encapsulation unit, suitable for muxing.
 *      - Sorts out any time stamps so that they only apply to pictures.
 *  3) Timestamp generator (dirac_TimeGenPush)
 *      - Many streams will not be correctly timestamped, ie, DTS&PTS for
 *        every encapsulation unit.  Timestamp generator syncs to avaliable
 *        timestamps and produces DTS&PTS for each encapsulation unit.
 *      - For 'Occasional' missing PTS|DTS:
 *          Missing timestamp is generated using interpolation from last
 *          known good values.
 *      - for All PTS missing:
 *          It is assumed that DTS values are fake, and are actually
 *          in the sequence of the PTS values at the output of a decoder.
 *          Fill in PTS by copying from DTS (accounting for reordering,
 *          by simulating reorder buffer); adjust DTS to provide correct
 *          value.  This is how demuxers like AVI work.
 *      - for All DTS missing:
 *          (Ie, PTS is present), reorder buffer is simulated to determine
 *          PTS for each encapsulation unit.
 *      - NB, doesn't handle all pts missing with real dts. (no way to
 *        distinguish from the fake dts case.)
 *
 *  DIRAC_NON_DATED is used to show a block should not have a time stamp
 *  associated (ie, don't interpolate a counter).  At the output, these
 *  blocks get dated with the last used timestamp (or are merged with
 *  another encapsulation unit).
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_block.h>

#include <vlc_bits.h>
#include <vlc_block_helper.h>

#define SANITIZE_PREV_PARSE_OFFSET 1

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin()
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_PACKETIZER )
    set_description( N_("Dirac packetizer") )
    set_capability( "packetizer", 50 )
    set_callbacks( Open, Close )
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct decoder_sys_t
{
    /* sync state */
    block_bytestream_t bytestream;
    int i_state;
    size_t i_offset;
    uint32_t u_last_npo;
    /* recovered timestamp from bytesteram for use
     * by synchroniser: should only get reset by the
     * synchronizer upon a discontinuity sentinel */
    mtime_t i_sync_pts;
    mtime_t i_sync_dts;

    /* build encapsulation unit state */
    block_t *p_eu; /*< Current encapsulation unit being built */
    block_t **pp_eu_last;
    uint32_t u_eu_last_npo; /* last next_parse_offset at input to encapsulation */
    mtime_t i_eu_pts;
    mtime_t i_eu_dts;

    /* timestamp generator state */
    date_t dts; /*< timegen decode clock, increments at picture rate */
    bool b_dts; /*< timegen decode clock valid */

    bool b_pts; /*< timegen presentation time valid */
    mtime_t i_pts; /*< timegen presentation time of picture u_pts_picnum */
    uint32_t u_pts_picnum; /*< picture number of timegen presentation time */

    mtime_t i_pts_offset; /*< maximum time between pts and dts */

    /* p_outqueue is the list of encapsulation units that have been
     * fed to the timegenerator.  the timegenerator stamps them in
     * the order it solves the time.  the main packetizer loop removes
     * completed encapsulation units from the front */
    block_t *p_outqueue;
    block_t **pp_outqueue_last;

    uint32_t u_tg_last_picnum; /*< most recent picturenumber output from RoB */
    bool b_tg_last_picnum; /*< u_tg_last_picnum valid */

    struct dirac_reorder_buffer {
        int u_size_max;
        int u_size;
        struct dirac_reorder_entry {
            struct dirac_reorder_entry *p_next;
            block_t *p_eu;
            uint32_t u_picnum;
        } p_entries[32], *p_head, *p_empty;
    } reorder_buf; /*< reorder buffer, used by timegenerator */

    /* packetizer state */
    mtime_t i_pts_last_out; /*< last output [from packetizer] pts */
    mtime_t i_dts_last_out; /*< last output [from packetizer] dts */

    struct seq_hdr_t {
        uint32_t u_width;
        uint32_t u_height;
        uint32_t u_fps_num;
        uint32_t u_fps_den;
        enum picture_coding_mode_t {
            DIRAC_FRAME_CODING=0,
            DIRAC_FIELD_CODING=1,
        } u_picture_coding_mode;
    } seq_hdr; /*< sequence header */
    bool b_seen_seq_hdr; /* sequence header valid */
    bool b_seen_eos; /* last data unit to be handled was an EOS */
};

typedef struct {
    uint32_t u_next_offset;
    uint32_t u_prev_offset;
    int i_parse_code;
} parse_info_t;

typedef struct {
    /*> next_parse_offset of the final data unit in associated block_t */
    uint32_t u_last_next_offset;
    /*> picture number is invalid if block has flags DIRAC_NON_DATED */
    uint32_t u_picture_number;
} dirac_block_encap_t;

enum {
    NOT_SYNCED=0,
    TRY_SYNC,
    SYNCED,
    SYNCED_INCOMPLETEDU,
};

enum {
    DIRAC_NON_DATED = (1 << BLOCK_FLAG_PRIVATE_SHIFT),
    DIRAC_DISCARD   = (2 << BLOCK_FLAG_PRIVATE_SHIFT),
};

enum {
    DIRAC_DU_IN_EU,
    DIRAC_DU_ENDS_EU,
};

/***
 * Block encapsulation functions.
 * Things are greately simplified by associating some metadata
 * with a block as it passes through the packetizer (saves having
 * to determine it again)
 *
 * unfortunately p_block doesn't have a p_priv, so some fakage
 * needs to happen:
 *   - Create a dummy block that has some extra storage, set up
 *     members to be identical to the actual block
 *   - Store private data there and pointer to orig block
 *   - modify block pointer to point to fake block
 *
 * NB, the add/new functions must not be used to blocks
 * that are referenced in lists, etc., [in this code, this is ok]
 * NB, don't call add on the same block multiple times (will leak)
 *
 * davidf has a patch that reverts this to use a p_priv in block_t.
 */
typedef struct {
    block_t fake;
    block_t *p_orig;
    dirac_block_encap_t *p_dbe;
} fake_block_t;

static dirac_block_encap_t *dirac_RemoveBlockEncap( block_t *p_block )
{
    fake_block_t *p_fake = (fake_block_t *)p_block;
    dirac_block_encap_t *p_dbe = p_fake->p_dbe;

    p_fake->p_dbe = NULL;
    return p_dbe;
}

static void dirac_ReleaseBlockAndEncap( block_t *p_block )
{
    fake_block_t *p_fake = (fake_block_t *)p_block;

    free( dirac_RemoveBlockEncap( p_block ) );
    block_Release( p_fake->p_orig );
    free( p_fake );
}

static void dirac_AddBlockEncap( block_t **pp_block, dirac_block_encap_t *p_dbe )
{
    /* must not fail, fixby: adding a p_priv to block_t */
    fake_block_t *p_fake = xcalloc( 1, sizeof( *p_fake ) );
    block_t *in = *pp_block, *out = &p_fake->fake;

    block_Init( out, in->p_buffer, in->i_buffer );
    out->i_flags = in->i_flags;
    out->i_nb_samples = in->i_nb_samples;
    out->i_pts = in->i_pts;
    out->i_dts = in->i_dts;
    out->i_length = in->i_length;
    out->pf_release = dirac_ReleaseBlockAndEncap;
    p_fake->p_orig = in;
    p_fake->p_dbe = p_dbe;

    *pp_block = out;
}

static dirac_block_encap_t *dirac_NewBlockEncap( block_t **pp_block )
{
    dirac_block_encap_t *p_dbe = calloc( 1, sizeof( *p_dbe ) );
    if( p_dbe ) dirac_AddBlockEncap( pp_block, p_dbe );
    return p_dbe;
}

static dirac_block_encap_t *dirac_GetBlockEncap( block_t *p_block )
{
    return ((fake_block_t *)p_block)->p_dbe;
}

/***
 * General utility funcions
 */

/**
 * given a chain of block_t, allocate and return an array containing
 * pointers to all the blocks. (Acts as a replacement for the old p_prev
 * member of block_t) */
static int block_ChainToArray( block_t *p_block, block_t ***ppp_array)
{
    if( !ppp_array )
        return 0;

    int i_num_blocks;
    block_ChainProperties( p_block, &i_num_blocks, NULL, NULL );

    *ppp_array = calloc( i_num_blocks, sizeof( block_t* ) );
    if( !*ppp_array ) return 0;

    for( int i = 0; i < i_num_blocks; i++ )
    {
        (*ppp_array)[i] = p_block;
        p_block = p_block->p_next;
    }

    return i_num_blocks;
}

/**
 * Destructively find and recover the earliest timestamp from start of
 * bytestream, up to i_length.
 */
static void dirac_RecoverTimestamps ( decoder_t *p_dec, size_t i_length )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block = p_sys->bytestream.p_block;

    /* Find the block with first non-flushed data */
    size_t i_offset = p_sys->bytestream.i_offset;
    for(; p_block != NULL; p_block = p_block->p_next )
    {
        if( i_offset < p_block->i_buffer )
            break;
        i_offset -= p_block->i_buffer;
    }

    i_offset += i_length;
    for(; p_block != NULL; p_block = p_block->p_next )
    {
        if( p_sys->i_sync_pts <= VLC_TS_INVALID && p_sys->i_sync_dts <= VLC_TS_INVALID )
        {
            /* oldest timestamp wins */
            p_sys->i_sync_pts = p_block->i_pts;
            p_sys->i_sync_dts = p_block->i_dts;
        }
        /* clear timestamps -- more than one data unit can come from a block */
        p_block->i_flags = 0;
        p_block->i_pts = p_block->i_dts = VLC_TS_INVALID;
        if( i_offset < p_block->i_buffer )
            break;
        i_offset -= p_block->i_buffer;
    }
}

/* backdate the list [p_block .. p_block->p_next where p_next == p_last] */
static void dirac_BackdateDTS( block_t *p_block, block_t *p_last, date_t *p_dts )
{
    /* Transverse p_last backwards.  (no p_prev anymore) */
    block_t **pp_array = NULL;
    int n = block_ChainToArray( p_block, &pp_array );
    while( n ) if( pp_array[--n] == p_last ) break;
    /* want to start at p_last->p_prev */
    while( n-- )
    {
        if( pp_array[n]->i_flags & DIRAC_NON_DATED )
            continue;
        if( pp_array[n]->i_dts <= VLC_TS_INVALID )
            pp_array[n]->i_dts = date_Decrement( p_dts, 1 );
    }
    free( pp_array );
}

/* backdate the list [p_block .. p_block->p_next where p_next == p_last] */
static void dirac_BackdatePTS( block_t *p_block, block_t *p_last, date_t *p_pts, uint32_t u_pts_picnum )
{
    /* Transverse p_last backwards.  (no p_prev anymore) */
    block_t **pp_array = NULL;
    int n = block_ChainToArray( p_block, &pp_array );
    while( n ) if( pp_array[--n] == p_last ) break;
    /* want to start at p_last->p_prev */
    while( n-- )
    {
        if( pp_array[n]->i_flags & DIRAC_NON_DATED )
            continue;
        if( pp_array[n]->i_dts > VLC_TS_INVALID )
            continue;
        dirac_block_encap_t *dbe = dirac_GetBlockEncap( pp_array[n] );
        int32_t u_pic_num = dbe ? dbe->u_picture_number : 0;
        int32_t i_dist = u_pic_num - u_pts_picnum;
        date_t pts = *p_pts;
        if( i_dist >= 0 )
            pp_array[n]->i_pts = date_Increment( &pts, i_dist );
        else
            pp_array[n]->i_pts = date_Decrement( &pts, -i_dist );
    }
    free( pp_array );
}

/***
 * Dirac spec defined relations
 */

static bool dirac_isEOS( uint8_t u_parse_code ) { return 0x10 == u_parse_code; }
static bool dirac_isSeqHdr( uint8_t u_parse_code ) { return 0 == u_parse_code; }
static bool dirac_isPicture( uint8_t u_parse_code ) { return 0x08 & u_parse_code; }
static int dirac_numRefs( uint8_t u_parse_code ) { return 0x3 & u_parse_code; }

static inline bool dirac_PictureNbeforeM( uint32_t u_n, uint32_t u_m )
{
    /* specified as: u_n occurs before u_m if:
     *   (u_m - u_n) mod (1<<32) < D */
    return (uint32_t)(u_m - u_n) < (1u<<31);
}

/***
 * Reorder buffer model
 */

static void dirac_ReorderInit( struct dirac_reorder_buffer *p_rb )
{
    memset( p_rb, 0, sizeof(*p_rb) );
    p_rb->u_size_max = 2;
    p_rb->p_empty = p_rb->p_entries;
    p_rb->p_entries[31].p_next = NULL;

    for( int i = 0; i < 31; i++ )
        p_rb->p_entries[i].p_next = &p_rb->p_entries[i+1];
}

/* simulate the dirac picture reorder buffer */
static block_t *dirac_Reorder( decoder_t *p_dec, block_t *p_block_in, uint32_t u_picnum )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( !p_sys->reorder_buf.u_size_max )
        /* reorder buffer disabled */
        return p_block_in;

    /* Modeling the reorder buffer:
     * 1. If the reorder buffer is not full, insert picture for reordering.
     *    No picture is output by the system this picture period
     * 2. If the reorder buffer is full:
     *    a. The picture decoded this period (u_picnum) bypasses the reorder
     *       buffer if it has a lower picture number than any entry in the
     *       reorder buffer. This picture is output by the system.
     *    b. Otherwise, the lowest picture number in the reorder buffer is
     *       removed from the buffer and output by the system.  The current
     *       decoded picture (u_picnum) is inserted into the reorder buffer
     */

    block_t *p_block = NULL;
    /* Determine if the picture needs to be inserted */
    if( p_sys->reorder_buf.u_size == p_sys->reorder_buf.u_size_max )
    {
        /* (2) reorder buffer is full */
        if( !p_sys->reorder_buf.u_size_max ||
            dirac_PictureNbeforeM( u_picnum, p_sys->reorder_buf.p_head->u_picnum ) )
        {
            /* (2a) current picture is first in order */
            return p_block_in;
        }

        /* (2b) extract the youngest picture in the buffer */
        p_block = p_sys->reorder_buf.p_head->p_eu;

        struct dirac_reorder_entry *p_tmp = p_sys->reorder_buf.p_head;
        p_sys->reorder_buf.p_head = p_tmp->p_next;
        p_tmp->p_next = p_sys->reorder_buf.p_empty;
        p_sys->reorder_buf.p_empty = p_tmp;

        p_sys->reorder_buf.u_size--;
    }

    /* (1) and (2b) both require u_picnum to be inserted */
    struct dirac_reorder_entry *p_current = p_sys->reorder_buf.p_empty;
    p_sys->reorder_buf.p_empty = p_current->p_next;
    p_sys->reorder_buf.u_size++;

    /* insertion sort to keep p_head always sorted, earliest first */
    struct dirac_reorder_entry **pp_at = &p_sys->reorder_buf.p_head;
    for( ; *pp_at; pp_at = &(*pp_at)->p_next )
        if( dirac_PictureNbeforeM( u_picnum, (*pp_at)->u_picnum ) )
            break;

    p_current->u_picnum = u_picnum;
    p_current->p_eu = p_block_in;
    p_current->p_next = *pp_at;
    *pp_at = p_current;

    return p_block;
}

/***
 * bytestream parsing and unmarshalling functions
 */

static bool dirac_UnpackParseInfo( parse_info_t *p_pi, block_bytestream_t *p_bs,
                                   size_t u_offset )
{
    uint8_t p_d[13];
    if( VLC_SUCCESS != block_PeekOffsetBytes( p_bs, u_offset, p_d, 13 ) )
        return false;

    if( p_d[0] != 'B' || p_d[1] != 'B' || p_d[2] != 'C' || p_d[3] != 'D' )
        return false;

    p_pi->i_parse_code = p_d[4];
    p_pi->u_next_offset = p_d[5] << 24 | p_d[6] << 16 | p_d[7] << 8 | p_d[8];
    p_pi->u_prev_offset = p_d[9] << 24 | p_d[10] << 16 | p_d[11] << 8 | p_d[12];
    return true;
}

static uint32_t dirac_uint( bs_t *p_bs )
{
    uint32_t u_count = 0, u_value = 0;
    while( !bs_eof( p_bs ) && !bs_read( p_bs, 1 ) )
    {
        u_count++;
        u_value <<= 1;
        u_value |= bs_read( p_bs, 1 );
    }
    return (1 << u_count) - 1 + u_value;
}

static int dirac_bool( bs_t *p_bs )
{
    return bs_read( p_bs, 1 );
}

/* read in useful bits from sequence header */
static bool dirac_UnpackSeqHdr( struct seq_hdr_t *p_sh, block_t *p_block )
{
    bs_t bs;
    bs_init( &bs, p_block->p_buffer, p_block->i_buffer );
    bs_skip( &bs, 13*8 ); /* parse_info_header */
    dirac_uint( &bs ); /* major_version */
    dirac_uint( &bs ); /* minor_version */
    dirac_uint( &bs ); /* profile */
    dirac_uint( &bs ); /* level */

    uint32_t u_video_format = dirac_uint( &bs ); /* index */
    if( u_video_format > 20 )
    {
        /* don't know how to parse this header */
        return false;
    }

    static const struct {
        uint32_t u_w, u_h;
    } dirac_size_tbl[] = {
        {640,480}, {176,120}, {176,144}, {352,240}, {352,288}, {704,480},
        {704,576}, {720,480}, {720,576}, {1280,720}, {1280,720}, {1920,1080},
        {1920,1080}, {1920,1080}, {1920,1080}, {2048,1080}, {4096,2160},
        {3840,2160}, {3840,2160}, {7680,4320}, {7680,4320},
    };

    p_sh->u_width = dirac_size_tbl[u_video_format].u_w;
    p_sh->u_height = dirac_size_tbl[u_video_format].u_h;
    if( dirac_bool( &bs ) )
    {
        p_sh->u_width = dirac_uint( &bs ); /* frame_width */
        p_sh->u_height = dirac_uint( &bs ); /* frame_height */
    }

    if( dirac_bool( &bs ) )
    {
        dirac_uint( &bs ); /* chroma_format */
    }

    if( dirac_bool( &bs ) )
    {
        dirac_uint( &bs ); /* scan_format */
    }

    static const struct {
        uint32_t u_n /* numerator */, u_d /* denominator */;
    } dirac_frate_tbl[] = { /* table 10.3 */
        {1, 1}, /* this value is not used */
        {24000,1001}, {24,1}, {25,1}, {30000,1001}, {30,1},
        {50,1}, {60000,1001}, {60,1}, {15000,1001}, {25,2},
    };

    const unsigned dirac_frate_tbl_size =
        sizeof( dirac_frate_tbl ) / sizeof( *dirac_frate_tbl );

    static const uint32_t dirac_vidfmt_frate[] = { /* table C.1 */
        1, 9, 10, 9, 10, 9, 10, 4, 3, 7, 6, 4, 3, 7, 6, 2, 2, 7, 6, 7, 6,
    };

    p_sh->u_fps_num = dirac_frate_tbl[dirac_vidfmt_frate[u_video_format]].u_n;
    p_sh->u_fps_den = dirac_frate_tbl[dirac_vidfmt_frate[u_video_format]].u_d;
    if( dirac_bool( &bs ) )
    {
        uint32_t frame_rate_index = dirac_uint( &bs );
        if( frame_rate_index >= dirac_frate_tbl_size )
        {
            /* invalid header */
            return false;
        }
        p_sh->u_fps_num = dirac_frate_tbl[frame_rate_index].u_n;
        p_sh->u_fps_den = dirac_frate_tbl[frame_rate_index].u_d;
        if( frame_rate_index == 0 )
        {
            p_sh->u_fps_num = dirac_uint( &bs ); /* frame_rate_numerator */
            p_sh->u_fps_den = dirac_uint( &bs ); /* frame_rate_denominator */
        }
    }

    /* must have a valid framerate */
    if( !p_sh->u_fps_num || !p_sh->u_fps_den )
        return false;

    if( dirac_bool( &bs ) )
    {
        uint32_t par_index = dirac_uint( &bs );
        if( !par_index )
        {
            dirac_uint( &bs ); /* par_num */
            dirac_uint( &bs ); /* par_den */
        }
    }

    if( dirac_bool( &bs ) )
    {
        dirac_uint( &bs ); /* clean_width */
        dirac_uint( &bs ); /* clean_height */
        dirac_uint( &bs ); /* clean_left_offset */
        dirac_uint( &bs ); /* clean_top_offset */
    }

    if( dirac_bool( &bs ) )
    {
        uint32_t signal_range_index = dirac_uint( &bs );
        if( !signal_range_index )
        {
            dirac_uint( &bs ); /* luma_offset */
            dirac_uint( &bs ); /* luma_excursion */
            dirac_uint( &bs ); /* chroma_offset */
            dirac_uint( &bs ); /* chroma_excursion */
        }
    }

    if( dirac_bool( &bs ) )
    {
        uint32_t colour_spec_index = dirac_uint( &bs );
        if( !colour_spec_index )
        {
            if( dirac_bool( &bs ) )
            {
                dirac_uint( &bs ); /* colour_primaries_index */
            }
            if( dirac_bool( &bs ) )
            {
                dirac_uint( &bs ); /* colour_matrix_index */
            }
            if( dirac_bool( &bs ) )
            {
                dirac_uint( &bs ); /* transfer_function_index */
            }
        }
    }

    p_sh->u_picture_coding_mode = dirac_uint( &bs );

    return true;
}

/***
 * Data Unit marshalling functions
 */

static block_t *dirac_EmitEOS( decoder_t *p_dec, uint32_t i_prev_parse_offset )
{
    const uint8_t p_eos[] = { 'B','B','C','D',0x10,0,0,0,13,0,0,0,0 };
    block_t *p_block = block_Alloc( 13 );
    if( !p_block )
        return NULL;
    memcpy( p_block->p_buffer, p_eos, 13 );

    SetDWBE( p_block->p_buffer + 9, i_prev_parse_offset );

    p_block->i_flags = DIRAC_NON_DATED;

    return p_block;

    (void) p_dec;
}

/***
 * Bytestream synchronizer
 * maps [Bytes] -> DataUnit
 */
static block_t *dirac_DoSync( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    parse_info_t pu;

    static const uint8_t p_parsecode[4] = {'B','B','C','D'};
    do {
        switch( p_sys->i_state )
        {
        case NOT_SYNCED:
        {
            if( VLC_SUCCESS !=
                block_FindStartcodeFromOffset( &p_sys->bytestream, &p_sys->i_offset, p_parsecode, 4 ) )
            {
                /* p_sys->i_offset will have been set to:
                 *   end of bytestream - amount of prefix found
                 * can resume search from this point when more data arrives */
                return NULL;
            }
            /* candidate parse_code_prefix has been found at p_sys->i_offset */
            if( VLC_SUCCESS != block_PeekOffsetBytes( &p_sys->bytestream, p_sys->i_offset + 12, NULL, 0 ) )
            {
                /* insufficient data has been accumulated to fully extract
                 * a parse_info header. exit for now in the hope of more
                 * data later to retry at exactly the same point */
                return NULL;
            }
            p_sys->i_state = TRY_SYNC;
            break; /* candidate found, try to sync */
        }
        case SYNCED: /* -> TRY_SYNC | NOT_SYNCED */
            /* sanity: can only reach this after having extracted a DU,
             * which causes data to be consumed and local i_offset to be reset */
            assert( p_sys->i_offset == 0 );
            if( VLC_SUCCESS != block_PeekOffsetBytes( &p_sys->bytestream, 12, NULL, 0 ) )
            {
                /* insufficient data has been accumulated to fully extract
                 * a parse_info header, retry later */
                return NULL;
            }
            if( !dirac_UnpackParseInfo( &pu, &p_sys->bytestream, 0 )
             || !pu.u_next_offset || (p_sys->u_last_npo != pu.u_prev_offset) )
            {
                /* !a: not a valid parse info.
                 * !pu.u_next_offset: don't know the length of the data unit
                 *                    search for the next one that points back
                 *                    to this one to determine length.
                 * (p_sys->u_last_npo != pu.u_prev_offset): some desync
                 */
                p_sys->i_state = NOT_SYNCED;
                break;
            }
            if( pu.u_next_offset > 1024*1024 )
            {
                /* sanity check for erronious hugs next_parse_offsets
                 * (eg, 2^32-1) that would cause a very long wait
                 * and large space consumption: fall back to try sync */
                p_sys->i_state = TRY_SYNC;
                break;
            }
            /* check that the start of the next data unit is avaliable */
            if( VLC_SUCCESS !=
                block_PeekOffsetBytes( &p_sys->bytestream, pu.u_next_offset + 12, NULL, 0 ) )
            {
                return NULL; /* retry later */
            }
            /* attempt to synchronise backwards from pu.u_next_offset */
            p_sys->i_offset = pu.u_next_offset;
            /* fall through */
        case TRY_SYNC: /* -> SYNCED | NOT_SYNCED */
        {
            if( !p_sys->i_offset )
                goto sync_fail; /* if a is at start of bytestream, b can't be in buffer */

            parse_info_t pu_a;
            bool a = dirac_UnpackParseInfo( &pu_a, &p_sys->bytestream, p_sys->i_offset );
            if( !a || (pu_a.u_prev_offset > p_sys->i_offset) )
                goto sync_fail; /* b lies beyond start of bytestream: can't sync */

            if( !pu_a.u_prev_offset )
            {
                if( p_sys->i_state == TRY_SYNC )
                {
                    goto sync_fail; /* can't find different pu_b from pu_a */
                }
                /* state == SYNCED: already know where pu_b is.
                 * pu_a has probably been inserted by something that doesn't
                 * know what the last next_parse_offset was */
                pu_a.u_prev_offset = pu.u_next_offset;
            }

            parse_info_t *pu_b = &pu;
            bool b = dirac_UnpackParseInfo( pu_b, &p_sys->bytestream, p_sys->i_offset - pu_a.u_prev_offset );
            if( !b || (pu_b->u_next_offset && pu_a.u_prev_offset != pu_b->u_next_offset) )
            {
                /* if pu_b->u_next_offset = 0, have to assume we've synced, ie,
                 * just rely on finding a valid pu_b from pu_a. */
                goto sync_fail;
            }
            p_sys->u_last_npo = pu_b->u_next_offset;
            if( !pu_b->u_next_offset ) pu_b->u_next_offset = pu_a.u_prev_offset;
            /* offset was pointing at pu_a, rewind to point at pu_b */
            p_sys->i_offset -= pu_a.u_prev_offset;
            p_sys->i_state = SYNCED;
            break;
        }
sync_fail:
            if( p_sys->i_state == SYNCED ) p_sys->i_offset = 0;
            p_sys->i_offset++;
            p_sys->i_state = NOT_SYNCED;
            break; /* find somewhere else to try again */
        default:;
        }
    } while( SYNCED != p_sys->i_state );

    /*
     * synced, attempt to extract a data unit
     */

    /* recover any timestamps from the data that is about to be flushed */
    dirac_RecoverTimestamps( p_dec, p_sys->i_offset );

    /* flush everything up to the start of the DU */
    block_SkipBytes( &p_sys->bytestream, p_sys->i_offset );
    block_BytestreamFlush( &p_sys->bytestream );
    p_sys->i_offset = 0;

    /* setup the data unit buffer */
    block_t *p_block = block_Alloc( pu.u_next_offset );
    if( !p_block )
        return NULL;

    p_block->i_pts = p_sys->i_sync_pts;
    p_block->i_dts = p_sys->i_sync_dts;
    p_sys->i_sync_pts = p_sys->i_sync_dts = VLC_TS_INVALID;

    /* recover any new timestamps from the data that is about to be consumed */
    dirac_RecoverTimestamps( p_dec, p_sys->i_offset );

    block_GetBytes( &p_sys->bytestream, p_block->p_buffer, p_block->i_buffer );

    /* save parse offset in private area for later use */
    dirac_block_encap_t *p_dbe = dirac_NewBlockEncap( &p_block );
    if( p_dbe ) p_dbe->u_last_next_offset = pu.u_next_offset;

    return p_block;
}

/***
 * Packet (Data Unit) inspection, learns parameters from sequence
 * headers, sets up flags, drops unwanted data units, sets
 * encapsulation unit termination policy
 */
static int dirac_InspectDataUnit( decoder_t *p_dec, block_t **pp_block, block_t *p_eu )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block = *pp_block;
    uint8_t u_parse_code = p_block->p_buffer[4];

    if( dirac_isEOS( u_parse_code ) )
    {
        if( p_sys->b_seen_eos )
        {
            /* remove duplicate EOS packets */
            block_Release( p_block );
            *pp_block = NULL;
            return DIRAC_DU_IN_EU;
        }
        /* p_block is an EOS packet */
        p_eu->i_flags |= BLOCK_FLAG_END_OF_SEQUENCE;
        /* for the moment, let this end an encapsulation unit */
        /* seeing an eos packet requires a flush of the packetizer
         * this is detected by the caller of this function */
        p_sys->b_seen_seq_hdr = false;
        p_sys->b_seen_eos = true;
        return DIRAC_DU_ENDS_EU;
#if 0
        /* let anything down streem know too */
        /*
        Actually, this is a bad idea:
         - It sets the discontinuity for every dirac EOS packet
           which doesnt imply a time discontinuity.
         - When the synchronizer detects a real discontinuity, it
           should copy the flags through.
        p_eu->i_flags |= BLOCK_FLAG_DISCONTINUITY;
        */
#endif
    }
    p_sys->b_seen_eos = false;

    if( dirac_isPicture( u_parse_code ) )
    {
        /* timestamps apply to pictures only */
        p_eu->i_dts = p_sys->i_eu_dts;
        p_eu->i_pts = p_sys->i_eu_pts;
        p_sys->i_eu_dts = p_sys->i_eu_pts = VLC_TS_INVALID;

        if( !p_sys->b_seen_seq_hdr )
        {
            /* can't timestamp in this case, discard later
             * so that the timestamps aren't lost */
            p_eu->i_flags |= DIRAC_DISCARD;
        }
        /* p_block is a picture -- it ends the 'encapsulation unit' */
        if( dirac_numRefs( u_parse_code ) )
        {
            /* if this picture is not an I frame, ensure that the
             * random access point flags are not set */
            p_eu->i_flags &= ~BLOCK_FLAG_TYPE_I;
        }
        dirac_block_encap_t *p_dbe = dirac_GetBlockEncap( p_block );
        if( p_dbe && p_block->i_buffer > 13+4 )
        {
            /* record the picture number to save the time gen functions
             * from having to inspect the data for it */
            p_dbe->u_picture_number = GetDWBE( p_block->p_buffer + 13 );
        }
        return DIRAC_DU_ENDS_EU;
    }

    if( dirac_isSeqHdr( u_parse_code ) )
    {
        if( !dirac_UnpackSeqHdr( &p_sys->seq_hdr, p_block ) )
        {
            /* couldn't parse the sequence header, just ignore it */
            return DIRAC_DU_IN_EU;
        }
        p_sys->b_seen_seq_hdr = true;

       /* a sequence header followed by an I frame is a random
        * access point; assume that this is the case */
        p_eu->i_flags |= BLOCK_FLAG_TYPE_I;

        es_format_t *p_es = &p_dec->fmt_out;

        p_es->video.i_width  = p_sys->seq_hdr.u_width;
        p_es->video.i_height = p_sys->seq_hdr.u_height;

        vlc_ureduce( &p_es->video.i_frame_rate, &p_es->video.i_frame_rate_base
                   , p_sys->seq_hdr.u_fps_num, p_sys->seq_hdr.u_fps_den, 0 );

        /* when field coding, dts needs to be incremented in terms of field periods */
        unsigned u_pics_per_sec = p_sys->seq_hdr.u_fps_num;
        if( p_sys->seq_hdr.u_picture_coding_mode == DIRAC_FIELD_CODING )
        {
            u_pics_per_sec *= 2;
        }
        date_Change( &p_sys->dts, u_pics_per_sec, p_sys->seq_hdr.u_fps_den );

        /* TODO: set p_sys->reorder_buf.u_size_max */
        p_sys->i_pts_offset = p_sys->reorder_buf.u_size_max
                            * 1000000
                            * p_es->video.i_frame_rate_base / p_es->video.i_frame_rate + 1;

        /* stash a copy of the seqhdr
         *  - required for ogg muxing
         *  - useful for error checking
         *  - it isn't allowed to change until an eos */
        free( p_es->p_extra );
        p_es->p_extra = calloc( 1, p_block->i_buffer + 13 );
        if( !p_es->p_extra )
        {
            p_es->i_extra = 0;
            return DIRAC_DU_IN_EU;
        }
        p_es->i_extra = p_block->i_buffer;
        memcpy( p_es->p_extra, p_block->p_buffer, p_block->i_buffer );
        /* append EOS as per Ogg guidelines */
        p_block = dirac_EmitEOS( p_dec, p_block->i_buffer );
        if( p_block )
        {
            memcpy( (uint8_t*)p_es->p_extra + p_es->i_extra, p_block->p_buffer, 13 );
            p_es->i_extra += 13;
        }

        return DIRAC_DU_IN_EU;
    }

    /* doesn't end an encapsulation unit */
    return DIRAC_DU_IN_EU;
}

/***
 * Encapsulation (packetization) suitable for all muxing standards
 * maps [DataUnit] -> EncapsulationUnit
 */
static block_t *dirac_BuildEncapsulationUnit( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    assert(p_block->i_buffer >= 13 && 0x42424344 == GetDWBE( p_block->p_buffer ));

    if( p_sys->i_eu_pts <= VLC_TS_INVALID && p_sys->i_eu_dts <= VLC_TS_INVALID )
    {
        /* earliest block with pts/dts gets to set the pts/dts for the dated
         * encapsulation unit as a whole */
        /* NB, the 'earliest block' criteria is aribtary */
        if( p_block->i_pts > VLC_TS_INVALID || p_block->i_dts > VLC_TS_INVALID )
        {
            p_sys->i_eu_pts = p_block->i_pts;
            p_sys->i_eu_dts = p_block->i_dts;
        }
    }

    /* inpectdataunit also updates flags for the EU.
     *  - if this is the first block in the EU, then it hasn't been added
     *    to the chain yet (so, p_block will become the front of the chain
     *  - otherwise, use the flags of the chain (first block) */
    block_t *p_eu = p_sys->p_eu ? p_sys->p_eu : p_block;
    int i_block = dirac_InspectDataUnit( p_dec, &p_block, p_eu);

    if( !p_block )
    {
        /* block has been discarded during inspection */
        /* becareful, don't discard anything that is dated,
         * as this needs to go into the timegen loop.  set
         * the DIRAC_DISCARD block flag, and it'll be dropped
         * at output time */
        return NULL;
    }

    block_ChainLastAppend( &p_sys->pp_eu_last, p_block );

    dirac_block_encap_t *p_dbe = dirac_GetBlockEncap( p_block );
#ifdef SANITIZE_PREV_PARSE_OFFSET
    /* fixup prev_parse_offset to point to the last data unit
     * to arrive */
    if( p_dbe )
    {
        SetDWBE( p_block->p_buffer + 9, p_sys->u_eu_last_npo );
        p_sys->u_eu_last_npo = p_dbe->u_last_next_offset;
    }
#endif

    if( i_block != DIRAC_DU_ENDS_EU )
    {
        /* encapsulation unit not ended */
        return NULL;
    }

    /* gather up encapsulation unit, reassociating the final
     * private state with the gathered block */
    block_t *p_eu_last = (block_t*) p_sys->pp_eu_last - offsetof( block_t, p_next );
    p_dbe = dirac_RemoveBlockEncap( p_eu_last );

    uint8_t u_parse_code = p_block->p_buffer[4];

    /* gather up the encapsulation unit */
    p_block = block_ChainGather( p_sys->p_eu );
    assert( p_block ); /* block_ChainGather doesn't define when it frees chain */

    p_block->i_flags |= DIRAC_NON_DATED;
    if( p_dbe )
    {
        dirac_AddBlockEncap( &p_block, p_dbe );
        if( dirac_isPicture( u_parse_code ) ) p_block->i_flags &= ~DIRAC_NON_DATED;
    }
    p_sys->p_eu = NULL;
    p_sys->pp_eu_last = &p_sys->p_eu;
    return p_block;
}

/**
 * dirac_TimeGenPush:
 * @p_dec: vlc object
 * @p_block_in: whole encapsulation unit to generate timestamps for
 *
 * Returns:
 *  0: everything ok
 *  1: EOS occurred, please flush and reset
 *  2: picture number discontinuity, please flush and reset
 */
static int dirac_TimeGenPush( decoder_t *p_dec, block_t *p_block_in )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    dirac_block_encap_t *p_dbe;

    if( p_block_in->i_flags & BLOCK_FLAG_END_OF_SEQUENCE )
    {
        /* NB, this test occurs after the timegen push, so as to
         * push the block into the output queue */
        return 1;
    }

    if( p_block_in->i_flags & DIRAC_NON_DATED )
    {
        /* no picture found, which means p_block_in is a non-dated EU,
         * do not try and put a date on it */
        return 0;
    }

    p_dbe = dirac_GetBlockEncap( p_block_in );
    uint32_t u_picnum = p_dbe ? p_dbe->u_picture_number : 0;
    /*
     * Simple DTS regeneration:
     *  - DTS values linearly increase in stream order.
     *  - Every time a DTS occurs at the input, sync to it
     *    - If this is the first DTS seen, backdate all the previous ones that are undated
     *  - If a DTS is missing, guess that it increases by one picture period
     *  - If never seen DTS, don't do anything
     */
    /*
     * Simple PTS regeneration
     *  - PTS values do not linearly increase in stream order.
     *  - Every time a PTS occurs at the input, sync to it and record picture number
     *  - If a PTS is missing, guess that it differs by the product of picture
     *    period and difference between picture number of sync point and current picture
     *  - If this is the first PTS seen, backdate all previous ones that are undated
     *  - If never seen PTS, don't do anything
     */
    /*
     * Stage 1, sync to input timestamps, backdate timestamps for old
     * EUs that are in the outqueue with missing dates
     */
    if( p_block_in->i_dts > VLC_TS_INVALID )
    do {
        /* if timestamps exist, sync to them */
        if( p_sys->b_dts )
            break;
        /* first dts seen, backdate any packets in outqueue */
        p_sys->b_dts = true;
        date_t dts = p_sys->dts;
        dirac_BackdateDTS( p_sys->p_outqueue, p_block_in, &dts );
    } while( 0 );

    if( p_block_in->i_pts > VLC_TS_INVALID )
    do {
        /* if timestamps exist, sync to them */
        p_sys->u_pts_picnum = u_picnum;
        p_sys->i_pts = p_block_in->i_pts;
        if( p_sys->b_pts )
            break;
        /* first pts seen, backdate any packets in outqueue */
        p_sys->b_pts = true;
        date_t pts = p_sys->dts;
        date_Set( &pts, p_sys->i_pts );
        dirac_BackdatePTS( p_sys->p_outqueue, p_block_in, &pts, p_sys->u_pts_picnum );
    } while( 0 );

    /*
     * Stage 2, don't attempt to forwards interpolate timestamps for
     * blocks if the picture rates aren't known
     */
    if( !p_sys->b_seen_seq_hdr )
    {
        return 0;
    }

    /*
     * Stage 3, for block_in, interpolate any missing timestamps
     */
    if( p_sys->b_dts && p_block_in->i_dts <= VLC_TS_INVALID )
    {
        /* dts has previously been seen, but not this time, interpolate */
        p_block_in->i_dts = date_Increment( &p_sys->dts, 1 );
    }

    if( p_sys->b_pts && p_block_in->i_pts <= VLC_TS_INVALID )
    {
        /* pts has previously been seen, but not this time, interpolate */
        date_t pts = p_sys->dts;
        date_Set( &pts, p_sys->i_pts );
        int32_t i_dist = u_picnum - p_sys->u_pts_picnum;
        if( i_dist >= 0 )
            p_block_in->i_pts = date_Increment( &pts, i_dist );
        else
            p_block_in->i_pts = date_Decrement( &pts, -i_dist );
    }

    /* If pts and dts have been seen, there is no need to simulate operation
     * of the decoder reorder buffer */
    /* If neither have been seen, there is little point in simulating */
    if( p_sys->b_dts == p_sys->b_pts )
        return 0;

    /* model the reorder buffer */
    block_t *p_block = dirac_Reorder( p_dec, p_block_in, u_picnum );
    if( !p_block )
        return 0;

    /* A future ehancement is to stop modeling the reorder buffer as soon as
     * the first packet is output -- interpolate the past and freewheel for
     * the future */

    p_dbe = dirac_GetBlockEncap( p_block );
    u_picnum = p_dbe ? p_dbe->u_picture_number : 0;
    if( p_sys->b_tg_last_picnum )
    {
        if( dirac_PictureNbeforeM( u_picnum, p_sys->u_tg_last_picnum ) )
        {
            msg_Warn( p_dec, "stream jumped? %d < %d: resetting"
                    , u_picnum, p_sys->u_tg_last_picnum );
            /* pictures only emerge from the reorder buffer in sequence
             * if a stream suddenly jumped backwards without a signaling
             * a discontinuity, some pictures will get stuck in the RoB.
             * flush the RoB. */
            /* this could be a bit less indiscriminate */
            p_dbe = dirac_GetBlockEncap( p_sys->p_outqueue );
            uint32_t u_prev_parse_offset = p_dbe ? p_dbe->u_last_next_offset : 0;
            block_ChainRelease( p_sys->p_outqueue );
            p_sys->p_outqueue = dirac_EmitEOS( p_dec, u_prev_parse_offset );
            if( p_sys->p_outqueue )
                p_sys->p_outqueue->i_flags = BLOCK_FLAG_DISCONTINUITY | DIRAC_NON_DATED;
            /* return 2, so as not to reset the b_dts flags -- needed if
             * using the rawdirac demuxer with broken stream */
            return 2;
        }
    }
    p_sys->b_tg_last_picnum = true;
    p_sys->u_tg_last_picnum = u_picnum;

    return 0;
}

/*****************************************************************************
 * Packetize: form dated encapsulation units from anything
 *****************************************************************************/
static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block = NULL;
    int i_flushing = 0;

    if( pp_block && *pp_block )
    {
        p_block = *pp_block;
        *pp_block = NULL;

        if( p_block->i_flags & BLOCK_FLAG_DISCONTINUITY )
        {
            /* pre-emptively insert an EOS at a discontinuity, protects
             * any decoders from any sudden changes */
            block_Release( p_block );
            p_block = dirac_EmitEOS( p_dec, 0 );
            if( p_block )
            {
                p_block->p_next = dirac_EmitEOS( p_dec, 13 );
                /* need two EOS to ensure it gets detected by synchro
                 * duplicates get discarded in forming encapsulation unit */
            }
        }
        else if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
        {
            /* silently discard corruption sentinels,
             * synchronizer will then discard affected data units.
             * do not produce an EOS data unit as this is very
             * disruptive to the stream (and may make a larger error). */
            block_Release( p_block );
            p_block = NULL;
        }
        if( p_block )
            block_BytestreamPush( &p_sys->bytestream, p_block );
    }

    /* form as many encapsulation units as possible, give up
     * when the synchronizer runs out of input data */
    while( ( p_block = dirac_DoSync( p_dec ) ) )
    {
        p_block = dirac_BuildEncapsulationUnit( p_dec, p_block );
        if( !p_block )
            continue;
        /* add to tail of output queue (ie, not reordered) */
        block_ChainLastAppend( &p_sys->pp_outqueue_last, p_block );
        /* insert encapsulation unit into timestamp generator
         * which then calculates some timestamps if required */
        i_flushing = dirac_TimeGenPush( p_dec, p_block );
        if( i_flushing )
            break;
    }

    block_t *p_output = NULL;
    block_t **pp_output = &p_output;

    /* extract all the dated packets from the head of the output queue */
    /* explicitly nondated packets repeat the previous timestamps to
     * stop vlc discarding them */
    while( (p_block = p_sys->p_outqueue) )
    {
        if( p_block->i_flags & DIRAC_DISCARD )
        {
            p_sys->p_outqueue = p_block->p_next;
            p_block->p_next = NULL;
            block_Release( p_block );
            continue;
        }

        if( i_flushing || p_block->i_flags & DIRAC_NON_DATED )
        {
            p_block->i_dts = p_sys->i_dts_last_out;
            p_block->i_pts = p_sys->i_pts_last_out;
        }
        else if( p_block->i_pts <= VLC_TS_INVALID ) break;
        else if( p_block->i_dts <= VLC_TS_INVALID ) break;

        p_sys->i_dts_last_out = p_block->i_dts;
        p_sys->i_pts_last_out = p_block->i_pts;

        p_sys->p_outqueue = p_block->p_next;
        p_block->p_next = NULL;
        /* clear any flags we set */
        p_block->i_flags &= ~BLOCK_FLAG_PRIVATE_MASK;
        block_ChainLastAppend( &pp_output, p_block );

        mtime_t i_delay = p_block->i_pts - p_block->i_dts;
        if( i_delay < 0 )
            msg_Err( p_dec, "pts - dts is negative(%"PRId64"): incorrect RoB size", i_delay );
    }

    if( i_flushing )
    {
        p_sys->i_eu_dts = p_sys->i_eu_pts = VLC_TS_INVALID;

        /* reset timegen state (except synchronizer) */
        p_sys->b_seen_seq_hdr = false;
        if( i_flushing < 2 )
        {
            /* this state isn't safe to loose if there was
             * an unsignalled discontinuity */
            p_sys->b_pts = p_sys->b_dts = false;
        }
        p_sys->b_tg_last_picnum = false;
        dirac_ReorderInit( &p_sys->reorder_buf );

        assert( p_sys->p_outqueue == NULL );
    }

    /* perform sanity check:
     *  if there were a block at the front of outqueue that never
     *  satisfied the extraction criteria, but all blocks after did,
     *  the output queue would grow bounded by the stream length.
     * If there are 10 data units in the output queue, assume this
     * has happened and purge all blocks that fail extraction criteria */
    int i_count;
    block_ChainProperties( p_sys->p_outqueue, &i_count, NULL, NULL );
    if( i_count > 9 )
    {
        p_block = p_sys->p_outqueue;
        while( p_block )
        {
            block_t *p_block_next = p_block->p_next;
            if( p_block->i_pts > VLC_TS_INVALID && p_block->i_dts > VLC_TS_INVALID )
                break;
            block_Release( p_block );
            p_sys->p_outqueue = p_block = p_block_next;
        }
    }

    if( !p_sys->p_outqueue )
    {
        p_sys->pp_outqueue_last = &p_sys->p_outqueue;
    }
    return p_output;
}

/*****************************************************************************
 * Open: probe the packetizer and return score
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec !=  VLC_CODEC_DIRAC )
        return VLC_EGENERIC;

    p_dec->pf_packetize = Packetize;

    /* Create the output format */
    es_format_Copy( &p_dec->fmt_out, &p_dec->fmt_in );
    p_dec->p_sys = p_sys = calloc( 1, sizeof( decoder_sys_t ) );

    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->i_eu_pts = p_sys->i_eu_dts = VLC_TS_INVALID;
    p_sys->i_sync_pts = p_sys->i_sync_dts = VLC_TS_INVALID;
    p_sys->i_dts_last_out = p_sys->i_pts_last_out = VLC_TS_INVALID;

    p_sys->i_state = NOT_SYNCED;
    block_BytestreamInit( &p_sys->bytestream );

    p_sys->pp_outqueue_last = &p_sys->p_outqueue;
    p_sys->pp_eu_last = &p_sys->p_eu;

    date_Init( &p_sys->dts, 1, 1 );
    dirac_ReorderInit( &p_sys->reorder_buf );

    if( p_dec->fmt_in.i_extra > 0 )
    {
        /* handle hacky systems like ogg that dump some headers
         * in p_extra. and packetizers that expect it to be filled
         * in before real startup */
        block_t *p_init = block_Alloc( p_dec->fmt_in.i_extra );
        if( !p_init )
        {
            /* memory might be avaliable soon.  it isn't the end of
             * the world that fmt_in.i_extra isn't handled */
            return VLC_SUCCESS;
        }
        memcpy( p_init->p_buffer, p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra );
        /* in theory p_extra should contain just a seqhdr&EOS.  if just a
         * seqhdr, ensure it is extracted by appending an EOS with
         * prev_offset = seqhdr length, ie i_extra.  If all were actually
         * ok, this won't do anything bad */
        if( ( p_init->p_next = dirac_EmitEOS( p_dec, p_dec->fmt_in.i_extra ) ) )
        {
            /* to ensure that one of these two EOS dataunits gets extracted,
             * send a second one */
            p_init->p_next->p_next = dirac_EmitEOS( p_dec, 13 );
        }

        block_t *p_block;
        while( ( p_block = Packetize( p_dec, &p_init ) ) )
            block_Release( p_block );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_BytestreamRelease( &p_sys->bytestream );
    if( p_sys->p_outqueue )
        block_ChainRelease( p_sys->p_outqueue );
    if( p_sys->p_eu )
        block_ChainRelease( p_sys->p_eu );
    free( p_sys );
}

