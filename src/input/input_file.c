/*****************************************************************************
 * input_file.c: functions to read from a file
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdio.h>
#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                            /* "input.h" */
#include <sys/stat.h>                                    /* fstat, off_t ... */
#include <byteorder.h>                                          /* ntohl ... */
#include <malloc.h>                                      /* malloc, read ... */
#include <string.h>


#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "intf_msg.h"

#include "input.h"
#include "input_file.h"

#define BUF_SIZE (797*3)

#define PS_METHOD 1
#define TS_METHOD 2

#define TS_PACKET_SIZE 188
#define TS_IN_UDP 7

#define PS_BUFFER_SIZE 16384
#define NO_PES 0
#define AUDIO_PES 1
#define VIDEO_PES 2
#define AC3_PES 3
#define SUBTITLE_PES 4
#define PRIVATE_PES 5
#define UNKNOWN_PES 12

#define PCR_PID 0x20 /* 0x20 == first video stream
                      * 0x40 == first audio stream */

typedef u8 file_ts_packet[TS_PACKET_SIZE];
typedef file_ts_packet udp_packet[TS_IN_UDP];

typedef struct synchro_struct
{
    mtime_t     delta_clock;
    mtime_t     slope;
    mtime_t     last_pcr_time;
    
    file_ts_packet *last_pcr;
} synchro_t;

typedef struct in_data_s
{
    int start, end;
    vlc_mutex_t lock;
    vlc_cond_t notfull;
    vlc_cond_t notempty;
    udp_packet buf[BUF_SIZE+1];
} in_data_t;

typedef struct own_pcr_s
{
    int start, end;
    vlc_mutex_t lock;
    file_ts_packet *buf[(BUF_SIZE+1)*TS_IN_UDP+1];
} own_pcr_t;

typedef struct options_s
{
    unsigned int pcr_pid;
    u8 i_file_type;
    int in; 
} options_t;

typedef struct s_ps
{
    unsigned int pat_counter;
    unsigned int pmt_counter;
    /* 
     * 16 audio mpeg streams
     * 16 audio AV3 streams
     * 16 video mpeg streams
     * 32 subtitle streams
     */
    unsigned int media_counter[0x100];
    unsigned int association_table[0x100];
    unsigned int found_streams;

    unsigned int found_pts;

    unsigned int ts_to_write;
    unsigned int ts_written;
    unsigned int sent_ts;

    unsigned char *ps_data;
    unsigned char *ps_end;
    unsigned char *ps_buffer;

    unsigned int pes_id;
    unsigned int private_id;
    unsigned int has_pts;
    unsigned int pcr_pid;

    unsigned int pes_type;
    unsigned int pes_size;
    unsigned int to_skip;
    unsigned int offset;
} ps_t;

typedef struct input_file_s
{
    boolean_t    b_die; /* b_die flag for the disk thread */
    vlc_thread_t disk_thread;

    synchro_t    synchro;
    ps_t         ps;
    in_data_t    in_data;
    options_t    options;
    own_pcr_t    own_pcr;
} input_file_t;

/* local prototypes */
void ps_fill( input_file_t * p_if, boolean_t wait );
ssize_t safe_read(int fd, unsigned char *buf, int count);
void input_DiskThread( input_file_t * p_if );
int init_synchro( input_file_t * p_if );

input_file_t input_file;

/******************************************************************************
 * ConvertPCRTime : extracts and converts the PCR time in microseconds
 ******************************************************************************/

s64 ConvertPCRTime(file_ts_packet *pcr_buff)
{
    return( (((((s64)U32_AT(((u8*)pcr_buff)+6)) << 1) | (((u8*)pcr_buff)[10] >> 7)) * 300) / 27 );
}

/******************************************************************************
 * wait_a_moment : Compute how long we must wait before sending a TS packet
 ******************************************************************************/

