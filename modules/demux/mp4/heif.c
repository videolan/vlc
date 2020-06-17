/*****************************************************************************
 * heif.c : ISO/IEC 23008-12 HEIF still picture demuxer
 *****************************************************************************
 * Copyright (C) 2018 Videolabs, VLC authors and VideoLAN
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_input.h>
#include <vlc_image.h>
#include <assert.h>
#include <limits.h>

#include "libmp4.h"
#include "heif.h"
#include "../../packetizer/iso_color_tables.h"

struct heif_private_t
{
    MP4_Box_t *p_root;
    es_out_id_t *id;
    vlc_tick_t i_pcr;
    vlc_tick_t i_end_display_time;
    vlc_tick_t i_image_duration;
    bool b_seekpoint_changed;
    uint32_t i_seekpoint;
    input_title_t *p_title;

    struct
    {
        MP4_Box_t *p_infe;
        es_format_t fmt;
        const MP4_Box_t *p_shared_header;
    } current;
};

static MP4_Box_t * NextAtom( MP4_Box_t *p_root,
                             vlc_fourcc_t i_type, const char *psz_path,
                             MP4_Box_t *p_infe )
{
    if( p_infe == NULL )
        p_infe = MP4_BoxGet( p_root, psz_path );
    else
        p_infe = p_infe->p_next;
    for( ; p_infe; p_infe = p_infe->p_next )
    {
        if( p_infe->i_type == i_type )
            return p_infe;
    }
    return NULL;
}

static MP4_Box_t * GetAtom( MP4_Box_t *p_root, MP4_Box_t *p_atom,
                            vlc_fourcc_t i_type, const char *psz_path,
                            bool(*pf_match)(const MP4_Box_t *, void *),
                            void *priv )
{
    while( (p_atom = NextAtom( p_root, i_type, psz_path, p_atom )) )
    {
        if( pf_match( p_atom, priv ) )
            return p_atom;
    }
    return NULL;
}

static bool MatchInfeID( const MP4_Box_t *p_infe, void *priv )
{
    return BOXDATA(p_infe)->i_item_id == *((uint32_t *) priv);
}

static bool MatchPureImage( const MP4_Box_t *p_infe, void *priv )
{
    MP4_Box_t *p_root = priv;
    const MP4_Box_t *p_iref = MP4_BoxGet( p_root, "meta/iref" );
    if( !p_iref )
        return true;
    for( const MP4_Box_t *p_refbox = p_iref->p_first;
                          p_refbox; p_refbox = p_refbox->p_next )
    {
        if( BOXDATA(p_refbox)->i_from_item_id == BOXDATA(p_infe)->i_item_id )
            return false;
    }
    return true;
}

static void SeekToPrevImageEnd( struct heif_private_t *p_sys, int i_picture )
{
    int i = 0;
    MP4_Box_t *p_infe = NULL;
    while( i < i_picture &&
          (p_infe = NextAtom( p_sys->p_root, ATOM_infe, "meta/iinf/infe", p_infe )) )
    {
        if( (BOXDATA(p_infe)->i_flags & 0x01) != 0x00 ||
                !MatchPureImage( p_infe, p_sys->p_root ) )
            continue;
        i++;
    }
    p_sys->current.p_infe = p_infe;
    p_sys->i_end_display_time = 0;
    p_sys->i_pcr = i * p_sys->i_image_duration;
}

static int ControlHEIF( demux_t *p_demux, int i_query, va_list args )
{
    struct heif_private_t *p_sys = (void *) p_demux->p_sys;

    switch( i_query )
    {
        case DEMUX_CAN_SEEK:
            *va_arg(args, bool *) = true;
            return VLC_SUCCESS;
        case DEMUX_GET_TITLE_INFO:
        {
            input_title_t ***ppp_title = va_arg( args, input_title_t *** );
            int *pi_int = va_arg( args, int* );
            int *pi_title_offset = va_arg( args, int* );
            int *pi_seekpoint_offset = va_arg( args, int* );

            if( !p_sys->p_title )
                return VLC_EGENERIC;

            *pi_int = 1;
            *ppp_title = malloc( sizeof( input_title_t*) );
            (*ppp_title)[0] = vlc_input_title_Duplicate( p_sys->p_title );
            *pi_title_offset = 0;
            *pi_seekpoint_offset = 0;
            return VLC_SUCCESS;
        }
        case DEMUX_SET_TITLE:
        {
            const int i_title = va_arg( args, int );
            if( !p_sys->p_title || i_title != 0 )
                return VLC_EGENERIC;
            return VLC_SUCCESS;
        }
        case DEMUX_GET_SEEKPOINT:
            *va_arg( args, int * ) = p_sys->i_seekpoint;
            return VLC_SUCCESS;
        case DEMUX_SET_SEEKPOINT:
        {
            const int i_seekpoint = va_arg( args, int );
            if( !p_sys->p_title )
                return VLC_EGENERIC;
            SeekToPrevImageEnd( p_sys, i_seekpoint );
            return VLC_SUCCESS;
        }
        case DEMUX_TEST_AND_CLEAR_FLAGS:
        {
            unsigned *restrict flags = va_arg( args, unsigned * );

            if ((*flags & INPUT_UPDATE_SEEKPOINT) && p_sys->b_seekpoint_changed)
            {
                *flags = INPUT_UPDATE_SEEKPOINT;
                p_sys->b_seekpoint_changed = false;
            }
            else
                *flags = 0;
            return VLC_SUCCESS;
        }
        case DEMUX_GET_LENGTH:
            *(va_arg( args, vlc_tick_t * )) = p_sys->p_title->i_seekpoint *
                                              p_sys->i_image_duration;
            return VLC_SUCCESS;
        case DEMUX_GET_TIME:
            *(va_arg(args, vlc_tick_t *)) = p_sys->i_pcr;
            return VLC_SUCCESS;
        case DEMUX_SET_TIME:
        {
            SeekToPrevImageEnd( p_sys, va_arg(args, vlc_tick_t) /
                                p_sys->i_image_duration );
            return VLC_SUCCESS;
        }
        case DEMUX_GET_POSITION:
            if( !p_sys->p_title->i_seekpoint )
                return VLC_EGENERIC;
            *(va_arg(args, double *)) = (double) p_sys->i_pcr /
                    (p_sys->p_title->i_seekpoint * p_sys->i_image_duration);
            return VLC_SUCCESS;
        case DEMUX_SET_POSITION:
        {
            SeekToPrevImageEnd( p_sys,  va_arg(args, double) * p_sys->p_title->i_seekpoint );
            return VLC_SUCCESS;
        }
        case DEMUX_CAN_PAUSE:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_GET_PTS_DELAY:
            return demux_vaControlHelper( p_demux->s, 0, -1, 0, 1, i_query, args );

        default:
            return VLC_EGENERIC;

    }
}

//static int DemuxCompositeImage( demux_t *p_demux )
//{

//}

static block_t *ReadItemExtents( demux_t *p_demux, uint32_t i_item_id,
                                 const MP4_Box_t *p_shared_header )
{
    struct heif_private_t *p_sys = (void *) p_demux->p_sys;
    block_t *p_block = NULL;

    MP4_Box_t *p_iloc = MP4_BoxGet( p_sys->p_root, "meta/iloc" );
    if( !p_iloc )
        return p_block;

    for( uint32_t i=0; i<BOXDATA(p_iloc)->i_item_count; i++ )
    {
        if( BOXDATA(p_iloc)->p_items[i].i_item_id != i_item_id )
            continue;

        block_t **pp_append = &p_block;

        /* Shared prefix data, ex: JPEG */
        if( p_shared_header )
        {
            *pp_append = block_Alloc( p_shared_header->data.p_binary->i_blob );
            if( *pp_append )
            {
                memcpy( (*pp_append)->p_buffer,
                        p_shared_header->data.p_binary->p_blob,
                        p_shared_header->data.p_binary->i_blob );
                pp_append = &((*pp_append)->p_next);
            }
        }

        for( uint16_t j=0; j<BOXDATA(p_iloc)->p_items[i].i_extent_count; j++ )
        {
            uint64_t i_offset = BOXDATA(p_iloc)->p_items[i].i_base_offset +
                                BOXDATA(p_iloc)->p_items[i].p_extents[j].i_extent_offset;
            uint64_t i_length = BOXDATA(p_iloc)->p_items[i].p_extents[j].i_extent_length;

            if( BOXDATA(p_iloc)->p_items[i].i_construction_method < 2 )
            {
                /* Extents are in 0:file, 1:idat */
                if( BOXDATA(p_iloc)->p_items[i].i_construction_method == 1 )
                {
                    MP4_Box_t *idat = MP4_BoxGet( p_sys->p_root, "meta/idat" );
                    if(!idat)
                        break;
                    i_offset += idat->i_pos + mp4_box_headersize(idat);
                    if( i_length == 0 ) /* Entire container */
                        i_length = idat->i_size - mp4_box_headersize(idat);
                }
                else
                {
                    if( i_length == 0 ) /* Entire container == file */
                    {
                        if( vlc_stream_GetSize( p_demux->s, &i_length )
                                == VLC_SUCCESS && i_length > i_offset )
                            i_length -= i_offset;
                        else
                            i_length = 0;
                    }
                }
                if( vlc_stream_Seek( p_demux->s, i_offset ) != VLC_SUCCESS )
                    break;
                *pp_append = vlc_stream_Block( p_demux->s, i_length );
            }
            /* Extents are 3:iloc reference */
            else if( BOXDATA(p_iloc)->p_items[i].i_construction_method == 2 )
            {
                /* FIXME ? That's totally untested and really complicated */
                uint32_t i_extent_index = BOXDATA(p_iloc)->p_items[i].p_extents[j].i_extent_index;
                if(i_extent_index == 0)
                    i_extent_index = 1; /* Inferred. Indexes start 1 */
                const MP4_Box_t *p_iref = MP4_BoxGet( p_sys->p_root, "meta/iref" );
                if(!p_iref)
                    break;
                for( const MP4_Box_t *p_refbox = p_iref->p_first;
                                      p_refbox; p_refbox = p_refbox->p_next )
                {
                    if( p_refbox->i_type != VLC_FOURCC('i','l','o','c') ||
                        BOXDATA(p_refbox)->i_from_item_id == i_item_id )
                        continue;

                    for( uint16_t k=0; k< BOXDATA(p_refbox)->i_reference_count; k++ )
                    {
                        if( --i_extent_index > 0 )
                            continue;
                        if( BOXDATA(p_refbox)->p_references[k].i_to_item_id != i_item_id )
                        {
                            *pp_append = ReadItemExtents(p_demux,
                                            BOXDATA(p_refbox)->p_references[k].i_to_item_id,
                                            NULL);
                        }
                    }

                    break;
                }
            }

            while( *pp_append )
                pp_append = &((*pp_append)->p_next);
        }
        break;
    }

    if( p_block )
        p_block = block_ChainGather( p_block );

    return p_block;
}

