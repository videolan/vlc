/*****************************************************************************
 * gtk_preferences.c: functions to handle the preferences dialog box.
 *****************************************************************************
 * Copyright (C) 2001-2004 VideoLAN (Centrale RÃ©seaux) and its contributors
 * $Id$
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

#include <vlc/vlc.h>
#include <vlc/intf.h>

#ifdef MODULE_NAME_IS_gnome
#   include <gnome.h>
#else
#   include <gtk/gtk.h>
#endif

#include <string.h>

#include "gtk_support.h"

#include "common.h"
#include "preferences.h"

/* local functions */
static void GtkCreateConfigDialog( char *, intf_thread_t * );

static void GtkConfigOk          ( GtkButton *, gpointer );
static void GtkConfigApply       ( GtkButton *, gpointer );
static void GtkConfigCancel      ( GtkButton *, gpointer );
static void GtkConfigSave        ( GtkButton *, gpointer );

static void GtkConfigDialogDestroyed ( GtkObject *, gpointer );

static void GtkStringChanged     ( GtkEditable *, gpointer );
static void GtkIntChanged        ( GtkEditable *, gpointer );
static void GtkIntRangedChanged  ( GtkEditable *, gpointer );
static void GtkFloatChanged      ( GtkEditable *, gpointer );
static void GtkFloatRangedChanged      ( GtkEditable *, gpointer );
static void GtkBoolChanged       ( GtkToggleButton *, gpointer );

static void GtkFreeHashTable     ( GtkObject *object );
static void GtkFreeHashValue     ( gpointer, gpointer, gpointer );
static gboolean GtkSaveHashValue ( gpointer, gpointer, gpointer );

static void GtkModuleConfigure   ( GtkButton *, gpointer );
static void GtkModuleSelected    ( GtkButton *, gpointer );
static void GtkModuleHighlighted ( GtkCList *, int, int, GdkEventButton *,
                                   gpointer );

/****************************************************************************
 * Callback for menuitems: display configuration interface window
 ****************************************************************************/
