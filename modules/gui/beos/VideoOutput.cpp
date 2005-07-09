/*****************************************************************************
 * vout_beos.cpp: beos video output display method
 *****************************************************************************
 * Copyright (C) 2000, 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Tony Castley <tcastley@mail.powerup.com.au>
 *          Richard Shepherd <richard@rshepherd.demon.co.uk>
 *          Stephan Aßmus <stippi@yellowbites.com>
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
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <stdio.h>
#include <string.h>                                            /* strerror() */

#include <Application.h>
#include <BitmapStream.h>
#include <Bitmap.h>
#include <Directory.h>
#include <DirectWindow.h>
#include <File.h>
#include <InterfaceKit.h>
#include <NodeInfo.h>
#include <String.h>
#include <TranslatorRoster.h>
#include <WindowScreen.h>

/* VLC headers */
#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>
#include <vlc_keys.h>

#include "InterfaceWindow.h"    // for load/save_settings()
#include "DrawingTidbits.h"
#include "MsgVals.h"

#include "VideoWindow.h"

/*****************************************************************************
 * vout_sys_t: BeOS video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the BeOS specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    VideoWindow *  p_window;

    int32_t i_width;
    int32_t i_height;

//    uint8_t *pp_buffer[3];
    uint32_t source_chroma;
    int i_index;

};

#define MOUSE_IDLE_TIMEOUT 2000000    // two seconds
#define MIN_AUTO_VSYNC_REFRESH 61    // Hz

/*****************************************************************************
 * beos_GetAppWindow : retrieve a BWindow pointer from the window name
 *****************************************************************************/
BWindow*
beos_GetAppWindow(char *name)
{
    int32_t     index;
    BWindow     *window;

    for (index = 0 ; ; index++)
    {
        window = be_app->WindowAt(index);
        if (window == NULL)
            break;
        if (window->LockWithTimeout(20000) == B_OK)
        {
            if (strcmp(window->Name(), name) == 0)
            {
                window->Unlock();
                break;
            }
            window->Unlock();
        }
    }
    return window;
}

static const int beos_keys[][2] =
{
    { B_LEFT_ARROW,  KEY_LEFT },
    { B_RIGHT_ARROW, KEY_RIGHT },
    { B_UP_ARROW,    KEY_UP },
    { B_DOWN_ARROW,  KEY_DOWN },
    { B_SPACE,       KEY_SPACE },
    { B_ENTER,       KEY_ENTER },
    { B_F1_KEY,      KEY_F1 },
    { B_F2_KEY,      KEY_F2 },
    { B_F3_KEY,      KEY_F3 },
    { B_F4_KEY,      KEY_F4 },
    { B_F5_KEY,      KEY_F5 },
    { B_F6_KEY,      KEY_F6 },
    { B_F7_KEY,      KEY_F7 },
    { B_F8_KEY,      KEY_F8 },
    { B_F9_KEY,      KEY_F9 },
    { B_F10_KEY,     KEY_F10 },
    { B_F11_KEY,     KEY_F11 },
    { B_F12_KEY,     KEY_F12 },
    { B_HOME,        KEY_HOME },
    { B_END,         KEY_END },
    { B_ESCAPE,      KEY_ESC },
    { B_PAGE_UP,     KEY_PAGEUP },
    { B_PAGE_DOWN,   KEY_PAGEDOWN },
    { B_TAB,         KEY_TAB },
    { B_BACKSPACE,   KEY_BACKSPACE }
};

static int ConvertKeyFromVLC( int key )
{
    for( unsigned i = 0; i < sizeof( beos_keys ) / sizeof( int ) / 2; i++ )
    {
        if( beos_keys[i][1] == key )
        {
            return beos_keys[i][0];
        }
    }
    return key;
}

static int ConvertKeyToVLC( int key )
{
    for( unsigned i = 0; i < sizeof( beos_keys ) / sizeof( int ) / 2; i++ )
    {
        if( beos_keys[i][0] == key )
        {
            return beos_keys[i][1];
        }
    }
    return key;
}

/*****************************************************************************
 * get_interface_window
 *****************************************************************************/
BWindow*
get_interface_window()
{
    return beos_GetAppWindow( "VLC " PACKAGE_VERSION );
}

class BackgroundView : public BView
{
 public:
                            BackgroundView(BRect frame, VLCView* view)
                            : BView(frame, "background",
                                    B_FOLLOW_ALL, B_FULL_UPDATE_ON_RESIZE),
                              fVideoView(view)
                            {
                                SetViewColor(kBlack);
                            }
    virtual                    ~BackgroundView() {}

    virtual    void            MouseDown(BPoint where)
                            {
                                // convert coordinates
                                where = fVideoView->ConvertFromParent(where);
                                // let him handle it
                                fVideoView->MouseDown(where);
                            }
    virtual    void            MouseMoved(BPoint where, uint32_t transit,
                                       const BMessage* dragMessage)
                            {
                                // convert coordinates
                                where = fVideoView->ConvertFromParent(where);
                                // let him handle it
                                fVideoView->MouseMoved(where, transit, dragMessage);
                                // notice: It might look like transit should be
                                // B_OUTSIDE_VIEW regardless, but leave it like this,
                                // otherwise, unwanted things will happen!
                            }

 private:
    VLCView*                fVideoView;
};


/*****************************************************************************
 * VideoSettings constructor and destructor
 *****************************************************************************/
