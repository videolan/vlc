/*****************************************************************************
 * timeshift.c
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN
 * $Id: demux.c 7546 2004-04-29 13:53:29Z gbazin $
 *
 * Author: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <stdlib.h>

#include <errno.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <unistd.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("Timeshift") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS_FILTER );
    set_capability( "access_filter", 0 );
    add_shortcut( "timeshift" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static block_t *Block  ( access_t *p_access );
static int      Control( access_t *, int i_query, va_list args );
static void     Thread ( access_t * );

#define TIMESHIFT_FIFO_MAX (4*1024*1024)
#define TIMESHIFT_FIFO_MIN (TIMESHIFT_FIFO_MAX/4)

struct access_sys_t
{
    block_fifo_t *p_fifo;

    vlc_bool_t b_opened;

    FILE *t1, *t2;

    char *psz_tmp1;
    char *psz_tmp2;

    FILE *w;
    int i_w;

    FILE *r;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t *p_access = (access_t*)p_this;
    access_t *p_src = p_access->p_source;
    access_sys_t *p_sys;
    vlc_bool_t b_bool;
#ifdef WIN32
    char buffer[4096];
    int i_size;
#endif

    /* Only work with not pace controled access */
    if( access2_Control( p_src, ACCESS_CAN_CONTROL_PACE, &b_bool ) || b_bool )
    {
        msg_Dbg( p_src, "ACCESS_CAN_CONTROL_PACE" );
        return VLC_EGENERIC;
    }
    /* Refuse access that can be paused */
    if( access2_Control( p_src, ACCESS_CAN_PAUSE, &b_bool ) || b_bool )
    {
        msg_Dbg( p_src, "ACCESS_CAN_PAUSE: timeshift useless" );
        return VLC_EGENERIC;
    }

    /* */
    p_access->pf_read = NULL;
    p_access->pf_block = Block;
    p_access->pf_seek = NULL;
    p_access->pf_control = Control;

    p_access->info = p_src->info;

    p_access->p_sys = p_sys = malloc( sizeof( access_sys_t ) );

    /* */
    p_sys->p_fifo = block_FifoNew( p_access );
    p_sys->b_opened = VLC_FALSE;
    p_sys->t1 = p_sys->t2 = NULL;
    p_sys->w = p_sys->r = NULL;
    p_sys->i_w = 0;

#ifdef WIN32
    i_size = GetTempPath( 4095, buffer );
    if( i_size <= 0 || i_size >= 4095 )
    {
        if( getcwd( buffer, 4095 ) == NULL )
            strcpy( buffer, "c:" );
    }
    /* remove last \\ if any */
    if( buffer[strlen(buffer)-1] == '\\' )
        buffer[strlen(buffer)-1] = '\0';

    asprintf( &p_sys->psz_tmp1, "%s\\vlc-timeshift-%d-%d-1.dat",
              buffer, GetCurrentProcessId(), p_access->i_object_id );
    asprintf( &p_sys->psz_tmp2, "%s\\vlc-timeshift-%d-%d-2.dat",
              buffer, GetCurrentProcessId(), p_access->i_object_id );
#else
    asprintf( &p_sys->psz_tmp1, "/tmp/vlc-timeshift-%d-%d-1.dat",
              getpid(), p_access->i_object_id );
    asprintf( &p_sys->psz_tmp2, "/tmp/vlc-timeshift-%d-%d-2.dat",
              getpid(), p_access->i_object_id );
#endif

    if( vlc_thread_create( p_access, "timeshift thread", Thread,
                           VLC_THREAD_PRIORITY_LOW, VLC_FALSE ) )
    {
        msg_Err( p_access, "cannot spawn timeshift access thread" );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    /* */
    msg_Dbg( p_access, "timeshift close called" );
    vlc_thread_join( p_access );

    if( p_sys->b_opened )
    {
        if( p_sys->t1 ) fclose( p_sys->t1 );
        if( p_sys->t2 ) fclose( p_sys->t2 );
        unlink( p_sys->psz_tmp1 );
        unlink( p_sys->psz_tmp2 );
    }

    free( p_sys->psz_tmp1 );
    free( p_sys->psz_tmp2 );

    block_FifoRelease( p_sys->p_fifo );
    free( p_sys );
}