static void wait_a_moment( input_file_t * p_if, file_ts_packet *ts)
{
    synchro_t * p_synchro = &input_file.synchro;
    
    static int retard_count = 0;
    static s64 wait_max = 0;
    s64 sendtime; /* the date at which the TS packet should be sent */
    s64 wait;
    
    sendtime = p_synchro->last_pcr_time + p_synchro->delta_clock +
        p_synchro->slope * ((ts - p_synchro->last_pcr + (BUF_SIZE+1)*TS_IN_UDP) % ((BUF_SIZE+1)*TS_IN_UDP)); 
    wait = sendtime - mdate();
    //fprintf(stderr,"last  PCR Time : %Ld\n", p_synchro->last_pcr_time );
    if( wait > 0 )
    { 
        retard_count = 0;
        if(wait > 100000)
        {
            fprintf( stderr, "Warning : wait time may be too long : %Ld\n", wait );
            return;
        }
        msleep( wait );
    }
    else
    {
        if( wait < wait_max )
        {
            wait_max = wait;
        }
        retard_count++;
        if( retard_count == 16 )
        {
            retard_count = 0;
            //fprintf( stderr, "delay : %Ldms, max delay : %Ldms\n", -wait/1000, -wait_max/1000 );
            fflush(stdout);
        }
    }
}

/******************************************************************************
 * adjust : Adjust the encoder clock & remove the PCR from own_pcr
 ******************************************************************************/

static void adjust( input_file_t * p_if, file_ts_packet *ts )
{
    synchro_t * p_synchro = &p_if->synchro;
    own_pcr_t * p_own_pcr = &p_if->own_pcr;
    file_ts_packet *next_pcr;
    int no_discontinuity = 1;
    
    if( ((u8*)ts)[5] & 0x80 )
    {
        /* There is a discontinuity - I recalculate the delta */
        p_synchro->delta_clock = mdate() - ConvertPCRTime(ts);
        intf_DbgMsg( "input warning: clock discontinuity\n" );
        no_discontinuity = 0;
    }
    else
    {
        p_synchro->last_pcr = ts;
        p_synchro->last_pcr_time = ConvertPCRTime( ts );
    }
        
    vlc_mutex_lock(&p_own_pcr->lock);
    p_own_pcr->start++;
    p_own_pcr->start %= (BUF_SIZE+1)*TS_IN_UDP+1;
    
    /* If we have 2 consecutiv PCR, we can reevaluate slope */
    if( (p_own_pcr->start != p_own_pcr->end) &&
        no_discontinuity &&
        !((((u8*) next_pcr = p_own_pcr->buf[p_own_pcr->start]))[5] & 0x80))
    {
        s64 current_pcr_time = ConvertPCRTime(ts);
        s64 next_pcr_time =    ConvertPCRTime(next_pcr);
        
        if( (next_pcr_time - current_pcr_time < 0) || (next_pcr_time - current_pcr_time > 700000))
        {
            fprintf( stderr, "Warning: possible discontinuity\n" );
            p_synchro->delta_clock = mdate() - next_pcr_time;
        }
        else
        {
                //fprintf(stderr,"next - current : %Ld\n", next_pcr_time - current_pcr_time);
            p_synchro->slope = (next_pcr_time - current_pcr_time) /
                ((next_pcr - ts + (BUF_SIZE+1)*TS_IN_UDP) % ((BUF_SIZE+1)*TS_IN_UDP));
                //fprintf(stderr,"slope : %Ld\n", p_synchro->slope);
        }
    }
    
    vlc_mutex_unlock(&p_own_pcr->lock);
}

/******************************************************************************
 * safe_read : Buffered reading method
 ******************************************************************************/

ssize_t safe_read(int fd, unsigned char *buf, int count)
{
    int ret, cnt=0;

    while(cnt < count)
    {
        ret = read(fd, buf+cnt, count-cnt);
        if(ret < 0)
            return ret;
        if(ret == 0)
            break;
        cnt += ret;
    }

    return cnt;
}

/******************************************************************************
 * keep_pcr : Put a TS packet in the fifo if it owns a PCR
 ******************************************************************************/

int keep_pcr(int pcr_pid, file_ts_packet *ts)
{
    own_pcr_t * p_own_pcr =   &input_file.own_pcr;

#define p ((u8 *)ts)
    if ((p[3] & 0x20) && p[4] && (p[5] & 0x10)
        && ((((p[1]<<8)+p[2]) & 0x1FFF) == pcr_pid))
    {
        /* adaptation_field_control is set, adaptation_field_lenght is not 0,
         * PCR_flag is set, pid == pcr_pid */
        vlc_mutex_lock(&p_own_pcr->lock);
        p_own_pcr->buf[p_own_pcr->end++] = ts;
        p_own_pcr->end %= (BUF_SIZE+1)*TS_IN_UDP+1;
        vlc_mutex_unlock(&p_own_pcr->lock);
        return 1;
    } 
    else
        return 0;
#undef p
}

