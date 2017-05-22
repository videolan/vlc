/*****************************************************************************
 * asx.c : ASX playlist format import
 *****************************************************************************
 * Copyright (C) 2005-2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/* See also:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/dd564668.aspx
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_xml.h>
#include <vlc_strings.h>
#include <vlc_charset.h>

#include <assert.h>
#include <ctype.h>

#include "playlist.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int ReadDir( stream_t *, input_item_node_t * );

static mtime_t ParseTime(xml_reader_t *p_xml_reader)
{
    char *psz_value = NULL;
    char *psz_start = NULL;

    const char *psz_node = NULL;
    const char *psz_txt = NULL;

    int i_subfractions = -1;

    int i_subresult = 0;
    mtime_t i_result = 0;

    do
    {
        psz_txt = xml_ReaderNextAttr( p_xml_reader, &psz_node );
    }
    while( psz_txt && strncasecmp( psz_txt, "VALUE", 5 ) );

    psz_value = strdup( psz_node );
    psz_start = psz_value;

    while( *psz_value )
    {
        if( isdigit( *psz_value ) )
        {
            i_subresult = i_subresult * 10;
            i_subresult += *psz_value - '0';
            if( i_subfractions != -1 )
                i_subfractions++;
        }
        else if( *psz_value == ':' )
        {
            i_result += i_subresult;
            i_result = i_result * 60;
            i_subresult = 0;
        }
        else if( *psz_value == '.' )
        {
            i_subfractions = 0;
            i_result += i_subresult;
            i_subresult = 0;
        }
        psz_value++;

    }
    if( i_subfractions == -1)
        i_result += i_subresult;

    /* Convert to microseconds */
    if( i_subfractions == -1)
        i_subfractions = 0;
    while( i_subfractions < 6 )
    {
        i_subresult = i_subresult * 10;
        i_subfractions++;
    }
    i_result = i_result * 1000000;
    if( i_subfractions != -1)
        i_result += i_subresult;

    free( psz_start );
    return i_result;
}

static void ReadElement( xml_reader_t *p_xml_reader, char **ppsz_txt )
{
    const char *psz_node = NULL;

    /* Read the text node */
    xml_ReaderNextNode( p_xml_reader, &psz_node );
    free( *ppsz_txt );
    *ppsz_txt = strdup( psz_node );
    vlc_xml_decode( *ppsz_txt );

    /* Read the end element */
    xml_ReaderNextNode( p_xml_reader, &psz_node );
    /* TODO :
     * Currently we don't check the agreement of start and end element
     * This function is only used to read the element that cannot have child
     * according to the reference.
     */
}

static bool PeekASX( stream_t *s )
{
    const uint8_t *p_peek;
    return ( vlc_stream_Peek( s->p_source, &p_peek, 12 ) == 12
             && !memcmp( p_peek, "<asx version", 12 ) );
}

/*****************************************************************************
 * Import_ASX: main import function
 *****************************************************************************/

int Import_ASX( vlc_object_t *p_this )
{
    stream_t *p_demux = (stream_t *)p_this;

    CHECK_FILE(p_demux);

    char *type = stream_MimeType( p_demux->p_source );

    if( stream_HasExtension( p_demux, ".asx" )
     || stream_HasExtension( p_demux, ".wax" )
     || stream_HasExtension( p_demux, ".wvx" )
     || (type != NULL && (strcasecmp(type, "video-x-ms-asf") == 0
                       || strcasecmp(type, "audio/x-ms-wax") == 0)
                      && PeekASX( p_demux ) ) )
    {
        msg_Dbg( p_demux, "found valid ASX playlist" );
        free(type);
    }
    else
    {
        free(type);
        return VLC_EGENERIC;
    }

    p_demux->pf_control = access_vaDirectoryControlHelper;
    p_demux->pf_readdir = ReadDir;
    return VLC_SUCCESS;
}

