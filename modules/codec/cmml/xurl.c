/*****************************************************************************
 * xurl.c: URL manipulation functions
 *****************************************************************************
 * Copyright (C) 2003-2004 Commonwealth Scientific and Industrial Research
 *                         Organisation (CSIRO) Australia
 * Copyright (C) 2004-2008 the VideoLAN team
 *
 * $Id$
 *
 * Authors: Andre Pang <Andre.Pang@csiro.au>
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

#include <stdio.h>

#include "xurl.h"

static char *streallocat( char *psz_string, const char *psz_to_append );

char        *XURL_FindQuery             ( char *psz_url );
static char *XURL_FindHostname          ( char *psz_url );
static char *XURL_FindPath              ( char *psz_url );
static char *XURL_FindFragment          ( char *psz_url );


char *XURL_Join( char *psz_url1, char *psz_url2 )
{
    if( XURL_IsAbsolute( psz_url1 ) )
        return XURL_Concat( psz_url1, psz_url2 );
    else
        return XURL_Concat( psz_url2, psz_url1 );
}

/* TODO: replace XURL_Concat's rel/absolute calculation with the one
 * specified by RFC2396, and also test it on their test suite :) */


char *XURL_Concat( char *psz_url, char *psz_append )
{
    char *psz_return_value = NULL;

    if( XURL_IsAbsolute( psz_append ) )
        return strdup( psz_append );

    if( XURL_IsAbsolute( psz_url ) )
    {
        if( XURL_HasAbsolutePath( psz_append ) )
        {
            char *psz_concat_url;

            psz_concat_url = XURL_GetSchemeAndHostname( psz_url );

            psz_concat_url = streallocat( psz_concat_url, psz_append );
#ifdef XURL_DEBUG
            fprintf( stderr, "XURL_Concat: concat is \"%s\"\n",
                     psz_concat_url );
#endif
            psz_return_value = psz_concat_url;
        }
        else
        {
            /* psz_append is a relative URL */
            char *psz_new_url;
 
            /* strip off last path component */
            psz_new_url = XURL_GetHead( psz_url );
            psz_new_url = streallocat( psz_new_url, psz_append );

            psz_return_value = psz_new_url;
        }
    }
    else
    {
        /* not an absolute URL */
        if( XURL_HasAbsolutePath( psz_append ) == false )
        {
            char *psz_new_url = XURL_GetHead( psz_url );

            psz_new_url = streallocat( psz_new_url, psz_append );
            psz_return_value = psz_new_url;
        }
        else
        {
            /* URL to append has an absolute path -- just use that instead */
            psz_return_value = strdup( psz_append );
        }
    }

    return psz_return_value;
}


bool XURL_IsAbsolute( char *psz_url )
{
    if( XURL_FindHostname( psz_url ) == NULL )
    {
#ifdef XURL_DEBUG
        fprintf( stderr, "XURL_IsAbsolute(%s) returning false\n", psz_url );
#endif
        return false;
    }
    else
    {
#ifdef XURL_DEBUG
        fprintf( stderr, "XURL_IsAbsolute(%s) returning true\n", psz_url );
#endif
        return true;
    }
}


bool XURL_HasFragment( char *psz_url )
{
    if( XURL_FindFragment( psz_url ) == NULL )
        return false;
    else
        return true;
}


char *XURL_FindHostname( char *psz_url )
{
    char *psz_return_value = NULL;

    char *psz_scheme_separator = strstr( psz_url, "://" );
    if( psz_scheme_separator != NULL)
    {
        char *psz_hostname = psz_scheme_separator + strlen( "://" );
        if( *psz_hostname != '\0')
            psz_return_value = psz_hostname;

#ifdef XURL_DEBUG
        fprintf( stderr, "XURL_FindHostname(%s): returning \"%s\"\n",
                 psz_url, psz_return_value );
#endif
    }

    return psz_return_value;
}


