/*****************************************************************************
 * mvar.c : Variables handling for the HTTP Interface
 *****************************************************************************
 * Copyright (C) 2001-2006 the VideoLAN team
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

#include "http.h"

/* Utility function for scandir */
static int Filter( const char *foo )
{
    return strcmp( foo, "." );
};

static int InsensitiveAlphasort( const char **foo1,
                                 const char **foo2 )
{
    return strcasecmp( *foo1, *foo2 );
};



mvar_t *E_(mvar_New)( const char *name, const char *value )
{
    mvar_t *v = malloc( sizeof( mvar_t ) );

    if( !v ) return NULL;
    v->name = strdup( name );
    v->value = strdup( value ? value : "" );

    v->i_field = 0;
    v->field = malloc( sizeof( mvar_t * ) );
    v->field[0] = NULL;

    return v;
}

void E_(mvar_Delete)( mvar_t *v )
{
    int i;

    free( v->name );
    free( v->value );

    for( i = 0; i < v->i_field; i++ )
    {
        E_(mvar_Delete)( v->field[i] );
    }
    free( v->field );
    free( v );
}

void E_(mvar_AppendVar)( mvar_t *v, mvar_t *f )
{
    v->field = realloc( v->field, sizeof( mvar_t * ) * ( v->i_field + 2 ) );
    v->field[v->i_field] = f;
    v->i_field++;
}

mvar_t *E_(mvar_Duplicate)( const mvar_t *v )
{
    int i;
    mvar_t *n;

    n = E_(mvar_New)( v->name, v->value );
    for( i = 0; i < v->i_field; i++ )
    {
        E_(mvar_AppendVar)( n, E_(mvar_Duplicate)( v->field[i] ) );
    }

    return n;
}

void E_(mvar_PushVar)( mvar_t *v, mvar_t *f )
{
    v->field = realloc( v->field, sizeof( mvar_t * ) * ( v->i_field + 2 ) );
    if( v->i_field > 0 )
    {
        memmove( &v->field[1], &v->field[0], sizeof( mvar_t * ) * v->i_field );
    }
    v->field[0] = f;
    v->i_field++;
}

void E_(mvar_RemoveVar)( mvar_t *v, mvar_t *f )
{
    int i;
    for( i = 0; i < v->i_field; i++ )
    {
        if( v->field[i] == f )
        {
            break;
        }
    }
    if( i >= v->i_field )
    {
        return;
    }

    if( i + 1 < v->i_field )
    {
        memmove( &v->field[i], &v->field[i+1], sizeof( mvar_t * ) * ( v->i_field - i - 1 ) );
    }
    v->i_field--;
    /* FIXME should do a realloc */
}

mvar_t *E_(mvar_GetVar)( mvar_t *s, const char *name )
{
    /* format: name[index].field */
    char *field = strchr( name, '.' );
    int i = 1 + (field != NULL) ? (field - name) : strlen( name );
    char base[i];
    char *p;
    int i_index;

    strlcpy( base, name, i );
    if( field != NULL )
        field++;

    if( ( p = strchr( base, '[' ) ) != NULL )
    {
        *p++ = '\0';
        sscanf( p, "%d]", &i_index );
        if( i_index < 0 )
        {
            return NULL;
        }
    }
    else
    {
        i_index = 0;
    }

    for( i = 0; i < s->i_field; i++ )
    {
        if( !strcmp( s->field[i]->name, base ) )
        {
            if( i_index > 0 )
            {
                i_index--;
            }
            else
            {
                if( field )
                {
                    return E_(mvar_GetVar)( s->field[i], field );
                }
                else
                {
                    return s->field[i];
                }
            }
        }
    }
    return NULL;
}

char *E_(mvar_GetValue)( mvar_t *v, char *field )
{
    if( *field == '\0' )
    {
        return v->value;
    }
    else
    {
        mvar_t *f = E_(mvar_GetVar)( v, field );
        if( f )
        {
            return f->value;
        }
        else
        {
            return field;
        }
    }
}

void E_(mvar_PushNewVar)( mvar_t *vars, const char *name,
                          const char *value )
{
    mvar_t *f = E_(mvar_New)( name, value );
    E_(mvar_PushVar)( vars, f );
}

void E_(mvar_AppendNewVar)( mvar_t *vars, const char *name,
                            const char *value )
{
    mvar_t *f = E_(mvar_New)( name, value );
    E_(mvar_AppendVar)( vars, f );
}


