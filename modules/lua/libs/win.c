/*****************************************************************************
 * win.c: Windows specific functions
 *****************************************************************************
 * Copyright (C) 2007-2012 the VideoLAN team
 *
 * Authors: Antoine Cellerier <dionoea at videolan tod org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_charset.h>

#include "../vlc.h"
#include "../libs.h"

#if !VLC_WINSTORE_APP

/* Based on modules/control/rc.c and include/vlc_interface.h */
static HANDLE GetConsole( lua_State *L )
{
    /* Get the file descriptor of the console input */
    HANDLE hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);
    if( hConsoleIn == INVALID_HANDLE_VALUE )
        luaL_error( L, "couldn't find user input handle" );
    return hConsoleIn;
}

#define MAX_LINE_LENGTH 1024

static bool ReadWin32( HANDLE *hConsoleIn, unsigned char *p_buffer, int *pi_size )
{
    INPUT_RECORD input_record;
    DWORD i_dw;

    // Prefer to fail early when there's not enough space to store a 4 bytes
    // UTF8 character. The function will be immediatly called again and we won't
    // lose an input
    while( *pi_size < MAX_LINE_LENGTH - 4 &&
           ReadConsoleInput( hConsoleIn, &input_record, 1, &i_dw ) )
    {
        if( input_record.EventType != KEY_EVENT ||
            !input_record.Event.KeyEvent.bKeyDown ||
            input_record.Event.KeyEvent.wVirtualKeyCode == VK_SHIFT ||
            input_record.Event.KeyEvent.wVirtualKeyCode == VK_CONTROL||
            input_record.Event.KeyEvent.wVirtualKeyCode == VK_MENU ||
            input_record.Event.KeyEvent.wVirtualKeyCode == VK_CAPITAL )
        {
            /* nothing interesting */
            continue;
        }
        if( input_record.Event.KeyEvent.uChar.AsciiChar == '\n' ||
            input_record.Event.KeyEvent.uChar.AsciiChar == '\r' )
        {
            putc( '\n', stdout );
            p_buffer[*pi_size] = '\n';
            (*pi_size)++;
            break;
        }
        switch( input_record.Event.KeyEvent.uChar.AsciiChar )
        {
        case '\b':
            if ( *pi_size == 0 )
                break;
            if ( *pi_size > 1 && (p_buffer[*pi_size - 1] & 0xC0) == 0x80 )
            {
                // pi_size currently points to the character to be written, so
                // we need to roll back from 2 bytes to start erasing the previous
                // character
                (*pi_size) -= 2;
                unsigned int nbBytes = 1;
                while( *pi_size > 0 && (p_buffer[*pi_size] & 0xC0) == 0x80 )
                {
                    (*pi_size)--;
                    nbBytes++;
                }
                assert( clz( (unsigned char)~(p_buffer[*pi_size]) ) == nbBytes + 1 );
                // The first utf8 byte will be overriden by a \0
            }
            else
                (*pi_size)--;
            p_buffer[*pi_size] = 0;

            fputs( "\b \b", stdout );
            break;
        default:
        {
            WCHAR psz_winput[] = { input_record.Event.KeyEvent.uChar.UnicodeChar, L'\0' };
            char* psz_input = FromWide( psz_winput );
            int input_size = strlen(psz_input);
            if ( *pi_size + input_size > MAX_LINE_LENGTH )
            {
                p_buffer[ *pi_size ] = 0;
                return false;
            }
            strcpy( (char*)&p_buffer[*pi_size], psz_input );
            utf8_fprintf( stdout, "%s", psz_input );
            free(psz_input);
            *pi_size += input_size;
        }
        }
    }

    p_buffer[ *pi_size ] = 0;
    return true;
}

static int vlclua_console_init( lua_State *L )
{
    (void)L;
    //if ( !AllocConsole() )
    //    luaL_error( L, "failed to allocate windows console" );
    AllocConsole();

    freopen( "CONOUT$", "w", stdout );
    freopen( "CONOUT$", "w", stderr );
    freopen( "CONIN$", "r", stdin );
    return 0;
}

static int vlclua_console_wait( lua_State *L )
{
    int i_timeout = (int)luaL_optinteger( L, 1, 0 );
    DWORD status = WaitForSingleObject( GetConsole( L ), i_timeout );
    lua_pushboolean( L, status == WAIT_OBJECT_0 );
    return 1;
}


static int vlclua_console_read( lua_State *L )
{
    char psz_buffer[MAX_LINE_LENGTH+1];
    int i_size = 0;
    ReadWin32( GetConsole( L ), (unsigned char*)psz_buffer, &i_size );
    lua_pushlstring( L, psz_buffer, i_size );

    return 1;
}

static int vlclua_console_write( lua_State *L )
{
    if( !lua_isstring( L, 1 ) )
        return luaL_error( L, "win.console_write usage: (text)" );
    const char* psz_line = luaL_checkstring( L, 1 );
    utf8_fprintf( stdout, "%s", psz_line );
    return 0;
}

/*****************************************************************************
 *
 *****************************************************************************/
static const luaL_Reg vlclua_win_reg[] = {
    { "console_init", vlclua_console_init },
    { "console_wait", vlclua_console_wait },
    { "console_read", vlclua_console_read },
    { "console_write", vlclua_console_write },
    { NULL, NULL }
};

void luaopen_win( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_win_reg );
    lua_setfield( L, -2, "win" );
}

#endif /* !VLC_WINSTORE_APP */