/******************************************************************************
 * get_pid : gets a pid from a PES type
 ******************************************************************************/

int get_pid (ps_t *p_ps)
{
    int i, tofind, delta;

    switch( p_ps->pes_type )
    {
        case VIDEO_PES:
            delta = 0x20;
            tofind = p_ps->pes_id;
            break;
        case AUDIO_PES:
            delta = 0x40;
            tofind = p_ps->pes_id;
            break;
        case SUBTITLE_PES:
            delta = 0x60;
            tofind = p_ps->private_id;
            break;
        case AC3_PES:
            delta = 0x80;
            tofind = p_ps->private_id;
            break;
	default:
            return(-1);
    }
	    
    /* look in the table if we can find one */
    for ( i=delta; i < delta + 0x20; i++ )
    {
        if ( p_ps->association_table[i] == tofind )
            return (i);

        if( !p_ps->association_table[i] )
            break;
    }

    /* we must allocate a new entry */
    if (i == delta + 0x20)
        return(-1);
    
    p_ps->association_table[i] = tofind;
    p_ps->media_counter[i] = 0;

    fprintf( stderr, "allocated new PID 0x%.2x to stream ID 0x%.2x\n", i, tofind );
    
    return ( i );
}

/******************************************************************************
 * write_media_ts : writes a ts packet from a ps stream
 ******************************************************************************/

void write_media_ts(ps_t *ps, unsigned char *ts, unsigned int pid)
{
    int i,j;
    s64 clock;
    long int extclock;

    /* if offset == 0, it means we haven't examined the PS yet */
    if (ps->offset == 0)
    {
        if (ps->pes_size < 184) {

            //fprintf(stderr,"[WARNING: small PES]\n");
            ts[0] = 0x47;    /* sync_byte */
            ts[1] = 0x40;    /* payload_unit_start_indicator si début de PES */
            ts[2] = pid;

            ts[3] = 0x30 + (ps->media_counter[pid] & 0x0f);
            ts[4] = 184 - ps->pes_size - 1;
            ts[5] = 0x00;
            for (i=6 ; i < 188 - ps->pes_size ; i++) ts[i]=0xFF; /* facultatif ? */
            memcpy(ts + 188 - ps->pes_size, ps->ps_data, ps->pes_size);
            
            /* this PS is finished, next time we'll pick a new one */
            ps->pes_type = NO_PES;
            ps->ps_data += ps->pes_size;
            ps->offset += ps->pes_size;
	    //fprintf( stderr, "wrote %i final data (size was 0x%.2x)\n", ps->offset, ps->pes_size);
            return;

        }
    }

    /* now we still can have offset == 0, but size is initialized */
    
    ts[0] = 0x47;                /* sync_byte */
    ts[1] = (ps->offset == 0) ? 0x40 : 0x00;    /* payload_unit_start_indicator si début de PES */
    ts[2] = pid;

    //fprintf( stderr, "checking clock for %.2x while we have %.2x\n", ps->pcr_pid, pid );
    //fprintf( stderr, "offset 0x%.2x, pts 0x%.2x, pid %i \n", ps->offset, ps->has_pts, pid );
    if ( (ps->offset == 0) && (ps->has_pts == 0xc0) && (ps->pcr_pid == pid) )
    {

        ts[3] = 0x30 + (ps->media_counter[pid] & 0x0f);
        ts[4] = 0x07;            /* taille de l'adaptation field */
        ts[5] = 0x50;            /* rtfm */
        
        /* on va lire le PTS */
        clock = ( ((s64)(ps->ps_data[9] & 0x0E) << 29) |
            (((s64)U16_AT(ps->ps_data + 10) << 14) - (1 << 14)) |
            ((s64)U16_AT(ps->ps_data + 12) >> 1) );
        
        //fprintf( stderr, "clock is %lli\n", clock );
        ps->has_pts = 0;

        extclock = 0x000;
        ts[6] = (clock & 0x1fe000000) >> 25;    /* ---111111110000000000000000000000000 */
        ts[7] = (clock & 0x001fe0000) >> 17;    /* ---000000001111111100000000000000000 */
        ts[8] = (clock & 0x00001fe00) >>  9;    /* ---000000000000000011111111000000000 */
        ts[9] = (clock & 0x0000001fe) >>  1;    /* ---000000000000000000000000111111110 */
            
        ts[10] = 0x7e + ((clock & 0x01) << 7) + ((extclock & 0x100) >> 8);
        ts[11] = extclock & 0xff;
    
        memcpy(ts + 4 + 8, ps->ps_data, 184 - 8);
            
        ts[15] = 0xe0; /* FIXME : we don't know how to choose program yet */
         
        ps->offset += 184 - 8;
        ps->ps_data += 184 - 8;
    }
    else if (ps->offset <= ps->pes_size - 184)
    {
            
        ts[3] = 0x10 + (ps->media_counter[pid] & 0x0f);
        memcpy(ts + 4, ps->ps_data, 184);

        ps->offset += 184;
        ps->ps_data += 184;
   
    }
    else
    {
            
        j = ps->pes_size - ps->offset;
        ts[3] = 0x30 + (ps->media_counter[pid] & 0x0f);
        ts[4] = 184 - j - 1;
        ts[5] = 0x00;
        for (i=6 ; i < 188 - j ; i++) ts[i]=0xFF; /* facultatif ? */
        memcpy(ts + 4 + 184 - j, ps->ps_data, j);
        ps->offset += j; /* offset = size */
        ps->ps_data += j; /* offset = size */

        /* the PES is finished */
        ps->pes_type = NO_PES;
        ps->sent_ts++;

        //fprintf( stderr, "wrote 0x%.2x data (size was 0x%.2x)\n", ps->offset, ps->pes_size);
    }

    //fprintf(stderr, "[PES size: %i]\n", ps->pes_size);
}

