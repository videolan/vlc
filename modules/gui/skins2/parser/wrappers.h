#if defined(__cplusplus)
extern "C" {
#endif


//---------------------------------------------------------------------------
// Divers
//---------------------------------------------------------------------------
void AddAnchor( void *pContext, char *x, char *y, char *len,
                char *priority );
void AddBitmap( void *pContext, char *name, char *file,
                char *transcolor );
void AddEvent( void *pContext, char *name, char *event, char *key );
void AddFont( void *pContext, char *name, char *font, char *size,
              char *color, char *italic, char *underline );
void StartGroup( void *pContext, char *x, char *y );
void EndGroup( void *pContext );

//---------------------------------------------------------------------------
// Theme
//---------------------------------------------------------------------------
void AddThemeInfo( void *pContext, char *name, char *author,
                   char *email, char *webpage );
void StartTheme( void *pContext, char *version, char *magnet,
                 char *alpha, char *movealpha, char *fadetime );
void EndTheme( void *pContext );

//---------------------------------------------------------------------------
// Window
//---------------------------------------------------------------------------
void StartWindow( void *pContext, char *name, char *x, char *y,
                  char *visible, char *dragdrop, char *playOnDrop );
void EndWindow( void *pContext );

//---------------------------------------------------------------------------
// Layout
//---------------------------------------------------------------------------
void StartLayout( void *pContext, char *id, char *width, char *height,
                  char *minwidth, char *maxwidth, char *minheight,
                  char *maxheight );
void EndLayout( void *pContext );

//---------------------------------------------------------------------------
// Control
//---------------------------------------------------------------------------
void AddImage( void *pContext, char *id, char *visible, char *x,
               char *y, char *lefttop, char *rightbottom, char *image,
               char *event, char *help );

void AddRectangle( void *pContext, char *id, char *visible, char *x,
                   char *y, char *w, char *h, char *color, char *event,
                   char *help );

void AddButton( void *pContext, char *id,
                char *x, char *y, char *lefttop, char *rightbottom,
                char *up, char *down, char *over,
                char *action, char *tooltiptext, char *help );

void AddCheckBox( void *pContext, char *id,
                  char *x, char *y, char *lefttop, char *rightbottom,
                  char *up1, char *down1, char *over1, char *up2,
                  char *down2, char *over2, char *state, char *action1,
                  char *action2, char *tooltiptext1, char *tooltiptext2,
                  char *help );

void AddSlider( void *pContext, char *id, char *visible, char *x, char *y,
                char *lefttop, char *rightbottom,
                char *up, char *down, char *over, char *points,
                char *thickness, char *value, char *tooltiptext, char *help );

void AddRadialSlider( void *pContext, char *id, char *visible, char *x, char *y,
                      char *lefttop, char *rightbottom, char *sequence,
                      char *nbImages, char *minAngle, char *maxAngle,
                      char *value, char *tooltiptext, char *help );

void AddText( void *pContext, char *id, char *visible, char *x, char *y,
              char *text, char *font, char *align, char *width, char *display,
              char *scroll, char *scrollspace, char *help );

void AddPlaylist( void *pContext, char *id, char *visible, char *x,
              char *y, char *width, char *height, char *lefttop,
              char *rightbottom, char *font, char *var, char *fgcolor,
              char *playcolor, char *bgcolor1, char *bgcolor2, char *selcolor,
              char *help );

void AddPlaylistEnd( void *pContext );
//---------------------------------------------------------------------------

#if defined(__cplusplus)
}
#endif