void GtkPreferencesShow( GtkMenuItem * menuitem, gpointer user_data )
{
    intf_thread_t * p_intf;

    p_intf = GtkGetIntf( menuitem );

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
    gtk_tooltips_set_tip( p_intf->p_sys->p_tooltips,                      \
                          item_event_box, text, "" );                     \
    gtk_container_set_border_width( GTK_CONTAINER(item_event_box), 4 );

/* draws a right aligned label in side of a widget */
#define LABEL_AND_WIDGET( label_text, widget, tooltip )                   \
    gtk_table_resize( GTK_TABLE(category_table), ++rows, 2 );             \
    item_align = gtk_alignment_new( 1, .5, 0, 0 );                        \
    item_label = gtk_label_new( label_text );                             \
    gtk_container_add( GTK_CONTAINER(item_align), item_label );           \
    gtk_table_attach_defaults( GTK_TABLE(category_table), item_align,     \
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
    module_t *p_parser = NULL;
    vlc_list_t *p_list;
    module_config_t *p_item;
    vlc_bool_t b_advanced = config_GetInt( p_intf, "advanced" );
    int i_index;

    guint rows = 0;

    GHashTable *config_hash_table;

    GtkWidget *item_event_box;

    GtkWidget *config_dialog;
    GtkWidget *config_dialog_vbox;
    GtkWidget *config_notebook;

    GtkWidget *category_table = NULL;
    GtkWidget *category_label = NULL;

#ifndef MODULE_NAME_IS_gnome
    GtkWidget *dialog_action_area;
#endif

    GtkWidget *ok_button;
    GtkWidget *apply_button;
    GtkWidget *save_button;
    GtkWidget *cancel_button;

    GtkWidget *item_align;
    GtkWidget *item_frame;
    GtkWidget *item_hbox;
    GtkWidget *item_label;
    GtkWidget *item_vbox;
    GtkWidget *item_combo;
    GtkWidget *string_entry;
    GtkWidget *integer_spinbutton;
    GtkWidget *integer_slider;
    GtkWidget *float_spinbutton;
    GtkWidget *float_slider;
    GtkObject *item_adj;
    GtkWidget *bool_checkbutton;
    GtkWidget *module_clist;
    GtkWidget *module_config_button;
    GtkWidget *module_select_button;

    gint category_max_height;

    /* Check if the dialog box is already opened because we don't want to
     * duplicate identical dialog windows. */
    config_dialog = (GtkWidget *)gtk_object_get_data(
                    GTK_OBJECT(p_intf->p_sys->p_window), psz_module_name );
    if( config_dialog )
    {
        /* Yeah it was open */
        gtk_widget_grab_focus( config_dialog );
        return;
    }


    /* Look for the selected module */
    p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( psz_module_name
             && !strcmp( psz_module_name, p_parser->psz_object_name ) )
        {
            break;
        }
    }

    if( !p_parser || i_index == p_list->i_count )
    {
        vlc_list_release( p_list );
        return;
    }

    /* We found it, now we can start building its configuration interface */
    /* Create the configuration dialog box */

#ifdef MODULE_NAME_IS_gnome
    config_dialog = gnome_dialog_new( p_parser->psz_longname, NULL );
    config_dialog_vbox = GNOME_DIALOG(config_dialog)->vbox;
#else
    config_dialog = gtk_dialog_new();
    gtk_window_set_title( GTK_WINDOW(config_dialog),
                          p_parser->psz_longname );
    config_dialog_vbox = GTK_DIALOG(config_dialog)->vbox;
#endif

    gtk_object_set_data( GTK_OBJECT(config_dialog), "p_intf", p_intf );

    category_max_height = config_GetInt( p_intf, MODULE_STRING "-prefs-maxh" );

    gtk_window_set_policy( GTK_WINDOW(config_dialog), TRUE, TRUE, FALSE );
    gtk_container_set_border_width( GTK_CONTAINER(config_dialog_vbox), 0 );

    /* Create our config hash table and associate it with the dialog box */
    config_hash_table = g_hash_table_new( NULL, NULL );
    gtk_object_set_data( GTK_OBJECT(config_dialog),
                         "config_hash_table", config_hash_table );

    /* Create notebook */
    config_notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable( GTK_NOTEBOOK(config_notebook), TRUE );
    gtk_container_add( GTK_CONTAINER(config_dialog_vbox), config_notebook );

    /* Enumerate config options and add corresponding config boxes */
    p_item = p_parser->p_config;

    if( p_item ) do
    {
        if( p_item->b_advanced && !b_advanced ) continue;

        if( p_item->i_type == CONFIG_HINT_CATEGORY ||
            p_item->i_type == CONFIG_HINT_END ||
            !category_table )
        {
            /*
             * Before we start building the interface for the new category, we
             * must close/finish the previous one we were generating.
             */
            if( category_table )
            {
                GtkWidget *_scrolled_window;
                GtkWidget *_viewport;
                GtkWidget *_vbox;
                GtkRequisition _requisition;

                /* create a vbox to deal with EXPAND/FILL issues in the
                 * notebook page, and pack it with the previously generated
                 * category_table */
                _vbox = gtk_vbox_new( FALSE, 0 );
                gtk_container_set_border_width( GTK_CONTAINER(_vbox), 4 );
                gtk_box_pack_start( GTK_BOX(_vbox), category_table,
                                    FALSE, FALSE, 0 );

                /* create a new scrolled window that will contain all of the
                 * above. */
                _scrolled_window = gtk_scrolled_window_new( NULL, NULL );
                gtk_scrolled_window_set_policy(
                    GTK_SCROLLED_WINDOW(_scrolled_window), GTK_POLICY_NEVER,
                    GTK_POLICY_AUTOMATIC );
                /* add scrolled window as a notebook page */
                gtk_notebook_append_page( GTK_NOTEBOOK(config_notebook),
                                          _scrolled_window, category_label );
                /* pack the vbox into the scrolled window */
                _viewport = gtk_viewport_new( NULL, NULL );
                gtk_viewport_set_shadow_type( GTK_VIEWPORT(_viewport),
                                              GTK_SHADOW_NONE );
                gtk_container_add( GTK_CONTAINER(_viewport), _vbox );
                gtk_container_add( GTK_CONTAINER(_scrolled_window),
                                   _viewport );

                /* set the size of the scrolled window to the size of the
                 * child widget */
                gtk_widget_show_all( _vbox );
                gtk_widget_size_request( _vbox, &_requisition );
                if( _requisition.height > category_max_height )
                    gtk_widget_set_usize( _scrolled_window, -1,
                                          category_max_height );
                else
                    gtk_widget_set_usize( _scrolled_window, -1,
                                          _requisition.height );

            }

            /*
             * Now we can start taking care of the new category
             */

            if( p_item->i_type != CONFIG_HINT_END )
            {
                /* create a new table for right-left alignment of children */
                category_table = gtk_table_new( 0, 0, FALSE );
                gtk_table_set_col_spacings( GTK_TABLE(category_table), 4 );
                rows = 0;

                /* create a new category label */
                if( p_item->i_type == CONFIG_HINT_CATEGORY )
                    category_label = gtk_label_new( p_item->psz_text );
                else
                    category_label = gtk_label_new( p_parser->psz_longname );
            }
        }

        switch( p_item->i_type )
        {

        case CONFIG_ITEM_MODULE:

            item_frame = gtk_frame_new( p_item->psz_text );

            gtk_table_resize( GTK_TABLE(category_table), ++rows, 2 );
            gtk_table_attach_defaults( GTK_TABLE(category_table), item_frame,
                                       0, 2, rows - 1, rows );

            item_vbox = gtk_vbox_new( FALSE, 4 );
            gtk_container_add( GTK_CONTAINER(item_frame), item_vbox );

            /* create a new clist widget */
            {
                gchar * titles[] = { N_("Name"), N_("Description") };
                titles[0] = _(titles[0]);
                titles[1] = _(titles[1]);

                module_clist = gtk_clist_new_with_titles( 2, titles );
            }
            gtk_object_set_data( GTK_OBJECT(module_clist), "p_intf", p_intf );
            gtk_clist_column_titles_passive( GTK_CLIST(module_clist) );
            gtk_clist_set_selection_mode( GTK_CLIST(module_clist),
                                          GTK_SELECTION_SINGLE);
            gtk_container_add( GTK_CONTAINER(item_vbox), module_clist );

            /* build a list of available modules */
            {
                gchar * entry[2];

                for( i_index = 0; i_index < p_list->i_count; i_index++ )
                {
                    p_parser = (module_t *)p_list->p_values[i_index].p_object ;

                    if( !strcmp( p_parser->psz_capability,
                                 p_item->psz_type ) )
                    {
                        entry[0] = p_parser->psz_object_name;
                        entry[1] = p_parser->psz_longname;
                        gtk_clist_append( GTK_CLIST(module_clist), entry );
                    }
                }
            }

            gtk_clist_set_column_auto_resize( GTK_CLIST(module_clist),
                                              0, TRUE );
            gtk_clist_set_column_auto_resize( GTK_CLIST(module_clist),
                                              1, TRUE );

            /* connect signals to the modules list */
            gtk_signal_connect( GTK_OBJECT(module_clist), "select_row",
                                GTK_SIGNAL_FUNC(GtkModuleHighlighted),
                                NULL );

            /* hbox holding the "select" and "configure" buttons */
            item_hbox = gtk_hbox_new( FALSE, 4 );
            gtk_container_add( GTK_CONTAINER(item_vbox), item_hbox);

            /* add configure button */
            module_config_button =
                gtk_button_new_with_label( _("Configure") );
            gtk_widget_set_sensitive( module_config_button, FALSE );
            gtk_container_add( GTK_CONTAINER(item_hbox),
                               module_config_button );
            gtk_object_set_data( GTK_OBJECT(module_config_button),
                                 "p_intf", p_intf );
            gtk_object_set_data( GTK_OBJECT(module_clist),
                                 "config_button", module_config_button );

            /* add select button */
            module_select_button =
                gtk_button_new_with_label( _("Select") );
            gtk_container_add( GTK_CONTAINER(item_hbox),
                               module_select_button );
            /* add a tooltip on mouseover */
            gtk_tooltips_set_tip( p_intf->p_sys->p_tooltips,
                                  module_select_button,
                                  p_item->psz_longtext, "" );

            /* hbox holding the "selected" label and text input */
            item_hbox = gtk_hbox_new( FALSE, 4 );
            gtk_container_add( GTK_CONTAINER(item_vbox), item_hbox);
            /* add new label */
            item_label = gtk_label_new( _("Selected:") );
            gtk_container_add( GTK_CONTAINER(item_hbox), item_label );

            /* add input box with default value */
            string_entry = gtk_entry_new();
            gtk_object_set_data( GTK_OBJECT(module_clist),
                                 "module_entry", string_entry );
            gtk_container_add( GTK_CONTAINER(item_hbox), string_entry );
            vlc_mutex_lock( p_item->p_lock );
            gtk_entry_set_text( GTK_ENTRY(string_entry),
                                p_item->psz_value ? p_item->psz_value : "" );
            vlc_mutex_unlock( p_item->p_lock );
            /* add a tooltip on mouseover */
            gtk_tooltips_set_tip( p_intf->p_sys->p_tooltips,
                                  string_entry, p_item->psz_longtext, "" );

            /* connect signals to the buttons */
            gtk_signal_connect( GTK_OBJECT(module_config_button), "clicked",
                                GTK_SIGNAL_FUNC(GtkModuleConfigure),
                                (gpointer)module_clist );
            gtk_signal_connect( GTK_OBJECT(module_select_button), "clicked",
                                GTK_SIGNAL_FUNC(GtkModuleSelected),
                                (gpointer)module_clist );

            /* connect signal to track changes in the text box */
            gtk_object_set_data( GTK_OBJECT(string_entry), "config_option",
                                 p_item->psz_name );
            gtk_signal_connect( GTK_OBJECT(string_entry), "changed",
                                GTK_SIGNAL_FUNC(GtkStringChanged),
                                (gpointer)config_dialog );
            break;

        case CONFIG_ITEM_STRING:
        case CONFIG_ITEM_FILE:
        case CONFIG_ITEM_DIRECTORY:

            if( !p_item->ppsz_list )
            {
                /* add input box with default value */
                item_combo = string_entry = gtk_entry_new();
            }
            else
            {
                /* add combo box with default value */
                GList *items = NULL;
                int i;

                for( i=0; p_item->ppsz_list[i]; i++ )
                    items = g_list_append( items, p_item->ppsz_list[i] );

                item_combo = gtk_combo_new();
                string_entry = GTK_COMBO(item_combo)->entry;
                gtk_combo_set_popdown_strings( GTK_COMBO(item_combo),
                                               items );

            }

            vlc_mutex_lock( p_item->p_lock );
            gtk_entry_set_text( GTK_ENTRY(string_entry),
                                p_item->psz_value ? p_item->psz_value : "" );
            vlc_mutex_unlock( p_item->p_lock );

            /* connect signal to track changes in the text box */
            gtk_object_set_data( GTK_OBJECT(string_entry), "config_option",
                                 p_item->psz_name );
            gtk_signal_connect( GTK_OBJECT(string_entry), "changed",
                                GTK_SIGNAL_FUNC(GtkStringChanged),
                                (gpointer)config_dialog );

            LABEL_AND_WIDGET( p_item->psz_text,
                              item_combo, p_item->psz_longtext );
            break;

        case CONFIG_ITEM_INTEGER:

            if (( p_item->i_max == 0) && ( p_item->i_min == 0))
            {
                /* add input box with default value */
                item_adj = gtk_adjustment_new( p_item->i_value,
                                               -1, 99999, 1, 10, 10 );
                integer_spinbutton = gtk_spin_button_new( GTK_ADJUSTMENT(item_adj), 1, 0 );

                /* connect signal to track changes in the spinbutton value */
                gtk_object_set_data( GTK_OBJECT(integer_spinbutton),
                                     "config_option", p_item->psz_name );
                gtk_signal_connect( GTK_OBJECT(integer_spinbutton), "changed",
                                    GTK_SIGNAL_FUNC(GtkIntChanged),
                                    (gpointer)config_dialog );

                LABEL_AND_WIDGET( p_item->psz_text,
                                  integer_spinbutton, p_item->psz_longtext );
            }
            else /* use i_min and i_max */
            {
                item_adj = gtk_adjustment_new( p_item->i_value, p_item->i_min,
                                               p_item->i_max, 1, 1, 0 );
                integer_slider = gtk_hscale_new( GTK_ADJUSTMENT(item_adj));
                gtk_scale_set_digits (GTK_SCALE(integer_slider), 0);
                                                
                /* connect signal to track changes in the spinbutton value */
                gtk_object_set_data( GTK_OBJECT(item_adj),
                                     "config_option", p_item->psz_name );
                gtk_signal_connect( GTK_OBJECT(item_adj), "value-changed",
                                    GTK_SIGNAL_FUNC(GtkIntRangedChanged),
                                    (gpointer)config_dialog );

                LABEL_AND_WIDGET( p_item->psz_text,
                                  integer_slider, p_item->psz_longtext );
            }
            break;

        case CONFIG_ITEM_FLOAT:

            if (( p_item->f_max == 0.0) && ( p_item->f_min == 0.0))
            {
                /* add input box with default value */
                item_adj = gtk_adjustment_new( p_item->f_value,
                                               0, 99999, 0.01, 10, 10 );
                float_spinbutton = gtk_spin_button_new( GTK_ADJUSTMENT(item_adj),
                                                        0.01, 2 );

                /* connect signal to track changes in the spinbutton value */
                gtk_object_set_data( GTK_OBJECT(float_spinbutton),
                                     "config_option", p_item->psz_name );
                gtk_signal_connect( GTK_OBJECT(float_spinbutton), "changed",
                                    GTK_SIGNAL_FUNC(GtkFloatChanged),
                                    (gpointer)config_dialog );

                LABEL_AND_WIDGET( p_item->psz_text,
                                  float_spinbutton, p_item->psz_longtext );
            }
            else /* use f_min and f_max */
            {
                item_adj = gtk_adjustment_new( p_item->f_value, p_item->f_min,
                                               p_item->f_max, 0.01, 0.01, 0 );
                float_slider = gtk_hscale_new( GTK_ADJUSTMENT(item_adj));
                gtk_scale_set_digits (GTK_SCALE(float_slider), 2);
                                                
                /* connect signal to track changes in the spinbutton value */
                gtk_object_set_data( GTK_OBJECT(item_adj),
                                     "config_option", p_item->psz_name );
                gtk_signal_connect( GTK_OBJECT(item_adj), "value-changed",
                                    GTK_SIGNAL_FUNC(GtkFloatRangedChanged),
                                    (gpointer)config_dialog );

                LABEL_AND_WIDGET( p_item->psz_text,
                                  float_slider, p_item->psz_longtext );
            }
            break;


        case CONFIG_ITEM_BOOL:

            /* add check button */
            bool_checkbutton = gtk_check_button_new();
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(bool_checkbutton),
                                          p_item->i_value );

            /* connect signal to track changes in the button state */
            gtk_object_set_data( GTK_OBJECT(bool_checkbutton), "config_option",
                                 p_item->psz_name );
            gtk_signal_connect( GTK_OBJECT(bool_checkbutton), "toggled",
                                GTK_SIGNAL_FUNC(GtkBoolChanged),
                                (gpointer)config_dialog );

            LABEL_AND_WIDGET( p_item->psz_text,
                              bool_checkbutton, p_item->psz_longtext );
            break;

        }

    }
    while( p_item->i_type != CONFIG_HINT_END && p_item++ );

    vlc_list_release( p_list );

