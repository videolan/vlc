/*****************************************************************************
 * simple_preferences.hpp : Simple prefs
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#ifndef VLC_QT_SIMPLE_PREFERENCES_HPP_
#define VLC_QT_SIMPLE_PREFERENCES_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

/* Auto-generated from .ui files */
#include "ui_sprefs_input.h"
#include "ui_sprefs_audio.h"
#include "ui_sprefs_video.h"
#include "ui_sprefs_subtitles.h"
#include "ui_sprefs_interface.h"
#include "ui_sprefs_medialibrary.h"

class MLFoldersEditor;

#ifdef _WIN32
# include "util/registry.hpp"
#endif

#include <QWidget>
class QLineEdit;
class QLabel;
class QPushButton;
class QAbstractButton;

enum {
    SPrefsInterface = 0,
    SPrefsAudio,
    SPrefsVideo,
    SPrefsSubtitles,
    SPrefsInputAndCodecs,
    SPrefsHotkeys,
    SPrefsMediaLibrary,
    SPrefsMax
};
#define SPrefsDefaultCat SPrefsInterface

enum {
    CachingCustom = 0,
    CachingLowest = 100,
    CachingLow    = 200,
    CachingNormal = 300,
    CachingHigh   = 500,
    CachingHigher = 1000
};

class ConfigControl;
class QString;

#ifdef _WIN32
class QTreeWidgetItem;
#endif

class SPrefsCatList : public QWidget
{
    Q_OBJECT
public:
    SPrefsCatList( qt_intf_t *, QWidget * );
    virtual ~SPrefsCatList() {}
private:
    qt_intf_t *p_intf;
signals:
    void currentItemChanged( int );
public slots:
    void switchPanel( int );
};

class SPrefsPanel : public QWidget
{
    Q_OBJECT
public:
    SPrefsPanel( qt_intf_t *, QWidget *, int );
    virtual ~SPrefsPanel();
    void apply();
#ifdef _WIN32
    void cleanLang();
#endif

private:
    template<typename ControlType, typename WidgetType>
    void configGeneric(const char* option, QLabel* label, WidgetType* control);
    template<typename ControlType, typename WidgetType>
    void configGenericNoUi(const char* option, QLabel* label, WidgetType* control);
    void configBool(const char* option, QAbstractButton* control);
    template<typename T>
    void configGenericFile(const char* option, QLabel* label, QLineEdit* control, QPushButton* button);


private:
    qt_intf_t *p_intf;
    QList<ConfigControl *> controls;

    int number;

    struct AudioControlGroup {
        AudioControlGroup(QLabel* label, QWidget* widget, QPushButton* button = nullptr)
            : label(label)
            , widget(widget)
            , button(button)
        {
        }

        AudioControlGroup() {}

        QLabel* label = nullptr;
        QWidget* widget = nullptr;
        QPushButton* button = nullptr;
    };
    QHash<QString, AudioControlGroup> audioControlGroups;
    QStringList qs_filter;
    QButtonGroup *radioGroup;
    QButtonGroup *layoutImages;

    char *lang;
    MLFoldersEditor *mlFoldersEditor {};
    MLFoldersEditor *mlBannedFoldersEditor {};

#ifdef _WIN32
    QList<QTreeWidgetItem *> listAsso;
    bool addType( const char * psz_ext, QTreeWidgetItem*, QTreeWidgetItem*, QVLCRegistry* );
    void saveLang();
#endif

    // used to revert properties on cancel which are immediately set
    bool m_isApplied = false;
    std::vector<std::unique_ptr<class PropertyResetter>> m_resetters;

    Ui::SPrefsInterface m_interfaceUI;
    Ui::SPrefsVideo m_videoUI;
    Ui::SPrefsAudio m_audioUI;
    Ui::SPrefsInputAndCodecs m_inputCodecUI;
    Ui::SPrefsSubtitles m_subtitlesUI;
    Ui::SPrefsMediaLibrary m_medialibUI;

/* Display only the options for the selected audio output */
private slots:
    void lastfm_Changed( int );
    void updateAudioOptions( int );
    void updateAudioVolume( int );
    void langChanged( int );
    void imageLayoutClick( QAbstractButton* );
    void handleLayoutChange( int );
    void updateLayoutSelection();
#ifdef _WIN32
    void assoDialog();
    void updateCheckBoxes( QTreeWidgetItem*, int );
    void saveAsso();
#endif
    void MLaddNewFolder( );
    void MLBanFolder( );

    void configML();
    void changeStyle( );

    void clean();
};

#endif
