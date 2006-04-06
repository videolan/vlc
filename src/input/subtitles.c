/*****************************************************************************
 * subtitles.c
 *****************************************************************************
 * Copyright (C) 2003-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan.org>
 * This is adapted code from the GPL'ed MPlayer (http://mplayerhq.hu)
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

/**
 *  \file
 *  This file contains functions to dectect subtitle files.
 */

#include <stdlib.h>
#include <vlc/vlc.h>
#include <vlc/input.h>
#include "charset.h"

#ifdef HAVE_DIRENT_H
#   include <dirent.h>
#endif

#ifdef HAVE_LIMITS_H
#   include <limits.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <ctype.h>

/**
 * What's between a directory and a filename?
 */
#if defined( WIN32 )
    #define DIRECTORY_SEPARATOR '\\'
#else
    #define DIRECTORY_SEPARATOR '/'
#endif

/**
 * We are not going to autodetect more subtitle files than this.
 */
#define MAX_SUBTITLE_FILES 128


/**
 * The possible extensions for subtitle files we support
 */
static const char * sub_exts[] = {  "utf", "utf8", "utf-8", "sub", "srt", "smi", "txt", "ssa", "idx", NULL };
/* extensions from unsupported types */
/* rt, aqt, jss, js, ass */

static void strcpy_trim( char *d, char *s )
{
    /* skip leading whitespace */
    while( *s && !isalnum(*s) )
    {
        s++;
    }
    for(;;)
    {
        /* copy word */
        while( *s && isalnum(*s) )
        {
            *d = tolower(*s);
            s++; d++;
        }
        if( *s == 0 ) break;
        /* trim excess whitespace */
        while( *s && !isalnum(*s) )
        {
            s++;
        }
        if( *s == 0 ) break;
        *d++ = ' ';
    }
    *d = 0;
}

static void strcpy_strip_ext( char *d, char *s )
{
    char *tmp = strrchr(s, '.');
    if( !tmp )
    {
        strcpy(d, s);
        return;
    }
    else
    {
        strncpy(d, s, tmp - s);
        d[tmp - s] = 0;
    }
    while( *d )
    {
        *d = tolower(*d);
        d++;
    }
}

static void strcpy_get_ext( char *d, char *s )
{
    char *tmp = strrchr(s, '.');
    if( !tmp )
    {
        strcpy(d, "");
        return;
    } else strcpy( d, tmp + 1 );
}

static int whiteonly( char *s )
{
  while ( *s )
  {
        if( isalnum( *s ) ) return 0;
        s++;
  }
  return 1;
}

typedef struct _subfn
{
    int priority;
    char *psz_fname;
    char *psz_ext;
} subfn;

static int compare_sub_priority( const void *a, const void *b )
{
    if (((subfn*)a)->priority > ((subfn*)b)->priority)
    {
        return -1;
    }

    if (((subfn*)a)->priority < ((subfn*)b)->priority)
    {
        return 1;
    }

#ifndef UNDER_CE
    return strcoll(((subfn*)a)->psz_fname, ((subfn*)b)->psz_fname);
#else
    return strcmp(((subfn*)a)->psz_fname, ((subfn*)b)->psz_fname);
#endif
}

/* Utility function for scandir */
static int Filter( const char *psz_dir_content )
{
    /* does it end with a subtitle extension? */
    const char *tmp = strrchr( psz_dir_content, '.');
    if( tmp == NULL )
        return 0;
    else
    {
        int i;
        tmp++;

        for( i = 0; sub_exts[i]; i++ )
            if( strcmp( sub_exts[i], tmp ) == 0 )
                return 1;
    }
    return 0;
}


/**
 * Convert a list of paths separated by ',' to a char**
 */
