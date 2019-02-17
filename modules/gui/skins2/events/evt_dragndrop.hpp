/*****************************************************************************
 * evt_dragndrop.hpp
 *****************************************************************************
 * Copyright (C) 2011 the VideoLAN team
 *
 * Author: Erwan Tulou      <erwan10 At videolan Dot org>
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

#ifndef EVT_DRAGNDROP_HPP
#define EVT_DRAGNDROP_HPP

#include "evt_generic.hpp"
#include <list>


/// Drag'n'Drop generic event
class EvtDrag: public EvtGeneric
{
public:
    EvtDrag( intf_thread_t *pIntf ): EvtGeneric( pIntf ) { }
    virtual ~EvtDrag() { }
    virtual const std::string getAsString() const { return "drag"; }
};


class EvtDragEnter: public EvtDrag
{
public:
    EvtDragEnter( intf_thread_t *pIntf ): EvtDrag( pIntf ) { }
    virtual ~EvtDragEnter() { }
    virtual const std::string getAsString() const { return "drag:enter"; }
};


class EvtDragLeave: public EvtDrag
{
public:
    EvtDragLeave( intf_thread_t *pIntf ): EvtDrag( pIntf ) { }
    virtual ~EvtDragLeave() { }
    virtual const std::string getAsString() const { return "drag:leave"; }
};


class EvtDragOver: public EvtDrag
{
public:
    EvtDragOver( intf_thread_t *pIntf, int x, int y )
        : EvtDrag( pIntf ), m_xPos( x ), m_yPos( y ) { }
    virtual ~EvtDragOver() { }
    virtual const std::string getAsString() const { return "drag:over"; }
    // Return the event coordinates
    int getXPos() const { return m_xPos; }
    int getYPos() const { return m_yPos; }
private:
    int m_xPos;
    int m_yPos;
};

class EvtDragDrop: public EvtDrag
{
public:
    EvtDragDrop( intf_thread_t *pIntf, int x, int y, const std::list<std::string>& files )
        : EvtDrag( pIntf ), m_files( files ), m_xPos( x ), m_yPos( y ) { }
    virtual ~EvtDragDrop() { }
    virtual const std::string getAsString() const { return "drag:drop"; }
    // Return the event coordinates
    int getXPos() const { return m_xPos; }
    int getYPos() const { return m_yPos; }
    const std::list<std::string>& getFiles() const { return m_files; }
private:
    std::list<std::string> m_files;
    int m_xPos;
    int m_yPos;
};

#endif
