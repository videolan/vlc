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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "http.h"
#include "vlc_url.h"
#include "vlc_meta.h"
#include "vlc_strings.h"

static vlc_object_t *GetVLCObject( intf_thread_t *p_intf,
                                   const char *psz_object,
                                   bool *pb_need_release )
{
    intf_sys_t    *p_sys = p_intf->p_sys;
    vlc_object_t *p_object = NULL;
    *pb_need_release = false;

    if( !strcmp( psz_object, "VLC_OBJECT_LIBVLC" ) )
        p_object = VLC_OBJECT(p_intf->p_libvlc);
    else if( !strcmp( psz_object, "VLC_OBJECT_PLAYLIST" ) )
        p_object = VLC_OBJECT(p_sys->p_playlist);
    else if( !strcmp( psz_object, "VLC_OBJECT_INPUT" ) )
        p_object = VLC_OBJECT(p_sys->p_input);
    else if( p_sys->p_input )
    {
        if( !strcmp( psz_object, "VLC_OBJECT_VOUT" ) )
            p_object = VLC_OBJECT( input_GetVout( p_sys->p_input ) );
        else if( !strcmp( psz_object, "VLC_OBJECT_AOUT" ) )
            p_object = VLC_OBJECT( input_GetAout( p_sys->p_input ) );
        if( p_object )
            *pb_need_release = true;
    }
    else
        msg_Warn( p_intf, "unknown object type (%s)", psz_object );

    return p_object;
}

void SSInit( rpn_stack_t *st )
{
    st->i_stack = 0;
}

void SSClean( rpn_stack_t *st )
{
    while( st->i_stack > 0 )
    {
        free( st->stack[--st->i_stack] );
    }
}

void SSPush( rpn_stack_t *st, const char *s )
{
    if( st->i_stack < STACK_MAX )
    {
        st->stack[st->i_stack++] = strdup( s );
    }
}

char *SSPop( rpn_stack_t *st )
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

int SSPopN( rpn_stack_t *st, mvar_t  *vars )
{
    char *name;

    char *end;
    int  i;

    name = SSPop( st );
    i = strtol( name, &end, 0 );
    if( end == name )
    {
        const char *value = mvar_GetValue( vars, name );
        i = atoi( value );
    }
    free( name );

    return( i );
}

void SSPushN( rpn_stack_t *st, int i )
{
    char v[12];

    snprintf( v, sizeof (v), "%d", i );
    SSPush( st, v );
}

