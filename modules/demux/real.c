/*****************************************************************************
 * real.c: Real demuxer.
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/input.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin();
    set_description( _("Real demuxer" ) );
    set_capability( "demux2", 15 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_callbacks( Open, Close );
    add_shortcut( "real" );
    add_shortcut( "rm" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct
{
    int         i_id;
    es_format_t fmt;

    es_out_id_t *p_es;

    int         i_frame;
    block_t     *p_frame;

} real_track_t;

struct demux_sys_t
{
    int64_t  i_data_offset;
    int64_t  i_data_size;
    uint32_t i_data_packets_count;
    uint32_t i_data_packets;
    int64_t  i_data_offset_next;

    int          i_track;
    real_track_t **track;

    uint8_t buffer[65536];

    int64_t     i_pcr;
};

static int Demux( demux_t *p_demux );
static int Control( demux_t *p_demux, int i_query, va_list args );

static int HeaderRead( demux_t *p_demux );

/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    uint8_t     *p_peek;

    if( stream_Peek( p_demux->s, &p_peek, 10 ) < 10 ) return VLC_EGENERIC;
    if( strncmp( p_peek, ".RMF", 4 ) ) return VLC_EGENERIC;

    /* Fill p_demux field */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->i_data_offset = 0;
    p_sys->i_track = 0;
    p_sys->track   = NULL;
    p_sys->i_pcr   = 1;


    /* Parse the headers */
    if( HeaderRead( p_demux ) )
    {
        int i;
        msg_Err( p_demux, "invalid header" );
        for( i = 0; i < p_sys->i_track; i++ )
        {
            real_track_t *tk = p_sys->track[i];

            if( tk->p_es )
            {
                es_out_Del( p_demux->out, tk->p_es );
            }
            free( tk );
        }
        if( p_sys->i_track > 0 )
        {
            free( p_sys->track );
        }
        free( p_sys );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;
    int i;

    for( i = 0; i < p_sys->i_track; i++ )
    {
        real_track_t *tk = p_sys->track[i];

        if( tk->p_frame )
        {
            block_Release( tk->p_frame );
        }
        free( tk );
    }

    if( p_sys->i_track > 0 )
    {
        free( p_sys->track );
    }

    free( p_sys );
}