bool XURL_HasAbsolutePath( char *psz_url )
{
#ifdef XURL_WIN32_PATHING
    if( psz_url[0] == '/' || psz_url[0] == '\\' )
#else
    if( psz_url[0] == '/' )
#endif
        return true;
    else
        return false;
}


char *XURL_GetHostname( char *psz_url )
{
    char *psz_return_value = NULL;
    char *psz_hostname = XURL_FindHostname( psz_url );

    if( psz_hostname != NULL )
    {
        char *psz_new_hostname;
        size_t i_hostname_length;

        char *psz_one_past_end_of_hostname = strchr( psz_hostname, '/' );
        if( psz_one_past_end_of_hostname != NULL)
        {
            /* Found a '/' after the hostname, so copy characters between
             * the hostname and the '/' to a new string */
            i_hostname_length = psz_one_past_end_of_hostname -
                psz_hostname;
        }
        else
        {
            /* Didn't find a '/', so copy from the start of the hostname
             * until the end of the string */
            i_hostname_length = strlen( psz_url ) - ( psz_hostname - psz_url );
        }

        /* Copy hostname to a new string */
        psz_new_hostname = malloc( i_hostname_length );
        if( psz_new_hostname == NULL )
            return NULL;
        strncpy( psz_new_hostname, psz_hostname, i_hostname_length );

#ifdef XURL_DEBUG
        fprintf (stderr, "XURL_GetHostname: psz_new_hostname is \"%s\"\n",
                 psz_new_hostname );
#endif
        psz_return_value = psz_new_hostname;
    }
    else
    {
        /* Didn't find a hostname */
        return NULL;
    }

    return psz_return_value;
}


char *XURL_GetSchemeAndHostname( char *psz_url )
{
    char *psz_scheme = NULL,
         *psz_hostname = NULL,
         *psz_scheme_and_hostname = NULL;

    psz_scheme = XURL_GetScheme( psz_url );
    psz_hostname = XURL_GetHostname( psz_url );
    if( psz_hostname && psz_scheme )
    {
        if( asprintf( &psz_scheme_and_hostname, "%s://%s", psz_scheme, psz_hostname ) == -1)
            psz_scheme_and_hostname = NULL;
    }

    free( psz_hostname );
    free( psz_scheme );
    return psz_scheme_and_hostname;
}

static char *XURL_FindFragment( char *psz_url )
{
    char *pc_hash = NULL;
    char *pc_return_value = NULL;
 
    pc_hash = strchr( psz_url, '#' );
    if( pc_hash != NULL )
        pc_return_value = pc_hash;

    return pc_return_value;
}

char *XURL_FindQuery( char *psz_url )
{
    char *pc_question_mark = NULL;
    char *pc_return_value = NULL;
 
    pc_question_mark = strchr( psz_url, '?' );
    if( pc_question_mark != NULL )
        pc_return_value = pc_question_mark;

    return pc_return_value;
}


char *XURL_GetScheme( char *psz_url )
{
    char *psz_colon;
    size_t i_scheme_length;
    char *new_scheme;

    if( XURL_IsAbsolute( psz_url ) == false )
        return strdup( "file" );

    /* this strchr will always succeed since we have an absolute URL, and thus
     * a scheme */
    psz_colon = strchr( psz_url, ':' );

    i_scheme_length = psz_colon - psz_url;

    new_scheme = malloc( i_scheme_length );
    if( new_scheme == NULL )
        return NULL;

    strncpy( new_scheme, psz_url, i_scheme_length );
    return new_scheme;
}


bool XURL_IsFileURL( char *psz_url )
{
    bool b_return_value;
    char *psz_scheme = XURL_GetScheme( psz_url );

    if( strcasecmp( psz_scheme, "file" ) == 0 )
        b_return_value = true;
    else
        b_return_value = false;

    free( psz_scheme );

    return b_return_value;
}