VideoSettings::VideoSettings()
    : fVideoSize( SIZE_100 ),
      fFlags( FLAG_CORRECT_RATIO ),
      fSettings( new BMessage( 'sett' ) )
{
    // read settings from disk
    status_t ret = load_settings( fSettings, "video_settings", "VideoLAN Client" );
    if ( ret == B_OK )
    {
        uint32_t flags;
        if ( fSettings->FindInt32( "flags", (int32*)&flags ) == B_OK )
            SetFlags( flags );
        uint32_t size;
        if ( fSettings->FindInt32( "video size", (int32*)&size ) == B_OK )
            SetVideoSize( size );
    }
    else
    {
        // figure out if we should use vertical sync by default
        BScreen screen(B_MAIN_SCREEN_ID);
        if (screen.IsValid())
        {
            display_mode mode;
            screen.GetMode(&mode);
            float refresh = (mode.timing.pixel_clock * 1000)
                            / ((mode.timing.h_total)* (mode.timing.v_total));
            if (refresh < MIN_AUTO_VSYNC_REFRESH)
                AddFlags(FLAG_SYNC_RETRACE);
        }
    }
}

VideoSettings::VideoSettings( const VideoSettings& clone )
    : fVideoSize( clone.VideoSize() ),
      fFlags( clone.Flags() ),
      fSettings( NULL )
{
}


VideoSettings::~VideoSettings()
{
    if ( fSettings )
    {
        // we are the default settings
        // and write our settings to disk
        if (fSettings->ReplaceInt32( "video size", VideoSize() ) != B_OK)
            fSettings->AddInt32( "video size", VideoSize() );
        if (fSettings->ReplaceInt32( "flags", Flags() ) != B_OK)
            fSettings->AddInt32( "flags", Flags() );

        save_settings( fSettings, "video_settings", "VideoLAN Client" );
        delete fSettings;
    }
    else
    {
        // we are just a clone of the default settings
        fDefaultSettings.SetVideoSize( VideoSize() );
        fDefaultSettings.SetFlags( Flags() );
    }
}

/*****************************************************************************
 * VideoSettings::DefaultSettings
 *****************************************************************************/
VideoSettings*
VideoSettings::DefaultSettings()
{
    return &fDefaultSettings;
}

/*****************************************************************************
 * VideoSettings::SetVideoSize
 *****************************************************************************/
void
VideoSettings::SetVideoSize( uint32_t mode )
{
    fVideoSize = mode;
}

// static variable initialization
VideoSettings
VideoSettings::fDefaultSettings;


/*****************************************************************************
 * VideoWindow constructor and destructor
 *****************************************************************************/
VideoWindow::VideoWindow(int v_width, int v_height, BRect frame,
                         vout_thread_t *p_videoout)
    : BWindow(frame, NULL, B_TITLED_WINDOW, B_NOT_CLOSABLE | B_NOT_MINIMIZABLE),
      i_width(frame.IntegerWidth()),
      i_height(frame.IntegerHeight()),
      winSize(frame),
      i_buffer(0),
      teardownwindow(false),
      fTrueWidth(v_width),
      fTrueHeight(v_height),
      fCachedFeel(B_NORMAL_WINDOW_FEEL),
      fInterfaceShowing(false),
      fInitStatus(B_ERROR),
      fSettings(new VideoSettings(*VideoSettings::DefaultSettings()))
{
    p_vout = p_videoout;

    // create the view to do the display
    view = new VLCView( Bounds(), p_vout );

    // create background view
    BView *mainView =  new BackgroundView( Bounds(), view );
    AddChild(mainView);
    mainView->AddChild(view);

    // allocate bitmap buffers
    for (int32_t i = 0; i < 3; i++)
        bitmap[i] = NULL;
    fInitStatus = _AllocateBuffers(v_width, v_height, &mode);

    // make sure we layout the view correctly
    FrameResized(i_width, i_height);

    if (fInitStatus >= B_OK && mode == OVERLAY)
    {
       overlay_restrictions r;

       bitmap[0]->GetOverlayRestrictions(&r);
       SetSizeLimits((i_width * r.min_width_scale), i_width * r.max_width_scale,
                     (i_height * r.min_height_scale), i_height * r.max_height_scale);
    }

    // vlc settings override settings from disk
    if (config_GetInt(p_vout, "fullscreen"))
        fSettings->AddFlags(VideoSettings::FLAG_FULL_SCREEN);

    _SetToSettings();
}

VideoWindow::~VideoWindow()
{
    int32 result;

    teardownwindow = true;
    wait_for_thread(fDrawThreadID, &result);
    _FreeBuffers();
    delete fSettings;
}

/*****************************************************************************
 * VideoWindow::MessageReceived
 *****************************************************************************/