#ifndef MODULE_NAME_IS_gnome
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
#endif

    /* Create the OK button */
#ifdef MODULE_NAME_IS_gnome
    gnome_dialog_append_button( GNOME_DIALOG(config_dialog),
                                GNOME_STOCK_BUTTON_OK );
    ok_button =
        GTK_WIDGET(g_list_last(GNOME_DIALOG(config_dialog)->buttons)->data);

    gnome_dialog_append_button( GNOME_DIALOG(config_dialog),
                                GNOME_STOCK_BUTTON_APPLY );
    apply_button =
        GTK_WIDGET(g_list_last(GNOME_DIALOG(config_dialog)->buttons)->data);

    gnome_dialog_append_button_with_pixmap(
        GNOME_DIALOG(config_dialog), _("Save"), GNOME_STOCK_PIXMAP_SAVE );
    save_button =
        GTK_WIDGET(g_list_last(GNOME_DIALOG(config_dialog)->buttons)->data);

    gnome_dialog_append_button( GNOME_DIALOG(config_dialog),
                                GNOME_STOCK_BUTTON_CANCEL );
    cancel_button =
        GTK_WIDGET(g_list_last(GNOME_DIALOG(config_dialog)->buttons)->data);
#else
    ok_button = gtk_button_new_with_label( _("OK") );
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
#endif

    gtk_signal_connect( GTK_OBJECT(ok_button), "clicked",
                        GTK_SIGNAL_FUNC(GtkConfigOk),
                        config_dialog );
    gtk_widget_set_sensitive( apply_button, FALSE );
    gtk_object_set_data( GTK_OBJECT(config_dialog), "apply_button",
                         apply_button );
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

