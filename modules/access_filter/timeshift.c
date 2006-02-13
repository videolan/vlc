/*****************************************************************************
 * timeshift.c: access filter implementing timeshifting capabilities
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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

#define GRANULARITY_TEXT N_("Timeshift granularity")
#define GRANULARITY_LONGTEXT N_( "Size of the temporary files use to store " \
  "the timeshifted stream." )
#define DIR_TEXT N_("Timeshift directory")
#define DIR_LONGTEXT N_( "Directory used to store the timeshift temporary " \
  "files." )

vlc_module_begin();
    set_shortname( _("Timeshift") );
    set_description( _("Timeshift") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS_FILTER );
    set_capability( "access_filter", 0 );
    add_shortcut( "timeshift" );
    set_callbacks( Open, Close );

    add_integer( "timeshift-granularity", 50, NULL, GRANULARITY_TEXT,
                 GRANULARITY_LONGTEXT, VLC_TRUE );
    add_directory( "timeshift-dir", 0, 0, DIR_TEXT, DIR_LONGTEXT, VLC_FALSE );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int      Seek( access_t *, int64_t );
static block_t *Block  ( access_t *p_access );
static int      Control( access_t *, int i_query, va_list args );
static void     Thread ( access_t *p_access );
static int      WriteBlockToFile( access_t *p_access, block_t *p_block );
static block_t *ReadBlockFromFile( access_t *p_access );
static char    *GetTmpFilePath( access_t *p_access );

#define TIMESHIFT_FIFO_MAX (10*1024*1024)
#define TIMESHIFT_FIFO_MIN (TIMESHIFT_FIFO_MAX/4)
#define TMP_FILE_MAX 256

typedef struct ts_entry_t
{
    FILE *file;
    struct ts_entry_t *p_next;

} ts_entry_t;

struct access_sys_t
{
    block_fifo_t *p_fifo;

    int  i_files;
    int  i_file_size;
    int  i_write_size;

    ts_entry_t *p_read_list;
    ts_entry_t **pp_read_last;
    ts_entry_t *p_write_list;
    ts_entry_t **pp_write_last;

    char *psz_filename_base;
    char *psz_filename;
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
    p_access->pf_seek = Seek;
    p_access->pf_control = Control;
    p_access->info = p_src->info;

    p_access->p_sys = p_sys = malloc( sizeof( access_sys_t ) );

    /* */
    p_sys->p_fifo = block_FifoNew( p_access );
    p_sys->i_write_size = 0;
    p_sys->i_files = 0;

    p_sys->p_read_list = NULL;
    p_sys->pp_read_last = &p_sys->p_read_list;
    p_sys->p_write_list = NULL;
    p_sys->pp_write_last = &p_sys->p_write_list;

    var_Create( p_access, "timeshift-dir",
                VLC_VAR_DIRECTORY | VLC_VAR_DOINHERIT );
    var_Create( p_access, "timeshift-granularity",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    p_sys->i_file_size = var_GetInteger( p_access, "timeshift-granularity" );
    if( p_sys->i_file_size < 1 ) p_sys->i_file_size = 1;
    p_sys->i_file_size *= 1024 * 1024; /* In MBytes */

    p_sys->psz_filename_base = GetTmpFilePath( p_access );
    p_sys->psz_filename = malloc( strlen( p_sys->psz_filename_base ) + 1000 );

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
    ts_entry_t *p_entry;
    int i;

    msg_Dbg( p_access, "timeshift close called" );
    vlc_thread_join( p_access );

    for( p_entry = p_sys->p_write_list; p_entry; )
    {
        ts_entry_t *p_next = p_entry->p_next;
        fclose( p_entry->file );
        free( p_entry );
        p_entry = p_next;
    }
    for( p_entry = p_sys->p_read_list; p_entry; )
    {
        ts_entry_t *p_next = p_entry->p_next;
        fclose( p_entry->file );
        free( p_entry );
        p_entry = p_next;
    }
    for( i = 0; i < p_sys->i_files; i++ )
    {
        sprintf( p_sys->psz_filename, "%s%i.dat",
                 p_sys->psz_filename_base, i );
        unlink( p_sys->psz_filename );
    }

    free( p_sys->psz_filename );
    free( p_sys->psz_filename_base );
    block_FifoRelease( p_sys->p_fifo );
    free( p_sys );
}

/*****************************************************************************
 *
 *****************************************************************************/