/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint8_t     header[18];
    int         i_size;
    int         i_id;
    int64_t     i_pts;
    int         i;
    real_track_t *tk = NULL;
    vlc_bool_t  b_selected;

    if( p_sys->i_data_packets >= p_sys->i_data_packets_count )
    {

        if( stream_Read( p_demux->s, header, 18 ) < 18 )
        {
            return 0;
        }
        if( strncmp( header, "DATA", 4 ) )
        {
            return 0;
        }
        p_sys->i_data_offset = stream_Tell( p_demux->s ) - 18;
        p_sys->i_data_size   = GetDWBE( &header[4] );
        p_sys->i_data_packets_count = GetDWBE( &header[10] );
        p_sys->i_data_packets = 0;
        p_sys->i_data_offset_next = GetDWBE( &header[14] );

        msg_Dbg( p_demux, "entering new DATA packets=%d next=%u",
                 p_sys->i_data_packets_count,
                 (uint32_t)p_sys->i_data_offset_next );
    }

    if( stream_Read( p_demux->s, header, 12 ) < 12 )
    {
        return 0;
    }
    i_size = GetWBE( &header[2] ) - 12;
    i_id   = GetWBE( &header[4] );
    i_pts  = 1000 * GetDWBE( &header[6] );
    /* header[11] -> flags 0x02 -> keyframe */
    msg_Dbg( p_demux, "packet %d size=%d id=%d pts=%u",
             p_sys->i_data_packets, i_size, i_id, (uint32_t)(i_pts/1000) );
    p_sys->i_data_packets++;

    stream_Read( p_demux->s, p_sys->buffer, i_size );

    for( i = 0; i < p_sys->i_track; i++ )
    {
        if( p_sys->track[i]->i_id == i_id )
        {
            tk = p_sys->track[i];
        }
    }

    if( tk == NULL )
    {
        msg_Warn( p_demux, "unknown track id(0x%x)", i_id );
        return 1;
    }
    es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, tk->p_es, &b_selected );

    if( tk->fmt.i_cat == VIDEO_ES && b_selected )
    {
        uint8_t     *p = p_sys->buffer;

        while( p < &p_sys->buffer[i_size - 2] )
        {
            uint8_t h = *p++;
            int     i_len = 0;
            int     i_copy;
            int     i_subseq = 0;
            int     i_seqnum = 0;
            int     i_offset = 0;

            if( (h&0xc0) == 0x40 )
            {
                /* Short header */
                p++;
                i_len = &p_sys->buffer[i_size] - p;
            }
            else
            {
                if( (h&0x40) == 0 )
                {
                    i_subseq = (*p++)&0x7f;
                }
                i_len = (p[0] << 8)|p[1]; p += 2;
                if( (i_len&0xc000) == 0 )
                {
                    i_len <<= 16;
                    i_len |= (p[0] << 8)|p[1]; p += 2;
                    i_len &= 0x3fffffff;
                }
                else
                {
                    i_len &= 0x3fff;
                }

                i_offset = (p[0] << 8)|p[1]; p += 2;
                if( (i_offset&0xc000) == 0 )
                {
                    i_offset <<= 16;
                    i_offset |= (p[0] << 8)|p[1]; p += 2;
                    i_offset &= 0x3fffffff;
                }
                else
                {
                    i_offset &= 0x3fff;
                }
                i_seqnum = *p++;
            }
            i_copy = i_len - i_offset;
            if( i_copy > &p_sys->buffer[i_size] - p )
            {
                i_copy = &p_sys->buffer[i_size] - p;
            }
            else if( i_copy < 0 )
            {
                break;
            }

            msg_Dbg( p_demux, "    - len=%d offset=%d size=%d subseq=%d seqnum=%d",
                     i_len, i_offset, i_copy, i_subseq, i_seqnum );

            if( (h&0xc0) == 0x80 )
            {
                /* last fragment -> fixes */
                i_copy = i_offset;
                i_offset = i_len - i_copy;
                msg_Dbg( p_demux, "last fixing copy=%d offset=%d", i_copy, i_offset );
            }

            if( tk->p_frame &&
                ( tk->p_frame->i_dts != i_pts ||
                  tk->i_frame != i_len ) )
            {
                msg_Dbg( p_demux, "sending size=%d", tk->p_frame->i_buffer );

                if( p_sys->i_pcr < tk->p_frame->i_dts )
                {
                    p_sys->i_pcr = tk->p_frame->i_dts;

                    es_out_Control( p_demux->out, ES_OUT_SET_PCR, (int64_t)p_sys->i_pcr );
                }
                es_out_Send( p_demux->out, tk->p_es, tk->p_frame );

                tk->i_frame = 0;
                tk->p_frame = NULL;
            }

            if( (h&0xc0) != 0x80 && (h&0xc0) != 0x00 && tk->p_frame == NULL )
            {
                /* no fragment */
                i_len = i_copy;
                i_offset = 0;
            }


            if( tk->p_frame == NULL )
            {
                msg_Dbg( p_demux, "new frame size=%d", i_len );
                tk->i_frame = i_len;
                if( ( tk->p_frame = block_New( p_demux, i_len + 8 + 1000) ) == NULL )
                {
                    return -1;
                }
                memset( &tk->p_frame->p_buffer[8], 0, i_len );
                tk->p_frame->i_dts = i_pts;
                tk->p_frame->i_pts = i_pts;

                ((uint32_t*)tk->p_frame->p_buffer)[0] = i_len;  /* len */
                ((uint32_t*)tk->p_frame->p_buffer)[1] = 0;      /* chunk counts */
            }

            if( i_offset < tk->i_frame)
            {
                int i_ck = ((uint32_t*)tk->p_frame->p_buffer)[1]++;

                msg_Dbg( p_demux, "copying new buffer n=%d offset=%d copy=%d", i_ck, i_offset, i_copy );

                ((uint32_t*)(tk->p_frame->p_buffer+i_len+8))[i_ck] = i_offset;

                memcpy( &tk->p_frame->p_buffer[i_offset + 8],
                        p, i_copy );
            }

            p += i_copy;

            if( (h&0xc0) != 0x80 )
            {
                break;
            }
#if 0
            if( tk->p_frame )
            {
                /* append data */
                int i_ck = ((uint32_t*)tk->p_frame->p_buffer)[1]++;

                if( (h&0xc0) == 0x80 )
                {
                    /* last fragment */
                    i_copy = i_offset;
                    i_offset = i_len - i_offset;

                    ((uint32_t*)(tk->p_frame->p_buffer+i_len+8))[i_ck] = i_offset;
                    memcpy( &tk->p_frame->p_buffer[i_offset+ 8], p, i_copy );
                    p += i_copy;

                    if( p_sys->i_pcr < tk->p_frame->i_dts )
                    {
                        p_sys->i_pcr = tk->p_frame->i_dts;
                        es_out_Control( p_demux->out, ES_OUT_SET_PCR, (int64_t)p_sys->i_pcr );
                    }
                    es_out_Send( p_demux->out, tk->p_es, tk->p_frame );

                    tk->i_frame = 0;
                    tk->p_frame = NULL;

                    continue;
                }

                ((uint32_t*)(tk->p_frame->p_buffer+i_len+8))[i_ck] = i_offset;
                memcpy( &tk->p_frame->p_buffer[i_offset + 8], p, i_copy );
                break;
            }

            if( (h&0xc0) != 0x00 )
            {
                block_t *p_frame;

                /* not fragmented */
                if( ( p_frame = block_New( p_demux, i_copy + 8 + 8 ) ) == NULL )
                {
                    return -1;
                }
                p_frame->i_dts = i_pts;
                p_frame->i_pts = i_pts;

                ((uint32_t*)p_frame->p_buffer)[0] = i_copy;
                ((uint32_t*)p_frame->p_buffer)[1] = 1;
                ((uint32_t*)(p_frame->p_buffer+i_copy+8))[0] = 0;
                memcpy( &p_frame->p_buffer[8], p, i_copy );

                p += i_copy;

                if( p_sys->i_pcr < p_frame->i_dts )
                {
                    p_sys->i_pcr = p_frame->i_dts;
                    es_out_Control( p_demux->out, ES_OUT_SET_PCR, (int64_t)p_sys->i_pcr );
                }
                es_out_Send( p_demux->out, tk->p_es, p_frame );
            }
            else
            {
                /* First fragment */
                tk->i_frame = i_len;
                if( ( tk->p_frame = block_New( p_demux, i_len + 8 + 1000) ) == NULL )
                {
                    return -1;
                }
                memset( &tk->p_frame->p_buffer[8], 0, i_len );
                tk->p_frame->i_dts = i_pts;
                tk->p_frame->i_pts = i_pts;

                ((uint32_t*)tk->p_frame->p_buffer)[0] = i_len;  /* len */
                ((uint32_t*)tk->p_frame->p_buffer)[1] = 1;      /* chunk counts */
                ((uint32_t*)(tk->p_frame->p_buffer+i_len+8))[0] = i_offset;
                memcpy( &tk->p_frame->p_buffer[i_offset + 8], p, i_copy );

                break;
            }
#endif
        }
    }
    else if( tk->fmt.i_cat == AUDIO_ES && b_selected )
    {
        /* Set PCR */
        if( p_sys->i_pcr < i_pts )
        {
            p_sys->i_pcr = i_pts;
            es_out_Control( p_demux->out, ES_OUT_SET_PCR, (int64_t)p_sys->i_pcr );
        }

        if( tk->fmt.i_codec == VLC_FOURCC( 'm', 'p', '4', 'a' ) )
        {
            int     i_sub = (p_sys->buffer[1] >> 4)&0x0f;
            uint8_t *p_sub = &p_sys->buffer[2+2*i_sub];

            int i;
            for( i = 0; i < i_sub; i++ )
            {
                int i_sub_size = GetWBE( &p_sys->buffer[2+i*2]);
                block_t *p_block = block_New( p_demux, i_sub_size );
                if( p_block )
                {
                    memcpy( p_block->p_buffer, p_sub, i_sub_size );
                    p_sub += i_sub_size;

                    p_block->i_dts =
                    p_block->i_pts = ( i == 0 ? i_pts : 0 );

                    es_out_Send( p_demux->out, tk->p_es, p_block );
                }
            }
        }
        else
        {
            block_t *p_block = block_New( p_demux, i_size );

            if( tk->fmt.i_codec == VLC_FOURCC( 'a', '5', '2', ' ' ) )
            {
                uint8_t *src = p_sys->buffer;
                uint8_t *dst = p_block->p_buffer;

                /* byte swap data */
                while( dst < &p_block->p_buffer[i_size- 1])
                {
                    *dst++ = src[1];
                    *dst++ = src[0];

                    src += 2;
                }
            }
            else
            {
                memcpy( p_block->p_buffer, p_sys->buffer, i_size );
            }
            p_block->i_dts =
            p_block->i_pts = i_pts;

            es_out_Send( p_demux->out, tk->p_es, p_block );
        }
    }


    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
