/*****************************************************************************
 * theme.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef THEME_HPP
#define THEME_HPP

#include "generic_bitmap.hpp"
#include "generic_font.hpp"
#include "generic_layout.hpp"
#include "popup.hpp"
#include "../src/window_manager.hpp"
#include "../commands/cmd_generic.hpp"
#include "../utils/bezier.hpp"
#include "../utils/variable.hpp"
#include "../utils/position.hpp"
#include "../controls/ctrl_generic.hpp"
#include <string>
#include <list>
#include <map>

class Builder;
class Interpreter;

/// Class storing the data of the current theme
class Theme: public SkinObject
{
private:
    friend class Builder;
    friend class Interpreter;
public:
    Theme( intf_thread_t *pIntf ): SkinObject( pIntf ),
        m_windowManager( getIntf() ) { }
    virtual ~Theme();

    void loadConfig();
    int readConfig();
    void saveConfig();
    void applyConfig();

    GenericBitmap *getBitmapById( const std::string &id ) const;
    GenericFont *getFontById( const std::string &id ) const;

#   define ObjByID( var ) ( const std::string &id ) const \
        { return var.find_object( id ); }
    Popup         *getPopupById    ObjByID( m_popups    )
    TopWindow     *getWindowById   ObjByID( m_windows   )
    GenericLayout *getLayoutById   ObjByID( m_layouts   )
    CtrlGeneric   *getControlById  ObjByID( m_controls  )
    Position      *getPositionById ObjByID( m_positions )
#   undef  ObjById

    WindowManager &getWindowManager() { return m_windowManager; }

private:
    template<class T> class IDmap: public std::map<std::string, T> {
    private:
        typedef typename std::map<std::string, T> parent;
    public:
        typename T::pointer find_object(const std::string &id) const
        {
            typename parent::const_iterator it = parent::find( id );
            return it!=parent::end() ? it->second.get() : NULL;
        }
        typename T::pointer find_first_object(const std::string &id) const;
    };

    struct save_t {
        TopWindow* win;
        GenericLayout* layout;
        int x;
        int y;
        int width;
        int height;
        int visible;
    };

    /// Store the bitmaps by ID
    IDmap<GenericBitmapPtr> m_bitmaps;
    /// Store the fonts by ID
    IDmap<GenericFontPtr> m_fonts;
    /// Store the popups by ID
    IDmap<PopupPtr> m_popups;
    /// Store the windows by ID
    IDmap<TopWindowPtr> m_windows;
    /// Store the layouts by ID
    IDmap<GenericLayoutPtr> m_layouts;
    /// Store the controls by ID
    IDmap<CtrlGenericPtr> m_controls;
    /// Store the panel positions by ID
    IDmap<PositionPtr> m_positions;
    /// Store the commands
    std::list<CmdGenericPtr> m_commands;
    /// Store the Bezier curves
    std::list<BezierPtr> m_curves;
    /// Store the variables
    std::list<VariablePtr> m_vars;
    /// List saved windows/layouts
    std::list<save_t> m_saved;

    WindowManager m_windowManager;
};


#endif
