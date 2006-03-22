/*****************************************************************************
 * xtag.c : a trivial parser for XML-like tags
 *****************************************************************************
 * Copyright (C) 2003-2004 Commonwealth Scientific and Industrial Research
 *                         Organisation (CSIRO) Australia
 * Copyright (C) 2000-2004 the VideoLAN team
 *
 * $Id$
 *
 * Authors: Conrad Parker <Conrad.Parker@csiro.au>
 *          Andre Pang <Andre.Pang@csiro.au>
 *          Gildas Bazin <gbazin@videolan.org>
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

#include <stdlib.h>
#include <vlc/vlc.h>

#include "vlc_xml.h"
#include "vlc_block.h"
#include "vlc_stream.h"

#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#undef XTAG_DEBUG

typedef struct _XList
{
    struct _XList *prev;
    struct _XList *next;
    void *data;
} XList;

/*
 * struct XTag is kind of a union ... it normally represents a whole
 * tag (and its children), but it could alternatively represent some
 * PCDATA. Basically, if tag->pcdata is non-NULL, interpret only it and
 * ignore the name, attributes and inner_tags.
 */
typedef struct _XTag
{
    char *name;
    char *pcdata;
    struct _XTag *parent;
    XList *attributes;
    XList *children;
    XList *current_child;
} XTag;

typedef struct _XAttribute
{
    char *name;
    char *value;
} XAttribute;

typedef struct _XTagParser
{
    int valid; /* boolean */
    XTag *current_tag;
    char *start;
    char *end;
} XTagParser;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("Simple XML Parser") );
    set_capability( "xml", 5 );
    set_callbacks( Open, Close );
vlc_module_end();

struct xml_reader_sys_t
{
    XTag *p_root; /* Root tag */
    XTag *p_curtag; /* Current tag */
    XList *p_curattr; /* Current attribute */
    vlc_bool_t b_endtag;
};

static xml_reader_t *ReaderCreate( xml_t *, stream_t * );
static void ReaderDelete( xml_reader_t * );
static int ReaderRead( xml_reader_t * );
static int ReaderNodeType( xml_reader_t * );
static char *ReaderName( xml_reader_t * );
static char *ReaderValue( xml_reader_t * );
static int ReaderNextAttr( xml_reader_t * );

static int ReaderUseDTD ( xml_reader_t *, vlc_bool_t );

static void CatalogLoad( xml_t *, const char * );
static void CatalogAdd( xml_t *, const char *, const char *, const char * );

static XTag *xtag_new_parse( const char *, int );
static char *xtag_get_name( XTag * );
static char *xtag_get_pcdata( XTag * );
static char *xtag_get_attribute( XTag *, char * );
static XTag *xtag_first_child( XTag *, char * );
static XTag *xtag_next_child( XTag *, char * );
static XTag *xtag_free( XTag * );
static int xtag_snprint( char *, int, XTag * );

/*****************************************************************************
 * Module initialization
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    xml_t *p_xml = (xml_t *)p_this;

    p_xml->pf_reader_create = ReaderCreate;
    p_xml->pf_reader_delete = ReaderDelete;

    p_xml->pf_catalog_load = CatalogLoad;
    p_xml->pf_catalog_add  = CatalogAdd;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Module deinitialization
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    return;
}

/*****************************************************************************
 * Catalogue functions
 *****************************************************************************/
static void CatalogLoad( xml_t *p_xml, const char *psz_filename )
{
    msg_Dbg( p_xml, "catalog support not implemented" );
}

static void CatalogAdd( xml_t *p_xml, const char *psz_arg1,
                          const char *psz_arg2, const char *psz_filename )
{
}

/*****************************************************************************
 * Reader functions
 *****************************************************************************/