#if 0
    demux_sys_t *p_sys = p_demux->p_sys;
    double f, *pf;
    int64_t i64, *pi64;

    switch( i_query )
    {
        case DEMUX_GET_POSITION:
            pf = (double*) va_arg( args, double* );
            i64 = stream_Size( p_demux->s );
            if( i64 > 0 )
            {
                *pf = (double)stream_Tell( p_demux->s ) / (double)i64;
            }
            else
            {
                *pf = 0.0;
            }
            return VLC_SUCCESS;
        case DEMUX_SET_POSITION:
            f = (double) va_arg( args, double );
            i64 = stream_Size( p_demux->s );

            es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

            return stream_Seek( p_demux->s, (int64_t)(i64 * f) );

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_mux_rate > 0 )
            {
                *pi64 = (int64_t)1000000 * ( stream_Tell( p_demux->s ) / 50 ) / p_sys->i_mux_rate;
                return VLC_SUCCESS;
            }
            *pi64 = 0;
            return VLC_EGENERIC;

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_mux_rate > 0 )
            {
                *pi64 = (int64_t)1000000 * ( stream_Size( p_demux->s ) / 50 ) / p_sys->i_mux_rate;
                return VLC_SUCCESS;
            }
            *pi64 = 0;
            return VLC_EGENERIC;

        case DEMUX_SET_TIME:
        case DEMUX_GET_FPS:
        default:
            return VLC_EGENERIC;
    }
