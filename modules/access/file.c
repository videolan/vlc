/*****************************************************************************
 * file.c: file input (file: access plug-in)
 *****************************************************************************
 * Copyright (C) 2001-2006 the VideoLAN team
 * Copyright © 2006-2007 Rémi Denis-Courmont
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_access.h>
#include <vlc_interface.h>

#include <assert.h>
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

#if defined( WIN32 )
#   include <io.h>
#   include <ctype.h>
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
/* FIXME the commandline on wince is a mess */
# define dup(a) -1
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

vlc_module_begin ()
    set_description( N_("File input") )
    set_shortname( N_("File") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    add_integer( "file-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT, CACHING_LONGTEXT, true )
    add_obsolete_string( "file-cat" )
    set_capability( "access", 50 )
    add_shortcut( "file" )
    add_shortcut( "stream" )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int  Seek( access_t *, int64_t );
static int  NoSeek( access_t *, int64_t );
static ssize_t Read( access_t *, uint8_t *, size_t );
static int  Control( access_t *, int, va_list );

static int  open_file( access_t *, const char * );

struct access_sys_t
{
    unsigned int i_nb_reads;

    int fd;

    /* */
    bool b_pace_control;
};

/*****************************************************************************
 * Open: open the file
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;

    bool    b_stdin = !strcmp (p_access->psz_path, "-");

    /* Update default_pts to a suitable value for file access */
    var_Create( p_access, "file-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    STANDARD_READ_ACCESS_INIT;
    p_sys->i_nb_reads = 0;

    if (!strcasecmp (p_access->psz_access, "stream"))
        p_sys->b_pace_control = false;
    else
        p_sys->b_pace_control = true;

    /* Open file */
    msg_Dbg (p_access, "opening file `%s'", p_access->psz_path);

    int fd = -1;
    if (b_stdin)
        fd = dup (0);
    else
        fd = open_file (p_access, p_access->psz_path);
    if (fd == -1)
        goto error;

#ifdef HAVE_SYS_STAT_H
    struct stat st;

    if (fstat (fd, &st))
    {
        msg_Err (p_access, "failed to read (%m)");
        goto error;
    }
    /* Directories can be opened and read from, but only readdir() knows
     * how to parse the data. The directory plugin will do it. */
    if (S_ISDIR (st.st_mode))
    {
        msg_Dbg (p_access, "ignoring directory");
        goto error;
    }
    if (S_ISREG (st.st_mode))
        p_access->info.i_size = st.st_size;
    else if (!S_ISBLK (st.st_mode))
        p_access->pf_seek = NoSeek;
#else
    if (b_stdin)
        p_access->pf_seek = NoSeek;
# warning File size not known!
#endif

    p_sys->fd = fd;
    return VLC_SUCCESS;

error:
    if (fd != -1)
        close (fd);
    free (p_sys);
    return VLC_EGENERIC;
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


#include <vlc_network.h>

/*****************************************************************************
 * Read: standard read on a file descriptor.
 *****************************************************************************/
static ssize_t Read( access_t *p_access, uint8_t *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    int fd = p_sys->fd;
    ssize_t i_ret;

#ifndef WIN32
    if (p_access->pf_seek == NoSeek)
        i_ret = net_Read (p_access, fd, NULL, p_buffer, i_len, false);
    else
#endif
        i_ret = read (fd, p_buffer, i_len);

    if( i_ret < 0 )
    {
        switch (errno)
        {
            case EINTR:
            case EAGAIN:
                break;

            default:
                msg_Err (p_access, "failed to read (%m)");
                intf_UserFatal (p_access, false, _("File reading failed"),
                                _("VLC could not read the file."));
                p_access->info.b_eof = true;
                return 0;
        }
    }
    else if( i_ret > 0 )
        p_access->info.i_pos += i_ret;
    else
        p_access->info.b_eof = true;

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
    return i_ret;
}


/*****************************************************************************
 * Seek: seek to a specific location in a file
 *****************************************************************************/
static int Seek (access_t *p_access, int64_t i_pos)
{
    p_access->info.i_pos = i_pos;
    p_access->info.b_eof = false;

    lseek (p_access->p_sys->fd, i_pos, SEEK_SET);
    return VLC_SUCCESS;
}

static int NoSeek (access_t *p_access, int64_t i_pos)
{
    /* assert(0); ?? */
    (void) p_access; (void) i_pos;
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    access_sys_t *p_sys = p_access->p_sys;
    bool    *pb_bool;
    int64_t *pi_64;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = (p_access->pf_seek != NoSeek);
            break;

        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = p_sys->b_pace_control;
            break;

        /* */
        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = var_GetInteger( p_access, "file-caching" ) * INT64_C(1000);
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
        case ACCESS_GET_PRIVATE_ID_STATE:
        case ACCESS_GET_CONTENT_TYPE:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query %d in control", i_query );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * open_file: Opens a specific file
 *****************************************************************************/
static int open_file (access_t *p_access, const char *path)
{
#if defined(WIN32)
    if (!strcasecmp (p_access->psz_access, "file")
      && ('/' == path[0]) && isalpha (path[1])
      && (':' == path[2]) && ('/' == path[3]))
        /* Explorer can open path such as file:/C:/ or file:///C:/
         * hence remove leading / if found */
        path++;
#endif

    int fd = utf8_open (path, O_RDONLY | O_NONBLOCK /* O_LARGEFILE*/, 0666);
    if (fd == -1)
    {
        msg_Err (p_access, "cannot open file %s (%m)", path);
        intf_UserFatal (p_access, false, _("File reading failed"),
                        _("VLC could not open the file \"%s\"."), path);
        return -1;
    }

#if defined(HAVE_FCNTL)
    /* We'd rather use any available memory for reading ahead
     * than for caching what we've already seen/heard */
# if defined(F_RDAHEAD)
    fcntl (fd, F_RDAHEAD, 1);
# endif
# if defined(F_NOCACHE)
    fcntl (fd, F_NOCACHE, 1);
# endif
#endif

    return fd;
}
