void on_generic_drop_data_received( intf_thread_t * p_intf, 
        GtkSelectionData *data, guint info, int position);
void rebuildCList(GtkCList * clist, playlist_t * playlist_p);
int hasValidExtension(gchar * filename);
int intf_AppendList( playlist_t * p_playlist, int i_pos, GList * list );
void GtkPlayListManage( gpointer p_data );
void on_generic_drop_data_received( intf_thread_t * p_intf,
        GtkSelectionData *data, guint info, int position);
gint compareItems(gconstpointer a, gconstpointer b);
GList * intf_readFiles(gchar * fsname );