/******************************************************************************
 * write_pat : writes a program association table
 ******************************************************************************/

void write_pat(ps_t *ps, unsigned char *ts)
{
    int i;

    //fprintf( stderr, "wrote a PAT\n");

    ts[0] = 0x47;        /* sync_byte */
    ts[1] = 0x40;
    ts[2] = 0x00;        /* PID = 0x0000 */
    ts[3] = 0x10 + (ps->pat_counter & 0x0f);
    ts[4] = ts[5] = 0x00;
    
    ts[6] = 0xb0; /* */
    ts[7] = 0x11; /* section_length = 0x011 */

    ts[8] = 0x00;
    ts[9] = 0xb0; /* TS id = 0x00b0 */

    ts[10] = 0xc1;
    /* section # and last section # */
    ts[11] = ts[12] = 0x00;

    /* Network PID (useless) */
    ts[13] = ts[14] = 0x00; ts[15] = 0xe0; ts[16] = 0x10;

    /* Program Map PID */
    ts[17] = 0x03; ts[18] = 0xe8; ts[19] = 0xe0; ts[20] = 0x64;

    /* CRC */
    ts[21] = 0x4d; ts[22] = 0x6a; ts[23] = 0x8b; ts[24] = 0x0f;

    for (i=25 ; i < 188 ; i++) ts[i]=0xFF; /* facultatif ? */

    ps->sent_ts++;
}

/******************************************************************************
 * write_pmt : writes a program map table
 ******************************************************************************/

