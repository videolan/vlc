/*****************************************************************************
 * parse.c: SPU parser
 *****************************************************************************
 * Copyright (C) 2000-2001, 2005, 2006 VLC authors and VideoLAN
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_codec.h>

#include "spudec.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
typedef struct
{
    int i_width;
    int i_height;
    int i_x;
    int i_y;
    vlc_tick_t i_start;
    vlc_tick_t i_stop;
    bool b_ephemer;
    bool b_subtitle;
} spu_properties_t;

typedef struct
{
    int   pi_offset[2];                              /* byte offsets to data */

    /* Color information */
    bool b_palette;
    uint8_t    pi_alpha[4];
    uint8_t    pi_yuv[4][3];
    /* PXCTLI commands storage */
    const uint8_t *p_pxctli;
    size_t i_pxclti;

    /* Auto crop fullscreen subtitles */
    bool b_auto_crop;
    int i_y_top_offset;
    int i_y_bottom_offset;

} subpicture_data_t;

static int  ParseControlSeq( decoder_t *, vlc_tick_t i_pts,
                             void(*pf_queue)(decoder_t *, subpicture_t *) );
static int  ParseRLE       ( decoder_t *, subpicture_data_t *,
                             const spu_properties_t *, uint16_t * );
static int  Render         ( decoder_t *, subpicture_t *, const uint16_t *,
                             const subpicture_data_t *, const spu_properties_t * );

/*****************************************************************************
 * AddNibble: read a nibble from a source packet and add it to our integer.
 *****************************************************************************/
static inline unsigned int AddNibble( unsigned int i_code,
                                      const uint8_t *p_src, unsigned int *pi_index )
{
    if( *pi_index & 0x1 )
    {
        return( i_code << 4 | ( p_src[(*pi_index)++ >> 1] & 0xf ) );
    }
    else
    {
        return( i_code << 4 | p_src[(*pi_index)++ >> 1] >> 4 );
    }
}

static void CLUTIdxToYUV(const struct subs_format_t *subs,
                         const uint8_t idx[4], uint8_t yuv[4][3])
{
    for( int i = 0; i < 4 ; i++ )
    {
        uint32_t i_ayvu = subs->spu.palette[1+idx[i]];
        /* FIXME: this job should be done sooner */
        yuv[3-i][0] = i_ayvu>>16;
        yuv[3-i][1] = i_ayvu;
        yuv[3-i][2] = i_ayvu>>8;
    }
}

