/*****************************************************************************
 * skin_common.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN (Centrale RÃ©seaux) and its contributors
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef SKIN_COMMON_HPP
#define SKIN_COMMON_HPP

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include <string>
using namespace std;

class AsyncQueue;
class Logger;
class Dialogs;
class Interpreter;
class OSFactory;
class OSLoop;
class VarManager;
class VlcProc;
class Theme;
class ThemeRepository;

#ifndef M_PI
#   define M_PI 3.14159265358979323846
#endif

// Useful macros
#define SKINS_DELETE( p ) \
   if( p ) \
   { \
       delete p; \
   } \
   else \
   { \
       msg_Err( getIntf(), "delete NULL pointer in %s at line %d", \
                __FILE__, __LINE__ ); \
   }


//---------------------------------------------------------------------------
// intf_sys_t: description and status of skin interface
//---------------------------------------------------------------------------
struct intf_sys_t
{
    /// The input thread
    input_thread_t *p_input;

    /// The playlist thread
    playlist_t *p_playlist;

    /// Message bank subscription
    msg_subscription_t *p_sub;

    // "Singleton" objects: MUST be initialized to NULL !
    /// Logger
    Logger *p_logger;
    /// Asynchronous command queue
    AsyncQueue *p_queue;
    /// Dialog provider
    Dialogs *p_dialogs;
    /// Script interpreter
    Interpreter *p_interpreter;
    /// Factory for OS specific classes
    OSFactory *p_osFactory;
    /// Main OS specific message loop
    OSLoop *p_osLoop;
    /// Variable manager
    VarManager *p_varManager;
    /// VLC state handler
    VlcProc *p_vlcProc;
    /// Theme repository
    ThemeRepository *p_repository;

    /// Current theme
    Theme *p_theme;
};


/// Base class for all skin classes
class SkinObject
{
    public:
        SkinObject( intf_thread_t *pIntf ): m_pIntf( pIntf ) {}
        virtual ~SkinObject() {}

        /// Getter (public because it is used in C callbacks in the win32
        /// interface)
        intf_thread_t *getIntf() const { return m_pIntf; }

        /// Class for callbacks
        class Callback {
            public:
                /// Type for callback methods
                typedef void (*CallbackFunc_t)( SkinObject* );

                /// Create a callback with the given object and function
                Callback( SkinObject *pObj, CallbackFunc_t pFunc ):
                    m_pObj( pObj ), m_pFunc( pFunc ) {}
                ~Callback() {}

                /// Getters
                SkinObject *getObj() const { return m_pObj; }
                CallbackFunc_t getFunc() const { return m_pFunc; }

            private:
                /// Pointer on the callback object
                SkinObject *const m_pObj;
                /// Pointer on the callback method
                CallbackFunc_t m_pFunc;
        };

    private:
        intf_thread_t *m_pIntf;
};


#endif