static int SetPictureProperties( demux_t *p_demux, uint32_t i_item_id,
                                 es_format_t *fmt, const MP4_Box_t **p_header )
{
    struct heif_private_t *p_sys = (void *) p_demux->p_sys;

    const MP4_Box_t *p_ipma = MP4_BoxGet( p_sys->p_root, "meta/iprp/ipma" );
    if( !p_ipma )
        return VLC_EGENERIC;

    /* Load properties */
    for( uint32_t i=0; i<BOXDATA(p_ipma)->i_entry_count; i++ )
    {
        if( BOXDATA(p_ipma)->p_entries[i].i_item_id != i_item_id )
            continue;

        for( uint8_t j=0; j<BOXDATA(p_ipma)->p_entries[i].i_association_count; j++ )
        {
            if( !BOXDATA(p_ipma)->p_entries[i].p_assocs[j].i_property_index )
                continue;

            const MP4_Box_t *p_prop = MP4_BoxGet( p_sys->p_root, "meta/iprp/ipco/[%u]",
                BOXDATA(p_ipma)->p_entries[i].p_assocs[j].i_property_index - 1 );
            if( !p_prop )
                continue;

            switch( p_prop->i_type )
            {
                case ATOM_hvcC:
                case ATOM_avcC:
                    if( !fmt->p_extra && p_prop->data.p_binary &&
                       ((fmt->i_codec == VLC_CODEC_HEVC && p_prop->i_type == ATOM_hvcC) ||
                        (fmt->i_codec == VLC_CODEC_H264 && p_prop->i_type == ATOM_avcC) ))
                    {
                        fmt->p_extra = malloc( p_prop->data.p_binary->i_blob );
                        if( fmt->p_extra )
                        {
                            fmt->i_extra = p_prop->data.p_binary->i_blob;
                            memcpy( fmt->p_extra, p_prop->data.p_binary->p_blob, fmt->i_extra );
                        }
                    }
                    break;
                case ATOM_av1C:
                    if( fmt->i_codec == VLC_CODEC_AV1 && !fmt->i_extra &&
                        p_prop->data.p_av1C->i_av1C >= 4 )
                    {
                        fmt->p_extra = malloc( p_prop->data.p_av1C->i_av1C );
                        if( fmt->p_extra )
                        {
                            fmt->i_extra = p_prop->data.p_av1C->i_av1C ;
                            memcpy( fmt->p_extra, p_prop->data.p_av1C->p_av1C, fmt->i_extra );
                        }
                    }
                    break;
                case ATOM_jpeC:
                    if( fmt->i_codec == VLC_CODEC_JPEG )
                        *p_header = p_prop;
                    break;
                case ATOM_ispe:
                    fmt->video.i_visible_width = p_prop->data.p_ispe->i_width;
                    fmt->video.i_visible_height = p_prop->data.p_ispe->i_height;
                    break;
                case ATOM_pasp:
                    if( p_prop->data.p_pasp->i_horizontal_spacing &&
                        p_prop->data.p_pasp->i_vertical_spacing )
                    {
                        fmt->video.i_sar_num = p_prop->data.p_pasp->i_horizontal_spacing;
                        fmt->video.i_sar_den = p_prop->data.p_pasp->i_vertical_spacing;
                    }
                    break;
                case ATOM_irot:
                    switch( p_prop->data.p_irot->i_ccw_degrees % 360 )
                    {
                        default:
                        case 0:   fmt->video.orientation = ORIENT_NORMAL ; break;
                        case 90:  fmt->video.orientation = ORIENT_ROTATED_90; break;
                        case 180: fmt->video.orientation = ORIENT_ROTATED_180 ; break;
                        case 270: fmt->video.orientation = ORIENT_ROTATED_270 ; break;
                    }
                    break;
                case ATOM_colr:
                    fmt->video.primaries = iso_23001_8_cp_to_vlc_primaries(
                                            p_prop->data.p_colr->nclc.i_primary_idx );
                    fmt->video.transfer = iso_23001_8_tc_to_vlc_xfer(
                                            p_prop->data.p_colr->nclc.i_transfer_function_idx );
                    fmt->video.space = iso_23001_8_mc_to_vlc_coeffs(
                                        p_prop->data.p_colr->nclc.i_matrix_idx );
                    fmt->video.color_range = p_prop->data.p_colr->nclc.i_full_range ?
                                COLOR_RANGE_FULL : COLOR_RANGE_LIMITED;
                    break;
                case ATOM_clli:
                    fmt->video.lighting.MaxCLL = p_prop->data.p_CoLL->i_maxCLL;
                    fmt->video.lighting.MaxFALL = p_prop->data.p_CoLL->i_maxFALL;
                    break;
                case ATOM_mdcv:
                    memcpy( fmt->video.mastering.primaries,
                            p_prop->data.p_SmDm->primaries, sizeof(uint16_t) * 6 );
                    memcpy( fmt->video.mastering.white_point,
                            p_prop->data.p_SmDm->white_point, sizeof(uint16_t) * 2 );
                    fmt->video.mastering.max_luminance = p_prop->data.p_SmDm->i_luminanceMax;
                    fmt->video.mastering.min_luminance = p_prop->data.p_SmDm->i_luminanceMin;
                    break;
            }
        }
    }

    fmt->video.i_frame_rate      = 1000;
    fmt->video.i_frame_rate_base = p_sys->i_image_duration / 1000;

    return VLC_SUCCESS;
}

