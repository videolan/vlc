/*****************************************************************************
 * gtk_preferences.c: functions to handle the preferences dialog box.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: gtk_preferences.c,v 1.18 2002/03/26 22:30:09 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Loïc Minier <lool@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble: Our main job is to build a nice interface from the modules config
 *   structure. Once this is done, we need to track each change made by the
 *   user to the data contained in this interface so that when/if he decides to
 *   apply his changes we can quickly commit them into the modules config
 *   structure. (for this last task we use a GHashTable to accumulate the
 *   changes. To commit them, we then just have to circle through it )
 *
 *****************************************************************************/
#include <sys/types.h>                                              /* off_t */
#include <stdlib.h>

#include <videolan/vlc.h>

#ifdef MODULE_NAME_IS_gnome
#   include <gnome.h>
#else
#   include <gtk/gtk.h>
#endif

#include <string.h>

#include "interface.h"

#include "gtk_support.h"
#include "gtk_common.h"
#include "gtk_preferences.h"

/* local functions */
static void GtkCreateConfigDialog( char *, intf_thread_t * );

static void GtkConfigOk          ( GtkButton *, gpointer );
static void GtkConfigApply       ( GtkButton *, gpointer );
static void GtkConfigCancel      ( GtkButton *, gpointer );
static void GtkConfigSave        ( GtkButton *, gpointer );

static void GtkConfigDialogDestroyed ( GtkObject *, gpointer );

static void GtkStringChanged     ( GtkEditable *, gpointer );
static void GtkIntChanged        ( GtkEditable *, gpointer );
static void GtkBoolChanged       ( GtkToggleButton *, gpointer );

static void GtkFreeHashTable     ( gpointer );
static void GtkFreeHashValue     ( gpointer, gpointer, gpointer );
static void GtkSaveHashValue     ( gpointer, gpointer, gpointer );

static void GtkPluginConfigure   ( GtkButton *, gpointer );
static void GtkPluginSelected    ( GtkButton *, gpointer );
static void GtkPluginHighlighted ( GtkCList *, int, int, GdkEventButton *,
                                   gpointer );

/****************************************************************************
 * Callback for menuitems: display configuration interface window
 ****************************************************************************/
void GtkPreferencesActivate( GtkMenuItem * menuitem, gpointer user_data )
{
    intf_thread_t * p_intf;

    p_intf = GetIntf( GTK_WIDGET(menuitem), (char*)user_data );

    GtkCreateConfigDialog( "main", p_intf );
}

/****************************************************************************
 * GtkCreateConfigDialog: dynamically creates the configuration dialog
 * box from all the configuration data provided by the selected module.
 ****************************************************************************/

/* create a new tooltipped area */
#define TOOLTIP( text )                                                   \
    /* create an event box to catch some events */                        \
    item_event_box = gtk_event_box_new();                                 \
    /* add a tooltip on mouseover */                                      \
    /* FIXME: have a different text for the private text */               \
    gtk_tooltips_set_tip( p_intf->p_sys->p_tooltips,                      \
                          item_event_box, text, text );                   \
    gtk_container_set_border_width( GTK_CONTAINER(item_event_box), 4 );

/* draws a right aligned label in side of a widget */
#define LABEL_AND_WIDGET( label_text, widget, tooltip )                   \
    gtk_table_resize( GTK_TABLE(category_table), ++rows, 2 );             \
    item_align = gtk_alignment_new( 1, .5, 0, 0 );                        \
    item_label = gtk_label_new( label_text );                             \
    gtk_container_add( GTK_CONTAINER(item_align), item_label );           \
    TOOLTIP(tooltip)                                                      \
    gtk_container_add( GTK_CONTAINER(item_event_box), item_align );       \
    gtk_table_attach_defaults( GTK_TABLE(category_table), item_event_box, \
                               0, 1, rows - 1, rows );                    \
    item_align = gtk_alignment_new( 0, .5, .5, 0 );                       \
    gtk_container_add( GTK_CONTAINER(item_align), widget );               \
    TOOLTIP(tooltip)                                                      \
    gtk_container_add( GTK_CONTAINER(item_event_box), item_align );       \
    gtk_table_attach_defaults( GTK_TABLE(category_table), item_event_box, \
                               1, 2, rows - 1, rows );

