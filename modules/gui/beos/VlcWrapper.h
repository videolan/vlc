/*****************************************************************************
 * intf_vlc_wrapper.h: BeOS plugin for vlc (derived from MacOS X port )
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: VlcWrapper.h,v 1.7 2002/11/26 01:06:08 titer Exp $
 *
 * Authors: Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Tony Castley <tony@castley.net>
 *          Stephan Aßmus <stippi@yellowbites.com>
 *          Eric Petit <titer@videolan.org>
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

#define SEEKSLIDER_RANGE 2048

class InterfaceWindow;
class Intf_VLCWrapper;

/*****************************************************************************
 * intf_sys_t: internal variables of the BeOS interface
 *****************************************************************************/
struct intf_sys_t
{
    InterfaceWindow * p_window;
    
    /* DVD mode */
    vlc_bool_t        b_disabled_menus;
    vlc_bool_t        b_loop;
    vlc_bool_t        b_mute;
    int	              i_part;
    int               i_saved_volume;
    int               i_channel;
    
    Intf_VLCWrapper * p_wrapper;
};

/*****************************************************************************
 * Intf_VLCWrapper
 *****************************************************************************
 * This class makes the link between the BeOS interface and the vlc core.
 * There is only one Intf_VLCWrapper instance at any time, which is stored
 * in p_intf->p_sys->p_wrapper
 *****************************************************************************/
class Intf_VLCWrapper
{
public:
    Intf_VLCWrapper( intf_thread_t *p_intf );
    ~Intf_VLCWrapper();
    
    bool UpdateInputAndAOut();
    
    int inputGetStatus();
    int InputStatus();
    int InputRate();
    int InputTell();
    int InputSize();
    void inputSeek();
    
    /* playlist control */
    int PlaylistSize();
    char *PlaylistItemName( int );
    int PlaylistCurrent();
    
    bool playlistPlay();
    void playlistPause();
    void playlistStop();
    void playlistNext();
    void playlistPrev();
    void playlistJumpTo( int );
    int  playlistSize();
    int  playlistCurrentPos();
    void playlistLock();
    void playlistUnlock();
    void playlistSkip(int i);
    void playlistGoto(int i);
    void loop(); 

    bool playlistPlaying();
    BList* playlistAsArray();
    void   getPlaylistInfo( int32& currentIndex,
                            int32& maxIndex );
    void   getTitleInfo( int32& currentIndex,
                         int32& maxIndex );
    void   getChapterInfo( int32& currentIndex,
                           int32& maxIndex );
    void getNavCapabilities( bool* canSkipPrev,
                             bool* canSkipNext );
	void navigatePrev();
	void navigateNext();

    /* DVD */
    bool HasTitles();
    void PrevTitle();
    void NextTitle();
    bool HasChapters();
    void PrevChapter();
    void NextChapter();

    /*  Stream Control */
    void playSlower();
    void playFaster();
    
    /* playback control */
    void volume_mute();
    void volume_restore();
    void set_volume(int value);
    void toggle_mute( );
    bool is_muted();
    bool is_playing();
    void maxvolume();
    bool has_audio();
    
    /* playback info */
    const char* getTimeAsString();
    float  getTimeAsFloat();
    void   setTimeAsFloat( float i_offset );

    /* open file/disc/network */
    void openFiles( BList *o_files, bool replace = true );
    void openDisc( BString o_type, BString o_device,
                   int i_title, int i_chapter );
    void openNet( BString o_addr, int i_port );
    void openNetChannel( BString o_addr, int i_port );
    void openNetHTTP( BString o_addr );

    /* menus management */
    void toggleProgram( int i_program );
    void toggleTitle( int i_title );
    void toggleChapter( int i_chapter );
    void toggleLanguage( int i_language );
    void toggleSubtitle( int i_subtitle );
    void channelNext();
    void channelPrev();

private:
    intf_thread_t * p_intf;
    input_thread_t * p_input;
    playlist_t * p_playlist;
    aout_instance_t * p_aout;
};