static int SetupPicture( demux_t *p_demux, const MP4_Box_t *p_infe,
                         es_format_t *fmt, const MP4_Box_t **p_header )
{
    fmt->i_codec = 0;
    *p_header = NULL;

    const uint32_t i_item_id = BOXDATA(p_infe)->i_item_id;
    const char *psz_mime = BOXDATA(p_infe)->psz_content_type;
    switch( BOXDATA(p_infe)->item_type )
    {
        case VLC_FOURCC('h','v','c','1'):
            es_format_Init( fmt, VIDEO_ES, VLC_CODEC_HEVC );
            break;
        case VLC_FOURCC('a','v','c','1'):
            es_format_Init( fmt, VIDEO_ES, VLC_CODEC_H264 );
            break;
        case ATOM_av01:
            es_format_Init( fmt, VIDEO_ES, VLC_CODEC_AV1 );
            break;
        case VLC_FOURCC('j','p','e','g'):
            es_format_Init( fmt, VIDEO_ES, VLC_CODEC_JPEG );
            break;
        default:
            if( psz_mime )
            {
                if( !strcasecmp( "image/jpeg", psz_mime ) )
                    es_format_Init( fmt, VIDEO_ES, VLC_CODEC_JPEG );
                else if( !strcasecmp( "image/avif", psz_mime ) )
                    es_format_Init( fmt, VIDEO_ES, VLC_CODEC_AV1 );
            }
            break;
    }

    if( fmt->i_codec == 0 )
        return VLC_EGENERIC;

    return SetPictureProperties( p_demux, i_item_id, fmt, p_header );
}

