// gcc -o main main.c `pkg-config --cflags --libs gtk+-2.0 libvlc`

/* Written by Vincent Schüßler */
/* License WTFPL http://sam.zoy.org/wtfpl/ */

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <gtk/gtk.h>
#include <vlc/vlc.h>

#define PRESET_H264_AAC_MP4_HIGH "MP4 high (h.264 + AAC)"
#define PRESET_H264_AAC_MP4_LOW "MP4 low (h.264 + AAC)"
#define PRESET_THEORA_VORBIS_OGG_HIGH "OGG high (Theora + Vorbis)"
#define PRESET_THEORA_VORBIS_OGG_LOW "OGG low (Theora + Vorbis)"
#define PRESET_VP8_VORBIS_WEBM_HIGH "WebM high (VP8 + Vorbis)"
#define PRESET_VP8_VORBIS_WEBM_LOW "WebM low (VP8 + Vorbis)"
#define BORDER_WIDTH 6

GtkWidget *window;
GtkWidget *source_entry, *dest_entry;
GtkWidget *progressbar;
libvlc_instance_t *vlcinst;
libvlc_media_list_t *medialist;
GtkWidget *run_button, *format_chooser, *spinner;
char stopped;

gchar* get_filepath(GtkWidget* widget, GtkFileChooserAction action) {
    GtkWidget *dialog;
    gchar *path;
    dialog = gtk_file_chooser_dialog_new("Choose location", GTK_WINDOW(gtk_widget_get_toplevel(widget)), action, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
    if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    }
    else {
        path = NULL;
    }
    gtk_widget_destroy(dialog);
    return path;
}

void on_select_source_path(GtkWidget *widget, gpointer data) {
    char *path;
    char scheme[] = "file://";
    char *uri;

    if(data==NULL) {
        path = (char*) get_filepath(widget, GTK_FILE_CHOOSER_ACTION_OPEN);
    }
    else {
        path = (char*) get_filepath(widget, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    }
    if(path != NULL) {
        uri = malloc((strlen(scheme)+strlen(path)+1) * sizeof(char));
        if(uri == NULL) return;
        uri[0] = '\0';
        strncat(uri, scheme, strlen(scheme));
        strncat(uri, path, strlen(path));
        g_free(path);
        
        gtk_entry_set_text(GTK_ENTRY(source_entry), uri);
        free(uri);
    }
}

void on_select_dest_path(GtkWidget *widget, gpointer data) {
    gchar *path;
    path = get_filepath(widget, GTK_FILE_CHOOSER_ACTION_SAVE);
    if(path != NULL) {
        gtk_entry_set_text(GTK_ENTRY(dest_entry), path);
        g_free(path);
    }
}

char* get_pos_string(float pos) {
    int len;
    const char format[] = "%.3f %%";
    char *pos_string;
    pos *= 100;
    len = snprintf(NULL, 0, format, pos);
    pos_string = malloc(len);
    if(pos_string==NULL) return NULL;
    sprintf(pos_string, format, pos);
    return pos_string;
}

gboolean update_progressbar(char *handle) {
    float pos = libvlc_vlm_get_media_instance_position(vlcinst, handle, 0);
    char *pos_string;
    if(pos < 1.0 && 0.0 <= pos) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressbar), pos);
        pos_string = get_pos_string(pos);
        if(pos_string != NULL) {
            gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressbar), pos_string);
            free(pos_string);
        }
        return TRUE;
    }
    if(stopped = 1) {
        free(handle);
        return FALSE;
    }
}

const char* get_transcode_string(char *preset) {
    static const char mp4_high[] = "#transcode{vcodec=h264,venc=x264{cfr=16},scale=1,acodec=mp4a,ab=160,channels=2,samplerate=44100}";
    static const char mp4_low[] = "#transcode{vcodec=h264,venc=x264{cfr=40},scale=1,acodec=mp4a,ab=96,channels=2,samplerate=44100}";
    static const char ogg_high[] = "#transcode{vcodec=theo,venc=theora{quality=9},scale=1,acodec=vorb,ab=160,channels=2,samplerate=44100}";
    static const char ogg_low[] = "#transcode{vcodec=theo,venc=theora{quality=4},scale=1,acodec=vorb,ab=96,channels=2,samplerate=44100}";
    static const char webm_high[] = "#transcode{vcodec=VP80,vb=2000,scale=1,acodec=vorb,ab=160,channels=2,samplerate=44100}";
    static const char webm_low[] = "#transcode{vcodec=VP80,vb=1000,scale=1,acodec=vorb,ab=96,channels=2,samplerate=44100}";
    static const char nothing[] = "";
    if(0 == strcmp(preset, PRESET_H264_AAC_MP4_HIGH)) {
        return mp4_high;
    }
    else if(0 == strcmp(preset, PRESET_H264_AAC_MP4_LOW)) {
        return mp4_low;
    }
    else if(0 == strcmp(preset, PRESET_THEORA_VORBIS_OGG_HIGH)) {
        return ogg_high;
    }
    else if(0 == strcmp(preset, PRESET_THEORA_VORBIS_OGG_LOW)) {
        return ogg_low;
    }
    else if(0 == strcmp(preset, PRESET_VP8_VORBIS_WEBM_HIGH)) {
        return webm_high;
    }
    else if(0 == strcmp(preset, PRESET_VP8_VORBIS_WEBM_LOW)) {
        return webm_low;
    }
    else {
        return nothing;
    }
}

