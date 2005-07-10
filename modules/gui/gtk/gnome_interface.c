/* This file was created automatically by glade and fixed by bootstrap */

#include <vlc/vlc.h>

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
    N_("Open a file"),
    (gpointer) GtkFileOpenShow, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Open _Disc..."),
    N_("Open Disc Media"),
    (gpointer) GtkDiscOpenShow, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CDROM,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Network stream..."),
    N_("Select a network stream"),
    (gpointer) GtkNetworkOpenShow, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_REFRESH,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("_Eject Disc"),
    N_("Eject disc"),
    (gpointer) GtkDiscEject, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_TOP,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_CLOSE_ITEM (GtkClose, NULL),
  GNOMEUIINFO_MENU_EXIT_ITEM (GnomeExit, NULL),
  GNOMEUIINFO_END
};

static GnomeUIInfo menubar_view_menu_uiinfo[] =
{
  {
    GNOME_APP_UI_ITEM, N_("_Hide interface"),
    NULL,
    (gpointer) GtkWindowToggle, NULL, NULL,
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
    (gpointer) GtkPlaylistShow, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_INDEX,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Modules..."),
    N_("Open the module manager"),
    (gpointer) GtkModulesShow, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_ATTACH,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Messages..."),
    N_("Open the messages window"),
    (gpointer) GtkMessagesShow, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo menubar_settings_menu_uiinfo[] =
{
  GNOMEUIINFO_MENU_PREFERENCES_ITEM (GtkPreferencesShow, NULL),
  GNOMEUIINFO_END
};

static GnomeUIInfo menubar_config_audio_menu_uiinfo[] =
{
  {
    GNOME_APP_UI_ITEM, N_("_Language"),
    N_("Select audio channel"),
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("Volume Up"),
    NULL,
    (gpointer) GtkVolumeUp, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Volume Down"),
    NULL,
    (gpointer) GtkVolumeDown, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Mute"),
    NULL,
    (gpointer) GtkVolumeMute, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("Channels"),
    NULL,
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Device"),
    NULL,
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo menubar_config_video_menu_menu_uiinfo[] =
{
  {
    GNOME_APP_UI_ITEM, N_("_Subtitles"),
    N_("Select subtitles channel"),
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("_Fullscreen"),
    NULL,
    (gpointer) GtkFullscreen, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("Deinterlace"),
    NULL,
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Screen"),
    NULL,
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo menubar_help_menu_uiinfo[] =
{
  GNOMEUIINFO_MENU_ABOUT_ITEM (GtkAboutShow, NULL),
  GNOMEUIINFO_END
};

static GnomeUIInfo menubar_uiinfo[] =
{
  GNOMEUIINFO_MENU_FILE_TREE (menubar_file_menu_uiinfo),
  GNOMEUIINFO_MENU_VIEW_TREE (menubar_view_menu_uiinfo),
  GNOMEUIINFO_MENU_SETTINGS_TREE (menubar_settings_menu_uiinfo),
  {
    GNOME_APP_UI_SUBTREE, N_("_Audio"),
    NULL,
    menubar_config_audio_menu_uiinfo, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_SUBTREE, N_("_Video"),
    NULL,
    menubar_config_video_menu_menu_uiinfo, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
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
  GtkWidget *appbar;
  GtkTooltips *tooltips;

  tooltips = gtk_tooltips_new ();

  intf_window = gnome_app_new ("VLC media player", _("VLC media player"));
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
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_close",
                            menubar_file_menu_uiinfo[6].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_file_menu_uiinfo[7].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_exit",
                            menubar_file_menu_uiinfo[7].widget,
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
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator9",
                            menubar_view_menu_uiinfo[1].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_view_menu_uiinfo[2].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_program",
                            menubar_view_menu_uiinfo[2].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (menubar_view_menu_uiinfo[2].widget, FALSE);

  gtk_widget_ref (menubar_view_menu_uiinfo[3].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_title",
                            menubar_view_menu_uiinfo[3].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (menubar_view_menu_uiinfo[3].widget, FALSE);

  gtk_widget_ref (menubar_view_menu_uiinfo[4].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_chapter",
                            menubar_view_menu_uiinfo[4].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (menubar_view_menu_uiinfo[4].widget, FALSE);

  gtk_widget_ref (menubar_view_menu_uiinfo[5].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator7",
                            menubar_view_menu_uiinfo[5].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_view_menu_uiinfo[6].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_playlist",
                            menubar_view_menu_uiinfo[6].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_view_menu_uiinfo[7].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_modules",
                            menubar_view_menu_uiinfo[7].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (menubar_view_menu_uiinfo[7].widget, FALSE);

  gtk_widget_ref (menubar_view_menu_uiinfo[8].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_messages",
                            menubar_view_menu_uiinfo[8].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_uiinfo[2].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_settings",
                            menubar_uiinfo[2].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_settings_menu_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_preferences",
                            menubar_settings_menu_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_uiinfo[3].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_config_audio",
                            menubar_uiinfo[3].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_config_audio_menu_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_audio",
                            menubar_config_audio_menu_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (menubar_config_audio_menu_uiinfo[0].widget, FALSE);

  gtk_widget_ref (menubar_config_audio_menu_uiinfo[1].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator25",
                            menubar_config_audio_menu_uiinfo[1].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_config_audio_menu_uiinfo[2].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_volume_up",
                            menubar_config_audio_menu_uiinfo[2].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_config_audio_menu_uiinfo[3].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_volume_down",
                            menubar_config_audio_menu_uiinfo[3].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_config_audio_menu_uiinfo[4].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_volume_mute",
                            menubar_config_audio_menu_uiinfo[4].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_config_audio_menu_uiinfo[5].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator16",
                            menubar_config_audio_menu_uiinfo[5].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_config_audio_menu_uiinfo[6].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_audio_channels",
                            menubar_config_audio_menu_uiinfo[6].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_config_audio_menu_uiinfo[7].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_audio_device",
                            menubar_config_audio_menu_uiinfo[7].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_uiinfo[4].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_config_video_menu",
                            menubar_uiinfo[4].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_config_video_menu_menu_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_subpictures",
                            menubar_config_video_menu_menu_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (menubar_config_video_menu_menu_uiinfo[0].widget, FALSE);

  gtk_widget_ref (menubar_config_video_menu_menu_uiinfo[1].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator23",
                            menubar_config_video_menu_menu_uiinfo[1].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_config_video_menu_menu_uiinfo[2].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_fullscreen",
                            menubar_config_video_menu_menu_uiinfo[2].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_config_video_menu_menu_uiinfo[3].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator24",
                            menubar_config_video_menu_menu_uiinfo[3].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_config_video_menu_menu_uiinfo[4].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_deinterlace",
                            menubar_config_video_menu_menu_uiinfo[4].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_config_video_menu_menu_uiinfo[5].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_video_device",
                            menubar_config_video_menu_menu_uiinfo[5].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_uiinfo[5].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_help",
                            menubar_uiinfo[5].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (menubar_help_menu_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_about",
                            menubar_help_menu_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  toolbar = gtk_toolbar_new ();
  gtk_widget_ref (toolbar);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar", toolbar,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar);
  gnome_app_add_toolbar (GNOME_APP (intf_window), GTK_TOOLBAR (toolbar), "toolbar",
                                BONOBO_DOCK_ITEM_BEH_EXCLUSIVE,
                                BONOBO_DOCK_TOP, 1, 0, 2);
  //gtk_toolbar_set_space_size (GTK_TOOLBAR (toolbar), 16);
  //gtk_toolbar_set_space_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_SPACE_LINE);
  //gtk_toolbar_set_button_relief (GTK_TOOLBAR (toolbar), GTK_RELIEF_NONE);

  //tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_OPEN);
  toolbar_file = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("File"),
                                _("Open a file"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_file);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_file", toolbar_file,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_file);

  //tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_CDROM);
  toolbar_disc = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Disc"),
                                _("Open disc"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_disc);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_disc", toolbar_disc,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_disc);

  //tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_REFRESH);
  toolbar_network = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Net"),
                                _("Select a network stream"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_network);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_network", toolbar_network,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_network);

  //tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_MIC);
  toolbar_sat = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Sat"),
                                _("Open a satellite card"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_sat);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_sat", toolbar_sat,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_sat);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  //tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_BACK);
  toolbar_back = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Back"),
                                _("Go backward"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_back);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_back", toolbar_back,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_back);
  gtk_widget_set_sensitive (toolbar_back, FALSE);

  //tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_STOP);
  toolbar_stop = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Stop"),
                                _("Stop stream"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_stop);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_stop", toolbar_stop,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_stop);

  //tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_TOP);
  toolbar_eject = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Eject"),
                                _("Eject disc"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_eject);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_eject", toolbar_eject,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_eject);

  //tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_FORWARD);
  toolbar_play = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Play"),
                                _("Play stream"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_play);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_play", toolbar_play,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_play);

  //tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_BOTTOM);
  toolbar_pause = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Pause"),
                                _("Pause stream"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_pause);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_pause", toolbar_pause,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_pause);
  gtk_widget_set_sensitive (toolbar_pause, FALSE);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  //tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_TIMER_STOP);
  toolbar_slow = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Slow"),
                                _("Play slower"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_slow);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_slow", toolbar_slow,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_slow);
  gtk_widget_set_sensitive (toolbar_slow, FALSE);

  //tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_TIMER);
  toolbar_fast = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Fast"),
                                _("Play faster"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_fast);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_fast", toolbar_fast,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_fast);
  gtk_widget_set_sensitive (toolbar_fast, FALSE);

  //tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_INDEX);
  toolbar_playlist = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Playlist"),
                                _("Open playlist"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_playlist);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_playlist", toolbar_playlist,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_playlist);

  //tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_FIRST);
  toolbar_prev = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Prev"),
                                _("Previous file"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_prev);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_prev", toolbar_prev,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_prev);

  //tmp_toolbar_icon = gnome_stock_pixmap_widget (intf_window, GNOME_STOCK_PIXMAP_LAST);
  toolbar_next = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Next"),
                                _("Next file"), NULL,
                                NULL, NULL, NULL);
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

  slider_frame = gtk_frame_new ("-:--:--");
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

  title_label = gtk_label_new ("--");
  gtk_widget_ref (title_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "title_label", title_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (title_label);
  gtk_box_pack_start (GTK_BOX (title_chapter_box), title_label, FALSE, FALSE, 0);

  button_title_prev = gtk_button_new_from_stock (GNOME_STOCK_BUTTON_PREV);
  gtk_widget_ref (button_title_prev);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "button_title_prev", button_title_prev,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (button_title_prev);
  gtk_box_pack_start (GTK_BOX (title_chapter_box), button_title_prev, FALSE, FALSE, 0);
  gtk_tooltips_set_tip (tooltips, button_title_prev, _("Select previous title"), NULL);

  button_title_next = gtk_button_new_from_stock (GNOME_STOCK_BUTTON_NEXT);
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

  chapter_label = gtk_label_new ("---");
  gtk_widget_ref (chapter_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "chapter_label", chapter_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chapter_label);
  gtk_box_pack_start (GTK_BOX (dvd_chapter_box), chapter_label, FALSE, FALSE, 0);

  button_chapter_prev = gtk_button_new_from_stock (GNOME_STOCK_BUTTON_DOWN);
  gtk_widget_ref (button_chapter_prev);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "button_chapter_prev", button_chapter_prev,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (button_chapter_prev);
  gtk_box_pack_start (GTK_BOX (dvd_chapter_box), button_chapter_prev, FALSE, FALSE, 0);
  gtk_tooltips_set_tip (tooltips, button_chapter_prev, _("Select previous chapter"), NULL);

  button_chapter_next = gtk_button_new_from_stock (GNOME_STOCK_BUTTON_UP);
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

  appbar = gnome_appbar_new (FALSE, TRUE, GNOME_PREFERENCES_NEVER);
  gtk_widget_ref (appbar);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "appbar", appbar,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (appbar);
  gnome_app_set_statusbar (GNOME_APP (intf_window), appbar);

  gtk_signal_connect (GTK_OBJECT (intf_window), "delete_event",
                      GTK_SIGNAL_FUNC (GtkWindowDelete),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (intf_window), "drag_data_received",
                      GTK_SIGNAL_FUNC (GtkWindowDrag),
                      NULL);
  gnome_app_install_menu_hints (GNOME_APP (intf_window), menubar_uiinfo);
  gtk_signal_connect (GTK_OBJECT (toolbar_file), "clicked",
                      GTK_SIGNAL_FUNC (GtkFileOpenShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_disc), "clicked",
                      GTK_SIGNAL_FUNC (GtkDiscOpenShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_network), "clicked",
                      GTK_SIGNAL_FUNC (GtkNetworkOpenShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_sat), "clicked",
                      GTK_SIGNAL_FUNC (GtkSatOpenShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_back), "clicked",
                      GTK_SIGNAL_FUNC (GtkControlBack),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_stop), "clicked",
                      GTK_SIGNAL_FUNC (GtkControlStop),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_eject), "clicked",
                      GTK_SIGNAL_FUNC (GtkDiscEject),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_play), "clicked",
                      GTK_SIGNAL_FUNC (GtkControlPlay),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_pause), "clicked",
                      GTK_SIGNAL_FUNC (GtkControlPause),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_slow), "clicked",
                      GTK_SIGNAL_FUNC (GtkControlSlow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_fast), "clicked",
                      GTK_SIGNAL_FUNC (GtkControlFast),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_playlist), "clicked",
                      GTK_SIGNAL_FUNC (GtkPlaylistShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_prev), "clicked",
                      GTK_SIGNAL_FUNC (GtkPlaylistPrev),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_next), "clicked",
                      GTK_SIGNAL_FUNC (GtkPlaylistNext),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (slider), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkSliderPress),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (slider), "button_release_event",
                      GTK_SIGNAL_FUNC (GtkSliderRelease),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (button_title_prev), "clicked",
                      GTK_SIGNAL_FUNC (GtkTitlePrev),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (button_title_next), "clicked",
                      GTK_SIGNAL_FUNC (GtkTitleNext),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (button_chapter_prev), "clicked",
                      GTK_SIGNAL_FUNC (GtkChapterPrev),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (button_chapter_next), "clicked",
                      GTK_SIGNAL_FUNC (GtkChapterNext),
                      NULL);

  gtk_object_set_data (GTK_OBJECT (intf_window), "tooltips", tooltips);

  return intf_window;
}

static GnomeUIInfo popup_audio_menu_uiinfo[] =
{
  {
    GNOME_APP_UI_ITEM, N_("_Language"),
    N_("Select audio channel"),
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_VOLUME,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("Volume up"),
    NULL,
    (gpointer) GtkVolumeUp, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Volume down"),
    NULL,
    (gpointer) GtkVolumeDown, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Mute"),
    NULL,
    (gpointer) GtkVolumeMute, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("Channels"),
    NULL,
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Device"),
    NULL,
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo popup_video_menu_uiinfo[] =
{
  {
    GNOME_APP_UI_ITEM, N_("_Subtitles"),
    N_("Select subtitles channel"),
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_FONT,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("_Fullscreen"),
    N_("Toggle fullscreen mode"),
    (gpointer) GtkFullscreen, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("Deinterlace"),
    NULL,
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Screen"),
    NULL,
    (gpointer) NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo popup_file_menu_uiinfo[] =
{
  {
    GNOME_APP_UI_ITEM, N_("_Open File..."),
    N_("Open a file"),
    (gpointer) GtkFileOpenShow, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_OPEN,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Open _Disc..."),
    N_("Open Disc Media"),
    (gpointer) GtkDiscOpenShow, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_CDROM,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Network Stream..."),
    N_("Select a network stream"),
    (gpointer) GtkNetworkOpenShow, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_REFRESH,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_ABOUT_ITEM (GtkAboutShow, NULL),
  GNOMEUIINFO_END
};

static GnomeUIInfo intf_popup_uiinfo[] =
{
  {
    GNOME_APP_UI_ITEM, N_("Play"),
    NULL,
    (gpointer) GtkControlPlay, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_FORWARD,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Pause"),
    NULL,
    (gpointer) GtkControlPause, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_BOTTOM,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Stop"),
    NULL,
    (gpointer) GtkControlStop, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_STOP,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Back"),
    NULL,
    (gpointer) GtkControlBack, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_BACK,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Slow"),
    NULL,
    (gpointer) GtkControlSlow, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_TIMER_STOP,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Fast"),
    NULL,
    (gpointer) GtkControlFast, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_TIMER,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("Next"),
    NULL,
    (gpointer) GtkPlaylistNext, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Prev"),
    NULL,
    (gpointer) GtkPlaylistPrev, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Jump..."),
    N_("Got directly so specified point"),
    (gpointer) GtkJumpShow, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_JUMP_TO,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
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
    GNOME_APP_UI_SUBTREE, N_("Audio"),
    NULL,
    popup_audio_menu_uiinfo, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_SUBTREE, N_("Video"),
    NULL,
    popup_video_menu_uiinfo, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_FILE_TREE (popup_file_menu_uiinfo),
  {
    GNOME_APP_UI_ITEM, N_("Toggle _Interface"),
    NULL,
    (gpointer) GtkWindowToggle, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Playlist..."),
    NULL,
    (gpointer) GtkPlaylistShow, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_MENU_PREFERENCES_ITEM (GtkPreferencesShow, NULL),
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_EXIT_ITEM (GnomeExit, NULL),
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
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_next",
                            intf_popup_uiinfo[7].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[8].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_prev",
                            intf_popup_uiinfo[8].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[9].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_jump",
                            intf_popup_uiinfo[9].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[10].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator17",
                            intf_popup_uiinfo[10].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[11].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_program",
                            intf_popup_uiinfo[11].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (intf_popup_uiinfo[11].widget, FALSE);

  gtk_widget_ref (intf_popup_uiinfo[12].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_navigation",
                            intf_popup_uiinfo[12].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (intf_popup_uiinfo[12].widget, FALSE);

  gtk_widget_ref (intf_popup_uiinfo[13].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_audio",
                            intf_popup_uiinfo[13].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (popup_audio_menu_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_language",
                            popup_audio_menu_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (popup_audio_menu_uiinfo[0].widget, FALSE);

  gtk_widget_ref (popup_audio_menu_uiinfo[1].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator20",
                            popup_audio_menu_uiinfo[1].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (popup_audio_menu_uiinfo[2].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_volume_up",
                            popup_audio_menu_uiinfo[2].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (popup_audio_menu_uiinfo[3].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_volume_down",
                            popup_audio_menu_uiinfo[3].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (popup_audio_menu_uiinfo[4].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_volume_mute",
                            popup_audio_menu_uiinfo[4].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (popup_audio_menu_uiinfo[5].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator21",
                            popup_audio_menu_uiinfo[5].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (popup_audio_menu_uiinfo[6].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_audio_channels",
                            popup_audio_menu_uiinfo[6].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (popup_audio_menu_uiinfo[7].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_audio_device",
                            popup_audio_menu_uiinfo[7].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[14].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_video",
                            intf_popup_uiinfo[14].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (popup_video_menu_uiinfo[0].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_subpictures",
                            popup_video_menu_uiinfo[0].widget,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_set_sensitive (popup_video_menu_uiinfo[0].widget, FALSE);

  gtk_widget_ref (popup_video_menu_uiinfo[1].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator18",
                            popup_video_menu_uiinfo[1].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (popup_video_menu_uiinfo[2].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_fullscreen",
                            popup_video_menu_uiinfo[2].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (popup_video_menu_uiinfo[3].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator19",
                            popup_video_menu_uiinfo[3].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (popup_video_menu_uiinfo[4].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_deinterlace",
                            popup_video_menu_uiinfo[4].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (popup_video_menu_uiinfo[5].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_video_device",
                            popup_video_menu_uiinfo[5].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[15].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator13",
                            intf_popup_uiinfo[15].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[16].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_file",
                            intf_popup_uiinfo[16].widget,
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

  gtk_widget_ref (intf_popup_uiinfo[17].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_interface_toggle",
                            intf_popup_uiinfo[17].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[18].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_playlist",
                            intf_popup_uiinfo[18].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[19].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_preferences",
                            intf_popup_uiinfo[19].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[20].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator2",
                            intf_popup_uiinfo[20].widget,
                            (GtkDestroyNotify) gtk_widget_unref);

  gtk_widget_ref (intf_popup_uiinfo[21].widget);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_exit",
                            intf_popup_uiinfo[21].widget,
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

  intf_about = gnome_about_new ("VLC media player", VERSION,
                        _("(c) 1996-2004 the VideoLAN team"),
                        _("This is the VLC media player, a DVD, MPEG and DivX player. It can play MPEG and MPEG2 files from a file or from a network source."),
                        authors,
                        NULL,
                        NULL,
                        NULL);
  gtk_object_set_data (GTK_OBJECT (intf_about), "intf_about", intf_about);

  return intf_about;
}

GtkWidget*
create_intf_open (void)
{
  GtkWidget *intf_open;
  GtkWidget *dialog_vbox5;
  GtkWidget *open_vbox;
  GtkWidget *frame10;
  GtkWidget *hbox21;
  GtkWidget *hbox22;
  GtkWidget *label34;
  GtkWidget *combo2;
  GtkWidget *entry_open;
  GtkWidget *label36;
  GtkWidget *open_notebook;
  GtkWidget *hbox20;
  GtkWidget *combo1;
  GtkWidget *entry_file;
  GtkWidget *vbox13;
  GtkWidget *open_browse;
  GtkWidget *open_file;
  GtkWidget *table5;
  GtkWidget *label35;
  GtkWidget *hbox24;
  GSList *disc_group = NULL;
  GtkWidget *disc_dvd;
  GtkWidget *disc_vcd;
  GtkWidget *disc_cdda;
  GtkWidget *label19;
  GtkWidget *disc_name;
  GtkWidget *disc_chapter_label;
  GtkWidget *disc_title_label;
  GtkWidget *disc_dvd_use_menu;
  GtkObject *disc_title_adj;
  GtkWidget *disc_title;
  GtkObject *disc_chapter_adj;
  GtkWidget *disc_chapter;
  GtkWidget *open_disc;
  GtkWidget *table4;
  GSList *table4_group = NULL;
  GtkWidget *network_udp;
  GtkWidget *network_multicast;
  GtkWidget *network_http;
  GtkWidget *network_udp_port_label;
  GtkWidget *network_multicast_address_label;
  GtkWidget *network_http_url_label;
  GtkWidget *network_multicast_address_combo;
  GtkWidget *network_multicast_address;
  GtkWidget *network_multicast_port_label;
  GtkObject *network_multicast_port_adj;
  GtkWidget *network_multicast_port;
  GtkWidget *network_http_url;
  GtkObject *network_udp_port_adj;
  GtkWidget *network_udp_port;
  GtkWidget *open_net;
  GtkWidget *table3;
  GtkWidget *label24;
  GtkWidget *label25;
  GtkWidget *label26;
  GtkObject *sat_freq_adj;
  GtkWidget *sat_freq;
  GtkWidget *label27;
  GtkWidget *hbox23;
  GSList *pol_group = NULL;
  GtkWidget *sat_pol_vert;
  GtkWidget *sat_pol_hor;
  GtkObject *sat_srate_adj;
  GtkWidget *sat_srate;
  GtkWidget *sat_fec;
  GList *sat_fec_items = NULL;
  GtkWidget *combo_entry1;
  GtkWidget *open_sat;
  GtkWidget *show_subtitle;
  GtkWidget *hbox_subtitle;
  GtkWidget *combo3;
  GtkWidget *entry_subtitle;
  GtkWidget *vbox14;
  GtkWidget *button4;
  GtkWidget *label37;
  GtkObject *subtitle_delay_adj;
  GtkWidget *subtitle_delay;
  GtkWidget *label38;
  GtkObject *subtitle_fps_adj;
  GtkWidget *subtitle_fps;
  GtkWidget *hbox28;
  GtkWidget *show_sout_settings;
  GtkWidget *sout_settings;
  GtkWidget *dialog_action_area5;
  GtkWidget *button1;
  GtkWidget *button3;

  intf_open = gnome_dialog_new (_("Open Stream"), NULL);
  gtk_object_set_data (GTK_OBJECT (intf_open), "intf_open", intf_open);
  gtk_window_set_modal (GTK_WINDOW (intf_open), TRUE);
  gtk_window_set_policy (GTK_WINDOW (intf_open), FALSE, FALSE, FALSE);
  gnome_dialog_close_hides (GNOME_DIALOG (intf_open), TRUE);

  dialog_vbox5 = GNOME_DIALOG (intf_open)->vbox;
  gtk_object_set_data (GTK_OBJECT (intf_open), "dialog_vbox5", dialog_vbox5);
  gtk_widget_show (dialog_vbox5);

  open_vbox = gtk_vbox_new (FALSE, 5);
  gtk_widget_ref (open_vbox);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "open_vbox", open_vbox,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (open_vbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox5), open_vbox, TRUE, TRUE, 0);

  frame10 = gtk_frame_new ("Media Resource Locator (MRL)");
  gtk_widget_ref (frame10);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "frame10", frame10,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame10);
  gtk_box_pack_start (GTK_BOX (open_vbox), frame10, FALSE, TRUE, 0);

  hbox21 = gtk_hbox_new (FALSE, 5);
  gtk_widget_ref (hbox21);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "hbox21", hbox21,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox21);
  gtk_container_add (GTK_CONTAINER (frame10), hbox21);
  gtk_container_set_border_width (GTK_CONTAINER (hbox21), 5);

  hbox22 = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (hbox22);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "hbox22", hbox22,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox22);
  gtk_box_pack_start (GTK_BOX (hbox21), hbox22, FALSE, TRUE, 0);

  label34 = gtk_label_new (_("Open Target:"));
  gtk_widget_ref (label34);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "label34", label34,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label34);
  gtk_box_pack_start (GTK_BOX (hbox22), label34, FALSE, FALSE, 0);

  combo2 = gtk_combo_new ();
  gtk_widget_ref (combo2);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "combo2", combo2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo2);
  gtk_box_pack_start (GTK_BOX (hbox21), combo2, TRUE, TRUE, 0);

  entry_open = GTK_COMBO (combo2)->entry;
  gtk_widget_ref (entry_open);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "entry_open", entry_open,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (entry_open);

  label36 = gtk_label_new (_("Alternatively, you can build an MRL using one of the following predefined targets:"));
  gtk_widget_ref (label36);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "label36", label36,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label36);
  gtk_box_pack_start (GTK_BOX (open_vbox), label36, TRUE, TRUE, 0);
  gtk_label_set_justify (GTK_LABEL (label36), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label36), 0, 1);

  open_notebook = gtk_notebook_new ();
  gtk_widget_ref (open_notebook);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "open_notebook", open_notebook,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (open_notebook);
  gtk_box_pack_start (GTK_BOX (open_vbox), open_notebook, TRUE, TRUE, 0);

  hbox20 = gtk_hbox_new (FALSE, 5);
  gtk_widget_ref (hbox20);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "hbox20", hbox20,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox20);
  gtk_container_add (GTK_CONTAINER (open_notebook), hbox20);
  gtk_container_set_border_width (GTK_CONTAINER (hbox20), 5);

  combo1 = gtk_combo_new ();
  gtk_widget_ref (combo1);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "combo1", combo1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo1);
  gtk_box_pack_start (GTK_BOX (hbox20), combo1, FALSE, TRUE, 0);

  entry_file = GTK_COMBO (combo1)->entry;
  gtk_widget_ref (entry_file);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "entry_file", entry_file,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (entry_file);

  vbox13 = gtk_vbox_new (TRUE, 0);
  gtk_widget_ref (vbox13);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "vbox13", vbox13,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox13);
  gtk_box_pack_start (GTK_BOX (hbox20), vbox13, FALSE, FALSE, 0);

  open_browse = gtk_button_new_with_label (_("Browse..."));
  gtk_widget_ref (open_browse);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "open_browse", open_browse,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (open_browse);
  gtk_box_pack_start (GTK_BOX (vbox13), open_browse, FALSE, FALSE, 0);

  open_file = gtk_label_new (_("File"));
  gtk_widget_ref (open_file);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "open_file", open_file,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (open_file);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (open_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (open_notebook), 0), open_file);

  table5 = gtk_table_new (5, 2, FALSE);
  gtk_widget_ref (table5);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "table5", table5,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table5);
  gtk_container_add (GTK_CONTAINER (open_notebook), table5);
  gtk_container_set_border_width (GTK_CONTAINER (table5), 5);
  gtk_table_set_row_spacings (GTK_TABLE (table5), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table5), 5);

  label35 = gtk_label_new (_("Disc type"));
  gtk_widget_ref (label35);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "label35", label35,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label35);
  gtk_table_attach (GTK_TABLE (table5), label35, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label35), 0, 0.5);

  hbox24 = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (hbox24);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "hbox24", hbox24,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox24);
  gtk_table_attach (GTK_TABLE (table5), hbox24, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  disc_dvd = gtk_radio_button_new_with_label (disc_group, _("DVD"));
  disc_group = gtk_radio_button_group (GTK_RADIO_BUTTON (disc_dvd));
  gtk_widget_ref (disc_dvd);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "disc_dvd", disc_dvd,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_dvd);
  gtk_box_pack_start (GTK_BOX (hbox24), disc_dvd, FALSE, FALSE, 0);

  disc_vcd = gtk_radio_button_new_with_label (disc_group, _("VCD"));
  disc_group = gtk_radio_button_group (GTK_RADIO_BUTTON (disc_vcd));
  gtk_widget_ref (disc_vcd);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "disc_vcd", disc_vcd,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_vcd);
  gtk_box_pack_start (GTK_BOX (hbox24), disc_vcd, FALSE, FALSE, 0);

  disc_cdda = gtk_radio_button_new_with_label (disc_group, _("Audio CD"));
  disc_group = gtk_radio_button_group (GTK_RADIO_BUTTON (disc_cdda));
  gtk_widget_ref (disc_cdda);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "disc_cdda", disc_cdda,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_cdda);
  gtk_box_pack_start (GTK_BOX (hbox24), disc_cdda, FALSE, FALSE, 0);

  label19 = gtk_label_new (_("Device name"));
  gtk_widget_ref (label19);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "label19", label19,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label19);
  gtk_table_attach (GTK_TABLE (table5), label19, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  disc_name = gtk_entry_new ();
  gtk_widget_ref (disc_name);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "disc_name", disc_name,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_name);
  gtk_table_attach (GTK_TABLE (table5), disc_name, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_entry_set_text (GTK_ENTRY (disc_name), "/dev/dvd");

  disc_chapter_label = gtk_label_new (_("Chapter"));
  gtk_widget_ref (disc_chapter_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "disc_chapter_label", disc_chapter_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_chapter_label);
  gtk_table_attach (GTK_TABLE (table5), disc_chapter_label, 0, 1, 4, 5,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (disc_chapter_label), 0, 0.5);

  disc_title_label = gtk_label_new (_("Title"));
  gtk_widget_ref (disc_title_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "disc_title_label", disc_title_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_title_label);
  gtk_table_attach (GTK_TABLE (table5), disc_title_label, 0, 1, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (disc_title_label), 0, 0.5);

  disc_dvd_use_menu = gtk_check_button_new_with_label (_("Use DVD menus"));
  gtk_widget_ref (disc_dvd_use_menu);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "disc_dvd_use_menu", disc_dvd_use_menu,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_dvd_use_menu);
  gtk_table_attach (GTK_TABLE (table5), disc_dvd_use_menu, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (disc_dvd_use_menu), TRUE);

  disc_title_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  disc_title = gtk_spin_button_new (GTK_ADJUSTMENT (disc_title_adj), 1, 0);
  gtk_widget_ref (disc_title);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "disc_title", disc_title,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_title);
  gtk_table_attach (GTK_TABLE (table5), disc_title, 1, 2, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  disc_chapter_adj = gtk_adjustment_new (1, 1, 65536, 1, 10, 10);
  disc_chapter = gtk_spin_button_new (GTK_ADJUSTMENT (disc_chapter_adj), 1, 0);
  gtk_widget_ref (disc_chapter);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "disc_chapter", disc_chapter,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_chapter);
  gtk_table_attach (GTK_TABLE (table5), disc_chapter, 1, 2, 4, 5,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  open_disc = gtk_label_new (_("Disc"));
  gtk_widget_ref (open_disc);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "open_disc", open_disc,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (open_disc);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (open_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (open_notebook), 1), open_disc);

  table4 = gtk_table_new (3, 6, FALSE);
  gtk_widget_ref (table4);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "table4", table4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table4);
  gtk_container_add (GTK_CONTAINER (open_notebook), table4);
  gtk_container_set_border_width (GTK_CONTAINER (table4), 5);
  gtk_table_set_row_spacings (GTK_TABLE (table4), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table4), 5);

  network_udp = gtk_radio_button_new_with_label (table4_group, "UDP/RTP");
  table4_group = gtk_radio_button_group (GTK_RADIO_BUTTON (network_udp));
  gtk_widget_ref (network_udp);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "network_udp", network_udp,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_udp);
  gtk_table_attach (GTK_TABLE (table4), network_udp, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  network_multicast = gtk_radio_button_new_with_label (table4_group, _("UDP/RTP Multicast"));
  table4_group = gtk_radio_button_group (GTK_RADIO_BUTTON (network_multicast));
  gtk_widget_ref (network_multicast);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "network_multicast", network_multicast,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_multicast);
  gtk_table_attach (GTK_TABLE (table4), network_multicast, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  network_http = gtk_radio_button_new_with_label (table4_group, "HTTP/FTP/MMS");
  table4_group = gtk_radio_button_group (GTK_RADIO_BUTTON (network_http));
  gtk_widget_ref (network_http);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "network_http", network_http,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_http);
  gtk_table_attach (GTK_TABLE (table4), network_http, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  network_udp_port_label = gtk_label_new (_("Port"));
  gtk_widget_ref (network_udp_port_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "network_udp_port_label", network_udp_port_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_udp_port_label);
  gtk_table_attach (GTK_TABLE (table4), network_udp_port_label, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (network_udp_port_label), 1, 0.5);

  network_multicast_address_label = gtk_label_new (_("Address"));
  gtk_widget_ref (network_multicast_address_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "network_multicast_address_label", network_multicast_address_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_multicast_address_label);
  gtk_table_attach (GTK_TABLE (table4), network_multicast_address_label, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_sensitive (network_multicast_address_label, FALSE);
  gtk_label_set_justify (GTK_LABEL (network_multicast_address_label), GTK_JUSTIFY_RIGHT);
  gtk_misc_set_alignment (GTK_MISC (network_multicast_address_label), 1, 0.5);

  network_http_url_label = gtk_label_new (_("URL"));
  gtk_widget_ref (network_http_url_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "network_http_url_label", network_http_url_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_http_url_label);
  gtk_table_attach (GTK_TABLE (table4), network_http_url_label, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_sensitive (network_http_url_label, FALSE);
  gtk_misc_set_alignment (GTK_MISC (network_http_url_label), 1, 0.5);

  network_multicast_address_combo = gtk_combo_new ();
  gtk_widget_ref (network_multicast_address_combo);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "network_multicast_address_combo", network_multicast_address_combo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_multicast_address_combo);
  gtk_table_attach (GTK_TABLE (table4), network_multicast_address_combo, 2, 4, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_sensitive (network_multicast_address_combo, FALSE);

  network_multicast_address = GTK_COMBO (network_multicast_address_combo)->entry;
  gtk_widget_ref (network_multicast_address);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "network_multicast_address", network_multicast_address,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_multicast_address);

  network_multicast_port_label = gtk_label_new (_("Port"));
  gtk_widget_ref (network_multicast_port_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "network_multicast_port_label", network_multicast_port_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_multicast_port_label);
  gtk_table_attach (GTK_TABLE (table4), network_multicast_port_label, 4, 5, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_sensitive (network_multicast_port_label, FALSE);
  gtk_misc_set_alignment (GTK_MISC (network_multicast_port_label), 1, 0.5);

  network_multicast_port_adj = gtk_adjustment_new (1234, 0, 65535, 1, 10, 10);
  network_multicast_port = gtk_spin_button_new (GTK_ADJUSTMENT (network_multicast_port_adj), 1, 0);
  gtk_widget_ref (network_multicast_port);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "network_multicast_port", network_multicast_port,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_multicast_port);
  gtk_table_attach (GTK_TABLE (table4), network_multicast_port, 5, 6, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_usize (network_multicast_port, 75, -2);
  gtk_widget_set_sensitive (network_multicast_port, FALSE);

  network_http_url = gtk_entry_new ();
  gtk_widget_ref (network_http_url);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "network_http_url", network_http_url,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_http_url);
  gtk_table_attach (GTK_TABLE (table4), network_http_url, 2, 6, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_sensitive (network_http_url, FALSE);

  network_udp_port_adj = gtk_adjustment_new (1234, 0, 65535, 1, 10, 10);
  network_udp_port = gtk_spin_button_new (GTK_ADJUSTMENT (network_udp_port_adj), 1, 0);
  gtk_widget_ref (network_udp_port);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "network_udp_port", network_udp_port,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_udp_port);
  gtk_table_attach (GTK_TABLE (table4), network_udp_port, 2, 3, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_usize (network_udp_port, 1, -2);

  open_net = gtk_label_new (_("Network"));
  gtk_widget_ref (open_net);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "open_net", open_net,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (open_net);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (open_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (open_notebook), 2), open_net);

  table3 = gtk_table_new (4, 2, FALSE);
  gtk_widget_ref (table3);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "table3", table3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table3);
  gtk_container_add (GTK_CONTAINER (open_notebook), table3);
  gtk_container_set_border_width (GTK_CONTAINER (table3), 5);
  gtk_table_set_row_spacings (GTK_TABLE (table3), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table3), 5);

  label24 = gtk_label_new (_("Symbol Rate"));
  gtk_widget_ref (label24);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "label24", label24,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label24);
  gtk_table_attach (GTK_TABLE (table3), label24, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label24), 0, 0.5);

  label25 = gtk_label_new (_("Frequency"));
  gtk_widget_ref (label25);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "label25", label25,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label25);
  gtk_table_attach (GTK_TABLE (table3), label25, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label25), 0, 0.5);

  label26 = gtk_label_new (_("Polarization"));
  gtk_widget_ref (label26);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "label26", label26,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label26);
  gtk_table_attach (GTK_TABLE (table3), label26, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label26), 0, 0.5);

  sat_freq_adj = gtk_adjustment_new (11954, 10000, 12999, 1, 10, 10);
  sat_freq = gtk_spin_button_new (GTK_ADJUSTMENT (sat_freq_adj), 1, 0);
  gtk_widget_ref (sat_freq);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "sat_freq", sat_freq,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sat_freq);
  gtk_table_attach (GTK_TABLE (table3), sat_freq, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  label27 = gtk_label_new (_("FEC"));
  gtk_widget_ref (label27);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "label27", label27,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label27);
  gtk_table_attach (GTK_TABLE (table3), label27, 0, 1, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label27), 0, 0.5);

  hbox23 = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (hbox23);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "hbox23", hbox23,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox23);
  gtk_table_attach (GTK_TABLE (table3), hbox23, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  sat_pol_vert = gtk_radio_button_new_with_label (pol_group, _("Vertical"));
  pol_group = gtk_radio_button_group (GTK_RADIO_BUTTON (sat_pol_vert));
  gtk_widget_ref (sat_pol_vert);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "sat_pol_vert", sat_pol_vert,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sat_pol_vert);
  gtk_box_pack_start (GTK_BOX (hbox23), sat_pol_vert, FALSE, FALSE, 0);

  sat_pol_hor = gtk_radio_button_new_with_label (pol_group, _("Horizontal"));
  pol_group = gtk_radio_button_group (GTK_RADIO_BUTTON (sat_pol_hor));
  gtk_widget_ref (sat_pol_hor);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "sat_pol_hor", sat_pol_hor,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sat_pol_hor);
  gtk_box_pack_start (GTK_BOX (hbox23), sat_pol_hor, FALSE, FALSE, 0);

  sat_srate_adj = gtk_adjustment_new (27500, 1000, 30000, 1, 10, 10);
  sat_srate = gtk_spin_button_new (GTK_ADJUSTMENT (sat_srate_adj), 1, 0);
  gtk_widget_ref (sat_srate);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "sat_srate", sat_srate,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sat_srate);
  gtk_table_attach (GTK_TABLE (table3), sat_srate, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  sat_fec = gtk_combo_new ();
  gtk_widget_ref (sat_fec);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "sat_fec", sat_fec,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sat_fec);
  gtk_table_attach (GTK_TABLE (table3), sat_fec, 1, 2, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  sat_fec_items = g_list_append (sat_fec_items, (gpointer) "1/2");
  sat_fec_items = g_list_append (sat_fec_items, (gpointer) "2/3");
  sat_fec_items = g_list_append (sat_fec_items, (gpointer) "3/4");
  sat_fec_items = g_list_append (sat_fec_items, (gpointer) "4/5");
  sat_fec_items = g_list_append (sat_fec_items, (gpointer) "5/6");
  sat_fec_items = g_list_append (sat_fec_items, (gpointer) "7/8");
  gtk_combo_set_popdown_strings (GTK_COMBO (sat_fec), sat_fec_items);
  g_list_free (sat_fec_items);

  combo_entry1 = GTK_COMBO (sat_fec)->entry;
  gtk_widget_ref (combo_entry1);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "combo_entry1", combo_entry1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo_entry1);
  gtk_entry_set_text (GTK_ENTRY (combo_entry1), "3/4");

  open_sat = gtk_label_new (_("Satellite"));
  gtk_widget_ref (open_sat);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "open_sat", open_sat,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (open_sat);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (open_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (open_notebook), 3), open_sat);

  show_subtitle = gtk_check_button_new_with_label (_("Subtitle"));
  gtk_widget_ref (show_subtitle);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "show_subtitle", show_subtitle,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (show_subtitle);
  gtk_box_pack_start (GTK_BOX (dialog_vbox5), show_subtitle, FALSE, FALSE, 0);

  hbox_subtitle = gtk_hbox_new (FALSE, 5);
  gtk_widget_ref (hbox_subtitle);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "hbox_subtitle", hbox_subtitle,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox_subtitle);
  gtk_box_pack_start (GTK_BOX (dialog_vbox5), hbox_subtitle, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox_subtitle), 5);

  combo3 = gtk_combo_new ();
  gtk_widget_ref (combo3);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "combo3", combo3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo3);
  gtk_box_pack_start (GTK_BOX (hbox_subtitle), combo3, FALSE, TRUE, 0);

  entry_subtitle = GTK_COMBO (combo3)->entry;
  gtk_widget_ref (entry_subtitle);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "entry_subtitle", entry_subtitle,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (entry_subtitle);

  vbox14 = gtk_vbox_new (TRUE, 0);
  gtk_widget_ref (vbox14);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "vbox14", vbox14,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox14);
  gtk_box_pack_start (GTK_BOX (hbox_subtitle), vbox14, FALSE, FALSE, 0);

  button4 = gtk_button_new_with_label (_("Browse..."));
  gtk_widget_ref (button4);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "button4", button4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (button4);
  gtk_box_pack_start (GTK_BOX (vbox14), button4, FALSE, FALSE, 0);

  label37 = gtk_label_new (_("delay"));
  gtk_widget_ref (label37);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "label37", label37,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label37);
  gtk_box_pack_start (GTK_BOX (hbox_subtitle), label37, TRUE, TRUE, 0);

  subtitle_delay_adj = gtk_adjustment_new (0, -1000, 1000, 0.1, 10, 10);
  subtitle_delay = gtk_spin_button_new (GTK_ADJUSTMENT (subtitle_delay_adj), 1, 1);
  gtk_widget_ref (subtitle_delay);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "subtitle_delay", subtitle_delay,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (subtitle_delay);
  gtk_box_pack_start (GTK_BOX (hbox_subtitle), subtitle_delay, TRUE, TRUE, 0);

  label38 = gtk_label_new (_("fps"));
  gtk_widget_ref (label38);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "label38", label38,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label38);
  gtk_box_pack_start (GTK_BOX (hbox_subtitle), label38, TRUE, TRUE, 0);

  subtitle_fps_adj = gtk_adjustment_new (0, 0, 100, 0.1, 10, 10);
  subtitle_fps = gtk_spin_button_new (GTK_ADJUSTMENT (subtitle_fps_adj), 1, 1);
  gtk_widget_ref (subtitle_fps);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "subtitle_fps", subtitle_fps,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (subtitle_fps);
  gtk_box_pack_start (GTK_BOX (hbox_subtitle), subtitle_fps, TRUE, TRUE, 0);

  hbox28 = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (hbox28);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "hbox28", hbox28,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox28);
  gtk_box_pack_start (GTK_BOX (dialog_vbox5), hbox28, TRUE, TRUE, 2);

  show_sout_settings = gtk_check_button_new_with_label (_("stream output"));
  gtk_widget_ref (show_sout_settings);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "show_sout_settings", show_sout_settings,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (show_sout_settings);
  gtk_box_pack_start (GTK_BOX (hbox28), show_sout_settings, FALSE, FALSE, 0);

  sout_settings = gtk_button_new_with_label (_("Settings..."));
  gtk_widget_ref (sout_settings);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "sout_settings", sout_settings,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_settings);
  gtk_box_pack_start (GTK_BOX (hbox28), sout_settings, FALSE, FALSE, 20);

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

  gtk_signal_connect_after (GTK_OBJECT (open_notebook), "switch_page",
                            GTK_SIGNAL_FUNC (GtkOpenNotebookChanged),
                            NULL);
  gtk_signal_connect (GTK_OBJECT (entry_file), "changed",
                      GTK_SIGNAL_FUNC (GtkOpenChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (open_browse), "clicked",
                      GTK_SIGNAL_FUNC (GtkFileShow),
                      "entry_file");
  gtk_signal_connect (GTK_OBJECT (disc_dvd), "toggled",
                      GTK_SIGNAL_FUNC (GtkDiscOpenDvd),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (disc_vcd), "toggled",
                      GTK_SIGNAL_FUNC (GtkDiscOpenVcd),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (disc_cdda), "toggled",
                      GTK_SIGNAL_FUNC (GtkDiscOpenCDDA),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (disc_name), "changed",
                      GTK_SIGNAL_FUNC (GtkOpenChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (disc_dvd_use_menu), "toggled",
                      GTK_SIGNAL_FUNC (GtkOpenChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (disc_title), "changed",
                      GTK_SIGNAL_FUNC (GtkOpenChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (disc_chapter), "changed",
                      GTK_SIGNAL_FUNC (GtkOpenChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (network_udp), "toggled",
                      GTK_SIGNAL_FUNC (GtkNetworkOpenUDP),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (network_multicast), "toggled",
                      GTK_SIGNAL_FUNC (GtkNetworkOpenMulticast),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (network_http), "toggled",
                      GTK_SIGNAL_FUNC (GtkNetworkOpenHTTP),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (network_multicast_address), "changed",
                      GTK_SIGNAL_FUNC (GtkOpenChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (network_multicast_port), "changed",
                      GTK_SIGNAL_FUNC (GtkOpenChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (network_http_url), "changed",
                      GTK_SIGNAL_FUNC (GtkOpenChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (network_udp_port), "changed",
                      GTK_SIGNAL_FUNC (GtkOpenChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (sat_freq), "changed",
                      GTK_SIGNAL_FUNC (GtkOpenChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (sat_pol_vert), "toggled",
                      GTK_SIGNAL_FUNC (GtkSatOpenToggle),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (sat_pol_hor), "toggled",
                      GTK_SIGNAL_FUNC (GtkSatOpenToggle),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (sat_srate), "changed",
                      GTK_SIGNAL_FUNC (GtkOpenChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (combo_entry1), "changed",
                      GTK_SIGNAL_FUNC (GtkOpenChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (show_subtitle), "clicked",
                      GTK_SIGNAL_FUNC (GtkOpenSubtitleShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (entry_subtitle), "changed",
                      GTK_SIGNAL_FUNC (GtkOpenChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (button4), "clicked",
                      GTK_SIGNAL_FUNC (GtkFileShow),
                      "entry_subtitle");
  gtk_signal_connect (GTK_OBJECT (subtitle_delay), "changed",
                      GTK_SIGNAL_FUNC (GtkOpenChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (subtitle_fps), "changed",
                      GTK_SIGNAL_FUNC (GtkOpenChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (show_sout_settings), "clicked",
                      GTK_SIGNAL_FUNC (GtkOpenSoutShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (sout_settings), "clicked",
                      GTK_SIGNAL_FUNC (GtkSoutSettings),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (button1), "clicked",
                      GTK_SIGNAL_FUNC (GtkOpenOk),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (button3), "clicked",
                      GTK_SIGNAL_FUNC (GtkOpenCancel),
                      NULL);

  return intf_open;
}

GtkWidget*
create_intf_file (void)
{
  GtkWidget *intf_file;
  GtkWidget *file_ok;
  GtkWidget *file_cancel;

  intf_file = gtk_file_selection_new (_("Open File"));
  gtk_object_set_data (GTK_OBJECT (intf_file), "intf_file", intf_file);
  gtk_container_set_border_width (GTK_CONTAINER (intf_file), 10);
  gtk_window_set_modal (GTK_WINDOW (intf_file), TRUE);
  gtk_file_selection_hide_fileop_buttons (GTK_FILE_SELECTION (intf_file));

  file_ok = GTK_FILE_SELECTION (intf_file)->ok_button;
  gtk_object_set_data (GTK_OBJECT (intf_file), "file_ok", file_ok);
  gtk_widget_show (file_ok);
  GTK_WIDGET_SET_FLAGS (file_ok, GTK_CAN_DEFAULT);

  file_cancel = GTK_FILE_SELECTION (intf_file)->cancel_button;
  gtk_object_set_data (GTK_OBJECT (intf_file), "file_cancel", file_cancel);
  gtk_widget_show (file_cancel);
  GTK_WIDGET_SET_FLAGS (file_cancel, GTK_CAN_DEFAULT);

  gtk_signal_connect (GTK_OBJECT (file_ok), "clicked",
                      GTK_SIGNAL_FUNC (GtkFileOk),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (file_cancel), "clicked",
                      GTK_SIGNAL_FUNC (GtkFileCancel),
                      NULL);

  return intf_file;
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

static GnomeUIInfo playlist_add_menu_uiinfo[] =
{
  {
    GNOME_APP_UI_ITEM, N_("Disc"),
    NULL,
    (gpointer) GtkDiscOpenShow, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("File"),
    NULL,
    (gpointer) GtkFileOpenShow, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Network"),
    NULL,
    (gpointer) GtkNetworkOpenShow, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("URL"),
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
  gnome_dialog_close_hides (GNOME_DIALOG (intf_playlist), TRUE);

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
  gtk_clist_set_selection_mode (GTK_CLIST (playlist_clist), GTK_SELECTION_EXTENDED);
  gtk_clist_column_titles_show (GTK_CLIST (playlist_clist));

  playlist_label_url = gtk_label_new (_("URL"));
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
                      NULL);
  gtk_signal_connect (GTK_OBJECT (intf_playlist), "delete_event",
                      GTK_SIGNAL_FUNC (gtk_widget_hide),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (playlist_clist), "event",
                      GTK_SIGNAL_FUNC (GtkPlaylistEvent),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (playlist_clist), "drag_data_received",
                      GTK_SIGNAL_FUNC (GtkPlaylistDragData),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (playlist_clist), "drag_motion",
                      GTK_SIGNAL_FUNC (GtkPlaylistDragMotion),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (playlist_ok), "clicked",
                      GTK_SIGNAL_FUNC (GtkPlaylistOk),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (playlist_cancel), "clicked",
                      GTK_SIGNAL_FUNC (GtkPlaylistCancel),
                      NULL);

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

  jump_label3 = gtk_label_new ("s.");
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

  jump_label1 = gtk_label_new ("m:");
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

  jump_label2 = gtk_label_new ("h:");
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
                      NULL);
  gtk_signal_connect (GTK_OBJECT (jump_cancel), "clicked",
                      GTK_SIGNAL_FUNC (GtkJumpCancel),
                      NULL);

  return intf_jump;
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
                      NULL);
  gtk_signal_connect (GTK_OBJECT (intf_messages), "delete_event",
                      GTK_SIGNAL_FUNC (gtk_widget_hide),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (messages_ok), "clicked",
                      GTK_SIGNAL_FUNC (GtkMessagesOk),
                      NULL);

  return intf_messages;
}

GtkWidget*
create_intf_sout (void)
{
  GtkWidget *intf_sout;
  GtkWidget *dialog_vbox7;
  GtkWidget *vbox16;
  GtkWidget *frame11;
  GtkWidget *hbox26;
  GtkWidget *label39;
  GtkWidget *combo4;
  GtkWidget *sout_entry_target;
  GtkWidget *frame12;
  GtkWidget *table6;
  GSList *sout_access_group = NULL;
  GtkWidget *sout_access_file;
  GtkWidget *sout_access_udp;
  GtkWidget *sout_access_rtp;
  GtkWidget *sout_file_path_label;
  GtkWidget *sout_udp_address_label;
  GtkWidget *sout_udp_address_combo;
  GtkWidget *sout_udp_address;
  GtkWidget *sout_udp_port_label;
  GtkObject *sout_udp_port_adj;
  GtkWidget *sout_udp_port;
  GtkWidget *combo5;
  GtkWidget *sout_file_path;
  GtkWidget *hbox27;
  GSList *sout_mux_group = NULL;
  GtkWidget *sout_mux_ts;
  GtkWidget *sout_mux_ps;
  GtkWidget *sout_mux_avi;
  GtkWidget *dialog_action_area7;
  GtkWidget *button7;
  GtkWidget *button9;

  intf_sout = gnome_dialog_new (NULL, NULL);
  gtk_object_set_data (GTK_OBJECT (intf_sout), "intf_sout", intf_sout);
  gtk_container_set_border_width (GTK_CONTAINER (intf_sout), 5);
  gtk_window_set_modal (GTK_WINDOW (intf_sout), TRUE);
  gtk_window_set_policy (GTK_WINDOW (intf_sout), FALSE, FALSE, FALSE);

  dialog_vbox7 = GNOME_DIALOG (intf_sout)->vbox;
  gtk_object_set_data (GTK_OBJECT (intf_sout), "dialog_vbox7", dialog_vbox7);
  gtk_widget_show (dialog_vbox7);

  vbox16 = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (vbox16);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "vbox16", vbox16,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox16);
  gtk_box_pack_start (GTK_BOX (dialog_vbox7), vbox16, TRUE, TRUE, 0);

  frame11 = gtk_frame_new (_("stream output (MRL)"));
  gtk_widget_ref (frame11);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "frame11", frame11,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame11);
  gtk_box_pack_start (GTK_BOX (vbox16), frame11, FALSE, TRUE, 0);

  hbox26 = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (hbox26);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "hbox26", hbox26,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox26);
  gtk_container_add (GTK_CONTAINER (frame11), hbox26);
  gtk_container_set_border_width (GTK_CONTAINER (hbox26), 5);

  label39 = gtk_label_new (_("Destination Target: "));
  gtk_widget_ref (label39);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "label39", label39,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label39);
  gtk_box_pack_start (GTK_BOX (hbox26), label39, FALSE, FALSE, 0);

  combo4 = gtk_combo_new ();
  gtk_widget_ref (combo4);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "combo4", combo4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo4);
  gtk_box_pack_start (GTK_BOX (hbox26), combo4, TRUE, TRUE, 0);

  sout_entry_target = GTK_COMBO (combo4)->entry;
  gtk_widget_ref (sout_entry_target);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_entry_target", sout_entry_target,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_entry_target);

  frame12 = gtk_frame_new (NULL);
  gtk_widget_ref (frame12);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "frame12", frame12,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame12);
  gtk_box_pack_start (GTK_BOX (vbox16), frame12, TRUE, TRUE, 0);

  table6 = gtk_table_new (3, 5, FALSE);
  gtk_widget_ref (table6);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "table6", table6,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table6);
  gtk_container_add (GTK_CONTAINER (frame12), table6);
  gtk_table_set_row_spacings (GTK_TABLE (table6), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table6), 5);

  sout_access_file = gtk_radio_button_new_with_label (sout_access_group, _("File"));
  sout_access_group = gtk_radio_button_group (GTK_RADIO_BUTTON (sout_access_file));
  gtk_widget_ref (sout_access_file);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_access_file", sout_access_file,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_access_file);
  gtk_table_attach (GTK_TABLE (table6), sout_access_file, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  sout_access_udp = gtk_radio_button_new_with_label (sout_access_group, _("UDP"));
  sout_access_group = gtk_radio_button_group (GTK_RADIO_BUTTON (sout_access_udp));
  gtk_widget_ref (sout_access_udp);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_access_udp", sout_access_udp,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_access_udp);
  gtk_table_attach (GTK_TABLE (table6), sout_access_udp, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  sout_access_rtp = gtk_radio_button_new_with_label (sout_access_group, _("RTP"));
  sout_access_group = gtk_radio_button_group (GTK_RADIO_BUTTON (sout_access_rtp));
  gtk_widget_ref (sout_access_rtp);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_access_rtp", sout_access_rtp,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_access_rtp);
  gtk_table_attach (GTK_TABLE (table6), sout_access_rtp, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  sout_file_path_label = gtk_label_new (_("Path:"));
  gtk_widget_ref (sout_file_path_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_file_path_label", sout_file_path_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_file_path_label);
  gtk_table_attach (GTK_TABLE (table6), sout_file_path_label, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (sout_file_path_label), 0, 0.5);

  sout_udp_address_label = gtk_label_new (_("Address:"));
  gtk_widget_ref (sout_udp_address_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_udp_address_label", sout_udp_address_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_udp_address_label);
  gtk_table_attach (GTK_TABLE (table6), sout_udp_address_label, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (sout_udp_address_label), 0, 0.5);

  sout_udp_address_combo = gtk_combo_new ();
  gtk_widget_ref (sout_udp_address_combo);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_udp_address_combo", sout_udp_address_combo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_udp_address_combo);
  gtk_table_attach (GTK_TABLE (table6), sout_udp_address_combo, 2, 3, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  sout_udp_address = GTK_COMBO (sout_udp_address_combo)->entry;
  gtk_widget_ref (sout_udp_address);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_udp_address", sout_udp_address,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_udp_address);

  sout_udp_port_label = gtk_label_new (_("Port"));
  gtk_widget_ref (sout_udp_port_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_udp_port_label", sout_udp_port_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_udp_port_label);
  gtk_table_attach (GTK_TABLE (table6), sout_udp_port_label, 3, 4, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (sout_udp_port_label), 0, 0.5);

  sout_udp_port_adj = gtk_adjustment_new (1234, 0, 65535, 1, 10, 10);
  sout_udp_port = gtk_spin_button_new (GTK_ADJUSTMENT (sout_udp_port_adj), 1, 0);
  gtk_widget_ref (sout_udp_port);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_udp_port", sout_udp_port,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_udp_port);
  gtk_table_attach (GTK_TABLE (table6), sout_udp_port, 4, 5, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  combo5 = gtk_combo_new ();
  gtk_widget_ref (combo5);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "combo5", combo5,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo5);
  gtk_table_attach (GTK_TABLE (table6), combo5, 2, 5, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  sout_file_path = GTK_COMBO (combo5)->entry;
  gtk_widget_ref (sout_file_path);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_file_path", sout_file_path,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_file_path);

  hbox27 = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (hbox27);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "hbox27", hbox27,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox27);
  gtk_table_attach (GTK_TABLE (table6), hbox27, 4, 5, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  sout_mux_ts = gtk_radio_button_new_with_label (sout_mux_group, _("TS"));
  sout_mux_group = gtk_radio_button_group (GTK_RADIO_BUTTON (sout_mux_ts));
  gtk_widget_ref (sout_mux_ts);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_mux_ts", sout_mux_ts,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_mux_ts);
  gtk_box_pack_start (GTK_BOX (hbox27), sout_mux_ts, FALSE, FALSE, 0);

  sout_mux_ps = gtk_radio_button_new_with_label (sout_mux_group, _("PS"));
  sout_mux_group = gtk_radio_button_group (GTK_RADIO_BUTTON (sout_mux_ps));
  gtk_widget_ref (sout_mux_ps);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_mux_ps", sout_mux_ps,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_mux_ps);
  gtk_box_pack_start (GTK_BOX (hbox27), sout_mux_ps, FALSE, FALSE, 0);

  sout_mux_avi = gtk_radio_button_new_with_label (sout_mux_group, _("AVI"));
  sout_mux_group = gtk_radio_button_group (GTK_RADIO_BUTTON (sout_mux_avi));
  gtk_widget_ref (sout_mux_avi);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_mux_avi", sout_mux_avi,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_mux_avi);
  gtk_box_pack_start (GTK_BOX (hbox27), sout_mux_avi, FALSE, FALSE, 0);

  dialog_action_area7 = GNOME_DIALOG (intf_sout)->action_area;
  gtk_object_set_data (GTK_OBJECT (intf_sout), "dialog_action_area7", dialog_action_area7);
  gtk_widget_show (dialog_action_area7);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area7), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (dialog_action_area7), 8);

  gnome_dialog_append_button (GNOME_DIALOG (intf_sout), GNOME_STOCK_BUTTON_OK);
  button7 = GTK_WIDGET (g_list_last (GNOME_DIALOG (intf_sout)->buttons)->data);
  gtk_widget_ref (button7);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "button7", button7,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (button7);
  GTK_WIDGET_SET_FLAGS (button7, GTK_CAN_DEFAULT);

  gnome_dialog_append_button (GNOME_DIALOG (intf_sout), GNOME_STOCK_BUTTON_CANCEL);
  button9 = GTK_WIDGET (g_list_last (GNOME_DIALOG (intf_sout)->buttons)->data);
  gtk_widget_ref (button9);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "button9", button9,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (button9);
  GTK_WIDGET_SET_FLAGS (button9, GTK_CAN_DEFAULT);

  gtk_signal_connect (GTK_OBJECT (sout_access_file), "toggled",
                      GTK_SIGNAL_FUNC (GtkSoutSettingsAccessFile),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (sout_access_udp), "toggled",
                      GTK_SIGNAL_FUNC (GtkSoutSettingsAccessUdp),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (sout_access_rtp), "toggled",
                      GTK_SIGNAL_FUNC (GtkSoutSettingsAccessUdp),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (sout_udp_address), "changed",
                      GTK_SIGNAL_FUNC (GtkSoutSettingsChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (sout_udp_port), "changed",
                      GTK_SIGNAL_FUNC (GtkSoutSettingsChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (sout_file_path), "changed",
                      GTK_SIGNAL_FUNC (GtkSoutSettingsChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (sout_mux_ts), "toggled",
                      GTK_SIGNAL_FUNC (GtkSoutSettingsChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (sout_mux_ps), "toggled",
                      GTK_SIGNAL_FUNC (GtkSoutSettingsChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (sout_mux_avi), "toggled",
                      GTK_SIGNAL_FUNC (GtkSoutSettingsChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (button7), "clicked",
                      GTK_SIGNAL_FUNC (GtkSoutSettingsOk),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (button9), "clicked",
                      GTK_SIGNAL_FUNC (GtkSoutSettingsCancel),
                      NULL);

  return intf_sout;
}

