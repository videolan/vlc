/*****************************************************************************
 * native.c :  Native playlist export module
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include <errno.h>                                                 /* ENOMEM */

#include <libxml/xmlwriter.h>
#include <libxml/encoding.h>
#define ENCODING "UTF-8"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int Export_Native ( vlc_object_t * );
char *ToUTF8( char *);

/*****************************************************************************
 * Native: main export function
 *****************************************************************************/
int Export_Native( vlc_object_t *p_this )
{
    playlist_t *p_playlist = (playlist_t*)p_this;
    playlist_export_t *p_export = (playlist_export_t *)p_playlist->p_private;

    int i,j,k;

    char *psz_name="VLC Playlist";
    int i_version = 1;

    int i_ret;
    xmlTextWriterPtr p_writer;
    xmlBufferPtr p_buf;
    xmlChar *tmp;

    msg_Dbg(p_playlist, "Saving using native format");

    /* Create a new XmlWriter */
    p_buf = xmlBufferCreate();
    if ( !p_buf )
    {
        msg_Warn( p_playlist, "Unable to create XML buffer" );
        return VLC_EGENERIC;
    }

    /* Create a new XmlWriter */
    p_writer = xmlNewTextWriterMemory( p_buf, 0);
    if ( !p_writer)
    {
        msg_Dbg( p_playlist, "Unable to create XML writer" );
        return VLC_EGENERIC;
    }

    /* Make a beautiful output */
    i_ret = xmlTextWriterSetIndent( p_writer, 1 );

    /* Start the document */
    i_ret = xmlTextWriterStartDocument(p_writer, NULL, ENCODING, NULL);
    if (i_ret < 0)
    {
        return VLC_EGENERIC;
    }

    i_ret = xmlTextWriterStartElement( p_writer, BAD_CAST "playlist");
    if (i_ret < 0)
    {
        msg_Dbg( p_playlist, "Unable to write root node");
        return VLC_EGENERIC;
    }

    for( i = 0; i< p_playlist->i_size ; i++)
    {
        playlist_item_t *p_item= p_playlist->pp_items[i];
        char *psz_utf8 = NULL;
        i_ret = xmlTextWriterStartElement( p_writer, BAD_CAST "item");

        /* Write item attributes */
        psz_utf8 = ToUTF8( p_item->input.psz_uri );
        if( !psz_utf8 ) return VLC_EGENERIC;
        i_ret = xmlTextWriterWriteAttribute( p_writer, "uri",
                             psz_utf8 );
        free( psz_utf8 );

        psz_utf8 = ToUTF8( p_item->input.psz_name );
        if( !psz_utf8 ) return VLC_EGENERIC;
        i_ret = xmlTextWriterWriteAttribute( p_writer, "name",
                             psz_utf8 );
        free( psz_utf8 );

        /* Write categories */
        for( j = 0; j< p_item->input.i_categories ; j++ )
        {
            info_category_t *p_cat = p_item->input.pp_categories[j];
            if( p_cat->i_infos > 0 )
            {
                i_ret = xmlTextWriterStartElement( p_writer,
                                                    "category" );

                psz_utf8 = ToUTF8( p_cat->psz_name );
                if( !psz_utf8 ) return VLC_EGENERIC;
                i_ret = xmlTextWriterWriteAttribute( p_writer, "name",
                                                     psz_utf8 );
                free( psz_utf8 );

                for( k = 0; k< p_cat->i_infos; k++)
                {
                    i_ret = xmlTextWriterStartElement( p_writer,"info" );

                    psz_utf8 = ToUTF8( p_cat->pp_infos[k]->psz_name);
                    if( !psz_utf8 ) return VLC_EGENERIC;
                    i_ret = xmlTextWriterWriteAttribute( p_writer,"name",
                                            psz_utf8 );
                    free( psz_utf8 );

                    psz_utf8 = ToUTF8( p_cat->pp_infos[k]->psz_value);
                    if( !psz_utf8 ) return VLC_EGENERIC;
                    i_ret = xmlTextWriterWriteAttribute( p_writer,"value",
                                            psz_utf8 );
                    free( psz_utf8 );

                    i_ret = xmlTextWriterEndElement( p_writer );
                }
                /* Finish category */
                i_ret = xmlTextWriterEndElement( p_writer);
            }
        }
        for( j = 0; j< p_item->input.i_options ; j++ )
        {
            i_ret = xmlTextWriterStartElement( p_writer,
                                               "option" );
            psz_utf8 = ToUTF8( p_item->input.ppsz_options[j] );
            if( !psz_utf8 ) return VLC_EGENERIC;
            i_ret = xmlTextWriterWriteAttribute( p_writer, "name",
                                                 psz_utf8 );
        }
        /* Finish item */
        i_ret = xmlTextWriterEndElement( p_writer );
    }
    /* Finish playlist */
    i_ret = xmlTextWriterEndElement( p_writer );

    i_ret = xmlTextWriterEndDocument(p_writer);
    xmlFreeTextWriter(p_writer);

    fprintf( p_export->p_file, "%s", (const char *) p_buf->content);
#if 0
    /* Write items */
    for( i = 0; i< p_playlist->i_size ; i++)
    {
        playlist_item_t *p_item= p_playlist->pp_items[i];
        ItemStripEntities( p_item );
        fprintf( p_export->p_file," <item uri=\"%s\" name=\"%s\"",
                        p_item->psz_uri,
                        p_item->psz_name );
        if( p_item->i_duration != -1 )
        {
            fprintf( p_export->p_file, "duration=\""I64Fi"\""
                     p_item->i_duration )
        }
        if( p_item->b_enabled != 1 )
        {
            fprintf( p_export->p_file," enabled=\"%i\"", p_item->b_enabled );
        }
        if( p_item->i_group != 1 )
        {
            fprintf( p_export->p_file,"group=\"%i\"", p_item->i_group );
        }
        if( p_item->i_played > 0 )
        {
            fprintf( p_export->p_file," played=\"%i\"",p_item->i_nb_played );
        }
        fprintf( p_export->p_file, ">\n" );

        for( j = 0; j< p_item->i_categories ; j++ )
        {
            item_info_category_t *p_cat = p_item->pp_categories[j];
            if( p_cat->i_infos > 0 )
            {
                fprintf( p_export->p_file,"  <category name=\"%s\">\n",
                                          p_cat->psz_name);
                for( k = 0; k< p_cat->i_infos; k++)
                {
                    fprintf( p_export->p_file,
                                   "   <info name=\"%s\" value=\"%s\" />\n",
                                   p_cat->pp_infos[k]->psz_name,
                                   p_cat->pp_infos[k]->psz_value );
                }
                fprintf( p_export->p_file,"  </category>\n" );
            }
        }
        for( j = 0; j< p_item->i_options ; j++ )
        {
            fprintf( p_export->p_file,"  <option name=\"%s\">\n",
                               p_item->ppsz_options[j]);
        }
        fprintf( p_export->p_file, " </item>\n");
    }

    /* Write groups */
    for( i = 0; i< p_playlist->i_groups ; i++)
    {
        fprintf( p_export->p_file," <group name=\"%s\" id=\"%i\" />\n",
                        p_playlist->pp_groups[i]->psz_name,
                        p_playlist->pp_groups[i]->i_id );
    }

    /* Write footer */
    fprintf( p_export->p_file,"</playlist>\n" );
#endif
    return VLC_SUCCESS;
}

char *ToUTF8( char *psz_in )
{
    int i_in      = strlen( psz_in );
    int i_out     = 2*i_in + 1;
    char *psz_out = (char *)malloc( sizeof(char) * i_out );

    isolat1ToUTF8( psz_out, &i_out, psz_in, &i_in );
    psz_out[ i_out ] = 0;
    return psz_out;
}