void
VideoWindow::MessageReceived( BMessage *p_message )
{
    switch( p_message->what )
    {
        case SHOW_INTERFACE:
            SetInterfaceShowing( true );
            break;
        case TOGGLE_FULL_SCREEN:
            BWindow::Zoom();
            break;
        case RESIZE_50:
        case RESIZE_100:
        case RESIZE_200:
            if (IsFullScreen())
                BWindow::Zoom();
            _SetVideoSize(p_message->what);
            break;
        case VERT_SYNC:
            SetSyncToRetrace(!IsSyncedToRetrace());
            break;
        case WINDOW_FEEL:
            {
                window_feel winFeel;
                if (p_message->FindInt32("WinFeel", (int32*)&winFeel) == B_OK)
                {
                    SetFeel(winFeel);
                    fCachedFeel = winFeel;
                    if (winFeel == B_FLOATING_ALL_WINDOW_FEEL)
                        fSettings->AddFlags(VideoSettings::FLAG_ON_TOP_ALL);
                    else
                        fSettings->ClearFlags(VideoSettings::FLAG_ON_TOP_ALL);
                }
            }
            break;
        case ASPECT_CORRECT:
            SetCorrectAspectRatio(!CorrectAspectRatio());
            break;

        case B_KEY_DOWN:
        case B_UNMAPPED_KEY_DOWN:
        case B_KEY_UP:
        case B_UNMAPPED_KEY_UP:
        {
            key_map * keys;
            char    * chars;
            int32     key, modifiers;

            if( p_message->FindInt32( "key", &key ) != B_OK ||
                p_message->FindInt32( "modifiers", &modifiers ) != B_OK )
            {
                /* Shouldn't happen */
                break;
            }

            if( ( p_message->what == B_KEY_UP ||
                  p_message->what == B_UNMAPPED_KEY_UP ) &&
                !( modifiers & B_COMMAND_KEY ) )
            {
                /* We only use the KEY_UP messages to detect Alt+X
                   shortcuts (because the KEY_DOWN messages aren't
                   sent when Alt is pressed) */
                break;
            }

            /* Special case for Alt+1, Alt+2 and Alt+3 shortcuts: since
               the character depends on the keymap, we use the key codes
               directly (18, 19, 20) */
            if( ( modifiers & B_COMMAND_KEY ) &&
                key >= 18 && key <= 20 )
            {
                if( key == 18 )
                    PostMessage( RESIZE_50 );
                else if( key == 19 )
                    PostMessage( RESIZE_100 );
                else
                    PostMessage( RESIZE_200 );

                break;
            }

            /* Get the current keymap */
            get_key_map( &keys, &chars );

            if( key >= 128 || chars[keys->normal_map[key]] != 1 )
            {
                /* Weird key or Unicode character */
                free( keys );
                free( chars );
                break;
            }

            vlc_value_t val;
            val.i_int = ConvertKeyToVLC( chars[keys->normal_map[key]+1] );

            if( modifiers & B_COMMAND_KEY )
            {
                val.i_int |= KEY_MODIFIER_ALT;
            }
            if( modifiers & B_SHIFT_KEY )
            {
                val.i_int |= KEY_MODIFIER_SHIFT;
            }
            if( modifiers & B_CONTROL_KEY )
            {
                val.i_int |= KEY_MODIFIER_CTRL;
            }
            var_Set( p_vout->p_vlc, "key-pressed", val );

            free( keys );
            free( chars );
            break;
        }

        default:
            BWindow::MessageReceived( p_message );
            break;
    }
}

/*****************************************************************************
 * VideoWindow::Zoom
 *****************************************************************************/
void
VideoWindow::Zoom(BPoint origin, float width, float height )
{
    ToggleFullScreen();
}

/*****************************************************************************
 * VideoWindow::FrameMoved
 *****************************************************************************/
void
VideoWindow::FrameMoved(BPoint origin)
{
    if (IsFullScreen())
        return ;
    winSize = Frame();
}

/*****************************************************************************
 * VideoWindow::FrameResized
 *****************************************************************************/
void
VideoWindow::FrameResized( float width, float height )
{
    int32_t useWidth = CorrectAspectRatio() ? i_width : fTrueWidth;
    int32_t useHeight = CorrectAspectRatio() ? i_height : fTrueHeight;
    float out_width, out_height;
    float out_left, out_top;
    float width_scale = width / useWidth;
    float height_scale = height / useHeight;

    if (width_scale <= height_scale)
    {
        out_width = (useWidth * width_scale);
        out_height = (useHeight * width_scale);
        out_left = 0;
        out_top = (height - out_height) / 2;
    }
    else   /* if the height is proportionally smaller */
    {
        out_width = (useWidth * height_scale);
        out_height = (useHeight * height_scale);
        out_top = 0;
        out_left = (width - out_width) / 2;
    }
    view->MoveTo(out_left,out_top);
    view->ResizeTo(out_width, out_height);

    if (!IsFullScreen())
        winSize = Frame();
}

/*****************************************************************************
 * VideoWindow::ScreenChanged
 *****************************************************************************/
void
VideoWindow::ScreenChanged(BRect frame, color_space format)
{
    BScreen screen(this);
    display_mode mode;
    screen.GetMode(&mode);
    float refresh = (mode.timing.pixel_clock * 1000)
                    / ((mode.timing.h_total) * (mode.timing.v_total));
    SetSyncToRetrace(refresh < MIN_AUTO_VSYNC_REFRESH);
}

/*****************************************************************************
 * VideoWindow::Activate
 *****************************************************************************/
void
VideoWindow::WindowActivated(bool active)
{
}

/*****************************************************************************
 * VideoWindow::drawBuffer
 *****************************************************************************/
void
VideoWindow::drawBuffer(int bufferIndex)
{
    i_buffer = bufferIndex;

    // sync to the screen if required
    if (IsSyncedToRetrace())
    {
        BScreen screen(this);
        screen.WaitForRetrace(22000);
    }
    if (fInitStatus >= B_OK && LockLooper())
    {
       // switch the overlay bitmap
       if (mode == OVERLAY)
       {
          rgb_color key;
          view->SetViewOverlay(bitmap[i_buffer],
                            bitmap[i_buffer]->Bounds() ,
                            view->Bounds(),
                            &key, B_FOLLOW_ALL,
                            B_OVERLAY_FILTER_HORIZONTAL|B_OVERLAY_FILTER_VERTICAL|
                            B_OVERLAY_TRANSFER_CHANNEL);
           view->SetViewColor(key);
       }
       else
       {
         // switch the bitmap
         view->DrawBitmap(bitmap[i_buffer], view->Bounds() );
       }
       UnlockLooper();
    }
}

/*****************************************************************************
 * VideoWindow::SetInterfaceShowing
 *****************************************************************************/
void
VideoWindow::ToggleInterfaceShowing()
{
    SetInterfaceShowing(!fInterfaceShowing);
}

