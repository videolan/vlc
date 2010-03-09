/*****************************************************************************
 * macro.c : Custom <vlc> macro handling
 *****************************************************************************
 * Copyright (C) 2001-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#include "http.h"
#include "macros.h"
#include <vlc_url.h>

static int MacroParse( macro_t *m, char *psz_src )
{
    char *dup = strdup( (char *)psz_src );
    char *src = dup;
    char *p;
    int     i_skip;

#define EXTRACT( name, l ) \
        src += l;    \
        p = strchr( src, '"' );             \
        if( p )                             \
        {                                   \
            *p++ = '\0';                    \
        }                                   \
        m->name = strdup( src );            \
        if( !p )                            \
        {                                   \
            break;                          \
        }                                   \
        src = p;

    /* init m */
    m->id = NULL;
    m->param1 = NULL;
    m->param2 = NULL;

    /* parse */
    src += 4;

    while( *src )
    {
        while( *src == ' ')
        {
            src++;
        }
        if( !strncmp( src, "id=\"", 4 ) )
        {
            EXTRACT( id, 4 );
        }
        else if( !strncmp( src, "param1=\"", 8 ) )
        {
            EXTRACT( param1, 8 );
        }
        else if( !strncmp( src, "param2=\"", 8 ) )
        {
            EXTRACT( param2, 8 );
        }
        else
        {
            break;
        }
    }
    if( strstr( src, "/>" ) )
    {
        src = strstr( src, "/>" ) + 2;
    }
    else
    {
        src += strlen( src );
    }

    if( m->id == NULL )
    {
        m->id = strdup( "" );
    }
    if( m->param1 == NULL )
    {
        m->param1 = strdup( "" );
    }
    if( m->param2 == NULL )
    {
        m->param2 = strdup( "" );
    }
    i_skip = src - dup;

    free( dup );
    return i_skip;
#undef EXTRACT
}

static void MacroClean( macro_t *m )
{
    free( m->id );
    free( m->param1 );
    free( m->param2 );
}

static int StrToMacroType( const char *name )
{
    int i;

    if( !name || *name == '\0')
    {
        return MVLC_UNKNOWN;
    }
    for( i = 0; StrToMacroTypeTab[i].psz_name != NULL; i++ )
    {
        if( !strcmp( name, StrToMacroTypeTab[i].psz_name ) )
        {
            return StrToMacroTypeTab[i].i_type;
        }
    }
    return MVLC_UNKNOWN;
}

