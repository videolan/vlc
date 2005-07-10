/* This file was created automatically by glade and fixed by bootstrap */

#include <vlc/vlc.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gtk_callbacks.h"
#include "gtk_interface.h"
#include "gtk_support.h"

GtkWidget*
create_intf_window (void)
{
  GtkWidget *intf_window;
  GtkWidget *window_vbox;
  GtkWidget *menubar_handlebox;
  GtkWidget *menubar;
  guint tmp_key;
  GtkWidget *menubar_file;
  GtkWidget *menubar_file_menu;
  GtkAccelGroup *menubar_file_menu_accels;
  GtkWidget *menubar_open;
  GtkWidget *menubar_disc;
  GtkWidget *menubar_network;
  GtkWidget *separator4;
  GtkWidget *menubar_eject;
  GtkWidget *separator14;
  GtkWidget *menubar_close;
  GtkWidget *menubar_exit;
  GtkWidget *menubar_view;
  GtkWidget *menubar_view_menu;
  GtkAccelGroup *menubar_view_menu_accels;
  GtkWidget *menubar_interface_hide;
  GtkWidget *separator13;
  GtkWidget *menubar_program;
  GtkWidget *menubar_title;
  GtkWidget *menubar_chapter;
  GtkWidget *separator11;
  GtkWidget *menubar_playlist;
  GtkWidget *menubar_modules;
  GtkWidget *menubar_messages;
  GtkWidget *menubar_settings;
  GtkWidget *menubar_settings_menu;
  GtkAccelGroup *menubar_settings_menu_accels;
  GtkWidget *menubar_preferences;
  GtkWidget *menubar_config_audio;
  GtkWidget *menubar_config_audio_menu;
  GtkAccelGroup *menubar_config_audio_menu_accels;
  GtkWidget *menubar_audio;
  GtkWidget *separator18;
  GtkWidget *menubar_volume_up;
  GtkWidget *menubar_volume_down;
  GtkWidget *menubar_volume_mute;
  GtkWidget *separator15;
  GtkWidget *menubar_audio_channels;
  GtkWidget *menubar_audio_device;
  GtkWidget *menubar_config_video;
  GtkWidget *menubar_config_video_menu;
  GtkAccelGroup *menubar_config_video_menu_accels;
  GtkWidget *menubar_subpictures;
  GtkWidget *separator25;
  GtkWidget *menubar_fullscreen;
  GtkWidget *separator24;
  GtkWidget *menubar_deinterlace;
  GtkWidget *menubar_video_device;
  GtkWidget *menubar_help;
  GtkWidget *menubar_help_menu;
  GtkAccelGroup *menubar_help_menu_accels;
  GtkWidget *menubar_about;
  GtkWidget *toolbar_handlebox;
  GtkWidget *toolbar;
  GtkWidget *toolbar_open;
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
  GtkWidget *slider_frame;
  GtkWidget *slider;
  GtkWidget *file_box;
  GtkWidget *label_status;
  GtkWidget *dvd_box;
  GtkWidget *dvd_label;
  GtkWidget *title_box;
  GtkWidget *title;
  GtkWidget *title_label;
  GtkWidget *title_prev_button;
  GtkWidget *title_next_button;
  GtkWidget *dvd_separator;
  GtkWidget *chapter_box;
  GtkWidget *chapter;
  GtkWidget *chapter_label;
  GtkWidget *chapter_prev_button;
  GtkWidget *chapter_next_button;
  GtkWidget *network_box;
  GtkWidget *network_address_label;
  GtkWidget *intf_statusbar;
  GtkAccelGroup *accel_group;
  GtkTooltips *tooltips;

  tooltips = gtk_tooltips_new ();

  accel_group = gtk_accel_group_new ();

  intf_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_object_set_data (GTK_OBJECT (intf_window), "intf_window", intf_window);
  gtk_window_set_title (GTK_WINDOW (intf_window), _("VLC media player"));
  gtk_window_set_policy (GTK_WINDOW (intf_window), TRUE, TRUE, TRUE);

  window_vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (window_vbox);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "window_vbox", window_vbox,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (window_vbox);
  gtk_container_add (GTK_CONTAINER (intf_window), window_vbox);

  menubar_handlebox = gtk_handle_box_new ();
  gtk_widget_ref (menubar_handlebox);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_handlebox", menubar_handlebox,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_handlebox);
  gtk_box_pack_start (GTK_BOX (window_vbox), menubar_handlebox, FALSE, TRUE, 0);

  menubar = gtk_menu_bar_new ();
  gtk_widget_ref (menubar);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar", menubar,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar);
  gtk_container_add (GTK_CONTAINER (menubar_handlebox), menubar);

  menubar_file = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_file)->child),
                                   _("_File"));
  gtk_widget_add_accelerator (menubar_file, "activate_item", accel_group,
                              tmp_key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
  gtk_widget_ref (menubar_file);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_file", menubar_file,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_file);
  gtk_container_add (GTK_CONTAINER (menubar), menubar_file);

  menubar_file_menu = gtk_menu_new ();
  gtk_widget_ref (menubar_file_menu);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_file_menu", menubar_file_menu,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menubar_file), menubar_file_menu);
  menubar_file_menu_accels = gtk_menu_get_accel_group (GTK_MENU (menubar_file_menu));

  menubar_open = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_open)->child),
                                   _("_Open File..."));
  gtk_widget_add_accelerator (menubar_open, "activate_item", menubar_file_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_open);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_open", menubar_open,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_open);
  gtk_container_add (GTK_CONTAINER (menubar_file_menu), menubar_open);
  gtk_tooltips_set_tip (tooltips, menubar_open, _("Open a file"), NULL);
  gtk_widget_add_accelerator (menubar_open, "activate", accel_group,
                              GDK_F3, 0,
                              GTK_ACCEL_VISIBLE);

  menubar_disc = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_disc)->child),
                                   _("Open _Disc..."));
  gtk_widget_add_accelerator (menubar_disc, "activate_item", menubar_file_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_disc);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_disc", menubar_disc,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_disc);
  gtk_container_add (GTK_CONTAINER (menubar_file_menu), menubar_disc);
  gtk_tooltips_set_tip (tooltips, menubar_disc, _("Open Disc Media"), NULL);
  gtk_widget_add_accelerator (menubar_disc, "activate", accel_group,
                              GDK_F4, 0,
                              GTK_ACCEL_VISIBLE);

  menubar_network = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_network)->child),
                                   _("_Network Stream..."));
  gtk_widget_add_accelerator (menubar_network, "activate_item", menubar_file_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_network);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_network", menubar_network,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_network);
  gtk_container_add (GTK_CONTAINER (menubar_file_menu), menubar_network);
  gtk_tooltips_set_tip (tooltips, menubar_network, _("Select a network stream"), NULL);
  gtk_widget_add_accelerator (menubar_network, "activate", accel_group,
                              GDK_F5, 0,
                              GTK_ACCEL_VISIBLE);

  separator4 = gtk_menu_item_new ();
  gtk_widget_ref (separator4);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator4", separator4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator4);
  gtk_container_add (GTK_CONTAINER (menubar_file_menu), separator4);
  gtk_widget_set_sensitive (separator4, FALSE);

  menubar_eject = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_eject)->child),
                                   _("_Eject Disc"));
  gtk_widget_add_accelerator (menubar_eject, "activate_item", menubar_file_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_eject);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_eject", menubar_eject,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_eject);
  gtk_container_add (GTK_CONTAINER (menubar_file_menu), menubar_eject);
  gtk_tooltips_set_tip (tooltips, menubar_eject, _("Eject disc"), NULL);

  separator14 = gtk_menu_item_new ();
  gtk_widget_ref (separator14);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator14", separator14,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator14);
  gtk_container_add (GTK_CONTAINER (menubar_file_menu), separator14);
  gtk_widget_set_sensitive (separator14, FALSE);

  menubar_close = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_close)->child),
                                   _("_Close"));
  gtk_widget_add_accelerator (menubar_close, "activate_item", menubar_file_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_close);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_close", menubar_close,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_close);
  gtk_container_add (GTK_CONTAINER (menubar_file_menu), menubar_close);
  gtk_tooltips_set_tip (tooltips, menubar_close, _("Close the window"), NULL);
  gtk_widget_add_accelerator (menubar_close, "activate", accel_group,
                              GDK_W, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  menubar_exit = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_exit)->child),
                                   _("E_xit"));
  gtk_widget_add_accelerator (menubar_exit, "activate_item", menubar_file_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_exit);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_exit", menubar_exit,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_exit);
  gtk_container_add (GTK_CONTAINER (menubar_file_menu), menubar_exit);
  gtk_tooltips_set_tip (tooltips, menubar_exit, _("Exit the program"), NULL);
  gtk_widget_add_accelerator (menubar_exit, "activate", accel_group,
                              GDK_Q, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  menubar_view = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_view)->child),
                                   _("_View"));
  gtk_widget_add_accelerator (menubar_view, "activate_item", accel_group,
                              tmp_key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
  gtk_widget_ref (menubar_view);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_view", menubar_view,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_view);
  gtk_container_add (GTK_CONTAINER (menubar), menubar_view);

  menubar_view_menu = gtk_menu_new ();
  gtk_widget_ref (menubar_view_menu);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_view_menu", menubar_view_menu,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menubar_view), menubar_view_menu);
  menubar_view_menu_accels = gtk_menu_get_accel_group (GTK_MENU (menubar_view_menu));

  menubar_interface_hide = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_interface_hide)->child),
                                   _("_Hide interface"));
  gtk_widget_add_accelerator (menubar_interface_hide, "activate_item", menubar_view_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_interface_hide);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_interface_hide", menubar_interface_hide,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_interface_hide);
  gtk_container_add (GTK_CONTAINER (menubar_view_menu), menubar_interface_hide);
  gtk_tooltips_set_tip (tooltips, menubar_interface_hide, _("Hide the main interface window"), NULL);

  separator13 = gtk_menu_item_new ();
  gtk_widget_ref (separator13);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator13", separator13,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator13);
  gtk_container_add (GTK_CONTAINER (menubar_view_menu), separator13);
  gtk_widget_set_sensitive (separator13, FALSE);

  menubar_program = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_program)->child),
                                   _("Progr_am"));
  gtk_widget_add_accelerator (menubar_program, "activate_item", menubar_view_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_program);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_program", menubar_program,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_program);
  gtk_container_add (GTK_CONTAINER (menubar_view_menu), menubar_program);
  gtk_widget_set_sensitive (menubar_program, FALSE);
  gtk_tooltips_set_tip (tooltips, menubar_program, _("Choose the program"), NULL);

  menubar_title = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_title)->child),
                                   _("_Title"));
  gtk_widget_add_accelerator (menubar_title, "activate_item", menubar_view_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_title);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_title", menubar_title,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_title);
  gtk_container_add (GTK_CONTAINER (menubar_view_menu), menubar_title);
  gtk_widget_set_sensitive (menubar_title, FALSE);
  gtk_tooltips_set_tip (tooltips, menubar_title, _("Navigate through the stream"), NULL);

  menubar_chapter = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_chapter)->child),
                                   _("_Chapter"));
  gtk_widget_add_accelerator (menubar_chapter, "activate_item", menubar_view_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_chapter);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_chapter", menubar_chapter,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_chapter);
  gtk_container_add (GTK_CONTAINER (menubar_view_menu), menubar_chapter);
  gtk_widget_set_sensitive (menubar_chapter, FALSE);

  separator11 = gtk_menu_item_new ();
  gtk_widget_ref (separator11);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator11", separator11,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator11);
  gtk_container_add (GTK_CONTAINER (menubar_view_menu), separator11);
  gtk_widget_set_sensitive (separator11, FALSE);

  menubar_playlist = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_playlist)->child),
                                   _("_Playlist..."));
  gtk_widget_add_accelerator (menubar_playlist, "activate_item", menubar_view_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_playlist);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_playlist", menubar_playlist,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_playlist);
  gtk_container_add (GTK_CONTAINER (menubar_view_menu), menubar_playlist);
  gtk_tooltips_set_tip (tooltips, menubar_playlist, _("Open the playlist window"), NULL);

  menubar_modules = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_modules)->child),
                                   _("_Modules..."));
  gtk_widget_add_accelerator (menubar_modules, "activate_item", menubar_view_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_modules);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_modules", menubar_modules,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_modules);
  gtk_container_add (GTK_CONTAINER (menubar_view_menu), menubar_modules);
  gtk_widget_set_sensitive (menubar_modules, FALSE);
  gtk_tooltips_set_tip (tooltips, menubar_modules, _("Open the module manager"), NULL);

  menubar_messages = gtk_menu_item_new_with_label (_("Messages..."));
  gtk_widget_ref (menubar_messages);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_messages", menubar_messages,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_messages);
  gtk_container_add (GTK_CONTAINER (menubar_view_menu), menubar_messages);
  gtk_tooltips_set_tip (tooltips, menubar_messages, _("Open the messages window"), NULL);

  menubar_settings = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_settings)->child),
                                   _("_Settings"));
  gtk_widget_add_accelerator (menubar_settings, "activate_item", accel_group,
                              tmp_key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
  gtk_widget_ref (menubar_settings);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_settings", menubar_settings,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_settings);
  gtk_container_add (GTK_CONTAINER (menubar), menubar_settings);

  menubar_settings_menu = gtk_menu_new ();
  gtk_widget_ref (menubar_settings_menu);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_settings_menu", menubar_settings_menu,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menubar_settings), menubar_settings_menu);
  menubar_settings_menu_accels = gtk_menu_get_accel_group (GTK_MENU (menubar_settings_menu));

  menubar_preferences = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_preferences)->child),
                                   _("_Preferences..."));
  gtk_widget_add_accelerator (menubar_preferences, "activate_item", menubar_settings_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_preferences);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_preferences", menubar_preferences,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_preferences);
  gtk_container_add (GTK_CONTAINER (menubar_settings_menu), menubar_preferences);
  gtk_tooltips_set_tip (tooltips, menubar_preferences, _("Configure the application"), NULL);

  menubar_config_audio = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_config_audio)->child),
                                   _("_Audio"));
  gtk_widget_add_accelerator (menubar_config_audio, "activate_item", accel_group,
                              tmp_key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
  gtk_widget_ref (menubar_config_audio);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_config_audio", menubar_config_audio,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_config_audio);
  gtk_container_add (GTK_CONTAINER (menubar), menubar_config_audio);

  menubar_config_audio_menu = gtk_menu_new ();
  gtk_widget_ref (menubar_config_audio_menu);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_config_audio_menu", menubar_config_audio_menu,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menubar_config_audio), menubar_config_audio_menu);
  menubar_config_audio_menu_accels = gtk_menu_get_accel_group (GTK_MENU (menubar_config_audio_menu));

  menubar_audio = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_audio)->child),
                                   _("_Language"));
  gtk_widget_add_accelerator (menubar_audio, "activate_item", menubar_config_audio_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_audio);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_audio", menubar_audio,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_audio);
  gtk_container_add (GTK_CONTAINER (menubar_config_audio_menu), menubar_audio);
  gtk_widget_set_sensitive (menubar_audio, FALSE);
  gtk_tooltips_set_tip (tooltips, menubar_audio, _("Select audio channel"), NULL);

  separator18 = gtk_menu_item_new ();
  gtk_widget_ref (separator18);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator18", separator18,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator18);
  gtk_container_add (GTK_CONTAINER (menubar_config_audio_menu), separator18);
  gtk_widget_set_sensitive (separator18, FALSE);

  menubar_volume_up = gtk_menu_item_new_with_label (_("Volume Up"));
  gtk_widget_ref (menubar_volume_up);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_volume_up", menubar_volume_up,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_volume_up);
  gtk_container_add (GTK_CONTAINER (menubar_config_audio_menu), menubar_volume_up);

  menubar_volume_down = gtk_menu_item_new_with_label (_("Volume Down"));
  gtk_widget_ref (menubar_volume_down);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_volume_down", menubar_volume_down,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_volume_down);
  gtk_container_add (GTK_CONTAINER (menubar_config_audio_menu), menubar_volume_down);

  menubar_volume_mute = gtk_menu_item_new_with_label (_("Mute"));
  gtk_widget_ref (menubar_volume_mute);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_volume_mute", menubar_volume_mute,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_volume_mute);
  gtk_container_add (GTK_CONTAINER (menubar_config_audio_menu), menubar_volume_mute);

  separator15 = gtk_menu_item_new ();
  gtk_widget_ref (separator15);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator15", separator15,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator15);
  gtk_container_add (GTK_CONTAINER (menubar_config_audio_menu), separator15);
  gtk_widget_set_sensitive (separator15, FALSE);

  menubar_audio_channels = gtk_menu_item_new_with_label (_("Channels"));
  gtk_widget_ref (menubar_audio_channels);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_audio_channels", menubar_audio_channels,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_audio_channels);
  gtk_container_add (GTK_CONTAINER (menubar_config_audio_menu), menubar_audio_channels);

  menubar_audio_device = gtk_menu_item_new_with_label (_("Device"));
  gtk_widget_ref (menubar_audio_device);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_audio_device", menubar_audio_device,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_audio_device);
  gtk_container_add (GTK_CONTAINER (menubar_config_audio_menu), menubar_audio_device);

  menubar_config_video = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_config_video)->child),
                                   _("_Video"));
  gtk_widget_add_accelerator (menubar_config_video, "activate_item", accel_group,
                              tmp_key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
  gtk_widget_ref (menubar_config_video);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_config_video", menubar_config_video,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_config_video);
  gtk_container_add (GTK_CONTAINER (menubar), menubar_config_video);

  menubar_config_video_menu = gtk_menu_new ();
  gtk_widget_ref (menubar_config_video_menu);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_config_video_menu", menubar_config_video_menu,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menubar_config_video), menubar_config_video_menu);
  menubar_config_video_menu_accels = gtk_menu_get_accel_group (GTK_MENU (menubar_config_video_menu));

  menubar_subpictures = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_subpictures)->child),
                                   _("_Subtitles"));
  gtk_widget_add_accelerator (menubar_subpictures, "activate_item", menubar_config_video_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_subpictures);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_subpictures", menubar_subpictures,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_subpictures);
  gtk_container_add (GTK_CONTAINER (menubar_config_video_menu), menubar_subpictures);
  gtk_widget_set_sensitive (menubar_subpictures, FALSE);
  gtk_tooltips_set_tip (tooltips, menubar_subpictures, _("Select subtitles channel"), NULL);

  separator25 = gtk_menu_item_new ();
  gtk_widget_ref (separator25);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator25", separator25,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator25);
  gtk_container_add (GTK_CONTAINER (menubar_config_video_menu), separator25);
  gtk_widget_set_sensitive (separator25, FALSE);

  menubar_fullscreen = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_fullscreen)->child),
                                   _("_Fullscreen"));
  gtk_widget_add_accelerator (menubar_fullscreen, "activate_item", menubar_config_video_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_fullscreen);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_fullscreen", menubar_fullscreen,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_fullscreen);
  gtk_container_add (GTK_CONTAINER (menubar_config_video_menu), menubar_fullscreen);

  separator24 = gtk_menu_item_new ();
  gtk_widget_ref (separator24);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator24", separator24,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator24);
  gtk_container_add (GTK_CONTAINER (menubar_config_video_menu), separator24);
  gtk_widget_set_sensitive (separator24, FALSE);

  menubar_deinterlace = gtk_menu_item_new_with_label (_("Deinterlace"));
  gtk_widget_ref (menubar_deinterlace);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_deinterlace", menubar_deinterlace,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_deinterlace);
  gtk_container_add (GTK_CONTAINER (menubar_config_video_menu), menubar_deinterlace);

  menubar_video_device = gtk_menu_item_new_with_label (_("Screen"));
  gtk_widget_ref (menubar_video_device);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_video_device", menubar_video_device,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_video_device);
  gtk_container_add (GTK_CONTAINER (menubar_config_video_menu), menubar_video_device);

  menubar_help = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_help)->child),
                                   _("_Help"));
  gtk_widget_add_accelerator (menubar_help, "activate_item", accel_group,
                              tmp_key, GDK_MOD1_MASK, (GtkAccelFlags) 0);
  gtk_widget_ref (menubar_help);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_help", menubar_help,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_help);
  gtk_container_add (GTK_CONTAINER (menubar), menubar_help);

  menubar_help_menu = gtk_menu_new ();
  gtk_widget_ref (menubar_help_menu);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_help_menu", menubar_help_menu,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menubar_help), menubar_help_menu);
  menubar_help_menu_accels = gtk_menu_get_accel_group (GTK_MENU (menubar_help_menu));

  menubar_about = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_about)->child),
                                   _("_About..."));
  gtk_widget_add_accelerator (menubar_about, "activate_item", menubar_help_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_about);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_about", menubar_about,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_about);
  gtk_container_add (GTK_CONTAINER (menubar_help_menu), menubar_about);
  gtk_tooltips_set_tip (tooltips, menubar_about, _("About this application"), NULL);

  toolbar_handlebox = gtk_handle_box_new ();
  gtk_widget_ref (toolbar_handlebox);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_handlebox", toolbar_handlebox,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_handlebox);
  gtk_box_pack_start (GTK_BOX (window_vbox), toolbar_handlebox, FALSE, TRUE, 0);

  toolbar = gtk_toolbar_new ();
  gtk_widget_ref (toolbar);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar", toolbar,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar);
  gtk_container_add (GTK_CONTAINER (toolbar_handlebox), toolbar);
  gtk_container_set_border_width (GTK_CONTAINER (toolbar), 1);
  //gtk_toolbar_set_space_size (GTK_TOOLBAR (toolbar), 16);
  //gtk_toolbar_set_space_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_SPACE_LINE);
  //gtk_toolbar_set_button_relief (GTK_TOOLBAR (toolbar), GTK_RELIEF_NONE);

  toolbar_open = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("File"),
                                _("Open a file"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_open);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_open", toolbar_open,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_open);

  toolbar_disc = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Disc"),
                                _("Open Disc"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_disc);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_disc", toolbar_disc,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_disc);

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

  toolbar_sat = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Sat"),
                                _("Open a Satellite Card"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_sat);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_sat", toolbar_sat,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_sat);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  toolbar_back = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Back"),
                                _("Go Backward"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_back);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_back", toolbar_back,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_back);
  gtk_widget_set_sensitive (toolbar_back, FALSE);

  toolbar_stop = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Stop"),
                                _("Stop Stream"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_stop);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_stop", toolbar_stop,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_stop);

  toolbar_eject = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Eject"),
                                NULL, NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_eject);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_eject", toolbar_eject,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_eject);

  toolbar_play = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Play"),
                                _("Play Stream"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_play);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_play", toolbar_play,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_play);

  toolbar_pause = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Pause"),
                                _("Pause Stream"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_pause);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_pause", toolbar_pause,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_pause);
  gtk_widget_set_sensitive (toolbar_pause, FALSE);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  toolbar_slow = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Slow"),
                                _("Play Slower"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_slow);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_slow", toolbar_slow,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_slow);
  gtk_widget_set_sensitive (toolbar_slow, FALSE);

  toolbar_fast = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Fast"),
                                _("Play Faster"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_fast);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_fast", toolbar_fast,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_fast);
  gtk_widget_set_sensitive (toolbar_fast, FALSE);

  toolbar_playlist = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Playlist"),
                                _("Open Playlist"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_playlist);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_playlist", toolbar_playlist,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_playlist);

  toolbar_prev = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Prev"),
                                _("Previous File"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_prev);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_prev", toolbar_prev,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_prev);

  toolbar_next = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Next"),
                                _("Next File"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_next);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_next", toolbar_next,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_next);

  slider_frame = gtk_frame_new ("-:--:--");
  gtk_widget_ref (slider_frame);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "slider_frame", slider_frame,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_box_pack_start (GTK_BOX (window_vbox), slider_frame, TRUE, TRUE, 0);
  gtk_frame_set_label_align (GTK_FRAME (slider_frame), 0.05, 0.5);

  slider = gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, 100, 1, 6.25, 0)));
  gtk_widget_ref (slider);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "slider", slider,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (slider);
  gtk_container_add (GTK_CONTAINER (slider_frame), slider);
  gtk_widget_set_usize (slider, 500, 15);
  gtk_scale_set_draw_value (GTK_SCALE (slider), FALSE);
  gtk_scale_set_digits (GTK_SCALE (slider), 3);

  file_box = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (file_box);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "file_box", file_box,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (file_box);
  gtk_box_pack_start (GTK_BOX (window_vbox), file_box, TRUE, TRUE, 0);
  gtk_widget_set_usize (file_box, 500, 24);

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
  gtk_box_pack_start (GTK_BOX (window_vbox), dvd_box, TRUE, TRUE, 0);
  gtk_widget_set_usize (dvd_box, 500, 24);

  dvd_label = gtk_label_new (_("Disc"));
  gtk_widget_ref (dvd_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "dvd_label", dvd_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (dvd_label);
  gtk_box_pack_start (GTK_BOX (dvd_box), dvd_label, TRUE, FALSE, 0);

  title_box = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (title_box);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "title_box", title_box,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (title_box);
  gtk_box_pack_start (GTK_BOX (dvd_box), title_box, TRUE, TRUE, 0);

  title = gtk_label_new (_("Title:"));
  gtk_widget_ref (title);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "title", title,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (title);
  gtk_box_pack_start (GTK_BOX (title_box), title, FALSE, FALSE, 5);

  title_label = gtk_label_new ("--");
  gtk_widget_ref (title_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "title_label", title_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (title_label);
  gtk_box_pack_start (GTK_BOX (title_box), title_label, FALSE, FALSE, 5);

  title_prev_button = gtk_button_new_with_label (_("Prev"));
  gtk_widget_ref (title_prev_button);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "title_prev_button", title_prev_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (title_prev_button);
  gtk_box_pack_start (GTK_BOX (title_box), title_prev_button, FALSE, FALSE, 5);
  gtk_button_set_relief (GTK_BUTTON (title_prev_button), GTK_RELIEF_NONE);

  title_next_button = gtk_button_new_with_label (_("Next"));
  gtk_widget_ref (title_next_button);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "title_next_button", title_next_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (title_next_button);
  gtk_box_pack_start (GTK_BOX (title_box), title_next_button, FALSE, FALSE, 5);
  gtk_button_set_relief (GTK_BUTTON (title_next_button), GTK_RELIEF_NONE);

  dvd_separator = gtk_vseparator_new ();
  gtk_widget_ref (dvd_separator);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "dvd_separator", dvd_separator,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (dvd_separator);
  gtk_box_pack_start (GTK_BOX (dvd_box), dvd_separator, TRUE, TRUE, 0);

  chapter_box = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (chapter_box);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "chapter_box", chapter_box,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chapter_box);
  gtk_box_pack_start (GTK_BOX (dvd_box), chapter_box, TRUE, TRUE, 0);

  chapter = gtk_label_new (_("Chapter:"));
  gtk_widget_ref (chapter);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "chapter", chapter,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chapter);
  gtk_box_pack_start (GTK_BOX (chapter_box), chapter, FALSE, FALSE, 5);

  chapter_label = gtk_label_new ("---");
  gtk_widget_ref (chapter_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "chapter_label", chapter_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chapter_label);
  gtk_box_pack_start (GTK_BOX (chapter_box), chapter_label, FALSE, FALSE, 5);

  chapter_prev_button = gtk_button_new_with_label (_("Prev"));
  gtk_widget_ref (chapter_prev_button);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "chapter_prev_button", chapter_prev_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chapter_prev_button);
  gtk_box_pack_start (GTK_BOX (chapter_box), chapter_prev_button, FALSE, FALSE, 5);
  gtk_button_set_relief (GTK_BUTTON (chapter_prev_button), GTK_RELIEF_NONE);

  chapter_next_button = gtk_button_new_with_label (_("Next"));
  gtk_widget_ref (chapter_next_button);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "chapter_next_button", chapter_next_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chapter_next_button);
  gtk_box_pack_start (GTK_BOX (chapter_box), chapter_next_button, FALSE, FALSE, 5);
  gtk_button_set_relief (GTK_BUTTON (chapter_next_button), GTK_RELIEF_NONE);

  network_box = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (network_box);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "network_box", network_box,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_box_pack_start (GTK_BOX (window_vbox), network_box, TRUE, TRUE, 0);
  gtk_widget_set_usize (network_box, 500, 24);

  network_address_label = gtk_label_new (_("No server"));
  gtk_widget_ref (network_address_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "network_address_label", network_address_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_address_label);
  gtk_box_pack_start (GTK_BOX (network_box), network_address_label, TRUE, TRUE, 0);

  intf_statusbar = gtk_statusbar_new ();
  gtk_widget_ref (intf_statusbar);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "intf_statusbar", intf_statusbar,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (intf_statusbar);
  gtk_box_pack_start (GTK_BOX (window_vbox), intf_statusbar, FALSE, FALSE, 0);

  gtk_signal_connect (GTK_OBJECT (intf_window), "drag_data_received",
                      GTK_SIGNAL_FUNC (GtkWindowDrag),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (intf_window), "delete_event",
                      GTK_SIGNAL_FUNC (GtkWindowDelete),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (menubar_open), "activate",
                      GTK_SIGNAL_FUNC (GtkFileOpenShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (menubar_disc), "activate",
                      GTK_SIGNAL_FUNC (GtkDiscOpenShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (menubar_network), "activate",
                      GTK_SIGNAL_FUNC (GtkNetworkOpenShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (menubar_eject), "activate",
                      GTK_SIGNAL_FUNC (GtkDiscEject),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (menubar_close), "activate",
                      GTK_SIGNAL_FUNC (GtkClose),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (menubar_exit), "activate",
                      GTK_SIGNAL_FUNC (GtkExit),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (menubar_interface_hide), "activate",
                      GTK_SIGNAL_FUNC (GtkWindowToggle),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (menubar_playlist), "activate",
                      GTK_SIGNAL_FUNC (GtkPlaylistShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (menubar_modules), "activate",
                      GTK_SIGNAL_FUNC (GtkModulesShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (menubar_messages), "activate",
                      GTK_SIGNAL_FUNC (GtkMessagesShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (menubar_preferences), "activate",
                      GTK_SIGNAL_FUNC (GtkPreferencesShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (menubar_volume_up), "activate",
                      GTK_SIGNAL_FUNC (GtkVolumeUp),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (menubar_volume_down), "activate",
                      GTK_SIGNAL_FUNC (GtkVolumeDown),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (menubar_volume_mute), "activate",
                      GTK_SIGNAL_FUNC (GtkVolumeMute),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (menubar_fullscreen), "activate",
                      GTK_SIGNAL_FUNC (GtkFullscreen),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (menubar_about), "activate",
                      GTK_SIGNAL_FUNC (GtkAboutShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_open), "clicked",
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
  gtk_signal_connect (GTK_OBJECT (slider), "button_release_event",
                      GTK_SIGNAL_FUNC (GtkSliderRelease),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (slider), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkSliderPress),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (title_prev_button), "clicked",
                      GTK_SIGNAL_FUNC (GtkTitlePrev),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (title_next_button), "clicked",
                      GTK_SIGNAL_FUNC (GtkTitleNext),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (chapter_prev_button), "clicked",
                      GTK_SIGNAL_FUNC (GtkChapterPrev),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (chapter_next_button), "clicked",
                      GTK_SIGNAL_FUNC (GtkChapterNext),
                      NULL);

  gtk_object_set_data (GTK_OBJECT (intf_window), "tooltips", tooltips);

  gtk_window_add_accel_group (GTK_WINDOW (intf_window), accel_group);

  return intf_window;
}

GtkWidget*
create_intf_popup (void)
{
  GtkWidget *intf_popup;
  GtkAccelGroup *intf_popup_accels;
  guint tmp_key;
  GtkWidget *popup_play;
  GtkWidget *popup_pause;
  GtkWidget *popup_stop;
  GtkWidget *popup_back;
  GtkWidget *popup_slow;
  GtkWidget *popup_fast;
  GtkWidget *separator16;
  GtkWidget *popup_next;
  GtkWidget *popup_prev;
  GtkWidget *popup_jump;
  GtkWidget *separator27;
  GtkWidget *popup_program;
  GtkWidget *popup_navigation;
  GtkWidget *popup_audio;
  GtkWidget *popup_audio_menu;
  GtkAccelGroup *popup_audio_menu_accels;
  GtkWidget *popup_language;
  GtkWidget *separator19;
  GtkWidget *popup_volume_up;
  GtkWidget *popup_volume_down;
  GtkWidget *popup_volume_mute;
  GtkWidget *separator20;
  GtkWidget *popup_audio_channels;
  GtkWidget *popup_audio_device;
  GtkWidget *popup_video;
  GtkWidget *popup_video_menu;
  GtkAccelGroup *popup_video_menu_accels;
  GtkWidget *popup_subpictures;
  GtkWidget *separator21;
  GtkWidget *popup_fullscreen;
  GtkWidget *separator23;
  GtkWidget *popup_deinterlace;
  GtkWidget *popup_video_device;
  GtkWidget *separator26;
  GtkWidget *popup_open;
  GtkWidget *popup_open_menu;
  GtkAccelGroup *popup_open_menu_accels;
  GtkWidget *popup_file;
  GtkWidget *popup_disc;
  GtkWidget *popup_network;
  GtkWidget *separator12;
  GtkWidget *popup_about;
  GtkWidget *popup_interface_toggle;
  GtkWidget *popup_playlist;
  GtkWidget *popup_preferences;
  GtkWidget *separator9;
  GtkWidget *popup_exit;
  GtkTooltips *tooltips;

  tooltips = gtk_tooltips_new ();

  intf_popup = gtk_menu_new ();
  gtk_object_set_data (GTK_OBJECT (intf_popup), "intf_popup", intf_popup);
  intf_popup_accels = gtk_menu_get_accel_group (GTK_MENU (intf_popup));

  popup_play = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (popup_play)->child),
                                   _("_Play"));
  gtk_widget_add_accelerator (popup_play, "activate_item", intf_popup_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (popup_play);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_play", popup_play,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_play);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_play);

  popup_pause = gtk_menu_item_new_with_label (_("Pause"));
  gtk_widget_ref (popup_pause);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_pause", popup_pause,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_pause);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_pause);
  gtk_widget_set_sensitive (popup_pause, FALSE);

  popup_stop = gtk_menu_item_new_with_label (_("Stop"));
  gtk_widget_ref (popup_stop);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_stop", popup_stop,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_stop);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_stop);

  popup_back = gtk_menu_item_new_with_label (_("Back"));
  gtk_widget_ref (popup_back);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_back", popup_back,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_back);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_back);
  gtk_widget_set_sensitive (popup_back, FALSE);

  popup_slow = gtk_menu_item_new_with_label (_("Slow"));
  gtk_widget_ref (popup_slow);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_slow", popup_slow,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_slow);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_slow);
  gtk_widget_set_sensitive (popup_slow, FALSE);

  popup_fast = gtk_menu_item_new_with_label (_("Fast"));
  gtk_widget_ref (popup_fast);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_fast", popup_fast,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_fast);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_fast);
  gtk_widget_set_sensitive (popup_fast, FALSE);

  separator16 = gtk_menu_item_new ();
  gtk_widget_ref (separator16);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator16", separator16,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator16);
  gtk_container_add (GTK_CONTAINER (intf_popup), separator16);
  gtk_widget_set_sensitive (separator16, FALSE);

  popup_next = gtk_menu_item_new_with_label (_("Next"));
  gtk_widget_ref (popup_next);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_next", popup_next,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_next);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_next);

  popup_prev = gtk_menu_item_new_with_label (_("Prev"));
  gtk_widget_ref (popup_prev);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_prev", popup_prev,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_prev);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_prev);

  popup_jump = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (popup_jump)->child),
                                   _("_Jump..."));
  gtk_widget_add_accelerator (popup_jump, "activate_item", intf_popup_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (popup_jump);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_jump", popup_jump,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_jump);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_jump);

  separator27 = gtk_menu_item_new ();
  gtk_widget_ref (separator27);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator27", separator27,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator27);
  gtk_container_add (GTK_CONTAINER (intf_popup), separator27);
  gtk_widget_set_sensitive (separator27, FALSE);

  popup_program = gtk_menu_item_new_with_label (_("Program"));
  gtk_widget_ref (popup_program);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_program", popup_program,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_program);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_program);
  gtk_widget_set_sensitive (popup_program, FALSE);

  popup_navigation = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (popup_navigation)->child),
                                   _("_Navigation"));
  gtk_widget_add_accelerator (popup_navigation, "activate_item", intf_popup_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (popup_navigation);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_navigation", popup_navigation,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_navigation);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_navigation);
  gtk_widget_set_sensitive (popup_navigation, FALSE);

  popup_audio = gtk_menu_item_new_with_label (_("Audio"));
  gtk_widget_ref (popup_audio);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_audio", popup_audio,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_audio);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_audio);

  popup_audio_menu = gtk_menu_new ();
  gtk_widget_ref (popup_audio_menu);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_audio_menu", popup_audio_menu,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (popup_audio), popup_audio_menu);
  popup_audio_menu_accels = gtk_menu_get_accel_group (GTK_MENU (popup_audio_menu));

  popup_language = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (popup_language)->child),
                                   _("_Language"));
  gtk_widget_add_accelerator (popup_language, "activate_item", popup_audio_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (popup_language);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_language", popup_language,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_language);
  gtk_container_add (GTK_CONTAINER (popup_audio_menu), popup_language);
  gtk_widget_set_sensitive (popup_language, FALSE);

  separator19 = gtk_menu_item_new ();
  gtk_widget_ref (separator19);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator19", separator19,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator19);
  gtk_container_add (GTK_CONTAINER (popup_audio_menu), separator19);
  gtk_widget_set_sensitive (separator19, FALSE);

  popup_volume_up = gtk_menu_item_new_with_label (_("Volume Up"));
  gtk_widget_ref (popup_volume_up);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_volume_up", popup_volume_up,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_volume_up);
  gtk_container_add (GTK_CONTAINER (popup_audio_menu), popup_volume_up);

  popup_volume_down = gtk_menu_item_new_with_label (_("Volume Down"));
  gtk_widget_ref (popup_volume_down);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_volume_down", popup_volume_down,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_volume_down);
  gtk_container_add (GTK_CONTAINER (popup_audio_menu), popup_volume_down);

  popup_volume_mute = gtk_menu_item_new_with_label (_("Mute"));
  gtk_widget_ref (popup_volume_mute);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_volume_mute", popup_volume_mute,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_volume_mute);
  gtk_container_add (GTK_CONTAINER (popup_audio_menu), popup_volume_mute);

  separator20 = gtk_menu_item_new ();
  gtk_widget_ref (separator20);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator20", separator20,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator20);
  gtk_container_add (GTK_CONTAINER (popup_audio_menu), separator20);
  gtk_widget_set_sensitive (separator20, FALSE);

  popup_audio_channels = gtk_menu_item_new_with_label (_("Channels"));
  gtk_widget_ref (popup_audio_channels);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_audio_channels", popup_audio_channels,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_audio_channels);
  gtk_container_add (GTK_CONTAINER (popup_audio_menu), popup_audio_channels);

  popup_audio_device = gtk_menu_item_new_with_label (_("Device"));
  gtk_widget_ref (popup_audio_device);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_audio_device", popup_audio_device,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_audio_device);
  gtk_container_add (GTK_CONTAINER (popup_audio_menu), popup_audio_device);

  popup_video = gtk_menu_item_new_with_label (_("Video"));
  gtk_widget_ref (popup_video);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_video", popup_video,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_video);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_video);

  popup_video_menu = gtk_menu_new ();
  gtk_widget_ref (popup_video_menu);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_video_menu", popup_video_menu,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (popup_video), popup_video_menu);
  popup_video_menu_accels = gtk_menu_get_accel_group (GTK_MENU (popup_video_menu));

  popup_subpictures = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (popup_subpictures)->child),
                                   _("_Subtitles"));
  gtk_widget_add_accelerator (popup_subpictures, "activate_item", popup_video_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (popup_subpictures);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_subpictures", popup_subpictures,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_subpictures);
  gtk_container_add (GTK_CONTAINER (popup_video_menu), popup_subpictures);
  gtk_widget_set_sensitive (popup_subpictures, FALSE);

  separator21 = gtk_menu_item_new ();
  gtk_widget_ref (separator21);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator21", separator21,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator21);
  gtk_container_add (GTK_CONTAINER (popup_video_menu), separator21);
  gtk_widget_set_sensitive (separator21, FALSE);

  popup_fullscreen = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (popup_fullscreen)->child),
                                   _("_Fullscreen"));
  gtk_widget_add_accelerator (popup_fullscreen, "activate_item", popup_video_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (popup_fullscreen);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_fullscreen", popup_fullscreen,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_fullscreen);
  gtk_container_add (GTK_CONTAINER (popup_video_menu), popup_fullscreen);

  separator23 = gtk_menu_item_new ();
  gtk_widget_ref (separator23);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator23", separator23,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator23);
  gtk_container_add (GTK_CONTAINER (popup_video_menu), separator23);
  gtk_widget_set_sensitive (separator23, FALSE);

  popup_deinterlace = gtk_menu_item_new_with_label (_("Deinterlace"));
  gtk_widget_ref (popup_deinterlace);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_deinterlace", popup_deinterlace,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_deinterlace);
  gtk_container_add (GTK_CONTAINER (popup_video_menu), popup_deinterlace);

  popup_video_device = gtk_menu_item_new_with_label (_("Screen"));
  gtk_widget_ref (popup_video_device);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_video_device", popup_video_device,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_video_device);
  gtk_container_add (GTK_CONTAINER (popup_video_menu), popup_video_device);

  separator26 = gtk_menu_item_new ();
  gtk_widget_ref (separator26);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator26", separator26,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator26);
  gtk_container_add (GTK_CONTAINER (intf_popup), separator26);
  gtk_widget_set_sensitive (separator26, FALSE);

  popup_open = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (popup_open)->child),
                                   _("_File"));
  gtk_widget_add_accelerator (popup_open, "activate_item", intf_popup_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (popup_open);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_open", popup_open,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_open);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_open);

  popup_open_menu = gtk_menu_new ();
  gtk_widget_ref (popup_open_menu);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_open_menu", popup_open_menu,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (popup_open), popup_open_menu);
  popup_open_menu_accels = gtk_menu_get_accel_group (GTK_MENU (popup_open_menu));

  popup_file = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (popup_file)->child),
                                   _("_Open File..."));
  gtk_widget_add_accelerator (popup_file, "activate_item", popup_open_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (popup_file);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_file", popup_file,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_file);
  gtk_container_add (GTK_CONTAINER (popup_open_menu), popup_file);
  gtk_tooltips_set_tip (tooltips, popup_file, _("Open a file"), NULL);

  popup_disc = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (popup_disc)->child),
                                   _("Open _Disc..."));
  gtk_widget_add_accelerator (popup_disc, "activate_item", popup_open_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (popup_disc);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_disc", popup_disc,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_disc);
  gtk_container_add (GTK_CONTAINER (popup_open_menu), popup_disc);
  gtk_tooltips_set_tip (tooltips, popup_disc, _("Open Disc Media"), NULL);

  popup_network = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (popup_network)->child),
                                   _("_Network Stream..."));
  gtk_widget_add_accelerator (popup_network, "activate_item", popup_open_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (popup_network);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_network", popup_network,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_network);
  gtk_container_add (GTK_CONTAINER (popup_open_menu), popup_network);
  gtk_tooltips_set_tip (tooltips, popup_network, _("Select a network stream"), NULL);

  separator12 = gtk_menu_item_new ();
  gtk_widget_ref (separator12);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator12", separator12,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator12);
  gtk_container_add (GTK_CONTAINER (popup_open_menu), separator12);
  gtk_widget_set_sensitive (separator12, FALSE);

  popup_about = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (popup_about)->child),
                                   _("_About..."));
  gtk_widget_add_accelerator (popup_about, "activate_item", popup_open_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (popup_about);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_about", popup_about,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_about);
  gtk_container_add (GTK_CONTAINER (popup_open_menu), popup_about);

  popup_interface_toggle = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (popup_interface_toggle)->child),
                                   _("Toggle _Interface"));
  gtk_widget_add_accelerator (popup_interface_toggle, "activate_item", intf_popup_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (popup_interface_toggle);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_interface_toggle", popup_interface_toggle,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_interface_toggle);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_interface_toggle);

  popup_playlist = gtk_menu_item_new_with_label (_("Playlist..."));
  gtk_widget_ref (popup_playlist);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_playlist", popup_playlist,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_playlist);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_playlist);

  popup_preferences = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (popup_preferences)->child),
                                   _("_Preferences..."));
  gtk_widget_add_accelerator (popup_preferences, "activate_item", intf_popup_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (popup_preferences);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_preferences", popup_preferences,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_preferences);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_preferences);

  separator9 = gtk_menu_item_new ();
  gtk_widget_ref (separator9);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator9", separator9,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator9);
  gtk_container_add (GTK_CONTAINER (intf_popup), separator9);
  gtk_widget_set_sensitive (separator9, FALSE);

  popup_exit = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (popup_exit)->child),
                                   _("E_xit"));
  gtk_widget_add_accelerator (popup_exit, "activate_item", intf_popup_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (popup_exit);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_exit", popup_exit,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_exit);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_exit);

  gtk_signal_connect (GTK_OBJECT (popup_play), "activate",
                      GTK_SIGNAL_FUNC (GtkControlPlay),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (popup_pause), "activate",
                      GTK_SIGNAL_FUNC (GtkControlPause),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (popup_stop), "activate",
                      GTK_SIGNAL_FUNC (GtkControlStop),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (popup_back), "activate",
                      GTK_SIGNAL_FUNC (GtkControlBack),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (popup_slow), "activate",
                      GTK_SIGNAL_FUNC (GtkControlSlow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (popup_fast), "activate",
                      GTK_SIGNAL_FUNC (GtkControlFast),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (popup_next), "activate",
                      GTK_SIGNAL_FUNC (GtkPlaylistNext),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (popup_prev), "activate",
                      GTK_SIGNAL_FUNC (GtkPlaylistPrev),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (popup_jump), "activate",
                      GTK_SIGNAL_FUNC (GtkJumpShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (popup_volume_up), "activate",
                      GTK_SIGNAL_FUNC (GtkVolumeUp),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (popup_volume_down), "activate",
                      GTK_SIGNAL_FUNC (GtkVolumeDown),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (popup_volume_mute), "activate",
                      GTK_SIGNAL_FUNC (GtkVolumeMute),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (popup_fullscreen), "activate",
                      GTK_SIGNAL_FUNC (GtkFullscreen),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (popup_file), "activate",
                      GTK_SIGNAL_FUNC (GtkFileOpenShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (popup_disc), "activate",
                      GTK_SIGNAL_FUNC (GtkDiscOpenShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (popup_network), "activate",
                      GTK_SIGNAL_FUNC (GtkNetworkOpenShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (popup_about), "activate",
                      GTK_SIGNAL_FUNC (GtkAboutShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (popup_interface_toggle), "activate",
                      GTK_SIGNAL_FUNC (GtkWindowToggle),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (popup_playlist), "activate",
                      GTK_SIGNAL_FUNC (GtkPlaylistShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (popup_preferences), "activate",
                      GTK_SIGNAL_FUNC (GtkPreferencesShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (popup_exit), "activate",
                      GTK_SIGNAL_FUNC (GtkExit),
                      NULL);

  gtk_object_set_data (GTK_OBJECT (intf_popup), "tooltips", tooltips);

  return intf_popup;
}

GtkWidget*
create_intf_about (void)
{
  GtkWidget *intf_about;
  GtkWidget *dialog_vbox1;
  GtkWidget *vbox3;
  GtkWidget *label14;
  GtkWidget *label18;
  GtkWidget *frame1;
  GtkWidget *vbox17;
  GtkWidget *label16;
  GtkWidget *label39;
  GtkWidget *label17;
  GtkWidget *dialog_action_area;
  GtkWidget *about_ok;

  intf_about = gtk_dialog_new ();
  gtk_object_set_data (GTK_OBJECT (intf_about), "intf_about", intf_about);
  gtk_container_set_border_width (GTK_CONTAINER (intf_about), 5);
  gtk_window_set_title (GTK_WINDOW (intf_about), _("About"));
  gtk_window_set_position (GTK_WINDOW (intf_about), GTK_WIN_POS_CENTER);
  gtk_window_set_policy (GTK_WINDOW (intf_about), FALSE, FALSE, FALSE);

  dialog_vbox1 = GTK_DIALOG (intf_about)->vbox;
  gtk_object_set_data (GTK_OBJECT (intf_about), "dialog_vbox1", dialog_vbox1);
  gtk_widget_show (dialog_vbox1);

  vbox3 = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (vbox3);
  gtk_object_set_data_full (GTK_OBJECT (intf_about), "vbox3", vbox3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox3);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox3, TRUE, TRUE, 0);

  label14 = gtk_label_new (_("VLC media player"));
  gtk_widget_ref (label14);
  gtk_object_set_data_full (GTK_OBJECT (intf_about), "label14", label14,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label14);
  gtk_box_pack_start (GTK_BOX (vbox3), label14, TRUE, TRUE, 0);
  gtk_misc_set_padding (GTK_MISC (label14), 0, 10);

  label18 = gtk_label_new (_("(c) 1996-2004 the VideoLAN team"));
  gtk_widget_ref (label18);
  gtk_object_set_data_full (GTK_OBJECT (intf_about), "label18", label18,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label18);
  gtk_box_pack_start (GTK_BOX (vbox3), label18, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label18), GTK_JUSTIFY_LEFT);
  gtk_misc_set_padding (GTK_MISC (label18), 0, 5);

  frame1 = gtk_frame_new (_("Authors"));
  gtk_widget_ref (frame1);
  gtk_object_set_data_full (GTK_OBJECT (intf_about), "frame1", frame1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame1);
  gtk_box_pack_start (GTK_BOX (vbox3), frame1, FALSE, FALSE, 0);

  vbox17 = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (vbox17);
  gtk_object_set_data_full (GTK_OBJECT (intf_about), "vbox17", vbox17,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox17);
  gtk_container_add (GTK_CONTAINER (frame1), vbox17);

  label16 = gtk_label_new (_("the VideoLAN team <videolan@videolan.org>"));
  gtk_widget_ref (label16);
  gtk_object_set_data_full (GTK_OBJECT (intf_about), "label16", label16,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label16);
  gtk_box_pack_start (GTK_BOX (vbox17), label16, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label16), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label16), 0.5, 0);
  gtk_misc_set_padding (GTK_MISC (label16), 5, 0);

  label39 = gtk_label_new ("http://www.videolan.org/");
  gtk_widget_ref (label39);
  gtk_object_set_data_full (GTK_OBJECT (intf_about), "label39", label39,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label39);
  gtk_box_pack_start (GTK_BOX (vbox17), label39, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label39), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label39), 0.5, 0);
  gtk_misc_set_padding (GTK_MISC (label39), 5, 0);

  label17 = gtk_label_new (_("This is the VLC media player, a DVD, MPEG and DivX player. It can play MPEG and MPEG2 files from a file or from a network source."));
  gtk_widget_ref (label17);
  gtk_object_set_data_full (GTK_OBJECT (intf_about), "label17", label17,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label17);
  gtk_box_pack_start (GTK_BOX (vbox3), label17, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label17), GTK_JUSTIFY_LEFT);
  gtk_label_set_line_wrap (GTK_LABEL (label17), TRUE);
  gtk_misc_set_padding (GTK_MISC (label17), 0, 5);

  dialog_action_area = GTK_DIALOG (intf_about)->action_area;
  gtk_object_set_data (GTK_OBJECT (intf_about), "dialog_action_area", dialog_action_area);
  gtk_widget_show (dialog_action_area);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area), 10);

  about_ok = gtk_button_new_with_label (_("OK"));
  gtk_widget_ref (about_ok);
  gtk_object_set_data_full (GTK_OBJECT (intf_about), "about_ok", about_ok,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (about_ok);
  gtk_box_pack_start (GTK_BOX (dialog_action_area), about_ok, FALSE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS (about_ok, GTK_CAN_DEFAULT);

  gtk_signal_connect (GTK_OBJECT (about_ok), "clicked",
                      GTK_SIGNAL_FUNC (GtkAboutOk),
                      NULL);

  gtk_widget_grab_default (about_ok);
  return intf_about;
}

GtkWidget*
create_intf_open (void)
{
  GtkWidget *intf_open;
  GtkWidget *dialog_vbox7;
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
  GtkWidget *disc_chapter_label;
  GtkWidget *disc_title_label;
  GtkWidget *disc_name;
  GtkObject *disc_title_adj;
  GtkWidget *disc_title;
  GtkObject *disc_chapter_adj;
  GtkWidget *disc_chapter;
  GtkWidget *disc_dvd_use_menu;
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
  GtkWidget *open_browse_subtitle;
  GtkWidget *label37;
  GtkObject *subtitle_delay_adj;
  GtkWidget *subtitle_delay;
  GtkWidget *label3;
  GtkObject *subtitle_fps_adj;
  GtkWidget *subtitle_fps;
  GtkWidget *hbox30;
  GtkWidget *show_sout_settings;
  GtkWidget *sout_settings;
  GtkWidget *dialog_action_area6;
  GtkWidget *hbox18;
  GtkWidget *hbox19;
  GtkWidget *open_ok;
  GtkWidget *open_cancel;
  GtkAccelGroup *accel_group;
  GtkTooltips *tooltips;

  tooltips = gtk_tooltips_new ();

  accel_group = gtk_accel_group_new ();

  intf_open = gtk_dialog_new ();
  gtk_object_set_data (GTK_OBJECT (intf_open), "intf_open", intf_open);
  gtk_container_set_border_width (GTK_CONTAINER (intf_open), 5);
  gtk_window_set_title (GTK_WINDOW (intf_open), _("Open Target"));
  gtk_window_set_policy (GTK_WINDOW (intf_open), FALSE, TRUE, TRUE);

  dialog_vbox7 = GTK_DIALOG (intf_open)->vbox;
  gtk_object_set_data (GTK_OBJECT (intf_open), "dialog_vbox7", dialog_vbox7);
  gtk_widget_show (dialog_vbox7);

  open_vbox = gtk_vbox_new (FALSE, 5);
  gtk_widget_ref (open_vbox);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "open_vbox", open_vbox,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (open_vbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox7), open_vbox, TRUE, TRUE, 0);

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
  gtk_misc_set_alignment (GTK_MISC (label19), 0, 0.5);

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

  disc_name = gtk_entry_new ();
  gtk_widget_ref (disc_name);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "disc_name", disc_name,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_name);
  gtk_table_attach (GTK_TABLE (table5), disc_name, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_entry_set_text (GTK_ENTRY (disc_name), "/dev/dvd");

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

  disc_dvd_use_menu = gtk_check_button_new_with_label (_("Use DVD menus"));
  gtk_widget_ref (disc_dvd_use_menu);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "disc_dvd_use_menu", disc_dvd_use_menu,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_dvd_use_menu);
  gtk_table_attach (GTK_TABLE (table5), disc_dvd_use_menu, 1, 2, 2, 3,
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

  network_udp = gtk_radio_button_new_with_label (table4_group, _("UDP/RTP"));
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

  network_http = gtk_radio_button_new_with_label (table4_group, _("HTTP/FTP/MMS"));
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

  sat_freq_adj = gtk_adjustment_new (1.1954e+07, 1e+07, 1.2999e+07, 1, 10, 10);
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

  sat_srate_adj = gtk_adjustment_new (2.75e+07, 1e+06, 3e+07, 1, 10, 10);
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
  gtk_box_pack_start (GTK_BOX (open_vbox), show_subtitle, FALSE, FALSE, 0);
  gtk_tooltips_set_tip (tooltips, show_subtitle, _("Use a subtitles file"), NULL);

  hbox_subtitle = gtk_hbox_new (FALSE, 5);
  gtk_widget_ref (hbox_subtitle);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "hbox_subtitle", hbox_subtitle,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox_subtitle);
  gtk_box_pack_start (GTK_BOX (open_vbox), hbox_subtitle, TRUE, TRUE, 0);
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
  gtk_tooltips_set_tip (tooltips, entry_subtitle, _("Select a subtitles file"), NULL);

  vbox14 = gtk_vbox_new (TRUE, 0);
  gtk_widget_ref (vbox14);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "vbox14", vbox14,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox14);
  gtk_box_pack_start (GTK_BOX (hbox_subtitle), vbox14, FALSE, FALSE, 0);

  open_browse_subtitle = gtk_button_new_with_label (_("Browse..."));
  gtk_widget_ref (open_browse_subtitle);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "open_browse_subtitle", open_browse_subtitle,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (open_browse_subtitle);
  gtk_box_pack_start (GTK_BOX (vbox14), open_browse_subtitle, FALSE, FALSE, 0);

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
  gtk_tooltips_set_tip (tooltips, subtitle_delay, _("Set the delay (in seconds)"), NULL);

  label3 = gtk_label_new (_("fps"));
  gtk_widget_ref (label3);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "label3", label3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label3);
  gtk_box_pack_start (GTK_BOX (hbox_subtitle), label3, TRUE, TRUE, 0);

  subtitle_fps_adj = gtk_adjustment_new (0, 0, 100, 0.1, 10, 10);
  subtitle_fps = gtk_spin_button_new (GTK_ADJUSTMENT (subtitle_fps_adj), 1, 1);
  gtk_widget_ref (subtitle_fps);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "subtitle_fps", subtitle_fps,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (subtitle_fps);
  gtk_box_pack_start (GTK_BOX (hbox_subtitle), subtitle_fps, TRUE, TRUE, 0);
  gtk_tooltips_set_tip (tooltips, subtitle_fps, _("Set the number of Frames Per Second"), NULL);

  hbox30 = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (hbox30);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "hbox30", hbox30,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox30);
  gtk_box_pack_start (GTK_BOX (dialog_vbox7), hbox30, FALSE, FALSE, 3);

  show_sout_settings = gtk_check_button_new_with_label (_("Stream output"));
  gtk_widget_ref (show_sout_settings);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "show_sout_settings", show_sout_settings,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (show_sout_settings);
  gtk_box_pack_start (GTK_BOX (hbox30), show_sout_settings, FALSE, FALSE, 0);
  gtk_tooltips_set_tip (tooltips, show_sout_settings, _("Use stream output"), NULL);

  sout_settings = gtk_button_new_with_label (_("Settings..."));
  gtk_widget_ref (sout_settings);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "sout_settings", sout_settings,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_settings);
  gtk_box_pack_start (GTK_BOX (hbox30), sout_settings, FALSE, FALSE, 20);
  gtk_tooltips_set_tip (tooltips, sout_settings, _("Stream output configuration "), NULL);

  dialog_action_area6 = GTK_DIALOG (intf_open)->action_area;
  gtk_object_set_data (GTK_OBJECT (intf_open), "dialog_action_area6", dialog_action_area6);
  gtk_widget_show (dialog_action_area6);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area6), 5);

  hbox18 = gtk_hbox_new (TRUE, 5);
  gtk_widget_ref (hbox18);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "hbox18", hbox18,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox18);
  gtk_box_pack_start (GTK_BOX (dialog_action_area6), hbox18, TRUE, TRUE, 0);

  hbox19 = gtk_hbox_new (TRUE, 5);
  gtk_widget_ref (hbox19);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "hbox19", hbox19,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox19);
  gtk_box_pack_end (GTK_BOX (hbox18), hbox19, FALSE, TRUE, 0);

  open_ok = gtk_button_new_with_label (_("OK"));
  gtk_widget_ref (open_ok);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "open_ok", open_ok,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (open_ok);
  gtk_box_pack_start (GTK_BOX (hbox19), open_ok, FALSE, TRUE, 0);

  open_cancel = gtk_button_new_with_label (_("Cancel"));
  gtk_widget_ref (open_cancel);
  gtk_object_set_data_full (GTK_OBJECT (intf_open), "open_cancel", open_cancel,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (open_cancel);
  gtk_box_pack_start (GTK_BOX (hbox19), open_cancel, FALSE, TRUE, 0);
  gtk_widget_add_accelerator (open_cancel, "clicked", accel_group,
                              GDK_Escape, 0,
                              GTK_ACCEL_VISIBLE);

  gtk_signal_connect (GTK_OBJECT (intf_open), "delete_event",
                      GTK_SIGNAL_FUNC (gtk_widget_hide),
                      NULL);
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
  gtk_signal_connect (GTK_OBJECT (disc_title), "changed",
                      GTK_SIGNAL_FUNC (GtkOpenChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (disc_chapter), "changed",
                      GTK_SIGNAL_FUNC (GtkOpenChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (disc_dvd_use_menu), "toggled",
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
  gtk_signal_connect (GTK_OBJECT (open_browse_subtitle), "clicked",
                      GTK_SIGNAL_FUNC (GtkFileShow),
                      "entry_subtitle");
  gtk_signal_connect (GTK_OBJECT (subtitle_delay), "changed",
                      GTK_SIGNAL_FUNC (GtkOpenChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (show_sout_settings), "clicked",
                      GTK_SIGNAL_FUNC (GtkOpenSoutShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (sout_settings), "clicked",
                      GTK_SIGNAL_FUNC (GtkSoutSettings),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (open_ok), "clicked",
                      GTK_SIGNAL_FUNC (GtkOpenOk),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (open_cancel), "clicked",
                      GTK_SIGNAL_FUNC (GtkOpenCancel),
                      NULL);

  gtk_object_set_data (GTK_OBJECT (intf_open), "tooltips", tooltips);

  gtk_window_add_accel_group (GTK_WINDOW (intf_open), accel_group);

  return intf_open;
}

GtkWidget*
create_intf_file (void)
{
  GtkWidget *intf_file;
  GtkWidget *file_ok;
  GtkWidget *file_cancel;

  intf_file = gtk_file_selection_new (_("Select File"));
  gtk_object_set_data (GTK_OBJECT (intf_file), "intf_file", intf_file);
  gtk_container_set_border_width (GTK_CONTAINER (intf_file), 10);
  gtk_window_set_modal (GTK_WINDOW (intf_file), TRUE);

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
create_intf_jump (void)
{
  GtkWidget *intf_jump;
  GtkWidget *dialog_vbox3;
  GtkWidget *jump_frame;
  GtkWidget *hbox13;
  GtkWidget *jump_second_label;
  GtkObject *jump_second_spinbutton_adj;
  GtkWidget *jump_second_spinbutton;
  GtkWidget *jump_minute_label;
  GtkObject *jump_minute_spinbutton_adj;
  GtkWidget *jump_minute_spinbutton;
  GtkWidget *jump_hour_label;
  GtkObject *jump_hour_spinbutton_adj;
  GtkWidget *jump_hour_spinbutton;
  GtkWidget *dialog_action_area2;
  GtkWidget *jump_ok_button;
  GtkWidget *jump_cancel_button;

  intf_jump = gtk_dialog_new ();
  gtk_object_set_data (GTK_OBJECT (intf_jump), "intf_jump", intf_jump);
  gtk_window_set_title (GTK_WINDOW (intf_jump), _("Jump"));
  gtk_window_set_policy (GTK_WINDOW (intf_jump), TRUE, TRUE, FALSE);

  dialog_vbox3 = GTK_DIALOG (intf_jump)->vbox;
  gtk_object_set_data (GTK_OBJECT (intf_jump), "dialog_vbox3", dialog_vbox3);
  gtk_widget_show (dialog_vbox3);

  jump_frame = gtk_frame_new (_("Go To:"));
  gtk_widget_ref (jump_frame);
  gtk_object_set_data_full (GTK_OBJECT (intf_jump), "jump_frame", jump_frame,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (jump_frame);
  gtk_box_pack_start (GTK_BOX (dialog_vbox3), jump_frame, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (jump_frame), 5);
  gtk_frame_set_label_align (GTK_FRAME (jump_frame), 0.05, 0.5);

  hbox13 = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (hbox13);
  gtk_object_set_data_full (GTK_OBJECT (intf_jump), "hbox13", hbox13,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox13);
  gtk_container_add (GTK_CONTAINER (jump_frame), hbox13);

  jump_second_label = gtk_label_new (_("s."));
  gtk_widget_ref (jump_second_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_jump), "jump_second_label", jump_second_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (jump_second_label);
  gtk_box_pack_end (GTK_BOX (hbox13), jump_second_label, FALSE, FALSE, 5);

  jump_second_spinbutton_adj = gtk_adjustment_new (0, 0, 60, 1, 10, 10);
  jump_second_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (jump_second_spinbutton_adj), 1, 0);
  gtk_widget_ref (jump_second_spinbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_jump), "jump_second_spinbutton", jump_second_spinbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (jump_second_spinbutton);
  gtk_box_pack_end (GTK_BOX (hbox13), jump_second_spinbutton, FALSE, TRUE, 0);

  jump_minute_label = gtk_label_new (_("m:"));
  gtk_widget_ref (jump_minute_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_jump), "jump_minute_label", jump_minute_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (jump_minute_label);
  gtk_box_pack_end (GTK_BOX (hbox13), jump_minute_label, FALSE, FALSE, 5);

  jump_minute_spinbutton_adj = gtk_adjustment_new (0, 0, 60, 1, 10, 10);
  jump_minute_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (jump_minute_spinbutton_adj), 1, 0);
  gtk_widget_ref (jump_minute_spinbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_jump), "jump_minute_spinbutton", jump_minute_spinbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (jump_minute_spinbutton);
  gtk_box_pack_end (GTK_BOX (hbox13), jump_minute_spinbutton, FALSE, TRUE, 0);

  jump_hour_label = gtk_label_new (_("h:"));
  gtk_widget_ref (jump_hour_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_jump), "jump_hour_label", jump_hour_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (jump_hour_label);
  gtk_box_pack_end (GTK_BOX (hbox13), jump_hour_label, FALSE, FALSE, 5);

  jump_hour_spinbutton_adj = gtk_adjustment_new (0, 0, 12, 1, 10, 10);
  jump_hour_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (jump_hour_spinbutton_adj), 1, 0);
  gtk_widget_ref (jump_hour_spinbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_jump), "jump_hour_spinbutton", jump_hour_spinbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (jump_hour_spinbutton);
  gtk_box_pack_end (GTK_BOX (hbox13), jump_hour_spinbutton, FALSE, TRUE, 0);

  dialog_action_area2 = GTK_DIALOG (intf_jump)->action_area;
  gtk_object_set_data (GTK_OBJECT (intf_jump), "dialog_action_area2", dialog_action_area2);
  gtk_widget_show (dialog_action_area2);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area2), 10);

  jump_ok_button = gtk_button_new_with_label (_("OK"));
  gtk_widget_ref (jump_ok_button);
  gtk_object_set_data_full (GTK_OBJECT (intf_jump), "jump_ok_button", jump_ok_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (jump_ok_button);
  gtk_box_pack_start (GTK_BOX (dialog_action_area2), jump_ok_button, TRUE, TRUE, 0);

  jump_cancel_button = gtk_button_new_with_label (_("Cancel"));
  gtk_widget_ref (jump_cancel_button);
  gtk_object_set_data_full (GTK_OBJECT (intf_jump), "jump_cancel_button", jump_cancel_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (jump_cancel_button);
  gtk_box_pack_start (GTK_BOX (dialog_action_area2), jump_cancel_button, TRUE, TRUE, 0);

  gtk_signal_connect (GTK_OBJECT (jump_ok_button), "clicked",
                      GTK_SIGNAL_FUNC (GtkJumpOk),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (jump_cancel_button), "clicked",
                      GTK_SIGNAL_FUNC (GtkJumpCancel),
                      NULL);

  return intf_jump;
}

GtkWidget*
create_intf_playlist (void)
{
  GtkWidget *intf_playlist;
  GtkWidget *dialog_vbox4;
  GtkWidget *playlist_menubar;
  GtkWidget *playlist_add;
  GtkWidget *playlist_add_menu;
  GtkAccelGroup *playlist_add_menu_accels;
  GtkWidget *playlist_add_disc;
  GtkWidget *playlist_add_file;
  GtkWidget *playlist_add_network;
  GtkWidget *playlist_add_url;
  GtkWidget *playlist_delete;
  GtkWidget *playlist_delete_menu;
  GtkAccelGroup *playlist_delete_menu_accels;
  GtkWidget *playlist_delete_all;
  GtkWidget *playlist_delete_selected;
  GtkWidget *playlist_selection;
  GtkWidget *playlist_selection_menu;
  GtkAccelGroup *playlist_selection_menu_accels;
  guint tmp_key;
  GtkWidget *playlist_selection_crop;
  GtkWidget *playlist_selection_invert;
  GtkWidget *playlist_selection_select;
  GtkWidget *scrolledwindow1;
  GtkWidget *playlist_clist;
  GtkWidget *label22;
  GtkWidget *label23;
  GtkWidget *dialog_action_area3;
  GtkWidget *playlist_ok_button;
  GtkWidget *playlist_cancel_button;

  intf_playlist = gtk_dialog_new ();
  gtk_object_set_data (GTK_OBJECT (intf_playlist), "intf_playlist", intf_playlist);
  gtk_window_set_title (GTK_WINDOW (intf_playlist), _("Playlist"));
  gtk_window_set_default_size (GTK_WINDOW (intf_playlist), 400, 300);
  gtk_window_set_policy (GTK_WINDOW (intf_playlist), TRUE, TRUE, FALSE);

  dialog_vbox4 = GTK_DIALOG (intf_playlist)->vbox;
  gtk_object_set_data (GTK_OBJECT (intf_playlist), "dialog_vbox4", dialog_vbox4);
  gtk_widget_show (dialog_vbox4);

  playlist_menubar = gtk_menu_bar_new ();
  gtk_widget_ref (playlist_menubar);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_menubar", playlist_menubar,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_menubar);
  gtk_box_pack_start (GTK_BOX (dialog_vbox4), playlist_menubar, FALSE, FALSE, 0);

  playlist_add = gtk_menu_item_new_with_label (_("Add"));
  gtk_widget_ref (playlist_add);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_add", playlist_add,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_add);
  gtk_container_add (GTK_CONTAINER (playlist_menubar), playlist_add);

  playlist_add_menu = gtk_menu_new ();
  gtk_widget_ref (playlist_add_menu);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_add_menu", playlist_add_menu,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (playlist_add), playlist_add_menu);
  playlist_add_menu_accels = gtk_menu_get_accel_group (GTK_MENU (playlist_add_menu));

  playlist_add_disc = gtk_menu_item_new_with_label (_("Disc"));
  gtk_widget_ref (playlist_add_disc);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_add_disc", playlist_add_disc,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_add_disc);
  gtk_container_add (GTK_CONTAINER (playlist_add_menu), playlist_add_disc);

  playlist_add_file = gtk_menu_item_new_with_label (_("File"));
  gtk_widget_ref (playlist_add_file);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_add_file", playlist_add_file,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_add_file);
  gtk_container_add (GTK_CONTAINER (playlist_add_menu), playlist_add_file);

  playlist_add_network = gtk_menu_item_new_with_label (_("Network"));
  gtk_widget_ref (playlist_add_network);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_add_network", playlist_add_network,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_add_network);
  gtk_container_add (GTK_CONTAINER (playlist_add_menu), playlist_add_network);

  playlist_add_url = gtk_menu_item_new_with_label (_("URL"));
  gtk_widget_ref (playlist_add_url);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_add_url", playlist_add_url,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_add_url);
  gtk_container_add (GTK_CONTAINER (playlist_add_menu), playlist_add_url);

  playlist_delete = gtk_menu_item_new_with_label (_("Delete"));
  gtk_widget_ref (playlist_delete);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_delete", playlist_delete,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_delete);
  gtk_container_add (GTK_CONTAINER (playlist_menubar), playlist_delete);

  playlist_delete_menu = gtk_menu_new ();
  gtk_widget_ref (playlist_delete_menu);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_delete_menu", playlist_delete_menu,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (playlist_delete), playlist_delete_menu);
  playlist_delete_menu_accels = gtk_menu_get_accel_group (GTK_MENU (playlist_delete_menu));

  playlist_delete_all = gtk_menu_item_new_with_label (_("All"));
  gtk_widget_ref (playlist_delete_all);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_delete_all", playlist_delete_all,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_delete_all);
  gtk_container_add (GTK_CONTAINER (playlist_delete_menu), playlist_delete_all);

  playlist_delete_selected = gtk_menu_item_new_with_label (_("Selected"));
  gtk_widget_ref (playlist_delete_selected);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_delete_selected", playlist_delete_selected,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_delete_selected);
  gtk_container_add (GTK_CONTAINER (playlist_delete_menu), playlist_delete_selected);

  playlist_selection = gtk_menu_item_new_with_label (_("Selection"));
  gtk_widget_ref (playlist_selection);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_selection", playlist_selection,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_selection);
  gtk_container_add (GTK_CONTAINER (playlist_menubar), playlist_selection);

  playlist_selection_menu = gtk_menu_new ();
  gtk_widget_ref (playlist_selection_menu);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_selection_menu", playlist_selection_menu,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (playlist_selection), playlist_selection_menu);
  playlist_selection_menu_accels = gtk_menu_get_accel_group (GTK_MENU (playlist_selection_menu));

  playlist_selection_crop = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (playlist_selection_crop)->child),
                                   _("_Crop"));
  gtk_widget_add_accelerator (playlist_selection_crop, "activate_item", playlist_selection_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (playlist_selection_crop);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_selection_crop", playlist_selection_crop,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_selection_crop);
  gtk_container_add (GTK_CONTAINER (playlist_selection_menu), playlist_selection_crop);

  playlist_selection_invert = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (playlist_selection_invert)->child),
                                   _("_Invert"));
  gtk_widget_add_accelerator (playlist_selection_invert, "activate_item", playlist_selection_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (playlist_selection_invert);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_selection_invert", playlist_selection_invert,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_selection_invert);
  gtk_container_add (GTK_CONTAINER (playlist_selection_menu), playlist_selection_invert);

  playlist_selection_select = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (playlist_selection_select)->child),
                                   _("_Select"));
  gtk_widget_add_accelerator (playlist_selection_select, "activate_item", playlist_selection_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (playlist_selection_select);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_selection_select", playlist_selection_select,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_selection_select);
  gtk_container_add (GTK_CONTAINER (playlist_selection_menu), playlist_selection_select);

  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow1);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "scrolledwindow1", scrolledwindow1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox4), scrolledwindow1, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow1), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  playlist_clist = gtk_clist_new (2);
  gtk_widget_ref (playlist_clist);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_clist", playlist_clist,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_clist);
  gtk_container_add (GTK_CONTAINER (scrolledwindow1), playlist_clist);
  gtk_container_set_border_width (GTK_CONTAINER (playlist_clist), 5);
  gtk_clist_set_column_width (GTK_CLIST (playlist_clist), 0, 257);
  gtk_clist_set_column_width (GTK_CLIST (playlist_clist), 1, 80);
  gtk_clist_set_selection_mode (GTK_CLIST (playlist_clist), GTK_SELECTION_EXTENDED);
  gtk_clist_column_titles_show (GTK_CLIST (playlist_clist));
  gtk_clist_set_shadow_type (GTK_CLIST (playlist_clist), GTK_SHADOW_OUT);

  label22 = gtk_label_new (_("File"));
  gtk_widget_ref (label22);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "label22", label22,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label22);
  gtk_clist_set_column_widget (GTK_CLIST (playlist_clist), 0, label22);

  label23 = gtk_label_new (_("Duration"));
  gtk_widget_ref (label23);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "label23", label23,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label23);
  gtk_clist_set_column_widget (GTK_CLIST (playlist_clist), 1, label23);

  dialog_action_area3 = GTK_DIALOG (intf_playlist)->action_area;
  gtk_object_set_data (GTK_OBJECT (intf_playlist), "dialog_action_area3", dialog_action_area3);
  gtk_widget_show (dialog_action_area3);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area3), 10);

  playlist_ok_button = gtk_button_new_with_label (_("OK"));
  gtk_widget_ref (playlist_ok_button);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_ok_button", playlist_ok_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_ok_button);
  gtk_box_pack_start (GTK_BOX (dialog_action_area3), playlist_ok_button, TRUE, TRUE, 0);

  playlist_cancel_button = gtk_button_new_with_label (_("Cancel"));
  gtk_widget_ref (playlist_cancel_button);
  gtk_object_set_data_full (GTK_OBJECT (intf_playlist), "playlist_cancel_button", playlist_cancel_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_cancel_button);
  gtk_box_pack_start (GTK_BOX (dialog_action_area3), playlist_cancel_button, TRUE, TRUE, 0);

  gtk_signal_connect (GTK_OBJECT (intf_playlist), "destroy",
                      GTK_SIGNAL_FUNC (gtk_widget_hide),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (intf_playlist), "delete_event",
                      GTK_SIGNAL_FUNC (gtk_widget_hide),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (playlist_add_disc), "activate",
                      GTK_SIGNAL_FUNC (GtkDiscOpenShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (playlist_add_file), "activate",
                      GTK_SIGNAL_FUNC (GtkFileOpenShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (playlist_add_network), "activate",
                      GTK_SIGNAL_FUNC (GtkNetworkOpenShow),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (playlist_add_url), "activate",
                      GTK_SIGNAL_FUNC (GtkPlaylistAddUrl),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (playlist_delete_all), "activate",
                      GTK_SIGNAL_FUNC (GtkPlaylistDeleteAll),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (playlist_delete_selected), "activate",
                      GTK_SIGNAL_FUNC (GtkPlaylistDeleteSelected),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (playlist_selection_crop), "activate",
                      GTK_SIGNAL_FUNC (GtkPlaylistCrop),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (playlist_selection_invert), "activate",
                      GTK_SIGNAL_FUNC (GtkPlaylistInvert),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (playlist_selection_select), "activate",
                      GTK_SIGNAL_FUNC (GtkPlaylistSelect),
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
  gtk_signal_connect (GTK_OBJECT (playlist_ok_button), "clicked",
                      GTK_SIGNAL_FUNC (GtkPlaylistOk),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (playlist_cancel_button), "clicked",
                      GTK_SIGNAL_FUNC (GtkPlaylistCancel),
                      NULL);

  return intf_playlist;
}

GtkWidget*
create_intf_messages (void)
{
  GtkWidget *intf_messages;
  GtkWidget *dialog_vbox6;
  GtkWidget *scrolledwindow2;
  GtkWidget *messages_textbox;
  GtkWidget *dialog_action_area5;
  GtkWidget *messages_ok;

  intf_messages = gtk_dialog_new ();
  gtk_object_set_data (GTK_OBJECT (intf_messages), "intf_messages", intf_messages);
  gtk_window_set_title (GTK_WINDOW (intf_messages), _("Messages"));
  gtk_window_set_default_size (GTK_WINDOW (intf_messages), 600, 400);
  gtk_window_set_policy (GTK_WINDOW (intf_messages), TRUE, TRUE, FALSE);

  dialog_vbox6 = GTK_DIALOG (intf_messages)->vbox;
  gtk_object_set_data (GTK_OBJECT (intf_messages), "dialog_vbox6", dialog_vbox6);
  gtk_widget_show (dialog_vbox6);

  scrolledwindow2 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow2);
  gtk_object_set_data_full (GTK_OBJECT (intf_messages), "scrolledwindow2", scrolledwindow2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow2);
  gtk_box_pack_start (GTK_BOX (dialog_vbox6), scrolledwindow2, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (scrolledwindow2), 5);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow2), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

  messages_textbox = gtk_text_new (NULL, NULL);
  gtk_widget_ref (messages_textbox);
  gtk_object_set_data_full (GTK_OBJECT (intf_messages), "messages_textbox", messages_textbox,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (messages_textbox);
  gtk_container_add (GTK_CONTAINER (scrolledwindow2), messages_textbox);

  dialog_action_area5 = GTK_DIALOG (intf_messages)->action_area;
  gtk_object_set_data (GTK_OBJECT (intf_messages), "dialog_action_area5", dialog_action_area5);
  gtk_widget_show (dialog_action_area5);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area5), 5);

  messages_ok = gtk_button_new_with_label (_("OK"));
  gtk_widget_ref (messages_ok);
  gtk_object_set_data_full (GTK_OBJECT (intf_messages), "messages_ok", messages_ok,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (messages_ok);
  gtk_box_pack_start (GTK_BOX (dialog_action_area5), messages_ok, FALSE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS (messages_ok, GTK_CAN_DEFAULT);

  gtk_signal_connect (GTK_OBJECT (intf_messages), "delete_event",
                      GTK_SIGNAL_FUNC (GtkMessagesDelete),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (messages_ok), "clicked",
                      GTK_SIGNAL_FUNC (GtkMessagesOk),
                      NULL);

  gtk_widget_grab_default (messages_ok);
  return intf_messages;
}

GtkWidget*
create_intf_sout (void)
{
  GtkWidget *intf_sout;
  GtkWidget *vbox15;
  GtkWidget *vbox16;
  GtkWidget *frame11;
  GtkWidget *hbox28;
  GtkWidget *label38;
  GtkWidget *combo4;
  GtkWidget *sout_entry_target;
  GtkWidget *frame12;
  GtkWidget *table6;
  GSList *sout_access_group = NULL;
  GtkWidget *sout_access_file;
  GtkWidget *sout_access_udp;
  GtkWidget *sout_access_rtp;
  GtkWidget *sout_file_path_label;
  GtkWidget *combo6;
  GtkWidget *sout_file_path;
  GtkWidget *sout_udp_address_label;
  GtkWidget *sout_udp_address_combo;
  GtkWidget *sout_udp_address;
  GtkObject *sout_udp_port_adj;
  GtkWidget *sout_udp_port;
  GtkWidget *hbox29;
  GSList *sout_mux_group = NULL;
  GtkWidget *sout_mux_ts;
  GtkWidget *sout_mux_ps;
  GtkWidget *sout_mux_avi;
  GtkWidget *sout_udp_port_label;
  GtkWidget *hbox25;
  GtkWidget *hbox26;
  GtkWidget *button1;
  GtkWidget *button2;

  intf_sout = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_object_set_data (GTK_OBJECT (intf_sout), "intf_sout", intf_sout);
  gtk_container_set_border_width (GTK_CONTAINER (intf_sout), 5);
  gtk_window_set_title (GTK_WINDOW (intf_sout), _("Stream output"));
  gtk_window_set_modal (GTK_WINDOW (intf_sout), TRUE);

  vbox15 = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (vbox15);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "vbox15", vbox15,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox15);
  gtk_container_add (GTK_CONTAINER (intf_sout), vbox15);

  vbox16 = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (vbox16);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "vbox16", vbox16,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox16);
  gtk_box_pack_start (GTK_BOX (vbox15), vbox16, TRUE, TRUE, 0);

  frame11 = gtk_frame_new (_("Stream output (MRL)"));
  gtk_widget_ref (frame11);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "frame11", frame11,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame11);
  gtk_box_pack_start (GTK_BOX (vbox16), frame11, FALSE, TRUE, 0);

  hbox28 = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (hbox28);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "hbox28", hbox28,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox28);
  gtk_container_add (GTK_CONTAINER (frame11), hbox28);
  gtk_container_set_border_width (GTK_CONTAINER (hbox28), 5);

  label38 = gtk_label_new (_("Destination Target: "));
  gtk_widget_ref (label38);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "label38", label38,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label38);
  gtk_box_pack_start (GTK_BOX (hbox28), label38, FALSE, FALSE, 0);

  combo4 = gtk_combo_new ();
  gtk_widget_ref (combo4);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "combo4", combo4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo4);
  gtk_box_pack_start (GTK_BOX (hbox28), combo4, TRUE, TRUE, 0);

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

  sout_access_udp = gtk_radio_button_new_with_label (sout_access_group, "UDP");
  sout_access_group = gtk_radio_button_group (GTK_RADIO_BUTTON (sout_access_udp));
  gtk_widget_ref (sout_access_udp);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_access_udp", sout_access_udp,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_access_udp);
  gtk_table_attach (GTK_TABLE (table6), sout_access_udp, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  sout_access_rtp = gtk_radio_button_new_with_label (sout_access_group, "RTP");
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

  combo6 = gtk_combo_new ();
  gtk_widget_ref (combo6);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "combo6", combo6,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo6);
  gtk_table_attach (GTK_TABLE (table6), combo6, 2, 5, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  sout_file_path = GTK_COMBO (combo6)->entry;
  gtk_widget_ref (sout_file_path);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_file_path", sout_file_path,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_file_path);

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

  sout_udp_port_adj = gtk_adjustment_new (1234, 0, 65535, 1, 10, 10);
  sout_udp_port = gtk_spin_button_new (GTK_ADJUSTMENT (sout_udp_port_adj), 1, 0);
  gtk_widget_ref (sout_udp_port);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_udp_port", sout_udp_port,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_udp_port);
  gtk_table_attach (GTK_TABLE (table6), sout_udp_port, 4, 5, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  hbox29 = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (hbox29);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "hbox29", hbox29,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox29);
  gtk_table_attach (GTK_TABLE (table6), hbox29, 4, 5, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  sout_mux_ts = gtk_radio_button_new_with_label (sout_mux_group, _("TS"));
  sout_mux_group = gtk_radio_button_group (GTK_RADIO_BUTTON (sout_mux_ts));
  gtk_widget_ref (sout_mux_ts);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_mux_ts", sout_mux_ts,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_mux_ts);
  gtk_box_pack_start (GTK_BOX (hbox29), sout_mux_ts, FALSE, FALSE, 0);

  sout_mux_ps = gtk_radio_button_new_with_label (sout_mux_group, _("PS"));
  sout_mux_group = gtk_radio_button_group (GTK_RADIO_BUTTON (sout_mux_ps));
  gtk_widget_ref (sout_mux_ps);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_mux_ps", sout_mux_ps,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_mux_ps);
  gtk_box_pack_start (GTK_BOX (hbox29), sout_mux_ps, FALSE, FALSE, 0);

  sout_mux_avi = gtk_radio_button_new_with_label (sout_mux_group, _("AVI"));
  sout_mux_group = gtk_radio_button_group (GTK_RADIO_BUTTON (sout_mux_avi));
  gtk_widget_ref (sout_mux_avi);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_mux_avi", sout_mux_avi,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_mux_avi);
  gtk_box_pack_start (GTK_BOX (hbox29), sout_mux_avi, FALSE, FALSE, 0);

  sout_udp_port_label = gtk_label_new (_("Port"));
  gtk_widget_ref (sout_udp_port_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "sout_udp_port_label", sout_udp_port_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (sout_udp_port_label);
  gtk_table_attach (GTK_TABLE (table6), sout_udp_port_label, 3, 4, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (sout_udp_port_label), 0, 0.5);

  hbox25 = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (hbox25);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "hbox25", hbox25,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox25);
  gtk_box_pack_end (GTK_BOX (vbox15), hbox25, FALSE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox25), 5);

  hbox26 = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (hbox26);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "hbox26", hbox26,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox26);
  gtk_box_pack_end (GTK_BOX (hbox25), hbox26, TRUE, TRUE, 3);

  button1 = gtk_button_new_with_label (_("OK"));
  gtk_widget_ref (button1);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "button1", button1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (button1);
  gtk_box_pack_start (GTK_BOX (hbox26), button1, TRUE, TRUE, 2);

  button2 = gtk_button_new_with_label (_("Cancel"));
  gtk_widget_ref (button2);
  gtk_object_set_data_full (GTK_OBJECT (intf_sout), "button2", button2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (button2);
  gtk_box_pack_start (GTK_BOX (hbox26), button2, TRUE, TRUE, 2);

  gtk_signal_connect (GTK_OBJECT (sout_access_file), "toggled",
                      GTK_SIGNAL_FUNC (GtkSoutSettingsAccessFile),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (sout_access_udp), "toggled",
                      GTK_SIGNAL_FUNC (GtkSoutSettingsAccessUdp),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (sout_access_rtp), "toggled",
                      GTK_SIGNAL_FUNC (GtkSoutSettingsAccessUdp),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (sout_file_path), "changed",
                      GTK_SIGNAL_FUNC (GtkSoutSettingsChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (sout_udp_address), "changed",
                      GTK_SIGNAL_FUNC (GtkSoutSettingsChanged),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (sout_udp_port), "changed",
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
  gtk_signal_connect (GTK_OBJECT (button1), "clicked",
                      GTK_SIGNAL_FUNC (GtkSoutSettingsOk),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (button2), "clicked",
                      GTK_SIGNAL_FUNC (GtkSoutSettingsCancel),
                      NULL);

  return intf_sout;
}