union heif_derivation_data
{
    struct
    {
        uint8_t rows_minus_one;
        uint8_t columns_minus_one;
        uint32_t output_width;
        uint32_t output_height;
    } ImageGrid;
};

static int ReadDerivationData_Grid( const uint8_t *p_data, size_t i_data,
                                    union heif_derivation_data *d )
{
    if( i_data < 8 || p_data[0] != 0x00 )
        return VLC_EGENERIC;

    uint8_t i_fieldlength = ((p_data[1] & 0x01) + 1) << 1;
    /* length is either 2 or 4 bytes */
    d->ImageGrid.rows_minus_one = p_data[2];
    d->ImageGrid.columns_minus_one = p_data[3];
    if(i_fieldlength == 2)
    {
        d->ImageGrid.output_width = GetWBE(&p_data[4]);
        d->ImageGrid.output_height = GetWBE(&p_data[6]);
    }
    else
    {
        if(i_data < 12)
            return VLC_EGENERIC;
        d->ImageGrid.output_width = GetDWBE(&p_data[4]);
        d->ImageGrid.output_height = GetDWBE(&p_data[8]);
    }
    return VLC_SUCCESS;
}

static int ReadDerivationData( demux_t *p_demux, vlc_fourcc_t type,
                               uint32_t i_item_id,
                               union heif_derivation_data *d )
{
    int i_ret = VLC_EGENERIC;
    block_t *p_data = ReadItemExtents( p_demux, i_item_id, NULL );
    if( p_data )
    {
        switch( type )
        {
            case VLC_FOURCC('g','r','i','d'):
                i_ret = ReadDerivationData_Grid( p_data->p_buffer,
                                                 p_data->i_buffer, d );
                /* fallthrough */
            default:
                break;
        }
        block_Release( p_data );
    }
    return i_ret;
}