/*****************************************************************************
 * VideoWindow::SetInterfaceShowing
 *****************************************************************************/
void
VideoWindow::SetInterfaceShowing(bool showIt)
{
    BWindow* window = get_interface_window();
    if (window)
    {
        if (showIt)
        {
            if (fCachedFeel != B_NORMAL_WINDOW_FEEL)
                SetFeel(B_NORMAL_WINDOW_FEEL);
            window->Activate(true);
            SendBehind(window);
        }
        else
        {
            SetFeel(fCachedFeel);
            Activate(true);
            window->SendBehind(this);
        }
        fInterfaceShowing = showIt;
    }
}

/*****************************************************************************
 * VideoWindow::SetCorrectAspectRatio
 *****************************************************************************/
void
VideoWindow::SetCorrectAspectRatio(bool doIt)
{
    if (CorrectAspectRatio() != doIt)
    {
        if (doIt)
            fSettings->AddFlags(VideoSettings::FLAG_CORRECT_RATIO);
        else
            fSettings->ClearFlags(VideoSettings::FLAG_CORRECT_RATIO);
        FrameResized(Bounds().Width(), Bounds().Height());
    }
}

/*****************************************************************************
 * VideoWindow::CorrectAspectRatio
 *****************************************************************************/
bool
VideoWindow::CorrectAspectRatio() const
{
    return fSettings->HasFlags(VideoSettings::FLAG_CORRECT_RATIO);
}

/*****************************************************************************
 * VideoWindow::ToggleFullScreen
 *****************************************************************************/
void
VideoWindow::ToggleFullScreen()
{
    SetFullScreen(!IsFullScreen());
}

/*****************************************************************************
 * VideoWindow::SetFullScreen
 *****************************************************************************/
void
VideoWindow::SetFullScreen(bool doIt)
{
    if (doIt)
    {
        SetLook( B_NO_BORDER_WINDOW_LOOK );
        BScreen screen( this );
        BRect rect = screen.Frame();
        Activate();
        MoveTo(0.0, 0.0);
        ResizeTo(rect.IntegerWidth(), rect.IntegerHeight());
        be_app->ObscureCursor();
        fInterfaceShowing = false;
        fSettings->AddFlags(VideoSettings::FLAG_FULL_SCREEN);
    }
    else
    {
        SetLook( B_TITLED_WINDOW_LOOK );
        MoveTo(winSize.left, winSize.top);
        ResizeTo(winSize.IntegerWidth(), winSize.IntegerHeight());
        be_app->ShowCursor();
        fInterfaceShowing = true;
        fSettings->ClearFlags(VideoSettings::FLAG_FULL_SCREEN);
    }
}

/*****************************************************************************
 * VideoWindow::IsFullScreen
 *****************************************************************************/
bool
VideoWindow::IsFullScreen() const
{
    return fSettings->HasFlags(VideoSettings::FLAG_FULL_SCREEN);
}

/*****************************************************************************
 * VideoWindow::SetSyncToRetrace
 *****************************************************************************/
void
VideoWindow::SetSyncToRetrace(bool doIt)
{
    if (doIt)
        fSettings->AddFlags(VideoSettings::FLAG_SYNC_RETRACE);
    else
        fSettings->ClearFlags(VideoSettings::FLAG_SYNC_RETRACE);
}

/*****************************************************************************
 * VideoWindow::IsSyncedToRetrace
 *****************************************************************************/
bool
VideoWindow::IsSyncedToRetrace() const
{
    return fSettings->HasFlags(VideoSettings::FLAG_SYNC_RETRACE);
}


/*****************************************************************************
 * VideoWindow::_AllocateBuffers
 *****************************************************************************/
status_t
VideoWindow::_AllocateBuffers(int width, int height, int* mode)
{
    // clear any old buffers
    _FreeBuffers();
    // set default mode
    *mode = BITMAP;
    bitmap_count = 3;

    BRect bitmapFrame( 0, 0, width, height );
    // read from config, if we are supposed to use overlay at all
    int noOverlay = !config_GetInt( p_vout, "overlay" );

    /* Test for overlay capability: for every chroma in colspace,
       we try to do double-buffered overlay, single-buffered overlay
       or basic overlay. If nothing worked, we then have to work with
       a non-overlay BBitmap. */
    for( int i = 0; i < COLOR_COUNT; i++ )
    {
        if( noOverlay )
            break;

        bitmap[0] = new BBitmap( bitmapFrame,
                                 B_BITMAP_WILL_OVERLAY |
                                 B_BITMAP_RESERVE_OVERLAY_CHANNEL,
                                 colspace[i].colspace );
        if( bitmap[0] && bitmap[0]->InitCheck() == B_OK )
        {
            colspace_index = i;

            *mode = OVERLAY;
            rgb_color key;
            view->SetViewOverlay( bitmap[0], bitmap[0]->Bounds(),
                                  view->Bounds(), &key, B_FOLLOW_ALL,
                                  B_OVERLAY_FILTER_HORIZONTAL |
                                  B_OVERLAY_FILTER_VERTICAL );
            view->SetViewColor( key );
            SetTitle( "VLC " PACKAGE_VERSION " (Overlay)" );

            bitmap[1] = new BBitmap( bitmapFrame, B_BITMAP_WILL_OVERLAY,
                                     colspace[colspace_index].colspace);
            if( bitmap[1] && bitmap[1]->InitCheck() == B_OK )
            {

                bitmap[2] = new BBitmap( bitmapFrame, B_BITMAP_WILL_OVERLAY,
                                         colspace[colspace_index].colspace);
                if( bitmap[2] && bitmap[2]->InitCheck() == B_OK )
                {
                    msg_Dbg( p_vout, "using double-buffered overlay" );
                }
                else
                {
                    msg_Dbg( p_vout, "using single-buffered overlay" );
                    bitmap_count = 2;
                    if( bitmap[2] ) { delete bitmap[2]; bitmap[2] = NULL; }
                }
            }
            else
            {
                msg_Dbg( p_vout, "using simple overlay" );
                bitmap_count = 1;
                if( bitmap[1] ) { delete bitmap[1]; bitmap[1] = NULL; }
            }
            break;
        }
        else
        {
            if( bitmap[0] ) { delete bitmap[0]; bitmap[0] = NULL; }
        }
    }

    if (*mode == BITMAP)
    {
        msg_Warn( p_vout, "no possible overlay" );

        // fallback to RGB
        colspace_index = DEFAULT_COL;    // B_RGB32
        bitmap[0] = new BBitmap( bitmapFrame, colspace[colspace_index].colspace );
        bitmap[1] = new BBitmap( bitmapFrame, colspace[colspace_index].colspace );
        bitmap[2] = new BBitmap( bitmapFrame, colspace[colspace_index].colspace );
        SetTitle( "VLC " PACKAGE_VERSION " (Bitmap)" );
    }
    // see if everything went well
    status_t status = B_ERROR;
    for (int32_t i = 0; i < bitmap_count; i++)
    {
        if (bitmap[i])
            status = bitmap[i]->InitCheck();
        if (status < B_OK)
            break;
    }
    if (status >= B_OK)
    {
        // clear bitmaps to black
        for (int32_t i = 0; i < bitmap_count; i++)
            _BlankBitmap(bitmap[i]);
    }
    return status;
}