static void GtkCreateConfigDialog( char *psz_module_name,
                                   intf_thread_t *p_intf )
{
    module_t *p_module, *p_module_bis;
    int i;

    guint rows = 0;

    GHashTable *config_hash_table;

    GtkWidget *item_event_box;

    GtkWidget *config_dialog;
    GtkWidget *config_dialog_vbox;
    GtkWidget *config_notebook;
    GtkWidget *category_table = NULL;
    GtkWidget *category_vbox = NULL;

    GtkWidget *dialog_action_area;
    GtkWidget *ok_button;
    GtkWidget *apply_button;
    GtkWidget *save_button;
    GtkWidget *cancel_button;

    GtkWidget *item_align;
    GtkWidget *item_frame;
    GtkWidget *item_hbox;
    GtkWidget *item_label;
    GtkWidget *item_vbox;
    GtkWidget *string_entry;
    GtkWidget *integer_spinbutton;
    GtkObject *item_adj;
    GtkWidget *bool_checkbutton;
    GtkWidget *plugin_clist;
    GtkWidget *plugin_config_button;
    GtkWidget *plugin_select_button;


    /* Check if the dialog box is already opened, if so this will save us
     * quite a bit of work. (the interface will be destroyed when you actually
     * close the dialog window, but remember that it is only hidden if you
     * clicked on the action buttons). This trick also allows us not to
     * duplicate identical dialog windows. */

    config_dialog = (GtkWidget *)gtk_object_get_data(
                    GTK_OBJECT(p_intf->p_sys->p_window), psz_module_name );
    if( config_dialog )
    {
        /* Yeah it was open */
        gtk_widget_show( config_dialog );
        gtk_widget_grab_focus( config_dialog );
        return;
    }


    /* Look for the selected module */
    for( p_module = p_module_bank->first ; p_module != NULL ;
         p_module = p_module->next )
    {

        if( psz_module_name && !strcmp( psz_module_name, p_module->psz_name ) )
            break;
    }
    if( !p_module ) return;

    /* We found it, now we can start building its configuration interface */
    /* Create the configuration dialog box */
    config_dialog = gtk_dialog_new();
    gtk_window_set_title( GTK_WINDOW(config_dialog), p_module->psz_longname );

    config_dialog_vbox = GTK_DIALOG(config_dialog)->vbox;
    gtk_container_set_border_width( GTK_CONTAINER(config_dialog_vbox), 0 );

    /* Create our config hash table and associate it with the dialog box */
    config_hash_table = g_hash_table_new( NULL, NULL );
    gtk_object_set_data_full( GTK_OBJECT(config_dialog),
                              "config_hash_table", config_hash_table,
                              (GtkDestroyNotify)GtkFreeHashTable );

    /* Create notebook */
    config_notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable( GTK_NOTEBOOK(config_notebook), TRUE );
    gtk_container_add( GTK_CONTAINER(config_dialog_vbox), config_notebook );

    /* Enumerate config options and add corresponding config boxes */
    for( i = 0; i < p_module->i_config_lines; i++ )
    {

        switch( p_module->p_config[i].i_type )
        {
        case MODULE_CONFIG_HINT_CATEGORY:

            /* add a new table for right-left alignment */
            category_table = gtk_table_new( 0, 0, FALSE );
            gtk_table_set_col_spacings( GTK_TABLE(category_table), 4 );
            rows = 0;
            /* the vbox serves only EXPAND / FILL issues in the notebook */
            category_vbox = gtk_vbox_new( FALSE, 0 );
            gtk_box_pack_start( GTK_BOX(category_vbox), category_table,
                                FALSE, FALSE, 0 );
            gtk_container_set_border_width( GTK_CONTAINER(category_vbox), 4 );

            /* add category vbox as a notebook page */
            item_label = gtk_label_new( p_module->p_config[i].psz_text );
            gtk_notebook_append_page( GTK_NOTEBOOK(config_notebook),
                                      category_vbox, item_label );


            break;

        case MODULE_CONFIG_ITEM_PLUGIN:

            item_frame = gtk_frame_new( p_module->p_config[i].psz_text );

            gtk_table_resize( GTK_TABLE(category_table), ++rows, 2 );
            gtk_table_attach_defaults( GTK_TABLE(category_table), item_frame,
                                       0, 2, rows - 1, rows );

            TOOLTIP( p_module->p_config[i].psz_longtext )
            gtk_container_add( GTK_CONTAINER(item_frame), item_event_box );

            item_vbox = gtk_vbox_new( FALSE, 4 );
            gtk_container_add( GTK_CONTAINER(item_event_box), item_vbox );

            /* create a new clist widget */
            {
                gchar * titles[] = { "Name", "Description" };

                plugin_clist =
                    gtk_clist_new_with_titles( 2, titles );
            }
            gtk_clist_column_titles_passive( GTK_CLIST(plugin_clist) );
            gtk_clist_set_selection_mode( GTK_CLIST(plugin_clist),
                                          GTK_SELECTION_SINGLE);
            gtk_container_add( GTK_CONTAINER(item_vbox), plugin_clist );

            /* build a list of available plugins */
            {
                gchar * entry[2];

                for( p_module_bis = p_module_bank->first ;
                     p_module_bis != NULL ;
                     p_module_bis = p_module_bis->next )
                {
                    if( p_module_bis->i_capabilities &
                        (1 << p_module->p_config[i].i_value) )
                    {
                        entry[0] = p_module_bis->psz_name;
                        entry[1] = p_module_bis->psz_longname;
                        gtk_clist_append( GTK_CLIST(plugin_clist), entry );
                    }
                }
            }

            gtk_clist_set_column_auto_resize( GTK_CLIST(plugin_clist),
                                              0, TRUE );
            gtk_clist_set_column_auto_resize( GTK_CLIST(plugin_clist),
                                              1, TRUE );

            /* connect signals to the plugins list */
            gtk_signal_connect( GTK_OBJECT(plugin_clist), "select_row",
                                GTK_SIGNAL_FUNC(GtkPluginHighlighted),
                                NULL );

            /* hbox holding the "select" and "configure" buttons */
            item_hbox = gtk_hbox_new( FALSE, 4 );
            gtk_container_add( GTK_CONTAINER(item_vbox), item_hbox);

            /* add configure button */
            plugin_config_button =
                gtk_button_new_with_label( _("Configure") );
            gtk_widget_set_sensitive( plugin_config_button, FALSE );
            gtk_container_add( GTK_CONTAINER(item_hbox),
                               plugin_config_button );
            gtk_object_set_data( GTK_OBJECT(plugin_config_button),
                                 "p_intf", p_intf );
            gtk_object_set_data( GTK_OBJECT(plugin_clist),
                                 "config_button", plugin_config_button );

            /* add select button */
            plugin_select_button =
                gtk_button_new_with_label( _("Select") );
            gtk_container_add( GTK_CONTAINER(item_hbox),
                               plugin_select_button );

            /* hbox holding the "selected" label and text input */
            item_hbox = gtk_hbox_new( FALSE, 4 );
            gtk_container_add( GTK_CONTAINER(item_vbox), item_hbox);
            /* add new label */
            item_label = gtk_label_new( _("Selected:") );
            gtk_container_add( GTK_CONTAINER(item_hbox), item_label );

            /* add input box with default value */
            string_entry = gtk_entry_new();
            gtk_object_set_data( GTK_OBJECT(plugin_clist),
                                 "plugin_entry", string_entry );
            gtk_container_add( GTK_CONTAINER(item_hbox), string_entry );
            vlc_mutex_lock( p_module->p_config[i].p_lock );
            gtk_entry_set_text( GTK_ENTRY(string_entry),
                                p_module->p_config[i].psz_value ?
                                p_module->p_config[i].psz_value : "" );
            vlc_mutex_unlock( p_module->p_config[i].p_lock );

            /* connect signals to the buttons */
            gtk_signal_connect( GTK_OBJECT(plugin_config_button), "clicked",
                                GTK_SIGNAL_FUNC(GtkPluginConfigure),
                                (gpointer)plugin_clist );
            gtk_signal_connect( GTK_OBJECT(plugin_select_button), "clicked",
                                GTK_SIGNAL_FUNC(GtkPluginSelected),
                                (gpointer)plugin_clist );

            /* connect signal to track changes in the text box */
            gtk_object_set_data( GTK_OBJECT(string_entry), "config_option",
                                 p_module->p_config[i].psz_name );
            gtk_signal_connect( GTK_OBJECT(string_entry), "changed",
                                GTK_SIGNAL_FUNC(GtkStringChanged),
                                (gpointer)config_dialog );
            break;

        case MODULE_CONFIG_ITEM_STRING:
        case MODULE_CONFIG_ITEM_FILE:

            /* add input box with default value */
            string_entry = gtk_entry_new();
            vlc_mutex_lock( p_module->p_config[i].p_lock );
            gtk_entry_set_text( GTK_ENTRY(string_entry),
                                p_module->p_config[i].psz_value ?
                                p_module->p_config[i].psz_value : "" );
            vlc_mutex_unlock( p_module->p_config[i].p_lock );

            /* connect signal to track changes in the text box */
            gtk_object_set_data( GTK_OBJECT(string_entry), "config_option",
                                 p_module->p_config[i].psz_name );
            gtk_signal_connect( GTK_OBJECT(string_entry), "changed",
                                GTK_SIGNAL_FUNC(GtkStringChanged),
                                (gpointer)config_dialog );

            LABEL_AND_WIDGET( p_module->p_config[i].psz_text,
                              string_entry,
                              p_module->p_config[i].psz_longtext );
            break;

        case MODULE_CONFIG_ITEM_INTEGER:

            /* add input box with default value */
            item_adj = gtk_adjustment_new( p_module->p_config[i].i_value,
                                           -1, 99999, 1, 10, 10 );
            integer_spinbutton = gtk_spin_button_new( GTK_ADJUSTMENT(item_adj),
                                                      1, 0 );

            /* connect signal to track changes in the spinbutton value */
            gtk_object_set_data( GTK_OBJECT(integer_spinbutton),
                                 "config_option",
                                 p_module->p_config[i].psz_name );
            gtk_signal_connect( GTK_OBJECT(integer_spinbutton), "changed",
                                GTK_SIGNAL_FUNC(GtkIntChanged),
                                (gpointer)config_dialog );

            LABEL_AND_WIDGET( p_module->p_config[i].psz_text,
                              integer_spinbutton,
                              p_module->p_config[i].psz_longtext );
            break;

        case MODULE_CONFIG_ITEM_BOOL:

            /* add check button */
            bool_checkbutton = gtk_check_button_new();
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(bool_checkbutton),
                                          p_module->p_config[i].i_value );

            /* connect signal to track changes in the button state */
            gtk_object_set_data( GTK_OBJECT(bool_checkbutton), "config_option",
                                 p_module->p_config[i].psz_name );
            gtk_signal_connect( GTK_OBJECT(bool_checkbutton), "toggled",
                                GTK_SIGNAL_FUNC(GtkBoolChanged),
                                (gpointer)config_dialog );

            LABEL_AND_WIDGET( p_module->p_config[i].psz_text,
                              bool_checkbutton,
                              p_module->p_config[i].psz_longtext );
            break;
        }
    }

    /* Now let's add the action buttons at the bottom of the page */

    dialog_action_area = GTK_DIALOG(config_dialog)->action_area;
    gtk_container_set_border_width( GTK_CONTAINER(dialog_action_area), 4 );

    /* add a new table for the config option */
    item_hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_end( GTK_BOX(dialog_action_area), item_hbox,
                      TRUE, FALSE, 0 );
    item_hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_end( GTK_BOX(dialog_action_area), item_hbox,
                      TRUE, FALSE, 0 );

    /* Create the OK button */
    ok_button = gtk_button_new_with_label( _("Ok") );
    gtk_box_pack_start( GTK_BOX(dialog_action_area), ok_button,
                        TRUE, TRUE, 0 );

    apply_button = gtk_button_new_with_label( _("Apply") );
    gtk_box_pack_start( GTK_BOX(dialog_action_area), apply_button,
                        TRUE, TRUE, 0 );

    save_button = gtk_button_new_with_label( _("Save") );
    gtk_box_pack_start( GTK_BOX(dialog_action_area), save_button,
                        TRUE, TRUE, 0 );

    cancel_button = gtk_button_new_with_label( _("Cancel") );
    gtk_box_pack_start( GTK_BOX(dialog_action_area), cancel_button,
                        TRUE, TRUE, 0 );

    gtk_signal_connect( GTK_OBJECT(ok_button), "clicked",
                        GTK_SIGNAL_FUNC(GtkConfigOk),
                        config_dialog );
    gtk_signal_connect( GTK_OBJECT(apply_button), "clicked",
                        GTK_SIGNAL_FUNC(GtkConfigApply),
                        config_dialog );
    gtk_signal_connect( GTK_OBJECT(save_button), "clicked",
                        GTK_SIGNAL_FUNC(GtkConfigSave),
                        config_dialog );
    gtk_signal_connect( GTK_OBJECT(cancel_button), "clicked",
                        GTK_SIGNAL_FUNC(GtkConfigCancel),
                        config_dialog );

    /* Ok, job done successfully. Let's keep a reference to the dialog box */
    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_window),
                         psz_module_name, config_dialog );
    gtk_object_set_data( GTK_OBJECT(config_dialog), "psz_module_name",
                         psz_module_name );

    /* we want this ref to be destroyed if the object is destroyed */
    gtk_signal_connect( GTK_OBJECT(config_dialog), "destroy",
                       GTK_SIGNAL_FUNC(GtkConfigDialogDestroyed),
                       (gpointer)p_intf );

    gtk_widget_show_all( config_dialog );
}