#undef LABEL_AND_WIDGET
#undef TOOLTIP

/****************************************************************************
 * GtkConfigApply: store the changes to the config inside the modules
 * configuration structure and clear the hash table.
 ****************************************************************************/
static void GtkConfigApply( GtkButton * button, gpointer user_data )
{
    intf_thread_t *p_intf;
    GHashTable *hash_table;
    GtkWidget *apply_button;

    hash_table = (GHashTable *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                    "config_hash_table" );
    p_intf = (intf_thread_t *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                   "p_intf" );
    g_hash_table_foreach_remove( hash_table, GtkSaveHashValue, (void*)p_intf );

    /* change the highlight status of the Apply button */
    apply_button = (GtkWidget *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                    "apply_button" );
    gtk_widget_set_sensitive( apply_button, FALSE );
}

static void GtkConfigOk( GtkButton * button, gpointer user_data )
{
    GtkConfigApply( button, user_data );
    gtk_widget_destroy( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}


static void GtkConfigCancel( GtkButton * button, gpointer user_data )
{
    gtk_widget_destroy( gtk_widget_get_toplevel( GTK_WIDGET (button) ) );
}

static void GtkConfigSave( GtkButton * button, gpointer user_data )
{
    intf_thread_t *p_intf;

    p_intf = (intf_thread_t *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                   "p_intf" );
    GtkConfigApply( button, user_data );
    config_SaveConfigFile( p_intf, NULL );
}

/****************************************************************************
 * GtkModuleHighlighted: display module description when an entry is selected
 *   in the clist, and activate the configure button if necessary.
 ****************************************************************************/
static void GtkModuleHighlighted( GtkCList *module_clist, int row, int column,
                                  GdkEventButton *event, gpointer user_data )
{
    intf_thread_t *p_intf;
    GtkWidget *config_button;
    module_t *p_parser;
    vlc_list_t *p_list;
    char *psz_name;
    int i_index;

    p_intf = (intf_thread_t *)gtk_object_get_data( GTK_OBJECT(module_clist),
                                                   "p_intf" );

    if( !gtk_clist_get_text( GTK_CLIST(module_clist), row, 0, &psz_name ) )
    {
        return;
    }

    /* look for module 'psz_name' */
    p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( !strcmp( p_parser->psz_object_name, psz_name ) )
        {
            gtk_object_set_data( GTK_OBJECT(module_clist),
                                 "module_highlighted", p_parser );
            config_button = gtk_object_get_data( GTK_OBJECT(module_clist),
                                                 "config_button" );
            if( p_parser->i_config_items )
                gtk_widget_set_sensitive( config_button, TRUE );
            else
                gtk_widget_set_sensitive( config_button, FALSE );

            break;
        }
    }

    vlc_list_release( p_list );
}

