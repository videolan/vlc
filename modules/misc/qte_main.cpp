/*****************************************************************************
 * qte_main.c : QT Embedded wrapper for gte_main
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman@wxs.nl>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
extern "C"
{
#include <vlc/vlc.h>
#include <stdlib.h>                                              /* atexit() */
}

#include <qapplication.h>
#include <qpainter.h>

extern "C"
{

typedef struct qte_thread_t
{
    VLC_COMMON_MEMBERS

    QApplication*       p_qte_application;
    QWidget*            p_qte_widget;
    bool                b_gui_server;

} qte_thread_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

static void QteMain ( qte_thread_t * );

/*****************************************************************************
 * Local variables (mutex-protected).
 *****************************************************************************/
static int            i_refcount = 0;
static qte_thread_t * p_qte_main = NULL;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define STANDALONE_TEXT N_("Run as standalone Qt/Embedded GUI Server")
#define STANDALONE_LONGTEXT N_("Use this option to run as standalone " \
    "Qt/Embedded GUI Server. This option is equivalent to the -qws option " \
    "from normal Qt.")

vlc_module_begin();
    set_description( _("Qt Embedded GUI helper") );
    set_capability( "gui-helper", 90 );
    add_bool( "qte-guiserver", 0, NULL, STANDALONE_TEXT, STANDALONE_LONGTEXT, VLC_FALSE );
    add_shortcut( "qte" );
    set_callbacks( Open, Close );
vlc_module_end();

} /* extern "C" */

/*****************************************************************************
 * Open: initialize and create window
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    vlc_value_t lockval;

    /* FIXME: put this in the module (de)initialization ASAP */
    var_Create( p_this->p_libvlc, "qte", VLC_VAR_MUTEX );

    var_Get( p_this->p_libvlc, "qte", &lockval );
    vlc_mutex_lock( (vlc_mutex_t *) lockval.p_address );

    if( i_refcount > 0 )
    {
        i_refcount++;
        vlc_mutex_unlock( (vlc_mutex_t *) lockval.p_address );

        return VLC_SUCCESS;
    }

    p_qte_main = (qte_thread_t *) vlc_object_create( p_this, sizeof(qte_thread_t) );

    /* Launch the QApplication::exec() thread. It will not return until the
     * application is properly initialized, which ensures us thread safety. */
    if( vlc_thread_create( p_qte_main, "qte_main", QteMain,
                           VLC_THREAD_PRIORITY_LOW, VLC_TRUE ) )
    {
        vlc_object_destroy( p_qte_main );
        i_refcount--;
        vlc_mutex_unlock( (vlc_mutex_t *) lockval.p_address );
        var_Destroy( p_this->p_libvlc, "qte" );
        return VLC_ETHREAD;
    }

    i_refcount++;
    vlc_mutex_unlock( (vlc_mutex_t *) lockval.p_address );

    vlc_object_attach( p_qte_main, p_this );
    msg_Dbg( p_this, "qte_main running" );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface window
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    vlc_value_t lockval;

    var_Get( p_this->p_libvlc, "qte", &lockval );
    vlc_mutex_lock( (vlc_mutex_t *) lockval.p_address );

    i_refcount--;

    if( i_refcount > 0 )
    {
        vlc_mutex_unlock( (vlc_mutex_t *) lockval.p_address );
        var_Destroy( p_this->p_libvlc, "qte" );
        return;
    }
    p_qte_main->p_qte_application->quit();

    /* Cleanup allocated classes. */
    delete p_qte_main->p_qte_widget;
    delete p_qte_main->p_qte_application;

    msg_Dbg( p_this, "Detaching qte_main" );
    vlc_object_detach( p_qte_main );

    vlc_object_destroy( p_qte_main );
    p_qte_main = NULL;

    vlc_mutex_unlock( (vlc_mutex_t *) lockval.p_address );
    var_Destroy( p_this->p_libvlc, "qte" );
}

/*****************************************************************************
 * QteMain: Qt Embedded thread
 *****************************************************************************
 * this part of the interface is in a separate thread so that we can call
 * qte_main() from within it without annoying the rest of the program.
 *****************************************************************************/
static void QteMain( qte_thread_t *p_this )
{
    int i_argc = 1;

    p_this->b_gui_server = VLC_FALSE;
    if( config_GetInt( p_this, "qte-guiserver" ) )
    {
        msg_Dbg( p_this, "Running as Qt Embedded standalone GuiServer" );
        p_this->b_gui_server = VLC_TRUE;
    }

    /* Run as standalone GuiServer or as GuiClient. */
    QApplication* pApp = new QApplication(i_argc, NULL,
        (p_this->b_gui_server ? (QApplication::GuiServer):(QApplication::GuiClient)) );
    if(pApp)
    {
        p_this->p_qte_application = pApp;
    }

    QWidget* pWidget = new QWidget(0, _("video") );
    if(pWidget)
    {
        p_this->p_qte_widget = pWidget;
    }

    /* signal the creation of the window */
    p_this->p_qte_application->setMainWidget(p_this->p_qte_widget);
    p_this->p_qte_widget->show();

    vlc_thread_ready( p_this );
    p_this->p_qte_application->exec();
}

