/* This file was created automatically by glade and fixed by bootstrap */

#include <vlc/vlc.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"

GtkWidget*
create_familiar (void)
{
  GtkWidget *familiar;
  GtkWidget *vbox;
  GtkWidget *toolbar;
  GtkWidget *tmp_toolbar_icon;
  GtkWidget *toolbar_open;
  GtkWidget *toolbar_playlist;
  GtkWidget *toolbar_preferences;
  GtkWidget *toolbar_rewind;
  GtkWidget *toolbar_pause;
  GtkWidget *toolbar_play;
  GtkWidget *toolbar_stop;
  GtkWidget *toolbar_forward;
  GtkWidget *toolbar_about;
  GtkWidget *slider_label;
  GtkWidget *slider;
  GtkWidget *notebook;
  GtkWidget *vbox1;
  GtkWidget *hbox1;
  GtkWidget *buttonMrlGo;
  GtkWidget *labelUrl;
  GtkWidget *mrl_combo;
  GList *mrl_combo_items = NULL;
  GtkWidget *mrl_entry;
  GtkWidget *mediabook;
  GtkWidget *scrolledwindow4;
  GtkWidget *viewport2;
  GtkWidget *clistmedia;
  GtkWidget *labelname;
  GtkWidget *labeltype;
  GtkWidget *label13;
  GtkWidget *vbox3;
  GSList *network_group = NULL;
  GtkWidget *network_multicast;
  GtkWidget *table1;
  GtkWidget *label21;
  GtkWidget *label22;
  GtkObject *network_multicast_port_adj;
  GtkWidget *network_multicast_port;
  GtkWidget *combo2;
  GtkWidget *network_multicast_address;
  GtkWidget *hseparator10;
  GtkWidget *hbox9;
  GtkWidget *network_http;
  GtkWidget *network_ftp;
  GtkWidget *network_mms;
  GtkWidget *hseparator11;
  GtkWidget *label20;
  GtkWidget *media2;
  GtkWidget *vbox4;
  GtkWidget *scrolledwindow5;
  GtkWidget *clistplaylist;
  GtkWidget *label25;
  GtkWidget *label26;
  GtkWidget *hbox11;
  GtkWidget *update_playlist;
  GtkWidget *playlist_del;
  GtkWidget *playlist_clear;
  GtkWidget *playlist;
  GtkWidget *vbox2;
  GtkWidget *cbautoplay;
  GtkWidget *hbox2;
  GtkWidget *buttonSave;
  GtkWidget *buttonApply;
  GtkWidget *buttonCancel;
  GtkWidget *preferences;
  GtkWidget *scrolledwindow3;
  GtkWidget *viewport1;
  GtkWidget *fixed2;
  GtkWidget *pixmap2;
  GtkWidget *label8;
  GtkWidget *label9;
  GtkWidget *label11;
  GtkWidget *label27;
  GtkWidget *label10;
  GtkWidget *about;

  familiar = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (familiar, "familiar");
  gtk_object_set_data (GTK_OBJECT (familiar), "familiar", familiar);
  gtk_window_set_title (GTK_WINDOW (familiar), _("VLC media player"));
  gtk_window_set_policy (GTK_WINDOW (familiar), TRUE, TRUE, TRUE);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_set_name (vbox, "vbox");
  gtk_widget_ref (vbox);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "vbox", vbox,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (familiar), vbox);

  toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);
  gtk_widget_set_name (toolbar, "toolbar");
  gtk_widget_ref (toolbar);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "toolbar", toolbar,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar);
  gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, FALSE, 5);
  gtk_widget_set_usize (toolbar, 240, 12);

  tmp_toolbar_icon = create_pixmap (familiar, "familiar-openb16x16.xpm");
  toolbar_open = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                "",
                                _("Open file"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_set_name (toolbar_open, "toolbar_open");
  gtk_widget_ref (toolbar_open);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "toolbar_open", toolbar_open,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_open);

  tmp_toolbar_icon = create_pixmap (familiar, "familiar-playlistb16x16.xpm");
  toolbar_playlist = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                "",
                                NULL, NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_set_name (toolbar_playlist, "toolbar_playlist");
  gtk_widget_ref (toolbar_playlist);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "toolbar_playlist", toolbar_playlist,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_playlist);

  tmp_toolbar_icon = create_pixmap (familiar, "familiar-preferencesb16x16.xpm");
  toolbar_preferences = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Preferences"),
                                _("Preferences"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_set_name (toolbar_preferences, "toolbar_preferences");
  gtk_widget_ref (toolbar_preferences);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "toolbar_preferences", toolbar_preferences,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_preferences);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  tmp_toolbar_icon = create_pixmap (familiar, "familiar-rewindb16x16.xpm");
  toolbar_rewind = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Rewind"),
                                _("Rewind stream"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_set_name (toolbar_rewind, "toolbar_rewind");
  gtk_widget_ref (toolbar_rewind);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "toolbar_rewind", toolbar_rewind,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_rewind);

  tmp_toolbar_icon = create_pixmap (familiar, "familiar-pauseb16x16.xpm");
  toolbar_pause = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Pause"),
                                _("Pause stream"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_set_name (toolbar_pause, "toolbar_pause");
  gtk_widget_ref (toolbar_pause);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "toolbar_pause", toolbar_pause,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_pause);

  tmp_toolbar_icon = create_pixmap (familiar, "familiar-playb16x16.xpm");
  toolbar_play = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Play"),
                                _("Play stream"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_set_name (toolbar_play, "toolbar_play");
  gtk_widget_ref (toolbar_play);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "toolbar_play", toolbar_play,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_play);

  tmp_toolbar_icon = create_pixmap (familiar, "familiar-stopb16x16.xpm");
  toolbar_stop = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Stop"),
                                _("Stop stream"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_set_name (toolbar_stop, "toolbar_stop");
  gtk_widget_ref (toolbar_stop);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "toolbar_stop", toolbar_stop,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_stop);

  tmp_toolbar_icon = create_pixmap (familiar, "familiar-forwardb16x16.xpm");
  toolbar_forward = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Forward"),
                                _("Forward stream"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_set_name (toolbar_forward, "toolbar_forward");
  gtk_widget_ref (toolbar_forward);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "toolbar_forward", toolbar_forward,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_forward);

  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  tmp_toolbar_icon = create_pixmap (familiar, "vlc16x16.xpm");
  toolbar_about = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("About"),
                                _("About"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_set_name (toolbar_about, "toolbar_about");
  gtk_widget_ref (toolbar_about);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "toolbar_about", toolbar_about,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_about);

  slider_label = gtk_label_new ("0:00:00");
  gtk_widget_set_name (slider_label, "slider_label");
  gtk_widget_ref (slider_label);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "slider_label", slider_label,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (slider_label);
  gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

  gtk_toolbar_append_widget (GTK_TOOLBAR (toolbar), slider_label, NULL, NULL);

  slider = gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, 100, 1, 6.25, 0)));
  gtk_widget_set_name (slider, "slider");
  gtk_widget_ref (slider);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "slider", slider,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (slider);
  gtk_box_pack_start (GTK_BOX (vbox), slider, FALSE, FALSE, 4);
  gtk_scale_set_draw_value (GTK_SCALE (slider), FALSE);
  gtk_scale_set_value_pos (GTK_SCALE (slider), GTK_POS_RIGHT);
  gtk_scale_set_digits (GTK_SCALE (slider), 3);

  notebook = gtk_notebook_new ();
  gtk_widget_set_name (notebook, "notebook");
  gtk_widget_ref (notebook);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "notebook", notebook,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (notebook);
  gtk_box_pack_end (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_set_name (vbox1, "vbox1");
  gtk_widget_ref (vbox1);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "vbox1", vbox1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox1);
  gtk_container_add (GTK_CONTAINER (notebook), vbox1);

  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_set_name (hbox1, "hbox1");
  gtk_widget_ref (hbox1);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "hbox1", hbox1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox1);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, TRUE, 0);

  buttonMrlGo = gtk_button_new_with_label (_("Add"));
  gtk_widget_set_name (buttonMrlGo, "buttonMrlGo");
  gtk_widget_ref (buttonMrlGo);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "buttonMrlGo", buttonMrlGo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (buttonMrlGo);
  gtk_box_pack_start (GTK_BOX (hbox1), buttonMrlGo, FALSE, FALSE, 0);

  labelUrl = gtk_label_new (_("MRL :"));
  gtk_widget_set_name (labelUrl, "labelUrl");
  gtk_widget_ref (labelUrl);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "labelUrl", labelUrl,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (labelUrl);
  gtk_box_pack_start (GTK_BOX (hbox1), labelUrl, FALSE, FALSE, 2);

  mrl_combo = gtk_combo_new ();
  gtk_widget_set_name (mrl_combo, "mrl_combo");
  gtk_widget_ref (mrl_combo);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "mrl_combo", mrl_combo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (mrl_combo);
  gtk_box_pack_start (GTK_BOX (hbox1), mrl_combo, TRUE, TRUE, 3);
  mrl_combo_items = g_list_append (mrl_combo_items, (gpointer) "file://");
  mrl_combo_items = g_list_append (mrl_combo_items, (gpointer) "ftp://");
  mrl_combo_items = g_list_append (mrl_combo_items, (gpointer) "http://");
  mrl_combo_items = g_list_append (mrl_combo_items, (gpointer) "udp://@:1234");
  mrl_combo_items = g_list_append (mrl_combo_items, (gpointer) "udp6://@:1234");
  mrl_combo_items = g_list_append (mrl_combo_items, (gpointer) "rtp://");
  mrl_combo_items = g_list_append (mrl_combo_items, (gpointer) "rtp6://");
  gtk_combo_set_popdown_strings (GTK_COMBO (mrl_combo), mrl_combo_items);
  g_list_free (mrl_combo_items);

  mrl_entry = GTK_COMBO (mrl_combo)->entry;
  gtk_widget_set_name (mrl_entry, "mrl_entry");
  gtk_widget_ref (mrl_entry);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "mrl_entry", mrl_entry,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (mrl_entry);
  gtk_entry_set_text (GTK_ENTRY (mrl_entry), "file://");

  mediabook = gtk_notebook_new ();
  gtk_widget_set_name (mediabook, "mediabook");
  gtk_widget_ref (mediabook);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "mediabook", mediabook,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (mediabook);
  gtk_box_pack_start (GTK_BOX (vbox1), mediabook, TRUE, TRUE, 0);

  scrolledwindow4 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_name (scrolledwindow4, "scrolledwindow4");
  gtk_widget_ref (scrolledwindow4);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "scrolledwindow4", scrolledwindow4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow4);
  gtk_container_add (GTK_CONTAINER (mediabook), scrolledwindow4);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow4), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  viewport2 = gtk_viewport_new (NULL, NULL);
  gtk_widget_set_name (viewport2, "viewport2");
  gtk_widget_ref (viewport2);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "viewport2", viewport2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (viewport2);
  gtk_container_add (GTK_CONTAINER (scrolledwindow4), viewport2);

  clistmedia = gtk_clist_new (2);
  gtk_widget_set_name (clistmedia, "clistmedia");
  gtk_widget_ref (clistmedia);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "clistmedia", clistmedia,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (clistmedia);
  gtk_container_add (GTK_CONTAINER (viewport2), clistmedia);
  gtk_clist_set_column_width (GTK_CLIST (clistmedia), 0, 129);
  gtk_clist_set_column_width (GTK_CLIST (clistmedia), 1, 80);
  gtk_clist_column_titles_show (GTK_CLIST (clistmedia));

  labelname = gtk_label_new (_("Name"));
  gtk_widget_set_name (labelname, "labelname");
  gtk_widget_ref (labelname);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "labelname", labelname,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (labelname);
  gtk_clist_set_column_widget (GTK_CLIST (clistmedia), 0, labelname);

  labeltype = gtk_label_new (_("Type"));
  gtk_widget_set_name (labeltype, "labeltype");
  gtk_widget_ref (labeltype);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "labeltype", labeltype,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (labeltype);
  gtk_clist_set_column_widget (GTK_CLIST (clistmedia), 1, labeltype);

  label13 = gtk_label_new (_("File"));
  gtk_widget_set_name (label13, "label13");
  gtk_widget_ref (label13);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "label13", label13,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label13);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (mediabook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (mediabook), 0), label13);

  vbox3 = gtk_vbox_new (FALSE, 2);
  gtk_widget_set_name (vbox3, "vbox3");
  gtk_widget_ref (vbox3);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "vbox3", vbox3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox3);
  gtk_container_add (GTK_CONTAINER (mediabook), vbox3);

  network_multicast = gtk_radio_button_new_with_label (network_group, _("UDP/RTP (Adress when Multicast)"));
  network_group = gtk_radio_button_group (GTK_RADIO_BUTTON (network_multicast));
  gtk_widget_set_name (network_multicast, "network_multicast");
  gtk_widget_ref (network_multicast);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "network_multicast", network_multicast,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_multicast);
  gtk_box_pack_start (GTK_BOX (vbox3), network_multicast, FALSE, FALSE, 0);

  table1 = gtk_table_new (2, 2, FALSE);
  gtk_widget_set_name (table1, "table1");
  gtk_widget_ref (table1);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "table1", table1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table1);
  gtk_box_pack_start (GTK_BOX (vbox3), table1, FALSE, TRUE, 0);
  gtk_table_set_col_spacings (GTK_TABLE (table1), 5);

  label21 = gtk_label_new (_("Address"));
  gtk_widget_set_name (label21, "label21");
  gtk_widget_ref (label21);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "label21", label21,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label21);
  gtk_table_attach (GTK_TABLE (table1), label21, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label21), 0, 0.5);

  label22 = gtk_label_new (_("Port"));
  gtk_widget_set_name (label22, "label22");
  gtk_widget_ref (label22);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "label22", label22,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label22);
  gtk_table_attach (GTK_TABLE (table1), label22, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label22), 0, 0.5);

  network_multicast_port_adj = gtk_adjustment_new (1234, 1, 65536, 1, 10, 10);
  network_multicast_port = gtk_spin_button_new (GTK_ADJUSTMENT (network_multicast_port_adj), 1, 0);
  gtk_widget_set_name (network_multicast_port, "network_multicast_port");
  gtk_widget_ref (network_multicast_port);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "network_multicast_port", network_multicast_port,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_multicast_port);
  gtk_table_attach (GTK_TABLE (table1), network_multicast_port, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  combo2 = gtk_combo_new ();
  gtk_widget_set_name (combo2, "combo2");
  gtk_widget_ref (combo2);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "combo2", combo2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (combo2);
  gtk_table_attach (GTK_TABLE (table1), combo2, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  network_multicast_address = GTK_COMBO (combo2)->entry;
  gtk_widget_set_name (network_multicast_address, "network_multicast_address");
  gtk_widget_ref (network_multicast_address);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "network_multicast_address", network_multicast_address,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_multicast_address);

  hseparator10 = gtk_hseparator_new ();
  gtk_widget_set_name (hseparator10, "hseparator10");
  gtk_widget_ref (hseparator10);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "hseparator10", hseparator10,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hseparator10);
  gtk_box_pack_start (GTK_BOX (vbox3), hseparator10, FALSE, TRUE, 0);

  hbox9 = gtk_hbox_new (TRUE, 0);
  gtk_widget_set_name (hbox9, "hbox9");
  gtk_widget_ref (hbox9);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "hbox9", hbox9,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox9);
  gtk_box_pack_start (GTK_BOX (vbox3), hbox9, FALSE, TRUE, 0);

  network_http = gtk_radio_button_new_with_label (network_group, _("HTTP"));
  network_group = gtk_radio_button_group (GTK_RADIO_BUTTON (network_http));
  gtk_widget_set_name (network_http, "network_http");
  gtk_widget_ref (network_http);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "network_http", network_http,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_http);
  gtk_box_pack_start (GTK_BOX (hbox9), network_http, FALSE, TRUE, 0);
  gtk_widget_set_sensitive (network_http, FALSE);

  network_ftp = gtk_radio_button_new_with_label (network_group, _("FTP"));
  network_group = gtk_radio_button_group (GTK_RADIO_BUTTON (network_ftp));
  gtk_widget_set_name (network_ftp, "network_ftp");
  gtk_widget_ref (network_ftp);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "network_ftp", network_ftp,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_ftp);
  gtk_box_pack_start (GTK_BOX (hbox9), network_ftp, FALSE, TRUE, 0);
  gtk_widget_set_sensitive (network_ftp, FALSE);

  network_mms = gtk_radio_button_new_with_label (network_group, _("MMS"));
  network_group = gtk_radio_button_group (GTK_RADIO_BUTTON (network_mms));
  gtk_widget_set_name (network_mms, "network_mms");
  gtk_widget_ref (network_mms);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "network_mms", network_mms,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (network_mms);
  gtk_box_pack_start (GTK_BOX (hbox9), network_mms, FALSE, TRUE, 0);
  gtk_widget_set_sensitive (network_mms, FALSE);

  hseparator11 = gtk_hseparator_new ();
  gtk_widget_set_name (hseparator11, "hseparator11");
  gtk_widget_ref (hseparator11);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "hseparator11", hseparator11,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hseparator11);
  gtk_box_pack_start (GTK_BOX (vbox3), hseparator11, TRUE, TRUE, 0);

  label20 = gtk_label_new (_("Network"));
  gtk_widget_set_name (label20, "label20");
  gtk_widget_ref (label20);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "label20", label20,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label20);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (mediabook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (mediabook), 1), label20);

  media2 = gtk_label_new (_("Media"));
  gtk_widget_set_name (media2, "media2");
  gtk_widget_ref (media2);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "media2", media2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (media2);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 0), media2);

  vbox4 = gtk_vbox_new (FALSE, 0);
  gtk_widget_set_name (vbox4, "vbox4");
  gtk_widget_ref (vbox4);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "vbox4", vbox4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox4);
  gtk_container_add (GTK_CONTAINER (notebook), vbox4);

  scrolledwindow5 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_name (scrolledwindow5, "scrolledwindow5");
  gtk_widget_ref (scrolledwindow5);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "scrolledwindow5", scrolledwindow5,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow5);
  gtk_box_pack_start (GTK_BOX (vbox4), scrolledwindow5, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow5), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  clistplaylist = gtk_clist_new (2);
  gtk_widget_set_name (clistplaylist, "clistplaylist");
  gtk_widget_ref (clistplaylist);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "clistplaylist", clistplaylist,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (clistplaylist);
  gtk_container_add (GTK_CONTAINER (scrolledwindow5), clistplaylist);
  gtk_clist_set_column_width (GTK_CLIST (clistplaylist), 0, 140);
  gtk_clist_set_column_width (GTK_CLIST (clistplaylist), 1, 80);
  gtk_clist_column_titles_show (GTK_CLIST (clistplaylist));

  label25 = gtk_label_new (_("MRL"));
  gtk_widget_set_name (label25, "label25");
  gtk_widget_ref (label25);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "label25", label25,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label25);
  gtk_clist_set_column_widget (GTK_CLIST (clistplaylist), 0, label25);
  gtk_label_set_justify (GTK_LABEL (label25), GTK_JUSTIFY_LEFT);

  label26 = gtk_label_new (_("Time"));
  gtk_widget_set_name (label26, "label26");
  gtk_widget_ref (label26);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "label26", label26,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label26);
  gtk_clist_set_column_widget (GTK_CLIST (clistplaylist), 1, label26);
  gtk_label_set_justify (GTK_LABEL (label26), GTK_JUSTIFY_RIGHT);

  hbox11 = gtk_hbox_new (TRUE, 0);
  gtk_widget_set_name (hbox11, "hbox11");
  gtk_widget_ref (hbox11);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "hbox11", hbox11,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox11);
  gtk_box_pack_start (GTK_BOX (vbox4), hbox11, FALSE, FALSE, 2);

  update_playlist = gtk_button_new_with_label (_("Update"));
  gtk_widget_set_name (update_playlist, "update_playlist");
  gtk_widget_ref (update_playlist);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "update_playlist", update_playlist,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (update_playlist);
  gtk_box_pack_start (GTK_BOX (hbox11), update_playlist, FALSE, FALSE, 0);

  playlist_del = gtk_button_new_with_label (_(" Del "));
  gtk_widget_set_name (playlist_del, "playlist_del");
  gtk_widget_ref (playlist_del);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "playlist_del", playlist_del,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_del);
  gtk_box_pack_start (GTK_BOX (hbox11), playlist_del, FALSE, FALSE, 5);

  playlist_clear = gtk_button_new_with_label (_(" Clear "));
  gtk_widget_set_name (playlist_clear, "playlist_clear");
  gtk_widget_ref (playlist_clear);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "playlist_clear", playlist_clear,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist_clear);
  gtk_box_pack_start (GTK_BOX (hbox11), playlist_clear, FALSE, FALSE, 5);

  playlist = gtk_label_new (_("Playlist"));
  gtk_widget_set_name (playlist, "playlist");
  gtk_widget_ref (playlist);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "playlist", playlist,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (playlist);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 1), playlist);

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_widget_set_name (vbox2, "vbox2");
  gtk_widget_ref (vbox2);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "vbox2", vbox2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox2);
  gtk_container_add (GTK_CONTAINER (notebook), vbox2);

  cbautoplay = gtk_check_button_new_with_label (_("Automatically play file"));
  gtk_widget_set_name (cbautoplay, "cbautoplay");
  gtk_widget_ref (cbautoplay);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "cbautoplay", cbautoplay,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cbautoplay);
  gtk_box_pack_start (GTK_BOX (vbox2), cbautoplay, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cbautoplay), TRUE);

  hbox2 = gtk_hbox_new (TRUE, 0);
  gtk_widget_set_name (hbox2, "hbox2");
  gtk_widget_ref (hbox2);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "hbox2", hbox2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox2);
  gtk_box_pack_end (GTK_BOX (vbox2), hbox2, FALSE, FALSE, 2);

  buttonSave = gtk_button_new_with_label (_(" Save "));
  gtk_widget_set_name (buttonSave, "buttonSave");
  gtk_widget_ref (buttonSave);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "buttonSave", buttonSave,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (buttonSave);
  gtk_box_pack_start (GTK_BOX (hbox2), buttonSave, FALSE, FALSE, 0);

  buttonApply = gtk_button_new_with_label (_(" Apply "));
  gtk_widget_set_name (buttonApply, "buttonApply");
  gtk_widget_ref (buttonApply);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "buttonApply", buttonApply,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (buttonApply);
  gtk_box_pack_start (GTK_BOX (hbox2), buttonApply, FALSE, FALSE, 0);

  buttonCancel = gtk_button_new_with_label (_(" Cancel "));
  gtk_widget_set_name (buttonCancel, "buttonCancel");
  gtk_widget_ref (buttonCancel);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "buttonCancel", buttonCancel,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (buttonCancel);
  gtk_box_pack_end (GTK_BOX (hbox2), buttonCancel, FALSE, FALSE, 0);

  preferences = gtk_label_new (_("Preference"));
  gtk_widget_set_name (preferences, "preferences");
  gtk_widget_ref (preferences);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "preferences", preferences,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 2), preferences);

  scrolledwindow3 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_name (scrolledwindow3, "scrolledwindow3");
  gtk_widget_ref (scrolledwindow3);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "scrolledwindow3", scrolledwindow3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow3);
  gtk_container_add (GTK_CONTAINER (notebook), scrolledwindow3);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow3), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  viewport1 = gtk_viewport_new (NULL, NULL);
  gtk_widget_set_name (viewport1, "viewport1");
  gtk_widget_ref (viewport1);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "viewport1", viewport1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (viewport1);
  gtk_container_add (GTK_CONTAINER (scrolledwindow3), viewport1);
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport1), GTK_SHADOW_NONE);

  fixed2 = gtk_fixed_new ();
  gtk_widget_set_name (fixed2, "fixed2");
  gtk_widget_ref (fixed2);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "fixed2", fixed2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixed2);
  gtk_container_add (GTK_CONTAINER (viewport1), fixed2);

  pixmap2 = create_pixmap (familiar, "vlc32x32.xpm");
  gtk_widget_set_name (pixmap2, "pixmap2");
  gtk_widget_ref (pixmap2);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "pixmap2", pixmap2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (pixmap2);
  gtk_fixed_put (GTK_FIXED (fixed2), pixmap2, 8, 0);
  gtk_widget_set_uposition (pixmap2, 8, 0);
  gtk_widget_set_usize (pixmap2, 50, 50);

  label8 = gtk_label_new (_("(c) 1996-2003 the VideoLAN team"));
  gtk_widget_set_name (label8, "label8");
  gtk_widget_ref (label8);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "label8", label8,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label8);
  gtk_fixed_put (GTK_FIXED (fixed2), label8, 16, 56);
  gtk_widget_set_uposition (label8, 16, 56);
  gtk_widget_set_usize (label8, 200, 18);

  label9 = gtk_label_new (_("Authors: The VideoLAN Team, http://www.videolan.org"));
  gtk_widget_set_name (label9, "label9");
  gtk_widget_ref (label9);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "label9", label9,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label9);
  gtk_fixed_put (GTK_FIXED (fixed2), label9, 16, 80);
  gtk_widget_set_uposition (label9, 16, 80);
  gtk_widget_set_usize (label9, 200, 40);
  gtk_label_set_line_wrap (GTK_LABEL (label9), TRUE);

  label11 = gtk_label_new (_("VLC media player"));
  gtk_widget_set_name (label11, "label11");
  gtk_widget_ref (label11);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "label11", label11,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label11);
  gtk_fixed_put (GTK_FIXED (fixed2), label11, 64, 8);
  gtk_widget_set_uposition (label11, 64, 8);
  gtk_widget_set_usize (label11, 120, 40);
  gtk_label_set_line_wrap (GTK_LABEL (label11), TRUE);

  label27 = gtk_label_new ("http://www.videolan.org");
  gtk_widget_set_name (label27, "label27");
  gtk_widget_ref (label27);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "label27", label27,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label27);
  gtk_fixed_put (GTK_FIXED (fixed2), label27, 16, 200);
  gtk_widget_set_uposition (label27, 16, 200);
  gtk_widget_set_usize (label27, 208, 16);

  label10 = gtk_label_new (_("The VideoLAN Client is a MPEG, MPEG 2, MP3, DivX player, that accepts input from local or network sources."));
  gtk_widget_set_name (label10, "label10");
  gtk_widget_ref (label10);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "label10", label10,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label10);
  gtk_fixed_put (GTK_FIXED (fixed2), label10, 16, 128);
  gtk_widget_set_uposition (label10, 16, 128);
  gtk_widget_set_usize (label10, 200, 70);
  gtk_label_set_justify (GTK_LABEL (label10), GTK_JUSTIFY_LEFT);
  gtk_label_set_line_wrap (GTK_LABEL (label10), TRUE);

  about = gtk_label_new (_("About"));
  gtk_widget_set_name (about, "about");
  gtk_widget_ref (about);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "about", about,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (about);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 3), about);

  gtk_signal_connect (GTK_OBJECT (familiar), "delete_event",
                      GTK_SIGNAL_FUNC (on_familiar_delete_event),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_open), "clicked",
                      GTK_SIGNAL_FUNC (on_toolbar_open_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_playlist), "clicked",
                      GTK_SIGNAL_FUNC (on_toolbar_playlist_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_preferences), "clicked",
                      GTK_SIGNAL_FUNC (on_toolbar_preferences_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_rewind), "clicked",
                      GTK_SIGNAL_FUNC (on_toolbar_rewind_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_pause), "clicked",
                      GTK_SIGNAL_FUNC (on_toolbar_pause_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_play), "clicked",
                      GTK_SIGNAL_FUNC (on_toolbar_play_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_stop), "clicked",
                      GTK_SIGNAL_FUNC (on_toolbar_stop_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_forward), "clicked",
                      GTK_SIGNAL_FUNC (on_toolbar_forward_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_about), "clicked",
                      GTK_SIGNAL_FUNC (on_toolbar_about_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (slider), "button_release_event",
                      GTK_SIGNAL_FUNC (FamiliarSliderRelease),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (slider), "button_press_event",
                      GTK_SIGNAL_FUNC (FamiliarSliderPress),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (buttonMrlGo), "clicked",
                      GTK_SIGNAL_FUNC (FamiliarMrlGo),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (mrl_entry), "changed",
                      GTK_SIGNAL_FUNC (on_comboURL_entry_changed),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (clistmedia), "select_row",
                      GTK_SIGNAL_FUNC (on_clistmedia_select_row),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (clistmedia), "click_column",
                      GTK_SIGNAL_FUNC (on_clistmedia_click_column),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (network_multicast), "toggled",
                      GTK_SIGNAL_FUNC (on_network_multicast_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (network_multicast_port), "changed",
                      GTK_SIGNAL_FUNC (on_network_multicast_port_changed),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (network_multicast_address), "changed",
                      GTK_SIGNAL_FUNC (on_network_multicast_address_changed),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (network_http), "toggled",
                      GTK_SIGNAL_FUNC (on_network_http_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (network_ftp), "toggled",
                      GTK_SIGNAL_FUNC (on_network_ftp_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (network_mms), "toggled",
                      GTK_SIGNAL_FUNC (on_network_mms_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (clistplaylist), "event",
                      GTK_SIGNAL_FUNC (FamiliarPlaylistEvent),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (update_playlist), "clicked",
                      GTK_SIGNAL_FUNC (FamiliarPlaylistUpdate),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (playlist_del), "clicked",
                      GTK_SIGNAL_FUNC (FamiliarPlaylistDel),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (playlist_clear), "clicked",
                      GTK_SIGNAL_FUNC (FamiliarPlaylistClear),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (cbautoplay), "toggled",
                      GTK_SIGNAL_FUNC (on_cbautoplay_toggled),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (buttonApply), "clicked",
                      GTK_SIGNAL_FUNC (FamiliarPreferencesApply),
                      NULL);

  return familiar;
}