/****************************************************************************
 * GtkModuleConfigure: display module configuration dialog box.
 ****************************************************************************/
static void GtkModuleConfigure( GtkButton *button, gpointer user_data )
{
    module_t *p_module;
    intf_thread_t *p_intf;

    p_module = (module_t *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                "module_highlighted" );

    if( !p_module ) return;
    p_intf = (intf_thread_t *)gtk_object_get_data( GTK_OBJECT(button),
                                                   "p_intf" );
    GtkCreateConfigDialog( p_module->psz_object_name, (gpointer)p_intf );

}

/****************************************************************************
 * GtkModuleSelected: select module.
 ****************************************************************************/
static void GtkModuleSelected( GtkButton *button, gpointer user_data )
{
    module_t *p_module;
    GtkWidget *widget;

    p_module = (module_t *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                "module_highlighted" );
    widget = (GtkWidget *)gtk_object_get_data( GTK_OBJECT(user_data),
                                               "module_entry" );
    if( !p_module ) return;

    gtk_entry_set_text( GTK_ENTRY(widget), p_module->psz_object_name );

}

/****************************************************************************
 * GtkStringChanged: signal called when the user changes a string value.
 ****************************************************************************/
static void GtkStringChanged( GtkEditable *editable, gpointer user_data )
{
    intf_thread_t *p_intf;
    module_config_t *p_config;
    GHashTable *hash_table;
    GtkWidget *apply_button;

    p_intf = (intf_thread_t *)gtk_object_get_data( GTK_OBJECT(editable),
                                                   "p_intf" );
    hash_table = (GHashTable *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                    "config_hash_table" );
    /* free old p_config */
    p_config = (module_config_t *)g_hash_table_lookup( hash_table,
                                                       (gpointer)editable );
    if( p_config ) GtkFreeHashValue( NULL, (gpointer)p_config, (void *)p_intf );

    p_config = malloc( sizeof(module_config_t) );
    p_config->i_type = CONFIG_ITEM_STRING;
    p_config->psz_value = gtk_editable_get_chars( editable, 0, -1 );
    p_config->psz_name = (char *)gtk_object_get_data( GTK_OBJECT(editable),
                                                      "config_option" );

    g_hash_table_insert( hash_table, (gpointer)editable,
                         (gpointer)p_config );

    /* change the highlight status of the Apply button */
    apply_button = (GtkWidget *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                    "apply_button" );
    gtk_widget_set_sensitive( apply_button, TRUE );
}
/****************************************************************************
 * GtkIntChanged: signal called when the user changes an integer value.
 ****************************************************************************/