static char **paths_to_list( char *psz_dir, char *psz_path )
{
    unsigned int i, k, i_nb_subdirs;
    char **subdirs; /* list of subdirectories to look in */

    if( !psz_dir ) return NULL;
    if( !psz_path ) return NULL;

    i_nb_subdirs = 1;
    for( k = 0; k < strlen( psz_path ); k++ )
    {
        if( psz_path[k] == ',' )
        {
            i_nb_subdirs++;
        }
    }

    if( i_nb_subdirs > 0 )
    {
        char *psz_parser = NULL, *psz_temp = NULL;

        subdirs = (char**)malloc( sizeof(char*) * ( i_nb_subdirs + 1 ) );
        memset( subdirs, 0, sizeof(char*) * ( i_nb_subdirs + 1 ) );
        i = 0;
        psz_parser = psz_path;
        while( psz_parser && *psz_parser )
        {
            char *psz_subdir;
            psz_subdir = psz_parser;
            psz_parser = strchr( psz_subdir, ',' );
            if( psz_parser )
            {
                *psz_parser = '\0';
                psz_parser++;
                while( *psz_parser == ' ' )
                {
                    psz_parser++;
                }
            }
            if( strlen( psz_subdir ) > 0 )
            {
                psz_temp = (char *)malloc( strlen(psz_dir)
                                           + strlen(psz_subdir) + 2 );
                if( psz_temp )
                {
                    sprintf( psz_temp, "%s%s%c",
                             psz_subdir[0] == '.' ? psz_dir : "",
                             psz_subdir,
                             psz_subdir[strlen(psz_subdir) - 1] ==
                              DIRECTORY_SEPARATOR ? '\0' : DIRECTORY_SEPARATOR );
                    subdirs[i] = psz_temp;
                    i++;
                }
            }
        }
        subdirs[i] = NULL;
    }
    else
    {
        subdirs = NULL;
    }
    return subdirs;
}


/**
 * Detect subtitle files.
 *
 * When called this function will split up the psz_name string into a
 * directory, filename and extension. It then opens the directory
 * in which the file resides and tries to find possible matches of
 * subtitles files.
 *
 * \ingroup Demux
 * \param p_this the calling \ref input_thread_t
 * \param psz_path a list of subdirectories (separated by a ',') to look in.
 * \param psz_name the complete filename to base the search on.
 * \return a NULL terminated array of filenames with detected possible subtitles.
 * The array contains max MAX_SUBTITLE_FILES items and you need to free it after use.
 */
