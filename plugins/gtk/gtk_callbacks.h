#include <gtk/gtk.h>
#include "gtk_control.h"
#include "gtk_menu.h"
#include "gtk_open.h"
#include "gtk_modules.h"
#include "gtk_playlist.h"
#include "gtk_preferences.h"

/* General glade callbacks */

/* main window callbacks: specific prototypes are in headers listed before */

gboolean GtkExit                ( GtkWidget *, GdkEventButton *, gpointer );
gboolean GtkWindowToggle        ( GtkWidget *, GdkEventButton *, gpointer );
gboolean GtkFullscreen          ( GtkWidget *, GdkEventButton *, gpointer );
gboolean GtkSliderRelease       ( GtkWidget *, GdkEventButton *, gpointer );
gboolean GtkSliderPress         ( GtkWidget *, GdkEventButton *, gpointer );
gboolean GtkWindowDelete        ( GtkWidget * widget, GdkEvent *, gpointer );
gboolean GtkJumpShow            ( GtkWidget *, GdkEventButton *, gpointer );
gboolean GtkAboutShow           ( GtkWidget *, GdkEventButton *, gpointer );
void     GtkTitlePrev           ( GtkButton * button, gpointer );
void     GtkTitleNext           ( GtkButton * button, gpointer );
void     GtkChapterPrev         ( GtkButton *, gpointer );
void     GtkChapterNext         ( GtkButton * button, gpointer );
void     GtkAboutOk             ( GtkButton *, gpointer );
void     GtkWindowDrag          ( GtkWidget *, GdkDragContext *,
                                  gint, gint, GtkSelectionData *,
                                  guint , guint, gpointer );
void     GtkJumpOk              ( GtkButton * button, gpointer );
void     GtkJumpCancel          ( GtkButton * button, gpointer user_data );
void     GtkExitActivate        ( GtkMenuItem *, gpointer );
void     GtkWindowToggleActivate( GtkMenuItem *, gpointer );
void     GtkFullscreenActivate  ( GtkMenuItem *, gpointer );
void     GtkAboutActivate       ( GtkMenuItem *, gpointer );
void     GtkJumpActivate        ( GtkMenuItem *, gpointer );

void
GtkPlaylistDestroy                     (GtkObject       *object,
                                        gpointer         user_data);