static char *XURL_FindPath( char *psz_url )
{
    if( XURL_IsAbsolute( psz_url ) )
    {
        char *psz_start_of_hostname = XURL_FindHostname( psz_url );
        if( psz_start_of_hostname != NULL )
            return strchr( psz_start_of_hostname, '/' );
        else
           return NULL;
    }
    else
    {
        if( XURL_HasAbsolutePath( psz_url ) == true )
            return psz_url;
        else
            return strdup (".");
    }
}


char *XURL_GetPath( char *psz_url )
{
    char *psz_return_value = NULL;
    char *psz_path = NULL;
    char *pc_question_mark = NULL;
    char *pc_fragment = NULL;

    psz_path = strdup( XURL_FindPath( psz_url ) );
#ifdef XURL_DEBUG
    fprintf( stderr, "XURL_GetPath: XURL_FindPath returning \"%s\"\n",
             psz_path );
#endif
    psz_return_value = psz_path;

    pc_question_mark = XURL_FindQuery( psz_path );
    if( pc_question_mark != NULL )
    {
        int i_path_length = pc_question_mark - psz_path;
        *( psz_path + i_path_length ) = '\0';
    }

    pc_fragment = XURL_FindFragment( psz_path );
    if( pc_fragment != NULL )
    {
#ifdef XURL_DEBUG
        fprintf( stderr, "XURL_GetPath: XURL_FindFragment returned \"%s\"\n",
                 pc_fragment );
#endif
        int i_path_length = pc_fragment - psz_path;
        *( psz_path + i_path_length ) = '\0';
    }

#ifdef XURL_DEBUG
    fprintf( stderr, "XURL_GetPath returning \"%s\"\n", psz_return_value );
#endif

    return psz_return_value;
}


char *XURL_GetHead( const char *psz_path )
{
    char *psz_path_head;
    char *pc_last_slash;

    /* kill everything up to the last / (including the /) */
#ifdef XURL_WIN32_PATHING
    /* Windows: Try looking for a \ first; if we don't find one, look for / */
    pc_last_slash = strrchr( psz_path, '\\' );
    if( pc_last_slash == NULL )
        pc_last_slash = strrchr( psz_path, '/' );
#else
    pc_last_slash = strrchr( psz_path, '/' );
#endif
    if( pc_last_slash == NULL )
    {
        psz_path_head = strdup( psz_path );
    }
    else
    {
        size_t i_characters_until_last_slash;

        i_characters_until_last_slash = pc_last_slash - psz_path;
        psz_path_head = malloc( i_characters_until_last_slash + 1 );
        strncpy( psz_path_head, psz_path, i_characters_until_last_slash + 1 );

        /* terminate the resulting string with '\0' */
        *(psz_path_head +
                i_characters_until_last_slash) = '\0';
    }

    /* append a trailing / */
    streallocat( psz_path_head, "/" );

    return psz_path_head;
}


char *XURL_GetWithoutFragment( char *psz_url )
{
    char *psz_return_value = NULL;
    char *psz_fragment;

    psz_fragment = XURL_FindFragment( psz_url );
    if( psz_fragment == NULL )
    {
        psz_return_value = strdup( psz_url );
    }
    else
    {
        size_t i_pre_fragment_length;
        char *psz_without_fragment;

        i_pre_fragment_length = psz_fragment - psz_url;

        psz_without_fragment = malloc( i_pre_fragment_length + 1 );
        if( psz_without_fragment == NULL )
        {
            psz_return_value = NULL;
        }
        else
        {
            memcpy( psz_without_fragment, psz_url, i_pre_fragment_length );
            *( psz_without_fragment + i_pre_fragment_length ) = '\0';
            psz_return_value = psz_without_fragment;
        }
    }
 
    return psz_return_value;
}

static char *streallocat( char *psz_string, const char *psz_to_append )
{
    size_t i_new_string_length = strlen( psz_string ) +
        strlen( psz_to_append ) + 1;

    psz_string = (char *) realloc( psz_string, i_new_string_length );
 
    return strcat( psz_string, psz_to_append );
}