void write_pmt(ps_t *ps, unsigned char *ts)
{
    int i;
    
    //fprintf( stderr, "wrote a PMT\n");

    ts[0] = 0x47;        /* sync_byte */
    ts[1] = 0x40;
    ts[2] = 0x0064;        /* PID = 0x0064 */
    ts[3] = 0x10 + (ps->pmt_counter & 0x0f);
    
    ts[4] = 0x00;
    ts[5] = 0x02;
    
    ts[6] = 0xb0; /* */
    ts[7] = 0x34; /* section_length = 0x034 */

    ts[8] = 0x03;
    ts[9] = 0xe8; /* prog number */

    ts[10] = 0xc1;
    /* section # and last section # */
    ts[11] = ts[12] = 0x00;

    /* PCR PID */
    ts[13] = 0xe0;
    ts[14] = 0x20;

    /* program_info_length == 0 */
    ts[15] = 0xf0; ts[16] = 0x00;

    /* Program Map / Video PID */
    ts[17] = 0x02; /* stream type = video */
    ts[18] = 0xe0; ts[19] = 0x20;
    ts[20] = 0xf0; ts[21] = 0x09; /* es info length */
    /* useless info */
    ts[22] = 0x07; ts[23] = 0x04; ts[24] = 0x08; ts[25] = 0x80; ts[26] = 0x24;
    ts[27] = 0x02; ts[28] = 0x11; ts[29] = 0x01; ts[30] = 0xfe;

    /* Audio PID */
    ts[31] = 0x88; /* stream type = audio */ /* FIXME : was 0x04 */
    ts[32] = 0xe0; ts[33] = 0x40;
    ts[34] = 0xf0; ts[35] = 0x00; /* es info length */

    /* reserved PID */
#if 0
    ts[36] = 0x82; /* stream type = private */
    ts[37] = 0xe0; ts[38] = 0x60; /* subtitles */
#else
    ts[36] = 0x81; /* stream type = private */
    ts[37] = 0xe0; ts[38] = 0x80; /* ac3 audio */
#endif
    ts[39] = 0xf0; ts[40] = 0x0f; /* es info length */
    /* useless info */
    ts[41] = 0x90; ts[42] = 0x01; ts[43] = 0x85; ts[44] = 0x89; ts[45] = 0x04;
    ts[46] = 0x54; ts[47] = 0x53; ts[48] = 0x49; ts[49] = 0x00; ts[50] = 0x0f;
    ts[51] = 0x04; ts[52] = 0x00; ts[53] = 0x00; ts[54] = 0x00; ts[55] = 0x10;

    /* CRC */
    ts[56] = 0x96; ts[57] = 0x70; ts[58] = 0x0b; ts[59] = 0x7c; /* for video pts */
    //ts[56] = 0xa1; ts[57] = 0x7c; ts[58] = 0xd8; ts[59] = 0xaa; /* for audio pts */

    for (i=60 ; i < 188 ; i++) ts[i]=0xFF; /* facultatif ? */

    ps->sent_ts++;
}


/******************************************************************************
 * ps_thread
 ******************************************************************************
 * We use threading to allow cool non-blocking read from the disk. This
 * implicit thread is the disk (producer) thread, it reads packets from
 * the PS file on the disk, and stores them in a FIFO.
 ******************************************************************************/

void ps_thread( input_file_t * p_if )
{
    int i;
    ps_t * p_ps =             &p_if->ps;
    own_pcr_t * p_own_pcr =   &p_if->own_pcr;
    in_data_t * p_in_data =   &p_if->in_data;
 
    /* Initialize the structures */
    p_own_pcr->start = p_own_pcr->end = 0; /* empty FIFO */
    vlc_mutex_init( &p_own_pcr->lock );
    p_in_data->start = p_in_data->end = 0; /* empty FIFO */
    vlc_mutex_init( &p_in_data->lock );
    vlc_cond_init( &p_in_data->notfull );
    vlc_cond_init( &p_in_data->notempty );
    
    p_ps->pes_type = NO_PES;
    p_ps->pes_id = 0;
    p_ps->private_id = 0;
    p_ps->pes_size = 0;
    p_ps->to_skip = 0;
    p_ps->pmt_counter = 0;
    p_ps->pat_counter = 0;
    for( i=0; i<256; i++ )
        p_ps->association_table[i] = 0;
    p_ps->offset = 0;
    p_ps->found_pts = 0;
    p_ps->found_streams = 0;
    p_ps->pcr_pid = p_if->options.pcr_pid;

    p_ps->ps_buffer = malloc(PS_BUFFER_SIZE);
    /* those 2 addresses are initialized so that a new packet is read */
    p_ps->ps_data = p_ps->ps_buffer + PS_BUFFER_SIZE - 1;
    /* fix the first byte stuff */
    p_ps->ps_data[0] = 0x00;
    p_ps->ps_end = p_ps->ps_buffer + PS_BUFFER_SIZE;
    
    /* Fill the fifo until it is full */
    ps_fill( p_if, 0 );
    /* Launch the thread which fills the fifo */
	vlc_thread_create( &p_if->disk_thread, "disk thread", (vlc_thread_func_t)input_DiskThread, p_if ); 	
    /* Init the synchronization XXX add error detection !!! */
    init_synchro( p_if );
}

/******************************************************************************
 * ps_read : ps reading method
 ******************************************************************************/

