/*****************************************************************************
 * wrappers.h: Wrappers around C++ objects
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: wrappers.h,v 1.3 2003/03/19 17:14:50 karibu Exp $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
 *          Emmanuel Puig    <karibu@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/


//---------------------------------------------------------------------------
#if defined(__cplusplus)
extern "C" {
#endif


//---------------------------------------------------------------------------
// Divers
//---------------------------------------------------------------------------
    void AddAnchor( char *x, char *y, char *len, char *priority );
    void AddBitmap( char *name, char *file, char *transcolor );
    void AddEvent( char *name, char *event, char *key );
    void AddFont( char *name, char *font, char *size,
                  char *color, char *weight, char *italic, char *underline );
    void StartControlGroup( char *x, char *y );
    void EndControlGroup();

//---------------------------------------------------------------------------
// Theme
//---------------------------------------------------------------------------
    void AddThemeInfo( char *name, char *author, char *email,
                       char *webpage );
    void StartTheme( char *log, char *magnet );
    void EndTheme();

//---------------------------------------------------------------------------
// Window
//---------------------------------------------------------------------------
    void StartWindow( char *name, char *x, char *y, char *visible,
                      char *fadetime, char *alpha, char *movealpha,
                      char *dragdrop );
    void EndWindow();

//---------------------------------------------------------------------------
// Control
//---------------------------------------------------------------------------
    void AddImage( char *id, char *visible, char *x, char *y, char *image,
                   char *event, char *help );

    void AddRectangle( char *id, char *visible, char *x, char *y, char *w,
                       char *h, char *color, char *event, char *help );

    void AddButton( char *id,
                    char *visible,
                    char *x, char *y,
                    char *up, char *down, char *disabled,
                    char *onclick, char *onmouseover, char *onmouseout,
                    char *tooltiptext, char *help );

    void AddCheckBox( char *id,
                      char *visible,
                      char *x, char *y,
                      char *img1, char *img2, char *clickimg1, char *clickimg2,
                      char *disabled1, char *disabled2,
                      char *onclick1, char *onclick2, char *onmouseover1,
                      char *onmouseout1, char *onmouseover2, char *onmouseout2,
                      char *tooltiptext1, char *tooltiptext2, char *help );

    void AddSlider( char *id, char *visible, char *x, char *y, char *type,
                    char *up, char *down, char *abs, char *ord,
                    char *tooltiptext, char *help );

    void AddText( char *id, char *visible, char *x, char *y, char *text,
                  char *font, char *align, char *width, char *display,
                  char *scroll, char *scrollspace, char *help );

    void AddPlayList( char *id, char *visible, char *x, char *y, char *width,
                      char *infowidth, char *font, char *playfont,
                      char *selcolor, char *abs, char *ord,
                      char *longfilename, char *help );

    void AddPlayListEnd();
//---------------------------------------------------------------------------

#if defined(__cplusplus)
}
#endif
