/*****************************************************************************
 * qtl.c: QuickTime Media Link Importer
 *****************************************************************************
 * Copyright (C) 2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -@t- videolan -Dot- org>
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

/*
See
http://developer.apple.com/documentation/QuickTime/QT6WhatsNew/Chap1/chapter_1_section_54.html
and
http://developer.apple.com/documentation/QuickTime/WhatsNewQT5/QT5NewChapt1/chapter_1_section_39.html

autoplay - true/false
controller - true/false
fullscreen - normal/double/half/current/full
href - url
kioskmode - true/false
loop - true/false/palindrome
movieid - integer
moviename - string
playeveryframe - true/false
qtnext - url
quitwhendone - true/false
src - url (required)
type - mime type
volume - 0 (mute) - 100 (max)

*/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>

#include "playlist.h"
#include <vlc_xml.h>

typedef enum { FULLSCREEN_NORMAL,
               FULLSCREEN_DOUBLE,
               FULLSCREEN_HALF,
               FULLSCREEN_CURRENT,
               FULLSCREEN_FULL } qtl_fullscreen_t;
const char* ppsz_fullscreen[] = { "normal", "double", "half", "current", "full" };
typedef enum { LOOP_TRUE,
               LOOP_FALSE,
               LOOP_PALINDROME } qtl_loop_t;
const char* ppsz_loop[] = { "true", "false", "palindrome" };

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);

/*****************************************************************************
 * Import_QTL: main import function
 *****************************************************************************/
int Import_QTL( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;

    if( !demux_IsPathExtension( p_demux, ".qtl" ) )
        return VLC_EGENERIC;

    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    msg_Dbg( p_demux, "using QuickTime Media Link reader" );

    return VLC_SUCCESS;
}