ssize_t ps_read (int fd, ps_t * p_ps, void *ts)
{
    int pid, readbytes = 0;
    int datasize;
    p_ps->ts_written = 0;
	  
    while(p_ps->ts_to_write)
    {   

        /* if there's not enough data to send */
        if((datasize = p_ps->ps_end - p_ps->ps_data) <= TS_PACKET_SIZE)
        {
            /* copy the remaining bits at the beginning of the PS buffer */
            memmove ( p_ps->ps_buffer, p_ps->ps_data, datasize);
            /* read some bytes */
            readbytes = safe_read(fd, p_ps->ps_buffer + datasize, PS_BUFFER_SIZE - datasize);

            if(readbytes == 0)
            {
                fprintf (stderr,"ps READ ERROR\n");
                return -1;
            }
            p_ps->ps_data = p_ps->ps_buffer;
            p_ps->ps_end = p_ps->ps_data + datasize + readbytes;
        }

        //printf("offset is %x, pes total size is %x, to skip is %x\n", p_ps->offset, p_ps->pes_size, p_ps->to_skip );
        if( p_ps->to_skip == 0 && p_ps->offset == p_ps->pes_size )
        {
            if( p_ps->ps_data[0] || p_ps->ps_data[1] || (p_ps->ps_data[2] != 0x01) )
            {
                fprintf (stderr,"Error: not a startcode (0x%.2x%.2x%.2x instead of 0x000001)\n", p_ps->ps_data[0], p_ps->ps_data[1], p_ps->ps_data[2] );
                return -1;
            }

	    p_ps->pes_type = NO_PES;
	    p_ps->offset = 0;
            p_ps->pes_size = (p_ps->ps_data[4] << 8) + p_ps->ps_data[5] + 6;
	    p_ps->has_pts = p_ps->ps_data[7] & 0xc0;
        }

        /* if the actual data we have in pes_data is not a PES, then
         * we read the next one. */
        if( (p_ps->pes_type == NO_PES) && !p_ps->to_skip )
        {
	    p_ps->pes_id = p_ps->ps_data[3];

            if (p_ps->pes_id == 0xbd)
            {
                p_ps->private_id = p_ps->ps_data[ 9 + p_ps->ps_data[8] ];
                if ((p_ps->private_id & 0xf0) == 0x80)
                {
                    /* flux audio ac3 */
                    p_ps->pes_type = AC3_PES;
                }
                else if ((p_ps->private_id & 0xf0) == 0x20)
                {
                    /* subtitles */
                    p_ps->pes_type = SUBTITLE_PES;
                }
                else
                {
                    /* unknown private data */
                    p_ps->pes_type = PRIVATE_PES;
                }
            }
            else if ((p_ps->pes_id & 0xe0) == 0xc0)
            {
                /* flux audio */
                p_ps->pes_type = AUDIO_PES;
               //write (1, p_ps->ps_data + 9 + p_ps->ps_data[8], 2048 - (9 + p_ps->ps_data[8]));
            }
            else if ((p_ps->pes_id & 0xf0) == 0xe0)
            {
                /* flux video */
                p_ps->pes_type = VIDEO_PES;
            }
            else if (p_ps->pes_id == 0xba)
            {
                p_ps->pes_type = NO_PES;
                p_ps->pes_size = 14; /* 8 extra characters after 0x000001ba**** */
                p_ps->to_skip = 14;
            }
            else
            {
                p_ps->pes_type = UNKNOWN_PES;
                p_ps->to_skip = p_ps->pes_size;
            }
        }

        if( p_ps->to_skip )
        {
            if( p_ps->to_skip < TS_PACKET_SIZE )
            {
                p_ps->ps_data += p_ps->to_skip;
                p_ps->offset += p_ps->to_skip;
                p_ps->to_skip = 0;
            }
            else
            {
                p_ps->ps_data += TS_PACKET_SIZE;
                p_ps->offset += TS_PACKET_SIZE;
                p_ps->to_skip -= TS_PACKET_SIZE;
            }
        }

        /* now that we know what we have, we can either 
         * write this packet's data in the buffer, skip it,
         * or write a PMT or PAT table and wait for the next
         * turn before writing the packet. */
        switch (p_ps->sent_ts & 0xff)
        {
            case 0x80:
                write_pmt(p_ps,ts);
                p_ps->pmt_counter++;
                p_ps->ts_to_write--; p_ps->ts_written++; ts+=188;
                break;
            case 0x00:
                write_pat(p_ps,ts);
                p_ps->pat_counter++;
                p_ps->ts_to_write--; p_ps->ts_written++; ts+=188;
                break;
        }

        /* if there's still no found PCR_PID, and no PTS in this PES, we trash it */
        if (!p_ps->found_pts)
        {
            if (p_ps->has_pts)
            {
                fprintf(stderr, "found a PTS, at last ...\n");
                p_ps->found_pts = 1;
            }
            else
                p_ps->pes_type = NO_PES;
        }
	
        if (p_ps->ts_to_write)
        {
            switch(p_ps->pes_type)
            {
                case VIDEO_PES:
                case AUDIO_PES:
		case SUBTITLE_PES:
                case AC3_PES:
                    pid = get_pid (p_ps);
                    write_media_ts(p_ps, ts, pid);
                    p_ps->ts_to_write--; p_ps->ts_written++; ts+=188;
                    p_ps->media_counter[pid]++;
                    break;
                case UNKNOWN_PES:
                default:
                    p_ps->pes_type = NO_PES;
                    break;
            }
        }
    }

    //p_ps->ps_data += TS_PACKET_SIZE;

    return p_ps->ts_written;
}