char **subtitles_Detect( input_thread_t *p_this, char *psz_path,
                         char *psz_name )
{
    vlc_value_t fuzzy;
    int j, i_result2, i_sub_count = 0, i_fname_len = 0;
    char *f_dir = NULL, *f_fname = NULL, *f_fname_noext = NULL, *f_fname_trim = NULL;
    char *tmp = NULL;

    char **tmp_subdirs, **subdirs; /* list of subdirectories to look in */

    subfn *result = NULL; /* unsorted results */
    char **result2; /* sorted results */

    char *psz_fname_original = strdup( psz_name );
    char *psz_fname = psz_fname_original;

    if( psz_fname == NULL ) return NULL;

    if( !strncmp( psz_fname, "file://", 7 ) )
    {
        psz_fname += 7;
    }

    /* extract filename & dirname from psz_fname */
    tmp = strrchr( psz_fname, DIRECTORY_SEPARATOR );
    if( tmp )
    {
        int dirlen = 0;

        f_fname = malloc( strlen(tmp) );
        if( f_fname )
            strcpy( f_fname, tmp+1 ); // we skip the seperator, so it will still fit in the allocated space
        dirlen = strlen(psz_fname) - strlen(tmp) + 1; // add the seperator
        f_dir = malloc( dirlen + 1 );
        if( f_dir )
        {
            strncpy( f_dir, psz_fname, dirlen );
            f_dir[dirlen] = 0;
        }
    }
    else
    {
        /* Get the current working directory */
        int dirlen;
#ifdef HAVE_UNISTD_H
        f_dir = getcwd( NULL, 0 );
#endif
        if( f_dir == NULL )
        {
            if( psz_fname_original ) free( psz_fname_original );
            return NULL;
        }
        dirlen = strlen( f_dir );
        f_dir = (char *)realloc(f_dir, dirlen +2 );
        f_dir[dirlen] = DIRECTORY_SEPARATOR;
        f_dir[dirlen+1] = '\0';
        f_fname = FromLocaleDup( psz_fname );
    }

    i_fname_len = strlen( f_fname );
    f_fname_noext = malloc(i_fname_len + 1);
    f_fname_trim = malloc(i_fname_len + 1 );

    strcpy_strip_ext( f_fname_noext, f_fname );
    strcpy_trim( f_fname_trim, f_fname_noext );

    result = (subfn*)malloc( sizeof(subfn) * MAX_SUBTITLE_FILES );
    if( result )
        memset( result, 0, sizeof(subfn) * MAX_SUBTITLE_FILES );

    var_Get( p_this, "sub-autodetect-fuzzy", &fuzzy );

    tmp_subdirs = paths_to_list( f_dir, psz_path );
    subdirs = tmp_subdirs;

    for( j = -1; (j == -1) || ( (j >= 0) && (subdirs != NULL) &&
        (*subdirs != NULL) ); j++)
    {
        const char *psz_dir = j < 0 ? f_dir : *subdirs;
        char **ppsz_dir_content;
        int i_dir_content;

        if( psz_dir == NULL )
            continue;

        /* parse psz_src dir */
        i_dir_content = utf8_scandir( psz_dir, &ppsz_dir_content, Filter,
                                      NULL );

        if( i_dir_content != -1 )
        {
            int a;

            msg_Dbg( p_this, "looking for a subtitle file in %s", psz_dir );
            for( a = 0; a < i_dir_content; a++ )
            {
                char *psz_name = vlc_fix_readdir_charset( p_this,
                                                          ppsz_dir_content[a] );
                char tmp_fname_noext[strlen( psz_name ) + 1];
                char tmp_fname_trim[strlen( psz_name ) + 1];
                char tmp_fname_ext[strlen( psz_name ) + 1];

                int i_prio = 0;

                if( psz_name == NULL )
                    continue;

                /* retrieve various parts of the filename */
                strcpy_strip_ext( tmp_fname_noext, psz_name );
                strcpy_get_ext( tmp_fname_ext, psz_name );
                strcpy_trim( tmp_fname_trim, tmp_fname_noext );

                if( !i_prio && !strcmp( tmp_fname_trim, f_fname_trim ) )
                {
                    /* matches the movie name exactly */
                    i_prio = 4;
                }
                if( !i_prio &&
                    ( tmp = strstr( tmp_fname_trim, f_fname_trim ) ) )
                {
                    /* contains the movie name */
                    tmp += strlen( f_fname_trim );
                    if( whiteonly( tmp ) )
                    {
                        /* chars in front of the movie name */
                        i_prio = 2;
                    }
                    else
                    {
                        /* chars after (and possibly in front of)
                         * the movie name */
                        i_prio = 3;
                    }
                }
                if( !i_prio )
                {
                    /* doesn't contain the movie name */
                    if( j == 0 ) i_prio = 1;
                }
                if( i_prio >= fuzzy.i_int )
                {
                    FILE *f;
                    char psz_path[strlen( psz_dir ) + strlen( psz_name ) + 1];

                    sprintf( psz_path, "%s%s", psz_dir, psz_name );
                    msg_Dbg( p_this,
                                "autodetected subtitle: %s with priority %d",
                                psz_path, i_prio );
                    /* FIXME: a portable wrapper for stat() or access() would be more suited */
                    if( ( f = utf8_fopen( psz_path, "rt" ) ) )
                    {
                        fclose( f );
                        msg_Dbg( p_this,
                                "autodetected subtitle: %s with priority %d",
                                psz_path, i_prio );
                        result[i_sub_count].priority = i_prio;
                        result[i_sub_count].psz_fname = strdup( psz_path );
                        result[i_sub_count].psz_ext = strdup(tmp_fname_ext);
                        i_sub_count++;
                    }
                    else
                    {
                        msg_Dbg( p_this, "fopen failed" );
                    }
                }
                if( i_sub_count >= MAX_SUBTITLE_FILES ) break;
                free( psz_name );
            }
            for( a = 0; a < i_dir_content; a++ )
                free( ppsz_dir_content[a] );
            if( ppsz_dir_content ) free( ppsz_dir_content );
        }
        if( j >= 0 ) if( *subdirs ) free( *subdirs++ );
    }

    if( tmp_subdirs )   free( tmp_subdirs );
    if( f_fname_trim )  free( f_fname_trim );
    if( f_fname_noext ) free( f_fname_noext );
    if( f_fname ) free( f_fname );
    if( f_dir )   free( f_dir );

    qsort( result, i_sub_count, sizeof( subfn ), compare_sub_priority );

    result2 = (char**)malloc( sizeof(char*) * ( i_sub_count + 1 ) );
    if( result2 )
        memset( result2, 0, sizeof(char*) * ( i_sub_count + 1 ) );
    i_result2 = 0;

    for( j = 0; j < i_sub_count; j++ )
    {
        if( result[j].psz_ext && !strcasecmp( result[j].psz_ext, "sub" ) )
        {
            int i;
            for( i = 0; i < i_sub_count; i++ )
            {
                if( result[i].psz_fname && result[j].psz_fname &&
                    !strncasecmp( result[j].psz_fname, result[i].psz_fname,
                                sizeof( result[j].psz_fname) - 4 ) &&
                    !strcasecmp( result[i].psz_ext, "idx" ) )
                    break;
            }
            if( i >= i_sub_count )
            {
                result2[i_result2] = result[j].psz_fname;
                i_result2++;
            }
        }
        else
        {
            result2[i_result2] = result[j].psz_fname;
            i_result2++;
        }
    }

    if( psz_fname_original ) free( psz_fname_original );
    if( result ) free( result );
    return result2;
}