static void ParsePXCTLI( decoder_t *p_dec, const subpicture_data_t *p_spu_data,
                         subpicture_t *p_spu )
{
    plane_t *p_plane = &p_spu->p_region->p_picture->p[0];
    video_palette_t *p_palette = p_spu->p_region->fmt.p_palette;

    for( size_t i=0;i<p_spu_data->i_pxclti; i++ )
    {
        uint16_t i_col = GetWBE(&p_spu_data->p_pxctli[i*6 + 0]);
        uint16_t i_color = GetWBE(&p_spu_data->p_pxctli[i*6 + 2]);
        uint16_t i_contrast = GetWBE(&p_spu_data->p_pxctli[i*6 + 4]);

        if(p_palette->i_entries +4 >= VIDEO_PALETTE_COLORS_MAX)
            break;

        if( p_dec->fmt_in.subs.spu.palette[0] == SPU_PALETTE_DEFINED )
        {
            /* Lookup the CLUT palette for the YUV values */
            uint8_t idx[4];
            uint8_t yuv[4][3];
            uint8_t alpha[4];
            idx[0] = (i_color >> 12)&0x0f;
            idx[1] = (i_color >>  8)&0x0f;
            idx[2] = (i_color >>  4)&0x0f;
            idx[3] = i_color&0x0f;
            CLUTIdxToYUV( &p_dec->fmt_in.subs, idx, yuv );

            /* Process the contrast */
            alpha[3] = (i_contrast >> 12)&0x0f;
            alpha[2] = (i_contrast >>  8)&0x0f;
            alpha[1] = (i_contrast >>  4)&0x0f;
            alpha[0] = i_contrast&0x0f;

            /* Create a new YUVA palette entries for the picture */
            int index_map[4];
            for( int j=0; j<4; j++ )
            {
                uint8_t yuvaentry[4];
                yuvaentry[0] = yuv[j][0];
                yuvaentry[1] = yuv[j][1];
                yuvaentry[2] = yuv[j][2];
                yuvaentry[3] = alpha[j] * 0x11;
                int i_index = VIDEO_PALETTE_COLORS_MAX;
                for( int k = p_palette->i_entries; k > 0; k-- )
                {
                    if( !memcmp( &p_palette->palette[k], yuvaentry, sizeof(uint8_t [4]) ) )
                    {
                        i_index = VIDEO_PALETTE_COLORS_MAX;
                        break;
                    }
                }

                /* Add an entry in out palette */
                if( i_index == VIDEO_PALETTE_COLORS_MAX )
                {
                    if(p_palette->i_entries == VIDEO_PALETTE_COLORS_MAX)
                    {
                        msg_Warn( p_dec, "Cannot create new color, skipping PXCTLI" );
                        return;
                    }
                    i_index = p_palette->i_entries++;
                    memcpy( p_palette->palette[ i_index ], yuvaentry, sizeof(uint8_t [4]) );
                }
                index_map[j] = i_index;
            }

            if( p_spu->p_region->i_x >= i_col )
                i_col -= p_spu->p_region->i_x;

            for( int j=0; j<p_plane->i_visible_lines; j++ )
            {
                uint8_t *p_line = &p_plane->p_pixels[j * p_plane->i_pitch];
                /* Extends to end of the line */
                for( int k=i_col; k<p_plane->i_visible_pitch; k++ )
                {
                    if( p_line[k] < 4 ) /* can forge write-again */
                        p_line[k] = index_map[ p_line[k] ];
                }
            }
        }
    }
}

/*****************************************************************************
 * OutputPicture:
 *****************************************************************************
 *****************************************************************************/
static void OutputPicture( decoder_t *p_dec,
                           const subpicture_data_t *p_spu_data,
                           const spu_properties_t *p_spu_properties,
                           void(*pf_queue)(decoder_t *, subpicture_t *) )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    subpicture_t *p_spu;
    uint16_t *p_pixeldata;

    /* Allocate the subpicture internal data. */
    p_spu = decoder_NewSubpicture( p_dec, NULL );
    if( !p_spu ) return;

    p_spu->i_original_picture_width =
        p_dec->fmt_in.subs.spu.i_original_frame_width;
    p_spu->i_original_picture_height =
        p_dec->fmt_in.subs.spu.i_original_frame_height;
    p_spu->i_start = p_spu_properties->i_start;
    p_spu->i_stop = p_spu_properties->i_stop;
    p_spu->b_ephemer = p_spu_properties->b_ephemer;
    p_spu->b_subtitle = p_spu_properties->b_subtitle;

    if( p_spu->i_stop <= p_spu->i_start && !p_spu->b_ephemer )
    {
        /* This subtitle will live for 5 seconds or until the next subtitle */
        p_spu->i_stop = p_spu->i_start + VLC_TICK_FROM_MS(500 * 11);
        p_spu->b_ephemer = true;
    }

    /* we are going to expand the RLE stuff so that we won't need to read
     * nibbles later on. This will speed things up a lot. Plus, we'll only
     * need to do this stupid interlacing stuff once.
     *
     * Rationale for the "p_spudec->i_rle_size * 4*sizeof(*spu_data.p_data)":
     *  one byte gaves two nibbles and may be used twice (once per field)
     * generating 4 codes.
     */
    p_pixeldata = vlc_alloc( p_sys->i_rle_size, sizeof(*p_pixeldata) * 2 * 2 );

    /* We try to display it */
    subpicture_data_t render_spu_data = *p_spu_data; /* Need a copy */
    if( ParseRLE( p_dec, &render_spu_data, p_spu_properties, p_pixeldata ) )
    {
        /* There was a parse error, delete the subpicture */
        subpicture_Delete( p_spu );
        free( p_pixeldata );
        return;
    }

