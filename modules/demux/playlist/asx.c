/*****************************************************************************
 * asx.c : ASX playlist format import
 *****************************************************************************
 * Copyright (C) 2005-2013 VLC authors and VideoLAN
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
#include <vlc_memstream.h>

#include <assert.h>
#include <ctype.h>
#include <string.h>

#include "playlist.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int ReadDir( stream_t *, input_item_node_t * );

static bool ParseTime(xml_reader_t *p_xml_reader, vlc_tick_t* pi_result )
{
    assert( pi_result );
    char *psz_value = NULL;
    char *psz_start = NULL;

    const char *psz_node = NULL;
    const char *psz_txt = NULL;

    int i_subfractions = -1;

    int i_subresult = 0;
    vlc_tick_t i_result = 0;

    do
    {
        psz_txt = xml_ReaderNextAttr( p_xml_reader, &psz_node );
    }
    while( psz_txt && strncasecmp( psz_txt, "VALUE", 5 ) );

    if( !psz_txt )
        return false;

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
    i_result = i_result * CLOCK_FREQ;
    if( i_subfractions != -1)
        i_result += VLC_TICK_FROM_US( i_subresult );

    free( psz_start );
    *pi_result = i_result;
    return true;
}

static bool ReadElement( xml_reader_t *p_xml_reader, char **ppsz_txt )
{
    const char *psz_node = NULL;

    /* Read the text node */
    int ret = xml_ReaderNextNode( p_xml_reader, &psz_node );
    if( ret <= 0 )
        return false;
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
    return true;
}

static bool PeekASX( stream_t *s )
{
    const uint8_t *p_peek;
    return ( vlc_stream_Peek( s->s, &p_peek, 12 ) == 12
             && !strncasecmp( (const char*) p_peek, "<asx version", 12 ) );
}

/*****************************************************************************
 * Import_ASX: main import function
 *****************************************************************************/

