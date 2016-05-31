/*****************************************************************************
 * renderer.hpp : Renderer output dialog
 ****************************************************************************
 * Copyright ( C ) 2015 the VideoLAN team
 * $Id$
 *
 * Authors: Steve Lhomme <robux4@videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

#ifndef QVLC_RENDERER_DIALOG_H_
#define QVLC_RENDERER_DIALOG_H_ 1

#include "util/qvlcframe.hpp"
#include "util/singleton.hpp"
#include "ui/renderer.h"

class MsgEvent;

class RendererDialog : public QVLCDialog, public Singleton<RendererDialog>
{
    Q_OBJECT

public:
    void discoveryEventReceived( const vlc_event_t * p_event );
    void setVisible(bool visible);

private:
    RendererDialog( intf_thread_t * );
    virtual ~RendererDialog();

    Ui::rendererWidget ui;
    void sinkMessage( const MsgEvent * );

private slots:
    void accept();
    void onReject();
    void close();

private:

    friend class          Singleton<RendererDialog>;
    vlc_renderer_discovery *p_rd;
    bool                  b_rd_started;
    void                  setSout( const vlc_renderer_item *p_item );

    static void           renderer_event_received( const vlc_event_t * p_event, void * user_data );
};


#endif
