

gboolean
PDAPlaylistEvent                  (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void PDARebuildCList( GtkCList * p_clist, playlist_t * p_playlist );

void PDAPlaylistClear             (GtkButton       *button,
                                        gpointer         user_data);


void
PDAPlaylistUpdate                 (GtkButton       *button,
                                        gpointer         user_data);

void
PDAPlaylistDel                    (GtkButton       *button,
                                        gpointer         user_data);