static int LoadGridImage( demux_t *p_demux,
                          image_handler_t *handler,
                          uint32_t i_pic_item_id,
                               uint8_t *p_buffer,
                               unsigned tile, unsigned gridcols,
                               unsigned imagewidth, unsigned imageheight )
{
    struct heif_private_t *p_sys = (void *) p_demux->p_sys;

    MP4_Box_t *p_infe = GetAtom( p_sys->p_root, NULL,
                                 ATOM_infe, "meta/iinf/infe",
                                 MatchInfeID, &i_pic_item_id );
    if( !p_infe )
        return VLC_EGENERIC;

    es_format_t fmt;
    es_format_Init(&fmt, UNKNOWN_ES, 0);

    const MP4_Box_t *p_shared_header = NULL;
    if( SetupPicture( p_demux, p_infe, &fmt, &p_shared_header ) != VLC_SUCCESS )
    {
        es_format_Clean( &fmt );
        return VLC_EGENERIC; /* Unsupported picture, goto next */
    }

    block_t *p_sample = ReadItemExtents( p_demux, i_pic_item_id,
                                         p_shared_header );
    if(!p_sample)
    {
        es_format_Clean( &fmt );
        return VLC_EGENERIC;
    }

    video_format_t decoded;
    video_format_Init( &decoded, VLC_CODEC_RGBA );

    fmt.video.i_chroma = fmt.i_codec;

    picture_t *p_picture = image_Read( handler, p_sample, &fmt, &decoded );

    es_format_Clean( &fmt );

    if ( !p_picture )
        return VLC_EGENERIC;

    const unsigned tilewidth = p_picture->format.i_visible_width;
    const unsigned tileheight = p_picture->format.i_visible_height;
    uint8_t *dstline = p_buffer;
    dstline += (tile / gridcols) * (imagewidth * tileheight * 4);
    for(;1;)
    {
        const unsigned offsetpxw = (tile % gridcols) * tilewidth;
        const unsigned offsetpxh = (tile / gridcols) * tileheight;
        if( offsetpxw > imagewidth )
            break;
        const uint8_t *srcline = p_picture->p[0].p_pixels;
        unsigned tocopylines = p_picture->p[0].i_lines;
        if(offsetpxh + tocopylines >= imageheight)
            tocopylines = imageheight - offsetpxh;
        for(unsigned i=0; i<tocopylines; i++)
        {
            size_t tocopypx = tilewidth;
            if( offsetpxw + tilewidth > imagewidth )
                tocopypx = imagewidth - offsetpxw;
            memcpy( &dstline[offsetpxw * 4], srcline, tocopypx * 4 );
            dstline += imagewidth * 4;
            srcline += p_picture->p[0].i_pitch;
        }

        break;
    }

    picture_Release( p_picture );

    return VLC_SUCCESS;
}