static void ProcessEntry( int *pi_n_entry, xml_reader_t *p_xml_reader,
                         input_item_node_t *p_subitems,
                         input_item_t *p_current_input, char *psz_prefix )
{
    const char *psz_node = NULL;
    const char *psz_txt = NULL;
    int i_type;

    char *psz_title = NULL;
    char *psz_artist = NULL;
    char *psz_copyright = NULL;
    char *psz_moreinfo = NULL;
    char *psz_description = NULL;
    char *psz_name = NULL;
    char *psz_mrl = NULL;
    char *psz_href = NULL;

    input_item_t *p_entry = NULL;

    int i_options;
    mtime_t i_start = 0;
    mtime_t i_duration = 0;
    char *ppsz_options[2];

    do
    {
        i_type = xml_ReaderNextNode( p_xml_reader, &psz_node );

        if( i_type == XML_READER_STARTELEM )
        {
            /* Metadata Node */
            if( !strncasecmp( psz_node, "TITLE", 5 ) )
                ReadElement( p_xml_reader, &psz_title );
            else if( !strncasecmp( psz_node, "AUTHOR", 6 ) )
                ReadElement( p_xml_reader, &psz_artist );
            else if( !strncasecmp( psz_node, "COPYRIGHT", 9 ) )
                ReadElement( p_xml_reader, &psz_copyright );
            else if( !strncasecmp( psz_node,"MOREINFO", 8 ) )
            {
                do
                {
                    psz_txt = xml_ReaderNextAttr( p_xml_reader, &psz_node );
                }
                while(psz_txt && strncasecmp( psz_txt, "HREF", 4 ) );

                if( !psz_txt )
                    ReadElement( p_xml_reader, &psz_moreinfo );
                else
                    psz_moreinfo = strdup( psz_node );
                vlc_xml_decode( psz_moreinfo );
            }
            else if( !strncasecmp( psz_node, "ABSTRACT", 8 ) )
                ReadElement( p_xml_reader, &psz_description );
            else if( !strncasecmp( psz_node, "DURATION", 8 ) )
                i_duration = ParseTime( p_xml_reader );
            else if( !strncasecmp( psz_node, "STARTTIME", 9 ) )
                i_start = ParseTime( p_xml_reader );
            else
            /* Reference Node */
            /* All ref node will be converted into an entry */
            if( !strncasecmp( psz_node, "REF", 3 ) )
            {
                *pi_n_entry = *pi_n_entry + 1;

                if( !psz_title )
                    psz_title = input_item_GetTitle( p_current_input );
                if( !psz_artist )
                    psz_artist = input_item_GetArtist( p_current_input );
                if( !psz_copyright )
                    psz_copyright = input_item_GetCopyright( p_current_input );
                if( !psz_description )
                    psz_description = input_item_GetDescription( p_current_input );

                do
                {
                    psz_txt = xml_ReaderNextAttr( p_xml_reader, &psz_node );
                }
                while( strncasecmp( psz_txt, "HREF", 4) );
                psz_href = strdup( psz_node );

                if( asprintf( &psz_name, "%d. %s", *pi_n_entry, psz_title ) == -1)
                    psz_name = strdup( psz_title );
                vlc_xml_decode( psz_href );
                psz_mrl = ProcessMRL( psz_href, psz_prefix );

                /* Add Time information */
                i_options = 0;
                if( i_start )
                {
                    if( asprintf( ppsz_options, ":start-time=%d" ,(int) i_start/1000000 ) != -1)
                        i_options++;
                }
                if( i_duration)
                {
                    if( asprintf( ppsz_options + i_options, ":stop-time=%d",
                                (int) (i_start+i_duration)/1000000 ) != -1)
                        i_options++;
                }

                /* Create the input item */
                p_entry = input_item_NewExt( psz_mrl, psz_name, i_duration,
                                             ITEM_TYPE_UNKNOWN, ITEM_NET_UNKNOWN );
                if( p_entry == NULL )
                    goto end;

                input_item_AddOptions( p_entry, i_options,
                                       (const char **)ppsz_options,
                                       VLC_INPUT_OPTION_TRUSTED );
                input_item_CopyOptions( p_entry, p_current_input );

                /* Add the metadata */
                if( psz_name )
                    input_item_SetTitle( p_entry, psz_name );
                if( psz_artist )
                    input_item_SetArtist( p_entry, psz_artist );
                if( psz_copyright )
                    input_item_SetCopyright( p_entry, psz_copyright );
                if( psz_moreinfo )
                    input_item_SetURL( p_entry, psz_moreinfo );
                if( psz_description )
                    input_item_SetDescription( p_entry, psz_description );
                if( i_duration > 0 )
                    p_entry->i_duration = i_duration;

                input_item_node_AppendItem( p_subitems, p_entry );

                input_item_Release( p_entry );

end:
                while( i_options )
                    free( ppsz_options[--i_options] );
                free( psz_name );
                free( psz_mrl );
            }
        }
    }
    while( i_type != XML_READER_ENDELEM || strncasecmp( psz_node, "ENTRY", 5 ) );

    free( psz_href );
    free( psz_title );
    free( psz_artist );
    free( psz_copyright );
    free( psz_moreinfo );
    free( psz_description );
}

static stream_t* UTF8Stream( stream_t *p_demux )
{
    stream_t *s = p_demux->p_source;
    uint64_t streamSize;

     if (vlc_stream_GetSize( s, &streamSize ) != VLC_SUCCESS)
         return NULL;
     // Don't attempt to convert/store huge streams
     if( streamSize > 1024 * 1024 )
         return NULL;
     char* psz_source = malloc( streamSize + 1 * sizeof( *psz_source ) );
     if ( unlikely( psz_source == NULL ) )
         return NULL;
     size_t i_read = 0;
     do
     {
         ssize_t i_ret = vlc_stream_Read( s, psz_source + i_read,
                                          streamSize > 1024 ? 1024 : streamSize );
         if ( i_ret <= 0 )
             break;
         assert( (size_t)i_ret <= streamSize );
         streamSize -= i_ret;
         i_read += i_ret;
     } while ( streamSize > 0 );
     psz_source[i_read] = 0;
     if ( IsUTF8( psz_source ) )
         return vlc_stream_MemoryNew( p_demux, (uint8_t*)psz_source, i_read, false );

     char *psz_utf8 = FromLatin1( psz_source );
     if( psz_utf8 == NULL )
     {
         free( psz_source );
         return NULL;
     }
     stream_t * p_stream = vlc_stream_MemoryNew( p_demux, (uint8_t*)psz_utf8, strlen(psz_utf8), false );
     free( psz_source );
     return p_stream;
}

