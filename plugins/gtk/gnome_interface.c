/* This file was created automatically by glade and fixed by fixfiles.sh */

#include <videolan/vlc.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gnome.h>

#include "gnome_callbacks.h"
#include "gnome_interface.h"
#include "gnome_support.h"

static GnomeUIInfo menubar_file_menu_uiinfo[] =
{
  {
    GNOME_APP_UI_ITEM, N_("_Open File..."),
    N_("Open a File"),
    (gpointer) GnomeMenubarFileOpenActivate, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Open _Disc..."),
    N_("Open a DVD or VCD"),
    (gpointer) GnomeMenubarDiscOpenActivate, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CDROM,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Network Stream..."),
    N_("Select a Network Stream"),
    (gpointer) GnomeMenubarNetworkOpenActivate, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_REFRESH,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("_Eject Disc"),
    N_("Eject disc"),
    (gpointer) GnomeMenubarDiscEjectActivate, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_TOP,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_EXIT_ITEM (GnomeMenubarExitActivate, NULL),
  GNOMEUIINFO_END
};

static GnomeUIInfo menubar_view_menu_uiinfo[] =
{
  {
    GNOME_APP_UI_ITEM, N_("_Hide interface"),
    NULL,
    (gpointer) GnomeMenubarWindowToggleActivate, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Fullscreen"),
    NULL,
    (gpointer) GnomeMenubarFullscreenActivate, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("Progr_am"),
    N_("Choose the program"),
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Title"),
    N_("Choose title"),
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Chapter"),
    N_("Choose chapter"),
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("_Playlist..."),
    N_("Open the playlist window"),
    (gpointer) GnomeMenubarPlaylistActivate, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_INDEX,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Modules..."),
    N_("Open the plugin manager"),
    (gpointer) GnomeMenubarModulesActivate, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_ATTACH,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Messages..."),
    N_("Open the messages window"),
    (gpointer) GnomeMenubarMessagesActivate, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo menubar_settings_menu_uiinfo[] =
{
  {
    GNOME_APP_UI_ITEM, N_("_Audio"),
    N_("Select audio channel"),
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Subtitles"),
    N_("Select subtitle unit"),
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_PREFERENCES_ITEM (GnomeMenubarPreferencesActivate, NULL),
  GNOMEUIINFO_END
};

static GnomeUIInfo menubar_help_menu_uiinfo[] =
{
  GNOMEUIINFO_MENU_ABOUT_ITEM (GnomeMenubarAboutActivate, NULL),
  GNOMEUIINFO_END
};

static GnomeUIInfo menubar_uiinfo[] =
{
  GNOMEUIINFO_MENU_FILE_TREE (menubar_file_menu_uiinfo),
  GNOMEUIINFO_MENU_VIEW_TREE (menubar_view_menu_uiinfo),
  GNOMEUIINFO_MENU_SETTINGS_TREE (menubar_settings_menu_uiinfo),
  GNOMEUIINFO_MENU_HELP_TREE (menubar_help_menu_uiinfo),
  GNOMEUIINFO_END
};

GtkWidget*
create_intf_window (void)
{
  GtkWidget *intf_window;
  GtkWidget *dockitem;
  GtkWidget *toolbar;
  GtkWidget *tmp_toolbar_icon;
  GtkWidget *toolbar_file;
  GtkWidget *toolbar_disc;
  GtkWidget *toolbar_network;
  GtkWidget *toolbar_sat;
  GtkWidget *toolbar_back;
  GtkWidget *toolbar_stop;
  GtkWidget *toolbar_eject;
  GtkWidget *toolbar_play;
  GtkWidget *toolbar_pause;
  GtkWidget *toolbar_slow;
  GtkWidget *toolbar_fast;
  GtkWidget *toolbar_playlist;
  GtkWidget *toolbar_prev;
  GtkWidget *toolbar_next;
  GtkWidget *vbox8;
  GtkWidget *slider_frame;
  GtkWidget *slider;
  GtkWidget *file_box;
  GtkWidget *label_status;
  GtkWidget *dvd_box;
  GtkWidget *label21;
  GtkWidget *title_chapter_box;
  GtkWidget *label19;
  GtkWidget *title_label;
  GtkWidget *button_title_prev;
  GtkWidget *button_title_next;
  GtkWidget *vseparator1;
  GtkWidget *dvd_chapter_box;
  GtkWidget *label20;
  GtkWidget *chapter_label;
  GtkWidget *button_chapter_prev;
  GtkWidget *button_chapter_next;
  GtkWidget *network_box;
  GtkWidget *network_address_label;
  GtkWidget *network_channel_box;
  GtkWidget *label_network;
  GtkObject *network_channel_spinbutton_adj;
  GtkWidget *network_channel_spinbutton;
  GtkWidget *network_channel_go_button;
  GtkWidget *appbar;
  GtkTooltips *tooltips;

  tooltips = gtk_tooltips_new ();

  intf_window = gnome_app_new ("VideoLAN Client", _("VideoLAN Client"));
  gtk_object_set_data (GTK_OBJECT (intf_window), "intf_window", intf_window);
  gtk_window_set_policy (GTK_WINDOW (intf_window), FALSE, TRUE, TRUE);

  dockitem = GNOME_APP (intf_window)->dock;
  gtk_widget_ref (dockitem);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "dockitem", dockitem,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (dockitem);

  gnome_app_create_menus (GNOME_APP (intf_window), menubar_uiinfo);

  gtk_widget_ref (menubar_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_file",
                            menubar_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_file_menu_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_open",
                            menubar_file_menu_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_file_menu_uiinfo[1].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_disc",
                            menubar_file_menu_uiinfo[1].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_file_menu_uiinfo[2].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_network",
                            menubar_file_menu_uiinfo[2].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_file_menu_uiinfo[3].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator1",
                            menubar_file_menu_uiinfo[3].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_file_menu_uiinfo[4].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_eject",
                            menubar_file_menu_uiinfo[4].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_file_menu_uiinfo[5].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator15",
                            menubar_file_menu_uiinfo[5].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_file_menu_uiinfo[6].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_exit",
                            menubar_file_menu_uiinfo[6].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_uiinfo[1].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_view",
                            menubar_uiinfo[1].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_view_menu_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_hide_interface",
                            menubar_view_menu_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_view_menu_uiinfo[1].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_fullscreen",
                            menubar_view_menu_uiinfo[1].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_view_menu_uiinfo[2].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator9",
                            menubar_view_menu_uiinfo[2].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_view_menu_uiinfo[3].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_program",
                            menubar_view_menu_uiinfo[3].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (menubar_view_menu_uiinfo[3].widget, FALSE);

  gtk_widget_ref (menubar_view_menu_uiinfo[4].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_title",
                            menubar_view_menu_uiinfo[4].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (menubar_view_menu_uiinfo[4].widget, FALSE);

  gtk_widget_ref (menubar_view_menu_uiinfo[5].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_chapter",
                            menubar_view_menu_uiinfo[5].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (menubar_view_menu_uiinfo[5].widget, FALSE);

  gtk_widget_ref (menubar_view_menu_uiinfo[6].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator7",
                            menubar_view_menu_uiinfo[6].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_view_menu_uiinfo[7].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_playlist",
                            menubar_view_menu_uiinfo[7].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_view_menu_uiinfo[8].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_modules",
                            menubar_view_menu_uiinfo[8].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (menubar_view_menu_uiinfo[8].widget, FALSE);

  gtk_widget_ref (menubar_view_menu_uiinfo[9].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_messages",
                            menubar_view_menu_uiinfo[9].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_uiinfo[2].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_settings",
                            menubar_uiinfo[2].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_settings_menu_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_audio",
                            menubar_settings_menu_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (menubar_settings_menu_uiinfo[0].widget, FALSE);

  gtk_widget_ref (menubar_settings_menu_uiinfo[1].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_subpictures",
                            menubar_settings_menu_uiinfo[1].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (menubar_settings_menu_uiinfo[1].widget, FALSE);

  gtk_widget_ref (menubar_settings_menu_uiinfo[2].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator5",
                            menubar_settings_menu_uiinfo[2].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_settings_menu_uiinfo[3].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_preferences",
                            menubar_settings_menu_uiinfo[3].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_uiinfo[3].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_help",
                            menubar_uiinfo[3].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_help_menu_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_about",
                            menubar_help_menu_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);
  gtk_widget_ref (toolbar);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar", toolbar,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar);
  gnome_app_add_toolbar (GNOME_APP (intf_window), GTK_TOOLBAR (toolbar), "toolbar",
                                GNOME_DOCK_ITEM_BEH_EXCLUSIVE,
                                GNOME_DOCK_TOP, 1, 0, 2);
  gtk_toolbar_set_space_size (GTK_TOOLBAR (toolbar), 16);
  gtk_toolbar_set_space_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_SPACE_LINE);
  gtk_toolbar_set_button_relief (GTK_TOOLBAR (toolbar), GTK_RELIEF_NONE);

  tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_OPEN);
  toolbar_file = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("File"),
                                _("Open a File"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_ref (toolbar_file);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_file", toolbar_file,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_file);

  tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_CDROM);
  toolbar_disc = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Disc"),
                                _("Open a DVD or VCD"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_ref (toolbar_disc);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_disc", toolbar_disc,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_disc);

  tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_REFRESH);
  toolbar_network = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Net"),
                                _("Select a Network Stream"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_ref (toolbar_network);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_network", toolbar_network,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_network);

  tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_MIC);
  toolbar_sat = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Sat"),
                                _("Open a Satellite Card"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_ref (toolbar_sat);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_sat", toolbar_sat,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_sat);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_BACK);
  toolbar_back = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Back"),
                                _("Go Backwards"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_ref (toolbar_back);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_back", toolbar_back,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_back);
  gtk_widget_set_sensitive (toolbar_back, FALSE);

  tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_STOP);
  toolbar_stop = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Stop"),
                                _("Stop Stream"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_ref (toolbar_stop);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_stop", toolbar_stop,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_stop);

  tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_TOP);
  toolbar_eject = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Eject"),
                                _("Eject disc"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_ref (toolbar_eject);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_eject", toolbar_eject,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_eject);

  tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_FORWARD);
  toolbar_play = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Play"),
                                _("Play Stream"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_ref (toolbar_play);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_play", toolbar_play,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_play);

  tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_BOTTOM);
  toolbar_pause = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Pause"),
                                _("Pause Stream"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_ref (toolbar_pause);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_pause", toolbar_pause,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_pause);
  gtk_widget_set_sensitive (toolbar_pause, FALSE);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_TIMER_STOP);
  toolbar_slow = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Slow"),
                                _("Play Slower"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_ref (toolbar_slow);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_slow", toolbar_slow,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_slow);
  gtk_widget_set_sensitive (toolbar_slow, FALSE);

  tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_TIMER);
  toolbar_fast = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Fast"),
                                _("Play Faster"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_ref (toolbar_fast);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_fast", toolbar_fast,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_fast);
  gtk_widget_set_sensitive (toolbar_fast, FALSE);

  tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_INDEX);
  toolbar_playlist = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Playlist"),
                                _("Open Playlist"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_ref (toolbar_playlist);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_playlist", toolbar_playlist,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_playlist);

  tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_FIRST);
  toolbar_prev = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Prev"),
                                _("Previous File"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_ref (toolbar_prev);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_prev", toolbar_prev,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_prev);

  tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_LAST);
  toolbar_next = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Next"),
                                _("Next File"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_ref (toolbar_next);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_next", toolbar_next,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_next);

  vbox8 = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (vbox8);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "vbox8", vbox8,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox8);
  gnome_app_set_contents (GNOME_APP (intf_window), vbox8);

  slider_frame = gtk_frame_new (_("-:--:--"));
  gtk_widget_ref (slider_frame);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "slider_frame", slider_frame,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_box_pack_start (GTK_BOX (vbox8), slider_frame, TRUE, TRUE, 0);
  gtk_frame_set_label_align (GTK_FRAME (slider_frame), 0.05, 0.5);

  slider = gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, 100, 1, 6.25, 0)));
  gtk_widget_ref (slider);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "slider", slider,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (slider);
  gtk_container_add (GTK_CONTAINER (slider_frame), slider);
  gtk_scale_set_draw_value (GTK_SCALE (slider), FALSE);
  gtk_scale_set_digits (GTK_SCALE (slider), 3);

  file_box = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (file_box);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "file_box", file_box,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (file_box);
  gtk_box_pack_start (GTK_BOX (vbox8), file_box, TRUE, TRUE, 0);

  label_status = gtk_label_new ("");
  gtk_widget_ref (label_status);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "label_status", label_status,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label_status);
  gtk_box_pack_start (GTK_BOX (file_box), label_status, TRUE, TRUE, 0);

  dvd_box = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (dvd_box);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "dvd_box", dvd_box,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_box_pack_start (GTK_BOX (vbox8), dvd_box, TRUE, TRUE, 0);

  label21 = gtk_label_new (_("Disc"));
  gtk_widget_ref (label21);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "label21", label21,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label21);
  gtk_box_pack_start (GTK_BOX (dvd_box), label21, TRUE, FALSE, 0);

  title_chapter_box = gtk_hbox_new (FALSE, 10);
  gtk_widget_ref (title_chapter_box);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "title_chapter_box", title_chapter_box,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (title_chapter_box);
  gtk_box_pack_start (GTK_BOX (dvd_box), title_chapter_box, TRUE, FALSE, 0);

  label19 = gtk_label_new (_("Title:"));
  gtk_widget_ref (label19);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "label19", label19,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label19);
  gtk_box_pack_start (GTK_BOX (title_chapter_box), label19, FALSE, FALSE, 0);

  title_label = gtk_label_new (_("--"));
  gtk_widget_ref (title_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "title_label", title_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (title_label);
  gtk_box_pack_start (GTK_BOX (title_chapter_box), title_label, FALSE, FALSE, 0);

  button_title_prev = gnome_stock_button (GNOME_STOCK_BUTTON_PREV);
  gtk_widget_ref (button_title_prev);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "button_title_prev", button_title_prev,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (button_title_prev);
  gtk_box_pack_start (GTK_BOX (title_chapter_box), button_title_prev, FALSE, FALSE, 0);
  gtk_tooltips_set_tip (tooltips, button_title_prev, _("Select previous title"), NULL);

  button_title_next = gnome_stock_button (GNOME_STOCK_BUTTON_NEXT);
  gtk_widget_ref (button_title_next);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "button_title_next", button_title_next,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (button_title_next);
  gtk_box_pack_start (GTK_BOX (title_chapter_box), button_title_next, FALSE, FALSE, 0);

  vseparator1 = gtk_vseparator_new ();
  gtk_widget_ref (vseparator1);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "vseparator1", vseparator1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vseparator1);
  gtk_box_pack_start (GTK_BOX (dvd_box), vseparator1, FALSE, FALSE, 0);

  dvd_chapter_box = gtk_hbox_new (FALSE, 10);
  gtk_widget_ref (dvd_chapter_box);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "dvd_chapter_box", dvd_chapter_box,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (dvd_chapter_box);
  gtk_box_pack_start (GTK_BOX (dvd_box), dvd_chapter_box, TRUE, FALSE, 0);

  label20 = gtk_label_new (_("Chapter:"));
  gtk_widget_ref (label20);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "label20", label20,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label20);
  gtk_box_pack_start (GTK_BOX (dvd_chapter_box), label20, FALSE, FALSE, 0);

  chapter_label = gtk_label_new (_("---"));
  gtk_widget_ref (chapter_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "chapter_label", chapter_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chapter_label);
  gtk_box_pack_start (GTK_BOX (dvd_chapter_box), chapter_label, FALSE, FALSE, 0);

  button_chapter_prev = gnome_stock_button (GNOME_STOCK_BUTTON_DOWN);
  gtk_widget_ref (button_chapter_prev);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "button_chapter_prev", button_chapter_prev,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (button_chapter_prev);
  gtk_box_pack_start (GTK_BOX (dvd_chapter_box), button_chapter_prev, FALSE, FALSE, 0);
  gtk_tooltips_set_tip (tooltips, button_chapter_prev, _("Select previous chapter"), NULL);

  button_chapter_next = gnome_stock_button (GNOME_STOCK_BUTTON_UP);
  gtk_widget_ref (button_chapter_next);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "button_chapter_next", button_chapter_next,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (button_chapter_next);
  gtk_box_pack_start (GTK_BOX (dvd_chapter_box), button_chapter_next, FALSE, FALSE, 0);
  gtk_tooltips_set_tip (tooltips, button_chapter_next, _("Select next chapter"), NULL);

  network_box = gtk_hbox_new (TRUE, 0);
  gtk_widget_ref (network_box);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "network_box", network_box,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_box_pack_start (GTK_BOX (vbox8), network_box, FALSE, FALSE, 0);

  network_address_label = gtk_label_new (_("No server"));
  gtk_widget_ref (network_address_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "network_address_label", network_address_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_address_label);
  gtk_box_pack_start (GTK_BOX (network_box), network_address_label, FALSE, FALSE, 0);

  network_channel_box = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (network_channel_box);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "network_channel_box", network_channel_box,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_channel_box);
  gtk_box_pack_start (GTK_BOX (network_box), network_channel_box, FALSE, FALSE, 0);

  label_network = gtk_label_new (_("Network Channel:"));
  gtk_widget_ref (label_network);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "label_network", label_network,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label_network);
  gtk_box_pack_start (GTK_BOX (network_channel_box), label_network, TRUE, FALSE, 5);

  network_channel_spinbutton_adj = gtk_adjustment_new (0, 0, 100, 1, 10, 10);
  network_channel_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (network_channel_spinbutton_adj), 1, 0);
  gtk_widget_ref (network_channel_spinbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "network_channel_spinbutton", network_channel_spinbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_channel_spinbutton);
  gtk_box_pack_start (GTK_BOX (network_channel_box), network_channel_spinbutton, FALSE, TRUE, 5);

  network_channel_go_button = gtk_button_new_with_label (_("Go!"));
  gtk_widget_ref (network_channel_go_button);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "network_channel_go_button", network_channel_go_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_channel_go_button);
  gtk_box_pack_start (GTK_BOX (network_channel_box), network_channel_go_button, FALSE, FALSE, 0);
  gtk_button_set_relief (GTK_BUTTON (network_channel_go_button), GTK_RELIEF_NONE);

  appbar = gnome_appbar_new (FALSE, TRUE, GNOME_PREFERENCES_NEVER);
  gtk_widget_ref (appbar);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "appbar", appbar,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (appbar);
  gnome_app_set_statusbar (GNOME_APP (intf_window), appbar);
  gtk_widget_set_usize (appbar, 500, -2);

  gtk_signal_connect (GTK_OBJECT (intf_window), "delete_event",
                      GTK_SIGNAL_FUNC (GtkWindowDelete),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (intf_window), "drag_data_received",
                      GTK_SIGNAL_FUNC (GtkWindowDrag),
                      "intf_window");
  gnome_app_install_menu_hints (GNOME_APP (intf_window), menubar_uiinfo);
  gtk_signal_connect (GTK_OBJECT (toolbar_file), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkFileOpenShow),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (toolbar_disc), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkDiscOpenShow),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (toolbar_network), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkNetworkOpenShow),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (toolbar_sat), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkSatOpenShow),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (toolbar_back), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkControlBack),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (toolbar_stop), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkControlStop),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (toolbar_eject), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkDiscEject),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (toolbar_play), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkControlPlay),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (toolbar_pause), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkControlPause),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (toolbar_slow), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkControlSlow),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (toolbar_fast), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkControlFast),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (toolbar_playlist), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkPlaylistShow),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (toolbar_prev), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkPlaylistPrev),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (toolbar_next), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkPlaylistNext),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (slider), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkSliderPress),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (slider), "button_release_event",
                      GTK_SIGNAL_FUNC (GtkSliderRelease),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (button_title_prev), "clicked",
                      GTK_SIGNAL_FUNC (GtkTitlePrev),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (button_title_next), "clicked",
                      GTK_SIGNAL_FUNC (GtkTitleNext),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (button_chapter_prev), "clicked",
                      GTK_SIGNAL_FUNC (GtkChapterPrev),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (button_chapter_next), "clicked",
                      GTK_SIGNAL_FUNC (GtkChapterNext),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (network_channel_spinbutton), "activate",
                      GTK_SIGNAL_FUNC (GtkNetworkJoin),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (network_channel_go_button), "clicked",
                      GTK_SIGNAL_FUNC (GtkChannelGo),
                      "intf_window");

  gtk_object_set_data (GTK_OBJECT (intf_window), "tooltips", tooltips);

  return intf_window;
}

