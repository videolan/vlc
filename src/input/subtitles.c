/*****************************************************************************
 * subtitles.c
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: subtitles.c,v 1.3 2003/10/02 00:16:05 hartman Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/**
 *  \file
 *  This file contains functions to dectect subtitle files.
 */

#include <stdlib.h>
#include <vlc/vlc.h>
#include <vlc/input.h>

#include "ninput.h"
#include <dirent.h>
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
 * This determines how fuzzy the returned results will be.
 *
 * Currently set to 3, other options are:
 * 0 = nothing
 * 1 = any subtitle file
 * 2 = any sub file containing movie name
 * 3 = sub file matching movie name exactly
 * 4 = sub file matching movie name with additional chars 
 */
#define SUB_FUZZY 3

/**
 * The possible extentions for subtitle files we support
 */
static const char * sub_exts[] = {  "utf", "utf8", "utf-8", "sub", "srt", "smi", "txt", "ssa", NULL};
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
	if (*s == 0) break;
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
    if( !tmp ) {
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
} subfn;

static int compare_sub_priority( const void *a, const void *b )
{
    if (((subfn*)a)->priority > ((subfn*)b)->priority) {
	return -1;
    } else if (((subfn*)a)->priority < ((subfn*)b)->priority) {
	return 1;
    } else {
	return strcoll(((subfn*)a)->psz_fname, ((subfn*)b)->psz_fname);
    }
}

/**
 * Detect subtitle files.
 * 
 * When called this function will split up the psz_fname string into a
 * directory, filename and extension. It then opens the directory
 * in which the file resides and tries to find possible matches of
 * subtitles files. 
 *
 * \ingroup Demux
 * \param p_this the calling \ref input_thread_t
 * \param psz_path a subdirectory to look into. This is not used atm.
 * \param psz_fname the complete filename to base the search on.
 * \return a NULL terminated array of filenames with detected possible subtitles.
 * The array contains max MAX_SUBTITLE_FILES items and you need to free it after use.
 */
char** subtitles_Detect( input_thread_t *p_this, char *psz_path, char *psz_fname )
{
    /* variables to be used for derivatives of psz_fname */
    char *f_dir, *f_fname, *f_fname_noext, *f_fname_trim, *tmp;
    /* variables to be used for derivatives FILE *f */
    char *tmp_fname_noext, *tmp_fname_trim, *tmp_fname_ext, *tmpresult;
 
    int len, i, j, i_sub_count;
    subfn *result; /* unsorted results */
    char **result2; /* sorted results */
 
    FILE *f;
    DIR *d;
    struct dirent *de;

    i_sub_count = 0;
    len = ( strlen( psz_fname ) > 256 ? strlen( psz_fname ) : 256 ) +
        ( strlen( psz_path ) > 256 ? strlen( psz_path ) : 256 ) + 2;

    f_dir = (char*)malloc(len);
    f_fname = (char*)malloc(len);
    f_fname_noext = (char*)malloc(len);
    f_fname_trim = (char*)malloc(len);

    tmp_fname_noext = (char*)malloc(len);
    tmp_fname_trim = (char*)malloc(len);
    tmp_fname_ext = (char*)malloc(len);

    tmpresult = (char*)malloc(len);

    result = (subfn*)malloc( sizeof(subfn) * MAX_SUBTITLE_FILES );
    memset( result, 0, sizeof(subfn) * MAX_SUBTITLE_FILES );

    /* extract filename & dirname from psz_fname */
    tmp = strrchr( psz_fname, DIRECTORY_SEPARATOR );
    if( tmp )
    {
        int pos;
	strcpy( f_fname, tmp + 1 );
	pos = tmp - psz_fname;
	strncpy( f_dir, psz_fname, pos + 1 );
	f_dir[pos + 1] = 0;
    }
    else
    {
	strcpy( f_fname, psz_fname );
	strcpy( f_dir, "" );
    }

    strcpy_strip_ext( f_fname_noext, f_fname );
    strcpy_trim( f_fname_trim, f_fname_noext );

    for( j = 0; j <= 1; j++)
    {
	d = opendir( j == 0 ? f_dir : psz_path );
	if( d )
        {
            int b_found;
	    while( ( de = readdir( d ) ) )
            {
		/* retrieve various parts of the filename */
		strcpy_strip_ext( tmp_fname_noext, de->d_name );
		strcpy_get_ext( tmp_fname_ext, de->d_name );
		strcpy_trim( tmp_fname_trim, tmp_fname_noext );

		/* does it end with a subtitle extension? */
		b_found = 0;
		for( i = 0; sub_exts[i]; i++ )
                {
		    if( strcmp(sub_exts[i], tmp_fname_ext ) == 0 )
                    {
			b_found = 1;
                        msg_Dbg( p_this, "found a possible subtitle: %s", de->d_name );
			break;
		    }
		}
 
		/* we have a (likely) subtitle file */
		if( b_found )
                {
		    int i_prio = 0;
		    if( !i_prio && strcmp( tmp_fname_trim, f_fname_trim ) == 0 )
                    {
			/* matches the movie name exactly */
			i_prio = 4;
		    }
		    if( !i_prio && ( tmp = strstr( tmp_fname_trim, f_fname_trim ) ) )
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
                            /* chars after (and possibly in front of) the movie name */
                            i_prio = 3;
                        }
		    }
		    if( !i_prio )
                    {
			/* doesn't contain the movie name */
			if( j == 0 ) i_prio = 1;
		    }

		    if( i_prio >= SUB_FUZZY )
                    {
                        sprintf( tmpresult, "%s%s", j == 0 ? f_dir : psz_path, de->d_name );
                        msg_Dbg( p_this, "autodetected subtitle: %s with priority %d", de->d_name, i_prio );
			if( ( f = fopen( tmpresult, "rt" ) ) )
                        {
			    fclose( f );
			    result[i_sub_count].priority = i_prio;
			    result[i_sub_count].psz_fname = strdup( tmpresult );
			    i_sub_count++;
			}
		    }

		}
		if( i_sub_count >= MAX_SUBTITLE_FILES ) break;
	    }
	    closedir( d );
	}
    }
    
    free( f_dir );
    free( f_fname );
    free( f_fname_noext );
    free( f_fname_trim );

    free( tmp_fname_noext );
    free( tmp_fname_trim );
    free( tmp_fname_ext );

    free( tmpresult );

    qsort( result, i_sub_count, sizeof( subfn ), compare_sub_priority );

    result2 = (char**)malloc( sizeof(char*) * ( i_sub_count + 1 ) );
    memset( result2, 0, sizeof(char*) * ( i_sub_count + 1 ) );

    for( i = 0; i < i_sub_count; i++ )
    {
	result2[i] = result[i].psz_fname;
    }
    result2[i_sub_count] = NULL;
    free( result );
    return result2;
}