static block_t *Block( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    block_t *p_block;

    if( p_access->b_die )
    {
        p_access->info.b_eof = VLC_TRUE;
        return NULL;
    }

    p_block = block_FifoGet( p_sys->p_fifo );
    //p_access->info.i_size -= p_block->i_buffer;
    return p_block;
}

/*****************************************************************************
 *
 *****************************************************************************/
static void Thread( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    access_t     *p_src = p_access->p_source;
    int i;

    while( !p_access->b_die )
    {
        block_t *p_block;

        /* Get a new block from the source */
        if( p_src->pf_block )
        {
            p_block = p_src->pf_block( p_src );

            if( p_block == NULL )
            {
                if( p_src->info.b_eof ) break;
                msleep( 1000 );
                continue;
            }
        }
        else
        {
            if( ( p_block = block_New( p_access, 2048 ) ) == NULL ) break;

            p_block->i_buffer =
                p_src->pf_read( p_src, p_block->p_buffer, 2048 );

            if( p_block->i_buffer < 0 )
            {
                block_Release( p_block );
                if( p_block->i_buffer == 0 ) break;
                msleep( 1000 );
                continue;
            }
        }

        /* Write block */
        if( !p_sys->p_write_list && !p_sys->p_read_list &&
            p_sys->p_fifo->i_size < TIMESHIFT_FIFO_MAX )
        {
            /* If there isn't too much timeshifted data,
             * write directly to FIFO */
            block_FifoPut( p_sys->p_fifo, p_block );

            //p_access->info.i_size += p_block->i_buffer;
            //p_access->info.i_update |= INPUT_UPDATE_SIZE;

            /* Nothing else to do */
            continue;
        }

        WriteBlockToFile( p_access, p_block );
        block_Release( p_block );

        /* Read from file to fill up the fifo */
        while( p_sys->p_fifo->i_size < TIMESHIFT_FIFO_MIN &&
               !p_access->b_die )
        {
            p_block = ReadBlockFromFile( p_access );
            if( !p_block ) break;
            block_FifoPut( p_sys->p_fifo, p_block );
        }
    }

    msg_Dbg( p_access, "timeshift: EOF" );

    /* Send dummy packet to avoid deadlock in TShiftBlock */
    for( i = 0; i < 2; i++ )
    {
        block_t *p_dummy = block_New( p_access, 128 );
        p_dummy->i_flags |= BLOCK_FLAG_DISCONTINUITY;
        memset( p_dummy->p_buffer, 0, p_dummy->i_buffer );
        block_FifoPut( p_sys->p_fifo, p_dummy );
    }
}

/*****************************************************************************
 * NextFileWrite:
 *****************************************************************************/
static void NextFileWrite( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    ts_entry_t   *p_next;

    if( !p_sys->p_write_list )
    {
        p_sys->i_write_size = 0;
        return;
    }

    p_next = p_sys->p_write_list->p_next;

    /* Put written file in read list */
    if( p_sys->i_write_size < p_sys->i_file_size )
        ftruncate( fileno( p_sys->p_write_list->file ), p_sys->i_write_size );

    fseek( p_sys->p_write_list->file, 0, SEEK_SET );
    *p_sys->pp_read_last = p_sys->p_write_list;
    p_sys->pp_read_last = &p_sys->p_write_list->p_next;
    p_sys->p_write_list->p_next = 0;

    /* Switch to next file to write */
    p_sys->p_write_list = p_next;
    if( !p_sys->p_write_list ) p_sys->pp_write_last = &p_sys->p_write_list;

    p_sys->i_write_size = 0;
}

/*****************************************************************************
 * NextFileRead:
 *****************************************************************************/
static void NextFileRead( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    ts_entry_t   *p_next;

    if( !p_sys->p_read_list ) return;

    p_next = p_sys->p_read_list->p_next;

    /* Put read file in write list */
    fseek( p_sys->p_read_list->file, 0, SEEK_SET );
    *p_sys->pp_write_last = p_sys->p_read_list;
    p_sys->pp_write_last = &p_sys->p_read_list->p_next;
    p_sys->p_read_list->p_next = 0;

    /* Switch to next file to read */
    p_sys->p_read_list = p_next;
    if( !p_sys->p_read_list ) p_sys->pp_read_last = &p_sys->p_read_list;
}

/*****************************************************************************
 * WriteBlockToFile:
 *****************************************************************************/