static GnomeUIInfo popup_file_menu_uiinfo[] =
{
  {
    GNOME_APP_UI_ITEM, N_("_Open File..."),
    N_("Open a File"),
    (gpointer) GnomePopupFileOpenActivate, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Open _Disc..."),
    N_("Open a DVD or VCD"),
    (gpointer) GnomePopupDiscOpenActivate, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CDROM,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Network Stream..."),
    N_("Select a Network Stream"),
    (gpointer) GnomePopupNetworkOpenActivate, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_REFRESH,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_ABOUT_ITEM (GnomePopupAboutActivate, NULL),
  GNOMEUIINFO_END
};

static GnomeUIInfo intf_popup_uiinfo[] =
{
  {
    GNOME_APP_UI_ITEM, N_("Play"),
    NULL,
    (gpointer) GnomePopupPlayActivate, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_FORWARD,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Pause"),
    NULL,
    (gpointer) GnomePopupPauseActivate, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_BOTTOM,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Stop"),
    NULL,
    (gpointer) GnomePopupStopActivate, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_STOP,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Back"),
    NULL,
    (gpointer) GnomePopupBackActivate, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_BACK,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Slow"),
    NULL,
    (gpointer) GnomePopupSlowActivate, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_TIMER_STOP,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Fast"),
    NULL,
    (gpointer) GnomePopupFastActivate, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_TIMER,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("Toggle _Interface"),
    NULL,
    (gpointer) GnomePopupWindowToggleActivate, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Fullscreen"),
    N_("Toggle fullscreen mode"),
    (gpointer) GnomePopupFullscreenActivate, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("Next"),
    NULL,
    (gpointer) GnomePopupNextActivate, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Prev"),
    NULL,
    (gpointer) GnomePopupPrevActivate, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Jump..."),
    N_("Got directly so specified point"),
    (gpointer) GnomePopupJumpActivate, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_JUMP_TO,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Program"),
    N_("Switch program"),
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Navigation"),
    N_("Navigate through titles and chapters"),
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SEARCH,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Audio"),
    N_("Select audio channel"),
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_VOLUME,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Subtitles"),
    N_("Select subtitle channel"),
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_FONT,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_FILE_TREE (popup_file_menu_uiinfo),
  {
    GNOME_APP_UI_ITEM, N_("Playlist..."),
    NULL,
    (gpointer) GnomePopupPlaylistActivate, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_MENU_PREFERENCES_ITEM (GnomePopupPreferencesActivate, NULL),
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_EXIT_ITEM (GnomePopupExitActivate, NULL),
  GNOMEUIINFO_END
};

GtkWidget*
create_intf_popup (void)
{
  GtkWidget *intf_popup;

  intf_popup = gtk_menu_new ();
  gtk_object_set_data (GTK_OBJECT (intf_popup), "intf_popup", intf_popup);
  gnome_app_fill_menu (GTK_MENU_SHELL (intf_popup), intf_popup_uiinfo,
                       NULL, FALSE, 0);

  gtk_widget_ref (intf_popup_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_play",
                            intf_popup_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[1].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_pause",
                            intf_popup_uiinfo[1].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[2].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_stop",
                            intf_popup_uiinfo[2].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[3].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_back",
                            intf_popup_uiinfo[3].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (intf_popup_uiinfo[3].widget, FALSE);

  gtk_widget_ref (intf_popup_uiinfo[4].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_slow",
                            intf_popup_uiinfo[4].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[5].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_fast",
                            intf_popup_uiinfo[5].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[6].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator3",
                            intf_popup_uiinfo[6].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[7].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_interface_toggle",
                            intf_popup_uiinfo[7].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[8].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_fullscreen",
                            intf_popup_uiinfo[8].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[9].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator8",
                            intf_popup_uiinfo[9].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[10].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_next",
                            intf_popup_uiinfo[10].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[11].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_prev",
                            intf_popup_uiinfo[11].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[12].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_jump",
                            intf_popup_uiinfo[12].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[13].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_program",
                            intf_popup_uiinfo[13].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (intf_popup_uiinfo[13].widget, FALSE);

  gtk_widget_ref (intf_popup_uiinfo[14].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_navigation",
                            intf_popup_uiinfo[14].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (intf_popup_uiinfo[14].widget, FALSE);

  gtk_widget_ref (intf_popup_uiinfo[15].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_audio",
                            intf_popup_uiinfo[15].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (intf_popup_uiinfo[15].widget, FALSE);

  gtk_widget_ref (intf_popup_uiinfo[16].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_subpictures",
                            intf_popup_uiinfo[16].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (intf_popup_uiinfo[16].widget, FALSE);

  gtk_widget_ref (intf_popup_uiinfo[17].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator13",
                            intf_popup_uiinfo[17].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[18].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_file",
                            intf_popup_uiinfo[18].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (popup_file_menu_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_open",
                            popup_file_menu_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (popup_file_menu_uiinfo[1].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_disc",
                            popup_file_menu_uiinfo[1].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (popup_file_menu_uiinfo[2].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_network",
                            popup_file_menu_uiinfo[2].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (popup_file_menu_uiinfo[3].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator4",
                            popup_file_menu_uiinfo[3].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (popup_file_menu_uiinfo[4].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_about",
                            popup_file_menu_uiinfo[4].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[19].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_playlist",
                            intf_popup_uiinfo[19].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[20].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_preferences",
                            intf_popup_uiinfo[20].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[21].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator2",
                            intf_popup_uiinfo[21].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[22].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_exit",
                            intf_popup_uiinfo[22].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  return intf_popup;
}

GtkWidget*
create_intf_about (void)
{
  const gchar *authors[] = {
    "the VideoLAN team <videolan@videolan.org>",
    "http://www.videolan.org/",
    NULL
  };
  GtkWidget *intf_about;

  intf_about = gnome_about_new ("VideoLAN Client", VERSION,
                        _("(C) 1996, 1997, 1998, 1999, 2000, 2001, 2002  - the VideoLAN Team"),
                        authors,
                        _("This is the VideoLAN client, a DVD and MPEG player. It can play MPEG and MPEG 2 files from a file or from a network source."),
                        NULL);
  gtk_object_set_data (GTK_OBJECT (intf_about), "intf_about", intf_about);

  return intf_about;
}

GtkWidget*
create_intf_fileopen (void)
{
  GtkWidget *intf_fileopen;
  GtkWidget *fileopen_ok;
  GtkWidget *fileopen_cancel;

  intf_fileopen = gtk_file_selection_new (_("Open File"));
  gtk_object_set_data (GTK_OBJECT (intf_fileopen), "intf_fileopen", intf_fileopen);
  gtk_container_set_border_width (GTK_CONTAINER (intf_fileopen), 10);
  gtk_window_set_modal (GTK_WINDOW (intf_fileopen), TRUE);
  gtk_file_selection_hide_fileop_buttons (GTK_FILE_SELECTION (intf_fileopen));

  fileopen_ok = GTK_FILE_SELECTION (intf_fileopen)->ok_button;
  gtk_object_set_data (GTK_OBJECT (intf_fileopen), "fileopen_ok", fileopen_ok);
  gtk_widget_show (fileopen_ok);
  GTK_WIDGET_SET_FLAGS (fileopen_ok, GTK_CAN_DEFAULT);

  fileopen_cancel = GTK_FILE_SELECTION (intf_fileopen)->cancel_button;
  gtk_object_set_data (GTK_OBJECT (intf_fileopen), "fileopen_cancel", fileopen_cancel);
  gtk_widget_show (fileopen_cancel);
  GTK_WIDGET_SET_FLAGS (fileopen_cancel, GTK_CAN_DEFAULT);

  gtk_signal_connect (GTK_OBJECT (fileopen_ok), "clicked",
                      GTK_SIGNAL_FUNC (GtkFileOpenOk),
                      "intf_fileopen");
  gtk_signal_connect (GTK_OBJECT (fileopen_cancel), "clicked",
                      GTK_SIGNAL_FUNC (GtkFileOpenCancel),
                      "intf_fileopen");

  return intf_fileopen;
}

GtkWidget*
create_intf_modules (void)
{
  GtkWidget *intf_modules;
  GtkWidget *dialog_vbox1;
  GtkWidget *label12;
  GtkWidget *dialog_action_area1;
  GtkWidget *modules_ok;
  GtkWidget *modules_apply;
  GtkWidget *modules_cancel;

  intf_modules = gnome_dialog_new (_("Modules"), NULL);
  gtk_object_set_data (GTK_OBJECT (intf_modules), "intf_modules", intf_modules);
  gtk_window_set_policy (GTK_WINDOW (intf_modules), FALSE, FALSE, FALSE);

  dialog_vbox1 = GNOME_DIALOG (intf_modules)->vbox;
  gtk_object_set_data (GTK_OBJECT (intf_modules), "dialog_vbox1", dialog_vbox1);
  gtk_widget_show (dialog_vbox1);

  label12 = gtk_label_new (_("Sorry, the module manager isn't functional yet. Please retry in a later version."));
  gtk_widget_ref (label12);
  gtk_object_set_data_full (GTK_OBJECT (intf_modules), "label12", label12,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label12);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), label12, FALSE, FALSE, 0);

  dialog_action_area1 = GNOME_DIALOG (intf_modules)->action_area;
  gtk_object_set_data (GTK_OBJECT (intf_modules), "dialog_action_area1", dialog_action_area1);
  gtk_widget_show (dialog_action_area1);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area1), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (dialog_action_area1), 8);

  gnome_dialog_append_button (GNOME_DIALOG (intf_modules), GNOME_STOCK_BUTTON_OK);
  modules_ok = GTK_WIDGET (g_list_last (GNOME_DIALOG (intf_modules)->buttons)->data);
  gtk_widget_ref (modules_ok);
  gtk_object_set_data_full (GTK_OBJECT (intf_modules), "modules_ok", modules_ok,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (modules_ok);
  GTK_WIDGET_SET_FLAGS (modules_ok, GTK_CAN_DEFAULT);

  gnome_dialog_append_button (GNOME_DIALOG (intf_modules), GNOME_STOCK_BUTTON_APPLY);
  modules_apply = GTK_WIDGET (g_list_last (GNOME_DIALOG (intf_modules)->buttons)->data);
  gtk_widget_ref (modules_apply);
  gtk_object_set_data_full (GTK_OBJECT (intf_modules), "modules_apply", modules_apply,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (modules_apply);
  GTK_WIDGET_SET_FLAGS (modules_apply, GTK_CAN_DEFAULT);

  gnome_dialog_append_button (GNOME_DIALOG (intf_modules), GNOME_STOCK_BUTTON_CANCEL);
  modules_cancel = GTK_WIDGET (g_list_last (GNOME_DIALOG (intf_modules)->buttons)->data);
  gtk_widget_ref (modules_cancel);
  gtk_object_set_data_full (GTK_OBJECT (intf_modules), "modules_cancel", modules_cancel,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (modules_cancel);
  GTK_WIDGET_SET_FLAGS (modules_cancel, GTK_CAN_DEFAULT);

  return intf_modules;
}

GtkWidget*
create_intf_disc (void)
{
  GtkWidget *intf_disc;
  GtkWidget *dialog_vbox4;
  GtkWidget *hbox2;
  GtkWidget *frame1;
  GtkWidget *vbox4;
  GSList *disc_group = NULL;
  GtkWidget *disc_dvd;
  GtkWidget *disc_vcd;
  GtkWidget *frame2;
  GtkWidget *table1;
  GtkWidget *label15;
  GtkWidget *label16;
  GtkObject *disc_title_adj;
  GtkWidget *disc_title;
  GtkObject *disc_chapter_adj;
  GtkWidget *disc_chapter;
  GtkWidget *hbox1;
  GtkWidget *label14;
  GtkWidget *disc_name;
  GtkWidget *dialog_action_area4;
  GtkWidget *disc_ok;
  GtkWidget *disc_cancel;

  intf_disc = gnome_dialog_new (_("Open Disc"), NULL);
  gtk_object_set_data (GTK_OBJECT (intf_disc), "intf_disc", intf_disc);
  gtk_window_set_modal (GTK_WINDOW (intf_disc), TRUE);
  gtk_window_set_policy (GTK_WINDOW (intf_disc), FALSE, FALSE, FALSE);

  dialog_vbox4 = GNOME_DIALOG (intf_disc)->vbox;
  gtk_object_set_data (GTK_OBJECT (intf_disc), "dialog_vbox4", dialog_vbox4);
  gtk_widget_show (dialog_vbox4);

  hbox2 = gtk_hbox_new (FALSE, 5);
  gtk_widget_ref (hbox2);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "hbox2", hbox2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox2);
  gtk_box_pack_start (GTK_BOX (dialog_vbox4), hbox2, TRUE, TRUE, 0);

  frame1 = gtk_frame_new (_("Disc type"));
  gtk_widget_ref (frame1);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "frame1", frame1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame1);
  gtk_box_pack_start (GTK_BOX (hbox2), frame1, TRUE, TRUE, 0);

  vbox4 = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (vbox4);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "vbox4", vbox4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox4);
  gtk_container_add (GTK_CONTAINER (frame1), vbox4);

  disc_dvd = gtk_radio_button_new_with_label (disc_group, _("DVD"));
  disc_group = gtk_radio_button_group (GTK_RADIO_BUTTON (disc_dvd));
  gtk_widget_ref (disc_dvd);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "disc_dvd", disc_dvd,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_dvd);
  gtk_box_pack_start (GTK_BOX (vbox4), disc_dvd, FALSE, FALSE, 0);

  disc_vcd = gtk_radio_button_new_with_label (disc_group, _("VCD"));
  disc_group = gtk_radio_button_group (GTK_RADIO_BUTTON (disc_vcd));
  gtk_widget_ref (disc_vcd);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "disc_vcd", disc_vcd,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_vcd);
  gtk_box_pack_start (GTK_BOX (vbox4), disc_vcd, FALSE, FALSE, 0);

  frame2 = gtk_frame_new (_("Starting position"));
  gtk_widget_ref (frame2);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "frame2", frame2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame2);
  gtk_box_pack_start (GTK_BOX (hbox2), frame2, TRUE, TRUE, 0);

  table1 = gtk_table_new (2, 2, FALSE);
  gtk_widget_ref (table1);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "table1", table1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table1);
  gtk_container_add (GTK_CONTAINER (frame2), table1);
  gtk_container_set_border_width (GTK_CONTAINER (table1), 5);
  gtk_table_set_row_spacings (GTK_TABLE (table1), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table1), 5);

  label15 = gtk_label_new (_("Title"));
  gtk_widget_ref (label15);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "label15", label15,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label15);
  gtk_table_attach (GTK_TABLE (table1), label15, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label15), 0, 0.5);

  label16 = gtk_label_new (_("Chapter"));
  gtk_widget_ref (label16);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "label16", label16,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label16);
  gtk_table_attach (GTK_TABLE (table1), label16, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label16), 0, 0.5);

  disc_title_adj = gtk_adjustment_new (1, 1, 65536, 1, 10, 10);
  disc_title = gtk_spin_button_new (GTK_ADJUSTMENT (disc_title_adj), 1, 0);
  gtk_widget_ref (disc_title);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "disc_title", disc_title,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_title);
  gtk_table_attach (GTK_TABLE (table1), disc_title, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  disc_chapter_adj = gtk_adjustment_new (1, 1, 65536, 1, 10, 10);
  disc_chapter = gtk_spin_button_new (GTK_ADJUSTMENT (disc_chapter_adj), 1, 0);
  gtk_widget_ref (disc_chapter);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "disc_chapter", disc_chapter,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_chapter);
  gtk_table_attach (GTK_TABLE (table1), disc_chapter, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  hbox1 = gtk_hbox_new (FALSE, 5);
  gtk_widget_ref (hbox1);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "hbox1", hbox1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox4), hbox1, TRUE, TRUE, 0);

  label14 = gtk_label_new (_("Device name:"));
  gtk_widget_ref (label14);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "label14", label14,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label14);
  gtk_box_pack_start (GTK_BOX (hbox1), label14, FALSE, FALSE, 0);

  disc_name = gtk_entry_new ();
  gtk_widget_ref (disc_name);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "disc_name", disc_name,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_name);
  gtk_box_pack_start (GTK_BOX (hbox1), disc_name, TRUE, TRUE, 0);
  gtk_entry_set_text (GTK_ENTRY (disc_name), config_GetPszVariable( "dvd_device" ));

  dialog_action_area4 = GNOME_DIALOG (intf_disc)->action_area;
  gtk_object_set_data (GTK_OBJECT (intf_disc), "dialog_action_area4", dialog_action_area4);
  gtk_widget_show (dialog_action_area4);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area4), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (dialog_action_area4), 8);

  gnome_dialog_append_button (GNOME_DIALOG (intf_disc), GNOME_STOCK_BUTTON_OK);
  disc_ok = GTK_WIDGET (g_list_last (GNOME_DIALOG (intf_disc)->buttons)->data);
  gtk_widget_ref (disc_ok);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "disc_ok", disc_ok,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_ok);
  GTK_WIDGET_SET_FLAGS (disc_ok, GTK_CAN_DEFAULT);

  gnome_dialog_append_button (GNOME_DIALOG (intf_disc), GNOME_STOCK_BUTTON_CANCEL);
  disc_cancel = GTK_WIDGET (g_list_last (GNOME_DIALOG (intf_disc)->buttons)->data);
  gtk_widget_ref (disc_cancel);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "disc_cancel", disc_cancel,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_cancel);
  GTK_WIDGET_SET_FLAGS (disc_cancel, GTK_CAN_DEFAULT);

  gtk_signal_connect (GTK_OBJECT (disc_dvd), "toggled",
                      GTK_SIGNAL_FUNC (GtkDiscOpenDvd),
                      "intf_disc");
  gtk_signal_connect (GTK_OBJECT (disc_vcd), "toggled",
                      GTK_SIGNAL_FUNC (GtkDiscOpenVcd),
                      "intf_disc");
  gtk_signal_connect (GTK_OBJECT (disc_ok), "clicked",
                      GTK_SIGNAL_FUNC (GtkDiscOpenOk),
                      "intf_disc");
  gtk_signal_connect (GTK_OBJECT (disc_cancel), "clicked",
                      GTK_SIGNAL_FUNC (GtkDiscOpenCancel),
                      "intf_disc");

  return intf_disc;
}

