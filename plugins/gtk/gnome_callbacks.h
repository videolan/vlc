#include <gnome.h>

#include "gtk_callbacks.h"

void
GnomeMenubarFileOpenActivate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomeMenubarDiscOpenActivate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomeMenubarNetworkOpenActivate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomeMenubarDiscEjectActivate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomeMenubarExitActivate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomeMenubarWindowToggleActivate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomeMenubarFullscreenActivate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomeMenubarPlaylistActivate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomeMenubarModulesActivate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomeMenubarPreferencesActivate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomeMenubarAboutActivate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePopupPlayActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePopupPauseActivate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePopupStopActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePopupBackActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePopupSlowActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePopupFastActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePopupWindowToggleActivate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePopupFullscreenActivate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePopupNextActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePopupPrevActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePopupFileOpenActivate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePopupDiscOpenActivate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePopupNetworkOpenActivate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePopupAboutActivate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePopupPlaylistActivate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePopupPreferencesActivate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePopupExitActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePlaylistDiscOpenActivate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePlaylistFileOpenActivate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePlaylistNetworkOpenActivate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GnomePopupJumpActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
GtkNetworkJoin                         (GtkEditable     *editable,
                                        gpointer         user_data);

void
GtkChannelGo                           (GtkButton       *button,
                                        gpointer         user_data);

void
GtkNetworkOpenBroadcast                (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
GtkNetworkOpenChannel                  (GtkToggleButton *togglebutton,
                                        gpointer         user_data);


void
GnomeMenubarMessagesActivate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);