void on_run(GtkWidget *widget, gpointer data) {
    char *handle;
    const char *transcode;
    char *source = (char*) gtk_entry_get_text(GTK_ENTRY(source_entry));
    char *dest = (char*) gtk_entry_get_text(GTK_ENTRY(dest_entry));
    char *preset= (char*) gtk_combo_box_get_active_text(GTK_COMBO_BOX(format_chooser));
    int i;
    char file_begin[] = ":file{dst=";
    char file_end[] = "}";
    char *sout;
    if(preset == NULL) return;
    gtk_widget_set_sensitive(widget, FALSE);
    handle = malloc((strlen(source)+4+1) * sizeof(char));
    if(handle == NULL) return;
    strncpy(handle, source, strlen(source));
    for(i=0;i<=3;++i) {
        handle[strlen(source)+i] = (char) (((unsigned int) rand()) & 63) + '0';
    }
    handle[strlen(source)+4] = '\0';
    transcode = get_transcode_string(preset);
    free(preset);
    sout = malloc((strlen(transcode)+strlen(file_begin)+strlen(dest)+strlen(file_end)+1) * sizeof(char));
    if(sout == NULL) return;
    strncpy(sout, transcode, strlen(transcode)+1);
    strncat(sout, file_begin, strlen(file_begin));
    strncat(sout, dest, strlen(dest));
    strncat(sout, file_end, strlen(file_end));
    libvlc_vlm_add_broadcast(vlcinst, handle, source, sout, 0, NULL, 1, 0);
    free(sout);
    libvlc_vlm_play_media(vlcinst, handle);
    gtk_widget_show(spinner);
    gtk_spinner_start(GTK_SPINNER(spinner));
    stopped = 0;
    g_timeout_add(50, (GSourceFunc) update_progressbar, handle);
}

void stop(void) {
    stopped = 1;
    gtk_spinner_stop(GTK_SPINNER(spinner));
    gtk_widget_hide(spinner);
    gtk_widget_set_sensitive(run_button, TRUE);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressbar), 0.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressbar), "");
}

gboolean on_end(void) {
    GtkWidget *dialog;
    stop();
    dialog = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Rip done");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return FALSE;
}

void on_end_vlc(const libvlc_event_t *event, void *data) {
    g_idle_add((GSourceFunc) on_end, NULL);
}

gboolean on_error(void) {
    GtkWidget *dialog;
    stop();
    dialog = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Error");gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return FALSE;
}

void on_error_vlc(const libvlc_event_t *event, void *data) {
    g_idle_add((GSourceFunc) on_error, NULL);
}