GtkWidget*
create_intf_network (void)
{
  GtkWidget *intf_network;
  GtkWidget *vbox5;
  GtkWidget *hbox3;
  GtkWidget *frame3;
  GtkWidget *vbox6;
  GSList *network_group = NULL;
  GtkWidget *network_ts;
  GtkWidget *network_rtp;
  GtkWidget *network_http;
  GtkWidget *frame4;
  GtkWidget *table2;
  GtkWidget *network_server_label;
  GtkWidget *network_port_label;
  GtkObject *network_port_adj;
  GtkWidget *network_port;
  GtkWidget *network_broadcast_check;
  GtkWidget *network_broadcast_combo;
  GtkWidget *network_broadcast;
  GtkWidget *network_server_combo;
  GtkWidget *network_server;
  GtkWidget *frame5;
  GtkWidget *hbox4;
  GtkWidget *network_channel_check;
  GtkWidget *network_channel_combo;
  GtkWidget *network_channel;
  GtkWidget *network_channel_port_label;
  GtkObject *network_channel_port_adj;
  GtkWidget *network_channel_port;
  GtkWidget *hbuttonbox1;
  GtkWidget *network_ok;
  GtkWidget *network_cancel;
  GtkTooltips *tooltips;

  tooltips = gtk_tooltips_new ();

  intf_network = gnome_dialog_new (_("Network Stream"), NULL);
  gtk_object_set_data (GTK_OBJECT (intf_network), "intf_network", intf_network);
  gtk_window_set_modal (GTK_WINDOW (intf_network), TRUE);
  gtk_window_set_policy (GTK_WINDOW (intf_network), FALSE, FALSE, FALSE);

  vbox5 = GNOME_DIALOG (intf_network)->vbox;
  gtk_object_set_data (GTK_OBJECT (intf_network), "vbox5", vbox5);
  gtk_widget_show (vbox5);

  hbox3 = gtk_hbox_new (FALSE, 5);
  gtk_widget_ref (hbox3);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "hbox3", hbox3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox3);
  gtk_box_pack_start (GTK_BOX (vbox5), hbox3, TRUE, TRUE, 0);

  frame3 = gtk_frame_new (_("Protocol"));
  gtk_widget_ref (frame3);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "frame3", frame3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame3);
  gtk_box_pack_start (GTK_BOX (hbox3), frame3, TRUE, TRUE, 0);

  vbox6 = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (vbox6);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "vbox6", vbox6,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox6);
  gtk_container_add (GTK_CONTAINER (frame3), vbox6);

  network_ts = gtk_radio_button_new_with_label (network_group, _("TS"));
  network_group = gtk_radio_button_group (GTK_RADIO_BUTTON (network_ts));
  gtk_widget_ref (network_ts);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_ts", network_ts,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_ts);
  gtk_box_pack_start (GTK_BOX (vbox6), network_ts, FALSE, FALSE, 0);

  network_rtp = gtk_radio_button_new_with_label (network_group, _("RTP"));
  network_group = gtk_radio_button_group (GTK_RADIO_BUTTON (network_rtp));
  gtk_widget_ref (network_rtp);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_rtp", network_rtp,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_rtp);
  gtk_box_pack_start (GTK_BOX (vbox6), network_rtp, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (network_rtp, FALSE);

  network_http = gtk_radio_button_new_with_label (network_group, _("HTTP"));
  network_group = gtk_radio_button_group (GTK_RADIO_BUTTON (network_http));
  gtk_widget_ref (network_http);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_http", network_http,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_http);
  gtk_box_pack_start (GTK_BOX (vbox6), network_http, FALSE, FALSE, 0);

  frame4 = gtk_frame_new (_("Server"));
  gtk_widget_ref (frame4);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "frame4", frame4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame4);
  gtk_box_pack_start (GTK_BOX (hbox3), frame4, TRUE, TRUE, 0);

  table2 = gtk_table_new (3, 2, FALSE);
  gtk_widget_ref (table2);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "table2", table2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table2);
  gtk_container_add (GTK_CONTAINER (frame4), table2);
  gtk_container_set_border_width (GTK_CONTAINER (table2), 5);
  gtk_table_set_row_spacings (GTK_TABLE (table2), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table2), 5);

  network_server_label = gtk_label_new (_("Address"));
  gtk_widget_ref (network_server_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_server_label", network_server_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_server_label);
  gtk_table_attach (GTK_TABLE (table2), network_server_label, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (network_server_label), 0, 0.5);

  network_port_label = gtk_label_new (_("Port"));
  gtk_widget_ref (network_port_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_port_label", network_port_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_port_label);
  gtk_table_attach (GTK_TABLE (table2), network_port_label, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (network_port_label), 0, 0.5);

  network_port_adj = gtk_adjustment_new (1234, 1024, 65535, 1, 10, 10);
  network_port = gtk_spin_button_new (GTK_ADJUSTMENT (network_port_adj), 1, 0);
  gtk_widget_ref (network_port);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_port", network_port,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_port);
  gtk_table_attach (GTK_TABLE (table2), network_port, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_tooltips_set_tip (tooltips, network_port, _("Port of the stream server"), NULL);

  network_broadcast_check = gtk_check_button_new_with_label (_("Broadcast"));
  gtk_widget_ref (network_broadcast_check);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_broadcast_check", network_broadcast_check,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_broadcast_check);
  gtk_table_attach (GTK_TABLE (table2), network_broadcast_check, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  network_broadcast_combo = gnome_entry_new (NULL);
  gtk_widget_ref (network_broadcast_combo);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_broadcast_combo", network_broadcast_combo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_broadcast_combo);
  gtk_table_attach (GTK_TABLE (table2), network_broadcast_combo, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  network_broadcast = gnome_entry_gtk_entry (GNOME_ENTRY (network_broadcast_combo));
  gtk_widget_ref (network_broadcast);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_broadcast", network_broadcast,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_broadcast);
  gtk_widget_set_sensitive (network_broadcast, FALSE);
  gtk_entry_set_text (GTK_ENTRY (network_broadcast), _("138.195.143.255"));

  network_server_combo = gnome_entry_new (NULL);
  gtk_widget_ref (network_server_combo);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_server_combo", network_server_combo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_server_combo);
  gtk_table_attach (GTK_TABLE (table2), network_server_combo, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  network_server = gnome_entry_gtk_entry (GNOME_ENTRY (network_server_combo));
  gtk_widget_ref (network_server);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_server", network_server,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_server);
  gtk_entry_set_text (GTK_ENTRY (network_server), _("vls"));

  frame5 = gtk_frame_new (_("Channels"));
  gtk_widget_ref (frame5);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "frame5", frame5,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame5);
  gtk_box_pack_start (GTK_BOX (vbox5), frame5, TRUE, TRUE, 0);
  gtk_frame_set_label_align (GTK_FRAME (frame5), 0.05, 0.5);

  hbox4 = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (hbox4);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "hbox4", hbox4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox4);
  gtk_container_add (GTK_CONTAINER (frame5), hbox4);

  network_channel_check = gtk_check_button_new_with_label (_("Channel server:"));
  gtk_widget_ref (network_channel_check);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_channel_check", network_channel_check,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_channel_check);
  gtk_box_pack_start (GTK_BOX (hbox4), network_channel_check, FALSE, FALSE, 0);

  network_channel_combo = gnome_entry_new (NULL);
  gtk_widget_ref (network_channel_combo);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_channel_combo", network_channel_combo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_channel_combo);
  gtk_box_pack_start (GTK_BOX (hbox4), network_channel_combo, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (network_channel_combo, FALSE);

  network_channel = gnome_entry_gtk_entry (GNOME_ENTRY (network_channel_combo));
  gtk_widget_ref (network_channel);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_channel", network_channel,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_channel);
  gtk_entry_set_text (GTK_ENTRY (network_channel), _("138.195.143.120"));

  network_channel_port_label = gtk_label_new (_("port:"));
  gtk_widget_ref (network_channel_port_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_channel_port_label", network_channel_port_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_channel_port_label);
  gtk_box_pack_start (GTK_BOX (hbox4), network_channel_port_label, FALSE, FALSE, 5);

  network_channel_port_adj = gtk_adjustment_new (6010, 1024, 65535, 1, 10, 10);
  network_channel_port = gtk_spin_button_new (GTK_ADJUSTMENT (network_channel_port_adj), 1, 0);
  gtk_widget_ref (network_channel_port);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_channel_port", network_channel_port,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_channel_port);
  gtk_box_pack_start (GTK_BOX (hbox4), network_channel_port, FALSE, FALSE, 0);
  gtk_widget_set_usize (network_channel_port, 60, -2);
  gtk_widget_set_sensitive (network_channel_port, FALSE);

  hbuttonbox1 = GNOME_DIALOG (intf_network)->action_area;
  gtk_object_set_data (GTK_OBJECT (intf_network), "hbuttonbox1", hbuttonbox1);
  gtk_widget_show (hbuttonbox1);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox1), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbuttonbox1), 8);

  gnome_dialog_append_button (GNOME_DIALOG (intf_network), GNOME_STOCK_BUTTON_OK);
  network_ok = GTK_WIDGET (g_list_last (GNOME_DIALOG (intf_network)->buttons)->data);
  gtk_widget_ref (network_ok);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_ok", network_ok,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_ok);
  GTK_WIDGET_SET_FLAGS (network_ok, GTK_CAN_DEFAULT);

  gnome_dialog_append_button (GNOME_DIALOG (intf_network), GNOME_STOCK_BUTTON_CANCEL);
  network_cancel = GTK_WIDGET (g_list_last (GNOME_DIALOG (intf_network)->buttons)->data);
  gtk_widget_ref (network_cancel);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_cancel", network_cancel,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_cancel);
  GTK_WIDGET_SET_FLAGS (network_cancel, GTK_CAN_DEFAULT);

  gtk_signal_connect (GTK_OBJECT (network_broadcast_check), "toggled",
                      GTK_SIGNAL_FUNC (GtkNetworkOpenBroadcast),
                      "intf_network");
  gtk_signal_connect (GTK_OBJECT (network_channel_check), "toggled",
                      GTK_SIGNAL_FUNC (GtkNetworkOpenChannel),
                      "intf_network");
  gtk_signal_connect (GTK_OBJECT (network_ok), "clicked",
                      GTK_SIGNAL_FUNC (GtkNetworkOpenOk),
                      "intf_network");
  gtk_signal_connect (GTK_OBJECT (network_cancel), "clicked",
                      GTK_SIGNAL_FUNC (GtkNetworkOpenCancel),
                      "intf_network");

  gtk_object_set_data (GTK_OBJECT (intf_network), "tooltips", tooltips);

  return intf_network;
}

static GnomeUIInfo playlist_add_menu_uiinfo[] =
{
  {
    GNOME_APP_UI_ITEM, N_("Disc"),
    NULL,
    (gpointer) GnomePlaylistDiscOpenActivate, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("File"),
    NULL,
    (gpointer) GnomePlaylistFileOpenActivate, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Network"),
    NULL,
    (gpointer) GnomePlaylistNetworkOpenActivate, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Url"),
    NULL,
    (gpointer) GtkPlaylistAddUrl, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo playlist_delete_menu_uiinfo[] =
{
  {
    GNOME_APP_UI_ITEM, N_("All"),
    NULL,
    (gpointer) GtkPlaylistDeleteAll, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Item"),
    NULL,
    (gpointer) GtkPlaylistDeleteSelected, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo playlist_selection_menu_uiinfo[] =
{
  {
    GNOME_APP_UI_ITEM, N_("Crop"),
    NULL,
    (gpointer) GtkPlaylistCrop, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Invert"),
    NULL,
    (gpointer) GtkPlaylistInvert, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Select"),
    NULL,
    (gpointer) GtkPlaylistSelect, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo playlist_menubar_uiinfo[] =
{
  {
    GNOME_APP_UI_SUBTREE, N_("Add"),
    NULL,
    playlist_add_menu_uiinfo, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_SUBTREE, N_("Delete"),
    NULL,
    playlist_delete_menu_uiinfo, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_SUBTREE, N_("Selection"),
    NULL,
    playlist_selection_menu_uiinfo, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_END
};

GtkWidget*
create_intf_playlist (void)
{
  GtkWidget *intf_playlist;
  GtkWidget *playlist_vbox;
  GtkWidget *playlist_scrolledwindow;
  GtkWidget *playlist_viewport;
  GtkWidget *playlist_frame;
  GtkWidget *playlist_clist;
  GtkWidget *playlist_label_url;
  GtkWidget *playlist_label_duration;
  GtkWidget *playlist_menubar;
  GtkWidget *playlist_action;
  GtkWidget *playlist_ok;
  GtkWidget *playlist_cancel;

  intf_playlist = gnome_dialog_new (_("Playlist"), NULL);
  gtk_object_set_data (GTK_OBJECT (intf_playlist), "intf_playlist", intf_playlist);
  gtk_window_set_default_size (GTK_WINDOW (intf_playlist), 400, 300);
  gtk_window_set_policy (GTK_WINDOW (intf_playlist), TRUE, TRUE, FALSE);

  playlist_vbox = GNOME_DIALOG (intf_playlist)->vbox;
  gtk_object_set_data (GTK_OBJECT (intf_playlist), "playlist_vbox", playlist_vbox);
  gtk_widget_show (playlist_vbox);

  playlist_scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (playlist_scrolledwindow);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_scrolledwindow", playlist_scrolledwindow,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_scrolledwindow);
  gtk_box_pack_start (GTK_BOX (playlist_vbox), playlist_scrolledwindow, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (playlist_scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  playlist_viewport = gtk_viewport_new (NULL, NULL);
  gtk_widget_ref (playlist_viewport);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_viewport", playlist_viewport,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_viewport);
  gtk_container_add (GTK_CONTAINER (playlist_scrolledwindow), playlist_viewport);

  playlist_frame = gtk_frame_new (_("Playlist"));
  gtk_widget_ref (playlist_frame);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_frame", playlist_frame,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_frame);
  gtk_container_add (GTK_CONTAINER (playlist_viewport), playlist_frame);
  gtk_container_set_border_width (GTK_CONTAINER (playlist_frame), 4);
  gtk_frame_set_label_align (GTK_FRAME (playlist_frame), 0.05, 0.5);

  playlist_clist = gtk_clist_new (2);
  gtk_widget_ref (playlist_clist);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_clist", playlist_clist,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_clist);
  gtk_container_add (GTK_CONTAINER (playlist_frame), playlist_clist);
  gtk_clist_set_column_width (GTK_CLIST (playlist_clist), 0, 287);
  gtk_clist_set_column_width (GTK_CLIST (playlist_clist), 1, 70);
  gtk_clist_column_titles_show (GTK_CLIST (playlist_clist));

  playlist_label_url = gtk_label_new (_("Url"));
  gtk_widget_ref (playlist_label_url);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_label_url", playlist_label_url,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_label_url);
  gtk_clist_set_column_widget (GTK_CLIST (playlist_clist), 0, playlist_label_url);

  playlist_label_duration = gtk_label_new (_("Duration"));
  gtk_widget_ref (playlist_label_duration);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_label_duration", playlist_label_duration,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_label_duration);
  gtk_clist_set_column_widget (GTK_CLIST (playlist_clist), 1, playlist_label_duration);

  playlist_menubar = gtk_menu_bar_new ();
  gtk_widget_ref (playlist_menubar);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_menubar", playlist_menubar,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_menubar);
  gtk_box_pack_start (GTK_BOX (playlist_vbox), playlist_menubar, FALSE, FALSE, 0);
  gnome_app_fill_menu (GTK_MENU_SHELL (playlist_menubar), playlist_menubar_uiinfo,
                       NULL, FALSE, 0);

  gtk_widget_ref (playlist_menubar_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_add",
                            playlist_menubar_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (playlist_add_menu_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_add_disc",
                            playlist_add_menu_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (playlist_add_menu_uiinfo[1].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_add_file",
                            playlist_add_menu_uiinfo[1].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (playlist_add_menu_uiinfo[2].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_add_network",
                            playlist_add_menu_uiinfo[2].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (playlist_add_menu_uiinfo[3].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_add_url",
                            playlist_add_menu_uiinfo[3].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (playlist_menubar_uiinfo[1].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_delete",
                            playlist_menubar_uiinfo[1].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (playlist_delete_menu_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_delete_all",
                            playlist_delete_menu_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (playlist_delete_menu_uiinfo[1].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_delete_item",
                            playlist_delete_menu_uiinfo[1].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (playlist_menubar_uiinfo[2].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_selection",
                            playlist_menubar_uiinfo[2].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (playlist_selection_menu_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_selection_crop",
                            playlist_selection_menu_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (playlist_selection_menu_uiinfo[1].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_selection_invert",
                            playlist_selection_menu_uiinfo[1].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (playlist_selection_menu_uiinfo[2].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_selection_select",
                            playlist_selection_menu_uiinfo[2].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  playlist_action = GNOME_DIALOG (intf_playlist)->action_area;
  gtk_object_set_data (GTK_OBJECT (intf_playlist), "playlist_action", playlist_action);
  gtk_widget_show (playlist_action);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (playlist_action), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (playlist_action), 8);
  gtk_button_box_set_child_size (GTK_BUTTON_BOX (playlist_action), 100, 38);

  gnome_dialog_append_button (GNOME_DIALOG (intf_playlist), GNOME_STOCK_BUTTON_OK);
  playlist_ok = GTK_WIDGET (g_list_last (GNOME_DIALOG (intf_playlist)->buttons)->data);
  gtk_widget_ref (playlist_ok);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_ok", playlist_ok,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_ok);
  GTK_WIDGET_SET_FLAGS (playlist_ok, GTK_CAN_DEFAULT);

  gnome_dialog_append_button (GNOME_DIALOG (intf_playlist), GNOME_STOCK_BUTTON_CANCEL);
  playlist_cancel = GTK_WIDGET (g_list_last (GNOME_DIALOG (intf_playlist)->buttons)->data);
  gtk_widget_ref (playlist_cancel);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_cancel", playlist_cancel,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_cancel);
  GTK_WIDGET_SET_FLAGS (playlist_cancel, GTK_CAN_DEFAULT);

  gtk_signal_connect (GTK_OBJECT (intf_playlist), "destroy",
                      GTK_SIGNAL_FUNC (gtk_widget_hide),
                      "intf_playlist");
  gtk_signal_connect (GTK_OBJECT (intf_playlist), "delete_event",
                      GTK_SIGNAL_FUNC (gtk_widget_hide),
                      "intf_playlist");
  gtk_signal_connect (GTK_OBJECT (playlist_clist), "event",
                      GTK_SIGNAL_FUNC (GtkPlaylistEvent),
                      "intf_playlist");
  gtk_signal_connect (GTK_OBJECT (playlist_clist), "drag_data_received",
                      GTK_SIGNAL_FUNC (GtkPlaylistDragData),
                      "intf_playlist");
  gtk_signal_connect (GTK_OBJECT (playlist_clist), "drag_motion",
                      GTK_SIGNAL_FUNC (GtkPlaylistDragMotion),
                      "intf_playlist");
  gtk_signal_connect (GTK_OBJECT (playlist_ok), "clicked",
                      GTK_SIGNAL_FUNC (GtkPlaylistOk),
                      "intf_playlist");
  gtk_signal_connect (GTK_OBJECT (playlist_cancel), "clicked",
                      GTK_SIGNAL_FUNC (GtkPlaylistCancel),
                      "intf_playlist");

  return intf_playlist;
}

GtkWidget*
create_intf_jump (void)
{
  GtkWidget *intf_jump;
  GtkWidget *jump_vbox;
  GtkWidget *jump_frame;
  GtkWidget *jump_box;
  GtkWidget *jump_label3;
  GtkObject *jump_second_spinbutton_adj;
  GtkWidget *jump_second_spinbutton;
  GtkWidget *jump_label1;
  GtkObject *jump_minute_spinbutton_adj;
  GtkWidget *jump_minute_spinbutton;
  GtkWidget *jump_label2;
  GtkObject *jump_hour_spinbutton_adj;
  GtkWidget *jump_hour_spinbutton;
  GtkWidget *jump_action;
  GtkWidget *jump_ok;
  GtkWidget *jump_cancel;

  intf_jump = gnome_dialog_new (NULL, NULL);
  gtk_object_set_data (GTK_OBJECT (intf_jump), "intf_jump", intf_jump);
  gtk_window_set_policy (GTK_WINDOW (intf_jump), FALSE, FALSE, FALSE);

  jump_vbox = GNOME_DIALOG (intf_jump)->vbox;
  gtk_object_set_data (GTK_OBJECT (intf_jump), "jump_vbox", jump_vbox);
  gtk_widget_show (jump_vbox);

  jump_frame = gtk_frame_new (_("Jump to: "));
  gtk_widget_ref (jump_frame);
  gtk_object_set_data_full (GTK_OBJECT (intf_jump), "jump_frame", jump_frame,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (jump_frame);
  gtk_box_pack_start (GTK_BOX (jump_vbox), jump_frame, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (jump_frame), 5);
  gtk_frame_set_label_align (GTK_FRAME (jump_frame), 0.05, 0.5);

  jump_box = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (jump_box);
  gtk_object_set_data_full (GTK_OBJECT (intf_jump), "jump_box", jump_box,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (jump_box);
  gtk_container_add (GTK_CONTAINER (jump_frame), jump_box);
  gtk_container_set_border_width (GTK_CONTAINER (jump_box), 5);

  jump_label3 = gtk_label_new (_("s."));
  gtk_widget_ref (jump_label3);
  gtk_object_set_data_full (GTK_OBJECT (intf_jump), "jump_label3", jump_label3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (jump_label3);
  gtk_box_pack_end (GTK_BOX (jump_box), jump_label3, FALSE, FALSE, 0);

  jump_second_spinbutton_adj = gtk_adjustment_new (0, 0, 100, 1, 10, 10);
  jump_second_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (jump_second_spinbutton_adj), 1, 0);
  gtk_widget_ref (jump_second_spinbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_jump), "jump_second_spinbutton", jump_second_spinbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (jump_second_spinbutton);
  gtk_box_pack_end (GTK_BOX (jump_box), jump_second_spinbutton, FALSE, FALSE, 5);

  jump_label1 = gtk_label_new (_("m:"));
  gtk_widget_ref (jump_label1);
  gtk_object_set_data_full (GTK_OBJECT (intf_jump), "jump_label1", jump_label1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (jump_label1);
  gtk_box_pack_end (GTK_BOX (jump_box), jump_label1, FALSE, FALSE, 5);

  jump_minute_spinbutton_adj = gtk_adjustment_new (0, 0, 59, 1, 10, 10);
  jump_minute_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (jump_minute_spinbutton_adj), 1, 0);
  gtk_widget_ref (jump_minute_spinbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_jump), "jump_minute_spinbutton", jump_minute_spinbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (jump_minute_spinbutton);
  gtk_box_pack_end (GTK_BOX (jump_box), jump_minute_spinbutton, FALSE, FALSE, 5);

  jump_label2 = gtk_label_new (_("h:"));
  gtk_widget_ref (jump_label2);
  gtk_object_set_data_full (GTK_OBJECT (intf_jump), "jump_label2", jump_label2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (jump_label2);
  gtk_box_pack_end (GTK_BOX (jump_box), jump_label2, FALSE, FALSE, 5);

  jump_hour_spinbutton_adj = gtk_adjustment_new (0, 0, 10, 1, 10, 10);
  jump_hour_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (jump_hour_spinbutton_adj), 1, 0);
  gtk_widget_ref (jump_hour_spinbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_jump), "jump_hour_spinbutton", jump_hour_spinbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (jump_hour_spinbutton);
  gtk_box_pack_end (GTK_BOX (jump_box), jump_hour_spinbutton, FALSE, FALSE, 5);

  jump_action = GNOME_DIALOG (intf_jump)->action_area;
  gtk_object_set_data (GTK_OBJECT (intf_jump), "jump_action", jump_action);
  gtk_widget_show (jump_action);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (jump_action), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (jump_action), 8);

  gnome_dialog_append_button (GNOME_DIALOG (intf_jump), GNOME_STOCK_BUTTON_OK);
  jump_ok = GTK_WIDGET (g_list_last (GNOME_DIALOG (intf_jump)->buttons)->data);
  gtk_widget_ref (jump_ok);
  gtk_object_set_data_full (GTK_OBJECT (intf_jump), "jump_ok", jump_ok,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (jump_ok);
  GTK_WIDGET_SET_FLAGS (jump_ok, GTK_CAN_DEFAULT);

  gnome_dialog_append_button (GNOME_DIALOG (intf_jump), GNOME_STOCK_BUTTON_CANCEL);
  jump_cancel = GTK_WIDGET (g_list_last (GNOME_DIALOG (intf_jump)->buttons)->data);
  gtk_widget_ref (jump_cancel);
  gtk_object_set_data_full (GTK_OBJECT (intf_jump), "jump_cancel", jump_cancel,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (jump_cancel);
  GTK_WIDGET_SET_FLAGS (jump_cancel, GTK_CAN_DEFAULT);

  gtk_signal_connect (GTK_OBJECT (jump_ok), "clicked",
                      GTK_SIGNAL_FUNC (GtkJumpOk),
                      "intf_jump");
  gtk_signal_connect (GTK_OBJECT (jump_cancel), "clicked",
                      GTK_SIGNAL_FUNC (GtkJumpCancel),
                      "intf_jump");

  return intf_jump;
}

GtkWidget*
create_intf_open (void)
{
  GtkWidget *intf_open;
  GtkWidget *dialog_vbox5;
  GtkWidget *tab_open;
  GtkWidget *vbox10;
  GtkWidget *hbox5;
  GtkWidget *frame6;
  GtkWidget *vbox11;
  GSList *vbox11_group = NULL;
  GtkWidget *radiobutton1;
  GtkWidget *radiobutton2;
  GtkWidget *frame7;
  GtkWidget *table3;
  GtkWidget *label24;
  GtkWidget *label25;
  GtkObject *spinbutton5_adj;
  GtkWidget *spinbutton5;
  GtkObject *spinbutton6_adj;
  GtkWidget *spinbutton6;
  GtkWidget *hbox6;
  GtkWidget *label29;
  GtkWidget *entry1;
  GtkWidget *tab_disc;
  GtkWidget *vbox12;
  GtkWidget *hbox7;
  GtkWidget *frame8;
  GtkWidget *vbox13;
  GSList *vbox13_group = NULL;
  GtkWidget *radiobutton3;
  GtkWidget *radiobutton4;
  GtkWidget *radiobutton5;
  GtkWidget *frame9;
  GtkWidget *vbox14;
  GtkWidget *table4;
  GtkWidget *label26;
  GtkWidget *label27;
  GtkWidget *combo1;
  GtkWidget *combo_entry1;
  GtkObject *spinbutton7_adj;
  GtkWidget *spinbutton7;
  GtkWidget *frame10;
  GtkWidget *hbox8;
  GtkWidget *checkbutton2;
  GtkWidget *combo3;
  GtkWidget *combo_entry3;
  GtkWidget *frame11;
  GtkWidget *hbox9;
  GtkWidget *checkbutton1;
  GtkWidget *combo2;
  GtkWidget *combo_entry2;
  GtkWidget *label28;
  GtkObject *spinbutton8_adj;
  GtkWidget *spinbutton8;
  GtkWidget *tab_network;
  GtkWidget *dialog_action_area5;
  GtkWidget *button1;
  GtkWidget *button3;

  intf_open = gnome_dialog_new (_("Open Stream"), NULL);
  gtk_object_set_data (GTK_OBJECT (intf_open), "intf_open", intf_open);
  gtk_window_set_modal (GTK_WINDOW (intf_open), TRUE);
  gtk_window_set_policy (GTK_WINDOW (intf_open), FALSE, FALSE, FALSE);

  dialog_vbox5 = GNOME_DIALOG (intf_open)->vbox;
  gtk_object_set_data (GTK_OBJECT (intf_open), "dialog_vbox5", dialog_vbox5);
  gtk_widget_show (dialog_vbox5);

  tab_open = gtk_notebook_new ();
  gtk_widget_ref (tab_open);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "tab_open", tab_open,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (tab_open);
  gtk_box_pack_start (GTK_BOX (dialog_vbox5), tab_open, TRUE, TRUE, 0);

  vbox10 = gtk_vbox_new (FALSE, 5);
  gtk_widget_ref (vbox10);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "vbox10", vbox10,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox10);
  gtk_container_add (GTK_CONTAINER (tab_open), vbox10);
  gtk_container_set_border_width (GTK_CONTAINER (vbox10), 5);

  hbox5 = gtk_hbox_new (FALSE, 5);
  gtk_widget_ref (hbox5);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "hbox5", hbox5,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox5);
  gtk_box_pack_start (GTK_BOX (vbox10), hbox5, TRUE, TRUE, 0);

  frame6 = gtk_frame_new (_("Disc type"));
  gtk_widget_ref (frame6);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "frame6", frame6,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame6);
  gtk_box_pack_start (GTK_BOX (hbox5), frame6, TRUE, TRUE, 0);

  vbox11 = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (vbox11);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "vbox11", vbox11,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox11);
  gtk_container_add (GTK_CONTAINER (frame6), vbox11);
  gtk_container_set_border_width (GTK_CONTAINER (vbox11), 5);

  radiobutton1 = gtk_radio_button_new_with_label (vbox11_group, _("DVD"));
  vbox11_group = gtk_radio_button_group (GTK_RADIO_BUTTON (radiobutton1));
  gtk_widget_ref (radiobutton1);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "radiobutton1", radiobutton1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (radiobutton1);
  gtk_box_pack_start (GTK_BOX (vbox11), radiobutton1, FALSE, FALSE, 0);

  radiobutton2 = gtk_radio_button_new_with_label (vbox11_group, _("VCD"));
  vbox11_group = gtk_radio_button_group (GTK_RADIO_BUTTON (radiobutton2));
  gtk_widget_ref (radiobutton2);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "radiobutton2", radiobutton2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (radiobutton2);
  gtk_box_pack_start (GTK_BOX (vbox11), radiobutton2, FALSE, FALSE, 0);

  frame7 = gtk_frame_new (_("Starting position"));
  gtk_widget_ref (frame7);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "frame7", frame7,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame7);
  gtk_box_pack_start (GTK_BOX (hbox5), frame7, TRUE, TRUE, 0);

  table3 = gtk_table_new (2, 2, FALSE);
  gtk_widget_ref (table3);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "table3", table3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table3);
  gtk_container_add (GTK_CONTAINER (frame7), table3);
  gtk_container_set_border_width (GTK_CONTAINER (table3), 5);
  gtk_table_set_row_spacings (GTK_TABLE (table3), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table3), 5);

  label24 = gtk_label_new (_("Title"));
  gtk_widget_ref (label24);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "label24", label24,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label24);
  gtk_table_attach (GTK_TABLE (table3), label24, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label24), 0, 0.5);

  label25 = gtk_label_new (_("Chapter"));
  gtk_widget_ref (label25);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "label25", label25,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label25);
  gtk_table_attach (GTK_TABLE (table3), label25, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label25), 0, 0.5);

  spinbutton5_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton5 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton5_adj), 1, 0);
  gtk_widget_ref (spinbutton5);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "spinbutton5", spinbutton5,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (spinbutton5);
  gtk_table_attach (GTK_TABLE (table3), spinbutton5, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  spinbutton6_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton6 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton6_adj), 1, 0);
  gtk_widget_ref (spinbutton6);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "spinbutton6", spinbutton6,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (spinbutton6);
  gtk_table_attach (GTK_TABLE (table3), spinbutton6, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  hbox6 = gtk_hbox_new (FALSE, 5);
  gtk_widget_ref (hbox6);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "hbox6", hbox6,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox6);
  gtk_box_pack_start (GTK_BOX (vbox10), hbox6, TRUE, TRUE, 0);

  label29 = gtk_label_new (_("Device name:"));
  gtk_widget_ref (label29);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "label29", label29,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label29);
  gtk_box_pack_start (GTK_BOX (hbox6), label29, FALSE, FALSE, 0);

  entry1 = gtk_entry_new ();
  gtk_widget_ref (entry1);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "entry1", entry1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (entry1);
  gtk_box_pack_start (GTK_BOX (hbox6), entry1, TRUE, TRUE, 0);
  gtk_entry_set_text (GTK_ENTRY (entry1), config_GetPszVariable( "dvd_device" ));

  tab_disc = gtk_label_new (_("Disc"));
  gtk_widget_ref (tab_disc);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "tab_disc", tab_disc,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (tab_disc);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (tab_open), gtk_notebook_get_nth_page (GTK_NOTEBOOK (tab_open), 0), tab_disc);

  vbox12 = gtk_vbox_new (FALSE, 5);
  gtk_widget_ref (vbox12);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "vbox12", vbox12,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox12);
  gtk_container_add (GTK_CONTAINER (tab_open), vbox12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox12), 5);

  hbox7 = gtk_hbox_new (FALSE, 5);
  gtk_widget_ref (hbox7);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "hbox7", hbox7,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox7);
  gtk_box_pack_start (GTK_BOX (vbox12), hbox7, TRUE, TRUE, 0);

  frame8 = gtk_frame_new (_("Protocol"));
  gtk_widget_ref (frame8);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "frame8", frame8,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame8);
  gtk_box_pack_start (GTK_BOX (hbox7), frame8, TRUE, TRUE, 0);

  vbox13 = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (vbox13);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "vbox13", vbox13,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox13);
  gtk_container_add (GTK_CONTAINER (frame8), vbox13);
  gtk_container_set_border_width (GTK_CONTAINER (vbox13), 5);

  radiobutton3 = gtk_radio_button_new_with_label (vbox13_group, _("UDP stream"));
  vbox13_group = gtk_radio_button_group (GTK_RADIO_BUTTON (radiobutton3));
  gtk_widget_ref (radiobutton3);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "radiobutton3", radiobutton3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (radiobutton3);
  gtk_box_pack_start (GTK_BOX (vbox13), radiobutton3, FALSE, FALSE, 0);

  radiobutton4 = gtk_radio_button_new_with_label (vbox13_group, _("HTTP"));
  vbox13_group = gtk_radio_button_group (GTK_RADIO_BUTTON (radiobutton4));
  gtk_widget_ref (radiobutton4);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "radiobutton4", radiobutton4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (radiobutton4);
  gtk_box_pack_start (GTK_BOX (vbox13), radiobutton4, FALSE, FALSE, 0);

  radiobutton5 = gtk_radio_button_new_with_label (vbox13_group, _("RTP"));
  vbox13_group = gtk_radio_button_group (GTK_RADIO_BUTTON (radiobutton5));
  gtk_widget_ref (radiobutton5);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "radiobutton5", radiobutton5,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (radiobutton5);
  gtk_box_pack_start (GTK_BOX (vbox13), radiobutton5, FALSE, FALSE, 0);

  frame9 = gtk_frame_new (_("Server"));
  gtk_widget_ref (frame9);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "frame9", frame9,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame9);
  gtk_box_pack_start (GTK_BOX (hbox7), frame9, TRUE, TRUE, 0);

  vbox14 = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (vbox14);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "vbox14", vbox14,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox14);
  gtk_container_add (GTK_CONTAINER (frame9), vbox14);

  table4 = gtk_table_new (2, 2, FALSE);
  gtk_widget_ref (table4);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "table4", table4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table4);
  gtk_box_pack_start (GTK_BOX (vbox14), table4, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table4), 5);
  gtk_table_set_row_spacings (GTK_TABLE (table4), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table4), 5);

  label26 = gtk_label_new (_("Address"));
  gtk_widget_ref (label26);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "label26", label26,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label26);
  gtk_table_attach (GTK_TABLE (table4), label26, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label26), 0, 0.5);

  label27 = gtk_label_new (_("Port"));
  gtk_widget_ref (label27);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "label27", label27,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label27);
  gtk_table_attach (GTK_TABLE (table4), label27, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label27), 0, 0.5);

  combo1 = gtk_combo_new ();
  gtk_widget_ref (combo1);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "combo1", combo1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo1);
  gtk_table_attach (GTK_TABLE (table4), combo1, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  combo_entry1 = GTK_COMBO (combo1)->entry;
  gtk_widget_ref (combo_entry1);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "combo_entry1", combo_entry1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo_entry1);

  spinbutton7_adj = gtk_adjustment_new (0, 0, 100, 1, 10, 10);
  spinbutton7 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton7_adj), 1, 0);
  gtk_widget_ref (spinbutton7);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "spinbutton7", spinbutton7,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (spinbutton7);
  gtk_table_attach (GTK_TABLE (table4), spinbutton7, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  frame10 = gtk_frame_new (_("Broadcast"));
  gtk_widget_ref (frame10);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "frame10", frame10,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame10);
  gtk_box_pack_start (GTK_BOX (vbox12), frame10, TRUE, TRUE, 0);

  hbox8 = gtk_hbox_new (FALSE, 5);
  gtk_widget_ref (hbox8);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "hbox8", hbox8,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox8);
  gtk_container_add (GTK_CONTAINER (frame10), hbox8);
  gtk_container_set_border_width (GTK_CONTAINER (hbox8), 5);

  checkbutton2 = gtk_check_button_new_with_label (_("Broadcast"));
  gtk_widget_ref (checkbutton2);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "checkbutton2", checkbutton2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (checkbutton2);
  gtk_box_pack_start (GTK_BOX (hbox8), checkbutton2, FALSE, FALSE, 0);

  combo3 = gtk_combo_new ();
  gtk_widget_ref (combo3);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "combo3", combo3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo3);
  gtk_box_pack_start (GTK_BOX (hbox8), combo3, TRUE, TRUE, 0);

  combo_entry3 = GTK_COMBO (combo3)->entry;
  gtk_widget_ref (combo_entry3);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "combo_entry3", combo_entry3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo_entry3);

  frame11 = gtk_frame_new (_("Channels"));
  gtk_widget_ref (frame11);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "frame11", frame11,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame11);
  gtk_box_pack_start (GTK_BOX (vbox12), frame11, TRUE, TRUE, 0);

  hbox9 = gtk_hbox_new (FALSE, 5);
  gtk_widget_ref (hbox9);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "hbox9", hbox9,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox9);
  gtk_container_add (GTK_CONTAINER (frame11), hbox9);
  gtk_container_set_border_width (GTK_CONTAINER (hbox9), 5);

  checkbutton1 = gtk_check_button_new_with_label (_("Channel server"));
  gtk_widget_ref (checkbutton1);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "checkbutton1", checkbutton1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (checkbutton1);
  gtk_box_pack_start (GTK_BOX (hbox9), checkbutton1, FALSE, FALSE, 0);

  combo2 = gtk_combo_new ();
  gtk_widget_ref (combo2);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "combo2", combo2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo2);
  gtk_box_pack_start (GTK_BOX (hbox9), combo2, TRUE, TRUE, 0);

  combo_entry2 = GTK_COMBO (combo2)->entry;
  gtk_widget_ref (combo_entry2);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "combo_entry2", combo_entry2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo_entry2);

  label28 = gtk_label_new (_("Port"));
  gtk_widget_ref (label28);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "label28", label28,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label28);
  gtk_box_pack_start (GTK_BOX (hbox9), label28, FALSE, FALSE, 0);

  spinbutton8_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  spinbutton8 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton8_adj), 1, 0);
  gtk_widget_ref (spinbutton8);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "spinbutton8", spinbutton8,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (spinbutton8);
  gtk_box_pack_start (GTK_BOX (hbox9), spinbutton8, TRUE, TRUE, 0);

  tab_network = gtk_label_new (_("Network"));
  gtk_widget_ref (tab_network);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "tab_network", tab_network,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (tab_network);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (tab_open), gtk_notebook_get_nth_page (GTK_NOTEBOOK (tab_open), 1), tab_network);

  dialog_action_area5 = GNOME_DIALOG (intf_open)->action_area;
  gtk_object_set_data (GTK_OBJECT (intf_open), "dialog_action_area5", dialog_action_area5);
  gtk_widget_show (dialog_action_area5);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area5), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (dialog_action_area5), 8);

  gnome_dialog_append_button (GNOME_DIALOG (intf_open), GNOME_STOCK_BUTTON_OK);
  button1 = GTK_WIDGET (g_list_last (GNOME_DIALOG (intf_open)->buttons)->data);
  gtk_widget_ref (button1);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "button1", button1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (button1);
  GTK_WIDGET_SET_FLAGS (button1, GTK_CAN_DEFAULT);

  gnome_dialog_append_button (GNOME_DIALOG (intf_open), GNOME_STOCK_BUTTON_CANCEL);
  button3 = GTK_WIDGET (g_list_last (GNOME_DIALOG (intf_open)->buttons)->data);
  gtk_widget_ref (button3);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "button3", button3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (button3);
  GTK_WIDGET_SET_FLAGS (button3, GTK_CAN_DEFAULT);

  return intf_open;
}