static int ReadDir( stream_t *p_demux, input_item_node_t *p_subitems )
{
    if (unlikely(p_demux->psz_url == NULL))
        return VLC_EGENERIC;

    const char *psz_node = NULL;
    char *psz_txt = NULL;
    char *psz_base = strdup( p_demux->psz_url );
    if (unlikely(psz_base == NULL))
        return VLC_ENOMEM;

    char *psz_title_asx = NULL;
    char *psz_entryref = NULL;

    xml_reader_t *p_xml_reader = NULL;
    input_item_t *p_current_input = GetCurrentItem( p_demux );
    stream_t* p_stream = UTF8Stream( p_demux );

    bool b_first_node = false;
    int i_type;
    int i_n_entry = 0;

    p_xml_reader = xml_ReaderCreate( p_demux, p_stream ? p_stream
                                                       : p_demux->p_source );
    if( !p_xml_reader )
    {
        msg_Err( p_demux, "Cannot parse ASX input file as XML");
        goto error;
    }

    do
    {
        i_type = xml_ReaderNextNode( p_xml_reader, &psz_node );
        if( i_type == XML_READER_ERROR )
            break;

        if( i_type == XML_READER_STARTELEM )
        {
            if( !b_first_node )
            {
                if(!strncasecmp( psz_node, "ASX", 3 ) )
                    b_first_node = true;
                else
                {
                    msg_Err( p_demux, "invalid root node" );
                    goto error;
                }
            }

            /* Metadata Node Handler */
            if( !strncasecmp( psz_node, "TITLE", 5 ) )
            {
                ReadElement( p_xml_reader, &psz_title_asx );
                input_item_SetTitle( p_current_input, psz_title_asx );
            }
            else if( !strncasecmp( psz_node, "AUTHOR", 6 ) )
            {
                ReadElement( p_xml_reader, &psz_txt );
                input_item_SetArtist( p_current_input, psz_txt );
            }
            else if( !strncasecmp( psz_node, "COPYRIGHT", 9 ) )
            {
                ReadElement( p_xml_reader, &psz_txt );
                input_item_SetCopyright( p_current_input, psz_txt );
            }
            else if( !strncasecmp( psz_node, "MOREINFO", 8 ) )
            {
                const char *psz_tmp;
                do
                {
                    psz_tmp = xml_ReaderNextAttr( p_xml_reader, &psz_node );
                }
                while( psz_tmp && strncasecmp( psz_tmp, "HREF", 4 ) );

                if( !psz_tmp )  // If HREF attribute doesn't exist
                    ReadElement( p_xml_reader, &psz_txt );
                else
                    psz_txt = strdup( psz_node );

                vlc_xml_decode( psz_txt );
                input_item_SetURL( p_current_input, psz_txt );
            }
            else if( !strncasecmp( psz_node, "ABSTRACT", 8 ) )
            {
                ReadElement( p_xml_reader, &psz_txt );
                input_item_SetDescription( p_current_input, psz_txt );
            }
            else
            /* Base Node handler */
            if( !strncasecmp( psz_node, "BASE", 4 ) )
                ReadElement( p_xml_reader, &psz_base );
            else
            /* Entry Ref Handler */
            if( !strncasecmp( psz_node, "ENTRYREF", 7 ) )
            {
                const char *psz_tmp;
                do
                {
                    psz_tmp = xml_ReaderNextAttr( p_xml_reader, &psz_node );
                }
                while( psz_tmp && !strncasecmp( psz_tmp, "HREF", 4 ) );

                /* Create new input item */
                input_item_t *p_input;
                psz_txt = strdup( psz_node );
                vlc_xml_decode( psz_txt );
                p_input = input_item_New( psz_txt, psz_title_asx );
                input_item_CopyOptions( p_input, p_current_input );
                input_item_node_AppendItem( p_subitems, p_input );

                input_item_Release( p_input );
            }
            else
            /* Entry Handler */
            if( !strncasecmp( psz_node, "ENTRY", 5 ) )
            {
                ProcessEntry( &i_n_entry, p_xml_reader, p_subitems,
                              p_current_input, psz_base);
            }
        /* FIXME Unsupported elements
            PARAM
            EVENT
            REPEAT
            ENDMARK
            STARTMARK
        */
        }
    }
    while( i_type != XML_READER_ENDELEM || strncasecmp( psz_node, "ASX", 3 ) );

error:
    free( psz_base );
    free( psz_title_asx );
    free( psz_entryref );
    free( psz_txt );

    if( p_xml_reader)
        xml_ReaderDelete( p_xml_reader );
    if( p_stream )
        vlc_stream_Delete( p_stream );

    return 0;
}