static void MacroDo( httpd_file_sys_t *p_args,
                     macro_t *m,
                     char *p_request, int i_request,
                     char **pp_data,  int *pi_data,
                     char **pp_dst )
{
    intf_thread_t  *p_intf = p_args->p_intf;
    intf_sys_t     *p_sys = p_args->p_intf->p_sys;
    char control[512];

#define ALLOC( l ) \
    {               \
        int __i__ = *pp_dst - *pp_data; \
        *pi_data += (l);                  \
        *pp_data = xrealloc( *pp_data, *pi_data );   \
        *pp_dst = (*pp_data) + __i__;   \
    }
#define PRINT( str ) \
    ALLOC( strlen( str ) + 1 ); \
    *pp_dst += sprintf( *pp_dst, "%s", str );

#define PRINTS( str, s ) \
    ALLOC( strlen( str ) + strlen( s ) + 1 ); \
    { \
        char * psz_cur = *pp_dst; \
        *pp_dst += sprintf( *pp_dst, str, s ); \
        while( psz_cur && *psz_cur ) \
        {  \
            /* Prevent script injection */ \
            if( *psz_cur == '<' ) *psz_cur = '*'; \
            if( *psz_cur == '>' ) *psz_cur = '*'; \
            psz_cur++ ; \
        } \
    }

    switch( StrToMacroType( m->id ) )
    {
        case MVLC_CONTROL:
            if( i_request <= 0 )
            {
                break;
            }
            ExtractURIValue( p_request, "control", control, 512 );
            if( *m->param1 && !strstr( m->param1, control ) )
            {
                msg_Warn( p_intf, "unauthorized control=%s", control );
                break;
            }
            switch( StrToMacroType( control ) )
            {
                case MVLC_PLAY:
                {
                    int i_item;
                    char item[512];

                    ExtractURIValue( p_request, "item", item, 512 );
                    i_item = atoi( item );
                    /* id = 0 : simply ask playlist to play */
                    if( i_item == 0 )
                    {
                        playlist_Play( p_sys->p_playlist );
                        msg_Dbg( p_intf, "requested playlist play" );
                        break;
                    }
                    //TODO: really locked here ?
                    playlist_Control( p_sys->p_playlist, PLAYLIST_VIEWPLAY,
                                      true, NULL,
                                      playlist_ItemGetById( p_sys->p_playlist,
                                      i_item ) );
                    msg_Dbg( p_intf, "requested playlist item: %i", i_item );
                    break;
                }
                case MVLC_STOP:
                    playlist_Control( p_sys->p_playlist, PLAYLIST_STOP,
                                      true );
                    msg_Dbg( p_intf, "requested playlist stop" );
                    break;
                case MVLC_PAUSE:
                    playlist_Control( p_sys->p_playlist, PLAYLIST_PAUSE,
                                      true );
                    msg_Dbg( p_intf, "requested playlist pause" );
                    break;
                case MVLC_NEXT:
                    playlist_Control( p_sys->p_playlist, PLAYLIST_SKIP,
                                      true, 1 );
                    msg_Dbg( p_intf, "requested playlist next" );
                    break;
                case MVLC_PREVIOUS:
                    playlist_Control( p_sys->p_playlist, PLAYLIST_SKIP,
                                      true, -1 );
                    msg_Dbg( p_intf, "requested playlist previous" );
                    break;
                case MVLC_FULLSCREEN:
                    if( p_sys->p_input )
                    {
                        bool fs = var_ToggleBool( p_sys->p_playlist,
                                                  "fullscreen" );
                        vout_thread_t *p_vout = input_GetVout( p_sys->p_input );
                        if( p_vout )
                        {
                            var_SetBool( p_vout, "fullscreen", fs );
                            vlc_object_release( p_vout );
                            msg_Dbg( p_intf, "requested fullscreen toggle" );
                        }
                    }
                    break;
                case MVLC_SEEK:
                {
                    char value[30];
                    ExtractURIValue( p_request, "seek_value", value, 30 );
                    decode_URI( value );
                    HandleSeek( p_intf, value );
                    break;
                }
                case MVLC_VOLUME:
                {
                    char vol[8];
                    audio_volume_t i_volume;
                    int i_value;

                    ExtractURIValue( p_request, "value", vol, 8 );
                    aout_VolumeGet( p_sys->p_playlist, &i_volume );
                    decode_URI( vol );

                    if( vol[0] == '+' )
                    {
                        i_value = atoi( vol + 1 );
                        if( (i_volume + i_value) > AOUT_VOLUME_MAX )
                        {
                            aout_VolumeSet( p_sys->p_playlist, AOUT_VOLUME_MAX );
                            msg_Dbg( p_intf, "requested volume set: max" );
                        }
                        else
                        {
                            aout_VolumeSet( p_sys->p_playlist, (i_volume + i_value) );
                            msg_Dbg( p_intf, "requested volume set: +%i", (i_volume + i_value) );
                        }
                    }
                    else if( vol[0] == '-' )
                    {
                        i_value = atoi( vol + 1 );
                        if( (i_volume - i_value) < AOUT_VOLUME_MIN )
                        {
                            aout_VolumeSet( p_sys->p_playlist, AOUT_VOLUME_MIN );
                            msg_Dbg( p_intf, "requested volume set: min" );
                        }
                        else
                        {
                            aout_VolumeSet( p_sys->p_playlist, (i_volume - i_value) );
                            msg_Dbg( p_intf, "requested volume set: -%i", (i_volume - i_value) );
                        }
                    }
                    else if( strstr(vol, "%") != NULL )
                    {
                        i_value = atoi( vol );
                        if( (i_value <= 400) && (i_value>=0) ){
                            aout_VolumeSet( p_sys->p_playlist, (i_value * (AOUT_VOLUME_MAX - AOUT_VOLUME_MIN))/400+AOUT_VOLUME_MIN);
                            msg_Dbg( p_intf, "requested volume set: %i%%", atoi( vol ));
                        }
                    }
                    else
                    {
                        i_value = atoi( vol );
                        if( ( i_value <= AOUT_VOLUME_MAX ) && ( i_value >= AOUT_VOLUME_MIN ) )
                        {
                            aout_VolumeSet( p_sys->p_playlist, atoi( vol ) );
                            msg_Dbg( p_intf, "requested volume set: %i", atoi( vol ) );
                        }
                    }
                    break;
                }

                /* playlist management */
                case MVLC_ADD:
                {
                    char mrl[1024], psz_name[1024], tmp[1024];
                    char *p, *str;
                    input_item_t *p_input;

                    ExtractURIValue( p_request, "mrl", tmp, 1024 );
                    decode_URI( tmp );
                    ExtractURIValue( p_request, "name", psz_name, 1024 );
                    decode_URI( psz_name );
                    if( !*psz_name )
                    {
                        memcpy( psz_name, tmp, 1024 );
                    }
                    /* addslashes for backward compatibility with the old
                     * http intf */
                    p = mrl; str = tmp;
                    while( *str != '\0' )
                    {
                        if( *str == '"' || *str == '\'' || *str == '\\' )
                        {
                            *p++ = '\\';
                        }
                        *p++ = *str;
                        str++;
                    }
                    *p = '\0';

                    p_input = MRLParse( p_intf, mrl, psz_name );

                    char *psz_uri = p_input ? input_item_GetURI( p_input ) : NULL;
                    if( psz_uri && *psz_uri &&
                        playlist_AddInput( p_sys->p_playlist, p_input,
                                           PLAYLIST_APPEND, PLAYLIST_END,
                                           true, false) == VLC_SUCCESS )
                        msg_Dbg( p_intf, "requested mrl add: %s", mrl );
                    else
                        msg_Warn( p_intf, "adding mrl failed: %s", mrl );
                    free( psz_uri );
                    if( p_input )
                        vlc_gc_decref( p_input );
                    break;
                }
                case MVLC_DEL:
                {
                    int *p_items = NULL;
                    size_t i_nb_items = 0;
                    char item[512];
                    const char *p_parser = p_request;

                    /* Get the list of items to delete */
                    while( (p_parser =
                            ExtractURIValue( p_parser, "item", item, 512 )) )
                    {
                        if( !*item ) continue;

                        int i_item = atoi( item );
                        p_items = xrealloc( p_items,
                                        (i_nb_items + 1) * sizeof(*p_items) );
                        p_items[i_nb_items] = i_item;
                        i_nb_items++;
                    }

                    for( size_t i = 0; i < i_nb_items; i++ )
                    {
                        playlist_item_t *p_item;

                        msg_Dbg( p_intf, "requested playlist delete: %d",
                                 p_items[i] );
                        p_item = playlist_ItemGetById( p_sys->p_playlist,
                                                       p_items[i] );
                        if( p_item )
                            playlist_DeleteFromInput( p_sys->p_playlist,
                                                      p_item->p_input,
                                                      false );
                    }

                    free( p_items );
                    break;
                }
                case MVLC_KEEP:
                {
                    int *p_items = NULL;
                    size_t i_nb_items = 0, i;
                    char item[512];
                    const char *p_parser = p_request;

                    /* Get the list of items to keep */
                    while( (p_parser =
                       ExtractURIValue( p_parser, "item", item, 512 )) )
                    {
                        if( !*item ) continue;

                        int i_item = atoi( item );
                        p_items = xrealloc( p_items,
                                        (i_nb_items + 1) * sizeof(*p_items) );
                        p_items[i_nb_items] = i_item;
                        i_nb_items++;
                    }

                    size_t size = p_sys->p_playlist->items.i_size;
                    for( i = 0; i < size; i++ )
                    {
                        size_t j;

                        /* Check if the item is in the keep list */
                        for( j = 0 ; j < i_nb_items ; j++ )
                        {
                            if( p_items[j] ==
                                ARRAY_VAL(p_sys->p_playlist->items,i)->i_id)
                                break;
                        }
                        if( j == i_nb_items )
                        {
                            msg_Dbg( p_intf, "requested playlist delete: %d",
                                   p_sys->p_playlist->items.p_elems[i]->i_id );
                            playlist_DeleteFromInput( p_sys->p_playlist,
                                p_sys->p_playlist->items.p_elems[i]->p_input,
                                                      false );
                        }
                    }

                    free( p_items );
                    break;
                }
                case MVLC_EMPTY:
                {
                    playlist_Clear( p_sys->p_playlist, false );
                    msg_Dbg( p_intf, "requested playlist empty" );
                    break;
                }
                case MVLC_SORT:
                {
                    char type[12];
                    char order[2];
                    char item[512];
                    int i_order;
                    int i_item;

                    ExtractURIValue( p_request, "type", type, 12 );
                    ExtractURIValue( p_request, "order", order, 2 );
                    ExtractURIValue( p_request, "item", item, 512 );
                    i_item = atoi( item );

                    if( order[0] == '0' ) i_order = ORDER_NORMAL;
                    else i_order = ORDER_REVERSE;

                    if( !strcmp( type , "title" ) )
                    {
                        playlist_RecursiveNodeSort( p_sys->p_playlist,
                                                    /* Ugly hack,but not worse than before ... */
                                                    p_sys->p_playlist->p_root_onelevel,
                                                    SORT_TITLE_NODES_FIRST,
                                                    ( i_order == 0 ) ? ORDER_NORMAL : ORDER_REVERSE );
                        msg_Dbg( p_intf, "requested playlist sort by title (%d)" , i_order );
                    }
                    else if( !strcmp( type , "author" ) )
                    {
                        playlist_RecursiveNodeSort( p_sys->p_playlist, /*playlist_ItemGetById( p_sys->p_playlist, i_item ),*/
                                                    p_sys->p_playlist->p_root_onelevel,
                                                    SORT_ARTIST,
                                                    ( i_order == 0 ) ? ORDER_NORMAL : ORDER_REVERSE );
                        msg_Dbg( p_intf, "requested playlist sort by author (%d)" , i_order );
                    }
                    else if( !strcmp( type , "shuffle" ) )
                    {
                        playlist_RecursiveNodeSort( p_sys->p_playlist, /*playlist_ItemGetById( p_sys->p_playlist, i_item ),*/
                                                    p_sys->p_playlist->p_root_onelevel,
                                                    SORT_RANDOM,
                                                    ( i_order == 0 ) ? ORDER_NORMAL : ORDER_REVERSE );
                        msg_Dbg( p_intf, "requested playlist shuffle");
                    }

                    break;
                }
                case MVLC_MOVE:
                {
                    char psz_pos[6];
                    char psz_newpos[6];
                    int i_pos;
                    int i_newpos;
                    ExtractURIValue( p_request, "psz_pos", psz_pos, 6 );
                    ExtractURIValue( p_request, "psz_newpos", psz_newpos, 6 );
                    i_pos = atoi( psz_pos );
                    i_newpos = atoi( psz_newpos );
                    /* FIXME FIXME TODO TODO XXX XXX
                    ( duplicate from rpn.c )
                    if ( i_pos < i_newpos )
                    {
                        playlist_Move( p_sys->p_playlist, i_pos, i_newpos + 1 );
                    }
                    else
                    {
                        playlist_Move( p_sys->p_playlist, i_pos, i_newpos );
                    }
                    msg_Dbg( p_intf, "requested move playlist item %d to %d", i_pos, i_newpos);
                    FIXME FIXME TODO TODO XXX XXX */
                    break;
                }

                /* admin function */
                case MVLC_CLOSE:
                {
                    char id[512];
                    ExtractURIValue( p_request, "id", id, 512 );
                    msg_Dbg( p_intf, "requested close id=%s", id );
#if 0
                    if( p_sys->p_httpd->pf_control( p_sys->p_httpd, HTTPD_SET_CLOSE, id, NULL ) )
                    {
                        msg_Warn( p_intf, "close failed for id=%s", id );
                    }
#endif
                    break;
                }
                case MVLC_SHUTDOWN:
                {
                    msg_Dbg( p_intf, "requested shutdown" );
                    libvlc_Quit( p_intf->p_libvlc );
                    break;
                }
#ifdef ENABLE_VLM
                /* vlm */
                case MVLC_VLM_NEW:
                case MVLC_VLM_SETUP:
                {
                    static const char vlm_properties[][9] =
                    {
                        /* no args */
                        "enabled", "disabled", "loop", "unloop",
                        /* args required */
                        "input", "output", "option", "date", "period",
                        "repeat", "append", "",
                    };
                    vlm_message_t *vlm_answer;
                    char name[512];
                    char *psz = xmalloc( strlen( p_request ) + 1000 );
                    char *p = psz;
                    char *vlm_error;
                    int i;

                    if( p_intf->p_sys->p_vlm == NULL )
                        p_intf->p_sys->p_vlm = vlm_New( p_intf );

                    if( p_intf->p_sys->p_vlm == NULL )
                    {
                        free( psz );
                        break;
                    }

                    ExtractURIValue( p_request, "name", name, 512 );
                    if( StrToMacroType( control ) == MVLC_VLM_NEW )
                    {
                        char type[20];
                        ExtractURIValue( p_request, "type", type, 20 );
                        p += sprintf( psz, "new %s %s", name, type );
                    }
                    else
                    {
                        p += sprintf( psz, "setup %s", name );
                    }
                    /* Parse the request */
                    for( i = 0; vlm_properties[i][0]; i++ )
                    {
                        char val[512];
                        ExtractURIValue( p_request,
                                               vlm_properties[i], val, 512 );
                        decode_URI( val );
                        if( strlen( val ) > 0 && i >= 4 )
                        {
                            p += sprintf( p, " %s %s", vlm_properties[i], val );
                        }
                        else if( TestURIParam( p_request, vlm_properties[i] ) && i < 4 )
                        {
                            p += sprintf( p, " %s", vlm_properties[i] );
                        }
                    }
                    vlm_ExecuteCommand( p_intf->p_sys->p_vlm, psz, &vlm_answer );
                    if( vlm_answer->psz_value == NULL ) /* there is no error */
                    {
                        vlm_error = strdup( "" );
                    }
                    else
                    {
                        if( asprintf( &vlm_error , "%s : %s" ,
                                      vlm_answer->psz_name,
                                      vlm_answer->psz_value ) == -1 )
                            vlm_error = NULL;
                    }

                    mvar_AppendNewVar( p_args->vars, "vlm_error", vlm_error );

                    vlm_MessageDelete( vlm_answer );
                    free( vlm_error );
                    free( psz );
                    break;
                }

                case MVLC_VLM_DEL:
                {
                    vlm_message_t *vlm_answer;
                    char name[512];
                    char psz[512+10];
                    if( p_intf->p_sys->p_vlm == NULL )
                        p_intf->p_sys->p_vlm = vlm_New( p_intf );

                    if( p_intf->p_sys->p_vlm == NULL ) break;

                    ExtractURIValue( p_request, "name", name, 512 );
                    sprintf( psz, "del %s", name );

                    vlm_ExecuteCommand( p_intf->p_sys->p_vlm, psz, &vlm_answer );
                    /* FIXME do a vlm_answer -> var stack conversion */
                    vlm_MessageDelete( vlm_answer );
                    break;
                }

                case MVLC_VLM_PLAY:
                case MVLC_VLM_PAUSE:
                case MVLC_VLM_STOP:
                case MVLC_VLM_SEEK:
                {
                    vlm_message_t *vlm_answer;
                    char name[512];
                    char psz[512+10];
                    if( p_intf->p_sys->p_vlm == NULL )
                        p_intf->p_sys->p_vlm = vlm_New( p_intf );

                    if( p_intf->p_sys->p_vlm == NULL ) break;

                    ExtractURIValue( p_request, "name", name, 512 );
                    if( StrToMacroType( control ) == MVLC_VLM_PLAY )
                        sprintf( psz, "control %s play", name );
                    else if( StrToMacroType( control ) == MVLC_VLM_PAUSE )
                        sprintf( psz, "control %s pause", name );
                    else if( StrToMacroType( control ) == MVLC_VLM_STOP )
                        sprintf( psz, "control %s stop", name );
                    else if( StrToMacroType( control ) == MVLC_VLM_SEEK )
                    {
                        char percent[20];
                        ExtractURIValue( p_request, "percent", percent, 512 );
                        sprintf( psz, "control %s seek %s", name, percent );
                    }

                    vlm_ExecuteCommand( p_intf->p_sys->p_vlm, psz, &vlm_answer );
                    /* FIXME do a vlm_answer -> var stack conversion */
                    vlm_MessageDelete( vlm_answer );
                    break;
                }
                case MVLC_VLM_LOAD:
                case MVLC_VLM_SAVE:
                {
                    vlm_message_t *vlm_answer;
                    char file[512];
                    char psz[512];

                    if( p_intf->p_sys->p_vlm == NULL )
                        p_intf->p_sys->p_vlm = vlm_New( p_intf );

                    if( p_intf->p_sys->p_vlm == NULL ) break;

                    ExtractURIValue( p_request, "file", file, 512 );
                    decode_URI( file );

                    if( StrToMacroType( control ) == MVLC_VLM_LOAD )
                        sprintf( psz, "load %s", file );
                    else
                        sprintf( psz, "save %s", file );

                    vlm_ExecuteCommand( p_intf->p_sys->p_vlm, psz, &vlm_answer );
                    /* FIXME do a vlm_answer -> var stack conversion */
                    vlm_MessageDelete( vlm_answer );
                    break;
                }
#endif /* ENABLE_VLM */
                default:
                    if( *control )
                    {
                        PRINTS( "<!-- control param(%s) unsupported -->", control );
                    }
                    break;
            }
            break;

        case MVLC_SET:
        {
            char    value[512];
            int     i;
            float   f;

            if( i_request <= 0 ||
                *m->param1  == '\0' ||
                strstr( p_request, m->param1 ) == NULL )
            {
                break;
            }
            ExtractURIValue( p_request, m->param1,  value, 512 );
            decode_URI( value );

            switch( StrToMacroType( m->param2 ) )
            {
                case MVLC_INT:
                    i = atoi( value );
                    config_PutInt( p_intf, m->param1, i );
                    break;
                case MVLC_FLOAT:
                    f = atof( value );
                    config_PutFloat( p_intf, m->param1, f );
                    break;
                case MVLC_STRING:
                    config_PutPsz( p_intf, m->param1, value );
                    break;
                default:
                    PRINTS( "<!-- invalid type(%s) in set -->", m->param2 )
            }
            break;
        }
        case MVLC_GET:
        {
            char    value[512];
            int     i;
            float   f;
            char    *psz;
            lldiv_t div;

            if( *m->param1  == '\0' )
            {
                break;
            }

            switch( StrToMacroType( m->param2 ) )
            {
                case MVLC_INT:
                    i = config_GetInt( p_intf, m->param1 );
                    sprintf( value, "%d", i );
                    break;
                case MVLC_FLOAT:
                    f = config_GetFloat( p_intf, m->param1 );
                    div = lldiv( f * 1000000 , 1000000 );
                    sprintf( value, "%lld.%06u", div.quot,
                            (unsigned int)div.rem );
                    break;
                case MVLC_STRING:
                    psz = config_GetPsz( p_intf, m->param1 );
                    if( psz != NULL )
                    {
                        strlcpy( value, psz,sizeof( value ) );
                        free( psz );
                    }
                    else
                        *value = '\0';
                    msg_Dbg( p_intf, "%d: value = \"%s\"", __LINE__, value );
                    break;
                default:
                    snprintf( value, sizeof( value ),
                              "invalid type(%s) in set", m->param2 );
                    break;
            }
            PRINTS( "%s", value );
            break;
        }
        case MVLC_VALUE:
        {
            char *s;
            const char *v;

            if( m->param1 )
            {
                EvaluateRPN( p_intf, p_args->vars, &p_args->stack, m->param1 );
                s = SSPop( &p_args->stack );
                v = mvar_GetValue( p_args->vars, s );
            }
            else
            {
                v = s = SSPop( &p_args->stack );
            }

            PRINTS( "%s", v );
            free( s );
            break;
        }
        case MVLC_RPN:
            EvaluateRPN( p_intf, p_args->vars, &p_args->stack, m->param1 );
            break;

        /* Useful to learn stack management */
        case MVLC_STACK:
        {
            int i;
            msg_Dbg( p_intf, "stack" );
            for (i=0;i<(&p_args->stack)->i_stack;i++)
                msg_Dbg( p_intf, "%d -> %s", i, (&p_args->stack)->stack[i] );
            break;
        }

        case MVLC_UNKNOWN:
        default:
            PRINTS( "<!-- invalid macro id=`%s' -->", m->id );
            msg_Dbg( p_intf, "invalid macro id=`%s'", m->id );
            break;
    }
#undef PRINTS
#undef PRINT
#undef ALLOC
}

