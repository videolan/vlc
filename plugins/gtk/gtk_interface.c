/* This file was created automatically by glade and fixed by fixfiles.sh */

#include <videolan/vlc.h>

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
  GtkWidget *menubar_exit;
  GtkWidget *menubar_view;
  GtkWidget *menubar_view_menu;
  GtkAccelGroup *menubar_view_menu_accels;
  GtkWidget *menubar_interface_hide;
  GtkWidget *menubar_fullscreen;
  GtkWidget *separator13;
  GtkWidget *menubar_title;
  GtkWidget *menubar_chapter;
  GtkWidget *menubar_angle;
  GtkWidget *separator11;
  GtkWidget *menubar_playlist;
  GtkWidget *menubar_modules;
  GtkWidget *menubar_messages;
  GtkWidget *menubar_settings;
  GtkWidget *menubar_settings_menu;
  GtkAccelGroup *menubar_settings_menu_accels;
  GtkWidget *menubar_audio;
  GtkWidget *menubar_subpictures;
  GtkWidget *separator8;
  GtkWidget *menubar_preferences;
  GtkWidget *menubar_help;
  GtkWidget *menubar_help_menu;
  GtkAccelGroup *menubar_help_menu_accels;
  GtkWidget *menubar_about;
  GtkWidget *toolbar_handlebox;
  GtkWidget *toolbar;
  GtkWidget *toolbar_open;
  GtkWidget *toolbar_disc;
  GtkWidget *toolbar_network;
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
  GtkWidget *network_channel_box;
  GtkWidget *channel_label;
  GtkObject *network_channel_spinbutton_adj;
  GtkWidget *network_channel_spinbutton;
  GtkWidget *network_channel_go_button;
  GtkWidget *intf_statusbar;
  GtkAccelGroup *accel_group;
  GtkTooltips *tooltips;

  tooltips = gtk_tooltips_new ();

  accel_group = gtk_accel_group_new ();

  intf_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_object_set_data (GTK_OBJECT (intf_window), "intf_window", intf_window);
  gtk_window_set_title (GTK_WINDOW (intf_window), _("VideoLAN Client"));
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
  menubar_file_menu_accels = gtk_menu_ensure_uline_accel_group (GTK_MENU (menubar_file_menu));

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
  gtk_tooltips_set_tip (tooltips, menubar_open, _("Open a File"), NULL);
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
  gtk_tooltips_set_tip (tooltips, menubar_disc, _("Open a DVD or VCD"), NULL);
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
  gtk_tooltips_set_tip (tooltips, menubar_network, _("Select a Network Stream"), NULL);
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
  menubar_view_menu_accels = gtk_menu_ensure_uline_accel_group (GTK_MENU (menubar_view_menu));

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

  menubar_fullscreen = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_fullscreen)->child),
                                   _("_Fullscreen"));
  gtk_widget_add_accelerator (menubar_fullscreen, "activate_item", menubar_view_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_fullscreen);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_fullscreen", menubar_fullscreen,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_fullscreen);
  gtk_container_add (GTK_CONTAINER (menubar_view_menu), menubar_fullscreen);

  separator13 = gtk_menu_item_new ();
  gtk_widget_ref (separator13);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator13", separator13,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator13);
  gtk_container_add (GTK_CONTAINER (menubar_view_menu), separator13);
  gtk_widget_set_sensitive (separator13, FALSE);

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

  menubar_angle = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_angle)->child),
                                   _("An_gle"));
  gtk_widget_add_accelerator (menubar_angle, "activate_item", menubar_view_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_angle);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_angle", menubar_angle,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_angle);
  gtk_container_add (GTK_CONTAINER (menubar_view_menu), menubar_angle);
  gtk_widget_set_sensitive (menubar_angle, FALSE);

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
  gtk_tooltips_set_tip (tooltips, menubar_modules, _("Open the plugin manager"), NULL);

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
  menubar_settings_menu_accels = gtk_menu_ensure_uline_accel_group (GTK_MENU (menubar_settings_menu));

  menubar_audio = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_audio)->child),
                                   _("A_udio"));
  gtk_widget_add_accelerator (menubar_audio, "activate_item", menubar_settings_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_audio);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_audio", menubar_audio,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_audio);
  gtk_container_add (GTK_CONTAINER (menubar_settings_menu), menubar_audio);
  gtk_widget_set_sensitive (menubar_audio, FALSE);
  gtk_tooltips_set_tip (tooltips, menubar_audio, _("Select audio language"), NULL);

  menubar_subpictures = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (menubar_subpictures)->child),
                                   _("_Subtitles"));
  gtk_widget_add_accelerator (menubar_subpictures, "activate_item", menubar_settings_menu_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (menubar_subpictures);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "menubar_subpictures", menubar_subpictures,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (menubar_subpictures);
  gtk_container_add (GTK_CONTAINER (menubar_settings_menu), menubar_subpictures);
  gtk_widget_set_sensitive (menubar_subpictures, FALSE);
  gtk_tooltips_set_tip (tooltips, menubar_subpictures, _("Select sub-title"), NULL);

  separator8 = gtk_menu_item_new ();
  gtk_widget_ref (separator8);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "separator8", separator8,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator8);
  gtk_container_add (GTK_CONTAINER (menubar_settings_menu), separator8);
  gtk_widget_set_sensitive (separator8, FALSE);

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
  menubar_help_menu_accels = gtk_menu_ensure_uline_accel_group (GTK_MENU (menubar_help_menu));

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

  toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);
  gtk_widget_ref (toolbar);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar", toolbar,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar);
  gtk_container_add (GTK_CONTAINER (toolbar_handlebox), toolbar);
  gtk_container_set_border_width (GTK_CONTAINER (toolbar), 1);
  gtk_toolbar_set_space_size (GTK_TOOLBAR (toolbar), 16);
  gtk_toolbar_set_space_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_SPACE_LINE);
  gtk_toolbar_set_button_relief (GTK_TOOLBAR (toolbar), GTK_RELIEF_NONE);

  toolbar_open = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("File"),
                                _("Open a File"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_open);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_open", toolbar_open,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_open);

  toolbar_disc = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Disc"),
                                _("Open a DVD or VCD"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_disc);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_disc", toolbar_disc,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_disc);

  toolbar_network = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Net"),
                                _("Select a Network Stream"), NULL,
                                NULL, NULL, NULL);
  gtk_widget_ref (toolbar_network);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "toolbar_network", toolbar_network,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_network);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  toolbar_back = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Back"),
                                _("Go Backwards"), NULL,
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

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

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

  slider_frame = gtk_frame_new (_("-:--:--"));
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

  title = gtk_label_new (_("Title:  "));
  gtk_widget_ref (title);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "title", title,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (title);
  gtk_box_pack_start (GTK_BOX (title_box), title, FALSE, FALSE, 5);

  title_label = gtk_label_new (_("--"));
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

  chapter = gtk_label_new (_("Chapter:  "));
  gtk_widget_ref (chapter);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "chapter", chapter,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (chapter);
  gtk_box_pack_start (GTK_BOX (chapter_box), chapter, FALSE, FALSE, 5);

  chapter_label = gtk_label_new (_("---"));
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

  network_address_label = gtk_label_new (_("No server !"));
  gtk_widget_ref (network_address_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "network_address_label", network_address_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_address_label);
  gtk_box_pack_start (GTK_BOX (network_box), network_address_label, TRUE, TRUE, 0);

  network_channel_box = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (network_channel_box);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "network_channel_box", network_channel_box,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_channel_box);
  gtk_box_pack_start (GTK_BOX (network_box), network_channel_box, TRUE, FALSE, 0);

  channel_label = gtk_label_new (_("Channel:  "));
  gtk_widget_ref (channel_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "channel_label", channel_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (channel_label);
  gtk_box_pack_start (GTK_BOX (network_channel_box), channel_label, FALSE, FALSE, 5);

  network_channel_spinbutton_adj = gtk_adjustment_new (0, 0, 100, 1, 10, 10);
  network_channel_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (network_channel_spinbutton_adj), 1, 0);
  gtk_widget_ref (network_channel_spinbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "network_channel_spinbutton", network_channel_spinbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_channel_spinbutton);
  gtk_box_pack_start (GTK_BOX (network_channel_box), network_channel_spinbutton, FALSE, TRUE, 0);

  network_channel_go_button = gtk_button_new_with_label (_("Go!"));
  gtk_widget_ref (network_channel_go_button);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "network_channel_go_button", network_channel_go_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_channel_go_button);
  gtk_box_pack_start (GTK_BOX (network_channel_box), network_channel_go_button, FALSE, FALSE, 0);
  gtk_button_set_relief (GTK_BUTTON (network_channel_go_button), GTK_RELIEF_NONE);

  intf_statusbar = gtk_statusbar_new ();
  gtk_widget_ref (intf_statusbar);
  gtk_object_set_data_full (GTK_OBJECT (intf_window), "intf_statusbar", intf_statusbar,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (intf_statusbar);
  gtk_box_pack_start (GTK_BOX (window_vbox), intf_statusbar, FALSE, FALSE, 0);
  gtk_widget_set_usize (intf_statusbar, 500, -2);

  gtk_signal_connect (GTK_OBJECT (intf_window), "drag_data_received",
                      GTK_SIGNAL_FUNC (GtkWindowDrag),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (intf_window), "delete_event",
                      GTK_SIGNAL_FUNC (GtkWindowDelete),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (menubar_open), "activate",
                      GTK_SIGNAL_FUNC (GtkFileOpenActivate),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (menubar_disc), "activate",
                      GTK_SIGNAL_FUNC (GtkDiscOpenActivate),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (menubar_network), "activate",
                      GTK_SIGNAL_FUNC (GtkNetworkOpenActivate),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (menubar_eject), "activate",
                      GTK_SIGNAL_FUNC (GtkEjectDiscActivate),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (menubar_exit), "activate",
                      GTK_SIGNAL_FUNC (GtkExitActivate),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (menubar_interface_hide), "activate",
                      GTK_SIGNAL_FUNC (GtkWindowToggleActivate),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (menubar_fullscreen), "activate",
                      GTK_SIGNAL_FUNC (GtkFullscreenActivate),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (menubar_playlist), "activate",
                      GTK_SIGNAL_FUNC (GtkPlaylistActivate),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (menubar_modules), "activate",
                      GTK_SIGNAL_FUNC (GtkModulesActivate),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (menubar_messages), "activate",
                      GTK_SIGNAL_FUNC (GtkMessagesActivate),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (menubar_preferences), "activate",
                      GTK_SIGNAL_FUNC (GtkPreferencesActivate),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (menubar_about), "activate",
                      GTK_SIGNAL_FUNC (GtkAboutActivate),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (toolbar_open), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkFileOpenShow),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (toolbar_disc), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkDiscOpenShow),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (toolbar_network), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkNetworkOpenShow),
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
  gtk_signal_connect (GTK_OBJECT (slider), "button_release_event",
                      GTK_SIGNAL_FUNC (GtkSliderRelease),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (slider), "button_press_event",
                      GTK_SIGNAL_FUNC (GtkSliderPress),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (title_prev_button), "clicked",
                      GTK_SIGNAL_FUNC (GtkTitlePrev),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (title_next_button), "clicked",
                      GTK_SIGNAL_FUNC (GtkTitleNext),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (chapter_prev_button), "clicked",
                      GTK_SIGNAL_FUNC (GtkChapterPrev),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (chapter_next_button), "clicked",
                      GTK_SIGNAL_FUNC (GtkChapterNext),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (network_channel_spinbutton), "activate",
                      GTK_SIGNAL_FUNC (GtkNetworkJoin),
                      "intf_window");
  gtk_signal_connect (GTK_OBJECT (network_channel_go_button), "clicked",
                      GTK_SIGNAL_FUNC (GtkChannelGo),
                      "intf_window");

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
  GtkWidget *separator6;
  GtkWidget *popup_interface_toggle;
  GtkWidget *popup_fullscreen;
  GtkWidget *separator5;
  GtkWidget *popup_next;
  GtkWidget *popup_prev;
  GtkWidget *popup_jump;
  GtkWidget *popup_navigation;
  GtkWidget *popup_angle;
  GtkWidget *popup_audio;
  GtkWidget *popup_subpictures;
  GtkWidget *popup_open;
  GtkWidget *popup_open_menu;
  GtkAccelGroup *popup_open_menu_accels;
  GtkWidget *popup_file;
  GtkWidget *popup_disc;
  GtkWidget *popup_network;
  GtkWidget *separator12;
  GtkWidget *popup_about;
  GtkWidget *popup_playlist;
  GtkWidget *popup_preferences;
  GtkWidget *separator9;
  GtkWidget *popup_exit;
  GtkTooltips *tooltips;

  tooltips = gtk_tooltips_new ();

  intf_popup = gtk_menu_new ();
  gtk_object_set_data (GTK_OBJECT (intf_popup), "intf_popup", intf_popup);
  intf_popup_accels = gtk_menu_ensure_uline_accel_group (GTK_MENU (intf_popup));

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

  separator6 = gtk_menu_item_new ();
  gtk_widget_ref (separator6);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator6", separator6,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator6);
  gtk_container_add (GTK_CONTAINER (intf_popup), separator6);
  gtk_widget_set_sensitive (separator6, FALSE);

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

  popup_fullscreen = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (popup_fullscreen)->child),
                                   _("_Fullscreen"));
  gtk_widget_add_accelerator (popup_fullscreen, "activate_item", intf_popup_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (popup_fullscreen);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_fullscreen", popup_fullscreen,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_fullscreen);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_fullscreen);

  separator5 = gtk_menu_item_new ();
  gtk_widget_ref (separator5);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "separator5", separator5,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (separator5);
  gtk_container_add (GTK_CONTAINER (intf_popup), separator5);
  gtk_widget_set_sensitive (separator5, FALSE);

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

  popup_angle = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (popup_angle)->child),
                                   _("An_gle"));
  gtk_widget_add_accelerator (popup_angle, "activate_item", intf_popup_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (popup_angle);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_angle", popup_angle,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_angle);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_angle);
  gtk_widget_set_sensitive (popup_angle, FALSE);

  popup_audio = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (popup_audio)->child),
                                   _("_Audio"));
  gtk_widget_add_accelerator (popup_audio, "activate_item", intf_popup_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (popup_audio);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_audio", popup_audio,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_audio);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_audio);
  gtk_widget_set_sensitive (popup_audio, FALSE);

  popup_subpictures = gtk_menu_item_new_with_label ("");
  tmp_key = gtk_label_parse_uline (GTK_LABEL (GTK_BIN (popup_subpictures)->child),
                                   _("_Subtitles"));
  gtk_widget_add_accelerator (popup_subpictures, "activate_item", intf_popup_accels,
                              tmp_key, 0, 0);
  gtk_widget_ref (popup_subpictures);
  gtk_object_set_data_full (GTK_OBJECT (intf_popup), "popup_subpictures", popup_subpictures,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (popup_subpictures);
  gtk_container_add (GTK_CONTAINER (intf_popup), popup_subpictures);
  gtk_widget_set_sensitive (popup_subpictures, FALSE);

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
  popup_open_menu_accels = gtk_menu_ensure_uline_accel_group (GTK_MENU (popup_open_menu));

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
  gtk_tooltips_set_tip (tooltips, popup_file, _("Open a File"), NULL);

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
  gtk_tooltips_set_tip (tooltips, popup_disc, _("Open a DVD or VCD"), NULL);

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
  gtk_tooltips_set_tip (tooltips, popup_network, _("Select a Network Stream"), NULL);

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
                      GTK_SIGNAL_FUNC (GtkPlayActivate),
                      "intf_popup");
  gtk_signal_connect (GTK_OBJECT (popup_pause), "activate",
                      GTK_SIGNAL_FUNC (GtkPauseActivate),
                      "intf_popup");
  gtk_signal_connect (GTK_OBJECT (popup_stop), "activate",
                      GTK_SIGNAL_FUNC (GtKStopActivate),
                      "intf_popup");
  gtk_signal_connect (GTK_OBJECT (popup_back), "activate",
                      GTK_SIGNAL_FUNC (GtkBackActivate),
                      "intf_popup");
  gtk_signal_connect (GTK_OBJECT (popup_slow), "activate",
                      GTK_SIGNAL_FUNC (GtkSlowActivate),
                      "intf_popup");
  gtk_signal_connect (GTK_OBJECT (popup_fast), "activate",
                      GTK_SIGNAL_FUNC (GtkFastActivate),
                      "intf_popup");
  gtk_signal_connect (GTK_OBJECT (popup_interface_toggle), "activate",
                      GTK_SIGNAL_FUNC (GtkWindowToggleActivate),
                      "intf_popup");
  gtk_signal_connect (GTK_OBJECT (popup_fullscreen), "activate",
                      GTK_SIGNAL_FUNC (GtkFullscreenActivate),
                      "intf_popup");
  gtk_signal_connect (GTK_OBJECT (popup_next), "activate",
                      GTK_SIGNAL_FUNC (GtkNextActivate),
                      "intf_popup");
  gtk_signal_connect (GTK_OBJECT (popup_prev), "activate",
                      GTK_SIGNAL_FUNC (GtkPrevActivate),
                      "intf_popup");
  gtk_signal_connect (GTK_OBJECT (popup_jump), "activate",
                      GTK_SIGNAL_FUNC (GtkJumpActivate),
                      "intf_popup");
  gtk_signal_connect (GTK_OBJECT (popup_file), "activate",
                      GTK_SIGNAL_FUNC (GtkFileOpenActivate),
                      "intf_popup");
  gtk_signal_connect (GTK_OBJECT (popup_disc), "activate",
                      GTK_SIGNAL_FUNC (GtkDiscOpenActivate),
                      "intf_popup");
  gtk_signal_connect (GTK_OBJECT (popup_network), "activate",
                      GTK_SIGNAL_FUNC (GtkNetworkOpenActivate),
                      "intf_popup");
  gtk_signal_connect (GTK_OBJECT (popup_about), "activate",
                      GTK_SIGNAL_FUNC (GtkAboutActivate),
                      "intf_popup");
  gtk_signal_connect (GTK_OBJECT (popup_playlist), "activate",
                      GTK_SIGNAL_FUNC (GtkPlaylistActivate),
                      "intf_popup");
  gtk_signal_connect (GTK_OBJECT (popup_preferences), "activate",
                      GTK_SIGNAL_FUNC (GtkPreferencesActivate),
                      "intf_popup");
  gtk_signal_connect (GTK_OBJECT (popup_exit), "activate",
                      GTK_SIGNAL_FUNC (GtkExitActivate),
                      "intf_popup");

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
  GtkWidget *label16;
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

  label14 = gtk_label_new (_("VideoLAN Client"));
  gtk_widget_ref (label14);
  gtk_object_set_data_full (GTK_OBJECT (intf_about), "label14", label14,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label14);
  gtk_box_pack_start (GTK_BOX (vbox3), label14, TRUE, TRUE, 0);
  gtk_misc_set_padding (GTK_MISC (label14), 0, 10);

  label18 = gtk_label_new (_("(C) 1996, 1997, 1998, 1999, 2000, 2001 - the VideoLAN Team"));
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

  label16 = gtk_label_new (_("Régis Duchesne <regis@via.ecp.fr>\nMichel Lespinasse <walken@zoy.org>\nOlivier Pomel <pomel@via.ecp.fr>\nPierre Baillet <oct@zoy.org>\nJean-Philippe Grimaldi <jeanphi@via.ecp.fr>\nAndres Krapf <dae@via.ecp.fr>\nChristophe Massiot <massiot@via.ecp.fr>\nVincent Seguin <seguin@via.ecp.fr>\nBenoit Steiner <benny@via.ecp.fr>\nArnaud de Bossoreille de Ribou <bozo@via.ecp.fr>\nJean-Marc Dressler <polux@via.ecp.fr>\nGaël Hendryckx <jimmy@via.ecp.fr>\nSamuel Hocevar <sam@zoy.org>\nBrieuc Jeunhomme <bbp@via.ecp.fr>\nMichel Kaempf <maxx@via.ecp.fr>\nStéphane Borel <stef@via.ecp.fr>\nRenaud Dartus <reno@via.ecp.fr>\nHenri Fallon <henri@via.ecp.fr>"));
  gtk_widget_ref (label16);
  gtk_object_set_data_full (GTK_OBJECT (intf_about), "label16", label16,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label16);
  gtk_container_add (GTK_CONTAINER (frame1), label16);
  gtk_label_set_justify (GTK_LABEL (label16), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label16), 0.5, 0);
  gtk_misc_set_padding (GTK_MISC (label16), 5, 5);

  label17 = gtk_label_new (_("This is the VideoLAN client, a DVD and MPEG player. It can play MPEG and MPEG 2 files from a file or from a network source."));
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
                      "intf_about");

  gtk_widget_grab_default (about_ok);
  return intf_about;
}