#ifdef DEBUG_SPUDEC
    msg_Dbg( p_dec, "total size: 0x%x, RLE offsets: 0x%x 0x%x",
             p_sys->i_spu_size,
             render_spu_data.pi_offset[0], render_spu_data.pi_offset[1] );
#endif

    if( Render( p_dec, p_spu, p_pixeldata, &render_spu_data, p_spu_properties ) )
    {
        subpicture_Delete( p_spu );
        free( p_pixeldata );
        return;
    }

    free( p_pixeldata );

    if( p_spu_data->p_pxctli && p_spu )
        ParsePXCTLI( p_dec, p_spu_data, p_spu );

    pf_queue( p_dec, p_spu );
}

static int Validate( decoder_t *p_dec, unsigned i_index,
                     unsigned i_cur_seq, unsigned i_next_seq,
                     const subpicture_data_t *p_spu_data,
                     const spu_properties_t *p_spu_properties )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Check that the next sequence index matches the current one */
    if( i_next_seq < i_cur_seq )
    {
        msg_Err( p_dec, "index mismatch (0x%.4x < 0x%.4x)",
                 i_next_seq, i_cur_seq );
        return VLC_EGENERIC;
    }

    if( i_index > p_sys->i_spu_size )
    {
        msg_Err( p_dec, "uh-oh, we went too far (0x%.4x > 0x%.4x)",
                 i_index, p_sys->i_spu_size );
        return VLC_EGENERIC;
    }

    const int i_spu_size = p_sys->i_spu - 4;
    if( p_spu_data->pi_offset[0] < 0 || p_spu_data->pi_offset[0] >= i_spu_size ||
        p_spu_data->pi_offset[1] < 0 || p_spu_data->pi_offset[1] >= i_spu_size )
    {
        msg_Err( p_dec, "invalid offset values" );
        return VLC_EGENERIC;
    }

    if( p_spu_properties->i_start == VLC_TICK_INVALID )
    {
        msg_Err( p_dec, "no `start display' command" );
        return VLC_EGENERIC;
    }

    /* Get rid of padding bytes */
    if( i_index > i_next_seq && p_sys->i_spu_size > i_index + 1 )
    {
        /* Zero or one padding byte are quite usual
         * More than one padding byte - this is very strange, but
         * we can ignore them. */
        msg_Warn( p_dec, "%i padding bytes, we usually get 0 or 1 of them",
                  p_sys->i_spu_size - i_index );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ParsePacket: parse an SPU packet and send it to the video output
 *****************************************************************************
 * This function parses the SPU packet and, if valid, sends it to the
 * video output.
 *****************************************************************************/
void ParsePacket( decoder_t *p_dec, void(*pf_queue)(decoder_t *, subpicture_t *) )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Getting the control part */
    if( ParseControlSeq( p_dec, p_sys->i_pts, pf_queue ) )
        return;
}

/*****************************************************************************
 * ParseControlSeq: parse all SPU control sequences
 *****************************************************************************
 * This is the most important part in SPU decoding. We get dates, palette
 * information, coordinates, and so on. For more information on the
 * subtitles format, see http://sam.zoy.org/doc/dvd/subtitles/index.html
 *****************************************************************************/