#undef FRAME
#undef SCROLLED_WINDOW
#undef LABEL
#undef TOOLTIP

/****************************************************************************
 * GtkConfigApply: store the changes to the config inside the modules
 * configuration structure
 ****************************************************************************/
void GtkConfigApply( GtkButton * button, gpointer user_data )
{
    GHashTable *hash_table;
    hash_table = (GHashTable *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                    "config_hash_table" );
    g_hash_table_foreach( hash_table, GtkSaveHashValue, NULL );

}

void GtkConfigOk( GtkButton * button, gpointer user_data )
{
    GtkConfigApply( button, user_data );
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}


void GtkConfigCancel( GtkButton * button, gpointer user_data )
{
    gtk_widget_hide( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}

void GtkConfigSave( GtkButton * button, gpointer user_data )
{
    GtkConfigApply( button, user_data );
    config_SaveConfigFile( NULL );
}

/****************************************************************************
 * GtkPluginHighlighted: display plugin description when an entry is selected
 *   in the clist, and activate the configure button if necessary.
 ****************************************************************************/
void GtkPluginHighlighted( GtkCList *plugin_clist, int row, int column,
                           GdkEventButton *event, gpointer user_data )
{
    GtkWidget *config_button;
    module_t *p_module;
    char *psz_name;

    if( gtk_clist_get_text( GTK_CLIST(plugin_clist), row, 0, &psz_name ) )
    {
        /* look for plugin 'psz_name' */
        for( p_module = p_module_bank->first ;
             p_module != NULL ;
             p_module = p_module->next )
        {
          if( !strcmp( p_module->psz_name, psz_name ) )
          {
              gtk_object_set_data( GTK_OBJECT(plugin_clist),
                                   "plugin_highlighted", p_module );
              config_button = gtk_object_get_data( GTK_OBJECT(plugin_clist),
                                                   "config_button" );
              if( p_module->i_config_items )
                  gtk_widget_set_sensitive( config_button, TRUE );
              else
                  gtk_widget_set_sensitive( config_button, FALSE );

              break;
          }
        }

    }
}

/****************************************************************************
 * GtkPluginConfigure: display plugin configuration dialog box.
 ****************************************************************************/
void GtkPluginConfigure( GtkButton *button, gpointer user_data )
{
    module_t *p_module;
    intf_thread_t *p_intf;

    p_module = (module_t *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                "plugin_highlighted" );

    if( !p_module ) return;
    p_intf = (intf_thread_t *)gtk_object_get_data( GTK_OBJECT(button),
                                                   "p_intf" );
    GtkCreateConfigDialog( p_module->psz_name, (gpointer)p_intf );

}

/****************************************************************************
 * GtkPluginSelected: select plugin.
 ****************************************************************************/
void GtkPluginSelected( GtkButton *button, gpointer user_data )
{
    module_t *p_module;
    GtkWidget *widget;

    p_module = (module_t *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                "plugin_highlighted" );
    widget = (GtkWidget *)gtk_object_get_data( GTK_OBJECT(user_data),
                                               "plugin_entry" );
    if( !p_module ) return;

    gtk_entry_set_text( GTK_ENTRY(widget), p_module->psz_name );

}

/****************************************************************************
 * GtkStringChanged: signal called when the user changes a string value.
 ****************************************************************************/
static void GtkStringChanged( GtkEditable *editable, gpointer user_data )
{
    module_config_t *p_config;

    GHashTable *hash_table;

    hash_table = (GHashTable *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                    "config_hash_table" );
    /* free old p_config */
    p_config = (module_config_t *)g_hash_table_lookup( hash_table,
                                                       (gpointer)editable );
    if( p_config ) GtkFreeHashValue( NULL, (gpointer)p_config, NULL );

    p_config = malloc( sizeof(module_config_t) );
    p_config->i_type = MODULE_CONFIG_ITEM_STRING;
    p_config->psz_value = gtk_editable_get_chars( editable, 0, -1 );
    p_config->psz_name = (char *)gtk_object_get_data( GTK_OBJECT(editable),
                                                      "config_option" );

    g_hash_table_insert( hash_table, (gpointer)editable,
                         (gpointer)p_config );
}

/****************************************************************************
 * GtkIntChanged: signal called when the user changes a an integer value.
 ****************************************************************************/
static void GtkIntChanged( GtkEditable *editable, gpointer user_data )
{
    module_config_t *p_config;

    GHashTable *hash_table;

    hash_table = (GHashTable *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                    "config_hash_table" );

    /* free old p_config */
    p_config = (module_config_t *)g_hash_table_lookup( hash_table,
                                                       (gpointer)editable );
    if( p_config ) GtkFreeHashValue( NULL, (gpointer)p_config, NULL );

    p_config = malloc( sizeof(module_config_t) );
    p_config->i_type = MODULE_CONFIG_ITEM_INTEGER;
    p_config->i_value =  gtk_spin_button_get_value_as_int(
                             GTK_SPIN_BUTTON(editable) );
    p_config->psz_name = (char *)gtk_object_get_data( GTK_OBJECT(editable),
                                                      "config_option" );

    g_hash_table_insert( hash_table, (gpointer)editable,
                         (gpointer)p_config );
}

/****************************************************************************
 * GtkStringChanged: signal called when the user changes a bool value.
 ****************************************************************************/
static void GtkBoolChanged( GtkToggleButton *button, gpointer user_data )
{
    module_config_t *p_config;

    GHashTable *hash_table;

    hash_table = (GHashTable *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                    "config_hash_table" );

    /* free old p_config */
    p_config = (module_config_t *)g_hash_table_lookup( hash_table,
                                                       (gpointer)button );
    if( p_config ) GtkFreeHashValue( NULL, (gpointer)p_config, NULL );

    p_config = malloc( sizeof(module_config_t) );
    p_config->i_type = MODULE_CONFIG_ITEM_BOOL;
    p_config->i_value = gtk_toggle_button_get_active( button );
    p_config->psz_name = (char *)gtk_object_get_data( GTK_OBJECT(button),
                                                      "config_option" );

    g_hash_table_insert( hash_table, (gpointer)button,
                         (gpointer)p_config );
}

/****************************************************************************
 * GtkFreeHashTable: signal called when the config hash table is destroyed.
 ****************************************************************************/
static void GtkFreeHashTable( gpointer user_data )
{
    GHashTable *hash_table = (GHashTable *)user_data;

    g_hash_table_foreach( hash_table, GtkFreeHashValue, NULL );
    g_hash_table_destroy( hash_table );
}

/****************************************************************************
 * GtkFreeHashValue: signal called when an element of the config hash table
 * is destroyed.
 ****************************************************************************/
static void GtkFreeHashValue( gpointer key, gpointer value, gpointer user_data)
{
    module_config_t *p_config = (module_config_t *)value;

    if( p_config->i_type == MODULE_CONFIG_ITEM_STRING )
        g_free( p_config->psz_value );
    free( p_config );
}

/****************************************************************************
 * GtkSaveHashValue: callback used when enumerating the hash table in
 * GtkConfigApply().
 ****************************************************************************/
static void GtkSaveHashValue( gpointer key, gpointer value, gpointer user_data)
{
    module_config_t *p_config = (module_config_t *)value;

    switch( p_config->i_type )
    {

    case MODULE_CONFIG_ITEM_STRING:
    case MODULE_CONFIG_ITEM_FILE:
    case MODULE_CONFIG_ITEM_PLUGIN:
        config_PutPszVariable( p_config->psz_name,
            *p_config->psz_value? p_config->psz_value : NULL );
        break;
    case MODULE_CONFIG_ITEM_INTEGER:
    case MODULE_CONFIG_ITEM_BOOL:
        config_PutIntVariable( p_config->psz_name, p_config->i_value );
        break;
    }
}

/****************************************************************************
 * GtkConfigDialogDestroyed: callback triggered when the config dialog box is
 * destroyed.
 ****************************************************************************/
static void GtkConfigDialogDestroyed( GtkObject *object, gpointer user_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)user_data;
    char *psz_module_name;

    psz_module_name = gtk_object_get_data( object, "psz_module_name" );

    /* remove the ref to the dialog box */
    gtk_object_set_data( GTK_OBJECT(p_intf->p_sys->p_window),
                         psz_module_name, NULL );

}
