

gboolean
FamiliarPlaylistEvent                  (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void FamiliarRebuildCList( GtkCList * p_clist, playlist_t * p_playlist );

void FamiliarPlaylistClear             (GtkButton       *button,
                                        gpointer         user_data);


void
FamiliarPlaylistUpdate                 (GtkButton       *button,
                                        gpointer         user_data);

void
FamiliarPlaylistDel                    (GtkButton       *button,
                                        gpointer         user_data);