void EvaluateRPN( intf_thread_t *p_intf, mvar_t  *vars,
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
            p = FirstWord( exp, exp );
            SSPush( st, exp );
            exp = p;
            continue;
        }

        /* extract token */
        p = FirstWord( exp, exp );
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
            SSPushN( st, ~SSPopN( st, vars ) );
        }
        else if( !strcmp( s, "^" ) )
        {
            SSPushN( st, SSPopN( st, vars ) ^ SSPopN( st, vars ) );
        }
        else if( !strcmp( s, "&" ) )
        {
            SSPushN( st, SSPopN( st, vars ) & SSPopN( st, vars ) );
        }
        else if( !strcmp( s, "|" ) )
        {
            SSPushN( st, SSPopN( st, vars ) | SSPopN( st, vars ) );
        }
        else if( !strcmp( s, "+" ) )
        {
            SSPushN( st, SSPopN( st, vars ) + SSPopN( st, vars ) );
        }
        else if( !strcmp( s, "-" ) )
        {
            int j = SSPopN( st, vars );
            int i = SSPopN( st, vars );
            SSPushN( st, i - j );
        }
        else if( !strcmp( s, "*" ) )
        {
            SSPushN( st, SSPopN( st, vars ) * SSPopN( st, vars ) );
        }
        else if( !strcmp( s, "/" ) )
        {
            int i, j;

            j = SSPopN( st, vars );
            i = SSPopN( st, vars );

            SSPushN( st, j != 0 ? i / j : 0 );
        }
        else if( !strcmp( s, "%" ) )
        {
            int i, j;

            j = SSPopN( st, vars );
            i = SSPopN( st, vars );

            SSPushN( st, j != 0 ? i % j : 0 );
        }
        /* 2. integer tests */
        else if( !strcmp( s, "=" ) )
        {
            SSPushN( st, SSPopN( st, vars ) == SSPopN( st, vars ) ? -1 : 0 );
        }
        else if( !strcmp( s, "!=" ) )
        {
            SSPushN( st, SSPopN( st, vars ) != SSPopN( st, vars ) ? -1 : 0 );
        }
        else if( !strcmp( s, "<" ) )
        {
            int j = SSPopN( st, vars );
            int i = SSPopN( st, vars );

            SSPushN( st, i < j ? -1 : 0 );
        }
        else if( !strcmp( s, ">" ) )
        {
            int j = SSPopN( st, vars );
            int i = SSPopN( st, vars );

            SSPushN( st, i > j ? -1 : 0 );
        }
        else if( !strcmp( s, "<=" ) )
        {
            int j = SSPopN( st, vars );
            int i = SSPopN( st, vars );

            SSPushN( st, i <= j ? -1 : 0 );
        }
        else if( !strcmp( s, ">=" ) )
        {
            int j = SSPopN( st, vars );
            int i = SSPopN( st, vars );

            SSPushN( st, i >= j ? -1 : 0 );
        }
        /* 3. string functions */
        else if( !strcmp( s, "strcat" ) )
        {
            char *s2 = SSPop( st );
            char *s1 = SSPop( st );
            char *str = malloc( strlen( s1 ) + strlen( s2 ) + 1 );

            strcpy( str, s1 );
            strcat( str, s2 );

            SSPush( st, str );
            free( s1 );
            free( s2 );
            free( str );
        }
        else if( !strcmp( s, "strcmp" ) )
        {
            char *s2 = SSPop( st );
            char *s1 = SSPop( st );

            SSPushN( st, strcmp( s1, s2 ) );
            free( s1 );
            free( s2 );
        }
        else if( !strcmp( s, "strncmp" ) )
        {
            int n = SSPopN( st, vars );
            char *s2 = SSPop( st );
            char *s1 = SSPop( st );

            SSPushN( st, strncmp( s1, s2 , n ) );
            free( s1 );
            free( s2 );
        }
        else if( !strcmp( s, "strsub" ) )
        {
            int n = SSPopN( st, vars );
            int m = SSPopN( st, vars );
            int i_len;
            char *s = SSPop( st );
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

            SSPush( st, str );
            free( s );
            free( str );
        }
        else if( !strcmp( s, "strlen" ) )
        {
            char *str = SSPop( st );

            SSPushN( st, strlen( str ) );
            free( str );
        }
        else if( !strcmp( s, "str_replace" ) )
        {
            char *psz_to = SSPop( st );
            char *psz_from = SSPop( st );
            char *psz_in = SSPop( st );
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

            SSPush( st, psz_out );
            free( psz_to );
            free( psz_from );
            free( psz_in );
            free( psz_out );
        }
        else if( !strcmp( s, "url_extract" ) )
        {
            const char *url = mvar_GetValue( vars, "url_value" );
            char *name = SSPop( st );
            char *value = ExtractURIString( url, name );
            if( value != NULL )
            {
                decode_URI( value );
                SSPush( st, value );
                free( value );
            }
            else
                SSPush( st, "" );

            free( name );
        }
        else if( !strcmp( s, "url_encode" ) )
        {
            char *url = SSPop( st );
            char *value = vlc_UrlEncode( url );
            free( url );
            SSPush( st, value );
            free( value );
        }
        else if( !strcmp( s, "xml_encode" )
              || !strcmp( s, "htmlspecialchars" ) )
        {
            char *url = SSPop( st );
            char *value = convert_xml_special_chars( url );
            free( url );
            SSPush( st, value );
            free( value );
        }
        else if( !strcmp( s, "addslashes" ) )
        {
            char *psz_src = SSPop( st );
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

            SSPush( st, psz_dest );
            free( psz_src );
            free( psz_dest );
        }
        else if( !strcmp( s, "stripslashes" ) )
        {
            char *psz_src = SSPop( st );
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

            SSPush( st, psz_dest );
            free( psz_src );
            free( psz_dest );
        }
        else if( !strcmp( s, "realpath" ) )
        {
            char *psz_src = SSPop( st );
            char *psz_dir = RealPath( psz_src );

            SSPush( st, psz_dir );
            free( psz_src );
            free( psz_dir );
        }
        /* 4. stack functions */
        else if( !strcmp( s, "dup" ) )
        {
            char *str = SSPop( st );
            SSPush( st, str );
            SSPush( st, str );
            free( str );
        }
        else if( !strcmp( s, "drop" ) )
        {
            char *str = SSPop( st );
            free( str );
        }
        else if( !strcmp( s, "swap" ) )
        {
            char *s1 = SSPop( st );
            char *s2 = SSPop( st );

            SSPush( st, s1 );
            SSPush( st, s2 );
            free( s1 );
            free( s2 );
        }
        else if( !strcmp( s, "flush" ) )
        {
            SSClean( st );
            SSInit( st );
        }
        else if( !strcmp( s, "store" ) )
        {
            char *value = SSPop( st );
            char *name  = SSPop( st );

            mvar_PushNewVar( vars, name, value );
            free( name );
            free( value );
        }
        else if( !strcmp( s, "value" ) )
        {
            char *name  = SSPop( st );
            const char *value = mvar_GetValue( vars, name );

            SSPush( st, value );

            free( name );
        }
        /* 5. player control */
        else if( !strcmp( s, "vlc_play" ) )
        {
            int i_id = SSPopN( st, vars );
            int i_ret;

            playlist_Lock( p_sys->p_playlist );
            i_ret = playlist_Control( p_sys->p_playlist, PLAYLIST_VIEWPLAY,
                                      pl_Locked, NULL,
                                      playlist_ItemGetById( p_sys->p_playlist,
                                      i_id ) );
            playlist_Unlock( p_sys->p_playlist );
            msg_Dbg( p_intf, "requested playlist item: %i", i_id );
            SSPushN( st, i_ret );
        }
        else if( !strcmp( s, "vlc_stop" ) )
        {
            playlist_Control( p_sys->p_playlist, PLAYLIST_STOP, pl_Unlocked );
            msg_Dbg( p_intf, "requested playlist stop" );
        }
        else if( !strcmp( s, "vlc_pause" ) )
        {
            playlist_Control( p_sys->p_playlist, PLAYLIST_PAUSE, pl_Unlocked );
            msg_Dbg( p_intf, "requested playlist pause" );
        }
        else if( !strcmp( s, "vlc_next" ) )
        {
            playlist_Control( p_sys->p_playlist, PLAYLIST_SKIP, pl_Unlocked, 1 );
            msg_Dbg( p_intf, "requested playlist next" );
        }
        else if( !strcmp( s, "vlc_previous" ) )
        {
            playlist_Control( p_sys->p_playlist, PLAYLIST_SKIP, pl_Unlocked, -1 );
            msg_Dbg( p_intf, "requested playlist previous" );
        }
        else if( !strcmp( s, "vlc_seek" ) )
        {
            char *psz_value = SSPop( st );
            HandleSeek( p_intf, psz_value );
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
                char *psz_object = SSPop( st );
                char *psz_variable = SSPop( st );
                bool b_need_release;

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
                char *psz_variable = SSPop( st );
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

            SSPush( st, psz_type );
        }
        else if( !strcmp( s, "vlc_var_set" ) )
        {
            char *psz_object = SSPop( st );
            char *psz_variable = SSPop( st );
            bool b_need_release;

            vlc_object_t *p_object = GetVLCObject( p_intf, psz_object,
                                                   &b_need_release );

            if( p_object != NULL )
            {
                bool b_error = false;
                char *psz_value = NULL;
                vlc_value_t val;
                int i_type;

                i_type = var_Type( p_object, psz_variable );

                switch( i_type & VLC_VAR_TYPE )
                {
                case VLC_VAR_BOOL:
                    val.b_bool = SSPopN( st, vars );
                    msg_Dbg( p_intf, "requested %s var change: %s->%d",
                             psz_object, psz_variable, val.b_bool );
                    break;
                case VLC_VAR_INTEGER:
                case VLC_VAR_HOTKEY:
                    val.i_int = SSPopN( st, vars );
                    msg_Dbg( p_intf, "requested %s var change: %s->%d",
                             psz_object, psz_variable, val.i_int );
                    break;
                case VLC_VAR_STRING:
                case VLC_VAR_MODULE:
                case VLC_VAR_FILE:
                case VLC_VAR_DIRECTORY:
                case VLC_VAR_VARIABLE:
                    val.psz_string = psz_value = SSPop( st );
                    msg_Dbg( p_intf, "requested %s var change: %s->%s",
                             psz_object, psz_variable, psz_value );
                    break;
                case VLC_VAR_FLOAT:
                    psz_value = SSPop( st );
                    val.f_float = atof( psz_value );
                    msg_Dbg( p_intf, "requested %s var change: %s->%f",
                             psz_object, psz_variable, val.f_float );
                    break;
                default:
                    SSPopN( st, vars );
                    msg_Warn( p_intf, "invalid %s variable type %d (%s)",
                              psz_object, i_type & VLC_VAR_TYPE, psz_variable );
                    b_error = true;
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
            char *psz_object = SSPop( st );
            char *psz_variable = SSPop( st );
            bool b_need_release;

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
                    SSPushN( st, val.b_bool );
                    break;
                case VLC_VAR_INTEGER:
                case VLC_VAR_HOTKEY:
                    SSPushN( st, val.i_int );
                    break;
                case VLC_VAR_STRING:
                case VLC_VAR_MODULE:
                case VLC_VAR_FILE:
                case VLC_VAR_DIRECTORY:
                case VLC_VAR_VARIABLE:
                    SSPush( st, val.psz_string );
                    free( val.psz_string );
                    break;
                case VLC_VAR_FLOAT:
                {
                    char psz_value[20];
                    lldiv_t value = lldiv( val.f_float * 1000000, 1000000 );
                    snprintf( psz_value, sizeof(psz_value), "%lld.%06u",
                                    value.quot, (unsigned int)value.rem );
                    SSPush( st, psz_value );
                    break;
                }
                default:
                    msg_Warn( p_intf, "invalid %s variable type %d (%s)",
                              psz_object, i_type & VLC_VAR_TYPE, psz_variable );
                    SSPush( st, "" );
                }
            }
            else
            {
                msg_Warn( p_intf, "vlc_var_get called without an object" );
                SSPush( st, "" );
            }
            free( psz_variable );
            free( psz_object );

            if( b_need_release && p_object != NULL )
                vlc_object_release( p_object );
        }
        else if( !strcmp( s, "vlc_object_exists" ) )
        {
            char *psz_object = SSPop( st );
            bool b_need_release;

            vlc_object_t *p_object = GetVLCObject( p_intf, psz_object,
                                                   &b_need_release );
            if( b_need_release && p_object != NULL )
                vlc_object_release( p_object );

            if( p_object != NULL )
                SSPush( st, "1" );
            else
                SSPush( st, "0" );
        }
        else if( !strcmp( s, "vlc_config_set" ) )
        {
            char *psz_variable = SSPop( st );
            int i_type = config_GetType( p_intf, psz_variable );

            switch( i_type & VLC_VAR_TYPE )
            {
            case VLC_VAR_BOOL:
            case VLC_VAR_INTEGER:
                config_PutInt( p_intf, psz_variable, SSPopN( st, vars ) );
                break;
            case VLC_VAR_STRING:
            case VLC_VAR_MODULE:
            case VLC_VAR_FILE:
            case VLC_VAR_DIRECTORY:
            {
                char *psz_string = SSPop( st );
                config_PutPsz( p_intf, psz_variable, psz_string );
                free( psz_string );
                break;
            }
            case VLC_VAR_FLOAT:
            {
                char *psz_string = SSPop( st );
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
            char *psz_variable = SSPop( st );
            int i_type = config_GetType( p_intf, psz_variable );

            switch( i_type & VLC_VAR_TYPE )
            {
            case VLC_VAR_BOOL:
            case VLC_VAR_INTEGER:
                SSPushN( st, config_GetInt( p_intf, psz_variable ) );
                break;
            case VLC_VAR_STRING:
            case VLC_VAR_MODULE:
            case VLC_VAR_FILE:
            case VLC_VAR_DIRECTORY:
            {
                char *psz_string = config_GetPsz( p_intf, psz_variable );
                SSPush( st, psz_string );
                free( psz_string );
                break;
            }
            case VLC_VAR_FLOAT:
            {
                char psz_string[20];
                lldiv_t value = lldiv( config_GetFloat( p_intf, psz_variable )
                                       * 1000000, 1000000 );
                snprintf( psz_string, sizeof(psz_string), "%lld.%06u",
                          value.quot, (unsigned int)value.rem );
                SSPush( st, psz_string );
                break;
            }
            default:
                msg_Warn( p_intf, "vlc_config_get called on unknown var (%s)",
                          psz_variable );
                SSPush( st, "" );
            }
            free( psz_variable );
        }
        else if( !strcmp( s, "vlc_config_save" ) )
        {
            char *psz_module = SSPop( st );
            int i_result;

            if( !*psz_module )
            {
                free( psz_module );
                psz_module = NULL;
            }
            i_result = config_SaveConfigFile( p_intf, psz_module );

            if( psz_module != NULL )
                free( psz_module );
            SSPushN( st, i_result );
        }
        else if( !strcmp( s, "vlc_config_reset" ) )
        {
            config_ResetAll( p_intf );
        }
        /* 6. playlist functions */
        else if( !strcmp( s, "playlist_add" ) )
        {
            char *psz_name = SSPop( st );
            char *mrl = SSPop( st );
            input_item_t *p_input;
            int i_ret;

            p_input = MRLParse( p_intf, mrl, psz_name );

            char *psz_uri = input_item_GetURI( p_input );
            if( !p_input || !psz_uri || !*psz_uri )
            {
                i_ret = VLC_EGENERIC;
                msg_Dbg( p_intf, "invalid requested mrl: %s", mrl );
            }
            else
            {
                i_ret = playlist_AddInput( p_sys->p_playlist, p_input,
                                   PLAYLIST_APPEND, PLAYLIST_END, true,
                                   pl_Unlocked );
                if( i_ret == VLC_SUCCESS )
                {
                    playlist_item_t *p_item;
                    msg_Dbg( p_intf, "requested mrl add: %s", mrl );
                    playlist_Lock( p_sys->p_playlist );
                    p_item = playlist_ItemGetByInput( p_sys->p_playlist,
                                                      p_input );
                    if( p_item )
                        i_ret = p_item->i_id;
                    playlist_Unlock( p_sys->p_playlist );
                }
                else
                    msg_Warn( p_intf, "adding mrl %s failed", mrl );
                vlc_gc_decref( p_input );
            }
            free( psz_uri );
            SSPushN( st, i_ret );

            free( mrl );
            free( psz_name );
        }
        else if( !strcmp( s, "playlist_empty" ) )
        {
            playlist_Clear( p_sys->p_playlist, pl_Unlocked );
            msg_Dbg( p_intf, "requested playlist empty" );
        }
        else if( !strcmp( s, "playlist_delete" ) )
        {
            int i_id = SSPopN( st, vars );
            playlist_Lock( p_sys->p_playlist );
            playlist_item_t *p_item = playlist_ItemGetById( p_sys->p_playlist,
                                                            i_id );
            if( p_item )
            {
                playlist_DeleteFromInput( p_sys->p_playlist,
                                          p_item->p_input->i_id, pl_Locked );
                msg_Dbg( p_intf, "requested playlist delete: %d", i_id );
            }
            else
            {
                msg_Dbg( p_intf, "couldn't find playlist item to delete (%d)",
                         i_id );
            }
            playlist_Unlock( p_sys->p_playlist );
        }
        else if( !strcmp( s, "playlist_move" ) )
        {
            /*int i_newpos =*/ SSPopN( st, vars );
            /*int i_pos =*/ SSPopN( st, vars );
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
            int i_order = SSPopN( st, vars );
            int i_sort = SSPopN( st, vars );
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
            char *psz_sd = SSPop( st );
            playlist_ServicesDiscoveryAdd( p_sys->p_playlist, psz_sd );
            free( psz_sd );
        }
        else if( !strcmp( s, "services_discovery_remove" ) )
        {
            char *psz_sd = SSPop( st );
            playlist_ServicesDiscoveryRemove( p_sys->p_playlist, psz_sd );
            free( psz_sd );
        }
        else if( !strcmp( s, "services_discovery_is_loaded" ) )
        {
            char *psz_sd = SSPop( st );
            SSPushN( st,
            playlist_IsServicesDiscoveryLoaded( p_sys->p_playlist, psz_sd ) );
            free( psz_sd );
        }
        else if( !strcmp( s, "vlc_volume_set" ) )
        {
            char *psz_vol = SSPop( st );
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
            char *psz_meta = SSPop( st );
            char *psz_val = NULL;
            if( p_sys->p_input && input_GetItem(p_sys->p_input) )
            {
#define p_item input_GetItem( p_sys->p_input )
                if( !strcmp( psz_meta, "ARTIST" ) )
                {
                    psz_val = input_item_GetArtist( p_item );
                }
                else if( !strcmp( psz_meta, "TITLE" ) )
                {
                    psz_val = input_item_GetTitle( p_item );
                    if( !psz_val )
                        psz_val = input_item_GetName( p_item );
                }
                else if( !strcmp( psz_meta, "ALBUM" ) )
                {
                    psz_val = input_item_GetAlbum( p_item );
                }
                else if( !strcmp( psz_meta, "GENRE" ) )
                {
                    psz_val = input_item_GetGenre( p_item );
                }
                else if( !strcmp( psz_meta, "COPYRIGHT" ) )
                {
                     psz_val = input_item_GetCopyright( p_item );
                }
                else if( !strcmp( psz_meta, "TRACK_NUMBER" ) )
                {
                    psz_val = input_item_GetTrackNum( p_item );
                }
                else if( !strcmp( psz_meta, "DESCRIPTION" ) )
                {
                    psz_val = input_item_GetDescription( p_item );
                }
                else if( !strcmp( psz_meta, "RATING" ) )
                {
                    psz_val = input_item_GetRating( p_item );
                }
                else if( !strcmp( psz_meta, "DATE" ) )
                {
                    psz_val = input_item_GetDate( p_item );
                }
                else if( !strcmp( psz_meta, "URL" ) )
                {
                    psz_val = input_item_GetURL( p_item );
                }
                else if( !strcmp( psz_meta, "LANGUAGE" ) )
                {
                    psz_val = input_item_GetLanguage( p_item );
                }
                else if( !strcmp( psz_meta, "NOW_PLAYING" ) )
                {
                    psz_val = input_item_GetNowPlaying( p_item );
                }
                else if( !strcmp( psz_meta, "PUBLISHER" ) )
                {
                    psz_val = input_item_GetPublisher( p_item );
                }
                else if( !strcmp( psz_meta, "ENCODED_BY" ) )
                {
                    psz_val = input_item_GetEncodedBy( p_item );
                }
                else if( !strcmp( psz_meta, "ART_URL" ) )
                {
                    psz_val = input_item_GetEncodedBy( p_item );
                }
                else if( !strcmp( psz_meta, "TRACK_ID" ) )
                {
                    psz_val = input_item_GetTrackID( p_item );
                }
#undef p_item
            }
            if( psz_val == NULL ) psz_val = strdup( "" );
            SSPush( st, psz_val );
            free( psz_meta );
            free( psz_val );
        }
#ifdef ENABLE_VLM
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
            while( strcmp( psz_elt = SSPop( st ), "" )
                   && strcmp( psz_elt, ";" ) )
            {
                char* psz_buf;
                if( asprintf( &psz_buf, "%s %s", psz_cmd, psz_elt ) == -1 )
                    psz_buf = NULL;
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
                if( asprintf( &psz_error , "%s : %s" , vlm_answer->psz_name,
                              vlm_answer->psz_value ) == -1 )
                    psz_error = NULL;
            }

            mvar_AppendNewVar( vars, "vlm_error", psz_error );
            /* this is kind of a duplicate but we need to have the message
             * without the command name for the "export" command */
            mvar_AppendNewVar( vars, "vlm_value", vlm_answer->psz_value );
            vlm_MessageDelete( vlm_answer );

            free( psz_cmd );
            free( psz_error );
        }
#endif /* ENABLE_VLM */
        else if( !strcmp( s, "snapshot" ) )
        {
            if( p_sys->p_input )
            {
                vout_thread_t *p_vout = input_GetVout( p_sys->p_input );
                if( p_vout )
                {
                    var_TriggerCallback( p_vout, "video-snapshot" );
                    vlc_object_release( p_vout );
                    msg_Dbg( p_intf, "requested snapshot" );
                }
            }
            break;

        }
        else
        {
            SSPush( st, s );
        }
    }
}