static int DerivedImageAssembleGrid( demux_t *p_demux, uint32_t i_grid_item_id,
                                     es_format_t *fmt, block_t **pp_block )
{
    struct heif_private_t *p_sys = (void *) p_demux->p_sys;

    const MP4_Box_t *p_iref = MP4_BoxGet( p_sys->p_root, "meta/iref" );
    if(!p_iref)
        return VLC_EGENERIC;

    const MP4_Box_t *p_refbox;
    for( p_refbox = p_iref->p_first; p_refbox; p_refbox = p_refbox->p_next )
    {
        if( p_refbox->i_type == VLC_FOURCC('d','i','m','g') &&
            BOXDATA(p_refbox)->i_from_item_id == i_grid_item_id )
            break;
    }

    if(!p_refbox)
        return VLC_EGENERIC;

    union heif_derivation_data derivation_data;
    if( ReadDerivationData( p_demux,
                            p_sys->current.BOXDATA(p_infe)->item_type,
                            i_grid_item_id, &derivation_data ) != VLC_SUCCESS )
        return VLC_EGENERIC;

    msg_Dbg(p_demux,"%ux%upx image %ux%u tiles composition",
            derivation_data.ImageGrid.output_width,
            derivation_data.ImageGrid.output_height,
            derivation_data.ImageGrid.columns_minus_one + 1,
            derivation_data.ImageGrid.columns_minus_one + 1);

    image_handler_t *handler = image_HandlerCreate( p_demux );
    if( !handler )
        return VLC_EGENERIC;

    block_t *p_block = block_Alloc( derivation_data.ImageGrid.output_width *
                                    derivation_data.ImageGrid.output_height * 4 );
    if( !p_block )
        return VLC_EGENERIC;
    *pp_block = p_block;

    es_format_Init( fmt, VIDEO_ES, VLC_CODEC_RGBA );
    fmt->video.i_sar_num =
    fmt->video.i_width =
    fmt->video.i_visible_width = derivation_data.ImageGrid.output_width;
    fmt->video.i_sar_den =
    fmt->video.i_height =
    fmt->video.i_visible_height = derivation_data.ImageGrid.output_height;

    for( uint16_t i=0; i<BOXDATA(p_refbox)->i_reference_count; i++ )
    {
        msg_Dbg( p_demux, "Loading tile %d/%d", i,
                 (derivation_data.ImageGrid.rows_minus_one + 1) *
                 (derivation_data.ImageGrid.columns_minus_one + 1) );
        LoadGridImage( p_demux, handler,
                       BOXDATA(p_refbox)->p_references[i].i_to_item_id,
                       p_block->p_buffer, i,
                       derivation_data.ImageGrid.columns_minus_one + 1,
                       derivation_data.ImageGrid.output_width,
                       derivation_data.ImageGrid.output_height );
    }

    SetPictureProperties( p_demux, i_grid_item_id, fmt, NULL );

    image_HandlerDelete( handler );

    return VLC_SUCCESS;
}