static int ParseControlSeq( decoder_t *p_dec, vlc_tick_t i_pts,
                            void(*pf_queue)(decoder_t *, subpicture_t *) )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Our current index in the SPU packet */
    unsigned int i_index;

    /* The next start-of-control-sequence index and the previous one */
    unsigned int i_next_seq = 0, i_cur_seq = 0;

    /* Command and date */
    uint8_t i_command = SPU_CMD_END;
    vlc_tick_t date = 0;
    bool b_cmd_offset = false;
    bool b_cmd_alpha = false;

    /* Create working space for spu data */
    subpicture_data_t spu_data_cmd;
    memset( &spu_data_cmd, 0, sizeof(spu_data_cmd) );
    spu_data_cmd.pi_offset[0] = -1;
    spu_data_cmd.pi_offset[1] = -1;
    spu_data_cmd.b_palette = false;
    spu_data_cmd.b_auto_crop = false;
    spu_data_cmd.i_y_top_offset = 0;
    spu_data_cmd.i_y_bottom_offset = 0;
    spu_data_cmd.pi_alpha[0] = 0x00;
    spu_data_cmd.pi_alpha[1] = 0x0f;
    spu_data_cmd.pi_alpha[2] = 0x0f;
    spu_data_cmd.pi_alpha[3] = 0x0f;

    subpicture_data_t spu_data = spu_data_cmd;

    /* Initialize the structure */
    spu_properties_t spu_properties;
    memset( &spu_properties, 0, sizeof(spu_properties) );
    spu_properties.i_start = VLC_TICK_INVALID;
    spu_properties.i_stop = VLC_TICK_INVALID;
    spu_properties.b_subtitle = true;

    for( i_index = 4 + p_sys->i_rle_size; i_index < p_sys->i_spu_size ; )
    {
        /* If we just read a command sequence, read the next one;
         * otherwise, go on with the commands of the current sequence. */
        if( i_command == SPU_CMD_END )
        {
            if( i_index + 4 > p_sys->i_spu_size )
            {
                msg_Err( p_dec, "overflow in SPU command sequence" );
                return VLC_EGENERIC;
            }

            /* */
            b_cmd_offset = false;
            b_cmd_alpha = false;
            /* Get the control sequence date */
            date = VLC_TICK_FROM_MS(GetWBE( &p_sys->buffer[i_index] ) * 11);

            /* Next offset */
            i_cur_seq = i_index;
            i_next_seq = GetWBE( &p_sys->buffer[i_index+2] );

            if( i_next_seq > p_sys->i_spu_size )
            {
                msg_Err( p_dec, "overflow in SPU next command sequence" );
                return VLC_EGENERIC;
            }

            /* Skip what we just read */
            i_index += 4;
        }

        i_command = p_sys->buffer[i_index];

        switch( i_command )
        {
        case SPU_CMD_FORCE_DISPLAY: /* 00 (force displaying) */
            spu_properties.i_start = i_pts + date;
            spu_properties.b_ephemer = true;
            /* ignores picture date as display start time
             * works around non displayable (offset by few ms)
             * spu menu over still frame in SPU_Select */
            spu_properties.b_subtitle = false;
            i_index += 1;
            break;

        /* Convert the dates in seconds to PTS values */
        case SPU_CMD_START_DISPLAY: /* 01 (start displaying) */
            spu_properties.i_start = i_pts + date;
            i_index += 1;
            break;

        case SPU_CMD_STOP_DISPLAY: /* 02 (stop displaying) */
            spu_properties.i_stop = i_pts + date;
            i_index += 1;
            break;

        case SPU_CMD_SET_PALETTE:
            /* 03xxxx (palette) */
            if( i_index + 3 > p_sys->i_spu_size )
            {
                msg_Err( p_dec, "overflow in SPU command" );
                return VLC_EGENERIC;
            }

            if( p_dec->fmt_in.subs.spu.palette[0] == SPU_PALETTE_DEFINED )
            {
                uint8_t idx[4];

                spu_data_cmd.b_palette = true;

                idx[0] = (p_sys->buffer[i_index+1]>>4)&0x0f;
                idx[1] = (p_sys->buffer[i_index+1])&0x0f;
                idx[2] = (p_sys->buffer[i_index+2]>>4)&0x0f;
                idx[3] = (p_sys->buffer[i_index+2])&0x0f;

                CLUTIdxToYUV( &p_dec->fmt_in.subs, idx, spu_data_cmd.pi_yuv );
            }

            i_index += 3;
            break;

        case SPU_CMD_SET_ALPHACHANNEL: /* 04xxxx (alpha channel) */
            if( i_index + 3 > p_sys->i_spu_size )
            {
                msg_Err( p_dec, "overflow in SPU command" );
                return VLC_EGENERIC;
            }

            if(!p_sys->b_disabletrans)
            { /* If we want to use original transparency values */
                b_cmd_alpha = true;
                spu_data_cmd.pi_alpha[3] = (p_sys->buffer[i_index+1]>>4)&0x0f;
                spu_data_cmd.pi_alpha[2] = (p_sys->buffer[i_index+1])&0x0f;
                spu_data_cmd.pi_alpha[1] = (p_sys->buffer[i_index+2]>>4)&0x0f;
                spu_data_cmd.pi_alpha[0] = (p_sys->buffer[i_index+2])&0x0f;
            }

            i_index += 3;
            break;

        case SPU_CMD_SET_COORDINATES: /* 05xxxyyyxxxyyy (coordinates) */
            if( i_index + 7 > p_sys->i_spu_size )
            {
                msg_Err( p_dec, "overflow in SPU command" );
                return VLC_EGENERIC;
            }

            spu_properties.i_x = (p_sys->buffer[i_index+1]<<4)|
                         ((p_sys->buffer[i_index+2]>>4)&0x0f);
            spu_properties.i_width = (((p_sys->buffer[i_index+2]&0x0f)<<8)|
                              p_sys->buffer[i_index+3]) - spu_properties.i_x + 1;

            spu_properties.i_y = (p_sys->buffer[i_index+4]<<4)|
                         ((p_sys->buffer[i_index+5]>>4)&0x0f);
            spu_properties.i_height = (((p_sys->buffer[i_index+5]&0x0f)<<8)|
                              p_sys->buffer[i_index+6]) - spu_properties.i_y + 1;
            if (spu_properties.i_width < 0 || spu_properties.i_height < 0) {
                msg_Err( p_dec, "integer overflow in SPU command" );
                return VLC_EGENERIC;
            }

            /* Auto crop fullscreen subtitles */
            if( spu_properties.i_height > 250 )
                spu_data.b_auto_crop = true;

            i_index += 7;
            break;

        case SPU_CMD_SET_OFFSETS: /* 06xxxxyyyy (byte offsets) */
            if( i_index + 5 > p_sys->i_spu_size )
            {
                msg_Err( p_dec, "overflow in SPU command" );
                return VLC_EGENERIC;
            }

            b_cmd_offset = true;
            spu_data.pi_offset[0] = GetWBE(&p_sys->buffer[i_index+1]) - 4;
            spu_data.pi_offset[1] = GetWBE(&p_sys->buffer[i_index+3]) - 4;
            i_index += 5;
            break;

        case SPU_CMD_SET_COLCON:
            if( i_index + 7 > p_sys->i_spu_size )
            {
                msg_Err( p_dec, "overflow in SPU command" );
                return VLC_EGENERIC;
            }

            spu_properties.i_start = i_pts + date;

            if( p_sys->i_spu_size >
                i_index + 3 + 4 + (p_sys->buffer[i_index+5] >> 4) )
            {
                spu_data.p_pxctli = &p_sys->buffer[i_index+3 + 4];
                spu_data.i_pxclti = p_sys->buffer[i_index+5] >> 4;
            }

            i_index += 1 + GetWBE(&p_sys->buffer[i_index+1]);
            break;

        case SPU_CMD_END: /* ff (end) */

            if( b_cmd_offset )
            {
                /* It seems that palette and alpha from the block having
                 * the cmd offset have to be used
                 * XXX is it all ? */
                spu_data.b_palette = spu_data_cmd.b_palette;
                if( spu_data_cmd.b_palette )
                    memcpy( spu_data.pi_yuv, spu_data_cmd.pi_yuv, sizeof(spu_data_cmd.pi_yuv) );
                if( b_cmd_alpha )
                    memcpy( spu_data.pi_alpha, spu_data_cmd.pi_alpha, sizeof(spu_data_cmd.pi_alpha) );
            }

            i_index += 1;

            if( Validate( p_dec, i_index, i_cur_seq, i_next_seq,
                          &spu_data, &spu_properties ) == VLC_SUCCESS )
                OutputPicture( p_dec, &spu_data, &spu_properties, pf_queue );

            break;

        default: /* xx (unknown command) */
            msg_Warn( p_dec, "unknown SPU command 0x%.2x", i_command );
            if( i_index + 1 < i_next_seq )
            {
                 /* There is at least one other command sequence */
                 if( p_sys->buffer[i_next_seq - 1] == SPU_CMD_END )
                 {
                     /* This is consistent. Skip to that command sequence. */
                     i_index = i_next_seq;
                 }
                 else
                 {
                     /* There were other commands. */
                     msg_Warn( p_dec, "cannot recover, dropping subtitle" );
                     return VLC_EGENERIC;
                 }
            }
            else
            {
                /* We were in the last command sequence. Stop parsing by
                 * pretending we met an SPU_CMD_END command. */
                i_command = SPU_CMD_END;
                i_index++;
            }
        }

        /* */
        if( i_command == SPU_CMD_END && i_index != i_next_seq )
            break;
    }

    /* Successfully parsed ! */

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ParseRLE: parse the RLE part of the subtitle
 *****************************************************************************
 * This part parses the subtitle graphical data and stores it in a more
 * convenient structure for later decoding. For more information on the
 * subtitles format, see http://sam.zoy.org/doc/dvd/subtitles/index.html
 *****************************************************************************/
