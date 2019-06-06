/*****************************************************************************
 * window_manager.hpp
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

#ifndef WINDOW_MANAGER_HPP
#define WINDOW_MANAGER_HPP

#include "skin_common.hpp"
#include "top_window.hpp"
#include "../utils/position.hpp"
#include <list>
#include <map>
#include <set>
#include <utility>


class GenericFont;
class GenericLayout;
class Anchor;
class Tooltip;
class Popup;


/// Window manager for skin windows
class WindowManager: public SkinObject
{
public:
    /// Direction of the resizing
    enum Direction_t
    {
        kResizeE,   // East
        kResizeSE,  // South-East
        kResizeS,   // South
        kNone       // Reserved for internal use
    };
    WindowManager( intf_thread_t *pIntf );
    virtual ~WindowManager();

    /**
     * Add a window to the list of known windows. Necessary if you want
     * your window to be movable...
     */
    void registerWindow( TopWindow &rWindow );

    /// Remove a previously registered window
    void unregisterWindow( TopWindow &rWindow );

    /// Tell the window manager that a move is initiated for rWindow
    void startMove( TopWindow &rWindow );

    /// Tell the window manager that the current move ended
    void stopMove();

    /**
     * Move the rWindow window to (left, top), and move all its
     * anchored windows.
     * If a new anchoring is detected, the windows will move accordingly.
     */
    void move( TopWindow &rWindow, int left, int top ) const;

    /// Tell the window manager that a resize is initiated for rLayout
    void startResize( GenericLayout &rLayout, Direction_t direction );

    /// Tell the window manager that the current resizing ended
    void stopResize();

    /**
     * Resize the rLayout layout to (width, height), and move all its
     * anchored windows, if some anchors are moved during the resizing.
     * If a new anchoring is detected, the windows will move (or resize)
     * accordingly.
     */
    void resize( GenericLayout &rLayout, int width, int height ) const;

    /// Maximize the given window
    void maximize( TopWindow &rWindow );

    /// Unmaximize the given window
    void unmaximize( TopWindow &rWindow );

    /// Raise all the registered windows
    void raiseAll() const;

    /// Show all the registered windows
    void showAll( bool firstTime = false ) const;

    /// Hide all the registered windows
    void hideAll() const;

    /// Synchronize the windows with their visibility variable
    void synchVisibility() const;

    /// Save the current visibility of the windows
    void saveVisibility();

    /// Restore the saved visibility of the windows
    void restoreVisibility() const;

    /// Raise the given window
    void raise( TopWindow &rWindow ) const { rWindow.raise(); }

    /// Show the given window
    void show( TopWindow &rWindow ) const;

    /// Hide the given window
    void hide( TopWindow &rWindow ) const { rWindow.hide(); }

    /// Set/unset all the windows on top
    void setOnTop( bool b_ontop );

    /// Toggle all the windows on top
    void toggleOnTop();

    /// Set the magnetism of screen edges
    void setMagnetValue( int magnet ) { m_magnet = magnet; }

    /// Set the alpha value of the static windows
    void setAlphaValue( int alpha )
        { m_alpha = (m_opacity != 255) ? m_opacity : alpha; }

    /// Set the alpha value of the moving windows
    void setMoveAlphaValue( int moveAlpha )
        { m_moveAlpha = (m_opacity != 255) ? m_opacity : moveAlpha; }

    /// Create the tooltip window
    void createTooltip( const GenericFont &rTipFont );

    /// Show the tooltip window
    void showTooltip();

    /// Hide the tooltip window
    void hideTooltip();

    /// Add a layout of the given window. This new layout will be the
    /// active one.
    void addLayout( TopWindow &rWindow, GenericLayout &rLayout );

    /// Change the active layout of the given window
    void setActiveLayout( TopWindow &rWindow, GenericLayout &rLayout );

    /// Mark the given popup as active
    void setActivePopup( Popup &rPopup ) { m_pPopup = &rPopup; }

    /// Return the active popup, or NULL if none is active
    Popup * getActivePopup() const { return m_pPopup; }

    /// getter to know whether opacity is needed
    bool isOpacityNeeded() const
    { return (m_opacityEnabled && (m_alpha != 255 || m_moveAlpha != 255 )); }

private:
    /// Some useful typedefs for lazy people like me
    typedef std::set<TopWindow*> WinSet_t;
    typedef std::list<Anchor*> AncList_t;

    /// Dependencies map
    /**
     * This map represents the graph of anchored windows: it associates
     * to a given window all the windows that are directly anchored by it.
     * This is not transitive, i.e. if a is in m_dep[b] and if b is in
     * m_dep[c], it doesn't mean that a is in m_dep[c] (in fact, it
     * would be extremely rare...)
     */
    std::map<TopWindow*, WinSet_t> m_dependencies;
    /// Store all the windows
    WinSet_t m_allWindows;
    /**
     * Store the windows that were visible when saveVisibility() was
     * last called.
     */
    WinSet_t m_savedWindows;
    /// Store the moving windows
    /**
     * This set is updated at every start of move.
     */
    WinSet_t m_movingWindows;
    /**
     * Store the moving windows in the context of resizing
     * These sets are updated at every start of move
     */
    //@{
    WinSet_t m_resizeMovingE;
    WinSet_t m_resizeMovingS;
    WinSet_t m_resizeMovingSE;
    //@}
    /// Indicate whether the windows are currently on top
    VariablePtr m_cVarOnTop;
    /// Magnetism of the screen edges (= scope of action)
    int m_magnet;
    /// Alpha value of the static windows
    int m_alpha;
    /// Alpha value of the moving windows
    int m_moveAlpha;
    /// transparency set by user
    bool m_opacityEnabled;
    /// opacity overridden by user
    int m_opacity;
    /// Direction of the current resizing
    Direction_t m_direction;
    /// Rect of the last maximized window
    SkinsRect m_maximizeRect;
    /// Tooltip
    Tooltip *m_pTooltip;
    /// Active popup, if any
    Popup *m_pPopup;

    /// Recursively build a set of windows anchored to the one given.
    void buildDependSet( WinSet_t &rWinSet, TopWindow *pWindow );

    /// Check anchoring
    /**
     * This function updates xOffset and yOffset, to take care of a new
     * anchoring (if any)
     */
    void checkAnchors( TopWindow *pWindow, int &xOffset, int &yOffset ) const;
};


#endif