#endif
    return VLC_EGENERIC;
}

/*****************************************************************************
 * HeaderRead:
 *****************************************************************************/
static int HeaderRead( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint8_t header[100];    /* FIXME */

    uint32_t    i_id;
    uint32_t    i_size;
    int64_t     i_skip;
    int         i_version;

    for( ;; )
    {
        /* Read the header */
        if( stream_Read( p_demux->s, header, 10 ) < 10 )
        {
            return VLC_EGENERIC;
        }
        i_id        = VLC_FOURCC( header[0], header[1], header[2], header[3] );
        i_size      = GetDWBE( &header[4] );
        i_version   = GetWBE( &header[8] );

        msg_Dbg( p_demux, "object %4.4s size=%d version=%d",
                 (char*)&i_id, i_size, i_version );

        if( i_size < 10 )
        {
            return VLC_EGENERIC;
        }
        i_skip = i_size - 10;

        if( i_id == VLC_FOURCC('.','R','M','F') )
        {
            if( stream_Read( p_demux->s, header, 8 ) < 8 )
            {
                return VLC_EGENERIC;
            }
            msg_Dbg( p_demux, "    - file version=0x%x num headers=%d",
                     GetDWBE( &header[0] ), GetDWBE( &header[4] ) );

            i_skip -= 8;
        }
        else if( i_id == VLC_FOURCC('P','R','O','P') )
        {
            int i_flags;

            if( stream_Read( p_demux->s, header, 40 ) < 40 )
            {
                return VLC_EGENERIC;
            }
            msg_Dbg( p_demux, "    - max bitrate=%d avg bitrate=%d",
                     GetDWBE( &header[0] ), GetDWBE( &header[4] ) );
            msg_Dbg( p_demux, "    - max packet size=%d avg bitrate=%d",
                     GetDWBE( &header[8] ), GetDWBE( &header[12] ) );
            msg_Dbg( p_demux, "    - packets count=%d", GetDWBE( &header[16] ) );
            msg_Dbg( p_demux, "    - duration=%d ms", GetDWBE( &header[20] ) );
            msg_Dbg( p_demux, "    - preroll=%d ms", GetDWBE( &header[24] ) );
            msg_Dbg( p_demux, "    - index offset=%d", GetDWBE( &header[28] ) );
            msg_Dbg( p_demux, "    - data offset=%d", GetDWBE( &header[32] ) );
            msg_Dbg( p_demux, "    - num streams=%d", GetWBE( &header[36] ) );
            i_flags = GetWBE( &header[38]);
            msg_Dbg( p_demux, "    - flags=0x%x %s%s%s",
                     i_flags,
                     i_flags&0x0001 ? "PN_SAVE_ENABLED " : "",
                     i_flags&0x0002 ? "PN_PERFECT_PLAY_ENABLED " : "",
                     i_flags&0x0004 ? "PN_LIVE_BROADCAST" : "" );
            i_skip -= 40;
        }
        else if( i_id == VLC_FOURCC('C','O','N','T') )
        {
            int i_len;
            char *psz;

            stream_Read( p_demux->s, header, 2 );
            if( ( i_len = GetWBE( header ) ) > 0 )
            {
                psz = malloc( i_len + 1 );
                stream_Read( p_demux->s, psz, i_len );
                psz[i_len] = '\0';

                msg_Dbg( p_demux, "    - title=`%s'", psz );
                free( psz );
                i_skip -= i_len;
            }
            i_skip -= 2;

            stream_Read( p_demux->s, header, 2 );
            if( ( i_len = GetWBE( header ) ) > 0 )
            {
                psz = malloc( i_len + 1 );
                stream_Read( p_demux->s, psz, i_len );
                psz[i_len] = '\0';

                msg_Dbg( p_demux, "    - author=`%s'", psz );
                free( psz );
                i_skip -= i_len;
            }
            i_skip -= 2;

            stream_Read( p_demux->s, header, 2 );
            if( ( i_len = GetWBE( header ) ) > 0 )
            {
                psz = malloc( i_len + 1 );
                stream_Read( p_demux->s, psz, i_len );
                psz[i_len] = '\0';

                msg_Dbg( p_demux, "    - copyright=`%s'", psz );
                free( psz );
                i_skip -= i_len;
            }
            i_skip -= 2;

            stream_Read( p_demux->s, header, 2 );
            if( ( i_len = GetWBE( header ) ) > 0 )
            {
                psz = malloc( i_len + 1 );
                stream_Read( p_demux->s, psz, i_len );
                psz[i_len] = '\0';

                msg_Dbg( p_demux, "    - comment=`%s'", psz );
                free( psz );
                i_skip -= i_len;
            }
            i_skip -= 2;
        }
        else if( i_id == VLC_FOURCC('M','D','P','R') )
        {
            int  i_num;
            int  i_len;
            char *psz;

            if( stream_Read( p_demux->s, header, 30 ) < 30 )
            {
                return VLC_EGENERIC;
            }
            i_num = GetWBE( header );
            msg_Dbg( p_demux, "    - id=0x%x", i_num );
            msg_Dbg( p_demux, "    - max bitrate=%d avg bitrate=%d", GetDWBE( &header[2] ), GetDWBE( &header[6] ) );
            msg_Dbg( p_demux, "    - max packet size=%d avg packet size=%d", GetDWBE( &header[10] ), GetDWBE( &header[14] ));
            msg_Dbg( p_demux, "    - start time=%d", GetDWBE( &header[18] ) );
            msg_Dbg( p_demux, "    - preroll=%d", GetDWBE( &header[22] ) );
            msg_Dbg( p_demux, "    - duration=%d", GetDWBE( &header[26] ));
            i_skip -= 30;


            stream_Read( p_demux->s, header, 1 );
            if( ( i_len = header[0] ) > 0 )
            {
                psz = malloc( i_len + 1 );
                stream_Read( p_demux->s, psz, i_len );
                psz[i_len] = '\0';

                msg_Dbg( p_demux, "    - name=`%s'", psz );
                free( psz );
                i_skip -= i_len;
            }
            i_skip--;

            stream_Read( p_demux->s, header, 1 );
            if( ( i_len = header[0] ) > 0 )
            {
                psz = malloc( i_len + 1 );
                stream_Read( p_demux->s, psz, i_len );
                psz[i_len] = '\0';

                msg_Dbg( p_demux, "    - mime=`%s'", psz );
                free( psz );
                i_skip -= i_len;
            }
            i_skip--;

            stream_Read( p_demux->s, header, 4 );
            if( ( i_len = GetDWBE( header ) ) > 0 )
            {
                es_format_t  fmt;
                real_track_t *tk;
                uint8_t *p_peek;

                msg_Dbg( p_demux, "    - specific data len=%d", i_len );
                if( stream_Peek( p_demux->s, &p_peek, 34 ) >= 34 )
                {
                    if( !strncmp( &p_peek[4], "VIDO", 4 ) )
                    {
                        es_format_Init( &fmt, VIDEO_ES,
                                        VLC_FOURCC( p_peek[8], p_peek[9], p_peek[10], p_peek[11] ) );
                        fmt.video.i_width = GetWBE( &p_peek[12] );
                        fmt.video.i_height= GetWBE( &p_peek[14] );

                        fmt.i_extra = 8;
                        fmt.p_extra = malloc( 8 );
                        ((uint32_t*)fmt.p_extra)[0] = GetDWBE( &p_peek[26] );
                        ((uint32_t*)fmt.p_extra)[1] = GetDWBE( &p_peek[30] );

                        msg_Dbg( p_demux, "    - video 0x%08x 0x%08x",
                                 ((uint32_t*)fmt.p_extra)[0],
                                 ((uint32_t*)fmt.p_extra)[1] );

                        if( GetDWBE( &p_peek[30] ) == 0x10003000 ||
                            GetDWBE( &p_peek[30] ) == 0x10003001 )
                        {
                            fmt.i_codec = VLC_FOURCC( 'R','V','1','3' );
                        }

                        msg_Dbg( p_demux, "    - video %4.4s %dx%d",
                                 (char*)&fmt.i_codec,
                                 fmt.video.i_width, fmt.video.i_height );

                        tk = malloc( sizeof( real_track_t ) );
                        tk->i_id = i_num;
                        tk->fmt = fmt;
                        tk->i_frame = 0;
                        tk->p_frame = NULL;
                        tk->p_es = es_out_Add( p_demux->out, &fmt );

                        TAB_APPEND( p_sys->i_track, p_sys->track, tk );
                    }
                    else if( !strncmp( p_peek, ".ra\xfd", 4 ) )
                    {
                        int     i_version = GetWBE( &p_peek[4] );
                        uint8_t *p_extra = NULL;
                        msg_Dbg( p_demux, "    - audio version=%d", i_version );

                        es_format_Init( &fmt, AUDIO_ES, 0 );
                        if( i_version == 4 && stream_Peek( p_demux->s, &p_peek, 56 ) >= 56 )
                        {
                            fmt.audio.i_channels = GetWBE( &p_peek[54] );
                            fmt.audio.i_rate = GetWBE( &p_peek[48] );

                            if( stream_Peek( p_demux->s, &p_peek, 57 ) >= 57 )
                            {
                                int i_extra = p_peek[56] + 1 + 4;
                                if( stream_Peek( p_demux->s, &p_peek, 57 + i_extra ) >= 57 + i_extra )
                                {
                                    memcpy( &fmt.i_codec, &p_peek[57 + p_peek[56] + 1], 4 );
                                }
                                p_extra = &p_peek[57 + p_peek[56] + 1+ 4 + 3];
                            }
                        }
                        else if( i_version == 5 && stream_Peek( p_demux->s, &p_peek, 70 ) >= 70 )
                        {
                            memcpy( &fmt.i_codec, &p_peek[66], 4 );
                            fmt.audio.i_channels = GetWBE( &p_peek[60] );
                            fmt.audio.i_rate = GetWBE( &p_peek[54] );

                            p_extra = &p_peek[66+4+3+1];
                        }
                        msg_Dbg( p_demux, "    - audio codec=%4.4s channels=%d rate=%dHz",
                                (char*)&fmt.i_codec,
                                fmt.audio.i_channels, fmt.audio.i_rate );

                        if( fmt.i_codec == VLC_FOURCC( 'd', 'n', 'e', 't' ) )
                        {
                            fmt.i_codec = VLC_FOURCC( 'a', '5', '2', ' ' );
                        }
                        else if( fmt.i_codec == VLC_FOURCC( 'r', 'a', 'a', 'c' ) ||
                                 fmt.i_codec == VLC_FOURCC( 'r', 'a', 'c', 'p' ) )
                        {
                            int i_peek = p_extra - p_peek;
                            if( stream_Peek( p_demux->s, &p_peek, i_peek + 4 ) >= i_peek + 4 )
                            {
                                int i_extra = GetDWBE( &p_peek[i_peek] );

                                if( i_extra > 1 && i_peek + 4 + i_extra <= i_len &&
                                    stream_Peek( p_demux->s, &p_peek, i_peek + 4 + i_extra ) >= i_peek + 4 + i_extra )
                                {
                                    fmt.i_extra = i_extra - 1;
                                    fmt.p_extra = malloc ( i_extra - 1 );
                                    memcpy( fmt.p_extra, &p_peek[i_peek+4+1], i_extra - 1 );

                                    msg_Dbg( p_demux, "        - extra data=%d", i_extra );
                                    {
                                        int i;
                                        for( i = 0; i < fmt.i_extra; i++ )
                                        {
                                            msg_Dbg( p_demux, "          data[%d] = 0x%x", i,((uint8_t*)fmt.p_extra)[i] );
                                        }
                                    }
                                }
                            }
                            fmt.i_codec = VLC_FOURCC( 'm', 'p', '4', 'a' );
                        }

                        if( fmt.i_codec != 0 )
                        {
                            tk = malloc( sizeof( real_track_t ) );
                            tk->i_id = i_num;
                            tk->fmt = fmt;
                            tk->i_frame = 0;
                            tk->p_frame = NULL;
                            tk->p_es = es_out_Add( p_demux->out, &fmt );

                            TAB_APPEND( p_sys->i_track, p_sys->track, tk );
                        }
                    }
                }
                stream_Read( p_demux->s, NULL, i_len );

                i_skip -= i_len;
            }
            i_skip -= 4;
        }
        else if( i_id == VLC_FOURCC('D','A','T','A') )
        {
            stream_Read( p_demux->s, header, 8 );

            p_sys->i_data_offset    = stream_Tell( p_demux->s ) - 10;
            p_sys->i_data_size      = i_size;
            p_sys->i_data_packets_count = GetDWBE( header );
            p_sys->i_data_packets   = 0;
            p_sys->i_data_offset_next = GetDWBE( &header[4] );

            msg_Dbg( p_demux, "    - packets count=%d next=%u",
                     p_sys->i_data_packets_count,
                     (uint32_t)p_sys->i_data_offset_next );

            /* we have finished the header */
            break;
        }
        else
        {
            /* unknow header */
            msg_Dbg( p_demux, "unknown chunk" );
        }

        if( i_skip < 0 )
        {
            return VLC_EGENERIC;
        }
        stream_Read( p_demux->s, NULL, i_skip );
    }

    /* TODO read index if possible */

    return VLC_SUCCESS;
}


