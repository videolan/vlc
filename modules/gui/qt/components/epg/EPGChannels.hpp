/*****************************************************************************
 * EPGChannels.hpp : EPGChannels
 ****************************************************************************
 * Copyright © 2009-2010 VideoLAN
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef EPGCHANNELS_HPP
#define EPGCHANNELS_HPP

#include "qt.hpp"

#include <QWidget>

class EPGView;
class EPGProgram;

class EPGChannels : public QWidget
{
    Q_OBJECT
public:
    EPGChannels( QWidget *parent, EPGView *m_epgView );

public slots:
    void setOffset( int offset );
    void addProgram( const EPGProgram * );
    void reset();

protected:
    void paintEvent( QPaintEvent *event ) Q_DECL_OVERRIDE;

private:
    EPGView *m_epgView;
    int m_offset;
    QList<const EPGProgram *> programsList;
};

#endif // EPGCHANNELS_HPP