static int ParseRLE( decoder_t *p_dec,
                     subpicture_data_t *p_spu_data,
                     const spu_properties_t *p_spu_properties,
                     uint16_t *p_pixeldata )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    const unsigned int i_width = p_spu_properties->i_width;
    const unsigned int i_height = p_spu_properties->i_height;
    unsigned int i_x, i_y;

    uint16_t *p_dest = p_pixeldata;

    /* The subtitles are interlaced, we need two offsets */
    unsigned int  i_id = 0;                   /* Start on the even SPU layer */
    unsigned int  pi_table[ 2 ];
    unsigned int *pi_offset;

    /* Cropping */
    bool b_empty_top = true;
    unsigned int i_skipped_top = 0, i_skipped_bottom = 0;
    unsigned int i_transparent_code = 0;

    /* Colormap statistics */
    int i_border = -1;
    int stats[4]; stats[0] = stats[1] = stats[2] = stats[3] = 0;

    pi_table[ 0 ] = p_spu_data->pi_offset[ 0 ] << 1;
    pi_table[ 1 ] = p_spu_data->pi_offset[ 1 ] << 1;

    for( i_y = 0 ; i_y < i_height ; i_y++ )
    {
        unsigned int i_code;
        pi_offset = pi_table + i_id;

        for( i_x = 0 ; i_x < i_width ; i_x += i_code >> 2 )
        {
            i_code = 0;
            for( unsigned int i_min = 1; i_min <= 0x40 && i_code < i_min; i_min <<= 2 )
            {
                if( (*pi_offset >> 1) >= p_sys->i_spu_size )
                {
                    msg_Err( p_dec, "out of bounds while reading rle" );
                    return VLC_EGENERIC;
                }
                i_code = AddNibble( i_code, &p_sys->buffer[4], pi_offset );
            }
            if( i_code < 0x0004 )
            {
                /* If the 14 first bits are set to 0, then it's a
                 * new line. We emulate it. */
                i_code |= ( i_width - i_x ) << 2;
            }

            if( ( (i_code >> 2) + i_x + i_y * i_width ) > i_height * i_width )
            {
                msg_Err( p_dec, "out of bounds, %i at (%i,%i) is out of %ix%i",
                         i_code >> 2, i_x, i_y, i_width, i_height );
                return VLC_EGENERIC;
            }

            /* Try to find the border color */
            if( p_spu_data->pi_alpha[ i_code & 0x3 ] != 0x00 )
            {
                i_border = i_code & 0x3;
                stats[i_border] += i_code >> 2;
            }

            /* Auto crop subtitles (a lot more optimized) */
            if( p_spu_data->b_auto_crop )
            {
                if( !i_y )
                {
                    /* We assume that if the first line is transparent, then
                     * it is using the palette index for the
                     * (background) transparent color */
                    if( (i_code >> 2) == i_width &&
                        p_spu_data->pi_alpha[ i_code & 0x3 ] == 0x00 )
                    {
                        i_transparent_code = i_code;
                    }
                    else
                    {
                        p_spu_data->b_auto_crop = false;
                    }
                }

                if( i_code == i_transparent_code )
                {
                    if( b_empty_top )
                    {
                        /* This is a blank top line, we skip it */
                      i_skipped_top++;
                    }
                    else
                    {
                        /* We can't be sure the current lines will be skipped,
                         * so we store the code just in case. */
                      *p_dest++ = i_code;
                      i_skipped_bottom++;
                    }
                }
                else
                {
                    /* We got a valid code, store it */
                    *p_dest++ = i_code;

                    /* Valid code means no blank line */
                    b_empty_top = false;
                    i_skipped_bottom = 0;
                }
            }
            else
            {
                *p_dest++ = i_code;
            }
        }

        /* Check that we didn't go too far */
        if( i_x > i_width )
        {
            msg_Err( p_dec, "i_x overflowed, %i > %i", i_x, i_width );
            return VLC_EGENERIC;
        }

        /* Byte-align the stream */
        if( *pi_offset & 0x1 )
        {
            (*pi_offset)++;
        }

        /* Swap fields */
        i_id = ~i_id & 0x1;
    }

    /* We shouldn't get any padding bytes */
    if( i_y < i_height )
    {
        msg_Err( p_dec, "padding bytes found in RLE sequence" );
        msg_Err( p_dec, "send mail to <sam@zoy.org> if you "
                        "want to help debugging this" );

        /* Skip them just in case */
        while( i_y < i_height )
        {
            *p_dest++ = i_width << 2;
            i_y++;
        }

        return VLC_EGENERIC;
    }

