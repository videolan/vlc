/*****************************************************************************
 * googleimage.c
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc_input.h>
#include <vlc_playlist.h>
#include <vlc_meta.h>
#include <vlc_stream.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int FindArt( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    set_shortname( N_( "GoogleImage" ) );
    set_description( _("Fetch Artwork from Google Images") );

    /* Go to image.google.com and download the first image closest to the
     * title */
    set_capability( "art finder", 30 );
    set_callbacks( FindArt, NULL );
vlc_module_end();

/*****************************************************************************
 *****************************************************************************/

static const char * kpsz_google_image_server = "images.google.com";
static const char * kpsz_first_image_token = "imgurl=";

static int FindArt( vlc_object_t *p_this )
{
    playlist_t *p_playlist = (playlist_t *)p_this;
    char *request, *fname, *psz;
    stream_t *p_stream;
	unsigned int count;
	unsigned int i;
    input_item_t *p_item = (input_item_t *)(p_playlist->p_private);
    char psz_buffer[65536]; /* XXX: We might want to lower that */
 
    assert( p_item );

    if( !p_item->p_meta || !p_item->p_meta->psz_title )
    {
		if( !p_item->psz_name )
		{
       	 	msg_Dbg( p_this, "item has no name" );
        	return VLC_EGENERIC;
		}
		fname = strdup( p_item->psz_name );
    }
    else
    {
        fname = strdup( p_item->p_meta->psz_title );
    }

	/* If there is a .xxx remove it, we get bad matches with it */
	count = strlen( fname );
	if( count > 4 && fname[count-4] == '.' )
		fname[count-4] = 0;

    /* Replace some special char by + */
    for( i = 0; i < strlen( fname ); i++)
    {
        if( fname[i] == ' ' || fname[i] == '-' || fname[i] == '.' )
            fname[i] = '+';
    }

    asprintf( &request, "http://%s/images?q=%s&ie=UTF-8", kpsz_google_image_server, fname );
	free( fname );

    msg_Dbg( p_this, "going to request %s", request );
    
    /* Sending the request */
    p_stream = stream_UrlNew( p_playlist, request );
	free( request );
    if( !p_stream )
    {
		msg_Err( p_this, "Can't connect" );
		return VLC_EGENERIC;
	}

	/* The access will return the whole page at once, we just abuse from that */
	count = stream_Read( p_stream , (uint8_t*)psz_buffer, sizeof(psz_buffer)-1 );
	if( count <= 0 )
	{
		msg_Warn( p_this, "Can't read the page" );
		stream_Delete( p_stream );			
		return VLC_EGENERIC;
	}

	psz_buffer[count] = 0;

	/* Find the first occurences of kpsz_first_image_token */
	if( !(psz = strstr( psz_buffer, kpsz_first_image_token )) )
	{
		msg_Warn( p_this, "No image found" );
		stream_Delete( p_stream );			
		return VLC_EGENERIC;
	}

	/* Found */

	psz += 7;

	/* Now get the end of the link */
	fname = psz;
	psz = strchr( fname, '&' );
	if( !psz )
	{
		msg_Dbg( p_this, "Invalid page (or chunk) (!)" );
		stream_Delete( p_stream );
		return VLC_EGENERIC;
	}
	*psz = 0;
	
	/* fname points to the url */

	stream_Delete( p_stream );

	/* We have the link */

	msg_Dbg( p_this, "Image is at '%s'\n", fname );
	vlc_meta_SetArtURL( p_item->p_meta, fname );
	return VLC_SUCCESS;
}