static int DemuxHEIF( demux_t *p_demux )
{
    struct heif_private_t *p_sys = (void *) p_demux->p_sys;

    /* Displaying a picture */
    if( p_sys->i_end_display_time > 0 )
    {
        bool b_empty;
        es_out_Control( p_demux->out, ES_OUT_GET_EMPTY, &b_empty );
        if( !b_empty || vlc_tick_now() <= p_sys->i_end_display_time )
        {
            vlc_tick_sleep( VLC_TICK_FROM_MS(40) );
            return VLC_DEMUXER_SUCCESS;
        }
        p_sys->i_end_display_time = 0;
    }

    /* Reset prev pic params */
    p_sys->current.p_shared_header = NULL;

    /* First or next picture */
    if( !p_sys->current.p_infe )
    {
        MP4_Box_t *p_pitm = MP4_BoxGet( p_sys->p_root, "meta/pitm" );
        if( !p_pitm )
            return VLC_DEMUXER_EOF;

        p_sys->current.p_infe = GetAtom( p_sys->p_root, NULL,
                                         ATOM_infe, "meta/iinf/infe",
                                         MatchInfeID, &BOXDATA(p_pitm)->i_item_id );
    }
    else
    {
        p_sys->current.p_infe = GetAtom( p_sys->p_root, p_sys->current.p_infe,
                                         ATOM_infe, "meta/iinf/infe",
                                         MatchPureImage, p_sys->p_root );
    }

    if( !p_sys->current.p_infe )
        return VLC_DEMUXER_EOF;

    const uint32_t i_current_item_id = p_sys->current.BOXDATA(p_infe)->i_item_id;
    const MP4_Box_t *p_ipco = MP4_BoxGet( p_sys->p_root, "meta/iprp/ipco" );
    if( !p_ipco )
        return VLC_DEMUXER_EOF;

    es_format_t fmt;
    es_format_Init(&fmt, UNKNOWN_ES, 0);

    block_t *p_block = NULL;
    if( p_sys->current.BOXDATA(p_infe)->item_type == VLC_FOURCC('g','r','i','d') )
    {
        if( DerivedImageAssembleGrid( p_demux, i_current_item_id,
                                      &fmt, &p_block ) != VLC_SUCCESS )
        {
            es_format_Clean( &fmt );
            return VLC_DEMUXER_SUCCESS;
        }
    }
    else
    {
        if( SetupPicture( p_demux, p_sys->current.p_infe,
                          &fmt, &p_sys->current.p_shared_header ) != VLC_SUCCESS )
        {
            es_format_Clean( &fmt );
            return VLC_DEMUXER_SUCCESS;
        }

        p_block = ReadItemExtents( p_demux, i_current_item_id,
                                   p_sys->current.p_shared_header );
        if( !p_block )
        {
            es_format_Clean( &fmt );
            return VLC_DEMUXER_SUCCESS; /* Goto next picture */
        }
    }

    es_format_Clean( &p_sys->current.fmt );
    es_format_Copy( &p_sys->current.fmt, &fmt );
    es_format_Clean( &fmt );
    if( p_sys->id )
        es_out_Del( p_demux->out, p_sys->id );
    p_sys->id = es_out_Add( p_demux->out, &p_sys->current.fmt );

    if( !p_sys->id )
    {
        p_sys->current.p_infe = NULL; /* Goto next picture */
        return VLC_DEMUXER_SUCCESS;
    }

    if( p_sys->i_pcr == VLC_TICK_INVALID )
    {
        p_sys->i_pcr = VLC_TICK_0;
        es_out_SetPCR( p_demux->out, p_sys->i_pcr );
    }

    p_block->i_dts = p_block->i_pts = p_sys->i_pcr;
    p_block->i_length = p_sys->i_image_duration;

    p_block->i_flags |= BLOCK_FLAG_END_OF_SEQUENCE;

    p_sys->i_end_display_time = vlc_tick_now() + p_block->i_length;
    p_sys->b_seekpoint_changed = true;

    p_sys->i_pcr = p_block->i_dts + p_block->i_length;
    es_out_Send( p_demux->out, p_sys->id, p_block );
    es_out_SetPCR( p_demux->out, p_sys->i_pcr );

    return VLC_DEMUXER_SUCCESS;
}