#ifdef DEBUG_SPUDEC
    msg_Dbg( p_dec, "valid subtitle, size: %ix%i, position: %i,%i",
             p_spu_properties->i_width, p_spu_properties->i_height,
             p_spu_properties->i_x, p_spu_properties->i_y );
#endif

    /* Crop if necessary */
    if( i_skipped_top || i_skipped_bottom )
    {
        p_spu_data->i_y_top_offset = i_skipped_top;
        p_spu_data->i_y_bottom_offset = i_skipped_bottom;
#ifdef DEBUG_SPUDEC
        msg_Dbg( p_dec, "cropped to: %ix%i, position: %i,%i",
                 p_spu_properties->i_width,
                 p_spu_properties->i_height - (i_skipped_top + i_skipped_bottom),
                 p_spu_properties->i_x, p_spu_properties->i_y + i_skipped_top );
#endif
    }

    /* Handle color if no palette was found */
    if( !p_spu_data->b_palette )
    {
        int i, i_inner = -1, i_shade = -1;

        /* Set the border color */
        if( i_border != -1 )
        {
            p_spu_data->pi_yuv[i_border][0] = 0x00;
            p_spu_data->pi_yuv[i_border][1] = 0x80;
            p_spu_data->pi_yuv[i_border][2] = 0x80;
            stats[i_border] = 0;
        }

        /* Find the inner colors */
        for( i = 0 ; i < 4 && i_inner == -1 ; i++ )
        {
            if( stats[i] )
            {
                i_inner = i;
            }
        }

        for(       ; i < 4 && i_shade == -1 ; i++ )
        {
            if( stats[i] )
            {
                if( stats[i] > stats[i_inner] )
                {
                    i_shade = i_inner;
                    i_inner = i;
                }
                else
                {
                    i_shade = i;
                }
            }
        }

        /* Set the inner color */
        if( i_inner != -1 )
        {
            p_spu_data->pi_yuv[i_inner][0] = 0xff;
            p_spu_data->pi_yuv[i_inner][1] = 0x80;
            p_spu_data->pi_yuv[i_inner][2] = 0x80;
        }

        /* Set the anti-aliasing color */
        if( i_shade != -1 )
        {
            p_spu_data->pi_yuv[i_shade][0] = 0x80;
            p_spu_data->pi_yuv[i_shade][1] = 0x80;
            p_spu_data->pi_yuv[i_shade][2] = 0x80;
        }

#ifdef DEBUG_SPUDEC
        msg_Dbg( p_dec, "using custom palette (border %i, inner %i, shade %i)",
                 i_border, i_inner, i_shade );
#endif
    }

    return VLC_SUCCESS;
}