GtkWidget*
create_intf_messages (void)
{
  GtkWidget *intf_messages;
  GtkWidget *dialog_vbox6;
  GtkWidget *scrolledwindow1;
  GtkWidget *messages_textbox;
  GtkWidget *dialog_action_area6;
  GtkWidget *messages_ok;

  intf_messages = gnome_dialog_new (_("Messages"), NULL);
  gtk_object_set_data (GTK_OBJECT (intf_messages), "intf_messages", intf_messages);
  gtk_window_set_policy (GTK_WINDOW (intf_messages), TRUE, TRUE, FALSE);
  gnome_dialog_close_hides (GNOME_DIALOG (intf_messages), TRUE);

  dialog_vbox6 = GNOME_DIALOG (intf_messages)->vbox;
  gtk_object_set_data (GTK_OBJECT (intf_messages), "dialog_vbox6", dialog_vbox6);
  gtk_widget_show (dialog_vbox6);

  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow1);
  gtk_object_set_data_full (GTK_OBJECT (intf_messages), "scrolledwindow1", scrolledwindow1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox6), scrolledwindow1, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow1), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

  messages_textbox = gtk_text_new (NULL, NULL);
  gtk_widget_ref (messages_textbox);
  gtk_object_set_data_full (GTK_OBJECT (intf_messages), "messages_textbox", messages_textbox,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (messages_textbox);
  gtk_container_add (GTK_CONTAINER (scrolledwindow1), messages_textbox);
  gtk_widget_set_usize (messages_textbox, 600, 400);

  dialog_action_area6 = GNOME_DIALOG (intf_messages)->action_area;
  gtk_object_set_data (GTK_OBJECT (intf_messages), "dialog_action_area6", dialog_action_area6);
  gtk_widget_show (dialog_action_area6);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area6), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (dialog_action_area6), 8);

  gnome_dialog_append_button (GNOME_DIALOG (intf_messages), GNOME_STOCK_BUTTON_OK);
  messages_ok = GTK_WIDGET (g_list_last (GNOME_DIALOG (intf_messages)->buttons)->data);
  gtk_widget_ref (messages_ok);
  gtk_object_set_data_full (GTK_OBJECT (intf_messages), "messages_ok", messages_ok,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (messages_ok);
  GTK_WIDGET_SET_FLAGS (messages_ok, GTK_CAN_DEFAULT);

  gtk_signal_connect (GTK_OBJECT (intf_messages), "destroy",
                      GTK_SIGNAL_FUNC (gtk_widget_hide),
                      "intf_playlist");
  gtk_signal_connect (GTK_OBJECT (intf_messages), "delete_event",
                      GTK_SIGNAL_FUNC (gtk_widget_hide),
                      "intf_playlist");
  gtk_signal_connect (GTK_OBJECT (messages_ok), "clicked",
                      GTK_SIGNAL_FUNC (GtkMessagesOk),
                      "intf_messages");

  return intf_messages;
}

