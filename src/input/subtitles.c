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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_input.h>
#include <vlc_charset.h>

#ifdef HAVE_DIRENT_H
#   include <dirent.h>
#endif

#include <limits.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif
#include <sys/stat.h>

#include <ctype.h>
#include "input_internal.h"

/**
 * We are not going to autodetect more subtitle files than this.
 */
#define MAX_SUBTITLE_FILES 128


/**
 * The possible extensions for subtitle files we support
 */
static const char const sub_exts[][6] = {
    "idx", "sub",  "srt",
    "ssa", "ass",  "smi",
    "utf", "utf8", "utf-8",
    "txt", "rt",   "aqt",
    "usf", "jss",  "cdg",
    "psb", "mpsub","mpl2",
    "pjs", "dks",
    ""
};

static void strcpy_trim( char *d, const char *s )
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

static void strcpy_strip_ext( char *d, const char *s )
{
    const char *tmp = strrchr(s, '.');
    if( !tmp )
    {
        strcpy(d, s);
        return;
    }
    else
        strlcpy(d, s, tmp - s + 1 );
    while( *d )
    {
        *d = tolower(*d);
        d++;
    }
}

static void strcpy_get_ext( char *d, const char *s )
{
    const char *tmp = strrchr(s, '.');
    if( !tmp )
        strcpy(d, "");
    else
        strcpy( d, tmp + 1 );
}

static int whiteonly( const char *s )
{
    while( *s )
    {
        if( isalnum( *s ) )
            return 0;
        s++;
    }
    return 1;
}

enum
{
    SUB_PRIORITY_NONE = 0,
    SUB_PRIORITY_MATCH_NONE = 1,
    SUB_PRIORITY_MATCH_RIGHT = 2,
    SUB_PRIORITY_MATCH_LEFT = 3,
    SUB_PRIORITY_MATCH_ALL = 4,
};
typedef struct
{
    int priority;
    char *psz_fname;
    char *psz_ext;
} vlc_subfn_t;

static int compare_sub_priority( const void *a, const void *b )
{
    const vlc_subfn_t *p0 = a;
    const vlc_subfn_t *p1 = b;

    if( p0->priority > p1->priority )
        return -1;

    if( p0->priority < p1->priority )
        return 1;

#ifndef UNDER_CE
    return strcoll( p0->psz_fname, p1->psz_fname);
#else
    return strcmp( p0->psz_fname, p1->psz_fname);
#endif
}

/*
 * Check if a file ends with a subtitle extension
 */
int subtitles_Filter( const char *psz_dir_content )
{
    const char *tmp = strrchr( psz_dir_content, '.');
    int i;

    if( !tmp )
        return 0;
    tmp++;

    for( i = 0; sub_exts[i][0]; i++ )
        if( strcasecmp( sub_exts[i], tmp ) == 0 )
            return 1;
    return 0;
}


/**
 * Convert a list of paths separated by ',' to a char**
 */