static xml_reader_t *ReaderCreate( xml_t *p_xml, stream_t *s )
{
    xml_reader_t *p_reader;
    char *p_buffer, *p_new;
    int i_size, i_pos = 0, i_buffer = 2048;
    XTag *p_root;

    /* Open and read file */
    p_buffer = malloc( i_buffer );
    if( p_buffer == NULL ) {
        msg_Err( p_xml, "out of memory" );
        return NULL;
    }

    while( ( i_size = stream_Read( s, &p_buffer[i_pos], 2048 ) ) == 2048 )
    {
        i_pos += i_size;
        i_buffer += i_size;
        p_new = realloc( p_buffer, i_buffer );
        if (!p_new) {
            msg_Err( p_xml, "out of memory" );
            free( p_buffer );
            return NULL;
        }
        p_buffer = p_new;
    }
    p_buffer[ i_pos + i_size ] = 0; /* 0 terminated string */

    if( i_pos + i_size == 0 )
    {
        msg_Dbg( p_xml, "empty XML" );
        free( p_buffer );
        return 0;
    }

    p_root = xtag_new_parse( p_buffer, i_buffer );
    if( !p_root )
    {
        msg_Warn( p_xml, "couldn't parse XML" );
        free( p_buffer );
        return 0;
    }

    p_reader = malloc( sizeof(xml_reader_t) );
    p_reader->p_sys = malloc( sizeof(xml_reader_sys_t) );
    p_reader->p_sys->p_root = p_root;
    p_reader->p_sys->p_curtag = NULL;
    p_reader->p_sys->p_curattr = NULL;
    p_reader->p_sys->b_endtag = VLC_FALSE;
    p_reader->p_xml = p_xml;

    p_reader->pf_read = ReaderRead;
    p_reader->pf_node_type = ReaderNodeType;
    p_reader->pf_name = ReaderName;
    p_reader->pf_value = ReaderValue;
    p_reader->pf_next_attr = ReaderNextAttr;
    p_reader->pf_use_dtd = ReaderUseDTD;

    return p_reader;
}

static void ReaderDelete( xml_reader_t *p_reader )
{
    xtag_free( p_reader->p_sys->p_root );
    free( p_reader->p_sys );
    free( p_reader );
}

static int ReaderUseDTD ( xml_reader_t *p_reader, vlc_bool_t b_use )
{
    return VLC_EGENERIC;
}

static int ReaderRead( xml_reader_t *p_reader )
{
    XTag *p_child;

    if( !p_reader->p_sys->p_curtag )
    {
        p_reader->p_sys->p_curtag = p_reader->p_sys->p_root;
        return 1;
    }

    while( 1 )
    {
        if( (p_child = xtag_next_child( p_reader->p_sys->p_curtag, 0 )) )
        {
            p_reader->p_sys->p_curtag = p_child;
            p_reader->p_sys->p_curattr = 0;
            p_reader->p_sys->b_endtag = VLC_FALSE;
            return 1;
        }

        if( p_reader->p_sys->p_curtag->name && /* no end tag for pcdata */
            !p_reader->p_sys->b_endtag )
        {
            p_reader->p_sys->b_endtag = VLC_TRUE;
            return 1;
        }

        p_reader->p_sys->b_endtag = VLC_FALSE;
        if( !p_reader->p_sys->p_curtag->parent ) return 0;
        p_reader->p_sys->p_curtag = p_reader->p_sys->p_curtag->parent;
    }

    return 0;
}

static int ReaderNodeType( xml_reader_t *p_reader )
{
    if( p_reader->p_sys->p_curtag->name &&
        p_reader->p_sys->b_endtag ) return XML_READER_ENDELEM;
    if( p_reader->p_sys->p_curtag->name ) return XML_READER_STARTELEM;
    if( p_reader->p_sys->p_curtag->pcdata ) return XML_READER_TEXT;
    return XML_READER_NONE;
}

static char *ReaderName( xml_reader_t *p_reader )
{
    const char *psz_name;

    if( !p_reader->p_sys->p_curattr )
    {
        psz_name = xtag_get_name( p_reader->p_sys->p_curtag );
#ifdef XTAG_DEBUG
        printf( "TAG: %s\n", psz_name );
#endif
    }
    else
        psz_name = ((XAttribute *)p_reader->p_sys->p_curattr->data)->name;

    if( psz_name ) return strdup( psz_name );
    else return 0;
}