/******************************************************************************
 * ps_fill : Fill the data buffer with TS created from a PS file
 ******************************************************************************/

void ps_fill( input_file_t * p_if, boolean_t wait )
{
    in_data_t * p_in_data = &p_if->in_data;
    ps_t * p_ps = &p_if->ps;
    int fd = p_if->options.in;
    int i, how_many;
    int pcr_flag;
    file_ts_packet *ts;

    /* How many TS packet for the next UDP packet */
    how_many = TS_IN_UDP;

    pcr_flag = 0;
    /* for every single TS packet */
    while( !p_if->b_die )
    {        
        /* wait until we have one free item to store the UDP packet read */
        vlc_mutex_lock(&p_in_data->lock);
        while((p_in_data->end+BUF_SIZE+1-p_in_data->start)%(BUF_SIZE+1) == BUF_SIZE )
        {
            /* The buffer is full */
            if(wait)
            {
                vlc_cond_wait(&p_in_data->notfull, &p_in_data->lock);
                if( p_if->b_die )
                    return;
            }
            else
            {
                vlc_mutex_unlock(&p_in_data->lock);
                if (!pcr_flag)
                {
                    intf_ErrMsg( "input error: bad PCR PID\n" );
                }
                return;
            }
        }
        vlc_mutex_unlock(&p_in_data->lock);
        
        /* read a whole UDP packet from the file */
        p_ps->ts_to_write = how_many;
        if(ps_read(fd, p_ps, ts = (file_ts_packet *)(p_in_data->buf + p_in_data->end)) != how_many)
        {
            msleep( 50000 ); /* XXX we need an INPUT_IDLE */
            intf_ErrMsg( "input error: read() error\n" );
        }
        
        /* Scan to mark TS packets containing a PCR */
        for(i=0; i<how_many; i++, ts++)
        {
            pcr_flag |= keep_pcr(p_ps->pcr_pid, ts);
        }
        
        vlc_mutex_lock(&p_in_data->lock);
        p_in_data->end++;
        p_in_data->end %= BUF_SIZE+1;
        vlc_cond_signal(&p_in_data->notempty);
        vlc_mutex_unlock(&p_in_data->lock);
    }
}

int init_synchro( input_file_t * p_if )
{
    int i, pcr_count;
    int howmany = TS_IN_UDP;
    file_ts_packet * ts;
    synchro_t * p_synchro = &p_if->synchro;
    in_data_t * p_in_data = &p_if->in_data;
    own_pcr_t * p_own_pcr = &p_if->own_pcr; 
    
    p_synchro->slope = 0;
    pcr_count = 0;
    
    /*
     * Initialisation of the synchro mecanism : wait for 1 PCR
     * to evaluate delta_clock
     */

    while( 1 )
    {
        vlc_mutex_lock( &p_in_data->lock );
        
        while( p_in_data->end == p_in_data->start )
        {
            vlc_cond_wait(&p_in_data->notempty, &p_in_data->lock);
        }
        /*
        if( p_in_data->end == p_in_data->start )
        {
            intf_ErrMsg( "input error: init_synchro error, not enough PCR found\n" );
            return( -1 );
        }
        */
        vlc_mutex_unlock( &p_in_data->lock );
        
        ts = (file_ts_packet*)(p_in_data->buf + p_in_data->start);
        for( i=0 ; i < howmany ; i++, ts++ )
        {
            if( ts  == p_own_pcr->buf[p_own_pcr->start] && !(((u8*)ts)[5] & 0x80) )
            {
                p_synchro->last_pcr = ts;
                p_synchro->last_pcr_time = ConvertPCRTime( ts );
                p_synchro->delta_clock = mdate() - ConvertPCRTime(ts);
                adjust( p_if, ts );
                pcr_count++;
            }
        }
        
        vlc_mutex_lock( &p_in_data->lock );
        p_in_data->start++;
        p_in_data->start %= BUF_SIZE + 1;
        vlc_cond_signal( &p_in_data->notfull );
        vlc_mutex_unlock( &p_in_data->lock );
        
        if(pcr_count)
            break; 
    }
    return( 0 );
}