/* arg= start[:stop[:step]],.. */
mvar_t *E_(mvar_IntegerSetNew)( const char *name, const char *arg )
{
    char *dup = strdup( arg );
    char *str = dup;
    mvar_t *s = E_(mvar_New)( name, "set" );

    while( str )
    {
        char *p;
        int  i_start,i_stop,i_step;
        int  i_match;

        p = strchr( str, ',' );
        if( p )
        {
            *p++ = '\0';
        }

        i_step = 0;
        i_match = sscanf( str, "%d:%d:%d", &i_start, &i_stop, &i_step );

        if( i_match == 1 )
        {
            i_stop = i_start;
            i_step = 1;
        }
        else if( i_match == 2 )
        {
            i_step = i_start < i_stop ? 1 : -1;
        }

        if( i_match >= 1 )
        {
            int i;

            if( ( i_start <= i_stop && i_step > 0 ) ||
                ( i_start >= i_stop && i_step < 0 ) )
            {
                for( i = i_start; ; i += i_step )
                {
                    char   value[79];

                    if( ( i_step > 0 && i > i_stop ) ||
                        ( i_step < 0 && i < i_stop ) )
                    {
                        break;
                    }

                    sprintf( value, "%d", i );

                    E_(mvar_PushNewVar)( s, name, value );
                }
            }
        }
        str = p;
    }

    free( dup );
    return s;
}

/********************************************************************
 * Special sets handling
 ********************************************************************/

mvar_t *E_(mvar_PlaylistSetNew)( intf_thread_t *p_intf, char *name,
                                 playlist_t *p_pl )
{
    playlist_view_t *p_view;
    mvar_t *s = E_(mvar_New)( name, "set" );


    vlc_mutex_lock( &p_pl->object_lock );

    p_view = playlist_ViewFind( p_pl, VIEW_CATEGORY ); /* FIXME */

    if( p_view != NULL )
        E_(PlaylistListNode)( p_intf, p_pl, p_view->p_root, name, s, 0 );

    vlc_mutex_unlock( &p_pl->object_lock );

    return s;
}

mvar_t *E_(mvar_InfoSetNew)( intf_thread_t *p_intf, char *name,
                             input_thread_t *p_input )
{
    mvar_t *s = E_(mvar_New)( name, "set" );
    int i, j;

    if( p_input == NULL )
    {
        return s;
    }

    vlc_mutex_lock( &p_input->input.p_item->lock );
    for ( i = 0; i < p_input->input.p_item->i_categories; i++ )
    {
        info_category_t *p_category = p_input->input.p_item->pp_categories[i];
        char *psz;

        mvar_t *cat  = E_(mvar_New)( name, "set" );
        mvar_t *iset = E_(mvar_New)( "info", "set" );

        psz = E_(FromUTF8)( p_intf, p_category->psz_name );
        E_(mvar_AppendNewVar)( cat, "name", psz );
        free( psz );
        E_(mvar_AppendVar)( cat, iset );

        for ( j = 0; j < p_category->i_infos; j++ )
        {
            info_t *p_info = p_category->pp_infos[j];
            mvar_t *info = E_(mvar_New)( "info", "" );
            char *psz_name = E_(FromUTF8)( p_intf, p_info->psz_name );
            char *psz_value = E_(FromUTF8)( p_intf, p_info->psz_value );

            /* msg_Dbg( p_input, "adding info name=%s value=%s",
                     psz_name, psz_value ); */
            E_(mvar_AppendNewVar)( info, "name",  psz_name );
            E_(mvar_AppendNewVar)( info, "value", psz_value );
            free( psz_name );
            free( psz_value );
            E_(mvar_AppendVar)( iset, info );
        }
        E_(mvar_AppendVar)( s, cat );
    }
    vlc_mutex_unlock( &p_input->input.p_item->lock );

    return s;
}

mvar_t *E_(mvar_ObjectSetNew)( intf_thread_t *p_intf, char *psz_name,
                               char *psz_capability )
{
    mvar_t *s = E_(mvar_New)( psz_name, "set" );
    int i;

    vlc_list_t *p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE,
                                        FIND_ANYWHERE );

    for( i = 0; i < p_list->i_count; i++ )
    {
        module_t *p_parser = (module_t *)p_list->p_values[i].p_object;
        if( !strcmp( p_parser->psz_capability, psz_capability ) )
        {
            mvar_t *sd = E_(mvar_New)( "sd", p_parser->psz_object_name );
            E_(mvar_AppendNewVar)( sd, "name",
                p_parser->psz_longname ? p_parser->psz_longname
                : ( p_parser->psz_shortname ? p_parser->psz_shortname
                : p_parser->psz_object_name ) );
            E_(mvar_AppendVar)( s, sd );
        }
    }

    vlc_list_release( p_list );

    return s;
}