static char *ReaderValue( xml_reader_t *p_reader )
{
    const char *psz_name;
    if( p_reader->p_sys->p_curtag->pcdata )
    {
#ifdef XTAG_DEBUG
        printf( "%s\n", p_reader->p_sys->p_curtag->pcdata );
#endif
        return strdup( p_reader->p_sys->p_curtag->pcdata );
    }

    if( !p_reader->p_sys->p_curattr ) return 0;

#ifdef XTAG_DEBUG
    printf( "%s=%s\n", ((XAttribute *)p_reader->p_sys->p_curattr->data)->name,
            ((XAttribute *)p_reader->p_sys->p_curattr->data)->value );
#endif

    psz_name = ((XAttribute *)p_reader->p_sys->p_curattr->data)->value;

    if( psz_name ) return strdup( psz_name );
    else return 0;
}

static int ReaderNextAttr( xml_reader_t *p_reader )
{
    if( !p_reader->p_sys->p_curattr )
        p_reader->p_sys->p_curattr = p_reader->p_sys->p_curtag->attributes;
    else if( p_reader->p_sys->p_curattr )
        p_reader->p_sys->p_curattr = p_reader->p_sys->p_curattr->next;
 
    if( p_reader->p_sys->p_curattr ) return VLC_SUCCESS;
    else return VLC_EGENERIC;
}

/*****************************************************************************
 * XTAG parser functions
 *****************************************************************************/

static XList *xlist_append( XList *list, void *data )
{
    XList *l, *last;

    l = (XList *)malloc( sizeof(XList) );
    l->prev = l->next = NULL;
    l->data = data;

    if( list == NULL ) return l;

    for( last = list; last; last = last->next )
        if( last->next == NULL ) break;

    if( last ) last->next = l;
    l->prev = last; 
    return list;
}

static void xlist_free( XList *list )
{
    XList *l, *ln;

    for( l = list; l; l = ln )
    {
        ln = l->next;
        free( l );
    }
}

/* Character classes */
#define X_NONE           0
#define X_WHITESPACE  1<<0
#define X_OPENTAG     1<<1
#define X_CLOSETAG    1<<2
#define X_DQUOTE      1<<3
#define X_SQUOTE      1<<4
#define X_EQUAL       1<<5
#define X_SLASH       1<<6
#define X_QMARK       1<<7
#define X_DASH        1<<8
#define X_EMARK       1<<9

static int xtag_cin( char c, int char_class )
{
    if( char_class & X_WHITESPACE ) if( isspace(c) ) return VLC_TRUE;
    if( char_class & X_OPENTAG )    if( c == '<' ) return VLC_TRUE;
    if( char_class & X_CLOSETAG )   if( c == '>' ) return VLC_TRUE;
    if( char_class & X_DQUOTE )     if( c == '"' ) return VLC_TRUE;
    if( char_class & X_SQUOTE )     if( c == '\'' ) return VLC_TRUE;
    if( char_class & X_EQUAL )      if( c == '=' ) return VLC_TRUE;
    if( char_class & X_SLASH )      if( c == '/' ) return VLC_TRUE;
    if( char_class & X_QMARK )      if( c == '?' ) return VLC_TRUE;
    if( char_class & X_DASH  )      if( c == '-' ) return VLC_TRUE;
    if( char_class & X_EMARK )      if( c == '!' ) return VLC_TRUE;

    return VLC_FALSE;
}

static int xtag_index( XTagParser *parser, int char_class )
{
    char *s = parser->start;
    int i;

    for( i = 0; s[i] && s != parser->end; i++ )
    {
        if( xtag_cin( s[i], char_class ) ) return i;
    }

    return -1;
}