static void GtkIntChanged( GtkEditable *editable, gpointer user_data )
{
    intf_thread_t *p_intf;
    module_config_t *p_config;
    GHashTable *hash_table;
    GtkWidget *apply_button;

    p_intf = (intf_thread_t *)gtk_object_get_data( GTK_OBJECT(editable),
                                                   "p_intf" );
    gtk_spin_button_update( GTK_SPIN_BUTTON(editable) );

    hash_table = (GHashTable *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                    "config_hash_table" );

    /* free old p_config */
    p_config = (module_config_t *)g_hash_table_lookup( hash_table,
                                                       (gpointer)editable );
    if( p_config ) GtkFreeHashValue( NULL, (gpointer)p_config, (void *)p_intf );

    p_config = malloc( sizeof(module_config_t) );
    p_config->i_type = CONFIG_ITEM_INTEGER;
    p_config->i_value = gtk_spin_button_get_value_as_int(
                            GTK_SPIN_BUTTON(editable) );
    p_config->psz_name = (char *)gtk_object_get_data( GTK_OBJECT(editable),
                                                      "config_option" );

    g_hash_table_insert( hash_table, (gpointer)editable,
                         (gpointer)p_config );

    /* change the highlight status of the Apply button */
    apply_button = (GtkWidget *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                    "apply_button" );
    gtk_widget_set_sensitive( apply_button, TRUE );
}