/*****************************************************************************
 * VideoWindow::_FreeBuffers
 *****************************************************************************/
void
VideoWindow::_FreeBuffers()
{
    if( bitmap[0] ) { delete bitmap[0]; bitmap[0] = NULL; }
    if( bitmap[1] ) { delete bitmap[1]; bitmap[1] = NULL; }
    if( bitmap[2] ) { delete bitmap[2]; bitmap[2] = NULL; }
    fInitStatus = B_ERROR;
}

/*****************************************************************************
 * VideoWindow::_BlankBitmap
 *****************************************************************************/
void
VideoWindow::_BlankBitmap(BBitmap* bitmap) const
{
    // no error checking (we do that earlier on and since it's a private function...

    // YCbCr:
    // Loss/Saturation points are Y 16-235 (absoulte); Cb/Cr 16-240 (center 128)

    // YUV:
    // Extrema points are Y 0 - 207 (absolute) U -91 - 91 (offset 128) V -127 - 127 (offset 128)

    // we only handle weird colorspaces with special care
    switch (bitmap->ColorSpace()) {
        case B_YCbCr422: {
            // Y0[7:0]  Cb0[7:0]  Y1[7:0]  Cr0[7:0]  Y2[7:0]  Cb2[7:0]  Y3[7:0]  Cr2[7:0]
            int32_t height = bitmap->Bounds().IntegerHeight() + 1;
            uint8_t* bits = (uint8_t*)bitmap->Bits();
            int32_t bpr = bitmap->BytesPerRow();
            for (int32_t y = 0; y < height; y++) {
                // handle 2 bytes at a time
                for (int32_t i = 0; i < bpr; i += 2) {
                    // offset into line
                    bits[i] = 16;
                    bits[i + 1] = 128;
                }
                // next line
                bits += bpr;
            }
            break;
        }
        case B_YCbCr420: {
// TODO: untested!!
            // Non-interlaced only, Cb0  Y0  Y1  Cb2 Y2  Y3  on even scan lines ...
            // Cr0  Y0  Y1  Cr2 Y2  Y3  on odd scan lines
            int32_t height = bitmap->Bounds().IntegerHeight() + 1;
            uint8_t* bits = (uint8_t*)bitmap->Bits();
            int32_t bpr = bitmap->BytesPerRow();
            for (int32_t y = 0; y < height; y += 1) {
                // handle 3 bytes at a time
                for (int32_t i = 0; i < bpr; i += 3) {
                    // offset into line
                    bits[i] = 128;
                    bits[i + 1] = 16;
                    bits[i + 2] = 16;
                }
                // next line
                bits += bpr;
            }
            break;
        }
        case B_YUV422: {
// TODO: untested!!
            // U0[7:0]  Y0[7:0]   V0[7:0]  Y1[7:0]  U2[7:0]  Y2[7:0]   V2[7:0]  Y3[7:0]
            int32_t height = bitmap->Bounds().IntegerHeight() + 1;
            uint8_t* bits = (uint8_t*)bitmap->Bits();
            int32_t bpr = bitmap->BytesPerRow();
            for (int32_t y = 0; y < height; y += 1) {
                // handle 2 bytes at a time
                for (int32_t i = 0; i < bpr; i += 2) {
                    // offset into line
                    bits[i] = 128;
                    bits[i + 1] = 0;
                }
                // next line
                bits += bpr;
            }
            break;
        }
        default:
            memset(bitmap->Bits(), 0, bitmap->BitsLength());
            break;
    }
}

/*****************************************************************************
 * VideoWindow::_SetVideoSize
 *****************************************************************************/
void
VideoWindow::_SetVideoSize(uint32_t mode)
{
    // let size depend on aspect correction
    int32_t width = CorrectAspectRatio() ? i_width : fTrueWidth;
    int32_t height = CorrectAspectRatio() ? i_height : fTrueHeight;
    switch (mode)
    {
        case RESIZE_50:
            width /= 2;
            height /= 2;
            break;
        case RESIZE_200:
            width *= 2;
            height *= 2;
            break;
        case RESIZE_100:
        default:
            break;
    }
    fSettings->ClearFlags(VideoSettings::FLAG_FULL_SCREEN);
    ResizeTo(width, height);
}

