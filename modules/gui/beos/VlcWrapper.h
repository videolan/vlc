/*****************************************************************************
 * VlcWrapper.h: BeOS plugin for vlc (derived from MacOS X port)
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: VlcWrapper.h,v 1.16 2003/01/25 20:15:41 titer Exp $
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
class VlcWrapper;

/*****************************************************************************
 * intf_sys_t: internal variables of the BeOS interface
 *****************************************************************************/
struct intf_sys_t
{
    msg_subscription_t * p_sub;

    InterfaceWindow *    p_window;
    
    vlc_bool_t           b_loop;
    vlc_bool_t           b_mute;
    int                  i_part;
    audio_volume_t       i_saved_volume;
    int                  i_channel;
    bool                 b_dvdold;
    
    VlcWrapper *         p_wrapper;
};

/*****************************************************************************
 * VlcWrapper
 *****************************************************************************
 * This class makes the link between the BeOS interface and the vlc core.
 * There is only one VlcWrapper instance at any time, which is stored
 * in p_intf->p_sys->p_wrapper
 *****************************************************************************/
class VlcWrapper
{
public:
    VlcWrapper( intf_thread_t *p_intf );
    ~VlcWrapper();
    
    bool UpdateInputAndAOut();
    
    /* Input */
    bool         HasInput();
    int          InputStatus();
    int          InputRate();
    void         InputSlower();
    void         InputFaster();
    BList *      GetChannels( int i_cat );
    void         ToggleLanguage( int i_language );
    void         ToggleSubtitle( int i_subtitle );
    const char * GetTimeAsString();
    float        GetTimeAsFloat();
    void         SetTimeAsFloat( float i_offset );
    bool         IsPlaying();
        
    /* Playlist */
    void    OpenFiles( BList *o_files, bool replace = true );
    void    OpenDisc( BString o_type, BString o_device,
                     int i_title, int i_chapter );
    int     PlaylistSize();
    char *  PlaylistItemName( int );
    int     PlaylistCurrent();
    bool    PlaylistPlay();
    void    PlaylistPause();
    void    PlaylistStop();
    void    PlaylistNext();
    void    PlaylistPrev();
    bool    PlaylistPlaying();
    void    GetPlaylistInfo( int32& currentIndex,
                             int32& maxIndex );
    void    PlaylistJumpTo( int );
    void    GetNavCapabilities( bool * canSkipPrev,
                                bool * canSkipNext );
    void    NavigatePrev();
    void    NavigateNext();

    /* Audio */
    bool           HasAudio();
    unsigned short GetVolume();
    void           SetVolume( int value );
    void           VolumeMute();
    void           VolumeRestore();
    bool           IsMuted();

    /* DVD */
    bool    HasTitles();
    BList * GetTitles();
    void    PrevTitle();
    void    NextTitle();
    void    ToggleTitle( int i_title );
    void    TitleInfo( int32& currentIndex, int32& maxIndex );

    bool    HasChapters();
    BList * GetChapters();
    void    PrevChapter();
    void    NextChapter();
    void    ToggleChapter( int i_chapter );
    void    ChapterInfo( int32& currentIndex, int32& maxIndex );
    
    /* Miscellanous */
    void         LoadSubFile( char * psz_file );
    
private:
    intf_thread_t *   p_intf;
    input_thread_t *  p_input;
    playlist_t *      p_playlist;
    aout_instance_t * p_aout;
};