mvar_t *E_(mvar_InputVarSetNew)( intf_thread_t *p_intf, char *name,
                                 input_thread_t *p_input,
                                 const char *psz_variable )
{
    intf_sys_t     *p_sys = p_intf->p_sys;
    mvar_t *s = E_(mvar_New)( name, "set" );
    vlc_value_t val, val_list, text_list;
    int i_type, i;

    if( p_input == NULL )
    {
        return s;
    }

    /* Check the type of the object variable */
    i_type = var_Type( p_sys->p_input, psz_variable );

    /* Make sure we want to display the variable */
    if( i_type & VLC_VAR_HASCHOICE )
    {
        var_Change( p_sys->p_input, psz_variable, VLC_VAR_CHOICESCOUNT, &val, NULL );
        if( val.i_int == 0 ) return s;
        if( (i_type & VLC_VAR_TYPE) != VLC_VAR_VARIABLE && val.i_int == 1 )
            return s;
    }
    else
    {
        return s;
    }

    switch( i_type & VLC_VAR_TYPE )
    {
    case VLC_VAR_VOID:
    case VLC_VAR_BOOL:
    case VLC_VAR_VARIABLE:
    case VLC_VAR_STRING:
    case VLC_VAR_INTEGER:
        break;
    default:
        /* Variable doesn't exist or isn't handled */
        return s;
    }

    if( var_Get( p_sys->p_input, psz_variable, &val ) < 0 )
    {
        return s;
    }

    if( var_Change( p_sys->p_input, psz_variable, VLC_VAR_GETLIST,
                    &val_list, &text_list ) < 0 )
    {
        if( (i_type & VLC_VAR_TYPE) == VLC_VAR_STRING ) free( val.psz_string );
        return s;
    }

    for( i = 0; i < val_list.p_list->i_count; i++ )
    {
        char *psz, psz_int[16];
        mvar_t *itm;

        switch( i_type & VLC_VAR_TYPE )
        {
        case VLC_VAR_STRING:
            itm = E_(mvar_New)( name, "set" );
            psz = E_(FromUTF8)( p_intf, text_list.p_list->p_values[i].psz_string );
            E_(mvar_AppendNewVar)( itm, "name", psz );
            psz = E_(FromUTF8)( p_intf, val_list.p_list->p_values[i].psz_string );
            E_(mvar_AppendNewVar)( itm, "id", psz );
            free( psz );
            snprintf( psz_int, sizeof(psz_int), "%d",
                      ( !strcmp( val.psz_string,
                                   val_list.p_list->p_values[i].psz_string )
                           && !( i_type & VLC_VAR_ISCOMMAND ) ) );
            E_(mvar_AppendNewVar)( itm, "selected", psz_int );
            E_(mvar_AppendVar)( s, itm );
            break;

        case VLC_VAR_INTEGER:
            itm = E_(mvar_New)( name, "set" );
            psz = E_(FromUTF8)( p_intf, text_list.p_list->p_values[i].psz_string );
            E_(mvar_AppendNewVar)( itm, "name", psz );
            snprintf( psz_int, sizeof(psz_int), "%d",
                      val_list.p_list->p_values[i].i_int );
            E_(mvar_AppendNewVar)( itm, "id", psz_int );
            snprintf( psz_int, sizeof(psz_int), "%d",
                      ( val.i_int == val_list.p_list->p_values[i].i_int )
                         && !( i_type & VLC_VAR_ISCOMMAND ) );
            E_(mvar_AppendNewVar)( itm, "selected", psz_int );
            E_(mvar_AppendVar)( s, itm );
            break;

        default:
            break;
        }
    }
    /* clean up everything */
    if( (i_type & VLC_VAR_TYPE) == VLC_VAR_STRING ) free( val.psz_string );
    var_Change( p_sys->p_input, psz_variable, VLC_VAR_FREELIST, &val_list,
                &text_list );
    return s;
}