/*****************************************************************************
 * input_FileOpen : open a file descriptor
 *****************************************************************************/

void input_DiskThread( input_file_t * p_if )
{
    ps_fill( p_if, 1 );    
    vlc_thread_exit();
}

/*****************************************************************************
 * input_FileOpen : open a file descriptor
 *****************************************************************************/
int input_FileOpen( input_thread_t *p_input )
{
    options_t * p_options = &input_file.options;

    p_options->in = open( p_input->psz_source, O_RDONLY );
    if( p_options->in < 0 )
    {
        intf_ErrMsg( "input error: cannot open the file %s", p_input->psz_source );
    }

    input_file.b_die = 0;
    read( p_options->in, &p_options->i_file_type, 1 );
    
    switch( p_options->i_file_type )
    {
    case 0x00:
        p_options->pcr_pid = PCR_PID;
        ps_thread( &input_file );
        break;
    case 0x47:
        intf_ErrMsg( "input error: ts file are not currently supported\n" );
        return( 1 );
    default:
        intf_ErrMsg( "input error: cannot determine stream type\n" );
        return( 1 );
    }

    
	return( 0 );	
}

/*****************************************************************************
 * input_FileRead : read from a file
 *****************************************************************************/
int input_FileRead( input_thread_t *p_input, const struct iovec *p_vector,
                    size_t i_count )
{
    in_data_t * p_in_data = &input_file.in_data;
    synchro_t * p_synchro = &input_file.synchro;
    own_pcr_t * p_own_pcr = &input_file.own_pcr;
    int i, howmany;
    file_ts_packet * ts;
    
    /* XXX XXX XXX
     * End condition not verified, should put a flag in ps_fill
     */
    howmany = TS_IN_UDP;
    //fprintf( stderr, "XXX icount = %d\n", (int)i_count );

    vlc_mutex_lock( &p_in_data->lock );
    while( p_in_data->end == p_in_data->start )
    {
        if( !input_file.b_die )
            vlc_cond_wait( &p_in_data->notempty, &p_in_data->lock );
    }
    vlc_mutex_unlock( &p_in_data->lock );

    ts = (file_ts_packet*)(p_in_data->buf + p_in_data->start);
    for( i=0 ; i < howmany ; i++, ts++ )
    {             
        if( p_synchro->slope && (i == howmany-1) )
        {
            wait_a_moment( &input_file, ts );
        }
        if( ts  == p_own_pcr->buf[p_own_pcr->start] )
        {
            /* the TS packet contains a PCR, so we try to adjust the clock */
            adjust( &input_file, ts );
        }
    }

    for( i=0 ; i<howmany ; i++ )
    {
        memcpy( p_vector[i].iov_base, (char*)(ts - howmany + i), p_vector[i].iov_len );
    }

    vlc_mutex_lock(&p_in_data->lock);
    p_in_data->start++;
    p_in_data->start %= BUF_SIZE + 1;
    vlc_cond_signal(&p_in_data->notfull);
    vlc_mutex_unlock(&p_in_data->lock);
    
  	return( 188*howmany );
}

/*****************************************************************************
 * input_FileClose : close a file descriptor
 *****************************************************************************/
void input_FileClose( input_thread_t *p_input )
{
    input_file.b_die = 1;
    vlc_cond_signal( &input_file.in_data.notfull );
    vlc_thread_join( input_file.disk_thread );

    close( input_file.options.in );
}