static void xtag_skip_over( XTagParser *parser, int char_class )
{
    char *s = parser->start;
    int i;

    if( !parser->valid ) return;

    for( i = 0; s[i] && s != parser->end; i++ )
    {
        if( !xtag_cin( s[i], char_class ) )
        {
            parser->start = &s[i];
            return;
        }
    }

    return;
}

static void xtag_skip_whitespace( XTagParser * parser )
{
    xtag_skip_over( parser, X_WHITESPACE );
}

static char *xtag_slurp_to( XTagParser *parser, int good_end, int bad_end )
{
    char *ret, *s = parser->start;
    int xi;

    if( !parser->valid ) return NULL;

    xi = xtag_index( parser, good_end | bad_end );

    if( xi > 0 && xtag_cin (s[xi], good_end) )
    {
        ret = malloc( (xi+1) * sizeof(char) );
        strncpy( ret, s, xi );
        ret[xi] = '\0';
        parser->start = &s[xi];
        return ret;
    }

    return NULL;
}

static int xtag_assert_and_pass( XTagParser *parser, int char_class )
{
    char *s = parser->start;

    if( !parser->valid ) return VLC_FALSE;

    if( !xtag_cin( s[0], char_class ) )
    {
        parser->valid = VLC_FALSE;
        return VLC_FALSE;
    }

    parser->start = &s[1];

    return VLC_TRUE;
}

static char *xtag_slurp_quoted( XTagParser *parser )
{
    char * ret, *s;
    int quote = X_DQUOTE; /* quote char to match on */
    int xi;

    if( !parser->valid ) return NULL;

    xtag_skip_whitespace( parser );

    s = parser->start;

    if( xtag_cin( s[0], X_SQUOTE ) ) quote = X_SQUOTE;

    if( !xtag_assert_and_pass( parser, quote ) ) return NULL;

    s = parser->start;

    for( xi = 0; s[xi]; xi++ )
    {
        if( xtag_cin( s[xi], quote ) )
        {
            if( !(xi > 1 && s[xi-1] == '\\') ) break;
        }
    }

    ret = malloc( (xi+1) * sizeof(char) );
    strncpy( ret, s, xi );
    ret[xi] = '\0';
    parser->start = &s[xi];

    if( !xtag_assert_and_pass( parser, quote ) ) return NULL;

    return ret;
}

static XAttribute *xtag_parse_attribute( XTagParser *parser )
{
    XAttribute *attr;
    char *name, *value;
    char *s;

    if( !parser->valid ) return NULL;

    xtag_skip_whitespace( parser );
 
    name = xtag_slurp_to( parser, X_WHITESPACE|X_EQUAL, X_SLASH|X_CLOSETAG );
    if( name == NULL ) return NULL;

    xtag_skip_whitespace( parser );
    s = parser->start;

    if( !xtag_assert_and_pass( parser, X_EQUAL ) )
    {
#ifdef XTAG_DEBUG
        printf( "xtag: attr failed EQUAL on <%s>\n", name );
#endif
        goto err_free_name;
    }

    xtag_skip_whitespace( parser );

    value = xtag_slurp_quoted( parser );

    if( value == NULL )
    {
#ifdef XTAG_DEBUG
        printf ("Got NULL quoted attribute value\n");
#endif
        goto err_free_name;
    }

    attr = malloc( sizeof (*attr) );
    attr->name = name;
    attr->value = value;
    return attr;

 err_free_name:
    free (name);
    parser->valid = VLC_FALSE;
    return NULL;
}

