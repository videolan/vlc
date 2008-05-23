/*****************************************************************************
 * winvlc.c: the Windows VLC player
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Lots of other people, see the libvlc AUTHORS file
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define UNICODE
#include <vlc/vlc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

static void find_end_quote( char **s, char **ppsz_parser, int i_quote )
{
    int i_bcount = 0;

    while( **s )
    {
        if( **s == '\\' )
        {
            **ppsz_parser = **s;
            (*ppsz_parser)++; (*s)++;
            i_bcount++;
        }
        else if( **s == '"' || **s == '\'' )
        {
            /* Preceeded by a number of '\' which we erase. */
            *ppsz_parser -= i_bcount / 2;
            if( i_bcount & 1 )
            {
                /* '\\' followed by a '"' or '\'' */
                *ppsz_parser -= 1;
                **ppsz_parser = **s;
                (*ppsz_parser)++; (*s)++;
                i_bcount = 0;
                continue;
            }

            if( **s == i_quote )
            {
                /* End */
                return;
            }
            else
            {
                /* Different quoting */
                int i_quote = **s;
                **ppsz_parser = **s;
                (*ppsz_parser)++; (*s)++;
                find_end_quote( s, ppsz_parser, i_quote );
                **ppsz_parser = **s;
                (*ppsz_parser)++; (*s)++;
            }

            i_bcount = 0;
        }
        else
        {
            /* A regular character */
            **ppsz_parser = **s;
            (*ppsz_parser)++; (*s)++;
            i_bcount = 0;
        }
    }
}

/*************************************************************************
 * vlc_parse_cmdline: Command line parsing into elements.
 *
 * The command line is composed of space/tab separated arguments.
 * Quotes can be used as argument delimiters and a backslash can be used to
 * escape a quote.
 *************************************************************************/
static char **vlc_parse_cmdline( const char *psz_cmdline, int *i_args )
{
    char **argv = NULL;
    char *s, *psz_parser, *psz_arg, *psz_orig;
    int i_bcount = 0, argc = 0;

    psz_orig = strdup( psz_cmdline );
    psz_arg = psz_parser = s = psz_orig;

    while( *s )
    {
        if( *s == '\t' || *s == ' ' )
        {
            /* We have a complete argument */
            *psz_parser = 0;
            argv = realloc( argv, (argc + 1) * sizeof (char *) );
            argv[argc] = psz_arg;

            /* Skip trailing spaces/tabs */
            do{ s++; } while( *s == '\t' || *s == ' ' );

            /* New argument */
            psz_arg = psz_parser = s;
            i_bcount = 0;
        }
        else if( *s == '\\' )
        {
            *psz_parser++ = *s++;
            i_bcount++;
        }
        else if( *s == '"' || *s == '\'' )
        {
            if( ( i_bcount & 1 ) == 0 )
            {
                /* Preceeded by an even number of '\', this is half that
                 * number of '\', plus a quote which we erase. */
                int i_quote = *s;
                psz_parser -= i_bcount / 2;
                s++;
                find_end_quote( &s, &psz_parser, i_quote );
                s++;
            }
            else
            {
                /* Preceeded by an odd number of '\', this is half that
                 * number of '\' followed by a '"' */
                psz_parser = psz_parser - i_bcount/2 - 1;
                *psz_parser++ = '"';
                s++;
            }
            i_bcount = 0;
        }
        else
        {
            /* A regular character */
            *psz_parser++ = *s++;
            i_bcount = 0;
        }
    }

    /* Take care of the last arg */
    if( *psz_arg )
    {
        *psz_parser = '\0';
        argv = realloc( argv, (argc + 1) * sizeof (char *) );
        argv[argc] = psz_arg;
    }

    *i_args = argc;
    return argv;
}


#ifdef UNDER_CE
# define wWinMain WinMain
#endif

/*****************************************************************************
 * wWinMain: parse command line, start interface and spawn threads.
 *****************************************************************************/
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPWSTR lpCmdLine, int nCmdShow )
{
    char **argv, psz_cmdline[wcslen(lpCmdLine) * 4];
    int argc, ret;

    (void)hInstance; (void)hPrevInstance; (void)nCmdShow;
    /* This clutters OSX GUI error logs */
    fprintf( stderr, "VLC media player %s\n", libvlc_get_version() );

    WideCharToMultiByte( CP_UTF8, 0, lpCmdLine, -1,
                         psz_cmdline, MAX_PATH, NULL, NULL );
    argv = vlc_parse_cmdline( psz_cmdline, &argc );

    libvlc_exception_t ex;
    libvlc_exception_init (&ex);

    /* Initialize libvlc */
    libvlc_instance_t *vlc = libvlc_new (argc, (const char **)argv, &ex);
    if (vlc != NULL)
    {
        libvlc_run_interface (vlc, NULL, &ex);
        libvlc_release (vlc);
    }

    ret = libvlc_exception_raised (&ex);
    libvlc_exception_clear (&ex);
    return ret;
}
