/*****************************************************************************
 * intf_vlc_wrapper.h: BeOS plugin for vlc (derived from MacOS X port )
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: intf_vlc_wrapper.h,v 1.1.2.3 2002/09/29 12:04:27 titer Exp $
 *
 * Authors: Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Tony Casltey <tony@castley.net>
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
class InterfaceWindow;

/*****************************************************************************
 * intf_sys_t: description and status of FB interface
 *****************************************************************************/
typedef struct intf_sys_s
{
    InterfaceWindow * p_window;
    char              i_key;
    int               b_disabled_menus;
    int	              i_part;
    int               i_saved_volume;
    int               b_loop;
    int               i_channel;
    int               b_mute;
} intf_sys_t;

/* Intf_VLCWrapper is a singleton class
    (only one instance at any time) */
class Intf_VLCWrapper
{
public:
    static bool manage();
    static void quit();
    /* playlist control */
    static bool playlistPlay();
    static void playlistPause();
    static void playlistStop();
    static void playlistNext();
    static void playlistPrev();
    static void playlistJumpTo( int pos );
    static int playlistCurrentPos();
    static int playlistSize();
	static void playlistLock();
	static void playlistUnlock();

    static void getNavCapabilities( bool* canSkipPrev,
    								bool* canSkipNext );
	static void	navigatePrev();
	static void	navigateNext();

//    static void channelNext();
//    static void channelPrev();
    static void loop();

    /* playback control */
    static void playSlower();
    static void playFaster();
    static void volume_mute();
    static void volume_restore();
    static void set_volume(int value);
    static void toggle_mute();
    static bool is_muted();
    static bool is_playing();
    static void maxvolume();
    static bool has_audio();
//    static void fullscreen();
    static void eject();

    /* playback info */
    static BString* getTimeAsString();
    static float  getTimeAsFloat();
    static void   setTimeAsFloat( float i_offset );
    static bool   playlistPlaying();
    static BList* playlistAsArray();

    /* open file/disc/network */
    static void openFiles( BList *o_files, bool replace = true );
    static void openDisc( BString o_type, BString o_device,
    					  int i_title, int i_chapter );
    static void openNet( BString o_addr, int i_port );
    static void openNetChannel( BString o_addr, int i_port );
    static void openNetHTTP( BString o_addr );

    /* menus management */
    static void toggleProgram( int i_program );
    static void toggleTitle( int i_title );
    static void toggleChapter( int i_chapter );
    static void toggleLanguage( int i_language );
    static void toggleSubtitle( int i_subtitle );
    static void setupMenus();
};

