/*****************************************************************************
 * rpn.c : RPN evaluator for the HTTP Interface
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
#include "vlc_url.h"
#include "vlc_meta.h"

static vlc_object_t *GetVLCObject( intf_thread_t *p_intf,
                                   const char *psz_object,
                                   vlc_bool_t *pb_need_release )
{
    intf_sys_t    *p_sys = p_intf->p_sys;
    int i_object_type = 0;
    vlc_object_t *p_object = NULL;
    *pb_need_release = VLC_FALSE;

    if( !strcmp( psz_object, "VLC_OBJECT_ROOT" ) )
        i_object_type = VLC_OBJECT_ROOT;
    else if( !strcmp( psz_object, "VLC_OBJECT_VLC" ) )
        p_object = VLC_OBJECT(p_intf->p_vlc);
    else if( !strcmp( psz_object, "VLC_OBJECT_INTF" ) )
        p_object = VLC_OBJECT(p_intf);
    else if( !strcmp( psz_object, "VLC_OBJECT_PLAYLIST" ) )
        p_object = VLC_OBJECT(p_sys->p_playlist);
    else if( !strcmp( psz_object, "VLC_OBJECT_INPUT" ) )
        p_object = VLC_OBJECT(p_sys->p_input);
    else if( !strcmp( psz_object, "VLC_OBJECT_VOUT" ) )
        i_object_type = VLC_OBJECT_VOUT;
    else if( !strcmp( psz_object, "VLC_OBJECT_AOUT" ) )
        i_object_type = VLC_OBJECT_AOUT;
    else if( !strcmp( psz_object, "VLC_OBJECT_SOUT" ) )
        i_object_type = VLC_OBJECT_SOUT;
    else
        msg_Warn( p_intf, "unknown object type (%s)", psz_object );

    if( p_object == NULL && i_object_type )
    {
        *pb_need_release = VLC_TRUE;
        p_object = vlc_object_find( p_intf, i_object_type, FIND_ANYWHERE );
    }

    return p_object;
}

void E_(SSInit)( rpn_stack_t *st )
{
    st->i_stack = 0;
}

void E_(SSClean)( rpn_stack_t *st )
{
    while( st->i_stack > 0 )
    {
        free( st->stack[--st->i_stack] );
    }
}

void E_(SSPush)( rpn_stack_t *st, const char *s )
{
    if( st->i_stack < STACK_MAX )
    {
        st->stack[st->i_stack++] = strdup( s );
    }
}

char *E_(SSPop)( rpn_stack_t *st )
{
    if( st->i_stack <= 0 )
    {
        return strdup( "" );
    }
    else
    {
        return st->stack[--st->i_stack];
    }
}

int E_(SSPopN)( rpn_stack_t *st, mvar_t  *vars )
{
    char *name;
    char *value;

    char *end;
    int  i;

    name = E_(SSPop)( st );
    i = strtol( name, &end, 0 );
    if( end == name )
    {
        value = E_(mvar_GetValue)( vars, name );
        i = atoi( value );
    }
    free( name );

    return( i );
}

void E_(SSPushN)( rpn_stack_t *st, int i )
{
    char v[12];

    snprintf( v, sizeof (v), "%d", i );
    E_(SSPush)( st, v );
}

void E_(EvaluateRPN)( intf_thread_t *p_intf, mvar_t  *vars,
                      rpn_stack_t *st, char *exp )
{
    intf_sys_t    *p_sys = p_intf->p_sys;

    while( exp != NULL && *exp != '\0' )
    {
        char *p, *s;

        /* skip space */
        while( *exp == ' ' )
        {
            exp++;
        }

        if( *exp == '\'' )
        {
            /* extract string */
            p = E_(FirstWord)( exp, exp );
            E_(SSPush)( st, exp );
            exp = p;
            continue;
        }

        /* extract token */
        p = E_(FirstWord)( exp, exp );
        s = exp;
        if( p == NULL )
        {
            exp += strlen( exp );
        }
        else
        {
            exp = p;
        }

        if( *s == '\0' )
        {
            break;
        }

        /* 1. Integer function */
        if( !strcmp( s, "!" ) )
        {
            E_(SSPushN)( st, ~E_(SSPopN)( st, vars ) );
        }
        else if( !strcmp( s, "^" ) )
        {
            E_(SSPushN)( st, E_(SSPopN)( st, vars ) ^ E_(SSPopN)( st, vars ) );
        }
        else if( !strcmp( s, "&" ) )
        {
            E_(SSPushN)( st, E_(SSPopN)( st, vars ) & E_(SSPopN)( st, vars ) );
        }
        else if( !strcmp( s, "|" ) )
        {
            E_(SSPushN)( st, E_(SSPopN)( st, vars ) | E_(SSPopN)( st, vars ) );
        }
        else if( !strcmp( s, "+" ) )
        {
            E_(SSPushN)( st, E_(SSPopN)( st, vars ) + E_(SSPopN)( st, vars ) );
        }
        else if( !strcmp( s, "-" ) )
        {
            int j = E_(SSPopN)( st, vars );
            int i = E_(SSPopN)( st, vars );
            E_(SSPushN)( st, i - j );
        }
        else if( !strcmp( s, "*" ) )
        {
            E_(SSPushN)( st, E_(SSPopN)( st, vars ) * E_(SSPopN)( st, vars ) );
        }
        else if( !strcmp( s, "/" ) )
        {
            int i, j;

            j = E_(SSPopN)( st, vars );
            i = E_(SSPopN)( st, vars );

            E_(SSPushN)( st, j != 0 ? i / j : 0 );
        }
        else if( !strcmp( s, "%" ) )
        {
            int i, j;

            j = E_(SSPopN)( st, vars );
            i = E_(SSPopN)( st, vars );

            E_(SSPushN)( st, j != 0 ? i % j : 0 );
        }
        /* 2. integer tests */
        else if( !strcmp( s, "=" ) )
        {
            E_(SSPushN)( st, E_(SSPopN)( st, vars ) == E_(SSPopN)( st, vars ) ? -1 : 0 );
        }
        else if( !strcmp( s, "!=" ) )
        {
            E_(SSPushN)( st, E_(SSPopN)( st, vars ) != E_(SSPopN)( st, vars ) ? -1 : 0 );
        }
        else if( !strcmp( s, "<" ) )
        {
            int j = E_(SSPopN)( st, vars );
            int i = E_(SSPopN)( st, vars );

            E_(SSPushN)( st, i < j ? -1 : 0 );
        }
        else if( !strcmp( s, ">" ) )
        {
            int j = E_(SSPopN)( st, vars );
            int i = E_(SSPopN)( st, vars );

            E_(SSPushN)( st, i > j ? -1 : 0 );
        }
        else if( !strcmp( s, "<=" ) )
        {
            int j = E_(SSPopN)( st, vars );
            int i = E_(SSPopN)( st, vars );

            E_(SSPushN)( st, i <= j ? -1 : 0 );
        }
        else if( !strcmp( s, ">=" ) )
        {
            int j = E_(SSPopN)( st, vars );
            int i = E_(SSPopN)( st, vars );

            E_(SSPushN)( st, i >= j ? -1 : 0 );
        }
        /* 3. string functions */
        else if( !strcmp( s, "strcat" ) )
        {
            char *s2 = E_(SSPop)( st );
            char *s1 = E_(SSPop)( st );
            char *str = malloc( strlen( s1 ) + strlen( s2 ) + 1 );

            strcpy( str, s1 );
            strcat( str, s2 );

            E_(SSPush)( st, str );
            free( s1 );
            free( s2 );
            free( str );
        }
        else if( !strcmp( s, "strcmp" ) )
        {
            char *s2 = E_(SSPop)( st );
            char *s1 = E_(SSPop)( st );

            E_(SSPushN)( st, strcmp( s1, s2 ) );
            free( s1 );
            free( s2 );
        }
        else if( !strcmp( s, "strncmp" ) )
        {
            int n = E_(SSPopN)( st, vars );
            char *s2 = E_(SSPop)( st );
            char *s1 = E_(SSPop)( st );

            E_(SSPushN)( st, strncmp( s1, s2 , n ) );
            free( s1 );
            free( s2 );
        }
        else if( !strcmp( s, "strsub" ) )
        {
            int n = E_(SSPopN)( st, vars );
            int m = E_(SSPopN)( st, vars );
            int i_len;
            char *s = E_(SSPop)( st );
            char *str;

            if( n >= m )
            {
                i_len = n - m + 1;
            }
            else
            {
                i_len = 0;
            }

            str = malloc( i_len + 1 );

            memcpy( str, s + m - 1, i_len );
            str[ i_len ] = '\0';

            E_(SSPush)( st, str );
            free( s );
            free( str );
        }
        else if( !strcmp( s, "strlen" ) )
        {
            char *str = E_(SSPop)( st );

            E_(SSPushN)( st, strlen( str ) );
            free( str );
        }
        else if( !strcmp( s, "str_replace" ) )
        {
            char *psz_to = E_(SSPop)( st );
            char *psz_from = E_(SSPop)( st );
            char *psz_in = E_(SSPop)( st );
            char *psz_in_current = psz_in;
            char *psz_out = malloc( strlen(psz_in) * strlen(psz_to) + 1 );
            char *psz_out_current = psz_out;

            while( (p = strstr( psz_in_current, psz_from )) != NULL )
            {
                memcpy( psz_out_current, psz_in_current, p - psz_in_current );
                psz_out_current += p - psz_in_current;
                strcpy( psz_out_current, psz_to );
                psz_out_current += strlen(psz_to);
                psz_in_current = p + strlen(psz_from);
            }
            strcpy( psz_out_current, psz_in_current );
            psz_out_current += strlen(psz_in_current);
            *psz_out_current = '\0';

            E_(SSPush)( st, psz_out );
            free( psz_to );
            free( psz_from );
            free( psz_in );
            free( psz_out );
        }
        else if( !strcmp( s, "url_extract" ) )
        {
            char *url = E_(mvar_GetValue)( vars, "url_value" );
            char *name = E_(SSPop)( st );
            char value[512];
            char *tmp;

            E_(ExtractURIValue)( url, name, value, 512 );
            decode_URI( value );
            tmp = E_(FromUTF8)( p_intf, value );
            E_(SSPush)( st, tmp );
            free( tmp );
            free( name );
        }
        else if( !strcmp( s, "url_encode" ) )
        {
            char *url = E_(SSPop)( st );
            char *value;

            value = E_(ToUTF8)( p_intf, url );
            free( url );
            url = value;
            value = vlc_UrlEncode( url );
            free( url );
            E_(SSPush)( st, value );
            free( value );
        }
        else if( !strcmp( s, "addslashes" ) )
        {
            char *psz_src = E_(SSPop)( st );
            char *psz_dest;
            char *str = psz_src;

            p = psz_dest = malloc( strlen( str ) * 2 + 1 );

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

            E_(SSPush)( st, psz_dest );
            free( psz_src );
            free( psz_dest );
        }
        else if( !strcmp( s, "stripslashes" ) )
        {
            char *psz_src = E_(SSPop)( st );
            char *psz_dest;
            char *str = psz_src;

            p = psz_dest = strdup( psz_src );

            while( *str )
            {
                if( *str == '\\' && *(str + 1) )
                {
                    str++;
                }
                *p++ = *str++;
            }
            *p = '\0';

            E_(SSPush)( st, psz_dest );
            free( psz_src );
            free( psz_dest );
        }
        else if( !strcmp( s, "htmlspecialchars" ) )
        {
            char *psz_src = E_(SSPop)( st );
            char *psz_dest;

            psz_dest = convert_xml_special_chars( psz_src );

            E_(SSPush)( st, psz_dest );
            free( psz_src );
            free( psz_dest );
        }
        else if( !strcmp( s, "realpath" ) )
        {
            char *psz_src = E_(SSPop)( st );
            char *psz_dir = E_(RealPath)( p_intf, psz_src );

            E_(SSPush)( st, psz_dir );
            free( psz_src );
            free( psz_dir );
        }
        /* 4. stack functions */
        else if( !strcmp( s, "dup" ) )
        {
            char *str = E_(SSPop)( st );
            E_(SSPush)( st, str );
            E_(SSPush)( st, str );
            free( str );
        }
        else if( !strcmp( s, "drop" ) )
        {
            char *str = E_(SSPop)( st );
            free( str );
        }
        else if( !strcmp( s, "swap" ) )
        {
            char *s1 = E_(SSPop)( st );
            char *s2 = E_(SSPop)( st );

            E_(SSPush)( st, s1 );
            E_(SSPush)( st, s2 );
            free( s1 );
            free( s2 );
        }
        else if( !strcmp( s, "flush" ) )
        {
            E_(SSClean)( st );
            E_(SSInit)( st );
        }
        else if( !strcmp( s, "store" ) )
        {
            char *value = E_(SSPop)( st );
            char *name  = E_(SSPop)( st );

            E_(mvar_PushNewVar)( vars, name, value );
            free( name );
            free( value );
        }
        else if( !strcmp( s, "value" ) )
        {
            char *name  = E_(SSPop)( st );
            char *value = E_(mvar_GetValue)( vars, name );

            E_(SSPush)( st, value );

            free( name );
        }
        /* 5. player control */
        else if( !strcmp( s, "vlc_play" ) )
        {
            int i_id = E_(SSPopN)( st, vars );
            int i_ret;

            i_ret = playlist_Control( p_sys->p_playlist, PLAYLIST_ITEMPLAY,
                                      playlist_ItemGetById( p_sys->p_playlist,
                                      i_id ) );
            msg_Dbg( p_intf, "requested playlist item: %i", i_id );
            E_(SSPushN)( st, i_ret );
        }
        else if( !strcmp( s, "vlc_stop" ) )
        {
            playlist_Control( p_sys->p_playlist, PLAYLIST_STOP );
            msg_Dbg( p_intf, "requested playlist stop" );
        }
        else if( !strcmp( s, "vlc_pause" ) )
        {
            playlist_Control( p_sys->p_playlist, PLAYLIST_PAUSE );
            msg_Dbg( p_intf, "requested playlist pause" );
        }
        else if( !strcmp( s, "vlc_next" ) )
        {
            playlist_Control( p_sys->p_playlist, PLAYLIST_SKIP, 1 );
            msg_Dbg( p_intf, "requested playlist next" );
        }
        else if( !strcmp( s, "vlc_previous" ) )
        {
            playlist_Control( p_sys->p_playlist, PLAYLIST_SKIP, -1 );
            msg_Dbg( p_intf, "requested playlist previous" );
        }
        else if( !strcmp( s, "vlc_seek" ) )
        {
            char *psz_value = E_(SSPop)( st );
            E_(HandleSeek)( p_intf, psz_value );
            msg_Dbg( p_intf, "requested playlist seek: %s", psz_value );
            free( psz_value );
        }
        else if( !strcmp( s, "vlc_var_type" )
                  || !strcmp( s, "vlc_config_type" ) )
        {
            vlc_object_t *p_object;
            const char *psz_type = NULL;
            int i_type = 0;

            if( !strcmp( s, "vlc_var_type" ) )
            {
                char *psz_object = E_(SSPop)( st );
                char *psz_variable = E_(SSPop)( st );
                vlc_bool_t b_need_release;

                p_object = GetVLCObject( p_intf, psz_object, &b_need_release );

                if( p_object != NULL )
                    i_type = var_Type( p_object, psz_variable );
                free( psz_variable );
                free( psz_object );
                if( b_need_release && p_object != NULL )
                    vlc_object_release( p_object );
            }
            else
            {
                char *psz_variable = E_(SSPop)( st );
                p_object = VLC_OBJECT(p_intf);
                i_type = config_GetType( p_object, psz_variable );
                free( psz_variable );
            }

            if( p_object != NULL )
            {
                switch( i_type & VLC_VAR_TYPE )
                {
                case VLC_VAR_BOOL:
                    psz_type = "VLC_VAR_BOOL";
                    break;
                case VLC_VAR_INTEGER:
                    psz_type = "VLC_VAR_INTEGER";
                    break;
                case VLC_VAR_HOTKEY:
                    psz_type = "VLC_VAR_HOTKEY";
                    break;
                case VLC_VAR_STRING:
                    psz_type = "VLC_VAR_STRING";
                    break;
                case VLC_VAR_MODULE:
                    psz_type = "VLC_VAR_MODULE";
                    break;
                case VLC_VAR_FILE:
                    psz_type = "VLC_VAR_FILE";
                    break;
                case VLC_VAR_DIRECTORY:
                    psz_type = "VLC_VAR_DIRECTORY";
                    break;
                case VLC_VAR_VARIABLE:
                    psz_type = "VLC_VAR_VARIABLE";
                    break;
                case VLC_VAR_FLOAT:
                    psz_type = "VLC_VAR_FLOAT";
                    break;
                default:
                    psz_type = "UNDEFINED";
                }
            }
            else
                psz_type = "INVALID";

            E_(SSPush)( st, psz_type );
        }
        else if( !strcmp( s, "vlc_var_set" ) )
        {
            char *psz_object = E_(SSPop)( st );
            char *psz_variable = E_(SSPop)( st );
            vlc_bool_t b_need_release;

            vlc_object_t *p_object = GetVLCObject( p_intf, psz_object,
                                                   &b_need_release );

            if( p_object != NULL )
            {
                vlc_bool_t b_error = VLC_FALSE;
                char *psz_value = NULL;
                vlc_value_t val;
                int i_type;

                i_type = var_Type( p_object, psz_variable );

                switch( i_type & VLC_VAR_TYPE )
                {
                case VLC_VAR_BOOL:
                    val.b_bool = E_(SSPopN)( st, vars );
                    msg_Dbg( p_intf, "requested %s var change: %s->%d",
                             psz_object, psz_variable, val.b_bool );
                    break;
                case VLC_VAR_INTEGER:
                case VLC_VAR_HOTKEY:
                    val.i_int = E_(SSPopN)( st, vars );
                    msg_Dbg( p_intf, "requested %s var change: %s->%d",
                             psz_object, psz_variable, val.i_int );
                    break;
                case VLC_VAR_STRING:
                case VLC_VAR_MODULE:
                case VLC_VAR_FILE:
                case VLC_VAR_DIRECTORY:
                case VLC_VAR_VARIABLE:
                    val.psz_string = psz_value = E_(SSPop)( st );
                    msg_Dbg( p_intf, "requested %s var change: %s->%s",
                             psz_object, psz_variable, psz_value );
                    break;
                case VLC_VAR_FLOAT:
                    psz_value = E_(SSPop)( st );
                    val.f_float = atof( psz_value );
                    msg_Dbg( p_intf, "requested %s var change: %s->%f",
                             psz_object, psz_variable, val.f_float );
                    break;
                default:
                    E_(SSPopN)( st, vars );
                    msg_Warn( p_intf, "invalid %s variable type %d (%s)",
                              psz_object, i_type & VLC_VAR_TYPE, psz_variable );
                    b_error = VLC_TRUE;
                }

                if( !b_error )
                    var_Set( p_object, psz_variable, val );
                if( psz_value != NULL )
                    free( psz_value );
            }
            else
                msg_Warn( p_intf, "vlc_var_set called without an object" );
            free( psz_variable );
            free( psz_object );

            if( b_need_release && p_object != NULL )
                vlc_object_release( p_object );
        }
        else if( !strcmp( s, "vlc_var_get" ) )
        {
            char *psz_object = E_(SSPop)( st );
            char *psz_variable = E_(SSPop)( st );
            vlc_bool_t b_need_release;

            vlc_object_t *p_object = GetVLCObject( p_intf, psz_object,
                                                   &b_need_release );

            if( p_object != NULL )
            {
                vlc_value_t val;
                int i_type;

                i_type = var_Type( p_object, psz_variable );
                var_Get( p_object, psz_variable, &val );

                switch( i_type & VLC_VAR_TYPE )
                {
                case VLC_VAR_BOOL:
                    E_(SSPushN)( st, val.b_bool );
                    break;
                case VLC_VAR_INTEGER:
                case VLC_VAR_HOTKEY:
                    E_(SSPushN)( st, val.i_int );
                    break;
                case VLC_VAR_STRING:
                case VLC_VAR_MODULE:
                case VLC_VAR_FILE:
                case VLC_VAR_DIRECTORY:
                case VLC_VAR_VARIABLE:
                    E_(SSPush)( st, val.psz_string );
                    free( val.psz_string );
                    break;
                case VLC_VAR_FLOAT:
                {
                    char psz_value[20];
                    lldiv_t value = lldiv( val.f_float * 1000000, 1000000 );
                    snprintf( psz_value, sizeof(psz_value), I64Fd".%06u",
                                    value.quot, (unsigned int)value.rem );
                    E_(SSPush)( st, psz_value );
                    break;
                }
                default:
                    msg_Warn( p_intf, "invalid %s variable type %d (%s)",
                              psz_object, i_type & VLC_VAR_TYPE, psz_variable );
                    E_(SSPush)( st, "" );
                }
            }
            else
            {
                msg_Warn( p_intf, "vlc_var_get called without an object" );
                E_(SSPush)( st, "" );
            }
            free( psz_variable );
            free( psz_object );

            if( b_need_release && p_object != NULL )
                vlc_object_release( p_object );
        }
        else if( !strcmp( s, "vlc_object_exists" ) )
        {
            char *psz_object = E_(SSPop)( st );
            vlc_bool_t b_need_release;

            vlc_object_t *p_object = GetVLCObject( p_intf, psz_object,
                                                   &b_need_release );
            if( b_need_release && p_object != NULL )
                vlc_object_release( p_object );

            if( p_object != NULL )
                E_(SSPush)( st, "1" );
            else
                E_(SSPush)( st, "0" );
        }
        else if( !strcmp( s, "vlc_config_set" ) )
        {
            char *psz_variable = E_(SSPop)( st );
            int i_type = config_GetType( p_intf, psz_variable );

            switch( i_type & VLC_VAR_TYPE )
            {
            case VLC_VAR_BOOL:
            case VLC_VAR_INTEGER:
                config_PutInt( p_intf, psz_variable, E_(SSPopN)( st, vars ) );
                break;
            case VLC_VAR_STRING:
            case VLC_VAR_MODULE:
            case VLC_VAR_FILE:
            case VLC_VAR_DIRECTORY:
            {
                char *psz_string = E_(SSPop)( st );
                config_PutPsz( p_intf, psz_variable, psz_string );
                free( psz_string );
                break;
            }
            case VLC_VAR_FLOAT:
            {
                char *psz_string = E_(SSPop)( st );
                config_PutFloat( p_intf, psz_variable, atof(psz_string) );
                free( psz_string );
                break;
            }
            default:
                msg_Warn( p_intf, "vlc_config_set called on unknown var (%s)",
                          psz_variable );
            }
            free( psz_variable );
        }
        else if( !strcmp( s, "vlc_config_get" ) )
        {
            char *psz_variable = E_(SSPop)( st );
            int i_type = config_GetType( p_intf, psz_variable );

            switch( i_type & VLC_VAR_TYPE )
            {
            case VLC_VAR_BOOL:
            case VLC_VAR_INTEGER:
                E_(SSPushN)( st, config_GetInt( p_intf, psz_variable ) );
                break;
            case VLC_VAR_STRING:
            case VLC_VAR_MODULE:
            case VLC_VAR_FILE:
            case VLC_VAR_DIRECTORY:
            {
                char *psz_string = config_GetPsz( p_intf, psz_variable );
                E_(SSPush)( st, psz_string );
                free( psz_string );
                break;
            }
            case VLC_VAR_FLOAT:
            {
                char psz_string[20];
                lldiv_t value = lldiv( config_GetFloat( p_intf, psz_variable )
                                       * 1000000, 1000000 );
                snprintf( psz_string, sizeof(psz_string), I64Fd".%06u",
                          value.quot, (unsigned int)value.rem );
                E_(SSPush)( st, psz_string );
                break;
            }
            default:
                msg_Warn( p_intf, "vlc_config_get called on unknown var (%s)",
                          psz_variable );
            }
            free( psz_variable );
        }
        else if( !strcmp( s, "vlc_config_save" ) )
        {
            char *psz_module = E_(SSPop)( st );
            int i_result;

            if( !*psz_module )
            {
                free( psz_module );
                psz_module = NULL;
            }
            i_result = config_SaveConfigFile( p_intf, psz_module );

            if( psz_module != NULL )
                free( psz_module );
            E_(SSPushN)( st, i_result );
        }
        else if( !strcmp( s, "vlc_config_reset" ) )
        {
            config_ResetAll( p_intf );
        }
        /* 6. playlist functions */
        else if( !strcmp( s, "playlist_add" ) )
        {
            char *psz_name = E_(SSPop)( st );
            char *mrl = E_(SSPop)( st );
            char *tmp;
            input_item_t *p_input;
            int i_id;

            tmp = E_(ToUTF8)( p_intf, psz_name );
            free( psz_name );
            psz_name = tmp;
            tmp = E_(ToUTF8)( p_intf, mrl );
            free( mrl );
            mrl = tmp;

            if( !*psz_name )
            {
                p_input = E_(MRLParse)( p_intf, mrl, mrl );
            }
            else
            {
                p_input = E_(MRLParse)( p_intf, mrl, psz_name );
            }

            if( !p_input || !p_input->psz_uri || !*p_input->psz_uri )
            {
                i_id = VLC_EGENERIC;
                msg_Dbg( p_intf, "invalid requested mrl: %s", mrl );
            }
            else
            {
                i_id = playlist_PlaylistAddInput( p_sys->p_playlist, p_input,
                                         PLAYLIST_APPEND, PLAYLIST_END );
                msg_Dbg( p_intf, "requested mrl add: %s", mrl );
            }
            E_(SSPushN)( st, i_id );

            free( mrl );
            free( psz_name );
        }
        else if( !strcmp( s, "playlist_empty" ) )
        {
            playlist_LockClear( p_sys->p_playlist );
            msg_Dbg( p_intf, "requested playlist empty" );
        }
        else if( !strcmp( s, "playlist_delete" ) )
        {
            int i_id = E_(SSPopN)( st, vars );
            playlist_LockDelete( p_sys->p_playlist, i_id );
            msg_Dbg( p_intf, "requested playlist delete: %d", i_id );
        }
        else if( !strcmp( s, "playlist_move" ) )
        {
            /*int i_newpos =*/ E_(SSPopN)( st, vars );
            /*int i_pos =*/ E_(SSPopN)( st, vars );
            /* FIXME FIXME TODO TODO XXX XXX
            do not release before fixing this
            if ( i_pos < i_newpos )
            {
                playlist_Move( p_sys->p_playlist, i_pos, i_newpos + 1 );
            }
            else
            {
                playlist_Move( p_sys->p_playlist, i_pos, i_newpos );
            }
            msg_Dbg( p_intf, "requested to move playlist item %d to %d",
                     i_pos, i_newpos);
               FIXME FIXME TODO TODO XXX XXX */
            msg_Err( p_intf, "moving using indexes is obsolete. We need to update this function" );
        }
        else if( !strcmp( s, "playlist_sort" ) )
        {
            int i_order = E_(SSPopN)( st, vars );
            int i_sort = E_(SSPopN)( st, vars );
            i_order = i_order % 2;
            i_sort = i_sort % 9;
            /* FIXME FIXME TODO TODO XXX XXX
            do not release before fixing this
            playlist_RecursiveNodeSort(  p_sys->p_playlist,
                                         p_sys->p_playlist->p_general,
                                         i_sort, i_order );
            msg_Dbg( p_intf, "requested sort playlist by : %d in order : %d",
                     i_sort, i_order );
               FIXME FIXME TODO TODO XXX XXX */
            msg_Err( p_intf, "this needs to be fixed to use the new playlist framework" );
        }
        else if( !strcmp( s, "services_discovery_add" ) )
        {
            char *psz_sd = E_(SSPop)( st );
            playlist_ServicesDiscoveryAdd( p_sys->p_playlist, psz_sd );
            free( psz_sd );
        }
        else if( !strcmp( s, "services_discovery_remove" ) )
        {
            char *psz_sd = E_(SSPop)( st );
            playlist_ServicesDiscoveryRemove( p_sys->p_playlist, psz_sd );
            free( psz_sd );
        }
        else if( !strcmp( s, "services_discovery_is_loaded" ) )
        {
            char *psz_sd = E_(SSPop)( st );
            E_(SSPushN)( st,
            playlist_IsServicesDiscoveryLoaded( p_sys->p_playlist, psz_sd ) );
            free( psz_sd );
        }
        else if( !strcmp( s, "vlc_volume_set" ) )
        {
            char *psz_vol = E_(SSPop)( st );
            int i_value;
            audio_volume_t i_volume;
            aout_VolumeGet( p_intf, &i_volume );
            if( psz_vol[0] == '+' )
            {
                i_value = atoi( psz_vol );
                if( (i_volume + i_value) > AOUT_VOLUME_MAX )
                    aout_VolumeSet( p_intf, AOUT_VOLUME_MAX );
                else
                    aout_VolumeSet( p_intf, i_volume + i_value );
            }
            else if( psz_vol[0] == '-' )
            {
                i_value = atoi( psz_vol );
                if( (i_volume + i_value) < AOUT_VOLUME_MIN )
                    aout_VolumeSet( p_intf, AOUT_VOLUME_MIN );
                else
                    aout_VolumeSet( p_intf, i_volume + i_value );
            }
            else if( strstr( psz_vol, "%") != NULL )
            {
                i_value = atoi( psz_vol );
                if( i_value < 0 ) i_value = 0;
                if( i_value > 400 ) i_value = 400;
                aout_VolumeSet( p_intf, (i_value * (AOUT_VOLUME_MAX - AOUT_VOLUME_MIN))/400+AOUT_VOLUME_MIN);
            }
            else
            {
                i_value = atoi( psz_vol );
                if( i_value > AOUT_VOLUME_MAX ) i_value = AOUT_VOLUME_MAX;
                if( i_value < AOUT_VOLUME_MIN ) i_value = AOUT_VOLUME_MIN;
                aout_VolumeSet( p_intf, i_value );
            }
            aout_VolumeGet( p_intf, &i_volume );
            free( psz_vol );
        }
        else if( !strcmp( s, "vlc_get_meta" ) )
        {
            char *psz_meta = E_(SSPop)( st );
            char *psz_val = NULL;
            if( p_sys->p_input && p_sys->p_input->input.p_item )
            {
#define p_item  p_sys->p_input->input.p_item
                if( !strcmp( psz_meta, "ARTIST" ) )
                {
                    psz_val = vlc_input_item_GetInfo( p_item,
                                _(VLC_META_INFO_CAT), _(VLC_META_ARTIST) );
                }
                else if( !strcmp( psz_meta, "TITLE" ) )
                {
                    psz_val = vlc_input_item_GetInfo( p_item,
                                _(VLC_META_INFO_CAT), _(VLC_META_TITLE) );
                    if( psz_val == NULL )
                        psz_val == strdup( p_item->psz_name );
                }
                else if( !strcmp( psz_meta, "ALBUM" ) )
                {
                    psz_val = vlc_input_item_GetInfo( p_item,
                                _(VLC_META_INFO_CAT), _(VLC_META_COLLECTION) );
                }
                else if( !strcmp( psz_meta, "GENRE" ) )
                {
                    psz_val = vlc_input_item_GetInfo( p_item,
                                _(VLC_META_INFO_CAT), _(VLC_META_GENRE) );
                }
                else
                {
                    psz_val = vlc_input_item_GetInfo( p_item,
                                            _(VLC_META_INFO_CAT), psz_meta );
                }
#undef p_item
            }
            if( psz_val == NULL ) psz_val = strdup( "" );
            E_(SSPush)( st, psz_val );
            free( psz_meta );
            free( psz_val );
        }
        else if( !strcmp( s, "vlm_command" ) || !strcmp( s, "vlm_cmd" ) )
        {
            char *psz_elt;
            char *psz_cmd = strdup( "" );
            char *psz_error;
            vlm_message_t *vlm_answer;

            /* make sure that we have a vlm object */
            if( p_intf->p_sys->p_vlm == NULL )
                p_intf->p_sys->p_vlm = vlm_New( p_intf );


            /* vlm command uses the ';' delimiter
             * (else we can't know when to stop) */
            while( strcmp( psz_elt = E_(SSPop)( st ), "" )
                   && strcmp( psz_elt, ";" ) )
            {
                char *psz_buf =
                    (char *)malloc( strlen( psz_cmd ) + strlen( psz_elt ) + 2 );
                sprintf( psz_buf, "%s %s", psz_cmd, psz_elt );
                free( psz_cmd );
                free( psz_elt );
                psz_cmd = psz_buf;
            }

            msg_Dbg( p_intf, "executing vlm command: %s", psz_cmd );
            vlm_ExecuteCommand( p_intf->p_sys->p_vlm, psz_cmd, &vlm_answer );

            if( vlm_answer->psz_value == NULL )
            {
                psz_error = strdup( "" );
            }
            else
            {
                psz_error = malloc( strlen(vlm_answer->psz_name) +
                                    strlen(vlm_answer->psz_value) +
                                    strlen( " : ") + 1 );
                sprintf( psz_error , "%s : %s" , vlm_answer->psz_name,
                                                 vlm_answer->psz_value );
            }

            E_(mvar_AppendNewVar)( vars, "vlm_error", psz_error );
            /* this is kind of a duplicate but we need to have the message
             * without the command name for the "export" command */
            E_(mvar_AppendNewVar)( vars, "vlm_value", vlm_answer->psz_value );
            vlm_MessageDelete( vlm_answer );

            free( psz_cmd );
            free( psz_error );
        }
        else
        {
            E_(SSPush)( st, s );
        }
    }
}