static char **paths_to_list( const char *psz_dir, char *psz_path )
{
    unsigned int i, k, i_nb_subdirs;
    char **subdirs; /* list of subdirectories to look in */
    char *psz_parser = psz_path;

    if( !psz_dir || !psz_path )
        return NULL;

    for( k = 0, i_nb_subdirs = 1; psz_path[k] != '\0'; k++ )
    {
        if( psz_path[k] == ',' )
            i_nb_subdirs++;
    }

    subdirs = calloc( i_nb_subdirs + 1, sizeof(char*) );
    if( !subdirs )
        return NULL;

    for( i = 0; psz_parser && *psz_parser != '\0' ; )
    {
        char *psz_subdir = psz_parser;
        psz_parser = strchr( psz_subdir, ',' );
        if( psz_parser )
        {
            *psz_parser++ = '\0';
            while( *psz_parser == ' ' )
                psz_parser++;
        }
        if( *psz_subdir == '\0' )
            continue;

        if( asprintf( &subdirs[i++], "%s%s%c",
                  psz_subdir[0] == '.' ? psz_dir : "",
                  psz_subdir,
                  psz_subdir[strlen(psz_subdir) - 1] == DIR_SEP_CHAR ?
                                           '\0' : DIR_SEP_CHAR ) == -1 )
            break;
    }
    subdirs[i] = NULL;

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
                         const char *psz_name_org )
{
    int i_fuzzy;
    int j, i_result2, i_sub_count, i_fname_len;
    char *f_dir = NULL, *f_fname = NULL, *f_fname_noext = NULL, *f_fname_trim = NULL;
    char *tmp = NULL;

    char **subdirs; /* list of subdirectories to look in */

    vlc_subfn_t *result = NULL; /* unsorted results */
    char **result2; /* sorted results */
    const char *psz_fname = psz_name_org;

    if( !psz_fname )
        return NULL;

    if( !strncmp( psz_fname, "file://", 7 ) )
        psz_fname += 7;

    /* extract filename & dirname from psz_fname */
    tmp = strrchr( psz_fname, DIR_SEP_CHAR );
    if( tmp )
    {
        const int i_dirlen = strlen(psz_fname)-strlen(tmp)+1; /* include the separator */
        f_fname = strdup( &tmp[1] );    /* skip the separator */
        f_dir = strndup( psz_fname, i_dirlen );
    }
    else
    {
#if defined (HAVE_UNISTD_H) && !defined (UNDER_CE)
        /* Get the current working directory */
        char *psz_cwd = getcwd( NULL, 0 );
#else
        char *psz_cwd = NULL;
#endif
        if( !psz_cwd )
            return NULL;

        f_fname = strdup( psz_fname );
        if( asprintf( &f_dir, "%s%c", psz_cwd, DIR_SEP_CHAR ) == -1 )
            f_dir = NULL; /* Assure that function will return in next test */
        free( psz_cwd );
    }
    if( !f_fname || !f_dir )
    {
        free( f_fname );
        free( f_dir );
        return NULL;
    }

    i_fname_len = strlen( f_fname );

    f_fname_noext = malloc(i_fname_len + 1);
    f_fname_trim = malloc(i_fname_len + 1 );
    if( !f_fname_noext || !f_fname_trim )
    {
        free( f_fname );
        free( f_dir );
        free( f_fname_noext );
        free( f_fname_trim );
        return NULL;
    }

    strcpy_strip_ext( f_fname_noext, f_fname );
    strcpy_trim( f_fname_trim, f_fname_noext );

    i_fuzzy = var_GetInteger( p_this, "sub-autodetect-fuzzy" );

    result = calloc( MAX_SUBTITLE_FILES+1, sizeof(vlc_subfn_t) ); /* We check it later (simplify code) */
    subdirs = paths_to_list( f_dir, psz_path );
    for( j = -1, i_sub_count = 0; (j == -1) || ( j >= 0 && subdirs != NULL && subdirs[j] != NULL ); j++ )
    {
        const char *psz_dir = j < 0 ? f_dir : subdirs[j];
        char **ppsz_dir_content;
        int i_dir_content;
        int a;

        if( psz_dir == NULL || ( j >= 0 && !strcmp( psz_dir, f_dir ) ) )
            continue;

        /* parse psz_src dir */
        i_dir_content = utf8_scandir( psz_dir, &ppsz_dir_content,
                                      subtitles_Filter, NULL );
        if( i_dir_content < 0 )
            continue;

        msg_Dbg( p_this, "looking for a subtitle file in %s", psz_dir );
        for( a = 0; a < i_dir_content && i_sub_count < MAX_SUBTITLE_FILES ; a++ )
        {
            char *psz_name = ppsz_dir_content[a];
            char tmp_fname_noext[strlen( psz_name ) + 1];
            char tmp_fname_trim[strlen( psz_name ) + 1];
            char tmp_fname_ext[strlen( psz_name ) + 1];

            int i_prio;

            if( psz_name == NULL || psz_name[0] == '.' )
                continue;

            /* retrieve various parts of the filename */
            strcpy_strip_ext( tmp_fname_noext, psz_name );
            strcpy_get_ext( tmp_fname_ext, psz_name );
            strcpy_trim( tmp_fname_trim, tmp_fname_noext );

            i_prio = SUB_PRIORITY_NONE;
            if( i_prio == SUB_PRIORITY_NONE && !strcmp( tmp_fname_trim, f_fname_trim ) )
            {
                /* matches the movie name exactly */
                i_prio = SUB_PRIORITY_MATCH_ALL;
            }
            if( i_prio == SUB_PRIORITY_NONE &&
                ( tmp = strstr( tmp_fname_trim, f_fname_trim ) ) )
            {
                /* contains the movie name */
                tmp += strlen( f_fname_trim );
                if( whiteonly( tmp ) )
                {
                    /* chars in front of the movie name */
                    i_prio = SUB_PRIORITY_MATCH_RIGHT;
                }
                else
                {
                    /* chars after (and possibly in front of)
                     * the movie name */
                    i_prio = SUB_PRIORITY_MATCH_LEFT;
                }
            }
            if( i_prio == SUB_PRIORITY_NONE &&
                j == 0 )
            {
                /* doesn't contain the movie name, prefer files in f_dir over subdirs */
                i_prio = SUB_PRIORITY_MATCH_NONE;
            }
            if( i_prio >= i_fuzzy )
            {
                char psz_path[strlen( psz_dir ) + strlen( psz_name ) + 1];
                struct stat st;

                sprintf( psz_path, "%s%s", psz_dir, psz_name );
                if( !strcmp( psz_path, psz_fname ) )
                    continue;

                if( !utf8_stat( psz_path, &st ) && S_ISREG( st.st_mode ) && result )
                {
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
                    msg_Dbg( p_this, "stat failed (autodetecting subtitle: %s with priority %d)",
                             psz_path, i_prio );
                }
            }
        }
        if( ppsz_dir_content )
        {
            for( a = 0; a < i_dir_content; a++ )
                free( ppsz_dir_content[a] );
            free( ppsz_dir_content );
        }
    }
    if( subdirs )
    {
        for( j = 0; subdirs[j]; j++ )
            free( subdirs[j] );
        free( subdirs );
    }
    free( f_fname );
    free( f_dir );
    free( f_fname_trim );
    free( f_fname_noext );

    if( !result )
        return NULL;

    qsort( result, i_sub_count, sizeof(vlc_subfn_t), compare_sub_priority );

    result2 = calloc( i_sub_count + 1, sizeof(char*) );

    for( j = 0, i_result2 = 0; j < i_sub_count && result2 != NULL; j++ )
    {
        bool b_reject = false;

        if( !result[j].psz_fname || !result[j].psz_ext ) /* memory out */
            break;

        if( !strcasecmp( result[j].psz_ext, "sub" ) )
        {
            int i;
            for( i = 0; i < i_sub_count; i++ )
            {
                if( result[i].psz_fname && result[i].psz_ext &&
                    !strncasecmp( result[j].psz_fname, result[i].psz_fname,
                                  strlen( result[j].psz_fname) - 3 ) &&
                    !strcasecmp( result[i].psz_ext, "idx" ) )
                    break;
            }
            if( i < i_sub_count )
                b_reject = true;
        }
        else if( !strcasecmp( result[j].psz_ext, "cdg" ) )
        {
            if( result[j].priority < SUB_PRIORITY_MATCH_ALL )
                b_reject = true;
        }

        /* */
        if( !b_reject )
            result2[i_result2++] = strdup( result[j].psz_fname );
    }

    for( j = 0; j < i_sub_count; j++ )
    {
        free( result[j].psz_fname );
        free( result[j].psz_ext );
    }
    free( result );

    return result2;
}