static
char *MacroSearch( char *src, char *end, int i_mvlc, bool b_after )
{
    int     i_id;
    int     i_level = 0;

    while( src < end )
    {
        if( src + 4 < end  && !strncmp( (char *)src, "<vlc", 4 ) )
        {
            int i_skip;
            macro_t m;

            i_skip = MacroParse( &m, src );

            i_id = StrToMacroType( m.id );

            switch( i_id )
            {
                case MVLC_IF:
                case MVLC_FOREACH:
                    i_level++;
                    break;
                case MVLC_END:
                    i_level--;
                    break;
                default:
                    break;
            }

            MacroClean( &m );

            if( ( i_mvlc == MVLC_END && i_level == -1 ) ||
                ( i_mvlc != MVLC_END && i_level == 0 && i_mvlc == i_id ) )
            {
                return src + ( b_after ? i_skip : 0 );
            }
            else if( i_level < 0 )
            {
                return NULL;
            }

            src += i_skip;
        }
        else
        {
            src++;
        }
    }

    return NULL;
}

void Execute( httpd_file_sys_t *p_args,
                     char *p_request, int i_request,
                     char **pp_data, int *pi_data,
                     char **pp_dst,
                     char *_src, char *_end )
{
    intf_thread_t  *p_intf = p_args->p_intf;

    char *src, *dup, *end;
    char *dst = *pp_dst;

    src = dup = xmalloc( _end - _src + 1 );
    end = src +( _end - _src );

    memcpy( src, _src, _end - _src );
    *end = '\0';

    /* we parse searching <vlc */
    while( src < end )
    {
        char *p;
        int i_copy;

        p = strstr( src, "<vlc" );
        if( p < end && p == src )
        {
            macro_t m;

            src += MacroParse( &m, src );

            //msg_Dbg( p_intf, "macro_id=%s", m.id );

            switch( StrToMacroType( m.id ) )
            {
                case MVLC_INCLUDE:
                {
                    FILE *f;
                    int  i_buffer;
                    char *p_buffer;
                    char psz_file[MAX_DIR_SIZE];
                    char *p;
                    char sep;

#if defined( WIN32 )
                    sep = '\\';
#else
                    sep = '/';
#endif

                    if( m.param1[0] != sep )
                    {
                        strcpy( psz_file, p_args->file );
                        p = strrchr( psz_file, sep );
                        if( p != NULL )
                            strcpy( p + 1, m.param1 );
                        else
                            strcpy( psz_file, m.param1 );
                    }
                    else
                    {
                        strcpy( psz_file, m.param1 );
                    }

                    /* We hereby assume that psz_file is in the
                     * local character encoding */
                    if( ( f = fopen( psz_file, "r" ) ) == NULL )
                    {
                        msg_Warn( p_args->p_intf,
                                  "unable to include file %s (%m)",
                                  psz_file );
                        break;
                    }

                    /* first we load in a temporary buffer */
                    FileLoad( f, &p_buffer, &i_buffer );

                    /* we parse executing all  <vlc /> macros */
                    Execute( p_args, p_request, i_request, pp_data, pi_data,
                             &dst, &p_buffer[0], &p_buffer[i_buffer] );
                    free( p_buffer );
                    fclose(f);
                    break;
                }
                case MVLC_IF:
                {
                    bool i_test;
                    char    *endif;

                    EvaluateRPN( p_intf, p_args->vars, &p_args->stack, m.param1 );
                    if( SSPopN( &p_args->stack, p_args->vars ) )
                    {
                        i_test = 1;
                    }
                    else
                    {
                        i_test = 0;
                    }
                    endif = MacroSearch( src, end, MVLC_END, true );

                    if( i_test == 0 )
                    {
                        char *start = MacroSearch( src, endif, MVLC_ELSE, true );

                        if( start )
                        {
                            char *stop  = MacroSearch( start, endif, MVLC_END, false );
                            if( stop )
                            {
                                Execute( p_args, p_request, i_request,
                                         pp_data, pi_data, &dst, start, stop );
                            }
                        }
                    }
                    else if( i_test == 1 )
                    {
                        char *stop;
                        if( ( stop = MacroSearch( src, endif, MVLC_ELSE, false ) ) == NULL )
                        {
                            stop = MacroSearch( src, endif, MVLC_END, false );
                        }
                        if( stop )
                        {
                            Execute( p_args, p_request, i_request,
                                     pp_data, pi_data, &dst, src, stop );
                        }
                    }

                    src = endif;
                    break;
                }
                case MVLC_FOREACH:
                {
                    char *endfor = MacroSearch( src, end, MVLC_END, true );
                    char *start = src;
                    char *stop = MacroSearch( src, end, MVLC_END, false );

                    if( stop )
                    {
                        mvar_t *index;
                        int    i_idx;
                        mvar_t *v;
                        if( !strcmp( m.param2, "integer" ) )
                        {
                            char *arg = SSPop( &p_args->stack );
                            index = mvar_IntegerSetNew( m.param1, arg );
                            free( arg );
                        }
                        else if( !strcmp( m.param2, "directory" ) )
                        {
                            char *arg = SSPop( &p_args->stack );
                            index = mvar_FileSetNew( p_intf, m.param1, arg );
                            free( arg );
                        }
                        else if( !strcmp( m.param2, "object" ) )
                        {
                            char *arg = SSPop( &p_args->stack );
                            index = mvar_ObjectSetNew( p_intf, m.param1, arg );
                            free( arg );
                        }
                        else if( !strcmp( m.param2, "playlist" ) )
                        {
                            index = mvar_PlaylistSetNew( p_intf, m.param1,
                                                    p_intf->p_sys->p_playlist );
                        }
                        else if( !strcmp( m.param2, "information" ) )
                        {
                            index = mvar_InfoSetNew( m.param1,
                                                     p_intf->p_sys->p_input );
                        }
                        else if( !strcmp( m.param2, "program" )
                                  || !strcmp( m.param2, "title" )
                                  || !strcmp( m.param2, "chapter" )
                                  || !strcmp( m.param2, "audio-es" )
                                  || !strcmp( m.param2, "video-es" )
                                  || !strcmp( m.param2, "spu-es" ) )
                        {
                            index = mvar_InputVarSetNew( p_intf, m.param1,
                                                         p_intf->p_sys->p_input,
                                                         m.param2 );
                        }
#ifdef ENABLE_VLM
                        else if( !strcmp( m.param2, "vlm" ) )
                        {
                            if( p_intf->p_sys->p_vlm == NULL )
                                p_intf->p_sys->p_vlm = vlm_New( p_intf );
                            index = mvar_VlmSetNew( m.param1, p_intf->p_sys->p_vlm );
                        }
#endif
#if 0
                        else if( !strcmp( m.param2, "hosts" ) )
                        {
                            index = mvar_HttpdInfoSetNew( m.param1, p_intf->p_sys->p_httpd, HTTPD_GET_HOSTS );
                        }
                        else if( !strcmp( m.param2, "urls" ) )
                        {
                            index = mvar_HttpdInfoSetNew( m.param1, p_intf->p_sys->p_httpd, HTTPD_GET_URLS );
                        }
                        else if( !strcmp( m.param2, "connections" ) )
                        {
                            index = mvar_HttpdInfoSetNew(m.param1, p_intf->p_sys->p_httpd, HTTPD_GET_CONNECTIONS);
                        }
#endif
                        else if( ( v = mvar_GetVar( p_args->vars, m.param2 ) ) )
                        {
                            index = mvar_Duplicate( v );
                        }
                        else
                        {
                            msg_Dbg( p_intf, "invalid index constructor (%s)", m.param2 );
                            src = endfor;
                            break;
                        }

                        for( i_idx = 0; i_idx < index->i_field; i_idx++ )
                        {
                            mvar_t *f = mvar_Duplicate( index->field[i_idx] );

                            //msg_Dbg( p_intf, "foreach field[%d] name=%s value=%s", i_idx, f->name, f->value );

                            free( f->name );
                            f->name = strdup( m.param1 );


                            mvar_PushVar( p_args->vars, f );
                            Execute( p_args, p_request, i_request,
                                     pp_data, pi_data, &dst, start, stop );
                            mvar_RemoveVar( p_args->vars, f );

                            mvar_Delete( f );
                        }
                        mvar_Delete( index );

                        src = endfor;
                    }
                    break;
                }
                default:
                    MacroDo( p_args, &m, p_request, i_request,
                             pp_data, pi_data, &dst );
                    break;
            }

            MacroClean( &m );
            continue;
        }

        i_copy =   ( (p == NULL || p > end ) ? end : p  ) - src;
        if( i_copy > 0 )
        {
            int i_index = dst - *pp_data;

            *pi_data += i_copy;
            *pp_data = xrealloc( *pp_data, *pi_data );
            dst = (*pp_data) + i_index;

            memcpy( dst, src, i_copy );
            dst += i_copy;
            src += i_copy;
        }
    }

    *pp_dst = dst;
    free( dup );
}
