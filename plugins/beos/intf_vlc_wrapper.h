/*****************************************************************************
 * intf_vlc_wrapper.h: BeOS plugin for vlc (derived from MacOS X port )
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: intf_vlc_wrapper.h,v 1.3 2002/07/23 13:16:51 tcastley Exp $
 *
 * Authors: Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Tony Casltey <tony@castley.net>
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
struct intf_sys_t
{
    InterfaceWindow *p_window;
    char              i_key;
    
    /* The input thread */
    input_thread_t * p_input;

    /* The messages window */
//    msg_subscription_t * p_sub;

    /* DVD mode */
    vlc_bool_t        b_disabled_menus;
    vlc_bool_t        b_loop;
    vlc_bool_t        b_mute;
    int	              i_part;
    int               i_saved_volume;
    int               i_channel;
    
};

/* Intf_VLCWrapper is a singleton class
    (only one instance at any time) */
class Intf_VLCWrapper
{
public:
    static Intf_VLCWrapper *getVLCWrapper(intf_thread_t *p_if);
    ~Intf_VLCWrapper();

//    bool manage();
    void quit();
    int inputGetStatus();
    /* playlist control */
    bool playlistPlay();
    void playlistPause();
    void playlistStop();
    void playlistNext();
    void playlistPrev();
    void playlistSkip(int i);
    void playlistGoto(int i);
/*  Playback Modes
		PLAYLIST_REPEAT_CURRENT
		PLAYLIST_FORWARD       
		PLAYLIST_BACKWARD      
		PLAYLIST_FORWARD_LOOP  
		PLAYLIST_BACKWARD_LOOP 
		PLAYLIST_RANDOM        
		PLAYLIST_REVERSE_RANDOM
*/

    /*  Stream Control */
    void playSlower();
    void playFaster();
    
    /* input control */
    int getStatus();    
    void setStatus(int status);
    void inputSeek();
    void toggleProgram(int i_program);
    void toggleTitle(int i_title);
    void toggleChapter(int i_chapter);
    void toggleLanguage(int i_language);
    void toggleSubtitle(int i_subtitle);
    void channelNext();
    void channelPrev();
    void eject();

    /* playback info */
    BString* getTimeAsString();
    float  getTimeAsFloat();
    void   setTimeAsFloat(float i_offset);
    BList* playlistAsArray();

    /* open file/disc/network */
    void openFiles(BList *o_files);
    void openDisc(BString o_type, BString o_device, int i_title, int i_chapter);
    void openNet(BString o_addr, int i_port);
    void openNetChannel(BString o_addr, int i_port);
    void openNetHTTP(BString o_addr);

    /* audio stuff */
    void toggleMute( );
    /* menus management */
    void setupMenus();
    
private:
    Intf_VLCWrapper( intf_thread_t *p_if );
  	es_descriptor_t *  p_audio_es;
    intf_thread_t *p_intf;

};

