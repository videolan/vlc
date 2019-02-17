/*****************************************************************************
 * cmd_resize.hpp
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

#ifndef CMD_RESIZE_HPP
#define CMD_RESIZE_HPP

#include "cmd_generic.hpp"

class WindowManager;
class GenericLayout;
class CtrlVideo;
struct vout_window_t;


/// Command to resize a layout
class CmdResize: public CmdGeneric
{
public:
    /// Resize the given layout
    CmdResize( intf_thread_t *pIntf, const WindowManager &rWindowManager,
               GenericLayout &rLayout, int width, int height );
    virtual ~CmdResize() { }
    virtual void execute();
    virtual std::string getType() const { return "resize"; }

private:
    const WindowManager &m_rWindowManager;
    GenericLayout &m_rLayout;
    int m_width, m_height;
};


/// Command to resize the vout window
class CmdResizeVout: public CmdGeneric
{
public:
    /// Resize the given layout
    CmdResizeVout( intf_thread_t *pIntf, struct vout_window_t* pWnd,
                   int width, int height );
    virtual ~CmdResizeVout() { }
    virtual void execute();
    virtual std::string getType() const { return "resize vout"; }

private:
    struct vout_window_t* m_pWnd;
    int m_width, m_height;
};


/// Command to toggle Fullscreen
class CmdSetFullscreen: public CmdGeneric
{
public:
    /// Resize the given layout
    CmdSetFullscreen( intf_thread_t *pIntf, struct vout_window_t* pWnd,
                      bool fullscreen );
    virtual ~CmdSetFullscreen() { }
    virtual void execute();
    virtual std::string getType() const { return "toggle fullscreen"; }

private:
    struct vout_window_t* m_pWnd;
    bool m_bFullscreen;
};
#endif
