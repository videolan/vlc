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
  GtkWidget *fixedMedia;
  GtkWidget *labelUrl;
  GtkWidget *scrolledwindow1;
  GtkWidget *clistmedia;
  GtkWidget *labelname;
  GtkWidget *labeltype;
  GtkWidget *labelsize;
  GtkWidget *labeluid;
  GtkWidget *labelgid;
  GtkWidget *comboURL;
  GList *comboURL_items = NULL;
  GtkWidget *comboURL_entry;
  GtkWidget *media;
  GtkWidget *fixedPreferences;
  GtkWidget *buttonSave;
  GtkWidget *buttonApply;
  GtkWidget *buttonCancel;
  GtkWidget *cbautoplay;
  GtkWidget *preferences;
  GtkWidget *fixedAbout;
  GtkWidget *logo;
  GtkWidget *labelVlc;
  GtkWidget *labelCopyright;
  GtkWidget *labelAuthors;
  GtkWidget *labelAbout;
  GtkWidget *about;

  familiar = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (familiar, "familiar");
  gtk_object_set_data (GTK_OBJECT (familiar), "familiar", familiar);
  gtk_widget_set_usize (familiar, 240, 320);
  gtk_window_set_title (GTK_WINDOW (familiar), _("vlc (familiar)"));
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
  gtk_box_pack_start (GTK_BOX (vbox), toolbar, TRUE, TRUE, 0);
  gtk_widget_set_usize (toolbar, 112, 16);

  tmp_toolbar_icon = create_pixmap (familiar, "familiar-openb16x16.xpm");
  toolbar_open = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
                                GTK_TOOLBAR_CHILD_BUTTON,
                                NULL,
                                _("Open"),
                                _("Open file"), NULL,
                                tmp_toolbar_icon, NULL, NULL);
  gtk_widget_set_name (toolbar_open, "toolbar_open");
  gtk_widget_ref (toolbar_open);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "toolbar_open", toolbar_open,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (toolbar_open);

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
  gtk_box_pack_start (GTK_BOX (vbox), slider, TRUE, FALSE, 0);
  gtk_scale_set_draw_value (GTK_SCALE (slider), FALSE);
  gtk_scale_set_value_pos (GTK_SCALE (slider), GTK_POS_RIGHT);
  gtk_scale_set_digits (GTK_SCALE (slider), 3);

  notebook = gtk_notebook_new ();
  gtk_widget_set_name (notebook, "notebook");
  gtk_widget_ref (notebook);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "notebook", notebook,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (notebook);
  gtk_box_pack_start (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);

  fixedMedia = gtk_fixed_new ();
  gtk_widget_set_name (fixedMedia, "fixedMedia");
  gtk_widget_ref (fixedMedia);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "fixedMedia", fixedMedia,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixedMedia);
  gtk_container_add (GTK_CONTAINER (notebook), fixedMedia);

  labelUrl = gtk_label_new (_("URL:"));
  gtk_widget_set_name (labelUrl, "labelUrl");
  gtk_widget_ref (labelUrl);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "labelUrl", labelUrl,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (labelUrl);
  gtk_fixed_put (GTK_FIXED (fixedMedia), labelUrl, 4, 8);
  gtk_widget_set_uposition (labelUrl, 4, 8);
  gtk_widget_set_usize (labelUrl, 38, 18);

  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_name (scrolledwindow1, "scrolledwindow1");
  gtk_widget_ref (scrolledwindow1);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "scrolledwindow1", scrolledwindow1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow1);
  gtk_fixed_put (GTK_FIXED (fixedMedia), scrolledwindow1, 0, 32);
  gtk_widget_set_uposition (scrolledwindow1, 0, 32);
  gtk_widget_set_usize (scrolledwindow1, 240, 208);

  clistmedia = gtk_clist_new (5);
  gtk_widget_set_name (clistmedia, "clistmedia");
  gtk_widget_ref (clistmedia);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "clistmedia", clistmedia,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (clistmedia);
  gtk_container_add (GTK_CONTAINER (scrolledwindow1), clistmedia);
  gtk_clist_set_column_width (GTK_CLIST (clistmedia), 0, 123);
  gtk_clist_set_column_width (GTK_CLIST (clistmedia), 1, 80);
  gtk_clist_set_column_width (GTK_CLIST (clistmedia), 2, 80);
  gtk_clist_set_column_width (GTK_CLIST (clistmedia), 3, 80);
  gtk_clist_set_column_width (GTK_CLIST (clistmedia), 4, 80);
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

  labelsize = gtk_label_new (_("Size"));
  gtk_widget_set_name (labelsize, "labelsize");
  gtk_widget_ref (labelsize);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "labelsize", labelsize,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (labelsize);
  gtk_clist_set_column_widget (GTK_CLIST (clistmedia), 2, labelsize);

  labeluid = gtk_label_new (_("User"));
  gtk_widget_set_name (labeluid, "labeluid");
  gtk_widget_ref (labeluid);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "labeluid", labeluid,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (labeluid);
  gtk_clist_set_column_widget (GTK_CLIST (clistmedia), 3, labeluid);

  labelgid = gtk_label_new (_("Group"));
  gtk_widget_set_name (labelgid, "labelgid");
  gtk_widget_ref (labelgid);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "labelgid", labelgid,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (labelgid);
  gtk_clist_set_column_widget (GTK_CLIST (clistmedia), 4, labelgid);

  comboURL = gtk_combo_new ();
  gtk_widget_set_name (comboURL, "comboURL");
  gtk_widget_ref (comboURL);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "comboURL", comboURL,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (comboURL);
  gtk_fixed_put (GTK_FIXED (fixedMedia), comboURL, 40, 4);
  gtk_widget_set_uposition (comboURL, 40, 4);
  gtk_widget_set_usize (comboURL, 185, 24);
  comboURL_items = g_list_append (comboURL_items, (gpointer) "file://");
  comboURL_items = g_list_append (comboURL_items, (gpointer) "ftp://");
  comboURL_items = g_list_append (comboURL_items, (gpointer) "http://");
  comboURL_items = g_list_append (comboURL_items, (gpointer) "udp://:1234");
  comboURL_items = g_list_append (comboURL_items, (gpointer) _("udp6://:1234"));
  comboURL_items = g_list_append (comboURL_items, (gpointer) "rtp://:1234");
  comboURL_items = g_list_append (comboURL_items, (gpointer) _("rtp6://:1234"));
  gtk_combo_set_popdown_strings (GTK_COMBO (comboURL), comboURL_items);
  g_list_free (comboURL_items);

  comboURL_entry = GTK_COMBO (comboURL)->entry;
  gtk_widget_set_name (comboURL_entry, "comboURL_entry");
  gtk_widget_ref (comboURL_entry);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "comboURL_entry", comboURL_entry,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (comboURL_entry);
  gtk_entry_set_text (GTK_ENTRY (comboURL_entry), "file://");

  media = gtk_label_new (_("Media"));
  gtk_widget_set_name (media, "media");
  gtk_widget_ref (media);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "media", media,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (media);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 0), media);

  fixedPreferences = gtk_fixed_new ();
  gtk_widget_set_name (fixedPreferences, "fixedPreferences");
  gtk_widget_ref (fixedPreferences);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "fixedPreferences", fixedPreferences,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixedPreferences);
  gtk_container_add (GTK_CONTAINER (notebook), fixedPreferences);

  buttonSave = gtk_button_new_with_label (_("Save"));
  gtk_widget_set_name (buttonSave, "buttonSave");
  gtk_widget_ref (buttonSave);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "buttonSave", buttonSave,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (buttonSave);
  gtk_fixed_put (GTK_FIXED (fixedPreferences), buttonSave, 8, 216);
  gtk_widget_set_uposition (buttonSave, 8, 216);
  gtk_widget_set_usize (buttonSave, 54, 24);

  buttonApply = gtk_button_new_with_label (_("Apply"));
  gtk_widget_set_name (buttonApply, "buttonApply");
  gtk_widget_ref (buttonApply);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "buttonApply", buttonApply,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (buttonApply);
  gtk_fixed_put (GTK_FIXED (fixedPreferences), buttonApply, 64, 216);
  gtk_widget_set_uposition (buttonApply, 64, 216);
  gtk_widget_set_usize (buttonApply, 54, 24);

  buttonCancel = gtk_button_new_with_label (_("Cancel"));
  gtk_widget_set_name (buttonCancel, "buttonCancel");
  gtk_widget_ref (buttonCancel);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "buttonCancel", buttonCancel,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (buttonCancel);
  gtk_fixed_put (GTK_FIXED (fixedPreferences), buttonCancel, 176, 216);
  gtk_widget_set_uposition (buttonCancel, 176, 216);
  gtk_widget_set_usize (buttonCancel, 54, 24);

  cbautoplay = gtk_check_button_new_with_label (_("Automatically play file."));
  gtk_widget_set_name (cbautoplay, "cbautoplay");
  gtk_widget_ref (cbautoplay);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "cbautoplay", cbautoplay,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (cbautoplay);
  gtk_fixed_put (GTK_FIXED (fixedPreferences), cbautoplay, 8, 8);
  gtk_widget_set_uposition (cbautoplay, 8, 8);
  gtk_widget_set_usize (cbautoplay, 216, 24);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cbautoplay), TRUE);

  preferences = gtk_label_new (_("Preference"));
  gtk_widget_set_name (preferences, "preferences");
  gtk_widget_ref (preferences);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "preferences", preferences,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (preferences);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 1), preferences);

  fixedAbout = gtk_fixed_new ();
  gtk_widget_set_name (fixedAbout, "fixedAbout");
  gtk_widget_ref (fixedAbout);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "fixedAbout", fixedAbout,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (fixedAbout);
  gtk_container_add (GTK_CONTAINER (notebook), fixedAbout);

  logo = create_pixmap (familiar, "vlc32x32.xpm");
  gtk_widget_set_name (logo, "logo");
  gtk_widget_ref (logo);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "logo", logo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (logo);
  gtk_fixed_put (GTK_FIXED (fixedAbout), logo, 8, 0);
  gtk_widget_set_uposition (logo, 8, 0);
  gtk_widget_set_usize (logo, 50, 50);

  labelVlc = gtk_label_new (_("VideoLAN Client\n for familiar Linux"));
  gtk_widget_set_name (labelVlc, "labelVlc");
  gtk_widget_ref (labelVlc);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "labelVlc", labelVlc,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (labelVlc);
  gtk_fixed_put (GTK_FIXED (fixedAbout), labelVlc, 64, 8);
  gtk_widget_set_uposition (labelVlc, 64, 8);
  gtk_widget_set_usize (labelVlc, 120, 40);
  gtk_label_set_line_wrap (GTK_LABEL (labelVlc), TRUE);

  labelCopyright = gtk_label_new (_("(c) 2002, the VideoLAN Team"));
  gtk_widget_set_name (labelCopyright, "labelCopyright");
  gtk_widget_ref (labelCopyright);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "labelCopyright", labelCopyright,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (labelCopyright);
  gtk_fixed_put (GTK_FIXED (fixedAbout), labelCopyright, 16, 56);
  gtk_widget_set_uposition (labelCopyright, 16, 56);
  gtk_widget_set_usize (labelCopyright, 200, 18);

  labelAuthors = gtk_label_new (_("Authors: The VideoLAN Team, http://www.videolan.org"));
  gtk_widget_set_name (labelAuthors, "labelAuthors");
  gtk_widget_ref (labelAuthors);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "labelAuthors", labelAuthors,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (labelAuthors);
  gtk_fixed_put (GTK_FIXED (fixedAbout), labelAuthors, 16, 80);
  gtk_widget_set_uposition (labelAuthors, 16, 80);
  gtk_widget_set_usize (labelAuthors, 200, 40);
  gtk_label_set_line_wrap (GTK_LABEL (labelAuthors), TRUE);

  labelAbout = gtk_label_new (_("The VideoLAN Client is a MPEG, MPEG 2, MP3, DivX player, that accepts input from local or network sources."));
  gtk_widget_set_name (labelAbout, "labelAbout");
  gtk_widget_ref (labelAbout);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "labelAbout", labelAbout,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (labelAbout);
  gtk_fixed_put (GTK_FIXED (fixedAbout), labelAbout, 16, 128);
  gtk_widget_set_uposition (labelAbout, 16, 128);
  gtk_widget_set_usize (labelAbout, 200, 70);
  gtk_label_set_justify (GTK_LABEL (labelAbout), GTK_JUSTIFY_LEFT);
  gtk_label_set_line_wrap (GTK_LABEL (labelAbout), TRUE);

  about = gtk_label_new (_("About"));
  gtk_widget_set_name (about, "about");
  gtk_widget_ref (about);
  gtk_object_set_data_full (GTK_OBJECT (familiar), "about", about,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (about);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 2), about);

  gtk_signal_connect (GTK_OBJECT (familiar), "delete_event",
                      GTK_SIGNAL_FUNC (on_familiar_delete_event),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (toolbar_open), "clicked",
                      GTK_SIGNAL_FUNC (on_toolbar_open_clicked),
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
  gtk_signal_connect (GTK_OBJECT (clistmedia), "select_row",
                      GTK_SIGNAL_FUNC (on_clistmedia_select_row),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (clistmedia), "click_column",
                      GTK_SIGNAL_FUNC (on_clistmedia_click_column),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (comboURL_entry), "changed",
                      GTK_SIGNAL_FUNC (on_comboURL_entry_changed),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (cbautoplay), "toggled",
                      GTK_SIGNAL_FUNC (on_cbautoplay_toggled),
                      NULL);

  return familiar;
}