#if 0
mvar_t *E_(mvar_HttpdInfoSetNew)( char *name, httpd_t *p_httpd, int i_type )
{
    mvar_t       *s = E_(mvar_New)( name, "set" );
    httpd_info_t info;
    int          i;

    if( !p_httpd->pf_control( p_httpd, i_type, &info, NULL ) )
    {
        for( i= 0; i < info.i_count; )
        {
            mvar_t *inf;

            inf = E_(mvar_New)( name, "set" );
            do
            {
                /* fprintf( stderr," mvar_HttpdInfoSetNew: append name=`%s' value=`%s'\n",
                            info.info[i].psz_name, info.info[i].psz_value ); */
                E_(mvar_AppendNewVar)( inf,
                                   info.info[i].psz_name,
                                   info.info[i].psz_value );
                i++;
            } while( i < info.i_count && strcmp( info.info[i].psz_name, "id" ) );
            E_(mvar_AppendVar)( s, inf );
        }
    }

    /* free mem */
    for( i = 0; i < info.i_count; i++ )
    {
        free( info.info[i].psz_name );
        free( info.info[i].psz_value );
    }
    if( info.i_count > 0 )
    {
        free( info.info );
    }

    return s;
}
#endif

mvar_t *E_(mvar_FileSetNew)( intf_thread_t *p_intf, char *name,
                             char *psz_dir )
{
    mvar_t *s = E_(mvar_New)( name, "set" );
#ifdef HAVE_SYS_STAT_H
    struct stat   stat_info;
#endif
    char        **ppsz_dir_content;
    int           i_dir_content, i;
    /* convert all / to native separator */
#if defined( WIN32 )
    const char sep = '\\';
#else
    const char sep = '/';
#endif

    psz_dir = E_(RealPath)( p_intf, psz_dir );

#ifdef HAVE_SYS_STAT_H
    if( (utf8_stat( psz_dir, &stat_info ) == -1 )
     || !S_ISDIR( stat_info.st_mode )
#   if defined( WIN32 )
          && psz_dir[0] != '\0' && (psz_dir[0] != '\\' || psz_dir[1] != '\0')
#   endif
      )
    {
        free( psz_dir );
        return s;
    }
#endif

    /* parse psz_src dir */
    if( ( i_dir_content = utf8_scandir( psz_dir, &ppsz_dir_content, Filter,
                                        InsensitiveAlphasort ) ) == -1 )
    {
        msg_Warn( p_intf, "error while scanning dir %s (%s)", psz_dir,
                  strerror(errno) );
        free( psz_dir );
        return s;
    }

    for( i = 0; i < i_dir_content; i++ )
    {
        char *psz_dir_content = ppsz_dir_content[i];
        char psz_tmp[strlen( psz_dir ) + 1 + strlen( psz_dir_content ) + 1];
        mvar_t *f;
        char *psz_name, *psz_ext, *psz_dummy;

#if defined( WIN32 )
        if( psz_dir[0] == '\0' || (psz_dir[0] == '\\' && psz_dir[1] == '\0') )
        {
            strcpy( psz_tmp, psz_dir_content );
        }
        else
#endif
        {
            sprintf( psz_tmp, "%s%c%s", psz_dir, sep, psz_dir_content );

#ifdef HAVE_SYS_STAT_H
            if( utf8_stat( psz_tmp, &stat_info ) == -1 )
            {
                free( psz_dir_content );
                continue;
            }
#endif
        }
        f = E_(mvar_New)( name, "set" );

        /* FIXME: merge vlc_fix_readdir_charset with utf8_readir */
        psz_dummy = vlc_fix_readdir_charset( p_intf, psz_dir_content );
        psz_name = E_(FromUTF8)( p_intf, psz_dummy );
        free( psz_dummy );

        /* put lower-case file extension in 'ext' */
        psz_ext = strrchr( psz_name, '.' );
        psz_ext = strdup( psz_ext != NULL ? psz_ext + 1 : "" );
        for( psz_dummy = psz_ext; *psz_dummy != '\0'; psz_dummy++ )
            *psz_dummy = tolower( *psz_dummy );

        E_(mvar_AppendNewVar)( f, "ext", psz_ext );
        free( psz_ext );

#if defined( WIN32 )
        if( psz_dir[0] == '\0' || (psz_dir[0] == '\\' && psz_dir[1] == '\0') )
        {
            char psz_tmp[3];
            sprintf( psz_tmp, "%c:", psz_name[0] );
            E_(mvar_AppendNewVar)( f, "name", psz_name );
            E_(mvar_AppendNewVar)( f, "basename", psz_tmp );
            E_(mvar_AppendNewVar)( f, "type", "directory" );
            E_(mvar_AppendNewVar)( f, "size", "unknown" );
            E_(mvar_AppendNewVar)( f, "date", "unknown" );
        }
        else
#endif
        {
            char psz_ctime[26];
            char psz_tmp[strlen( psz_dir ) + 1 + strlen( psz_name ) + 1];

            sprintf( psz_tmp, "%s%c%s", psz_dir, sep, psz_name );
            E_(mvar_AppendNewVar)( f, "name", psz_tmp );
            E_(mvar_AppendNewVar)( f, "basename", psz_name );

#ifdef HAVE_SYS_STAT_H
            if( S_ISDIR( stat_info.st_mode ) )
            {
                E_(mvar_AppendNewVar)( f, "type", "directory" );
            }
            else if( S_ISREG( stat_info.st_mode ) )
            {
                E_(mvar_AppendNewVar)( f, "type", "file" );
            }
            else
            {
                E_(mvar_AppendNewVar)( f, "type", "unknown" );
            }

            sprintf( psz_ctime, I64Fd, (int64_t)stat_info.st_size );
            E_(mvar_AppendNewVar)( f, "size", psz_ctime );

            /* FIXME memory leak FIXME */
#   ifdef HAVE_CTIME_R
            ctime_r( &stat_info.st_mtime, psz_ctime );
            E_(mvar_AppendNewVar)( f, "date", psz_ctime );
#   else
            E_(mvar_AppendNewVar)( f, "date", ctime( &stat_info.st_mtime ) );
#   endif

#else
            E_(mvar_AppendNewVar)( f, "type", "unknown" );
            E_(mvar_AppendNewVar)( f, "size", "unknown" );
            E_(mvar_AppendNewVar)( f, "date", "unknown" );
#endif
        }

        E_(mvar_AppendVar)( s, f );

        free( psz_name );
        free( psz_dir_content );
    }

    free( psz_dir );
    if( ppsz_dir_content != NULL )
        free( ppsz_dir_content );
    return s;
}