static XTag *xtag_parse_tag( XTagParser *parser )
{
    XTag *tag, *inner;
    XAttribute *attr;
    char *name;
    char *pcdata;
    char *s;
	 int xi;

    if( !parser->valid ) return NULL;

    s = parser->start;

    /* if this starts a comment tag, skip until end */
    if( (parser->end - parser->start) > 7 &&
		  xtag_cin( s[0], X_OPENTAG ) && xtag_cin( s[1], X_EMARK ) &&
        xtag_cin( s[2], X_DASH ) && xtag_cin( s[3], X_DASH ) )
    {
        parser->start = s = &s[4];
        while( (xi = xtag_index( parser, X_DASH )) >= 0 )
        {
            parser->start = s = &s[xi+1];
            if( xtag_cin( s[0], X_DASH ) && xtag_cin( s[1], X_CLOSETAG ) )
            {
                parser->start = &s[2];
                xtag_skip_whitespace( parser );
                return xtag_parse_tag( parser );
            }
        }
        return NULL;
    }

    /* ignore processing instructions '<?' ... '?>' */
    if( (parser->end - parser->start) > 4 &&
		  xtag_cin( s[0], X_OPENTAG ) && xtag_cin( s[1], X_QMARK ) )
    {
        parser->start = s = &s[2];
        while ((xi = xtag_index( parser, X_QMARK )) >= 0) {
            if (xtag_cin( s[xi+1], X_CLOSETAG )) {
                parser->start = &s[xi+2];
                xtag_skip_whitespace( parser );
                return xtag_parse_tag( parser );
            }
        }
        return NULL;
    }

    /* ignore doctype  '<!DOCTYPE' ... '>' */
    if ( (parser->end - parser->start) > 8 &&
			!strncmp( s, "<!DOCTYPE", 9 ) ) {
        xi = xtag_index( parser, X_CLOSETAG );
        if ( xi > 0 ) {
            parser->start = s = &s[xi+1];
            xtag_skip_whitespace( parser );
            return xtag_parse_tag( parser );
        }
        else {
            return NULL;
        }
    }

    if( (pcdata = xtag_slurp_to( parser, X_OPENTAG, X_NONE )) != NULL )
    {
        tag = malloc( sizeof(*tag) );
        tag->name = NULL;
        tag->pcdata = pcdata;
        tag->parent = parser->current_tag;
        tag->attributes = NULL;
        tag->children = NULL;
        tag->current_child = NULL;

        return tag;
    }

    /* if this starts a close tag, return NULL and let the parent take it */
    if( xtag_cin( s[0], X_OPENTAG ) && xtag_cin( s[1], X_SLASH ) )
        return NULL;

    /* parse CDATA content */
    if ( (parser->end - parser->start) > 8 && 
			!strncmp( s, "<![CDATA[", 9 ) ) {
        parser->start = s = &s[9];
        while (parser->end - s > 2) {
            if (strncmp( s, "]]>", 3 ) == 0) {
                if ( !(tag = malloc( sizeof(*tag))) ) return NULL;
                if ( !(pcdata = malloc( sizeof(char)*(s - parser->start + 1))) ) return NULL;
                strncpy( pcdata, parser->start, s - parser->start );
                pcdata[s - parser->start]='\0';
                parser->start = s = &s[3];
                tag->name = NULL;
                tag->pcdata = pcdata;
                tag->parent = parser->current_tag;
                tag->attributes = NULL;
                tag->children = NULL;
                tag->current_child = NULL;
                return tag;
            }
            else {
                s++;
            }
        }
        return NULL;
    }

    if( !xtag_assert_and_pass( parser, X_OPENTAG ) ) return NULL;

    name = xtag_slurp_to( parser, X_WHITESPACE|X_SLASH|X_CLOSETAG, X_NONE );
    if( name == NULL ) return NULL;

#ifdef XTAG_DEBUG
    printf ("<%s ...\n", name);
#endif

    tag = malloc( sizeof(*tag) );
    tag->name = name;
    tag->pcdata = NULL;
    tag->parent = parser->current_tag;
    tag->attributes = NULL;
    tag->children = NULL;
    tag->current_child = NULL;

    s = parser->start;

    if( xtag_cin( s[0], X_WHITESPACE ) )
    {
        while( (attr = xtag_parse_attribute( parser )) != NULL )
        {
            tag->attributes = xlist_append( tag->attributes, attr );
        }
    }

    xtag_skip_whitespace( parser );

    s = parser->start;

    if( xtag_cin( s[0], X_CLOSETAG ) )
    {
        parser->current_tag = tag;

        xtag_assert_and_pass( parser, X_CLOSETAG );

        while( (inner = xtag_parse_tag( parser ) ) != NULL )
        {
            tag->children = xlist_append( tag->children, inner );
        }

        parser->current_tag = tag->parent;
        xtag_skip_whitespace( parser );

        xtag_assert_and_pass( parser, X_OPENTAG );
        xtag_assert_and_pass( parser, X_SLASH );
        name = xtag_slurp_to( parser, X_WHITESPACE | X_CLOSETAG, X_NONE );
        if( name )
        {
            if( strcmp( name, tag->name ) )
            {
#ifdef XTAG_DEBUG
                printf ("got %s expected %s\n", name, tag->name);
#endif
                parser->valid = VLC_FALSE;
            }
            free( name );
        }

        xtag_skip_whitespace( parser );
        xtag_assert_and_pass( parser, X_CLOSETAG );
        xtag_skip_whitespace( parser );
    }
    else
    {
        xtag_assert_and_pass( parser, X_SLASH );
        xtag_assert_and_pass( parser, X_CLOSETAG );
        xtag_skip_whitespace( parser );
    }

    return tag;
}

