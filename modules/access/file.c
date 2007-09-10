/*****************************************************************************
 * file.c: file input (file: access plug-in)
 *****************************************************************************
 * Copyright (C) 2001-2006 the VideoLAN team
 * Copyright © 2006 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Rémi Denis-Courmont <rem # videolan # org>
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
#include <vlc/vlc.h>
#include <vlc_input.h>
#include <vlc_access.h>
#include <vlc_interface.h>

#include <errno.h>
#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif

#if defined( WIN32 ) && !defined( UNDER_CE )
#   include <io.h>
#else
#   include <unistd.h>
#   include <poll.h>
#endif

#if defined( WIN32 ) && !defined( UNDER_CE )
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

#include <vlc_charset.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Caching value for files. This " \
    "value should be set in milliseconds." )
#define CAT_TEXT N_("Concatenate with additional files")
#define CAT_LONGTEXT N_( \
    "Play split files as if they were part of a unique file. " \
    "You need to specify a comma-separated list of files." )

vlc_module_begin();
    set_description( _("File input") );
    set_shortname( _("File") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );
    add_integer( "file-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    add_obsolete_string( "file-cat" );
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

static int  open_file( access_t *, const char * );

struct access_sys_t
{
    unsigned int i_nb_reads;
    vlc_bool_t   b_kfir;

    int fd;

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

    vlc_bool_t    b_stdin = !strcmp (p_access->psz_path, "-");

    /* Update default_pts to a suitable value for file access */
    var_Create( p_access, "file-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    STANDARD_READ_ACCESS_INIT;
    p_sys->i_nb_reads = 0;
    p_sys->b_kfir = VLC_FALSE;
    int fd = p_sys->fd = -1;

    if (!strcasecmp (p_access->psz_access, "stream"))
    {
        p_sys->b_seekable = VLC_FALSE;
        p_sys->b_pace_control = VLC_FALSE;
    }
    else if (!strcasecmp (p_access->psz_access, "kfir"))
    {
        p_sys->b_seekable = VLC_FALSE;
        p_sys->b_pace_control = VLC_FALSE;
        p_sys->b_kfir = VLC_TRUE;
    }
    else
    {
        p_sys->b_seekable = VLC_TRUE;
        p_sys->b_pace_control = VLC_TRUE;
    }

    /* Open file */
    msg_Dbg (p_access, "opening file `%s'", p_access->psz_path);

    if (b_stdin)
        fd = dup (0);
    else
        fd = open_file (p_access, p_access->psz_path);

#ifdef HAVE_SYS_STAT_H
    struct stat st;

    while (fd != -1)
    {
        if (fstat (fd, &st))
            msg_Err (p_access, "fstat(%d): %s", fd, strerror (errno));
        else
        if (S_ISDIR (st.st_mode))
            /* The directory plugin takes care of that */
            msg_Dbg (p_access, "file is a directory, aborting");
        else
            break; // success

        close (fd);
        fd = -1;
    }
#endif

    if (fd == -1)
    {
        free (p_sys);
        return VLC_EGENERIC;
    }
    p_sys->fd = fd;

#ifdef HAVE_SYS_STAT_H
    p_access->info.i_size = st.st_size;
    if (!S_ISREG (st.st_mode) && !S_ISBLK (st.st_mode)
     && (!S_ISCHR (st.st_mode) || (st.st_size == 0)))
        p_sys->b_seekable = VLC_FALSE;
#else
    p_sys->b_seekable = !b_stdin;
# warning File size not known!
#endif

    if (p_sys->b_seekable && (p_access->info.i_size == 0))
    {
        /* FIXME that's bad because all others access will be probed */
        msg_Err (p_access, "file is empty, aborting");
        Close (p_this);
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close the target
 *****************************************************************************/
static void Close (vlc_object_t * p_this)
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    close (p_sys->fd);
    free (p_sys);
}

/*****************************************************************************
 * Read: standard read on a file descriptor.
 *****************************************************************************/
static int Read( access_t *p_access, uint8_t *p_buffer, int i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_ret;
    int fd = p_sys->fd;

#if !defined(WIN32) && !defined(UNDER_CE)
    if( !p_sys->b_pace_control )
    {
        if( !p_sys->b_kfir )
        {
            /* Find if some data is available. This won't work under Windows. */
            do
            {
                struct pollfd ufd;

                if( p_access->b_die )
                    return 0;

                memset (&ufd, 0, sizeof (ufd));
                ufd.fd = fd;
                ufd.events = POLLIN;

                i_ret = poll (&ufd, 1, 500);
            }
            while (i_ret <= 0);

            i_ret = read (fd, p_buffer, i_len);
        }
        else
        {
            /* b_kfir ; work around a buggy poll() driver implementation */
            while (((i_ret = read (fd, p_buffer, i_len)) == 0)
                && !p_access->b_die)
            {
                msleep( INPUT_ERROR_SLEEP );
            }
        }
    }
    else
#endif /* WIN32 || UNDER_CE */
        /* b_pace_control || WIN32 */
        i_ret = read( fd, p_buffer, i_len );

    if( i_ret < 0 )
    {
        switch (errno)
        {
            case EINTR:
            case EAGAIN:
                break;

            default:
                msg_Err (p_access, "read failed (%s)", strerror (errno));
                intf_UserFatal (p_access, VLC_FALSE, _("File reading failed"),
                                _("VLC could not read file \"%s\"."),
                                strerror (errno));
        }

        /* Delay a bit to avoid consuming all the CPU. This is particularly
         * useful when reading from an unconnected FIFO. */
        msleep( INPUT_ERROR_SLEEP );
    }

    p_sys->i_nb_reads++;

#ifdef HAVE_SYS_STAT_H
    if( p_access->info.i_size != 0 &&
        (p_sys->i_nb_reads % INPUT_FSTAT_NB_READS) == 0 )
    {
        struct stat st;

        if ((fstat (fd, &st) == 0)
         && (p_access->info.i_size != st.st_size))
        {
            p_access->info.i_size = st.st_size;
            p_access->info.i_update |= INPUT_UPDATE_SIZE;
        }
    }
#endif

    if( i_ret > 0 )
        p_access->info.i_pos += i_ret;
    else if( i_ret == 0 )
        p_access->info.b_eof = VLC_TRUE;

    return i_ret;
}

/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek (access_t *p_access, int64_t i_pos)
{
    if (i_pos > p_access->info.i_size)
    {
        msg_Err (p_access, "seeking too far");
        i_pos = p_access->info.i_size;
    }
    else if (i_pos < 0)
    {
        msg_Err (p_access, "seeking too early");
        i_pos = 0;
    }

    p_access->info.i_pos = i_pos;
    p_access->info.b_eof = VLC_FALSE;

    /* Determine which file we need to access */
    lseek (p_access->p_sys->fd, i_pos, SEEK_SET);
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


static char *expand_path (const access_t *p_access, const char *path)
{
    if (strncmp (path, "~/", 2) == 0)
    {
        char *res;

         // TODO: we should also support the ~cmassiot/ syntax
         if (asprintf (&res, "%s/%s", p_access->p_libvlc->psz_homedir, path + 2) == -1)
             return NULL;
         return res;
    }

#if defined(WIN32)
    if (!strcasecmp (p_access->psz_access, "file")
      && ('/' == path[0]) && path[1] && (':' == path[2]) && ('/' == path[3]))
        // Explorer can open path such as file:/C:/ or file:///C:/
        // hence remove leading / if found
        return strdup (path + 1);
#endif

    return strdup (path);
}


/*****************************************************************************
 * open_file: Opens a specific file
 *****************************************************************************/
static int open_file (access_t *p_access, const char *psz_name)
{
    char *path = expand_path (p_access, psz_name);

#ifdef UNDER_CE
    p_sys->fd = utf8_fopen( path, "rb" );
    if ( !p_sys->fd )
    {
        msg_Err( p_access, "cannot open file %s", psz_name );
        intf_UserFatal( p_access, VLC_FALSE, _("File reading failed"),
                        _("VLC could not open file \"%s\"."), psz_name );
        free (path);
        return VLC_EGENERIC;
    }

    fseek( p_sys->fd, 0, SEEK_END );
    p_access->info.i_size = ftell( p_sys->fd );
    p_access->info.i_update |= INPUT_UPDATE_SIZE;
    fseek( p_sys->fd, 0, SEEK_SET );
#else
    int fd = utf8_open (path, O_RDONLY | O_NONBLOCK /* O_LARGEFILE*/, 0666);
    free (path);
    if (fd == -1)
    {
        msg_Err (p_access, "cannot open file %s (%s)", psz_name,
                 strerror (errno));
        intf_UserFatal (p_access, VLC_FALSE, _("File reading failed"),
                        _("VLC could not open file \"%s\" (%s)."),
                        psz_name, strerror (errno));
        return -1;
    }

# if defined(HAVE_FCNTL_H) && defined(F_FDAHEAD) && defined(F_NOCACHE)
    /* We'd rather use any available memory for reading ahead
     * than for caching what we've already seen/heard */
    fcntl (fd, F_RDAHEAD, 1);
    fcntl (fd, F_NOCACHE, 1);
# endif
#endif

    return fd;
}
