#include <gtk/gtk.h>
#include "gtk_control.h"
#include "gtk_menu.h"
#include "gtk_open.h"
#include "gtk_modules.h"
#include "gtk_playlist.h"
#include "gtk_preferences.h"

/* General glade callbacks */

gboolean
GtkExit                                (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);
gboolean
GtkWindowToggle                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);


gboolean
GtkSliderRelease                       (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
GtkSliderPress                         (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
GtkTitlePrev                           (GtkButton       *button,
                                        gpointer         user_data);

void
GtkTitleNext                           (GtkButton       *button,
                                        gpointer         user_data);

void
GtkChapterPrev                         (GtkButton       *button,
                                        gpointer         user_data);

void
GtkChapterNext                         (GtkButton       *button,
                                        gpointer         user_data);


gboolean
GtkFullscreen                          (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);
gboolean
GtkAboutShow                           (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void GtkAboutOk( GtkButton * button, gpointer user_data);


void
GtkWindowDrag                          (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gint             x,
                                        gint             y,
                                        GtkSelectionData *data,
                                        guint            info,
                                        guint            time,
                                        gpointer         user_data);

gboolean
GtkWindowDelete                        (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
GtkJumpShow                            (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);
void
GtkJumpOk                              (GtkButton       *button,
                                        gpointer         user_data);
void
GtkJumpCancel                          (GtkButton       *button,
                                        gpointer         user_data);


gboolean
GtkDiscOpenShow                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
GtkFileOpenShow                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
GtkNetworkOpenShow                     (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);



void
on_menubar_open_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_open_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_disc_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_network_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_exit_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_interface_hide_activate     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menubar_fullscreen_activate         (GtkMenuItem     *menuitem,
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
on_popup_play_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_pause_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_stop_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_back_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_slow_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_fast_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_interface_toggle_activate     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_fullscreen_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_next_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_prev_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_jump_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_file_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_disc_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_network_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_about_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_playlist_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_preferences_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_popup_exit_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);



void
GtkPreferencesOk                       (GtkButton       *button,
                                        gpointer         user_data);

void
GtkPreferencesApply                    (GtkButton       *button,
                                        gpointer         user_data);

void
GtkPreferencesCancel                   (GtkButton       *button,
                                        gpointer         user_data);

void
GtkFileOpenActivate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GtkDiscOpenActivate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GtkNetworkOpenActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GtkExitActivate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GtkWindowToggleActivate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GtkFullscreenActivate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GtkPlaylistActivate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GtkModulesActivate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GtkPreferencesActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GtkAboutActivate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);


void
GtkNextActivate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GtkPrevActivate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GtkJumpActivate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GtkDiscOpenActivate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GtkFileOpenActivate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GtkNetworkOpenActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GtkPlaylistAddUrl                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);
