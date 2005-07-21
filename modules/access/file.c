/*****************************************************************************
 * file.c: file input (file: access plug-in)
 *****************************************************************************
 * Copyright (C) 2001-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
#include <vlc/input.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#   include <sys/time.h>
#endif
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( WIN32 ) && !defined( UNDER_CE )
#   include <io.h>
#endif

#if defined( WIN32 ) && !defined( UNDER_CE )
/* stat() support for large files on win32 */
#   define stat _stati64
#   define fstat(a,b) _fstati64(a,b)
#   ifdef lseek
#      undef lseek
#   endif
#   define lseek _lseeki64
#elif defined( UNDER_CE )
#   ifdef read
#      undef read
#   endif
#   define read(a,b,c) fread(b,1,c,a)
#   define close(a) fclose(a)
#   ifdef lseek
#      undef lseek
#   endif
#   define lseek fseek
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for file streams. This " \
    "value should be set in millisecond units." )
#define CAT_TEXT N_("Concatenate with additional files")
#define CAT_LONGTEXT N_( \
    "Allows you to play split files as if they were part of a unique file. " \
    "Specify a comma-separated list of files." )

vlc_module_begin();
    set_description( _("Standard filesystem file input") );
    set_shortname( _("File") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );
    add_integer( "file-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    add_string( "file-cat", NULL, NULL, CAT_TEXT, CAT_LONGTEXT, VLC_TRUE );
    set_capability( "access2", 50 );
    add_shortcut( "file" );
    add_shortcut( "stream" );
    add_shortcut( "kfir" );
    set_callbacks( Open, Close );
vlc_module_end();


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int  Seek( access_t *, int64_t );
static int  Read( access_t *, uint8_t *, int );
static int  Control( access_t *, int, va_list );

static int  _OpenFile( access_t *, char * );

typedef struct
{
    char     *psz_name;
    int64_t  i_size;

} file_entry_t;

struct access_sys_t
{
    unsigned int i_nb_reads;
    vlc_bool_t   b_kfir;

    /* Files list */
    int          i_file;
    file_entry_t **file;

    /* Current file */
    int  i_index;
#ifndef UNDER_CE
    int  fd;
    int  fd_backup;
#else
    FILE *fd;
    FILE *fd_backup;
#endif

    /* */
    vlc_bool_t b_seekable;
    vlc_bool_t b_pace_control;
};

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;
    char *psz_name = p_access->psz_path;
    char *psz;

#ifdef HAVE_SYS_STAT_H
    int                 i_stat;
    struct stat         stat_info;
#endif
    vlc_bool_t          b_stdin;

    file_entry_t *      p_file;


    b_stdin = psz_name[0] == '-' && psz_name[1] == '\0';

#ifdef HAVE_SYS_STAT_H
    if( !b_stdin && (i_stat = stat( psz_name, &stat_info )) == (-1) )
    {
        msg_Warn( p_access, "cannot stat() file `%s' (%s)",
                  psz_name, strerror(errno));
        return VLC_EGENERIC;
    }
#endif

    p_access->pf_read = Read;
    p_access->pf_block = NULL;
    p_access->pf_seek = Seek;
    p_access->pf_control = Control;
    p_access->info.i_update = 0;
    p_access->info.i_size = 0;
    p_access->info.i_pos = 0;
    p_access->info.b_eof = VLC_FALSE;
    p_access->info.i_title = 0;
    p_access->info.i_seekpoint = 0;
    p_access->p_sys = p_sys = malloc( sizeof( access_sys_t ) );
    p_sys->i_nb_reads = 0;
    p_sys->b_kfir = VLC_FALSE;
    p_sys->file = NULL;
    p_sys->i_file = 0;
    p_sys->i_index = 0;
#ifndef UNDER_CE
    p_sys->fd = -1;
#endif

    if( !strcasecmp( p_access->psz_access, "stream" ) )
    {
        p_sys->b_seekable = VLC_FALSE;
        p_sys->b_pace_control = VLC_FALSE;
    }
    else if( !strcasecmp( p_access->psz_access, "kfir" ) )
    {
        p_sys->b_seekable = VLC_FALSE;
        p_sys->b_pace_control = VLC_FALSE;
        p_sys->b_kfir = VLC_TRUE;
    }
    else
    {
        /* file:%s or %s */
        p_sys->b_pace_control = VLC_TRUE;

        if( b_stdin )
        {
            p_sys->b_seekable = VLC_FALSE;
        }
#ifdef UNDER_CE
        else if( VLC_TRUE )
        {
            /* We'll update i_size after it's been opened */
            p_sys->b_seekable = VLC_TRUE;
        }
#elif defined( HAVE_SYS_STAT_H )
        else if( S_ISREG(stat_info.st_mode) || S_ISCHR(stat_info.st_mode) ||
                 S_ISBLK(stat_info.st_mode) )
        {
            p_sys->b_seekable = VLC_TRUE;
            p_access->info.i_size = stat_info.st_size;
        }
        else if( S_ISFIFO(stat_info.st_mode)
#   if !defined( SYS_BEOS ) && !defined( WIN32 )
                  || S_ISSOCK(stat_info.st_mode)
#   endif
               )
        {
            p_sys->b_seekable = VLC_FALSE;
        }
#endif
        else
        {
            msg_Err( p_access, "unknown file type for `%s'", psz_name );
            return VLC_EGENERIC;
        }
    }

    msg_Dbg( p_access, "opening file `%s'", psz_name );

    if( b_stdin )
    {
        p_sys->fd = 0;
    }
    else if( _OpenFile( p_access, psz_name ) )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( p_sys->b_seekable && !p_access->info.i_size )
    {
        /* FIXME that's bad because all others access will be probed */
        msg_Err( p_access, "file %s is empty, aborting", psz_name );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Update default_pts to a suitable value for file access */
    var_Create( p_access, "file-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    /*
     * Get the additional list of files
     */
    p_file = malloc( sizeof(file_entry_t) );
    p_file->i_size = p_access->info.i_size;
    p_file->psz_name = strdup( psz_name );
    TAB_APPEND( p_sys->i_file, p_sys->file, p_file );

    psz = var_CreateGetString( p_access, "file-cat" );
    if( *psz )
    {
        char *psz_parser = psz_name = psz;
        int64_t i_size;

        while( psz_name && *psz_name )
        {
            psz_parser = strchr( psz_name, ',' );
            if( psz_parser ) *psz_parser = 0;

            psz_name = strdup( psz_name );
            if( psz_name )
            {
                msg_Dbg( p_access, "adding file `%s'", psz_name );
                i_size = 0;

#ifdef HAVE_SYS_STAT_H
                if( !stat( psz_name, &stat_info ) )
                {
                    p_access->info.i_size += stat_info.st_size;
                    i_size = stat_info.st_size;
                }
                else
                {
                    msg_Dbg( p_access, "cannot stat() file `%s'", psz_name );
                }
#endif
                p_file = malloc( sizeof(file_entry_t) );
                p_file->i_size = i_size;
                p_file->psz_name = psz_name;

                TAB_APPEND( p_sys->i_file, p_sys->file, p_file );
            }

            psz_name = psz_parser;
            if( psz_name ) psz_name++;
        }
    }
    free( psz );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;
    int i;

    close( p_sys->fd );

    for( i = 0; i < p_sys->i_file; i++ )
    {
        free( p_sys->file[i]->psz_name );
        free( p_sys->file[i] );
    }
    free( p_sys->file );

    free( p_sys );
}

/*****************************************************************************
 * Read: standard read on a file descriptor.
 *****************************************************************************/
static int Read( access_t *p_access, uint8_t *p_buffer, int i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_ret;

#if !defined(WIN32) && !defined(UNDER_CE)
    if( !p_sys->b_pace_control )
    {
        if( !p_sys->b_kfir )
        {
            /* Find if some data is available. This won't work under Windows. */
            struct timeval  timeout;
            fd_set          fds;

            /* Initialize file descriptor set */
            FD_ZERO( &fds );
            FD_SET( p_sys->fd, &fds );

            /* We'll wait 0.5 second if nothing happens */
            timeout.tv_sec = 0;
            timeout.tv_usec = 500000;

            /* Find if some data is available */
            while( (i_ret = select( p_sys->fd + 1, &fds, NULL, NULL, &timeout )) == 0
                    || (i_ret < 0 && errno == EINTR) )
            {
                FD_ZERO( &fds );
                FD_SET( p_sys->fd, &fds );
                timeout.tv_sec = 0;
                timeout.tv_usec = 500000;

                if( p_access->b_die )
                    return 0;
            }

            if( i_ret < 0 )
            {
                msg_Err( p_access, "select error (%s)", strerror(errno) );
                return -1;
            }

            i_ret = read( p_sys->fd, p_buffer, i_len );
        }
        else
        {
            /* b_kfir ; work around a buggy poll() driver implementation */
            while ( (i_ret = read( p_sys->fd, p_buffer, i_len )) == 0 &&
                    !p_access->b_die )
            {
                msleep( INPUT_ERROR_SLEEP );
            }
        }
    }
    else
#endif /* WIN32 || UNDER_CE */
    {
        /* b_pace_control || WIN32 */
        i_ret = read( p_sys->fd, p_buffer, i_len );
    }

    if( i_ret < 0 )
    {
        if( errno != EINTR && errno != EAGAIN )
            msg_Err( p_access, "read failed (%s)", strerror(errno) );

        /* Delay a bit to avoid consuming all the CPU. This is particularly
         * useful when reading from an unconnected FIFO. */
        msleep( INPUT_ERROR_SLEEP );
    }

    p_sys->i_nb_reads++;
#ifdef HAVE_SYS_STAT_H
    if( p_access->info.i_size != 0 &&
        (p_sys->i_nb_reads % INPUT_FSTAT_NB_READS) == 0 )
    {
        struct stat stat_info;
        int i_file = p_sys->i_index;

        if ( fstat( p_sys->fd, &stat_info ) == -1 )
        {
            msg_Warn( p_access, "couldn't stat again the file (%s)", strerror(errno) );
        }
        else if ( p_sys->file[i_file]->i_size != stat_info.st_size )
        {
            p_access->info.i_size += (stat_info.st_size - p_sys->file[i_file]->i_size );
            p_sys->file[i_file]->i_size = stat_info.st_size;
            p_access->info.i_update |= INPUT_UPDATE_SIZE;
        }
    }
#endif

    /* If we reached an EOF then switch to the next file in the list */
    if ( i_ret == 0 && p_sys->i_index + 1 < p_sys->i_file )
    {
        char *psz_name = p_sys->file[++p_sys->i_index]->psz_name;
        p_sys->fd_backup = p_sys->fd;

        msg_Dbg( p_access, "opening file `%s'", psz_name );

        if ( _OpenFile( p_access, psz_name ) )
        {
            p_sys->fd = p_sys->fd_backup;
            return 0;
        }

        close( p_sys->fd_backup );

        /* We have to read some data */
        return Read( p_access, p_buffer, i_len );
    }

    if( i_ret > 0 )
        p_access->info.i_pos += i_ret;
    else if( i_ret == 0 )
        p_access->info.b_eof = VLC_TRUE;

    return i_ret;
}

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek( access_t *p_access, int64_t i_pos )
{
    access_sys_t *p_sys = p_access->p_sys;
    int64_t i_size = 0;

    /* Check which file we need to access */
    if( p_sys->i_file > 1 )
    {
        int i;
        char *psz_name;
        p_sys->fd_backup = p_sys->fd;

        for( i = 0; i < p_sys->i_file - 1; i++ )
        {
            if( i_pos < p_sys->file[i]->i_size + i_size )
                break;
            i_size += p_sys->file[i]->i_size;
        }
        psz_name = p_sys->file[i]->psz_name;

        msg_Dbg( p_access, "opening file `%s'", psz_name );

        if ( i != p_sys->i_index && !_OpenFile( p_access, psz_name ) )
        {
            /* Close old file */
            close( p_sys->fd_backup );
            p_sys->i_index = i;
        }
        else
        {
            p_sys->fd = p_sys->fd_backup;
        }
    }

    lseek( p_sys->fd, i_pos - i_size, SEEK_SET );

    p_access->info.i_pos = i_pos;
    if( p_access->info.i_size < p_access->info.i_pos )
    {
        msg_Err( p_access, "seeking too far" );
        p_access->info.i_pos = p_access->info.i_size;
    }
    else if( p_access->info.i_pos < 0 )
    {
        msg_Err( p_access, "seeking too early" );
        p_access->info.i_pos = 0;
    }
    /* Reset eof */
    p_access->info.b_eof = VLC_FALSE;

    /* FIXME */
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    access_sys_t *p_sys = p_access->p_sys;
    vlc_bool_t   *pb_bool;
    int          *pi_int;
    int64_t      *pi_64;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = p_sys->b_seekable;
            break;

        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = p_sys->b_pace_control;
            break;

        /* */
        case ACCESS_GET_MTU:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = 0;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = var_GetInteger( p_access, "file-caching" ) * I64C(1000);
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
            /* Nothing to do */
            break;

        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
        case ACCESS_GET_META:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}


/*****************************************************************************
 * OpenFile: Opens a specific file
 *****************************************************************************/
static int _OpenFile( access_t * p_access, char * psz_name )
{
    access_sys_t *p_sys = p_access->p_sys;

#ifdef UNDER_CE
    p_sys->fd = fopen( psz_name, "rb" );
    if ( !p_sys->fd )
    {
        msg_Err( p_access, "cannot open file %s", psz_name );
        return VLC_EGENERIC;
    }

    fseek( p_sys->fd, 0, SEEK_END );
    p_access->info.i_size = ftell( p_sys->fd );
    p_access->info.i_update |= INPUT_UPDATE_SIZE;
    fseek( p_sys->fd, 0, SEEK_SET );
#else

    p_sys->fd = open( psz_name, O_NONBLOCK /*| O_LARGEFILE*/ );
    if ( p_sys->fd == -1 )
    {
        msg_Err( p_access, "cannot open file %s (%s)", psz_name,
                 strerror(errno) );
        return VLC_EGENERIC;
    }
#endif

    return VLC_SUCCESS;
}