int Import_ASX( vlc_object_t *p_this )
{
    stream_t *p_demux = (stream_t *)p_this;

    CHECK_FILE(p_demux);

    char *type = stream_MimeType( p_demux->s );

    if( stream_HasExtension( p_demux, ".asx" )
     || stream_HasExtension( p_demux, ".wax" )
     || stream_HasExtension( p_demux, ".wvx" )
     || (type != NULL && (strcasecmp(type, "video/x-ms-asf") == 0
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
    vlc_tick_t i_start = 0;
    vlc_tick_t i_duration;
    char *ppsz_options[2];

    do
    {
        i_duration = INPUT_DURATION_UNSET;
        i_type = xml_ReaderNextNode( p_xml_reader, &psz_node );

        if( i_type == XML_READER_ERROR || i_type == XML_READER_NONE )
            break;

        if( i_type == XML_READER_STARTELEM )
        {
            /* Metadata Node */
            if( !strncasecmp( psz_node, "TITLE", 5 ) )
            {
                if( !ReadElement( p_xml_reader, &psz_title ) )
                    break;
            }
            else if( !strncasecmp( psz_node, "AUTHOR", 6 ) )
            {
                if( !ReadElement( p_xml_reader, &psz_artist ) )
                    break;
            }
            else if( !strncasecmp( psz_node, "COPYRIGHT", 9 ) )
            {
                if( !ReadElement( p_xml_reader, &psz_copyright ) )
                    break;
            }
            else if( !strncasecmp( psz_node,"MOREINFO", 8 ) )
            {
                do
                {
                    psz_txt = xml_ReaderNextAttr( p_xml_reader, &psz_node );
                }
                while(psz_txt && strncasecmp( psz_txt, "HREF", 4 ) );

                if( !psz_txt )
                {
                    if( !ReadElement( p_xml_reader, &psz_moreinfo ) )
                        break;
                }
                else
                    psz_moreinfo = strdup( psz_node );
                vlc_xml_decode( psz_moreinfo );
            }
            else if( !strncasecmp( psz_node, "ABSTRACT", 8 ) )
            {
                if( !ReadElement( p_xml_reader, &psz_description ) )
                    break;
            }
            else if( !strncasecmp( psz_node, "DURATION", 8 ) )
            {
                if( !ParseTime( p_xml_reader, &i_duration ) )
                   break;
            }
            else if( !strncasecmp( psz_node, "STARTTIME", 9 ) )
            {
                 if( !ParseTime( p_xml_reader, &i_start ) )
                     break;
            }
            /* Reference Node */
            /* All ref node will be converted into an entry */
            else if( !strncasecmp( psz_node, "REF", 3 ) )
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
                while( psz_txt != NULL && strncasecmp( psz_txt, "HREF", 4) );
                if( psz_txt == NULL )
                    break;
                psz_href = strdup( psz_node );

                if( asprintf( &psz_name, "%d. %s", *pi_n_entry, psz_title ) == -1)
                    psz_name = strdup( psz_title );
                vlc_xml_decode( psz_href );
                psz_mrl = ProcessMRL( psz_href, psz_prefix );

                /* Add Time information */
                i_options = 0;
                if( i_start )
                {
                    if( asprintf( ppsz_options, ":start-time=%"PRId64 ,
                                  SEC_FROM_VLC_TICK(i_start) ) != -1)
                        i_options++;
                }
                if( i_duration)
                {
                    if( asprintf( ppsz_options + i_options,
                                  ":stop-time=%"PRId64,
                                  SEC_FROM_VLC_TICK(i_start + i_duration) ) != -1)
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

/// this looks for patterns like &name; &#DEC; or &#xHEX;
static bool isXmlEncoded(const char* psz_str)
{
    assert( psz_str != NULL );
    //look for special characters
    if( strpbrk(psz_str, "<>'\"") != NULL )
        return false;

    bool is_escaped = false;
    while( true )
    {
        const char* psz_amp = strchr(psz_str, '&');
        if( psz_amp == NULL )
            break;
        const char* psz_end = strchr(psz_amp, ';');
        if(  psz_end == NULL )
            return false;

        else if(psz_amp[1] == '#')
        {
            if( psz_amp[2] == 'x' )
            {
                const char* psz_ptr = &psz_amp[3];
                if( psz_ptr  ==  psz_end )
                    return false;
                for (  ; psz_ptr < psz_end; psz_ptr++)
                    if( ! isxdigit( *psz_ptr ) )
                        return false;
            }
            else
            {
                const char* psz_ptr = &(psz_amp[2]);
                if( psz_ptr ==  psz_end )
                    return false;
                for (  ; psz_ptr < psz_end; psz_ptr++)
                    if( ! isdigit( *psz_ptr ) )
                        return false;
            }
        }
        else
        {
            const char* psz_ptr = &(psz_amp[1]);
            if( psz_ptr ==  psz_end )
                return false;
            for (  ; psz_ptr < psz_end; psz_ptr++)
                if( ! isalnum( *psz_ptr ) )
                    return false;
        }
        is_escaped = true;
        psz_str = psz_end;
    }
    return is_escaped;
}

static void memstream_puts_xmlencoded(struct vlc_memstream* p_stream, const char* psz_begin, const char* psz_end)
{
    char *psz_tmp = NULL;
    if(psz_end == NULL)
        psz_tmp = strdup( psz_begin );
    else
        psz_tmp = strndup( psz_begin, psz_end - psz_begin );

    if ( psz_tmp == NULL )
        return;

    if( isXmlEncoded( psz_tmp ) )
        vlc_memstream_puts( p_stream, psz_tmp );
    else
    {
        char *psz_tmp_encoded = vlc_xml_encode( psz_tmp );
        if ( !psz_tmp_encoded )
        {
            free( psz_tmp );
            return;
        }
        vlc_memstream_puts( p_stream, psz_tmp_encoded );
        free( psz_tmp_encoded );
    }
    free(psz_tmp);
}

/**
 * ASX doesn't requires to be a strict XML document, this function will
 *  - make tags and attributes upercase
 *  - escape strings when required
 */
static char* ASXToXML( char* psz_source )
{
    bool b_in_string= false;
    char *psz_source_cur = psz_source;
    char *psz_source_old = psz_source;
    char c_string_delim;

    struct vlc_memstream stream_out;
    if( vlc_memstream_open( &stream_out ) != 0 )
        return NULL;

    while ( psz_source_cur != NULL && *psz_source_cur != '\0' )
    {
        psz_source_old = psz_source_cur;
        //search tag start
        if( ( psz_source_cur = strchr( psz_source_cur, '<' ) ) == NULL )
        {
            memstream_puts_xmlencoded(&stream_out, psz_source_old, NULL);
            //vlc_memstream_puts( &stream_out, psz_source_old );
            break;
        }

        memstream_puts_xmlencoded(&stream_out, psz_source_old, psz_source_cur);
        psz_source_old = psz_source_cur;

        //skip if comment, no need to copy them to the ouput.
        if( strncmp( psz_source_cur, "<!--", 4 ) == 0 )
        {
            psz_source_cur += 4;
            psz_source_cur =  strstr( psz_source_cur, "-->" );
            if( psz_source_cur == NULL)
                break;
            else
            {
                psz_source_cur += 3;
                continue;
            }
        }
        else
        {
            vlc_memstream_putc( &stream_out, '<' );
            psz_source_cur++;
        }

        for (  ; *psz_source_cur != '\0'; psz_source_cur++ )
        {
            if( b_in_string == false )
            {
                if( *psz_source_cur == '>')
                {
                    vlc_memstream_putc( &stream_out, '>' );
                    psz_source_cur++;
                    break;
                }
                if( *psz_source_cur == '"' || *psz_source_cur == '\'' )
                {
                    c_string_delim = *psz_source_cur;
                    b_in_string = true;
                    vlc_memstream_putc( &stream_out, c_string_delim );
                }
                else
                {
                    //convert tag and attributes to upper case
                    vlc_memstream_putc( &stream_out, vlc_ascii_toupper( *psz_source_cur ) );
                }
            }
            else
            {
                psz_source_old = psz_source_cur;
                psz_source_cur = strchr( psz_source_cur, c_string_delim );
                if( psz_source_cur == NULL )
                    break;

                memstream_puts_xmlencoded(&stream_out, psz_source_old, psz_source_cur);
                vlc_memstream_putc( &stream_out, c_string_delim );
                b_in_string = false;
            }
        }
    }
    if( vlc_memstream_close( &stream_out ) != 0 )
        return NULL;

    return stream_out.ptr;
}

static char *detectXmlEncoding( const char *psz_xml )
{
    const char *psz_keyword_begin = NULL;
    const char *psz_keyword_end = NULL;

    const char *psz_value_begin = NULL;
    const char *psz_value_end = NULL;

    psz_xml += strspn( psz_xml, " \n\r\t" );
    if( strncasecmp( psz_xml, "<?xml", 5 ) != 0 )
        return NULL;
    psz_xml += 5;

    const char *psz_end = strstr( psz_xml, "?>" );
    if( psz_end == NULL )
        return NULL;

    while( psz_xml < psz_end )
    {
        psz_keyword_begin = psz_xml = psz_xml + strspn( psz_xml, " \n\r\t" );
        if( *psz_xml == '\0' )
            return NULL;
        psz_keyword_end = psz_xml = psz_xml + strcspn( psz_xml, " \n\r\t=" );
        if( *psz_xml == '\0' )
            return NULL;

        psz_xml += strspn( psz_xml, " \n\r\t" );
        if( *psz_xml != '=' )
            return NULL;
        psz_xml++;

        psz_xml += strspn( psz_xml, " \n\r\t" );
        char quote = *psz_xml;
        if( quote != '"' && quote != '\'' )
            return NULL;

        psz_value_begin = ++psz_xml;
        psz_value_end = psz_xml = strchr( psz_xml, quote );
        if( psz_xml == NULL )
            return NULL;
        psz_xml++;

        if( strncasecmp( psz_keyword_begin, "encoding", psz_keyword_end -  psz_keyword_begin ) == 0
             && ( psz_value_end -psz_value_begin) > 0 )
        {
            return strndup(psz_value_begin, psz_value_end -psz_value_begin);
        }
    }

    return NULL;
}


static stream_t* PreparseStream( stream_t *p_demux )
{
    stream_t *s = p_demux->s;
    uint64_t streamSize;
    static const size_t maxsize = 1024 * 1024;

    if( vlc_stream_GetSize( s, &streamSize ) != VLC_SUCCESS)
        streamSize = maxsize;

     // Don't attempt to convert/store huge streams
     if( streamSize > maxsize )
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


     char *encoding = detectXmlEncoding( psz_source );
     if( encoding != NULL )
     {
         if( strcasecmp( encoding, "UTF-8" ) == 0 )
            free( encoding );
         else
         {
            //strip xml prologue to avoid double conversion
            char *tmp = strstr( psz_source, "?>" ) + 2;
            tmp = FromCharset( encoding, tmp, strlen( tmp ) );
            free( psz_source );
            free( encoding );
            if ( !tmp )
                return NULL;
            psz_source = tmp;
         }
     }
     else if( !IsUTF8( psz_source ) )
     {
         char *tmp = FromLocaleDup( psz_source );
         free( psz_source );
         if( !tmp )
             return NULL;
         psz_source = tmp;
     }

    char *psz_source_xml = ASXToXML( psz_source );
    free( psz_source );
    if( psz_source_xml == NULL )
         return NULL;

     stream_t * p_stream = vlc_stream_MemoryNew( p_demux, (uint8_t*)psz_source_xml, strlen(psz_source_xml), false );
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
    stream_t* p_stream = PreparseStream( p_demux );

    bool b_first_node = false;
    int i_type;
    int i_n_entry = 0;

    p_xml_reader = xml_ReaderCreate( p_demux, p_stream ? p_stream
                                                       : p_demux->s );
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
                if( ! ReadElement( p_xml_reader, &psz_title_asx ) )
                    break;
                input_item_SetTitle( p_current_input, psz_title_asx );
            }
            else if( !strncasecmp( psz_node, "AUTHOR", 6 ) )
            {
                if( ! ReadElement( p_xml_reader, &psz_txt ) )
                    break;
                input_item_SetArtist( p_current_input, psz_txt );
            }
            else if( !strncasecmp( psz_node, "COPYRIGHT", 9 ) )
            {
                if( ! ReadElement( p_xml_reader, &psz_txt ) )
                    break;
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
                {
                    if( ! ReadElement( p_xml_reader, &psz_txt ) )
                        break;
                }
                else
                    psz_txt = strdup( psz_node );

                vlc_xml_decode( psz_txt );
                input_item_SetURL( p_current_input, psz_txt );
            }
            else if( !strncasecmp( psz_node, "ABSTRACT", 8 ) )
            {
                if( ! ReadElement( p_xml_reader, &psz_txt ) )
                    break;
                input_item_SetDescription( p_current_input, psz_txt );
            }
            else
            /* Base Node handler */
            if( !strncasecmp( psz_node, "BASE", 4 ) )
            {
                if( ! ReadElement( p_xml_reader, &psz_base ) )
                    break;
            }
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
                if( ! psz_tmp )
                    break;

                /* Create new input item */
                input_item_t *p_input;
                psz_txt = strdup( psz_node );
                vlc_xml_decode( psz_txt );
                p_input = input_item_New( psz_txt, psz_title_asx );
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