static int WriteBlockToFile( access_t *p_access, block_t *p_block )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_write, i_buffer;

    if( p_sys->i_write_size == p_sys->i_file_size ) NextFileWrite( p_access );

    /* Open new file if necessary */
    if( !p_sys->p_write_list )
    {
        FILE *file;

        sprintf( p_sys->psz_filename, "%s%i.dat",
                 p_sys->psz_filename_base, p_sys->i_files );
        file = utf8_fopen( p_sys->psz_filename, "w+b" );

        if( !file && p_sys->i_files < 2 )
        {
            /* We just can't work with less than 2 buffer files */
            msg_Err( p_access, "cannot open temporary file '%s' (%s)",
                     p_sys->psz_filename, strerror(errno) );
            return VLC_EGENERIC;
        }
        else if( !file ) return VLC_EGENERIC;

        p_sys->p_write_list = malloc( sizeof(ts_entry_t) );
        p_sys->p_write_list->p_next = 0;
        p_sys->p_write_list->file = file;
        p_sys->pp_write_last = &p_sys->p_write_list->p_next;

        p_sys->i_files++;
    }

    /* Write to file */
    i_buffer = __MIN( p_block->i_buffer,
                      p_sys->i_file_size - p_sys->i_write_size );

    i_write = fwrite( p_block->p_buffer, 1, i_buffer,
                      p_sys->p_write_list->file );

    if( i_write > 0 ) p_sys->i_write_size += i_write;

    //p_access->info.i_size += i_write;
    //p_access->info.i_update |= INPUT_UPDATE_SIZE;

    if( i_write < i_buffer )
    {
        /* Looks like we're short of space */

        if( !p_sys->p_write_list->p_next )
        {
            msg_Warn( p_access, "no more space, overwritting old data" );
            NextFileRead( p_access );
            NextFileRead( p_access );
        }

        /* Make sure we switch to next file in write list */
        p_sys->i_write_size = p_sys->i_file_size;
    }

    p_block->p_buffer += i_write;
    p_block->i_buffer -= i_write;

    /* Check if we have some data left */
    if( p_block->i_buffer ) return WriteBlockToFile( p_access, p_block );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ReadBlockFromFile:
 *****************************************************************************/
static block_t *ReadBlockFromFile( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    block_t *p_block;

    if( !p_sys->p_read_list && p_sys->p_write_list )
    {
        /* Force switching to next write file, that should
         * give us something to read */
        NextFileWrite( p_access );
    }

    if( !p_sys->p_read_list ) return 0;

    p_block = block_New( p_access, 4096 );
    p_block->i_buffer = fread( p_block->p_buffer, 1, 4096,
                               p_sys->p_read_list->file );

    if( p_block->i_buffer == 0 ) NextFileRead( p_access );

    //p_access->info.i_size -= p_block->i_buffer;
    //p_access->info.i_update |= INPUT_UPDATE_SIZE;

    return p_block;
}

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( access_t *p_access, int64_t i_pos )
{
    //access_sys_t *p_sys = p_access->p_sys;
    return VLC_SUCCESS;
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
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_TRUE;
            break;

        case ACCESS_CAN_CONTROL_PACE:   /* Not really true */
        case ACCESS_CAN_PAUSE:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_TRUE;
            break;

        case ACCESS_GET_MTU:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = 0;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            return access2_Control( p_src, ACCESS_GET_PTS_DELAY, pi_64 );

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
 * GetTmpFilePath:
 *****************************************************************************/
#ifdef WIN32
#define getpid() GetCurrentProcessId()
#endif
static char *GetTmpFilePath( access_t *p_access )
{
    char *psz_dir = var_GetString( p_access, "timeshift-dir" );
    char *psz_filename_base;

    if( ( psz_dir != NULL ) && ( psz_dir[0] == '\0' ) )
    {
        free( psz_dir );
        psz_dir = NULL;
    }

    if( psz_dir == NULL )
    {
#ifdef WIN32
        char psz_local_dir[MAX_PATH];
        int i_size;

        i_size = GetTempPath( MAX_PATH, psz_local_dir );
        if( i_size <= 0 || i_size > MAX_PATH )
        {
            if( !getcwd( psz_local_dir, MAX_PATH ) )
                strcpy( psz_local_dir, "C:" );
        }

        psz_dir = FromLocaleDup( MAX_PATH + 1 );

        /* remove last \\ if any */
        if( psz_dir[strlen(psz_dir)-1] == '\\' )
            psz_dir[strlen(psz_dir)-1] = '\0';
#else
        psz_dir = strdup( "/tmp" );
#endif
    }

    asprintf( &psz_filename_base, "%s/vlc-timeshift-%d-%d-",
              psz_dir, getpid(), p_access->i_object_id );
    free( psz_dir );

    return psz_filename_base;
}