/*****************************************************************************
 * VideoWindow::_SetToSettings
 *****************************************************************************/
void
VideoWindow::_SetToSettings()
{
    // adjust dimensions
    uint32_t mode = RESIZE_100;
    switch (fSettings->VideoSize())
    {
        case VideoSettings::SIZE_50:
            mode = RESIZE_50;
            break;
        case VideoSettings::SIZE_200:
            mode = RESIZE_200;
            break;
        case VideoSettings::SIZE_100:
        case VideoSettings::SIZE_OTHER:
        default:
            break;
    }
    bool fullscreen = IsFullScreen();    // remember settings
    _SetVideoSize(mode);                // because this will reset settings
    // the fullscreen status is reflected in the settings,
    // but not yet in the windows state
    if (fullscreen)
        SetFullScreen(true);
    if (fSettings->HasFlags(VideoSettings::FLAG_ON_TOP_ALL))
        fCachedFeel = B_FLOATING_ALL_WINDOW_FEEL;
    else
        fCachedFeel = B_NORMAL_WINDOW_FEEL;
    SetFeel(fCachedFeel);
}

/*****************************************************************************
 * VLCView::VLCView
 *****************************************************************************/
VLCView::VLCView(BRect bounds, vout_thread_t *p_vout_instance )
    : BView(bounds, "video view", B_FOLLOW_NONE, B_WILL_DRAW | B_PULSE_NEEDED),
      fLastMouseMovedTime(mdate()),
      fCursorHidden(false),
      fCursorInside(false),
      fIgnoreDoubleClick(false)
{
    p_vout = p_vout_instance;
    SetViewColor(B_TRANSPARENT_32_BIT);
}

/*****************************************************************************
 * VLCView::~VLCView
 *****************************************************************************/
VLCView::~VLCView()
{
}

/*****************************************************************************
 * VLCVIew::AttachedToWindow
 *****************************************************************************/
void
VLCView::AttachedToWindow()
{
    // periodically check if we want to hide the pointer
    Window()->SetPulseRate(1000000);
}

/*****************************************************************************
 * VLCVIew::MouseDown
 *****************************************************************************/
void
VLCView::MouseDown(BPoint where)
{
    VideoWindow* videoWindow = dynamic_cast<VideoWindow*>(Window());
    BMessage* msg = Window()->CurrentMessage();
    int32 clicks;
    uint32_t buttons;
    msg->FindInt32("clicks", &clicks);
    msg->FindInt32("buttons", (int32*)&buttons);

    if (videoWindow)
    {
        if (buttons & B_PRIMARY_MOUSE_BUTTON)
        {
            if (clicks == 2 && !fIgnoreDoubleClick)
                Window()->Zoom();
            /* else
                videoWindow->ToggleInterfaceShowing(); */
            fIgnoreDoubleClick = false;
        }
        else
        {
            if (buttons & B_SECONDARY_MOUSE_BUTTON)
            {
                // clicks will be 2 next time (if interval short enough)
                // even if the first click and the second
                // have not been made with the same mouse button
                fIgnoreDoubleClick = true;
                // launch popup menu
                BPopUpMenu *menu = new BPopUpMenu("context menu");
                menu->SetRadioMode(false);
                // In full screen, add an item to show/hide the interface
                if( videoWindow->IsFullScreen() )
                {
                    BMenuItem *intfItem =
                        new BMenuItem( _("Show Interface"), new BMessage(SHOW_INTERFACE) );
                    menu->AddItem( intfItem );
                }
                // Resize to 50%
                BMenuItem *halfItem = new BMenuItem(_("50%"), new BMessage(RESIZE_50));
                menu->AddItem(halfItem);
                // Resize to 100%
                BMenuItem *origItem = new BMenuItem(_("100%"), new BMessage(RESIZE_100));
                menu->AddItem(origItem);
                // Resize to 200%
                BMenuItem *doubleItem = new BMenuItem(_("200%"), new BMessage(RESIZE_200));
                menu->AddItem(doubleItem);
                // Toggle FullScreen
                BMenuItem *zoomItem = new BMenuItem(_("Fullscreen"), new BMessage(TOGGLE_FULL_SCREEN));
                zoomItem->SetMarked(videoWindow->IsFullScreen());
                menu->AddItem(zoomItem);
    
                menu->AddSeparatorItem();
    
                // Toggle vSync
                BMenuItem *vsyncItem = new BMenuItem(_("Vertical Sync"), new BMessage(VERT_SYNC));
                vsyncItem->SetMarked(videoWindow->IsSyncedToRetrace());
                menu->AddItem(vsyncItem);
                // Correct Aspect Ratio
                BMenuItem *aspectItem = new BMenuItem(_("Correct Aspect Ratio"), new BMessage(ASPECT_CORRECT));
                aspectItem->SetMarked(videoWindow->CorrectAspectRatio());
                menu->AddItem(aspectItem);
    
                menu->AddSeparatorItem();
    
                // Window Feel Items
/*                BMessage *winNormFeel = new BMessage(WINDOW_FEEL);
                winNormFeel->AddInt32("WinFeel", (int32_t)B_NORMAL_WINDOW_FEEL);
                BMenuItem *normWindItem = new BMenuItem("Normal Window", winNormFeel);
                normWindItem->SetMarked(videoWindow->Feel() == B_NORMAL_WINDOW_FEEL);
                menu->AddItem(normWindItem);
                
                BMessage *winFloatFeel = new BMessage(WINDOW_FEEL);
                winFloatFeel->AddInt32("WinFeel", (int32_t)B_FLOATING_APP_WINDOW_FEEL);
                BMenuItem *onTopWindItem = new BMenuItem("App Top", winFloatFeel);
                onTopWindItem->SetMarked(videoWindow->Feel() == B_FLOATING_APP_WINDOW_FEEL);
                menu->AddItem(onTopWindItem);
                
                BMessage *winAllFeel = new BMessage(WINDOW_FEEL);
                winAllFeel->AddInt32("WinFeel", (int32_t)B_FLOATING_ALL_WINDOW_FEEL);
                BMenuItem *allSpacesWindItem = new BMenuItem("On Top All Workspaces", winAllFeel);
                allSpacesWindItem->SetMarked(videoWindow->Feel() == B_FLOATING_ALL_WINDOW_FEEL);
                menu->AddItem(allSpacesWindItem);*/

                BMessage *windowFeelMsg = new BMessage( WINDOW_FEEL );
                bool onTop = videoWindow->Feel() == B_FLOATING_ALL_WINDOW_FEEL;
                window_feel feel = onTop ? B_NORMAL_WINDOW_FEEL : B_FLOATING_ALL_WINDOW_FEEL;
                windowFeelMsg->AddInt32( "WinFeel", (int32_t)feel );
                BMenuItem *windowFeelItem = new BMenuItem( _("Stay On Top"), windowFeelMsg );
                windowFeelItem->SetMarked( onTop );
                menu->AddItem( windowFeelItem );

                menu->AddSeparatorItem();

                BMenuItem* screenShotItem = new BMenuItem( _("Take Screen Shot"),
                                                           new BMessage( SCREEN_SHOT ) );
                menu->AddItem( screenShotItem );

                menu->SetTargetForItems( this );
                ConvertToScreen( &where );
                BRect mouseRect( where.x - 5, where.y - 5,
                                 where.x + 5, where.y + 5 );
                menu->Go( where, true, false, mouseRect, true );
            }
        }
    }
    fLastMouseMovedTime = mdate();
    fCursorHidden = false;
}

