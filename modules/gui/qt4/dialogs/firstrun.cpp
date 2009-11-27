/*****************************************************************************
 * firstrun : First Run dialogs
 ****************************************************************************
 * Copyright © 2009 VideoLAN
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb (at) videolan.org>
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


#include "dialogs/firstrun.hpp"

#include "components/preferences_widgets.hpp"

#include <QGridLayout>
#include <QGroupBox>

FirstRun::FirstRun( QWidget *_p, intf_thread_t *_p_intf )
         : QWidget( _p ), p_intf( _p_intf )
{
#ifndef HAVE_MAEMO
    /**
     * Ask for the network policy on FIRST STARTUP
     **/
    if( config_GetInt( p_intf, "qt-privacy-ask") )
    {
        buildPrivDialog();
        setVisible( true );
    }
    else
        close();
#endif
}

void FirstRun::save()
{
    QList<ConfigControl *>::Iterator i;
    for( i = controlsList.begin() ; i != controlsList.end() ; i++ )
    {
        ConfigControl *c = qobject_cast<ConfigControl *>(*i);
        c->doApply( p_intf );
    }
    config_PutInt( p_intf,  "qt-privacy-ask", 0 );
    /* We have to save here because the user may not launch Prefs */
    config_SaveConfigFile( p_intf, NULL );
    close();
}

void FirstRun::buildPrivDialog()
{
    setWindowTitle( qtr( "Privacy and Network Policies" ) );
    setWindowRole( "vlc-privacy" );
    setWindowModality( Qt::ApplicationModal );
    setWindowFlags( Qt::Dialog );
    setAttribute( Qt::WA_DeleteOnClose );

    QGridLayout *gLayout = new QGridLayout( this );

    QGroupBox *blabla = new QGroupBox( qtr( "Privacy and Network Warning" ) );
    QGridLayout *blablaLayout = new QGridLayout( blabla );
    QLabel *text = new QLabel( qtr(
        "<p>The <i>VideoLAN Team</i> doesn't like when an application goes "
        "online without authorization.</p>\n "
        "<p><i>VLC media player</i> can retreive limited information from "
        "the Internet in order to get CD covers or to check "
        "for available updates.</p>\n"
        "<p><i>VLC media player</i> <b>DOES NOT</b> send or collect <b>ANY</b> "
        "information, even anonymously, about your usage.</p>\n"
        "<p>Therefore please select from the following options, the default being "
        "almost no access to the web.</p>\n") );
    text->setWordWrap( true );
    text->setTextFormat( Qt::RichText );

    blablaLayout->addWidget( text, 0, 0 ) ;

    QGroupBox *options = new QGroupBox;
    QGridLayout *optionsLayout = new QGridLayout( options );

    gLayout->addWidget( blabla, 0, 0, 1, 3 );
    gLayout->addWidget( options, 1, 0, 1, 3 );
    module_config_t *p_config;
    ConfigControl *control;
    int line = 0;

#define CONFIG_GENERIC( option, type )                            \
    p_config =  config_FindConfig( VLC_OBJECT(p_intf), option );  \
    if( p_config )                                                \
    {                                                             \
        control =  new type ## ConfigControl( VLC_OBJECT(p_intf), \
                p_config, options, false, optionsLayout, line );  \
        controlsList.append( control );                              \
    }

#define CONFIG_GENERIC_NOBOOL( option, type )                     \
    p_config =  config_FindConfig( VLC_OBJECT(p_intf), option );  \
    if( p_config )                                                \
    {                                                             \
        control =  new type ## ConfigControl( VLC_OBJECT(p_intf), \
                p_config, options, optionsLayout, line );         \
        controlsList.append( control );                              \
    }

    CONFIG_GENERIC( "album-art", IntegerList ); line++;
#ifdef UPDATE_CHECK
    CONFIG_GENERIC_NOBOOL( "qt-updates-notif", Bool ); line++;
#endif

    QPushButton *ok = new QPushButton( qtr( "OK" ) );

    gLayout->addWidget( ok, 2, 2 );

    CONNECT( ok, clicked(), this, save() );
}