static int Render( decoder_t *p_dec, subpicture_t *p_spu,
                    const uint16_t *p_pixeldata,
                    const subpicture_data_t *p_spu_data,
                    const spu_properties_t *p_spu_properties )
{
    uint8_t *p_p;
    int i_x, i_y, i_len, i_color, i_pitch;
    const uint16_t *p_source = p_pixeldata;
    video_format_t fmt;
    video_palette_t palette;

    /* Create a new subpicture region */
    video_format_Init( &fmt, VLC_CODEC_YUVP );
    fmt.i_sar_num = 0; /* 0 means use aspect ratio of background video */
    fmt.i_sar_den = 1;
    fmt.i_width = fmt.i_visible_width = p_spu_properties->i_width;
    fmt.i_height = fmt.i_visible_height = p_spu_properties->i_height -
        p_spu_data->i_y_top_offset - p_spu_data->i_y_bottom_offset;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    fmt.p_palette = &palette;
    fmt.p_palette->i_entries = 4;
    for( i_x = 0; i_x < fmt.p_palette->i_entries; i_x++ )
    {
        fmt.p_palette->palette[i_x][0] = p_spu_data->pi_yuv[i_x][0];
        fmt.p_palette->palette[i_x][1] = p_spu_data->pi_yuv[i_x][1];
        fmt.p_palette->palette[i_x][2] = p_spu_data->pi_yuv[i_x][2];
        fmt.p_palette->palette[i_x][3] = p_spu_data->pi_alpha[i_x] * 0x11;
    }

    p_spu->p_region = subpicture_region_New( &fmt );
    if( !p_spu->p_region )
    {
        fmt.p_palette = NULL;
        video_format_Clean( &fmt );
        msg_Err( p_dec, "cannot allocate SPU region" );
        return VLC_EGENERIC;
    }

    p_spu->p_region->i_x = p_spu_properties->i_x;
    p_spu->p_region->i_y = p_spu_properties->i_y + p_spu_data->i_y_top_offset;
    p_p = p_spu->p_region->p_picture->p->p_pixels;
    i_pitch = p_spu->p_region->p_picture->p->i_pitch;

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0; i_y < (int)fmt.i_height * i_pitch; i_y += i_pitch )
    {
        /* Draw until we reach the end of the line */
        for( i_x = 0 ; i_x < (int)fmt.i_width; i_x += i_len )
        {
            /* Get the RLE part, then draw the line */
            i_color = *p_source & 0x3;
            i_len = *p_source++ >> 2;
            memset( p_p + i_x + i_y, i_color, i_len );
        }
    }

    fmt.p_palette = NULL;
    video_format_Clean( &fmt );

    return VLC_SUCCESS;
}