GtkWidget*
create_intf_fileopen (void)
{
  GtkWidget *intf_fileopen;
  GtkWidget *fileopen_ok;
  GtkWidget *fileopen_cancel;

  intf_fileopen = gtk_file_selection_new (_("Select File"));
  gtk_object_set_data (GTK_OBJECT (intf_fileopen), "intf_fileopen", intf_fileopen);
  gtk_container_set_border_width (GTK_CONTAINER (intf_fileopen), 10);
  gtk_window_set_modal (GTK_WINDOW (intf_fileopen), TRUE);

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
create_intf_disc (void)
{
  GtkWidget *intf_disc;
  GtkWidget *dialog_vbox2;
  GtkWidget *vbox4;
  GtkWidget *hbox3;
  GtkWidget *frame2;
  GtkWidget *vbox5;
  GSList *disc_group = NULL;
  GtkWidget *disc_dvd;
  GtkWidget *disc_vcd;
  GtkWidget *frame3;
  GtkWidget *table1;
  GtkObject *disc_title_adj;
  GtkWidget *disc_title;
  GtkObject *disc_chapter_adj;
  GtkWidget *disc_chapter;
  GtkWidget *label20;
  GtkWidget *label21;
  GtkWidget *hbox2;
  GtkWidget *label19;
  GtkWidget *disc_name;
  GtkWidget *dialog_action_area1;
  GtkWidget *hbox1;
  GtkWidget *disc_ok;
  GtkWidget *disc_cancel;

  intf_disc = gtk_dialog_new ();
  gtk_object_set_data (GTK_OBJECT (intf_disc), "intf_disc", intf_disc);
  gtk_window_set_title (GTK_WINDOW (intf_disc), _("Open Disc"));
  gtk_window_set_modal (GTK_WINDOW (intf_disc), TRUE);
  gtk_window_set_policy (GTK_WINDOW (intf_disc), FALSE, FALSE, FALSE);

  dialog_vbox2 = GTK_DIALOG (intf_disc)->vbox;
  gtk_object_set_data (GTK_OBJECT (intf_disc), "dialog_vbox2", dialog_vbox2);
  gtk_widget_show (dialog_vbox2);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_vbox2), 5);

  vbox4 = gtk_vbox_new (FALSE, 5);
  gtk_widget_ref (vbox4);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "vbox4", vbox4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox4);
  gtk_box_pack_start (GTK_BOX (dialog_vbox2), vbox4, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox4), 5);

  hbox3 = gtk_hbox_new (FALSE, 5);
  gtk_widget_ref (hbox3);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "hbox3", hbox3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox3);
  gtk_box_pack_start (GTK_BOX (vbox4), hbox3, TRUE, TRUE, 0);

  frame2 = gtk_frame_new (_("Disc type"));
  gtk_widget_ref (frame2);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "frame2", frame2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame2);
  gtk_box_pack_start (GTK_BOX (hbox3), frame2, TRUE, TRUE, 0);

  vbox5 = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (vbox5);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "vbox5", vbox5,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox5);
  gtk_container_add (GTK_CONTAINER (frame2), vbox5);

  disc_dvd = gtk_radio_button_new_with_label (disc_group, _("DVD"));
  disc_group = gtk_radio_button_group (GTK_RADIO_BUTTON (disc_dvd));
  gtk_widget_ref (disc_dvd);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "disc_dvd", disc_dvd,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_dvd);
  gtk_box_pack_start (GTK_BOX (vbox5), disc_dvd, FALSE, FALSE, 0);

  disc_vcd = gtk_radio_button_new_with_label (disc_group, _("VCD"));
  disc_group = gtk_radio_button_group (GTK_RADIO_BUTTON (disc_vcd));
  gtk_widget_ref (disc_vcd);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "disc_vcd", disc_vcd,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_vcd);
  gtk_box_pack_start (GTK_BOX (vbox5), disc_vcd, FALSE, FALSE, 0);

  frame3 = gtk_frame_new (_("Starting position"));
  gtk_widget_ref (frame3);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "frame3", frame3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame3);
  gtk_box_pack_start (GTK_BOX (hbox3), frame3, TRUE, TRUE, 0);

  table1 = gtk_table_new (2, 2, FALSE);
  gtk_widget_ref (table1);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "table1", table1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table1);
  gtk_container_add (GTK_CONTAINER (frame3), table1);
  gtk_container_set_border_width (GTK_CONTAINER (table1), 5);
  gtk_table_set_row_spacings (GTK_TABLE (table1), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table1), 5);

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

  label20 = gtk_label_new (_("Chapter"));
  gtk_widget_ref (label20);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "label20", label20,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label20);
  gtk_table_attach (GTK_TABLE (table1), label20, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label20), 0, 0.5);

  label21 = gtk_label_new (_("Title"));
  gtk_widget_ref (label21);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "label21", label21,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label21);
  gtk_table_attach (GTK_TABLE (table1), label21, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label21), 0, 0.5);

  hbox2 = gtk_hbox_new (FALSE, 5);
  gtk_widget_ref (hbox2);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "hbox2", hbox2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox2);
  gtk_box_pack_start (GTK_BOX (vbox4), hbox2, TRUE, TRUE, 0);

  label19 = gtk_label_new (_("Device name"));
  gtk_widget_ref (label19);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "label19", label19,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label19);
  gtk_box_pack_start (GTK_BOX (hbox2), label19, FALSE, FALSE, 0);

  disc_name = gtk_entry_new ();
  gtk_widget_ref (disc_name);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "disc_name", disc_name,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_name);
  gtk_box_pack_start (GTK_BOX (hbox2), disc_name, TRUE, TRUE, 0);
  gtk_entry_set_text (GTK_ENTRY (disc_name), DVD_DEVICE);

  dialog_action_area1 = GTK_DIALOG (intf_disc)->action_area;
  gtk_object_set_data (GTK_OBJECT (intf_disc), "dialog_action_area1", dialog_action_area1);
  gtk_widget_show (dialog_action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 5);

  hbox1 = gtk_hbox_new (TRUE, 5);
  gtk_widget_ref (hbox1);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "hbox1", hbox1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox1);
  gtk_box_pack_end (GTK_BOX (dialog_action_area1), hbox1, FALSE, TRUE, 0);

  disc_ok = gtk_button_new_with_label (_("OK"));
  gtk_widget_ref (disc_ok);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "disc_ok", disc_ok,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_ok);
  gtk_box_pack_start (GTK_BOX (hbox1), disc_ok, FALSE, TRUE, 0);

  disc_cancel = gtk_button_new_with_label (_("Cancel"));
  gtk_widget_ref (disc_cancel);
  gtk_object_set_data_full (GTK_OBJECT (intf_disc), "disc_cancel", disc_cancel,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (disc_cancel);
  gtk_box_pack_start (GTK_BOX (hbox1), disc_cancel, FALSE, TRUE, 0);

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
  GtkWidget *vbox7;
  GtkWidget *vbox8;
  GtkWidget *hbox6;
  GtkWidget *frame4;
  GtkWidget *vbox9;
  GSList *network_group = NULL;
  GtkWidget *network_ts;
  GtkWidget *network_rtp;
  GtkWidget *network_http;
  GtkWidget *frame5;
  GtkWidget *table2;
  GtkObject *network_port_adj;
  GtkWidget *network_port;
  GtkWidget *network_port_label;
  GtkWidget *network_server_label;
  GtkWidget *network_broadcast_check;
  GtkWidget *network_broadcast_combo;
  GtkWidget *network_broadcast;
  GtkWidget *network_server_combo;
  GList *network_server_combo_items = NULL;
  GtkWidget *network_server;
  GtkWidget *frame6;
  GtkWidget *hbox14;
  GtkWidget *network_channel_check;
  GtkWidget *network_channel_combo;
  GtkWidget *network_channel;
  GtkWidget *network_channel_port_label;
  GtkObject *network_channel_port_adj;
  GtkWidget *network_channel_port;
  GtkWidget *hbox4;
  GtkWidget *hbox5;
  GtkWidget *network_ok;
  GtkWidget *network_cancel;

  intf_network = gtk_dialog_new ();
  gtk_object_set_data (GTK_OBJECT (intf_network), "intf_network", intf_network);
  gtk_window_set_title (GTK_WINDOW (intf_network), _("Open Network"));
  gtk_window_set_modal (GTK_WINDOW (intf_network), TRUE);
  gtk_window_set_policy (GTK_WINDOW (intf_network), FALSE, FALSE, FALSE);

  vbox7 = GTK_DIALOG (intf_network)->vbox;
  gtk_object_set_data (GTK_OBJECT (intf_network), "vbox7", vbox7);
  gtk_widget_show (vbox7);
  gtk_container_set_border_width (GTK_CONTAINER (vbox7), 5);

  vbox8 = gtk_vbox_new (FALSE, 5);
  gtk_widget_ref (vbox8);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "vbox8", vbox8,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox8);
  gtk_box_pack_start (GTK_BOX (vbox7), vbox8, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox8), 5);

  hbox6 = gtk_hbox_new (FALSE, 5);
  gtk_widget_ref (hbox6);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "hbox6", hbox6,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox6);
  gtk_box_pack_start (GTK_BOX (vbox8), hbox6, TRUE, TRUE, 0);

  frame4 = gtk_frame_new (_("Protocol"));
  gtk_widget_ref (frame4);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "frame4", frame4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame4);
  gtk_box_pack_start (GTK_BOX (hbox6), frame4, TRUE, TRUE, 0);

  vbox9 = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (vbox9);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "vbox9", vbox9,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox9);
  gtk_container_add (GTK_CONTAINER (frame4), vbox9);

  network_ts = gtk_radio_button_new_with_label (network_group, _("TS"));
  network_group = gtk_radio_button_group (GTK_RADIO_BUTTON (network_ts));
  gtk_widget_ref (network_ts);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_ts", network_ts,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_ts);
  gtk_box_pack_start (GTK_BOX (vbox9), network_ts, FALSE, FALSE, 0);

  network_rtp = gtk_radio_button_new_with_label (network_group, _("RTP"));
  network_group = gtk_radio_button_group (GTK_RADIO_BUTTON (network_rtp));
  gtk_widget_ref (network_rtp);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_rtp", network_rtp,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_rtp);
  gtk_box_pack_start (GTK_BOX (vbox9), network_rtp, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (network_rtp, FALSE);

  network_http = gtk_radio_button_new_with_label (network_group, _("HTTP"));
  network_group = gtk_radio_button_group (GTK_RADIO_BUTTON (network_http));
  gtk_widget_ref (network_http);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_http", network_http,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_http);
  gtk_box_pack_start (GTK_BOX (vbox9), network_http, FALSE, FALSE, 0);

  frame5 = gtk_frame_new (_("Server"));
  gtk_widget_ref (frame5);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "frame5", frame5,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame5);
  gtk_box_pack_start (GTK_BOX (hbox6), frame5, TRUE, TRUE, 0);

  table2 = gtk_table_new (3, 2, FALSE);
  gtk_widget_ref (table2);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "table2", table2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table2);
  gtk_container_add (GTK_CONTAINER (frame5), table2);
  gtk_container_set_border_width (GTK_CONTAINER (table2), 5);
  gtk_table_set_row_spacings (GTK_TABLE (table2), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table2), 5);

  network_port_adj = gtk_adjustment_new (1234, 0, 65535, 1, 10, 10);
  network_port = gtk_spin_button_new (GTK_ADJUSTMENT (network_port_adj), 1, 0);
  gtk_widget_ref (network_port);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_port", network_port,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_port);
  gtk_table_attach (GTK_TABLE (table2), network_port, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  network_port_label = gtk_label_new (_("Port"));
  gtk_widget_ref (network_port_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_port_label", network_port_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_port_label);
  gtk_table_attach (GTK_TABLE (table2), network_port_label, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (network_port_label), 0, 0.5);

  network_server_label = gtk_label_new (_("Address"));
  gtk_widget_ref (network_server_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_server_label", network_server_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_server_label);
  gtk_table_attach (GTK_TABLE (table2), network_server_label, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (network_server_label), 0, 0.5);

  network_broadcast_check = gtk_check_button_new_with_label (_("Broadcast"));
  gtk_widget_ref (network_broadcast_check);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_broadcast_check", network_broadcast_check,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_broadcast_check);
  gtk_table_attach (GTK_TABLE (table2), network_broadcast_check, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  network_broadcast_combo = gtk_combo_new ();
  gtk_widget_ref (network_broadcast_combo);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_broadcast_combo", network_broadcast_combo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_broadcast_combo);
  gtk_table_attach (GTK_TABLE (table2), network_broadcast_combo, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_sensitive (network_broadcast_combo, FALSE);

  network_broadcast = GTK_COMBO (network_broadcast_combo)->entry;
  gtk_widget_ref (network_broadcast);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_broadcast", network_broadcast,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_broadcast);
  gtk_widget_set_sensitive (network_broadcast, FALSE);
  gtk_entry_set_text (GTK_ENTRY (network_broadcast), _("138.195.143.255"));

  network_server_combo = gtk_combo_new ();
  gtk_widget_ref (network_server_combo);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_server_combo", network_server_combo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_server_combo);
  gtk_table_attach (GTK_TABLE (table2), network_server_combo, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  network_server_combo_items = g_list_append (network_server_combo_items, (gpointer) _("vls"));
  gtk_combo_set_popdown_strings (GTK_COMBO (network_server_combo), network_server_combo_items);
  g_list_free (network_server_combo_items);

  network_server = GTK_COMBO (network_server_combo)->entry;
  gtk_widget_ref (network_server);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_server", network_server,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_server);
  gtk_entry_set_text (GTK_ENTRY (network_server), _("vls"));

  frame6 = gtk_frame_new (_("Channels"));
  gtk_widget_ref (frame6);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "frame6", frame6,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame6);
  gtk_box_pack_start (GTK_BOX (vbox8), frame6, TRUE, TRUE, 5);
  gtk_frame_set_label_align (GTK_FRAME (frame6), 0.05, 0.5);

  hbox14 = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (hbox14);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "hbox14", hbox14,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox14);
  gtk_container_add (GTK_CONTAINER (frame6), hbox14);

  network_channel_check = gtk_check_button_new_with_label (_("Channel server:"));
  gtk_widget_ref (network_channel_check);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_channel_check", network_channel_check,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_channel_check);
  gtk_box_pack_start (GTK_BOX (hbox14), network_channel_check, FALSE, FALSE, 0);

  network_channel_combo = gtk_combo_new ();
  gtk_widget_ref (network_channel_combo);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_channel_combo", network_channel_combo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_channel_combo);
  gtk_box_pack_start (GTK_BOX (hbox14), network_channel_combo, TRUE, TRUE, 0);

  network_channel = GTK_COMBO (network_channel_combo)->entry;
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
  gtk_box_pack_start (GTK_BOX (hbox14), network_channel_port_label, FALSE, FALSE, 5);

  network_channel_port_adj = gtk_adjustment_new (6010, 1024, 100, 1, 10, 10);
  network_channel_port = gtk_spin_button_new (GTK_ADJUSTMENT (network_channel_port_adj), 1, 0);
  gtk_widget_ref (network_channel_port);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_channel_port", network_channel_port,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_channel_port);
  gtk_box_pack_start (GTK_BOX (hbox14), network_channel_port, TRUE, TRUE, 0);
  gtk_widget_set_usize (network_channel_port, 62, -2);

  hbox4 = GTK_DIALOG (intf_network)->action_area;
  gtk_object_set_data (GTK_OBJECT (intf_network), "hbox4", hbox4);
  gtk_widget_show (hbox4);
  gtk_container_set_border_width (GTK_CONTAINER (hbox4), 5);

  hbox5 = gtk_hbox_new (TRUE, 5);
  gtk_widget_ref (hbox5);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "hbox5", hbox5,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox5);
  gtk_box_pack_end (GTK_BOX (hbox4), hbox5, FALSE, TRUE, 0);

  network_ok = gtk_button_new_with_label (_("OK"));
  gtk_widget_ref (network_ok);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_ok", network_ok,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_ok);
  gtk_box_pack_start (GTK_BOX (hbox5), network_ok, FALSE, TRUE, 0);

  network_cancel = gtk_button_new_with_label (_("Cancel"));
  gtk_widget_ref (network_cancel);
  gtk_object_set_data_full (GTK_OBJECT (intf_network), "network_cancel", network_cancel,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_cancel);
  gtk_box_pack_start (GTK_BOX (hbox5), network_cancel, FALSE, TRUE, 0);

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

  return intf_network;
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

  jump_frame = gtk_frame_new (_("Go to:"));
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

  jump_ok_button = gtk_button_new_with_label (_("Ok"));
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
                      "intf_jump");
  gtk_signal_connect (GTK_OBJECT (jump_cancel_button), "clicked",
                      GTK_SIGNAL_FUNC (GtkJumpCancel),
                      "intf_jump");

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
  playlist_add_menu_accels = gtk_menu_ensure_uline_accel_group (GTK_MENU (playlist_add_menu));

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

  playlist_add_url = gtk_menu_item_new_with_label (_("Url"));
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
  playlist_delete_menu_accels = gtk_menu_ensure_uline_accel_group (GTK_MENU (playlist_delete_menu));

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
  playlist_selection_menu_accels = gtk_menu_ensure_uline_accel_group (GTK_MENU (playlist_selection_menu));

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

  playlist_ok_button = gtk_button_new_with_label (_("Ok"));
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
                      GTK_SIGNAL_FUNC (GtkDiscOpenActivate),
                      "intf_playlist");
  gtk_signal_connect (GTK_OBJECT (playlist_add_file), "activate",
                      GTK_SIGNAL_FUNC (GtkFileOpenActivate),
                      "intf_playlist");
  gtk_signal_connect (GTK_OBJECT (playlist_add_network), "activate",
                      GTK_SIGNAL_FUNC (GtkNetworkOpenActivate),
                      "intf_playlist");
  gtk_signal_connect (GTK_OBJECT (playlist_add_url), "activate",
                      GTK_SIGNAL_FUNC (GtkPlaylistAddUrl),
                      "intf_playlist");
  gtk_signal_connect (GTK_OBJECT (playlist_delete_all), "activate",
                      GTK_SIGNAL_FUNC (GtkPlaylistDeleteAll),
                      "intf_playlist");
  gtk_signal_connect (GTK_OBJECT (playlist_delete_selected), "activate",
                      GTK_SIGNAL_FUNC (GtkPlaylistDeleteSelected),
                      "intf_playlist");
  gtk_signal_connect (GTK_OBJECT (playlist_selection_crop), "activate",
                      GTK_SIGNAL_FUNC (GtkPlaylistCrop),
                      "intf_playlist");
  gtk_signal_connect (GTK_OBJECT (playlist_selection_invert), "activate",
                      GTK_SIGNAL_FUNC (GtkPlaylistInvert),
                      "intf_playlist");
  gtk_signal_connect (GTK_OBJECT (playlist_selection_select), "activate",
                      GTK_SIGNAL_FUNC (GtkPlaylistSelect),
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
  gtk_signal_connect (GTK_OBJECT (playlist_ok_button), "clicked",
                      GTK_SIGNAL_FUNC (GtkPlaylistOk),
                      "intf_playlist");
  gtk_signal_connect (GTK_OBJECT (playlist_cancel_button), "clicked",
                      GTK_SIGNAL_FUNC (GtkPlaylistCancel),
                      "intf_playlist");

  return intf_playlist;
}

GtkWidget*
create_intf_preferences (void)
{
  GtkWidget *intf_preferences;
  GtkWidget *dialog_vbox5;
  GtkWidget *preferences_notebook;
  GtkWidget *preferences_file_table;
  GtkWidget *preferences_file_path_label;
  GtkWidget *preferences_file_path_combo;
  GtkWidget *preferences_file_path_entry;
  GtkWidget *preferences_file_path_button;
  GtkWidget *preferences_file;
  GtkWidget *preferences_disc_table;
  GtkWidget *preferences_disc_vcd_label;
  GtkWidget *preference_disc_dvd_label;
  GtkWidget *preferences_disc_dvd_combo;
  GList *preferences_disc_dvd_combo_items = NULL;
  GtkWidget *preferences_disc_dvd_entry;
  GtkWidget *preferences_disc_vcd_combo;
  GList *preferences_disc_vcd_combo_items = NULL;
  GtkWidget *preferences_disc_vcd_entry;
  GtkWidget *preferences_disc_dvd_browse_button;
  GtkWidget *preferences_disc_vcd_browse_button;
  GtkWidget *preferences_disc;
  GtkWidget *preferences_network_table;
  GtkWidget *preferences_network_server_combo;
  GtkWidget *preferences_network_server_entry;
  GtkWidget *preferences_network_broadcast_combo;
  GtkWidget *preferences_network_broadcast_entry;
  GtkWidget *preferences_network_server_label;
  GtkWidget *preferences_network_port_label;
  GtkObject *preferences_network_port_spinbutton_adj;
  GtkWidget *preferences_network_port_spinbutton;
  GtkWidget *preferences_network_broadcast_label;
  GtkWidget *preferences_network_broadcast_checkbutton;
  GtkWidget *preferences_network_protocol_label;
  GSList *preferences_network_protocol_group_group = NULL;
  GtkWidget *preferences_network_ts_radiobutton;
  GtkWidget *preferences_network_rtp_radiobutton;
  GtkWidget *preferences_network_http_radiobutton;
  GtkWidget *preferences_network;
  GtkWidget *preferences_interface_table;
  GtkWidget *preferences_interface_combo;
  GList *preferences_interface_combo_items = NULL;
  GtkWidget *preferences_interface_entry;
  GtkWidget *preferences_interface_label;
  GtkWidget *preferences_interface;
  GtkWidget *preferences_video_table;
  GtkWidget *preferences_video_output_combo;
  GList *preferences_video_output_combo_items = NULL;
  GtkWidget *preferences_video_output_entry;
  GtkWidget *preferences_video_output_label;
  GtkWidget *preferences_video_width_label;
  GtkObject *preferences_video_height_spinbutton_adj;
  GtkWidget *preferences_video_height_spinbutton;
  GtkObject *preferences_video_width_spinbutton_adj;
  GtkWidget *preferences_video_width_spinbutton;
  GtkWidget *preferences_video_height_label;
  GtkWidget *preferences_depth_optionmenu;
  GtkWidget *preferences_depth_optionmenu_menu;
  GtkWidget *glade_menuitem;
  GtkWidget *preferences_video_depth_label;
  GtkWidget *preferences_video_fullscreen_depth_label;
  GtkWidget *preferences_video_fullscreen_depth_optionmenu;
  GtkWidget *preferences_video_fullscreen_depth_optionmenu_menu;
  GtkObject *preferences_video_gamma_spinbutton_adj;
  GtkWidget *preferences_video_gamma_spinbutton;
  GtkWidget *preferences_video_gamma_label;
  GtkWidget *preferences_video_fullscreen_checkbutton;
  GtkWidget *preferences_video_grayscale_checkbutton;
  GtkWidget *preferences_video;
  GtkWidget *preferences_audio_table;
  GtkWidget *preferences_audio_quality_optionmenu;
  GtkWidget *preferences_audio_quality_optionmenu_menu;
  GtkWidget *preferences_audio_frequency_optionmenu;
  GtkWidget *preferences_audio_frequency_optionmenu_menu;
  GtkWidget *preferences_audio_channels_optionmenu;
  GtkWidget *preferences_audio_channels_optionmenu_menu;
  GtkWidget *preferences_audio_output_label;
  GtkWidget *preferences_audio_device_label;
  GtkWidget *preferences_audio_quality_label;
  GtkWidget *preferences_audio_frequency_label;
  GtkWidget *preferences_audio_channel_label;
  GtkWidget *preferences_audio_spdif_checkbutton;
  GtkWidget *preferences_audio_output_combo;
  GList *preferences_audio_output_combo_items = NULL;
  GtkWidget *preferences_audio_output_entry;
  GtkWidget *preferences_audio_device_combo;
  GList *preferences_audio_device_combo_items = NULL;
  GtkWidget *preferences_audio_device_entry;
  GtkWidget *preferences_audio;
  GtkWidget *preferences_playlist_table;
  GtkWidget *preferences_playlist_startup_checkbutton;
  GtkWidget *preferences_playlist_enqueue_checkbutton;
  GtkWidget *preferences_playlist_loop_checkbutton;
  GtkWidget *preferences_playlist;
  GtkWidget *preferences_misc_table;
  GtkWidget *preferences_misc_associated_frame;
  GtkWidget *preferences_misc_associated_table;
  GtkWidget *preferences_misc_associated_checkbutton;
  GtkWidget *preferences_misc_mp2_checkbutton;
  GtkWidget *preferences_misc_vob_checkbutton;
  GtkWidget *preferences_misc_ts_checkbutton;
  GtkWidget *preferences_misc_message_frame;
  GtkWidget *preferences_misc_messages_table;
  GtkWidget *preferences_misc_messages_label;
  GtkObject *preferences_misc_messages_spinbutton_adj;
  GtkWidget *preferences_misc_messages_spinbutton;
  GtkWidget *preferences_misc;
  GtkWidget *dialog_action_area4;
  GtkWidget *preferences_ok_button;
  GtkWidget *preferences_apply_button;
  GtkWidget *preferences_cancel_button;

  intf_preferences = gtk_dialog_new ();
  gtk_object_set_data (GTK_OBJECT (intf_preferences), "intf_preferences", intf_preferences);
  gtk_window_set_title (GTK_WINDOW (intf_preferences), _("Preferences"));
  gtk_window_set_policy (GTK_WINDOW (intf_preferences), TRUE, TRUE, FALSE);

  dialog_vbox5 = GTK_DIALOG (intf_preferences)->vbox;
  gtk_object_set_data (GTK_OBJECT (intf_preferences), "dialog_vbox5", dialog_vbox5);
  gtk_widget_show (dialog_vbox5);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_vbox5), 5);

  preferences_notebook = gtk_notebook_new ();
  gtk_widget_ref (preferences_notebook);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_notebook", preferences_notebook,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_notebook);
  gtk_box_pack_start (GTK_BOX (dialog_vbox5), preferences_notebook, TRUE, TRUE, 0);

  preferences_file_table = gtk_table_new (1, 3, FALSE);
  gtk_widget_ref (preferences_file_table);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_file_table", preferences_file_table,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_file_table);
  gtk_container_add (GTK_CONTAINER (preferences_notebook), preferences_file_table);
  gtk_container_set_border_width (GTK_CONTAINER (preferences_file_table), 5);
  gtk_table_set_row_spacings (GTK_TABLE (preferences_file_table), 5);
  gtk_table_set_col_spacings (GTK_TABLE (preferences_file_table), 5);

  preferences_file_path_label = gtk_label_new (_("Default path:"));
  gtk_widget_ref (preferences_file_path_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_file_path_label", preferences_file_path_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_file_path_label);
  gtk_table_attach (GTK_TABLE (preferences_file_table), preferences_file_path_label, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (preferences_file_path_label), 0, 0.5);

  preferences_file_path_combo = gtk_combo_new ();
  gtk_widget_ref (preferences_file_path_combo);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_file_path_combo", preferences_file_path_combo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_file_path_combo);
  gtk_table_attach (GTK_TABLE (preferences_file_table), preferences_file_path_combo, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  preferences_file_path_entry = GTK_COMBO (preferences_file_path_combo)->entry;
  gtk_widget_ref (preferences_file_path_entry);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_file_path_entry", preferences_file_path_entry,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_file_path_entry);

  preferences_file_path_button = gtk_button_new_with_label (_("Browse"));
  gtk_widget_ref (preferences_file_path_button);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_file_path_button", preferences_file_path_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_file_path_button);
  gtk_table_attach (GTK_TABLE (preferences_file_table), preferences_file_path_button, 2, 3, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  preferences_file = gtk_label_new (_("File"));
  gtk_widget_ref (preferences_file);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_file", preferences_file,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_file);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (preferences_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (preferences_notebook), 0), preferences_file);

  preferences_disc_table = gtk_table_new (2, 3, FALSE);
  gtk_widget_ref (preferences_disc_table);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_disc_table", preferences_disc_table,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_disc_table);
  gtk_container_add (GTK_CONTAINER (preferences_notebook), preferences_disc_table);
  gtk_container_set_border_width (GTK_CONTAINER (preferences_disc_table), 5);
  gtk_table_set_row_spacings (GTK_TABLE (preferences_disc_table), 5);
  gtk_table_set_col_spacings (GTK_TABLE (preferences_disc_table), 5);

  preferences_disc_vcd_label = gtk_label_new (_("Default VCD path"));
  gtk_widget_ref (preferences_disc_vcd_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_disc_vcd_label", preferences_disc_vcd_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_disc_vcd_label);
  gtk_table_attach (GTK_TABLE (preferences_disc_table), preferences_disc_vcd_label, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (preferences_disc_vcd_label), 0, 0.5);

  preference_disc_dvd_label = gtk_label_new (_("Default DVD path:"));
  gtk_widget_ref (preference_disc_dvd_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preference_disc_dvd_label", preference_disc_dvd_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preference_disc_dvd_label);
  gtk_table_attach (GTK_TABLE (preferences_disc_table), preference_disc_dvd_label, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (preference_disc_dvd_label), 0, 0.5);

  preferences_disc_dvd_combo = gtk_combo_new ();
  gtk_widget_ref (preferences_disc_dvd_combo);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_disc_dvd_combo", preferences_disc_dvd_combo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_disc_dvd_combo);
  gtk_table_attach (GTK_TABLE (preferences_disc_table), preferences_disc_dvd_combo, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  preferences_disc_dvd_combo_items = g_list_append (preferences_disc_dvd_combo_items, (gpointer) DVD_DEVICE);
  gtk_combo_set_popdown_strings (GTK_COMBO (preferences_disc_dvd_combo), preferences_disc_dvd_combo_items);
  g_list_free (preferences_disc_dvd_combo_items);

  preferences_disc_dvd_entry = GTK_COMBO (preferences_disc_dvd_combo)->entry;
  gtk_widget_ref (preferences_disc_dvd_entry);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_disc_dvd_entry", preferences_disc_dvd_entry,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_disc_dvd_entry);
  gtk_entry_set_text (GTK_ENTRY (preferences_disc_dvd_entry), DVD_DEVICE);

  preferences_disc_vcd_combo = gtk_combo_new ();
  gtk_widget_ref (preferences_disc_vcd_combo);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_disc_vcd_combo", preferences_disc_vcd_combo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_disc_vcd_combo);
  gtk_table_attach (GTK_TABLE (preferences_disc_table), preferences_disc_vcd_combo, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  preferences_disc_vcd_combo_items = g_list_append (preferences_disc_vcd_combo_items, (gpointer) _("/dev/cdrom"));
  gtk_combo_set_popdown_strings (GTK_COMBO (preferences_disc_vcd_combo), preferences_disc_vcd_combo_items);
  g_list_free (preferences_disc_vcd_combo_items);

  preferences_disc_vcd_entry = GTK_COMBO (preferences_disc_vcd_combo)->entry;
  gtk_widget_ref (preferences_disc_vcd_entry);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_disc_vcd_entry", preferences_disc_vcd_entry,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_disc_vcd_entry);
  gtk_entry_set_text (GTK_ENTRY (preferences_disc_vcd_entry), _("/dev/cdrom"));

  preferences_disc_dvd_browse_button = gtk_button_new_with_label (_("Browse"));
  gtk_widget_ref (preferences_disc_dvd_browse_button);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_disc_dvd_browse_button", preferences_disc_dvd_browse_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_disc_dvd_browse_button);
  gtk_table_attach (GTK_TABLE (preferences_disc_table), preferences_disc_dvd_browse_button, 2, 3, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  preferences_disc_vcd_browse_button = gtk_button_new_with_label (_("Browse"));
  gtk_widget_ref (preferences_disc_vcd_browse_button);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_disc_vcd_browse_button", preferences_disc_vcd_browse_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_disc_vcd_browse_button);
  gtk_table_attach (GTK_TABLE (preferences_disc_table), preferences_disc_vcd_browse_button, 2, 3, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  preferences_disc = gtk_label_new (_("Disc"));
  gtk_widget_ref (preferences_disc);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_disc", preferences_disc,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_disc);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (preferences_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (preferences_notebook), 1), preferences_disc);

  preferences_network_table = gtk_table_new (7, 2, FALSE);
  gtk_widget_ref (preferences_network_table);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_network_table", preferences_network_table,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_network_table);
  gtk_container_add (GTK_CONTAINER (preferences_notebook), preferences_network_table);
  gtk_container_set_border_width (GTK_CONTAINER (preferences_network_table), 5);
  gtk_table_set_row_spacings (GTK_TABLE (preferences_network_table), 5);
  gtk_table_set_col_spacings (GTK_TABLE (preferences_network_table), 5);

  preferences_network_server_combo = gtk_combo_new ();
  gtk_widget_ref (preferences_network_server_combo);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_network_server_combo", preferences_network_server_combo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_network_server_combo);
  gtk_table_attach (GTK_TABLE (preferences_network_table), preferences_network_server_combo, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  preferences_network_server_entry = GTK_COMBO (preferences_network_server_combo)->entry;
  gtk_widget_ref (preferences_network_server_entry);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_network_server_entry", preferences_network_server_entry,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_network_server_entry);
  gtk_entry_set_text (GTK_ENTRY (preferences_network_server_entry), _("vls "));

  preferences_network_broadcast_combo = gtk_combo_new ();
  gtk_widget_ref (preferences_network_broadcast_combo);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_network_broadcast_combo", preferences_network_broadcast_combo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_network_broadcast_combo);
  gtk_table_attach (GTK_TABLE (preferences_network_table), preferences_network_broadcast_combo, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  preferences_network_broadcast_entry = GTK_COMBO (preferences_network_broadcast_combo)->entry;
  gtk_widget_ref (preferences_network_broadcast_entry);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_network_broadcast_entry", preferences_network_broadcast_entry,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_network_broadcast_entry);

  preferences_network_server_label = gtk_label_new (_("Default server:"));
  gtk_widget_ref (preferences_network_server_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_network_server_label", preferences_network_server_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_network_server_label);
  gtk_table_attach (GTK_TABLE (preferences_network_table), preferences_network_server_label, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (preferences_network_server_label), 0, 0.5);

  preferences_network_port_label = gtk_label_new (_("Default port:"));
  gtk_widget_ref (preferences_network_port_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_network_port_label", preferences_network_port_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_network_port_label);
  gtk_table_attach (GTK_TABLE (preferences_network_table), preferences_network_port_label, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (preferences_network_port_label), 0, 0.5);

  preferences_network_port_spinbutton_adj = gtk_adjustment_new (1234, 0, 65535, 1, 10, 10);
  preferences_network_port_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (preferences_network_port_spinbutton_adj), 1, 0);
  gtk_widget_ref (preferences_network_port_spinbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_network_port_spinbutton", preferences_network_port_spinbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_network_port_spinbutton);
  gtk_table_attach (GTK_TABLE (preferences_network_table), preferences_network_port_spinbutton, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  preferences_network_broadcast_label = gtk_label_new (_("Broadcast address:"));
  gtk_widget_ref (preferences_network_broadcast_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_network_broadcast_label", preferences_network_broadcast_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_network_broadcast_label);
  gtk_table_attach (GTK_TABLE (preferences_network_table), preferences_network_broadcast_label, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (preferences_network_broadcast_label), 0, 0.5);

  preferences_network_broadcast_checkbutton = gtk_check_button_new_with_label (_("Broadcast mode"));
  gtk_widget_ref (preferences_network_broadcast_checkbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_network_broadcast_checkbutton", preferences_network_broadcast_checkbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_network_broadcast_checkbutton);
  gtk_table_attach (GTK_TABLE (preferences_network_table), preferences_network_broadcast_checkbutton, 0, 1, 3, 4,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  preferences_network_protocol_label = gtk_label_new (_("Default protocol:"));
  gtk_widget_ref (preferences_network_protocol_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_network_protocol_label", preferences_network_protocol_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_network_protocol_label);
  gtk_table_attach (GTK_TABLE (preferences_network_table), preferences_network_protocol_label, 0, 1, 4, 7,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (preferences_network_protocol_label), 0, 0.5);

  preferences_network_ts_radiobutton = gtk_radio_button_new_with_label (preferences_network_protocol_group_group, _("TS"));
  preferences_network_protocol_group_group = gtk_radio_button_group (GTK_RADIO_BUTTON (preferences_network_ts_radiobutton));
  gtk_widget_ref (preferences_network_ts_radiobutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_network_ts_radiobutton", preferences_network_ts_radiobutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_network_ts_radiobutton);
  gtk_table_attach (GTK_TABLE (preferences_network_table), preferences_network_ts_radiobutton, 1, 2, 4, 5,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  preferences_network_rtp_radiobutton = gtk_radio_button_new_with_label (preferences_network_protocol_group_group, _("RTP"));
  preferences_network_protocol_group_group = gtk_radio_button_group (GTK_RADIO_BUTTON (preferences_network_rtp_radiobutton));
  gtk_widget_ref (preferences_network_rtp_radiobutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_network_rtp_radiobutton", preferences_network_rtp_radiobutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_network_rtp_radiobutton);
  gtk_table_attach (GTK_TABLE (preferences_network_table), preferences_network_rtp_radiobutton, 1, 2, 5, 6,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  preferences_network_http_radiobutton = gtk_radio_button_new_with_label (preferences_network_protocol_group_group, _("HTTP"));
  preferences_network_protocol_group_group = gtk_radio_button_group (GTK_RADIO_BUTTON (preferences_network_http_radiobutton));
  gtk_widget_ref (preferences_network_http_radiobutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_network_http_radiobutton", preferences_network_http_radiobutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_network_http_radiobutton);
  gtk_table_attach (GTK_TABLE (preferences_network_table), preferences_network_http_radiobutton, 1, 2, 6, 7,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  preferences_network = gtk_label_new (_("Network"));
  gtk_widget_ref (preferences_network);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_network", preferences_network,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_network);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (preferences_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (preferences_notebook), 2), preferences_network);

  preferences_interface_table = gtk_table_new (1, 2, FALSE);
  gtk_widget_ref (preferences_interface_table);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_interface_table", preferences_interface_table,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_interface_table);
  gtk_container_add (GTK_CONTAINER (preferences_notebook), preferences_interface_table);
  gtk_container_set_border_width (GTK_CONTAINER (preferences_interface_table), 5);
  gtk_table_set_row_spacings (GTK_TABLE (preferences_interface_table), 5);
  gtk_table_set_col_spacings (GTK_TABLE (preferences_interface_table), 5);

  preferences_interface_combo = gtk_combo_new ();
  gtk_widget_ref (preferences_interface_combo);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_interface_combo", preferences_interface_combo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_interface_combo);
  gtk_table_attach (GTK_TABLE (preferences_interface_table), preferences_interface_combo, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  preferences_interface_combo_items = g_list_append (preferences_interface_combo_items, (gpointer) _("Gtk+"));
  preferences_interface_combo_items = g_list_append (preferences_interface_combo_items, (gpointer) _("Gnome"));
  gtk_combo_set_popdown_strings (GTK_COMBO (preferences_interface_combo), preferences_interface_combo_items);
  g_list_free (preferences_interface_combo_items);

  preferences_interface_entry = GTK_COMBO (preferences_interface_combo)->entry;
  gtk_widget_ref (preferences_interface_entry);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_interface_entry", preferences_interface_entry,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_interface_entry);
  gtk_entry_set_text (GTK_ENTRY (preferences_interface_entry), _("Gtk+"));

  preferences_interface_label = gtk_label_new (_("Default interface:"));
  gtk_widget_ref (preferences_interface_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_interface_label", preferences_interface_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_interface_label);
  gtk_table_attach (GTK_TABLE (preferences_interface_table), preferences_interface_label, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (preferences_interface_label), 0, 0.5);

  preferences_interface = gtk_label_new (_("Interface"));
  gtk_widget_ref (preferences_interface);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_interface", preferences_interface,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_interface);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (preferences_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (preferences_notebook), 3), preferences_interface);

  preferences_video_table = gtk_table_new (7, 2, FALSE);
  gtk_widget_ref (preferences_video_table);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_video_table", preferences_video_table,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_video_table);
  gtk_container_add (GTK_CONTAINER (preferences_notebook), preferences_video_table);
  gtk_container_set_border_width (GTK_CONTAINER (preferences_video_table), 5);
  gtk_table_set_row_spacings (GTK_TABLE (preferences_video_table), 5);
  gtk_table_set_col_spacings (GTK_TABLE (preferences_video_table), 5);

  preferences_video_output_combo = gtk_combo_new ();
  gtk_widget_ref (preferences_video_output_combo);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_video_output_combo", preferences_video_output_combo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_video_output_combo);
  gtk_table_attach (GTK_TABLE (preferences_video_table), preferences_video_output_combo, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  preferences_video_output_combo_items = g_list_append (preferences_video_output_combo_items, (gpointer) _("sdl"));
  preferences_video_output_combo_items = g_list_append (preferences_video_output_combo_items, (gpointer) _("xvideo"));
  preferences_video_output_combo_items = g_list_append (preferences_video_output_combo_items, (gpointer) _("x11"));
  gtk_combo_set_popdown_strings (GTK_COMBO (preferences_video_output_combo), preferences_video_output_combo_items);
  g_list_free (preferences_video_output_combo_items);

  preferences_video_output_entry = GTK_COMBO (preferences_video_output_combo)->entry;
  gtk_widget_ref (preferences_video_output_entry);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_video_output_entry", preferences_video_output_entry,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_video_output_entry);
  gtk_entry_set_text (GTK_ENTRY (preferences_video_output_entry), _("sdl"));

  preferences_video_output_label = gtk_label_new (_("Default output:"));
  gtk_widget_ref (preferences_video_output_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_video_output_label", preferences_video_output_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_video_output_label);
  gtk_table_attach (GTK_TABLE (preferences_video_table), preferences_video_output_label, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (preferences_video_output_label), 0, 0.5);

  preferences_video_width_label = gtk_label_new (_("Default width:"));
  gtk_widget_ref (preferences_video_width_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_video_width_label", preferences_video_width_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_video_width_label);
  gtk_table_attach (GTK_TABLE (preferences_video_table), preferences_video_width_label, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (preferences_video_width_label), 0, 0.5);

  preferences_video_height_spinbutton_adj = gtk_adjustment_new (576, 0, 1200, 1, 10, 10);
  preferences_video_height_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (preferences_video_height_spinbutton_adj), 1, 0);
  gtk_widget_ref (preferences_video_height_spinbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_video_height_spinbutton", preferences_video_height_spinbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_video_height_spinbutton);
  gtk_table_attach (GTK_TABLE (preferences_video_table), preferences_video_height_spinbutton, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  preferences_video_width_spinbutton_adj = gtk_adjustment_new (720, 100, 1600, 1, 10, 10);
  preferences_video_width_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (preferences_video_width_spinbutton_adj), 1, 0);
  gtk_widget_ref (preferences_video_width_spinbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_video_width_spinbutton", preferences_video_width_spinbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_video_width_spinbutton);
  gtk_table_attach (GTK_TABLE (preferences_video_table), preferences_video_width_spinbutton, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  preferences_video_height_label = gtk_label_new (_("Default height:"));
  gtk_widget_ref (preferences_video_height_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_video_height_label", preferences_video_height_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_video_height_label);
  gtk_table_attach (GTK_TABLE (preferences_video_table), preferences_video_height_label, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (preferences_video_height_label), 0, 0.5);

  preferences_depth_optionmenu = gtk_option_menu_new ();
  gtk_widget_ref (preferences_depth_optionmenu);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_depth_optionmenu", preferences_depth_optionmenu,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_depth_optionmenu);
  gtk_table_attach (GTK_TABLE (preferences_video_table), preferences_depth_optionmenu, 1, 2, 3, 4,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  preferences_depth_optionmenu_menu = gtk_menu_new ();
  glade_menuitem = gtk_menu_item_new_with_label (_("15 bits"));
  gtk_widget_show (glade_menuitem);
  gtk_menu_append (GTK_MENU (preferences_depth_optionmenu_menu), glade_menuitem);
  glade_menuitem = gtk_menu_item_new_with_label (_("16 bits"));
  gtk_widget_show (glade_menuitem);
  gtk_menu_append (GTK_MENU (preferences_depth_optionmenu_menu), glade_menuitem);
  glade_menuitem = gtk_menu_item_new_with_label (_("32 bits"));
  gtk_widget_show (glade_menuitem);
  gtk_menu_append (GTK_MENU (preferences_depth_optionmenu_menu), glade_menuitem);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (preferences_depth_optionmenu), preferences_depth_optionmenu_menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (preferences_depth_optionmenu), 1);

  preferences_video_depth_label = gtk_label_new (_("Default depth:"));
  gtk_widget_ref (preferences_video_depth_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_video_depth_label", preferences_video_depth_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_video_depth_label);
  gtk_table_attach (GTK_TABLE (preferences_video_table), preferences_video_depth_label, 0, 1, 3, 4,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (preferences_video_depth_label), 0, 0.5);

  preferences_video_fullscreen_depth_label = gtk_label_new (_("Fullscreen depth:"));
  gtk_widget_ref (preferences_video_fullscreen_depth_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_video_fullscreen_depth_label", preferences_video_fullscreen_depth_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_video_fullscreen_depth_label);
  gtk_table_attach (GTK_TABLE (preferences_video_table), preferences_video_fullscreen_depth_label, 0, 1, 4, 5,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (preferences_video_fullscreen_depth_label), 0, 0.5);

  preferences_video_fullscreen_depth_optionmenu = gtk_option_menu_new ();
  gtk_widget_ref (preferences_video_fullscreen_depth_optionmenu);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_video_fullscreen_depth_optionmenu", preferences_video_fullscreen_depth_optionmenu,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_video_fullscreen_depth_optionmenu);
  gtk_table_attach (GTK_TABLE (preferences_video_table), preferences_video_fullscreen_depth_optionmenu, 1, 2, 4, 5,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  preferences_video_fullscreen_depth_optionmenu_menu = gtk_menu_new ();
  glade_menuitem = gtk_menu_item_new_with_label (_("15 bits"));
  gtk_widget_show (glade_menuitem);
  gtk_menu_append (GTK_MENU (preferences_video_fullscreen_depth_optionmenu_menu), glade_menuitem);
  glade_menuitem = gtk_menu_item_new_with_label (_("16 bits"));
  gtk_widget_show (glade_menuitem);
  gtk_menu_append (GTK_MENU (preferences_video_fullscreen_depth_optionmenu_menu), glade_menuitem);
  glade_menuitem = gtk_menu_item_new_with_label (_("32 bits"));
  gtk_widget_show (glade_menuitem);
  gtk_menu_append (GTK_MENU (preferences_video_fullscreen_depth_optionmenu_menu), glade_menuitem);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (preferences_video_fullscreen_depth_optionmenu), preferences_video_fullscreen_depth_optionmenu_menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (preferences_video_fullscreen_depth_optionmenu), 2);

  preferences_video_gamma_spinbutton_adj = gtk_adjustment_new (0, 0, 10, 0.1, 1, 1);
  preferences_video_gamma_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (preferences_video_gamma_spinbutton_adj), 1, 1);
  gtk_widget_ref (preferences_video_gamma_spinbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_video_gamma_spinbutton", preferences_video_gamma_spinbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_video_gamma_spinbutton);
  gtk_table_attach (GTK_TABLE (preferences_video_table), preferences_video_gamma_spinbutton, 1, 2, 5, 6,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  preferences_video_gamma_label = gtk_label_new (_("Gamma:"));
  gtk_widget_ref (preferences_video_gamma_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_video_gamma_label", preferences_video_gamma_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_video_gamma_label);
  gtk_table_attach (GTK_TABLE (preferences_video_table), preferences_video_gamma_label, 0, 1, 5, 6,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (preferences_video_gamma_label), 0, 0.5);

  preferences_video_fullscreen_checkbutton = gtk_check_button_new_with_label (_("Fullscreen on play"));
  gtk_widget_ref (preferences_video_fullscreen_checkbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_video_fullscreen_checkbutton", preferences_video_fullscreen_checkbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_video_fullscreen_checkbutton);
  gtk_table_attach (GTK_TABLE (preferences_video_table), preferences_video_fullscreen_checkbutton, 0, 1, 6, 7,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  preferences_video_grayscale_checkbutton = gtk_check_button_new_with_label (_("Grayscale"));
  gtk_widget_ref (preferences_video_grayscale_checkbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_video_grayscale_checkbutton", preferences_video_grayscale_checkbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_video_grayscale_checkbutton);
  gtk_table_attach (GTK_TABLE (preferences_video_table), preferences_video_grayscale_checkbutton, 1, 2, 6, 7,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  preferences_video = gtk_label_new (_("Video"));
  gtk_widget_ref (preferences_video);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_video", preferences_video,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_video);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (preferences_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (preferences_notebook), 4), preferences_video);

  preferences_audio_table = gtk_table_new (6, 2, FALSE);
  gtk_widget_ref (preferences_audio_table);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_audio_table", preferences_audio_table,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_audio_table);
  gtk_container_add (GTK_CONTAINER (preferences_notebook), preferences_audio_table);
  gtk_container_set_border_width (GTK_CONTAINER (preferences_audio_table), 5);
  gtk_table_set_row_spacings (GTK_TABLE (preferences_audio_table), 5);
  gtk_table_set_col_spacings (GTK_TABLE (preferences_audio_table), 5);

  preferences_audio_quality_optionmenu = gtk_option_menu_new ();
  gtk_widget_ref (preferences_audio_quality_optionmenu);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_audio_quality_optionmenu", preferences_audio_quality_optionmenu,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_audio_quality_optionmenu);
  gtk_table_attach (GTK_TABLE (preferences_audio_table), preferences_audio_quality_optionmenu, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  preferences_audio_quality_optionmenu_menu = gtk_menu_new ();
  glade_menuitem = gtk_menu_item_new_with_label (_("8 bits"));
  gtk_widget_show (glade_menuitem);
  gtk_menu_append (GTK_MENU (preferences_audio_quality_optionmenu_menu), glade_menuitem);
  glade_menuitem = gtk_menu_item_new_with_label (_("16 bits"));
  gtk_widget_show (glade_menuitem);
  gtk_menu_append (GTK_MENU (preferences_audio_quality_optionmenu_menu), glade_menuitem);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (preferences_audio_quality_optionmenu), preferences_audio_quality_optionmenu_menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (preferences_audio_quality_optionmenu), 1);

  preferences_audio_frequency_optionmenu = gtk_option_menu_new ();
  gtk_widget_ref (preferences_audio_frequency_optionmenu);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_audio_frequency_optionmenu", preferences_audio_frequency_optionmenu,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_audio_frequency_optionmenu);
  gtk_table_attach (GTK_TABLE (preferences_audio_table), preferences_audio_frequency_optionmenu, 1, 2, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  preferences_audio_frequency_optionmenu_menu = gtk_menu_new ();
  glade_menuitem = gtk_menu_item_new_with_label (_("48000 Hz"));
  gtk_widget_show (glade_menuitem);
  gtk_menu_append (GTK_MENU (preferences_audio_frequency_optionmenu_menu), glade_menuitem);
  glade_menuitem = gtk_menu_item_new_with_label (_("44100 Hz"));
  gtk_widget_show (glade_menuitem);
  gtk_menu_append (GTK_MENU (preferences_audio_frequency_optionmenu_menu), glade_menuitem);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (preferences_audio_frequency_optionmenu), preferences_audio_frequency_optionmenu_menu);

  preferences_audio_channels_optionmenu = gtk_option_menu_new ();
  gtk_widget_ref (preferences_audio_channels_optionmenu);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_audio_channels_optionmenu", preferences_audio_channels_optionmenu,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_audio_channels_optionmenu);
  gtk_table_attach (GTK_TABLE (preferences_audio_table), preferences_audio_channels_optionmenu, 1, 2, 4, 5,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  preferences_audio_channels_optionmenu_menu = gtk_menu_new ();
  glade_menuitem = gtk_menu_item_new_with_label (_("Mono"));
  gtk_widget_show (glade_menuitem);
  gtk_menu_append (GTK_MENU (preferences_audio_channels_optionmenu_menu), glade_menuitem);
  glade_menuitem = gtk_menu_item_new_with_label (_("Stereo"));
  gtk_widget_show (glade_menuitem);
  gtk_menu_append (GTK_MENU (preferences_audio_channels_optionmenu_menu), glade_menuitem);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (preferences_audio_channels_optionmenu), preferences_audio_channels_optionmenu_menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (preferences_audio_channels_optionmenu), 1);

  preferences_audio_output_label = gtk_label_new (_("Default output:"));
  gtk_widget_ref (preferences_audio_output_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_audio_output_label", preferences_audio_output_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_audio_output_label);
  gtk_table_attach (GTK_TABLE (preferences_audio_table), preferences_audio_output_label, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (preferences_audio_output_label), 0, 0.5);

  preferences_audio_device_label = gtk_label_new (_("Default device:"));
  gtk_widget_ref (preferences_audio_device_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_audio_device_label", preferences_audio_device_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_audio_device_label);
  gtk_table_attach (GTK_TABLE (preferences_audio_table), preferences_audio_device_label, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (preferences_audio_device_label), 0, 0.5);

  preferences_audio_quality_label = gtk_label_new (_("Quality:"));
  gtk_widget_ref (preferences_audio_quality_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_audio_quality_label", preferences_audio_quality_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_audio_quality_label);
  gtk_table_attach (GTK_TABLE (preferences_audio_table), preferences_audio_quality_label, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (preferences_audio_quality_label), 0, 0.5);

  preferences_audio_frequency_label = gtk_label_new (_("Frequency:"));
  gtk_widget_ref (preferences_audio_frequency_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_audio_frequency_label", preferences_audio_frequency_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_audio_frequency_label);
  gtk_table_attach (GTK_TABLE (preferences_audio_table), preferences_audio_frequency_label, 0, 1, 3, 4,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (preferences_audio_frequency_label), 0, 0.5);

  preferences_audio_channel_label = gtk_label_new (_("Channels:"));
  gtk_widget_ref (preferences_audio_channel_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_audio_channel_label", preferences_audio_channel_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_audio_channel_label);
  gtk_table_attach (GTK_TABLE (preferences_audio_table), preferences_audio_channel_label, 0, 1, 4, 5,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (preferences_audio_channel_label), 0, 0.5);

  preferences_audio_spdif_checkbutton = gtk_check_button_new_with_label (_("Spdif output"));
  gtk_widget_ref (preferences_audio_spdif_checkbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_audio_spdif_checkbutton", preferences_audio_spdif_checkbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_audio_spdif_checkbutton);
  gtk_table_attach (GTK_TABLE (preferences_audio_table), preferences_audio_spdif_checkbutton, 0, 2, 5, 6,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  preferences_audio_output_combo = gtk_combo_new ();
  gtk_widget_ref (preferences_audio_output_combo);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_audio_output_combo", preferences_audio_output_combo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_audio_output_combo);
  gtk_table_attach (GTK_TABLE (preferences_audio_table), preferences_audio_output_combo, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  preferences_audio_output_combo_items = g_list_append (preferences_audio_output_combo_items, (gpointer) _("dsp"));
  preferences_audio_output_combo_items = g_list_append (preferences_audio_output_combo_items, (gpointer) _("alsa"));
  gtk_combo_set_popdown_strings (GTK_COMBO (preferences_audio_output_combo), preferences_audio_output_combo_items);
  g_list_free (preferences_audio_output_combo_items);

  preferences_audio_output_entry = GTK_COMBO (preferences_audio_output_combo)->entry;
  gtk_widget_ref (preferences_audio_output_entry);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_audio_output_entry", preferences_audio_output_entry,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_audio_output_entry);
  gtk_entry_set_text (GTK_ENTRY (preferences_audio_output_entry), _("dsp"));

  preferences_audio_device_combo = gtk_combo_new ();
  gtk_widget_ref (preferences_audio_device_combo);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_audio_device_combo", preferences_audio_device_combo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_audio_device_combo);
  gtk_table_attach (GTK_TABLE (preferences_audio_table), preferences_audio_device_combo, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  preferences_audio_device_combo_items = g_list_append (preferences_audio_device_combo_items, (gpointer) _("/dev/dsp"));
  gtk_combo_set_popdown_strings (GTK_COMBO (preferences_audio_device_combo), preferences_audio_device_combo_items);
  g_list_free (preferences_audio_device_combo_items);

  preferences_audio_device_entry = GTK_COMBO (preferences_audio_device_combo)->entry;
  gtk_widget_ref (preferences_audio_device_entry);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_audio_device_entry", preferences_audio_device_entry,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_audio_device_entry);
  gtk_entry_set_text (GTK_ENTRY (preferences_audio_device_entry), _("/dev/dsp"));

  preferences_audio = gtk_label_new (_("Audio"));
  gtk_widget_ref (preferences_audio);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_audio", preferences_audio,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_audio);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (preferences_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (preferences_notebook), 5), preferences_audio);

  preferences_playlist_table = gtk_table_new (3, 1, FALSE);
  gtk_widget_ref (preferences_playlist_table);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_playlist_table", preferences_playlist_table,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_playlist_table);
  gtk_container_add (GTK_CONTAINER (preferences_notebook), preferences_playlist_table);
  gtk_container_set_border_width (GTK_CONTAINER (preferences_playlist_table), 5);
  gtk_table_set_row_spacings (GTK_TABLE (preferences_playlist_table), 5);
  gtk_table_set_col_spacings (GTK_TABLE (preferences_playlist_table), 5);

  preferences_playlist_startup_checkbutton = gtk_check_button_new_with_label (_("Launch on startup"));
  gtk_widget_ref (preferences_playlist_startup_checkbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_playlist_startup_checkbutton", preferences_playlist_startup_checkbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_playlist_startup_checkbutton);
  gtk_table_attach (GTK_TABLE (preferences_playlist_table), preferences_playlist_startup_checkbutton, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  preferences_playlist_enqueue_checkbutton = gtk_check_button_new_with_label (_("Enqueue as default"));
  gtk_widget_ref (preferences_playlist_enqueue_checkbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_playlist_enqueue_checkbutton", preferences_playlist_enqueue_checkbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_playlist_enqueue_checkbutton);
  gtk_table_attach (GTK_TABLE (preferences_playlist_table), preferences_playlist_enqueue_checkbutton, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  preferences_playlist_loop_checkbutton = gtk_check_button_new_with_label (_("Loop on playlist end"));
  gtk_widget_ref (preferences_playlist_loop_checkbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_playlist_loop_checkbutton", preferences_playlist_loop_checkbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_playlist_loop_checkbutton);
  gtk_table_attach (GTK_TABLE (preferences_playlist_table), preferences_playlist_loop_checkbutton, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  preferences_playlist = gtk_label_new (_("Playlist"));
  gtk_widget_ref (preferences_playlist);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_playlist", preferences_playlist,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_playlist);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (preferences_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (preferences_notebook), 6), preferences_playlist);

  preferences_misc_table = gtk_table_new (1, 2, FALSE);
  gtk_widget_ref (preferences_misc_table);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_misc_table", preferences_misc_table,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_misc_table);
  gtk_container_add (GTK_CONTAINER (preferences_notebook), preferences_misc_table);
  gtk_container_set_border_width (GTK_CONTAINER (preferences_misc_table), 5);
  gtk_table_set_row_spacings (GTK_TABLE (preferences_misc_table), 5);
  gtk_table_set_col_spacings (GTK_TABLE (preferences_misc_table), 5);

  preferences_misc_associated_frame = gtk_frame_new (_("Types associated with vlc"));
  gtk_widget_ref (preferences_misc_associated_frame);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_misc_associated_frame", preferences_misc_associated_frame,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_misc_associated_frame);
  gtk_table_attach (GTK_TABLE (preferences_misc_table), preferences_misc_associated_frame, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  preferences_misc_associated_table = gtk_table_new (4, 1, FALSE);
  gtk_widget_ref (preferences_misc_associated_table);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_misc_associated_table", preferences_misc_associated_table,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_misc_associated_table);
  gtk_container_add (GTK_CONTAINER (preferences_misc_associated_frame), preferences_misc_associated_table);

  preferences_misc_associated_checkbutton = gtk_check_button_new_with_label (_("mpeg"));
  gtk_widget_ref (preferences_misc_associated_checkbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_misc_associated_checkbutton", preferences_misc_associated_checkbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_misc_associated_checkbutton);
  gtk_table_attach (GTK_TABLE (preferences_misc_associated_table), preferences_misc_associated_checkbutton, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  preferences_misc_mp2_checkbutton = gtk_check_button_new_with_label (_("mp2"));
  gtk_widget_ref (preferences_misc_mp2_checkbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_misc_mp2_checkbutton", preferences_misc_mp2_checkbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_misc_mp2_checkbutton);
  gtk_table_attach (GTK_TABLE (preferences_misc_associated_table), preferences_misc_mp2_checkbutton, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  preferences_misc_vob_checkbutton = gtk_check_button_new_with_label (_("vob"));
  gtk_widget_ref (preferences_misc_vob_checkbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_misc_vob_checkbutton", preferences_misc_vob_checkbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_misc_vob_checkbutton);
  gtk_table_attach (GTK_TABLE (preferences_misc_associated_table), preferences_misc_vob_checkbutton, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  preferences_misc_ts_checkbutton = gtk_check_button_new_with_label (_("ts"));
  gtk_widget_ref (preferences_misc_ts_checkbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_misc_ts_checkbutton", preferences_misc_ts_checkbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_misc_ts_checkbutton);
  gtk_table_attach (GTK_TABLE (preferences_misc_associated_table), preferences_misc_ts_checkbutton, 0, 1, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  preferences_misc_message_frame = gtk_frame_new (_("Messages"));
  gtk_widget_ref (preferences_misc_message_frame);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_misc_message_frame", preferences_misc_message_frame,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_misc_message_frame);
  gtk_table_attach (GTK_TABLE (preferences_misc_table), preferences_misc_message_frame, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  preferences_misc_messages_table = gtk_table_new (1, 2, FALSE);
  gtk_widget_ref (preferences_misc_messages_table);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_misc_messages_table", preferences_misc_messages_table,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_misc_messages_table);
  gtk_container_add (GTK_CONTAINER (preferences_misc_message_frame), preferences_misc_messages_table);

  preferences_misc_messages_label = gtk_label_new (_("Warning level: "));
  gtk_widget_ref (preferences_misc_messages_label);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_misc_messages_label", preferences_misc_messages_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_misc_messages_label);
  gtk_table_attach (GTK_TABLE (preferences_misc_messages_table), preferences_misc_messages_label, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (preferences_misc_messages_label), 0, 0.5);

  preferences_misc_messages_spinbutton_adj = gtk_adjustment_new (0, 0, 100, 1, 10, 10);
  preferences_misc_messages_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (preferences_misc_messages_spinbutton_adj), 1, 0);
  gtk_widget_ref (preferences_misc_messages_spinbutton);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_misc_messages_spinbutton", preferences_misc_messages_spinbutton,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_misc_messages_spinbutton);
  gtk_table_attach (GTK_TABLE (preferences_misc_messages_table), preferences_misc_messages_spinbutton, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND),
                    (GtkAttachOptions) (0), 0, 0);

  preferences_misc = gtk_label_new (_("Misc"));
  gtk_widget_ref (preferences_misc);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_misc", preferences_misc,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_misc);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (preferences_notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (preferences_notebook), 7), preferences_misc);

  dialog_action_area4 = GTK_DIALOG (intf_preferences)->action_area;
  gtk_object_set_data (GTK_OBJECT (intf_preferences), "dialog_action_area4", dialog_action_area4);
  gtk_widget_show (dialog_action_area4);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area4), 10);

  preferences_ok_button = gtk_button_new_with_label (_("Ok"));
  gtk_widget_ref (preferences_ok_button);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_ok_button", preferences_ok_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_ok_button);
  gtk_box_pack_start (GTK_BOX (dialog_action_area4), preferences_ok_button, TRUE, TRUE, 0);

  preferences_apply_button = gtk_button_new_with_label (_("Apply"));
  gtk_widget_ref (preferences_apply_button);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_apply_button", preferences_apply_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_apply_button);
  gtk_box_pack_start (GTK_BOX (dialog_action_area4), preferences_apply_button, TRUE, TRUE, 0);

  preferences_cancel_button = gtk_button_new_with_label (_("Cancel"));
  gtk_widget_ref (preferences_cancel_button);
  gtk_object_set_data_full (GTK_OBJECT (intf_preferences), "preferences_cancel_button", preferences_cancel_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences_cancel_button);
  gtk_box_pack_start (GTK_BOX (dialog_action_area4), preferences_cancel_button, TRUE, TRUE, 0);

  gtk_signal_connect (GTK_OBJECT (preferences_ok_button), "clicked",
                      GTK_SIGNAL_FUNC (GtkPreferencesOk),
                      "intf_preferences");
  gtk_signal_connect (GTK_OBJECT (preferences_apply_button), "clicked",
                      GTK_SIGNAL_FUNC (GtkPreferencesApply),
                      "intf_preferences");
  gtk_signal_connect (GTK_OBJECT (preferences_cancel_button), "clicked",
                      GTK_SIGNAL_FUNC (GtkPreferencesCancel),
                      "intf_preferences");

  return intf_preferences;
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
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area5), 10);

  messages_ok = gtk_button_new_with_label (_("OK"));
  gtk_widget_ref (messages_ok);
  gtk_object_set_data_full (GTK_OBJECT (intf_messages), "messages_ok", messages_ok,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (messages_ok);
  gtk_box_pack_start (GTK_BOX (dialog_action_area5), messages_ok, FALSE, TRUE, 0);
  GTK_WIDGET_SET_FLAGS (messages_ok, GTK_CAN_DEFAULT);

  gtk_signal_connect (GTK_OBJECT (intf_messages), "delete_event",
                      GTK_SIGNAL_FUNC (GtkMessagesDelete),
                      "intf_messages");
  gtk_signal_connect (GTK_OBJECT (messages_ok), "clicked",
                      GTK_SIGNAL_FUNC (GtkMessagesOk),
                      "intf_messages");

  gtk_widget_grab_default (messages_ok);
  return intf_messages;
}