/***************************************************************************************
 * GtkIntRangedChanged: signal called when the user changes an integer with range value.
 **************************************************************************************/
static void GtkIntRangedChanged( GtkEditable *editable, gpointer user_data )
{
    intf_thread_t *p_intf;
    module_config_t *p_config;
    GHashTable *hash_table;
    GtkWidget *apply_button;

    p_intf = (intf_thread_t *)gtk_object_get_data( GTK_OBJECT(editable),
                                                   "p_intf" );

    hash_table = (GHashTable *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                    "config_hash_table" );

    /* free old p_config */
    p_config = (module_config_t *)g_hash_table_lookup( hash_table,
                                                       (gpointer)editable );
    if( p_config ) GtkFreeHashValue( NULL, (gpointer)p_config, (void *)p_intf );

    p_config = malloc( sizeof(module_config_t) );
    p_config->i_type = CONFIG_ITEM_INTEGER;
    p_config->i_value = ((GTK_ADJUSTMENT(editable))->value);
    p_config->psz_name = (char *)gtk_object_get_data( GTK_OBJECT(editable),
                                                      "config_option" );

    g_hash_table_insert( hash_table, (gpointer)editable,
                         (gpointer)p_config );

    /* change the highlight status of the Apply button */
    apply_button = (GtkWidget *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                    "apply_button" );
    gtk_widget_set_sensitive( apply_button, TRUE );
}

/****************************************************************************
 * GtkFloatChanged: signal called when the user changes a float value.
 ****************************************************************************/
static void GtkFloatChanged( GtkEditable *editable, gpointer user_data )
{
    intf_thread_t *p_intf;
    module_config_t *p_config;
    GHashTable *hash_table;
    GtkWidget *apply_button;

    p_intf = (intf_thread_t *)gtk_object_get_data( GTK_OBJECT(editable),
                                                   "p_intf" );
    gtk_spin_button_update( GTK_SPIN_BUTTON(editable) );

    hash_table = (GHashTable *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                    "config_hash_table" );

    /* free old p_config */
    p_config = (module_config_t *)g_hash_table_lookup( hash_table,
                                                       (gpointer)editable );
    if( p_config ) GtkFreeHashValue( NULL, (gpointer)p_config, (void *)p_intf );

    p_config = malloc( sizeof(module_config_t) );
    p_config->i_type = CONFIG_ITEM_FLOAT;
    p_config->f_value = gtk_spin_button_get_value_as_float(
                           GTK_SPIN_BUTTON(editable) );
    p_config->psz_name = (char *)gtk_object_get_data( GTK_OBJECT(editable),
                                                      "config_option" );

    g_hash_table_insert( hash_table, (gpointer)editable,
                         (gpointer)p_config );

    /* change the highlight status of the Apply button */
    apply_button = (GtkWidget *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                    "apply_button" );
    gtk_widget_set_sensitive( apply_button, TRUE );
}

/***************************************************************************************
 * GtkIntRangedChanged: signal called when the user changes an integer with range value.
 **************************************************************************************/