static int Demux( demux_t *p_demux )
{
    xml_reader_t *p_xml_reader;
    const char *node;
    input_item_t *p_input;
    int i_ret = -1;

    /* List of all possible attributes. The only required one is "src" */
    bool b_autoplay = false;
    bool b_controler = true;
    qtl_fullscreen_t fullscreen = false;
    char *psz_href = NULL;
    bool b_kioskmode = false;
    qtl_loop_t loop = LOOP_FALSE;
    int i_movieid = -1;
    char *psz_moviename = NULL;
    bool b_playeveryframe = false;
    char *psz_qtnext = NULL;
    bool b_quitwhendone = false;
    char *psz_src = NULL;
    char *psz_mimetype = NULL;
    int i_volume = 100;

    input_item_t *p_current_input = GetCurrentItem(p_demux);

    p_xml_reader = xml_ReaderCreate( p_demux, p_demux->s );
    if( !p_xml_reader )
        goto error;

    /* check root node */
    if( xml_ReaderNextNode( p_xml_reader, &node ) != XML_READER_STARTELEM
     || strcmp( node, "embed" ) )
    {
        msg_Err( p_demux, "invalid root node <%s>", node );

        /* second line has <?quicktime tag ... so we try to skip it */
        msg_Dbg( p_demux, "trying to read one more node" );
        if( xml_ReaderNextNode( p_xml_reader, &node ) != XML_READER_STARTELEM
         || strcmp( node, "embed" ) )
        {
            msg_Err( p_demux, "invalid root node <%s>", node );
            goto error;
        }
    }

    const char *attrname, *value;
    while( (attrname = xml_ReaderNextAttr( p_xml_reader, &value )) != NULL )
    {
        if( !strcmp( attrname, "autoplay" ) )
            b_autoplay = !strcmp( value, "true" );
        else if( !strcmp( attrname, "controler" ) )
            b_controler = !strcmp( attrname, "false" );
        else if( !strcmp( attrname, "fullscreen" ) )
        {
            if( !strcmp( value, "double" ) )
                fullscreen = FULLSCREEN_DOUBLE;
            else if( !strcmp( value, "half" ) )
                fullscreen = FULLSCREEN_HALF;
            else if( !strcmp( value, "current" ) )
                fullscreen = FULLSCREEN_CURRENT;
            else if( !strcmp( value, "full" ) )
                fullscreen = FULLSCREEN_FULL;
            else
                fullscreen = FULLSCREEN_NORMAL;
        }
        else if( !strcmp( attrname, "href" ) )
        {
            free( psz_href );
            psz_href = strdup( value );
        }
        else if( !strcmp( attrname, "kioskmode" ) )
            b_kioskmode = !strcmp( value, "true" );
        else if( !strcmp( attrname, "loop" ) )
        {
            if( !strcmp( value, "true" ) )
                loop = LOOP_TRUE;
            else if( !strcmp( value, "palindrome" ) )
                loop = LOOP_PALINDROME;
            else
                loop = LOOP_FALSE;
        }
        else if( !strcmp( attrname, "movieid" ) )
            i_movieid = atoi( value );
        else if( !strcmp( attrname, "moviename" ) )
        {
            free( psz_moviename );
            psz_moviename = strdup( value );
        }
        else if( !strcmp( attrname, "playeveryframe" ) )
            b_playeveryframe = !strcmp( value, "true" );
        else if( !strcmp( attrname, "qtnext" ) )
        {
            free( psz_qtnext );
            psz_qtnext = strdup( value );
        }
        else if( !strcmp( attrname, "quitwhendone" ) )
            b_quitwhendone = !strcmp( value, "true" );
        else if( !strcmp( attrname, "src" ) )
        {
            free( psz_src );
            psz_src = strdup( value );
        }
        else if( !strcmp( attrname, "mimetype" ) )
        {
            free( psz_mimetype );
            psz_mimetype = strdup( value );
        }
        else if( !strcmp( attrname, "volume" ) )
            i_volume = atoi( value );
        else
            msg_Dbg( p_demux, "Attribute %s with value %s isn't valid",
                     attrname, value );
    }

    msg_Dbg( p_demux, "autoplay: %s (unused by VLC)",
             b_autoplay ? "true": "false" );
    msg_Dbg( p_demux, "controler: %s (unused by VLC)",
             b_controler ? "true": "false" );
    msg_Dbg( p_demux, "fullscreen: %s (unused by VLC)",
             ppsz_fullscreen[fullscreen] );
    msg_Dbg( p_demux, "href: %s", psz_href );
    msg_Dbg( p_demux, "kioskmode: %s (unused by VLC)",
             b_kioskmode ? "true":"false" );
    msg_Dbg( p_demux, "loop: %s (unused by VLC)", ppsz_loop[loop] );
    msg_Dbg( p_demux, "movieid: %d (unused by VLC)", i_movieid );
    msg_Dbg( p_demux, "moviename: %s", psz_moviename );
    msg_Dbg( p_demux, "playeverframe: %s (unused by VLC)",
             b_playeveryframe ? "true":"false" );
    msg_Dbg( p_demux, "qtnext: %s", psz_qtnext );
    msg_Dbg( p_demux, "quitwhendone: %s (unused by VLC)",
             b_quitwhendone ? "true":"false" );
    msg_Dbg( p_demux, "src: %s", psz_src );
    msg_Dbg( p_demux, "mimetype: %s", psz_mimetype );
    msg_Dbg( p_demux, "volume: %d (unused by VLC)", i_volume );


    if( !psz_src )
    {
        msg_Err( p_demux, "Mandatory attribute 'src' not found" );
    }
    else
    {
        input_item_node_t *p_subitems = input_item_node_Create( p_current_input );
        p_input = input_item_New( psz_src, psz_moviename );
#define SADD_INFO( type, field ) if( field ) { input_item_AddInfo( \
                    p_input, "QuickTime Media Link", type, "%s", field ) ; }
        SADD_INFO( "href", psz_href );
        SADD_INFO( _("Mime"), psz_mimetype );
        input_item_node_AppendItem( p_subitems, p_input );
        vlc_gc_decref( p_input );
        if( psz_qtnext )
        {
            p_input = input_item_New( psz_qtnext, NULL );
            input_item_node_AppendItem( p_subitems, p_input );
            vlc_gc_decref( p_input );
        }
        input_item_node_PostAndDelete( p_subitems );
    }

    i_ret = 0; /* Needed for correct operation of go back */

error:
    if( p_xml_reader )
        xml_ReaderDelete( p_xml_reader );

    vlc_gc_decref(p_current_input);

    free( psz_href );
    free( psz_moviename );
    free( psz_qtnext );
    free( psz_src );
    free( psz_mimetype );
    return i_ret;
}
