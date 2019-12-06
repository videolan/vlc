/*****************************************************************************
 * mediainfo.hpp : Information about a stream
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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
 ******************************************************************************/

#ifndef QVLC_MEDIAINFO_DIALOG_H_
#define QVLC_MEDIAINFO_DIALOG_H_ 1

#include "widgets/native/qvlcframe.hpp"
#include "info_panels.hpp"
#include "util/singleton.hpp"

class QTabWidget;

class MediaInfoDialog : public QVLCFrame, public Singleton<MediaInfoDialog>
{
    Q_OBJECT
public:
    MediaInfoDialog( intf_thread_t *,
                     input_item_t * input = NULL );

    enum panel
    {
        META_PANEL = 0,
        EXTRAMETA_PANEL,
        INFO_PANEL,
        INPUTSTATS_PANEL
    };

    void showTab( panel );
    int currentTab();
#if 0
    void setInput( input_item_t * );
#endif

private:
    virtual ~MediaInfoDialog();

    bool isMainInputInfo;

    QTabWidget *infoTabW;

    InputStatsPanel *ISP;
    MetaPanel *MP;
    InfoPanel *IP;
    ExtraMetaPanel *EMP;

    QPushButton *saveMetaButton;
    QLineEdit   *uriLine;

private slots:
    void updateAllTabs( input_item_t * );
    void clearAllTabs();

    void close() Q_DECL_OVERRIDE;

    void saveMeta();
    void updateButtons( int i_tab );
    void updateURI( const QString& );

    friend class    Singleton<MediaInfoDialog>;
};

#endif