/*****************************************************************************
 * VLCVIew::MouseUp
 *****************************************************************************/
void
VLCView::MouseUp( BPoint where )
{
    vlc_value_t val;
    val.b_bool = VLC_TRUE;
    var_Set( p_vout, "mouse-clicked", val );
}

/*****************************************************************************
 * VLCVIew::MouseMoved
 *****************************************************************************/
void
VLCView::MouseMoved(BPoint point, uint32 transit, const BMessage* dragMessage)
{
    fLastMouseMovedTime = mdate();
    fCursorHidden = false;
    fCursorInside = ( transit == B_INSIDE_VIEW || transit == B_ENTERED_VIEW );

    if( !fCursorInside )
    {
        return;
    }

    vlc_value_t val;
    unsigned int i_width, i_height, i_x, i_y;
    vout_PlacePicture( p_vout, (unsigned int)Bounds().Width(),
                       (unsigned int)Bounds().Height(),
                       &i_x, &i_y, &i_width, &i_height );
    val.i_int = ( (int)point.x - i_x ) * p_vout->render.i_width / i_width;
    var_Set( p_vout, "mouse-x", val );
    val.i_int = ( (int)point.y - i_y ) * p_vout->render.i_height / i_height;
    var_Set( p_vout, "mouse-y", val );
    val.b_bool = VLC_TRUE;
    var_Set( p_vout, "mouse-moved", val );
}

/*****************************************************************************
 * VLCVIew::Pulse
 *****************************************************************************/
void
VLCView::Pulse()
{
    // We are getting the pulse messages no matter if the mouse is over
    // this view. If we are in full screen mode, we want to hide the cursor
    // even if it is not.
    VideoWindow *videoWindow = dynamic_cast<VideoWindow*>(Window());
    if (!fCursorHidden)
    {
        if (fCursorInside
            && mdate() - fLastMouseMovedTime > MOUSE_IDLE_TIMEOUT)
        {
            be_app->ObscureCursor();
            fCursorHidden = true;
            
            // hide the interface window as well if full screen
            if (videoWindow && videoWindow->IsFullScreen())
                videoWindow->SetInterfaceShowing(false);
        }
    }

    // Workaround to disable the screensaver in full screen:
    // we simulate an activity every 29 seconds    
    if( videoWindow && videoWindow->IsFullScreen() &&
        mdate() - fLastMouseMovedTime > 29000000 )
    {
        BPoint where;
        uint32 buttons;
        GetMouse(&where, &buttons, false);
        ConvertToScreen(&where);
        set_mouse_position((int32_t) where.x, (int32_t) where.y);
    }
}

/*****************************************************************************
 * VLCVIew::Draw
 *****************************************************************************/
void
VLCView::Draw(BRect updateRect)
{
    VideoWindow* window = dynamic_cast<VideoWindow*>( Window() );
    if ( window && window->mode == BITMAP )
        FillRect( updateRect );
}

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Init       ( vout_thread_t * );
static void End        ( vout_thread_t * );
static int  Manage     ( vout_thread_t * );
static void Display    ( vout_thread_t *, picture_t * );
static int  Control    ( vout_thread_t *, int, va_list );

static int  BeosOpenDisplay ( vout_thread_t *p_vout );
static void BeosCloseDisplay( vout_thread_t *p_vout );

/*****************************************************************************
 * OpenVideo: allocates BeOS video thread output method
 *****************************************************************************
 * This function allocates and initializes a BeOS vout method.
 *****************************************************************************/
int E_(OpenVideo) ( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    /* Allocate structure */
    p_vout->p_sys = (vout_sys_t*) malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return( 1 );
    }
    p_vout->p_sys->i_width = p_vout->render.i_width;
    p_vout->p_sys->i_height = p_vout->render.i_height;
    p_vout->p_sys->source_chroma = p_vout->render.i_chroma;

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_render = NULL;
    p_vout->pf_display = Display;
    p_vout->pf_control = Control;

    return( 0 );
}