static XTag *xtag_free( XTag *xtag )
{
    XList *l;
    XAttribute *attr;
    XTag *child;

    if( xtag == NULL ) return NULL;

    if( xtag->name ) free( xtag->name );
    if( xtag->pcdata ) free( xtag->pcdata );

    for( l = xtag->attributes; l; l = l->next )
    {
        if( (attr = (XAttribute *)l->data) != NULL )
        {
            if( attr->name ) free( attr->name );
            if( attr->value ) free( attr->value );
            free( attr );
        }
    }
    xlist_free( xtag->attributes );

    for( l = xtag->children; l; l = l->next )
    {
        child = (XTag *)l->data;
        xtag_free( child );
    }
    xlist_free( xtag->children );

    free( xtag );

    return NULL;
}

static XTag *xtag_new_parse( const char *s, int n )
{
    XTagParser parser;
    XTag *tag, *ttag, *wrapper;

    parser.valid = VLC_TRUE;
    parser.current_tag = NULL;
    parser.start = (char *)s;

    if( n == -1 ) parser.end = NULL;
    else if( n == 0 )
    {
#ifdef XTAG_DEBUG
        printf ("empty buffer");
#endif        
        return NULL;
    }
    else parser.end = (char *)&s[n];

    /* can't have whitespace pcdata outside rootnode */
    xtag_skip_whitespace( &parser );

    tag = xtag_parse_tag( &parser );

    if( !parser.valid )
    {
#ifdef XTAG_DEBUG
        printf ("invalid file");
#endif
        xtag_free( tag );
        return NULL;
    }

    if( (ttag = xtag_parse_tag( &parser )) != NULL )
    {
        if( !parser.valid )
        {
            xtag_free( ttag );
            return tag;
        }

        wrapper = malloc( sizeof(XTag) );
        wrapper->name = NULL;
        wrapper->pcdata = NULL;
        wrapper->parent = NULL;
        wrapper->attributes = NULL;
        wrapper->children = NULL;
        wrapper->current_child = NULL;

        wrapper->children = xlist_append( wrapper->children, tag );
        wrapper->children = xlist_append( wrapper->children, ttag );

        while( (ttag = xtag_parse_tag( &parser )) != NULL )
        {
            if( !parser.valid )
            {
                xtag_free( ttag );
                return wrapper;
            }

            wrapper->children = xlist_append( wrapper->children, ttag );
        }
        return wrapper;
    }

    return tag;
}

static char *xtag_get_name( XTag *xtag )
{
    return xtag ? xtag->name : NULL;
}

static char *xtag_get_pcdata( XTag *xtag )
{
    XList *l;
    XTag *child;

    if( xtag == NULL ) return NULL;

    for( l = xtag->children; l; l = l->next )
    {
        child = (XTag *)l->data;
        if( child->pcdata != NULL )
        {
            return child->pcdata;
        }
    }

    return NULL;
}