GtkWidget*
create_intf_sat (void)
{
  GtkWidget *intf_sat;
  GtkWidget *vbox15;
  GtkWidget *hbox10;
  GtkWidget *frame13;
  GtkWidget *table5;
  GtkWidget *label30;
  GtkObject *sat_freq_adj;
  GtkWidget *sat_freq;
  GtkWidget *label31;
  GSList *table5_group = NULL;
  GtkWidget *sat_pol_hor;
  GtkWidget *sat_pol_vert;
  GtkWidget *label33;
  GtkObject *sat_srate_adj;
  GtkWidget *sat_srate;
  GtkWidget *hbuttonbox2;
  GtkWidget *sat_ok;
  GtkWidget *sat_cancel;

  intf_sat = gnome_dialog_new (_("Open Satellite Card"), NULL);
  gtk_object_set_data (GTK_OBJECT (intf_sat), "intf_sat", intf_sat);
  gtk_window_set_modal (GTK_WINDOW (intf_sat), TRUE);
  gtk_window_set_policy (GTK_WINDOW (intf_sat), FALSE, FALSE, FALSE);

  vbox15 = GNOME_DIALOG (intf_sat)->vbox;
  gtk_object_set_data (GTK_OBJECT (intf_sat), "vbox15", vbox15);
  gtk_widget_show (vbox15);

  hbox10 = gtk_hbox_new (FALSE, 5);
  gtk_widget_ref (hbox10);
  gtk_object_set_data_full (GTK_OBJECT (intf_sat), "hbox10", hbox10,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox10);
  gtk_box_pack_start (GTK_BOX (vbox15), hbox10, TRUE, TRUE, 0);

  frame13 = gtk_frame_new (_("Transponder Settings"));
  gtk_widget_ref (frame13);
  gtk_object_set_data_full (GTK_OBJECT (intf_sat), "frame13", frame13,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame13);
  gtk_box_pack_start (GTK_BOX (hbox10), frame13, TRUE, TRUE, 0);

  table5 = gtk_table_new (4, 2, FALSE);
  gtk_widget_ref (table5);
  gtk_object_set_data_full (GTK_OBJECT (intf_sat), "table5", table5,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table5);
  gtk_container_add (GTK_CONTAINER (frame13), table5);
  gtk_container_set_border_width (GTK_CONTAINER (table5), 5);
  gtk_table_set_row_spacings (GTK_TABLE (table5), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table5), 5);

  label30 = gtk_label_new (_("Frequency"));
  gtk_widget_ref (label30);
  gtk_object_set_data_full (GTK_OBJECT (intf_sat), "label30", label30,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label30);
  gtk_table_attach (GTK_TABLE (table5), label30, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label30), 0, 0.5);

  sat_freq_adj = gtk_adjustment_new (12553, 1, 65536, 1, 10, 10);
  sat_freq = gtk_spin_button_new (GTK_ADJUSTMENT (sat_freq_adj), 1, 0);
  gtk_widget_ref (sat_freq);
  gtk_object_set_data_full (GTK_OBJECT (intf_sat), "sat_freq", sat_freq,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sat_freq);
  gtk_table_attach (GTK_TABLE (table5), sat_freq, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  label31 = gtk_label_new (_("Polarization"));
  gtk_widget_ref (label31);
  gtk_object_set_data_full (GTK_OBJECT (intf_sat), "label31", label31,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label31);
  gtk_table_attach (GTK_TABLE (table5), label31, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label31), 0, 0.5);

  sat_pol_hor = gtk_radio_button_new_with_label (table5_group, _("Horizontal"));
  table5_group = gtk_radio_button_group (GTK_RADIO_BUTTON (sat_pol_hor));
  gtk_widget_ref (sat_pol_hor);
  gtk_object_set_data_full (GTK_OBJECT (intf_sat), "sat_pol_hor", sat_pol_hor,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sat_pol_hor);
  gtk_table_attach (GTK_TABLE (table5), sat_pol_hor, 1, 2, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  sat_pol_vert = gtk_radio_button_new_with_label (table5_group, _("Vertical"));
  table5_group = gtk_radio_button_group (GTK_RADIO_BUTTON (sat_pol_vert));
  gtk_widget_ref (sat_pol_vert);
  gtk_object_set_data_full (GTK_OBJECT (intf_sat), "sat_pol_vert", sat_pol_vert,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sat_pol_vert);
  gtk_table_attach (GTK_TABLE (table5), sat_pol_vert, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sat_pol_vert), TRUE);

  label33 = gtk_label_new (_("Symbol Rate"));
  gtk_widget_ref (label33);
  gtk_object_set_data_full (GTK_OBJECT (intf_sat), "label33", label33,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label33);
  gtk_table_attach (GTK_TABLE (table5), label33, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label33), 0, 0.5);

  sat_srate_adj = gtk_adjustment_new (27500, 0, 100, 1, 10, 10);
  sat_srate = gtk_spin_button_new (GTK_ADJUSTMENT (sat_srate_adj), 1, 0);
  gtk_widget_ref (sat_srate);
  gtk_object_set_data_full (GTK_OBJECT (intf_sat), "sat_srate", sat_srate,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sat_srate);
  gtk_table_attach (GTK_TABLE (table5), sat_srate, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  hbuttonbox2 = GNOME_DIALOG (intf_sat)->action_area;
  gtk_object_set_data (GTK_OBJECT (intf_sat), "hbuttonbox2", hbuttonbox2);
  gtk_widget_show (hbuttonbox2);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox2), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbuttonbox2), 8);

  gnome_dialog_append_button (GNOME_DIALOG (intf_sat), GNOME_STOCK_BUTTON_OK);
  sat_ok = GTK_WIDGET (g_list_last (GNOME_DIALOG (intf_sat)->buttons)->data);
  gtk_widget_ref (sat_ok);
  gtk_object_set_data_full (GTK_OBJECT (intf_sat), "sat_ok", sat_ok,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sat_ok);
  GTK_WIDGET_SET_FLAGS (sat_ok, GTK_CAN_DEFAULT);

  gnome_dialog_append_button (GNOME_DIALOG (intf_sat), GNOME_STOCK_BUTTON_CANCEL);
  sat_cancel = GTK_WIDGET (g_list_last (GNOME_DIALOG (intf_sat)->buttons)->data);
  gtk_widget_ref (sat_cancel);
  gtk_object_set_data_full (GTK_OBJECT (intf_sat), "sat_cancel", sat_cancel,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sat_cancel);
  GTK_WIDGET_SET_FLAGS (sat_cancel, GTK_CAN_DEFAULT);

  gtk_signal_connect (GTK_OBJECT (sat_ok), "clicked",
                      GTK_SIGNAL_FUNC (GtkSatOpenOk),
                      "intf_disc");
  gtk_signal_connect (GTK_OBJECT (sat_cancel), "clicked",
                      GTK_SIGNAL_FUNC (GtkSatOpenCancel),
                      "intf_disc");

  return intf_sat;
}