/*****************************************************************************
 *
 *****************************************************************************/
static block_t *Block( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( p_access->b_die )
    {
        p_access->info.b_eof = VLC_TRUE;
        return NULL;
    }

    return block_FifoGet( p_sys->p_fifo );
}

/*****************************************************************************
 *
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    access_t     *p_src = p_access->p_source;

    vlc_bool_t   *pb_bool;
    int          *pi_int;
    int64_t      *pi_64;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_FALSE;
            break;

        case ACCESS_CAN_CONTROL_PACE:   /* Not really true */
        case ACCESS_CAN_PAUSE:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_TRUE;
            break;

        /* */
        case ACCESS_GET_MTU:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = 0;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            return access2_Control( p_src, ACCESS_GET_PTS_DELAY, pi_64 );
        /* */
        case ACCESS_SET_PAUSE_STATE:
            return VLC_SUCCESS;

        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_GET_META:
            return VLC_EGENERIC;

        case ACCESS_SET_PRIVATE_ID_STATE:
        case ACCESS_GET_PRIVATE_ID_STATE:
        case ACCESS_SET_PRIVATE_ID_CA:
            return access2_vaControl( p_src, i_query, args );

        default:
            msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 *
 *****************************************************************************/
static void Thread( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    access_t     *p_src = p_access->p_source;
    int i_loop = 0;

    while( !p_access->b_die )
    {
        block_t *p_block;

        /* Get a new block */
        if( p_src->pf_block )
        {
            p_block = p_src->pf_block( p_src );

            if( p_block == NULL )
            {
                if( p_src->info.b_eof )
                    break;

                msleep( 1000 );
                continue;
            }
        }
        else
        {
            if( ( p_block = block_New( p_access, 2048 ) ) == NULL )
                break;

            p_block->i_buffer = p_src->pf_read( p_src, p_block->p_buffer, 2048);
            if( p_block->i_buffer < 0 )
            {
                block_Release( p_block );
                if( p_block->i_buffer == 0 )
                    break;
                msleep( 1000 );
                continue;
            }
        }

        /* Open dump files if we need them */
        if( p_sys->p_fifo->i_size >= TIMESHIFT_FIFO_MAX && !p_sys->b_opened )
        {
            msg_Dbg( p_access, "opening first temporary files (%s)",
                     p_sys->psz_tmp1 );

            p_sys->b_opened = VLC_TRUE;
            p_sys->t1 = p_sys->t2 = NULL;
            p_sys->w = p_sys->r = NULL;

            p_sys->t1 = fopen( p_sys->psz_tmp1, "w+b" );
            if( p_sys->t1 )
            {
                msg_Dbg( p_access, "opening second temporary files (%s)",
                         p_sys->psz_tmp2 );

                p_sys->t2 = fopen( p_sys->psz_tmp2, "w+b" );
                if( p_sys->t2 )
                {
                    p_sys->w = p_sys->t1;
                    p_sys->i_w = 0;

                    msg_Dbg( p_access, "start writing into temporary file" );
                }
                else
                {
                    msg_Err( p_access, "cannot open temporary file '%s' (%s)",
                             p_sys->psz_tmp2, strerror(errno) );
                    fclose( p_sys->t1 );
                    p_sys->t1 = NULL;
                }
            }
            else
            {
                msg_Err( p_access, "cannot open temporary file '%s' (%s)",
                         p_sys->psz_tmp1, strerror(errno) );
            }
        }

        if( p_sys->w )
        {
            int i_write;

            /* Dump the block */
            i_write = fwrite( p_block->p_buffer, 1, p_block->i_buffer,
                              p_sys->w );
            block_Release( p_block );

            if( i_write > 0 )
                p_sys->i_w += i_write;
            else
                msg_Warn( p_access, "cannot write data" );

            /* Start reading from a file if fifo isn't at 25% */
            if( p_sys->p_fifo->i_size < TIMESHIFT_FIFO_MIN && !p_sys->r )
            {
                msg_Dbg( p_access, "start reading from temporary file (dumped=%d)", p_sys->i_w );

                p_sys->r = p_sys->w;
                fseek( p_sys->r, 0, SEEK_SET );

                p_sys->w = p_sys->t2;
                p_sys->i_w = 0;
            }

            if( p_sys->r )
            {
                while( p_sys->p_fifo->i_size < TIMESHIFT_FIFO_MIN )
                {
                    p_block = block_New( p_access, 4096 );
                    p_block->i_buffer = fread( p_block->p_buffer, 1, 4096,
                                               p_sys->r );

                    if( p_block->i_buffer > 0 )
                    {
                        block_FifoPut( p_sys->p_fifo, p_block );
                    }
                    else if( p_sys->i_w > 32*1024)
                    {
                        FILE *tmp;
                        block_Release( p_block );

                        msg_Dbg( p_access, "switching temporary files (dumped=%d)", p_sys->i_w );

                        /* Switch read/write */
                        tmp = p_sys->r;

                        p_sys->r = p_sys->w;
                        fseek( p_sys->r, 0, SEEK_SET );

                        p_sys->w = tmp;
                        fseek( p_sys->w, 0, SEEK_SET );
                        ftruncate( fileno(p_sys->w), 0 );
                        p_sys->i_w = 0;
                    }
                    else
                    {
                        msg_Dbg( p_access, "removing temporary files" );

                        /* We will remove the need of tmp files */
                        if( p_sys->i_w > 0 )
                        {
                            msg_Dbg( p_access, "loading temporary file" );
                            fseek( p_sys->w, 0, SEEK_SET );
                            for( ;; )
                            {
                                p_block = block_New( p_access, 4096 );
                                p_block->i_buffer = fread( p_block->p_buffer,
                                                           1, 4096,
                                                           p_sys->w );
                                if( p_block->i_buffer <= 0 )
                                {
                                    block_Release( p_block );
                                    break;
                                }
                                block_FifoPut( p_sys->p_fifo, p_block );
                            }
                        }

                        p_sys->b_opened = VLC_FALSE;

                        fclose( p_sys->t1 );
                        fclose( p_sys->t2 );

                        p_sys->t1 = p_sys->t2 = NULL;
                        p_sys->w = p_sys->r = NULL;

                        unlink( p_sys->psz_tmp1 );
                        unlink( p_sys->psz_tmp2 );
                        break;
                    }
                }
            }
        }
        else if( p_sys->p_fifo->i_size < TIMESHIFT_FIFO_MAX )
        {
            block_FifoPut( p_sys->p_fifo, p_block );
        }
        else
        {
            /* We failed to opened files so trash new data */
            block_Release( p_block );
        }
#if 0
        if( (i_loop % 400) == 0 )
            msg_Dbg( p_access, "timeshift: buff=%d", p_sys->p_fifo->i_size );
#endif
        i_loop++;
    }

    msg_Warn( p_access, "timeshift: EOF" );

    /* Send dummy packet to avoid deadlock in TShiftBlock */
    for( i_loop = 0; i_loop < 2; i_loop++ )
    {
        block_t *p_dummy = block_New( p_access, 128 );

        p_dummy->i_flags |= BLOCK_FLAG_DISCONTINUITY;
        memset( p_dummy->p_buffer, 0, p_dummy->i_buffer );

        block_FifoPut( p_sys->p_fifo, p_dummy );
    }
}