void E_(mvar_VlmSetNewLoop)( char *name, vlm_t *vlm, mvar_t *s, vlm_message_t *el, vlc_bool_t b_name );
void E_(mvar_VlmSetNewLoop)( char *name, vlm_t *vlm, mvar_t *s, vlm_message_t *el, vlc_bool_t b_name )
{
    /* Over name */
    mvar_t        *set;
    int k;

    /* Add a node with name and info */
    set = E_(mvar_New)( name, "set" );
    if( b_name == VLC_TRUE )
    {
        E_(mvar_AppendNewVar)( set, "name", el->psz_name );
    }

    for( k = 0; k < el->i_child; k++ )
    {
        vlm_message_t *ch = el->child[k];
        if( ch->i_child > 0 )
        {
            E_(mvar_VlmSetNewLoop)( ch->psz_name, vlm, set, ch, VLC_FALSE );
        }
        else
        {
            if( ch->psz_value )
            {
                E_(mvar_AppendNewVar)( set, ch->psz_name, ch->psz_value );
            }
            else
            {
                E_(mvar_AppendNewVar)( set, el->psz_name, ch->psz_name );
            }
        }
    }

    E_(mvar_AppendVar)( s, set );
}

mvar_t *E_(mvar_VlmSetNew)( char *name, vlm_t *vlm )
{
    mvar_t        *s = E_(mvar_New)( name, "set" );
    vlm_message_t *msg;
    int    i;

    if( vlm == NULL ) return s;

    if( vlm_ExecuteCommand( vlm, "show", &msg ) )
    {
        return s;
    }

    for( i = 0; i < msg->i_child; i++ )
    {
        /* Over media, schedule */
        vlm_message_t *ch = msg->child[i];
        int j;

        for( j = 0; j < ch->i_child; j++ )
        {
            /* Over name */
            vlm_message_t *el = ch->child[j];
            vlm_message_t *inf, *desc;
            char          psz[6 + strlen(el->psz_name)];

            sprintf( psz, "show %s", el->psz_name );
            if( vlm_ExecuteCommand( vlm, psz, &inf ) )
                continue;
            desc = inf->child[0];

            E_(mvar_VlmSetNewLoop)( el->psz_name, vlm, s, desc, VLC_TRUE );

            vlm_MessageDelete( inf );
        }
    }
    vlm_MessageDelete( msg );

    return s;
}