int OpenHEIF( vlc_object_t * p_this )
{
    demux_t  *p_demux = (demux_t *)p_this;
    const uint8_t *p_peek;

    if( vlc_stream_Peek( p_demux->s, &p_peek, 12 ) < 12 )
        return VLC_EGENERIC;

    if( VLC_FOURCC( p_peek[4], p_peek[5], p_peek[6], p_peek[7] ) != ATOM_ftyp )
        return VLC_EGENERIC;

    switch( VLC_FOURCC( p_peek[8], p_peek[9], p_peek[10], p_peek[11] ) )
    {
        case BRAND_mif1:
        case BRAND_heic:
        case BRAND_heix:
        case BRAND_jpeg:
        case BRAND_avci:
        case BRAND_avif:
            break;
        case BRAND_msf1:
        case BRAND_hevc:
        case BRAND_hevx:
        case BRAND_avcs:
        case BRAND_avis:
        default:
            return VLC_EGENERIC;
    }

    MP4_Box_t *p_root = MP4_BoxGetRoot( p_demux->s );
    if( !p_root )
        return VLC_EGENERIC;

    MP4_BoxDumpStructure( p_demux->s, p_root );

    struct heif_private_t *p_sys = calloc( 1, sizeof(*p_sys) );
    p_demux->p_sys = (void *) p_sys;
    p_sys->p_root = p_root;
    p_sys->p_title = vlc_input_title_New();
    if( !p_sys->p_title )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }

    p_sys->i_image_duration = vlc_tick_from_sec(var_InheritFloat( p_demux, "heif-image-duration" ));
    if( p_sys->i_image_duration <= 0 )
        p_sys->i_image_duration = VLC_TICK_FROM_SEC(HEIF_DEFAULT_DURATION);

    MP4_Box_t *p_infe = NULL;
    while( (p_infe = NextAtom( p_root, ATOM_infe, "meta/iinf/infe", p_infe )) )
    {
        if( (BOXDATA(p_infe)->i_flags & 0x01) != 0x00 ||
                !MatchPureImage( p_infe, p_root ) )
            continue;
        seekpoint_t *s = vlc_seekpoint_New();
        if( s )
        {
            s->i_time_offset = p_sys->p_title->i_seekpoint * p_sys->i_image_duration;
            if( BOXDATA(p_infe)->psz_item_name )
                s->psz_name = strdup( BOXDATA(p_infe)->psz_item_name );
            TAB_APPEND( p_sys->p_title->i_seekpoint, p_sys->p_title->seekpoint, s );
        }
    }

    es_format_Init( &p_sys->current.fmt, UNKNOWN_ES, 0 );

    p_demux->pf_demux = DemuxHEIF;
    p_demux->pf_control = ControlHEIF;

    return VLC_SUCCESS;
}

void CloseHEIF ( vlc_object_t * p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    struct heif_private_t *p_sys = (void *) p_demux->p_sys;
    MP4_BoxFree( p_sys->p_root );
    if( p_sys->id )
        es_out_Del( p_demux->out, p_sys->id );
    es_format_Clean( &p_sys->current.fmt );
    vlc_input_title_Delete( p_sys->p_title );
    free( p_sys );
}