int main (int argc, char *argv[]) {
    GtkWidget *media_list, *scrolled;
    GtkWidget *choose_frame, *output_frame, *run_frame;
    GtkWidget *vbox, *choose_table, *output_table, *run_table;
    GtkWidget *source_label, *source_button, *source_folder_button;
    GtkWidget *dest_label, *dest_button;

    libvlc_event_manager_t *evtman;

    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(window), BORDER_WIDTH);
    gtk_window_set_title(GTK_WINDOW(window), "libVLC DVD ripper");

    //setup vbox
    vbox = gtk_vbox_new(FALSE, BORDER_WIDTH);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // setup media list with scrolled window
    media_list = gtk_tree_view_new();
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(media_list), FALSE);
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), media_list);

    // setup "choose"-frame
    choose_frame = gtk_frame_new("Choose DVD");
    gtk_box_pack_start(GTK_BOX(vbox), choose_frame, TRUE, TRUE, 0);
    choose_table = gtk_table_new(1, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(choose_table), BORDER_WIDTH/2);
    gtk_table_set_col_spacings(GTK_TABLE(choose_table), BORDER_WIDTH/2);
    gtk_container_set_border_width(GTK_CONTAINER(choose_table), BORDER_WIDTH);
    source_label = gtk_label_new("Input file or folder:");
    source_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(source_entry), "dvd://");
    source_button = gtk_button_new_with_label("Open file");
    source_folder_button = gtk_button_new_with_label("Open folder");
    g_signal_connect(G_OBJECT(source_button), "clicked", G_CALLBACK(on_select_source_path), NULL);
    g_signal_connect(G_OBJECT(source_folder_button), "clicked", G_CALLBACK(on_select_source_path), (gpointer) 1);
    gtk_table_attach(GTK_TABLE(choose_table), source_label, 0, 1, 0, 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
    gtk_table_attach(GTK_TABLE(choose_table), source_entry, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);
    gtk_table_attach(GTK_TABLE(choose_table), source_button, 2, 3, 0, 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
    gtk_table_attach(GTK_TABLE(choose_table), source_folder_button, 3, 4, 0, 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
    gtk_container_add(GTK_CONTAINER(choose_frame), choose_table);

    // setup "output"-frame
    output_frame = gtk_frame_new("Output settings");
    gtk_box_pack_start(GTK_BOX(vbox), output_frame, TRUE, TRUE, 0);
    output_table = gtk_table_new(2, 3, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(output_table), BORDER_WIDTH/2);
    gtk_table_set_col_spacings(GTK_TABLE(output_table), BORDER_WIDTH/2);
    gtk_container_set_border_width(GTK_CONTAINER(output_table), BORDER_WIDTH);
    gtk_container_add(GTK_CONTAINER(output_frame), output_table);
    dest_label = gtk_label_new("Output file:");
    dest_entry = gtk_entry_new();
    format_chooser = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(format_chooser), PRESET_H264_AAC_MP4_HIGH);
    gtk_combo_box_append_text(GTK_COMBO_BOX(format_chooser), PRESET_H264_AAC_MP4_LOW);
    gtk_combo_box_append_text(GTK_COMBO_BOX(format_chooser), PRESET_THEORA_VORBIS_OGG_HIGH);
    gtk_combo_box_append_text(GTK_COMBO_BOX(format_chooser), PRESET_THEORA_VORBIS_OGG_LOW);
    gtk_combo_box_append_text(GTK_COMBO_BOX(format_chooser), PRESET_VP8_VORBIS_WEBM_HIGH);
    gtk_combo_box_append_text(GTK_COMBO_BOX(format_chooser), PRESET_VP8_VORBIS_WEBM_LOW);
    gtk_combo_box_set_active(GTK_COMBO_BOX(format_chooser), 0);
    dest_button = gtk_button_new_from_stock(GTK_STOCK_OPEN);
    g_signal_connect(G_OBJECT(dest_button), "clicked", G_CALLBACK(on_select_dest_path), NULL);
    gtk_table_attach(GTK_TABLE(output_table), dest_label, 0, 1, 0, 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
    gtk_table_attach(GTK_TABLE(output_table), dest_entry, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);
    gtk_table_attach(GTK_TABLE(output_table), dest_button, 2, 3, 0, 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
    gtk_table_attach(GTK_TABLE(output_table), format_chooser, 0, 4, 1, 2, GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);

    // setup "run"-frame
    run_frame = gtk_frame_new("Run");
    gtk_box_pack_start(GTK_BOX(vbox), run_frame, TRUE, TRUE, 0);
    run_table = gtk_table_new(3, 2, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(run_table), BORDER_WIDTH/2);
    gtk_table_set_col_spacings(GTK_TABLE(run_table), BORDER_WIDTH/2);
    gtk_container_set_border_width(GTK_CONTAINER(run_table), BORDER_WIDTH);
    gtk_container_add(GTK_CONTAINER(run_frame), run_table);
    progressbar = gtk_progress_bar_new();
    spinner = gtk_spinner_new();
    run_button = gtk_button_new_from_stock(GTK_STOCK_OK);
    g_signal_connect(G_OBJECT(run_button), "clicked", G_CALLBACK(on_run), NULL);
    gtk_table_attach(GTK_TABLE(run_table), progressbar, 0, 3, 0, 1, GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);
    gtk_table_attach(GTK_TABLE(run_table), gtk_label_new(""), 0, 1, 1, 2, GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_table_attach(GTK_TABLE(run_table), spinner, 1, 2, 1, 2, GTK_SHRINK, GTK_SHRINK, 0, 0);
    gtk_table_attach(GTK_TABLE(run_table), run_button, 2, 3, 1, 2, GTK_SHRINK, GTK_SHRINK, 0, 0);

    // setup vlc
    vlcinst = libvlc_new(0, NULL);
    evtman = libvlc_vlm_get_event_manager(vlcinst);
	libvlc_event_attach(evtman, libvlc_VlmMediaInstanceStatusEnd, on_end_vlc, NULL);
	libvlc_event_attach(evtman, libvlc_VlmMediaInstanceStatusError, on_error_vlc, NULL);

    g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(window);
    gtk_widget_hide(spinner);
    gtk_main ();
    libvlc_release(vlcinst);
    return 0;
}
