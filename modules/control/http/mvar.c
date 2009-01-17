/*****************************************************************************
 * mvar.c : Variables handling for the HTTP Interface
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
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
#include <limits.h>

#include <assert.h>

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



mvar_t *mvar_New( const char *name, const char *value )
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

void mvar_Delete( mvar_t *v )
{
    int i;

    free( v->name );
    free( v->value );

    for( i = 0; i < v->i_field; i++ )
    {
        mvar_Delete( v->field[i] );
    }
    free( v->field );
    free( v );
}

void mvar_AppendVar( mvar_t *v, mvar_t *f )
{
    v->field = realloc( v->field, sizeof( mvar_t * ) * ( v->i_field + 2 ) );
    v->field[v->i_field] = f;
    v->i_field++;
}

mvar_t *mvar_Duplicate( const mvar_t *v )
{
    int i;
    mvar_t *n;

    n = mvar_New( v->name, v->value );
    for( i = 0; i < v->i_field; i++ )
    {
        mvar_AppendVar( n, mvar_Duplicate( v->field[i] ) );
    }

    return n;
}

void mvar_PushVar( mvar_t *v, mvar_t *f )
{
    v->field = realloc( v->field, sizeof( mvar_t * ) * ( v->i_field + 2 ) );
    if( v->i_field > 0 )
    {
        memmove( &v->field[1], &v->field[0], sizeof( mvar_t * ) * v->i_field );
    }
    v->field[0] = f;
    v->i_field++;
}

void mvar_RemoveVar( mvar_t *v, mvar_t *f )
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

mvar_t *mvar_GetVar( mvar_t *s, const char *name )
{
    /* format: name[index].field */
    const char *field = strchr( name, '.' );
    char base[1 + (field ? (size_t)(field - name) : strlen( name ))];
    char *p;
    int i_index, i;

    strlcpy( base, name, sizeof (base) );
    if( field != NULL )
        field++;

    if( ( p = strchr( base, '[' ) ) != NULL )
    {
        char *end;
        unsigned long l = strtoul( p, &end, 0 );

        if( ( l > INT_MAX ) || strcmp( "]", end ) )
            return NULL;

        *p++ = '\0';
        i_index = (int)l;
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
                    return mvar_GetVar( s->field[i], field );
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

const char *mvar_GetValue( mvar_t *v, const char *field )
{
    if( *field == '\0' )
    {
        return v->value;
    }
    else
    {
        mvar_t *f = mvar_GetVar( v, field );
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

void mvar_PushNewVar( mvar_t *vars, const char *name,
                          const char *value )
{
    mvar_t *f = mvar_New( name, value );
    mvar_PushVar( vars, f );
}

void mvar_AppendNewVar( mvar_t *vars, const char *name,
                            const char *value )
{
    mvar_t *f = mvar_New( name, value );
    mvar_AppendVar( vars, f );
}


/* arg= start[:stop[:step]],.. */
mvar_t *mvar_IntegerSetNew( const char *name, const char *arg )
{
    char *dup = strdup( arg );
    char *str = dup;
    mvar_t *s = mvar_New( name, "set" );

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

                    mvar_PushNewVar( s, name, value );
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

mvar_t *mvar_PlaylistSetNew( intf_thread_t *p_intf, char *name,
                                 playlist_t *p_pl )
{
    mvar_t *s = mvar_New( name, "set" );
    playlist_Lock( p_pl );
    PlaylistListNode( p_intf, p_pl, p_pl->p_root_category , name, s, 0 );
    playlist_Unlock( p_pl );
    return s;
}

mvar_t *mvar_InfoSetNew( char *name, input_thread_t *p_input )
{
    mvar_t *s = mvar_New( name, "set" );
    int i, j;

    if( p_input == NULL || p_input->p == NULL /* workarround assert in input_GetItem */ )
    {
        return s;
    }

    vlc_mutex_lock( &input_GetItem(p_input)->lock );
    for ( i = 0; i < input_GetItem(p_input)->i_categories; i++ )
    {
        info_category_t *p_category = input_GetItem(p_input)->pp_categories[i];

        mvar_t *cat  = mvar_New( name, "set" );
        mvar_t *iset = mvar_New( "info", "set" );

        mvar_AppendNewVar( cat, "name", p_category->psz_name );
        mvar_AppendVar( cat, iset );

        for ( j = 0; j < p_category->i_infos; j++ )
        {
            info_t *p_info = p_category->pp_infos[j];
            mvar_t *info = mvar_New( "info", "" );

            /* msg_Dbg( p_input, "adding info name=%s value=%s",
                     psz_name, psz_value ); */
            mvar_AppendNewVar( info, "name",  p_info->psz_name );
            mvar_AppendNewVar( info, "value", p_info->psz_value );
            mvar_AppendVar( iset, info );
        }
        mvar_AppendVar( s, cat );
    }
    vlc_mutex_unlock( &input_GetItem(p_input)->lock );

    return s;
}

mvar_t *mvar_ObjectSetNew( intf_thread_t *p_intf, char *psz_name,
                               const char *psz_capability )
{
    mvar_t *s = mvar_New( psz_name, "set" );
    size_t i;

    module_t **p_list = module_list_get( NULL );

    for( i = 0; p_list[i]; i++ )
    {
        module_t *p_parser = p_list[i];
        if( module_provides( p_parser, psz_capability ) )
        {
            mvar_t *sd = mvar_New( "sd", module_get_object( p_parser ) );
            mvar_AppendNewVar( sd, "name",
                                   module_get_name( p_parser, true ) );
            mvar_AppendVar( s, sd );
        }
    }

    module_list_free( p_list );

    return s;
}

mvar_t *mvar_InputVarSetNew( intf_thread_t *p_intf, char *name,
                                 input_thread_t *p_input,
                                 const char *psz_variable )
{
    intf_sys_t     *p_sys = p_intf->p_sys;
    mvar_t *s = mvar_New( name, "set" );
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
        char psz_int[16];
        mvar_t *itm;

        switch( i_type & VLC_VAR_TYPE )
        {
        case VLC_VAR_STRING:
            itm = mvar_New( name, "set" );
            mvar_AppendNewVar( itm, "name", text_list.p_list->p_values[i].psz_string );
            mvar_AppendNewVar( itm, "id", val_list.p_list->p_values[i].psz_string );
            snprintf( psz_int, sizeof(psz_int), "%d",
                      ( !strcmp( val.psz_string,
                                   val_list.p_list->p_values[i].psz_string )
                           && !( i_type & VLC_VAR_ISCOMMAND ) ) );
            mvar_AppendNewVar( itm, "selected", psz_int );
            mvar_AppendVar( s, itm );
            break;

        case VLC_VAR_INTEGER:
            itm = mvar_New( name, "set" );
            mvar_AppendNewVar( itm, "name", text_list.p_list->p_values[i].psz_string );
            snprintf( psz_int, sizeof(psz_int), "%d",
                      val_list.p_list->p_values[i].i_int );
            mvar_AppendNewVar( itm, "id", psz_int );
            snprintf( psz_int, sizeof(psz_int), "%d",
                      ( val.i_int == val_list.p_list->p_values[i].i_int )
                         && !( i_type & VLC_VAR_ISCOMMAND ) );
            mvar_AppendNewVar( itm, "selected", psz_int );
            mvar_AppendVar( s, itm );
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
mvar_t *mvar_HttpdInfoSetNew( char *name, httpd_t *p_httpd, int i_type )
{
    mvar_t       *s = mvar_New( name, "set" );
    httpd_info_t info;
    int          i;

    if( !p_httpd->pf_control( p_httpd, i_type, &info, NULL ) )
    {
        for( i= 0; i < info.i_count; )
        {
            mvar_t *inf;

            inf = mvar_New( name, "set" );
            do
            {
                /* fprintf( stderr," mvar_HttpdInfoSetNew: append name=`%s' value=`%s'\n",
                            info.info[i].psz_name, info.info[i].psz_value ); */
                mvar_AppendNewVar( inf,
                                   info.info[i].psz_name,
                                   info.info[i].psz_value );
                i++;
            } while( i < info.i_count && strcmp( info.info[i].psz_name, "id" ) );
            mvar_AppendVar( s, inf );
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

mvar_t *mvar_FileSetNew( intf_thread_t *p_intf, char *name,
                             char *psz_dir )
{
    mvar_t *s = mvar_New( name, "set" );
    char        **ppsz_dir_content;
    int           i_dir_content, i;
    psz_dir = RealPath( psz_dir );

    /* parse psz_src dir */
    if( ( i_dir_content = utf8_scandir( psz_dir, &ppsz_dir_content, Filter,
                                        InsensitiveAlphasort ) ) == -1 )
    {
        if( errno != ENOENT && errno != ENOTDIR )
            msg_Warn( p_intf, "error while scanning dir %s (%m)", psz_dir );
        free( psz_dir );
        return s;
    }

    for( i = 0; i < i_dir_content; i++ )
    {
#ifdef HAVE_SYS_STAT_H
        struct stat stat_info;
#endif
        char *psz_name = ppsz_dir_content[i], *psz_ext, *psz_dummy;
        char psz_tmp[strlen( psz_dir ) + 1 + strlen( psz_name ) + 1];
        mvar_t *f;

#if defined( WIN32 )
        if( psz_dir[0] == '\0' || (psz_dir[0] == '\\' && psz_dir[1] == '\0') )
        {
            strcpy( psz_tmp, psz_name );
        }
        else
#endif
        {
            sprintf( psz_tmp, "%s"DIR_SEP"%s", psz_dir, psz_name );

#ifdef HAVE_SYS_STAT_H
            if( utf8_stat( psz_tmp, &stat_info ) == -1 )
            {
                free( psz_name );
                continue;
            }
#endif
        }
        f = mvar_New( name, "set" );

        /* put lower-case file extension in 'ext' */
        psz_ext = strrchr( psz_name, '.' );
        psz_ext = strdup( psz_ext != NULL ? psz_ext + 1 : "" );
        for( psz_dummy = psz_ext; *psz_dummy != '\0'; psz_dummy++ )
            *psz_dummy = tolower( *psz_dummy );

        mvar_AppendNewVar( f, "ext", psz_ext );
        free( psz_ext );

#if defined( WIN32 )
        if( psz_dir[0] == '\0' || (psz_dir[0] == '\\' && psz_dir[1] == '\0') )
        {
            char psz_tmp[3];
            sprintf( psz_tmp, "%c:", psz_name[0] );
            mvar_AppendNewVar( f, "name", psz_name );
            mvar_AppendNewVar( f, "basename", psz_tmp );
            mvar_AppendNewVar( f, "type", "directory" );
            mvar_AppendNewVar( f, "size", "unknown" );
            mvar_AppendNewVar( f, "date", "unknown" );
        }
        else
#endif
        {
            char psz_buf[26];
            char psz_tmp[strlen( psz_dir ) + 1 + strlen( psz_name ) + 1];

            sprintf( psz_tmp, "%s"DIR_SEP"%s", psz_dir, psz_name );
            mvar_AppendNewVar( f, "name", psz_tmp );
            mvar_AppendNewVar( f, "basename", psz_name );

#ifdef HAVE_SYS_STAT_H
            if( S_ISDIR( stat_info.st_mode ) )
            {
                mvar_AppendNewVar( f, "type", "directory" );
            }
            else if( S_ISREG( stat_info.st_mode ) )
            {
                mvar_AppendNewVar( f, "type", "file" );
            }
            else
            {
                mvar_AppendNewVar( f, "type", "unknown" );
            }

            snprintf( psz_buf, sizeof( psz_buf ), "%"PRId64,
                      (int64_t)stat_info.st_size );
            mvar_AppendNewVar( f, "size", psz_buf );

            /* FIXME memory leak FIXME */
#   ifdef HAVE_CTIME_R
            ctime_r( &stat_info.st_mtime, psz_buf );
            mvar_AppendNewVar( f, "date", psz_buf );
#   else
            mvar_AppendNewVar( f, "date", ctime( &stat_info.st_mtime ) );
#   endif

#else
            mvar_AppendNewVar( f, "type", "unknown" );
            mvar_AppendNewVar( f, "size", "unknown" );
            mvar_AppendNewVar( f, "date", "unknown" );
#endif
        }

        mvar_AppendVar( s, f );

        free( psz_name );
    }

    free( psz_dir );
    free( ppsz_dir_content );
    return s;
}

static void mvar_VlmSetNewLoop( char *name, vlm_t *vlm, mvar_t *s,
                                vlm_message_t *el, bool b_name )
{
    /* Over name */
    mvar_t        *set;
    int k;

    /* Add a node with name and info */
    set = mvar_New( name, "set" );
    if( b_name == true )
    {
        mvar_AppendNewVar( set, "name", el->psz_name );
    }

    for( k = 0; k < el->i_child; k++ )
    {
        vlm_message_t *ch = el->child[k];
        if( ch->i_child > 0 )
        {
            mvar_VlmSetNewLoop( ch->psz_name, vlm, set, ch, false );
        }
        else
        {
            if( ch->psz_value )
            {
                mvar_AppendNewVar( set, ch->psz_name, ch->psz_value );
            }
            else
            {
                mvar_AppendNewVar( set, el->psz_name, ch->psz_name );
            }
        }
    }

    mvar_AppendVar( s, set );
}

mvar_t *mvar_VlmSetNew( char *name, vlm_t *vlm )
{
    mvar_t        *s = mvar_New( name, "set" );
#ifdef ENABLE_VLM
    vlm_message_t *msg;
    int    i;

    if( vlm == NULL ) return s;

    if( vlm_ExecuteCommand( vlm, "show", &msg ) )
        return s;

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

            mvar_VlmSetNewLoop( el->psz_name, vlm, s, desc, true );

            vlm_MessageDelete( inf );
        }
    }
    vlm_MessageDelete( msg );
#endif /* ENABLE_VLM */
    return s;
}
