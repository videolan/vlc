/*****************************************************************************
 * input_info.c: Convenient functions to handle the input info structures
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: input_info.c,v 1.9 2002/11/11 14:39:12 sam Exp $
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
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
#include <string.h>

#include <vlc/vlc.h>

#include "stream_control.h"
#include "input_ext-intf.h"
#include "interface.h"

/**
 * \brief Find info category by name.
 *
 * Returns a pointer to the info category with the given name, and
 * creates it if necessary
 *
 * \param p_input pointer to the input thread in which the info is to be found
 * \param psz_name the name of the category to be found
 * \returns a pointer to the category with the given name
 */
input_info_category_t * input_InfoCategory( input_thread_t * p_input,
                                            char * psz_name)
{
    input_info_category_t * p_category, * p_prev;
    p_prev = NULL;
    for ( p_category = p_input->stream.p_info;
          (p_category != NULL) && strcmp( p_category->psz_name, psz_name ); 
          p_category = p_category->p_next)
    {
        p_prev = p_category;
    }
    if ( p_category )
    {
        return p_category;
    }
    else
    {
        p_category = malloc( sizeof( input_info_category_t ) );
        if ( !p_category )
        {
            msg_Err( p_input, "No mem" );
            return NULL;
        }
        p_category->psz_name = strdup( psz_name );
        p_category->p_next = NULL;
        p_category->p_info = NULL;
        p_prev->p_next = p_category;
        return p_category;
    }
}

/**
 * \brief Add a info item to a category
 *
 * \param p_category Pointer to the category to put this info in
 * \param psz_name Name of the info item to add
 * \param psz_format printf style format string for the value.
 * \return a negative number on error. 0 on success.
 */
int input_AddInfo( input_info_category_t * p_category, char * psz_name,
                   char * psz_format, ...)
{
    input_info_t * p_info, * p_prev;
    char * psz_str = NULL;
    va_list args;

    p_prev = NULL;
    if ( !p_category )
    {
        return -1;
    }
    
    va_start( args, psz_format );
    
    /*
     * Convert message to string
     */
#if defined(HAVE_VASPRINTF) && !defined(SYS_DARWIN)
    vasprintf( &psz_str, psz_format, args );
#else
    psz_str = (char*) malloc( strlen(psz_format) + INTF_MAX_MSG_SIZE );
    if( psz_str == NULL )
    {
        return -1;
    }

    vsprintf( psz_str, psz_format, args );
#endif

    va_end( args );
    p_info = p_category->p_info;
    while ( p_info )
    {
        p_prev = p_info;
        p_info = p_info->p_next;
    }
    p_info = malloc( sizeof( input_info_t ) );
    if( !p_info )
    {
        return -1;
    }
    p_info->psz_name = strdup( psz_name );
    p_info->psz_value = psz_str;
    p_info->p_next = NULL;
    if ( p_prev )
    {
        p_prev->p_next = p_info;
    }
    else
    {
        p_category->p_info = p_info;
    }
    return 0;
}

/**
 * \brief Destroy info structures
 * \internal
 *
 * \param p_input The input thread to be cleaned for info
 * \returns for the moment 0
 */
int input_DelInfo( input_thread_t * p_input )
{
    input_info_category_t * p_category, * p_prev_category;
    input_info_t * p_info, * p_prev_info;
    
    p_category = p_input->stream.p_info;
    while ( p_category )
    {
        p_info = p_category->p_info;
        while ( p_info )
        {
            if ( p_info->psz_name )
            {
                free( p_info->psz_name );
            }
            if ( p_info->psz_value )
            {
                free( p_info->psz_value );
            }
            p_prev_info = p_info;
            p_info = p_info->p_next;
            free( p_prev_info );
        }
        if ( p_category->psz_name )
        {
            free( p_category->psz_name );
        }
        p_prev_category = p_category;
        p_category = p_category->p_next;
        free( p_prev_category );
    }
    return 0;
}