/*****************************************************************************
 * Init: initialize BeOS video thread output method
 *****************************************************************************/
int Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;

    I_OUTPUTPICTURES = 0;

    /* Open and initialize device */
    if( BeosOpenDisplay( p_vout ) )
    {
        msg_Err(p_vout, "vout error: can't open display");
        return 0;
    }
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;

    /* Assume we have square pixels */
    p_vout->output.i_aspect = p_vout->p_sys->i_width
                               * VOUT_ASPECT_FACTOR / p_vout->p_sys->i_height;
    p_vout->output.i_chroma = colspace[p_vout->p_sys->p_window->colspace_index].chroma;
    p_vout->p_sys->i_index = 0;

    p_vout->b_direct = 1;

    p_vout->output.i_rmask  = 0x00ff0000;
    p_vout->output.i_gmask  = 0x0000ff00;
    p_vout->output.i_bmask  = 0x000000ff;

    for( int buffer_index = 0 ;
         buffer_index < p_vout->p_sys->p_window->bitmap_count;
         buffer_index++ )
    {
       p_pic = NULL;
       /* Find an empty picture slot */
       for( i_index = 0 ; i_index < VOUT_MAX_PICTURES ; i_index++ )
       {
           p_pic = NULL;
           if( p_vout->p_picture[ i_index ].i_status == FREE_PICTURE )
           {
               p_pic = p_vout->p_picture + i_index;
               break;
           }
       }

       if( p_pic == NULL )
       {
           return 0;
       }
       p_pic->p->p_pixels = (uint8_t*)p_vout->p_sys->p_window->bitmap[buffer_index]->Bits();
       p_pic->p->i_lines = p_vout->p_sys->i_height;
       p_pic->p->i_visible_lines = p_vout->p_sys->i_height;

       p_pic->p->i_pixel_pitch = colspace[p_vout->p_sys->p_window->colspace_index].pixel_bytes;
       p_pic->i_planes = colspace[p_vout->p_sys->p_window->colspace_index].planes;
       p_pic->p->i_pitch = p_vout->p_sys->p_window->bitmap[buffer_index]->BytesPerRow();
       p_pic->p->i_visible_pitch = p_pic->p->i_pixel_pitch * ( p_vout->p_sys->p_window->bitmap[buffer_index]->Bounds().IntegerWidth() + 1 );

       p_pic->i_status = DESTROYED_PICTURE;
       p_pic->i_type   = DIRECT_PICTURE;

       PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

       I_OUTPUTPICTURES++;
    }

    return( 0 );
}

/*****************************************************************************
 * End: terminate BeOS video thread output method
 *****************************************************************************/
void End( vout_thread_t *p_vout )
{
    BeosCloseDisplay( p_vout );
}

/*****************************************************************************
 * Manage
 *****************************************************************************/
static int Manage( vout_thread_t * p_vout )
{
    if( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        p_vout->p_sys->p_window->PostMessage( TOGGLE_FULL_SCREEN );
        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }

    return 0;
}

/*****************************************************************************
 * CloseVideo: destroy BeOS video thread output method
 *****************************************************************************
 * Terminate an output method created by DummyCreateOutputMethod
 *****************************************************************************/
void E_(CloseVideo) ( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    free( p_vout->p_sys );
}

/*****************************************************************************
 * Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to BeOS image, waits until
 * it is displayed and switch the two rendering buffers, preparing next frame.
 *****************************************************************************/
void Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    VideoWindow * p_win = p_vout->p_sys->p_window;

    /* draw buffer if required */
    if (!p_win->teardownwindow)
    {
       p_win->drawBuffer(p_vout->p_sys->i_index);
    }
    /* change buffer */
    p_vout->p_sys->i_index = ++p_vout->p_sys->i_index %
        p_vout->p_sys->p_window->bitmap_count;
    p_pic->p->p_pixels = (uint8_t*)p_vout->p_sys->p_window->bitmap[p_vout->p_sys->i_index]->Bits();
}

static int Control( vout_thread_t * p_vout, int i_query, va_list args )
{
    return vout_vaControlDefault( p_vout, i_query, args );
}

/* following functions are local */

/*****************************************************************************
 * BeosOpenDisplay: open and initialize BeOS device
 *****************************************************************************/
static int BeosOpenDisplay( vout_thread_t *p_vout )
{

    p_vout->p_sys->p_window = new VideoWindow( p_vout->p_sys->i_width - 1,
                                               p_vout->p_sys->i_height - 1,
                                               BRect( 20, 50,
                                                      20 + p_vout->i_window_width - 1,
                                                      50 + p_vout->i_window_height - 1 ),
                                               p_vout );
    if( p_vout->p_sys->p_window == NULL )
    {
        msg_Err( p_vout, "cannot allocate VideoWindow" );
        return( 1 );
    }
    else
    {
        p_vout->p_sys->p_window->Show();
    }

    return( 0 );
}

/*****************************************************************************
 * BeosDisplay: close and reset BeOS device
 *****************************************************************************
 * Returns all resources allocated by BeosOpenDisplay and restore the original
 * state of the device.
 *****************************************************************************/
static void BeosCloseDisplay( vout_thread_t *p_vout )
{
    VideoWindow * p_win = p_vout->p_sys->p_window;
    /* Destroy the video window */
    if( p_win != NULL && !p_win->teardownwindow)
    {
        p_win->Lock();
        p_win->teardownwindow = true;
        p_win->Hide();
        p_win->Quit();
    }
    p_win = NULL;
}
