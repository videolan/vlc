#include <gtk/gtk.h>

/* General glade callbacks */

void
on_intf_window_drag_data_received      (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gint             x,
                                        gint             y,
                                        GtkSelectionData *data,
                                        guint            info,
                                        guint            time,
                                        gpointer         user_data);

void
on_toolbar_open_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_back_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_stop_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_play_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_pause_clicked               (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_slow_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_fast_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_playlist_clicked            (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_prev_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_next_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_network_clicked             (GtkButton       *button,
                                        gpointer         user_data);

void
on_intf_fileopen_destroy               (GtkObject       *object,
                                        gpointer         user_data);

void
on_fileopen_ok_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_fileopen_cancel_clicked             (GtkButton       *button,
                                        gpointer         user_data);

void
on_intf_modules_destroy                (GtkObject       *object,
                                        gpointer         user_data);

void
on_modules_ok_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_modules_apply_clicked               (GtkButton       *button,
                                        gpointer         user_data);

void
on_modules_cancel_clicked              (GtkButton       *button,
                                        gpointer         user_data);

void
on_intf_playlist_destroy               (GtkObject       *object,
                                        gpointer         user_data);

void
on_playlist_ok_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_popup_fast_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_open_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_exit_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_play_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_exit_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_playlist_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_modules_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_preferences_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_about_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_pause_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_slow_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_open_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_about_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_about_ok_clicked                    (GtkButton       *button,
                                        gpointer         user_data);


void
on_disc_dvd_toggled                    (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_disc_vcd_toggled                    (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_disc_ok_clicked                     (GtkButton       *button,
                                        gpointer         user_data);

void
on_disc_cancel_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_menubar_disc_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_toolbar_disc_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_popup_disc_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_audio_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_subpictures_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_audio_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_subpictures_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_navigation_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_title_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_chapter_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_playlist_clist_event                        (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_intf_window_delete                  (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_intf_playlist_destroy_event         (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_intf_playlist_destroy_event         (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_intf_playlist_drag_data_received    (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gint             x,
                                        gint             y,
                                        GtkSelectionData *data,
                                        guint            info,
                                        guint            time,
                                        gpointer         user_data);

gboolean
on_playlist_clist_event                        (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_intf_playlist_destroy_event         (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_intf_window_destroy                 (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_intf_window_destroy                 (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_main_window_toggle                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_delete_clicked                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_crop_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_invertselection_clicked             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_playlist_clist_drag_motion          (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gint             x,
                                        gint             y,
                                        guint            time,
                                        gpointer         user_data);

void
on_intf_network_destroy                (GtkObject       *object,
                                        gpointer         user_data);

void
on_network_ok_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_network_cancel_clicked              (GtkButton       *button,
                                        gpointer         user_data);

void
on_menubar_network_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_network_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_slider_button_release_event         (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_slider_button_press_event           (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_menubar_fullscreen_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_angle_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);