static char *xtag_get_attribute( XTag *xtag, char *attribute )
{
    XList *l;
    XAttribute *attr;

    if( xtag == NULL ) return NULL;

    for( l = xtag->attributes; l; l = l->next )
    {
        if( (attr = (XAttribute *)l->data) != NULL )
        {
            if( !strcmp( attr->name, attribute ) ) return attr->value;
        }
    }

    return NULL;
}

static XTag *xtag_first_child( XTag *xtag, char *name )
{
    XList *l;
    XTag *child;

    if( xtag == NULL ) return NULL;
    if( (l = xtag->children) == NULL ) return NULL;

    if( name == NULL )
    {
        xtag->current_child = l;
        return (XTag *)l->data;
    }

    for( ; l; l = l->next )
    {
        child = (XTag *)l->data;

        if( !strcmp( child->name, name ) )
        {
            xtag->current_child = l;
            return child;
        }
    }

    xtag->current_child = NULL;

    return NULL;
}

static XTag *xtag_next_child( XTag *xtag, char *name )
{
    XList *l;
    XTag *child;

    if( xtag == NULL ) return NULL;

    if( (l = xtag->current_child) == NULL )
        return xtag_first_child( xtag, name );

    if( (l = l->next) == NULL ) return NULL;

    if( name == NULL )
    {
        xtag->current_child = l;
        return (XTag *)l->data;
    }

    for( ; l; l = l->next )
    {
        child = (XTag *)l->data;

        if( !strcmp( child->name, name ) )
        {
            xtag->current_child = l;
            return child;
        }
    }

    xtag->current_child = NULL;

    return NULL;
}

/*
 * This snprints function takes a variable list of char *, the last of
 * which must be NULL, and prints each in turn to buf.
 * Returns C99-style total length that would have been written, even if
 * this is larger than n.
 */
static int xtag_snprints( char *buf, int n, ... )
{
    va_list ap;
    char *s;
    int len, to_copy, total = 0;

    va_start( ap, n );
  
    for( s = va_arg( ap, char * ); s; s = va_arg( ap, char *) )
    {
        len = strlen (s);

        if( (to_copy = __MIN(n, len) ) > 0 )
        {
            memcpy( buf, s, to_copy );
            buf += to_copy;
            n -= to_copy;
        }

        total += len;
    }

    va_end( ap );

    return total;
}

static int xtag_snprint( char *buf, int n, XTag *xtag )
{
    int nn, written = 0;
    XList *l;
    XAttribute *attr;
    XTag *child;

#define FORWARD(N) \
    buf += __MIN(n, N); \
    n = __MAX(n-N, 0);  \
    written += N;

    if( xtag == NULL )
    {
        if( n > 0 ) buf[0] = '\0';
        return 0;
    }

    if( xtag->pcdata )
    {
        nn = xtag_snprints( buf, n, xtag->pcdata, NULL );
        FORWARD( nn );

        return written;
    }

    if( xtag->name )
    {
        nn = xtag_snprints( buf, n, "<", xtag->name, NULL );
        FORWARD( nn );

        for( l = xtag->attributes; l; l = l->next )
        {
            attr = (XAttribute *)l->data;
      
            nn = xtag_snprints( buf, n, " ", attr->name, "=\"", attr->value,
                                "\"", NULL);
            FORWARD( nn );
        }

        if( xtag->children == NULL )
        {
            nn = xtag_snprints ( buf, n, "/>", NULL );
            FORWARD( nn );

            return written;
        }

        nn = xtag_snprints( buf, n, ">", NULL );
        FORWARD( nn );
    }

    for( l = xtag->children; l; l = l->next )
    {
        child = (XTag *)l->data;

        nn = xtag_snprint( buf, n, child );
        FORWARD( nn );
    }

    if( xtag->name )
    {
        nn = xtag_snprints( buf, n, "</", xtag->name, ">", NULL );
        FORWARD( nn );
    }

    return written;
}