static void GtkFloatRangedChanged( GtkEditable *editable, gpointer user_data )
{
    intf_thread_t *p_intf;
    module_config_t *p_config;
    GHashTable *hash_table;
    GtkWidget *apply_button;

    p_intf = (intf_thread_t *)gtk_object_get_data( GTK_OBJECT(editable),
                                                   "p_intf" );

    hash_table = (GHashTable *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                    "config_hash_table" );

    /* free old p_config */
    p_config = (module_config_t *)g_hash_table_lookup( hash_table,
                                                       (gpointer)editable );
    if( p_config ) GtkFreeHashValue( NULL, (gpointer)p_config, (void *)p_intf );

    p_config = malloc( sizeof(module_config_t) );
    p_config->i_type = CONFIG_ITEM_FLOAT;
    p_config->f_value = ((GTK_ADJUSTMENT(editable))->value);
    p_config->psz_name = (char *)gtk_object_get_data( GTK_OBJECT(editable),
                                                      "config_option" );

    g_hash_table_insert( hash_table, (gpointer)editable,
                         (gpointer)p_config );

    /* change the highlight status of the Apply button */
    apply_button = (GtkWidget *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                    "apply_button" );
    gtk_widget_set_sensitive( apply_button, TRUE );
}

/****************************************************************************
 * GtkBoolChanged: signal called when the user changes a bool value.
 ****************************************************************************/
static void GtkBoolChanged( GtkToggleButton *button, gpointer user_data )
{
    intf_thread_t *p_intf;
    module_config_t *p_config;
    GHashTable *hash_table;
    GtkWidget *apply_button;

    p_intf = (intf_thread_t *)gtk_object_get_data( GTK_OBJECT(button),
                                                   "p_intf" );
    hash_table = (GHashTable *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                    "config_hash_table" );

    /* free old p_config */
    p_config = (module_config_t *)g_hash_table_lookup( hash_table,
                                                       (gpointer)button );
    if( p_config ) GtkFreeHashValue( NULL, (gpointer)p_config, (void *)p_intf );

    p_config = malloc( sizeof(module_config_t) );
    p_config->i_type = CONFIG_ITEM_BOOL;
    p_config->i_value = gtk_toggle_button_get_active( button );
    p_config->psz_name = (char *)gtk_object_get_data( GTK_OBJECT(button),
                                                      "config_option" );

    g_hash_table_insert( hash_table, (gpointer)button,
                         (gpointer)p_config );

    /* change the highlight status of the Apply button */
    apply_button = (GtkWidget *)gtk_object_get_data( GTK_OBJECT(user_data),
                                                     "apply_button" );
    gtk_widget_set_sensitive( apply_button, TRUE );
}

/****************************************************************************
 * GtkFreeHashTable: signal called when the config hash table is destroyed.
 ****************************************************************************/
static void GtkFreeHashTable( GtkObject *object )
{
    GHashTable *hash_table = (GHashTable *)gtk_object_get_data( object,
                                                         "config_hash_table" );
    intf_thread_t *p_intf = (intf_thread_t *)gtk_object_get_data( object,
                                                                  "p_intf" );

    g_hash_table_foreach( hash_table, GtkFreeHashValue, (void *)p_intf );
    g_hash_table_destroy( hash_table );
}

/****************************************************************************
 * GtkFreeHashValue: signal called when an element of the config hash table
 * is destroyed.
 ****************************************************************************/
static void GtkFreeHashValue( gpointer key, gpointer value, gpointer user_data)
{
    module_config_t * p_config = (module_config_t *)value;

    if( p_config->i_type == CONFIG_ITEM_STRING )
        if( p_config->psz_value ) g_free( p_config->psz_value );
    free( p_config );
}

/****************************************************************************
 * GtkSaveHashValue: callback used when enumerating the hash table in
 * GtkConfigApply().
 ****************************************************************************/
static gboolean GtkSaveHashValue( gpointer key, gpointer value,
                                  gpointer user_data )
{
    intf_thread_t *   p_intf   = (intf_thread_t *)user_data;
    module_config_t * p_config = (module_config_t *)value;

    switch( p_config->i_type )
    {

    case CONFIG_ITEM_STRING:
    case CONFIG_ITEM_FILE:
    case CONFIG_ITEM_DIRECTORY:
    case CONFIG_ITEM_MODULE:
        config_PutPsz( p_intf, p_config->psz_name,
                       *p_config->psz_value ? p_config->psz_value : NULL );
        break;
    case CONFIG_ITEM_INTEGER:
    case CONFIG_ITEM_BOOL:
        config_PutInt( p_intf, p_config->psz_name, p_config->i_value );
        break;
    case CONFIG_ITEM_FLOAT:
        config_PutFloat( p_intf, p_config->psz_name, p_config->f_value );
        break;
    }

    /* free the hash value we allocated */
    if( p_config->i_type == CONFIG_ITEM_STRING )
        g_free( p_config->psz_value );
    free( p_config );

    /* return TRUE so glib will free the hash entry */
    return TRUE;
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

    GtkFreeHashTable( object );
}
