#include "defs.h"

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include <gnome.h>

#include "stream_control.h"
#include "input_ext-intf.h"

#include "interface.h"

#include "gnome_sys.h"
#include "gnome_callbacks.h"
#include "gnome_interface.h"
#include "gnome_support.h"

#define GET_GNOME_STRUCT( item, parent ) \
gtk_object_get_data( \
    GTK_OBJECT( lookup_widget(GTK_WIDGET(item), parent) ), \
    "p_intf" );

void
on_modules_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_exit_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf;

    p_intf = GET_GNOME_STRUCT( menuitem, "intf_window" );

    p_intf->b_die = 1;
}


void
on_open_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_preferences_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_plugins_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_about_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf;

    p_intf = GET_GNOME_STRUCT( menuitem, "intf_window" );

    if( !GTK_IS_WIDGET( p_intf->p_sys->p_about ) )
    {
        p_intf->p_sys->p_about = create_intf_about ();
    }
    gtk_widget_show( p_intf->p_sys->p_about );
}


void
on_stop_clicked                        (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf;

    p_intf = GET_GNOME_STRUCT( button, "intf_window" );

}


void
on_control_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf;

    p_intf = GET_GNOME_STRUCT( menuitem, "intf_window" );

    /* lock the change structure */
    vlc_mutex_lock( &p_intf->p_sys->change_lock );

    if( p_intf->p_sys->b_window )
    {
        gtk_widget_hide( p_intf->p_sys->p_window );
        p_intf->p_sys->b_window = 0;
    }
    else
    {
        if( !GTK_IS_WIDGET( p_intf->p_sys->p_window ) )
        {
            p_intf->p_sys->p_window = create_intf_window ();
        }
        gtk_widget_show( p_intf->p_sys->p_window );
        gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_window),
                             "p_intf", p_intf );
        p_intf->p_sys->b_window = 1;
    }

    /* unlock the change structure */
    vlc_mutex_unlock( &p_intf->p_sys->change_lock );
}


void
on_playlist_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf;

    p_intf = GET_GNOME_STRUCT( menuitem, "intf_window" );

    /* lock the change structure */
    vlc_mutex_lock( &p_intf->p_sys->change_lock );

    if( p_intf->p_sys->b_playlist )
    {
        gtk_widget_hide( p_intf->p_sys->p_playlist );
        p_intf->p_sys->b_playlist = 0;
    }
    else
    {
        if( !GTK_IS_WIDGET( p_intf->p_sys->p_playlist ) )
        {
            p_intf->p_sys->p_playlist = create_intf_playlist ();
        }
        gtk_widget_show( p_intf->p_sys->p_playlist );
        gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_playlist),
                             "p_intf", p_intf );
        p_intf->p_sys->b_playlist = 1;
    }

    /* unlock the change structure */
    vlc_mutex_unlock( &p_intf->p_sys->change_lock );
}

void
on_popup_control_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf;

    p_intf = GET_GNOME_STRUCT( menuitem, "intf_popup" );

    /* lock the change structure */
    vlc_mutex_lock( &p_intf->p_sys->change_lock );

    if( p_intf->p_sys->b_window )
    {
        gtk_widget_hide( p_intf->p_sys->p_window );
        p_intf->p_sys->b_window = 0;
    }
    else
    {
        if( !GTK_IS_WIDGET( p_intf->p_sys->p_window ) )
        {
            p_intf->p_sys->p_window = create_intf_window ();
        }
        gtk_widget_show( p_intf->p_sys->p_window );
        gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_window),
                             "p_intf", p_intf );
        p_intf->p_sys->b_window = 1;
    }

    /* unlock the change structure */
    vlc_mutex_unlock( &p_intf->p_sys->change_lock );
}


void
on_popup_playlist_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf;

    p_intf = GET_GNOME_STRUCT( menuitem, "intf_popup" );

    /* lock the change structure */
    vlc_mutex_lock( &p_intf->p_sys->change_lock );

    if( p_intf->p_sys->b_playlist )
    {
        gtk_widget_hide( p_intf->p_sys->p_playlist );
        p_intf->p_sys->b_playlist = 0;
    }
    else
    {
        if( !GTK_IS_WIDGET( p_intf->p_sys->p_playlist ) )
        {
            p_intf->p_sys->p_playlist = create_intf_playlist ();
        }
        gtk_widget_show( p_intf->p_sys->p_playlist );
        gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_playlist),
                             "p_intf", p_intf );
        p_intf->p_sys->b_playlist = 1;
    }

    /* unlock the change structure */
    vlc_mutex_unlock( &p_intf->p_sys->change_lock );
}



void
on_popup_exit_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf;

    p_intf = GET_GNOME_STRUCT( menuitem, "intf_popup" );

    p_intf->b_die = 1;
}


void
on_popup_about_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf;

    p_intf = GET_GNOME_STRUCT( menuitem, "intf_popup" );

    if( !GTK_IS_WIDGET( p_intf->p_sys->p_about ) )
    {
        p_intf->p_sys->p_about = create_intf_about ();
    }
    gtk_widget_show( p_intf->p_sys->p_about );
}


void
on_intf_window_destroy                 (GtkObject       *object,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf;

    p_intf = GET_GNOME_STRUCT( object, "intf_window" );

    /* FIXME don't destroy the window, just hide it */
    p_intf->p_sys->p_window = NULL;

    p_intf->b_die = 1;
}


void
on_intf_playlist_destroy               (GtkObject       *object,
                                        gpointer         user_data)
{
   ;
}



void
on_channel1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_channel2_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_channel3_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_channel4_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_channel5_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_popup_channel1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_popup_channel2_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_popup_channel3_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_popup_channel4_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_popup_channel5_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_popup_config_channels_activate      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_config_channels_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_user_guide_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_popup_stop_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_popup_play_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf;

    p_intf = GET_GNOME_STRUCT( menuitem, "intf_popup" );

    input_Play( p_intf->p_input );
}


void
on_playlist_close_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
on_play_clicked                        (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf;

    p_intf = GET_GNOME_STRUCT( button, "intf_window" );

    input_Play( p_intf->p_input );
}


void
on_channel0_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_popup_channel0_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_open_clicked                        (GtkButton       *button,
                                        gpointer         user_data)
{
    GnomeUIInfo test_uiinfo[] =
    {
        {
            GNOME_APP_UI_ITEM, N_( "Barf" ),
            NULL,
            on_exit_activate, NULL, NULL,
            GNOME_APP_PIXMAP_NONE, NULL,
            0, 0, NULL
        }
    };

    intf_thread_t *p_intf;

    p_intf = GET_GNOME_STRUCT( button, "intf_window" );

    gnome_app_insert_menus (GNOME_APP (p_intf->p_sys->p_window),
                              "_View/Channel/None",
                              test_uiinfo);
}


void
on_pause_clicked                       (GtkButton       *button,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf;

    p_intf = GET_GNOME_STRUCT( button, "intf_window" );

    input_Pause( p_intf->p_input );
}


void
on_popup_pause_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    intf_thread_t *p_intf;

    p_intf = GET_GNOME_STRUCT( menuitem, "intf_popup" );

    input_Pause( p_intf->p_input );
}



void
on_mute_clicked                        (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
on_popup_mute_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}

