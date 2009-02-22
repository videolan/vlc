/**
 * @file libvlc.cs
 * @brief Unmanaged LibVLC APIs
 * @ingroup Internals
 *
 * @defgroup Internals LibVLC internals
 * This covers internal marshalling functions to use the native LibVLC.
 * Only VLC developpers should need to read this section.
 */

/**********************************************************************
 *  Copyright (C) 2007-2009 Rémi Denis-Courmont.                      *
 *  This program is free software; you can redistribute and/or modify *
 *  it under the terms of the GNU General Public License as published *
 *  by the Free Software Foundation; version 2 of the license, or (at *
 *  your option) any later version.                                   *
 *                                                                    *
 *  This program is distributed in the hope that it will be useful,   *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *  See the GNU General Public License for more details.              *
 *                                                                    *
 *  You should have received a copy of the GNU General Public License *
 *  along with this program; if not, you can get it from:             *
 *  http://www.gnu.org/copyleft/gpl.html                              *
 **********************************************************************/

using System;
using System.Runtime.InteropServices;

namespace VideoLAN.LibVLC
{
    /**
     * @brief Native: unmanaged LibVLC APIs
     * @ingroup Internals
     */
    internal static class LibVLC
    {
        /* core.c */
        [DllImport ("libvlc.dll", EntryPoint="libvlc_get_version")]
        public static extern IntPtr GetVersion ();

        [DllImport ("libvlc.dll", EntryPoint="libvlc_get_compiler")]
        public static extern IntPtr GetCompiler ();

        [DllImport ("libvlc.dll", EntryPoint="libvlc_get_changeset")]
        public static extern IntPtr GetChangeset ();

        [DllImport ("libvlc.dll", EntryPoint="libvlc_new")]
        public static extern
        InstanceHandle Create (int argc, U8String[] argv, NativeException ex);

        /*[DllImport ("libvlc.dll", EntryPoint="libvlc_retain")]
        public static extern
        void Retain (InstanceHandle h, NativeException ex);*/

        [DllImport ("libvlc.dll", EntryPoint="libvlc_release")]
        public static extern
        void Release (IntPtr h, NativeException ex);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_add_intf")]
        public static extern
        void AddIntf (InstanceHandle h, U8String name, NativeException ex);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_wait")]
        public static extern
        void Wait (InstanceHandle h);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_get_vlc_instance")]
        public static extern
        SafeHandle GetVLCInstance (InstanceHandle h);

        /* media.c */
        [DllImport ("libvlc.dll", EntryPoint="libvlc_media_new")]
        public static extern
        MediaHandle MediaCreate (InstanceHandle inst, U8String mrl,
                                 NativeException ex);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_media_new_as_node")]
        public static extern
        MediaHandle MediaCreateAsNode (InstanceHandle inst, U8String name,
                                       NativeException ex);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_media_add_option")]
        public static extern
        void MediaAddOption (MediaHandle media, U8String options,
                             NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_add_option_untrusted")]
        public static extern
        void MediaAddUntrustedOption (MediaHandle media, U8String options,
                                      NativeException ex);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_media_release")]
        public static extern
        void MediaRelease (IntPtr ptr);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_media_get_mrl")]
        public static extern
        StringHandle MediaGetMRL (MediaHandle media, NativeException ex);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_media_duplicate")]
        public static extern
        MediaHandle MediaDuplicate (MediaHandle media);

        /*[DllImport ("libvlc.dll", EntryPoint="libvlc_media_read_meta")]
        public static extern
        MediaHandle MediaDuplicate (MediaHandle media, int type,
                                    NativeException ex);*/

        /*[DllImport ("libvlc.dll", EntryPoint="libvlc_media_get_state")]
        public static extern
        int MediaGetState (MediaHandle media, NativeException ex);*/

        /*[DllImport ("libvlc.dll", EntryPoint="libvlc_media_subitems")]
        public static extern
        MediaListHandle MediaSubItems (MediaHandle media, NativeException ex);*/

        /*[DllImport ("libvlc.dll", EntryPoint="libvlc_media_event_manager")]
        public static extern
        EventManagerHandle MediaGetEventManager (MediaHandle media,
                                                 NativeException ex);*/

        [DllImport ("libvlc.dll", EntryPoint="libvlc_media_get_duration")]
        public static extern
        long MediaGetDuration (MediaHandle media, NativeException ex);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_media_is_preparsed")]
        public static extern
        int MediaIsPreparsed (MediaHandle media, NativeException ex);

        /*[DllImport ("libvlc.dll", EntryPoint="libvlc_media_set_user_data")]
        public static extern
        void MediaIsPreparsed (MediaHandle media, IntPtr data,
                               NativeException ex);*/

        /*[DllImport ("libvlc.dll", EntryPoint="libvlc_media_get_user_data")]
        public static extern
        IntPtr MediaIsPreparsed (MediaHandle media, NativeException ex);*/

        /* media_player.c */
        [DllImport ("libvlc.dll", EntryPoint="libvlc_media_player_new")]
        public static extern
        PlayerHandle PlayerCreate (InstanceHandle inst, NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_new_from_media")]
        public static extern
        PlayerHandle PlayerCreateFromMedia (MediaHandle media,
                                            NativeException ex);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_media_player_release")]
        public static extern
        void PlayerRelease (IntPtr ptr);

        /* PlayerRetain */

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_set_media")]
        public static extern
        void PlayerSetMedia (PlayerHandle player, MediaHandle media,
                             NativeException ex);

        /*[DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_get_media")]
        public static extern
        MediaHandle PlayerGetMedia (PlayerHandle player,
                                    NativeException ex);*/

        /*[DllImport ("libvlc.dll",
                      EntryPoint="libvlc_media_player_event_manager")]
        public static extern
        EventManagerHandle PlayerGetEventManager (PlayerHandle media,
                                                  NativeException ex);*/
        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_is_playing")]
        public static extern
        int PlayerIsPlaying (PlayerHandle player, NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_play")]
        public static extern
        void PlayerPlay (PlayerHandle player, NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_pause")]
        public static extern
        void PlayerPause (PlayerHandle player, NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_stop")]
        public static extern
        void PlayerStop (PlayerHandle player, NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_set_xwindow")]
        public static extern
        void PlayerSetXWindow (PlayerHandle player, int xid,
                               NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_get_xwindow")]
        public static extern
        int PlayerGetXWindow (PlayerHandle player);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_set_hwnd")]
        public static extern
        void PlayerSetHWND (PlayerHandle player, SafeHandle hwnd,
                            NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_get_hwnd")]
        public static extern
        SafeHandle PlayerGetHWND (PlayerHandle player);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_get_length")]
        public static extern
        long PlayerGetLength (PlayerHandle player, NativeException ex);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_media_player_get_time")]
        public static extern
        long PlayerGetTime (PlayerHandle player, NativeException ex);

        [DllImport ("libvlc.dll", EntryPoint="libvlc_media_player_set_time")]
        public static extern
        void PlayerSetTime (PlayerHandle player, long time,
                            NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_get_position")]
        public static extern
        float PlayerGetPosition (PlayerHandle player, NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_set_position")]
        public static extern
        void PlayerSetPosition (PlayerHandle player, float position,
                                NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_get_chapter")]
        public static extern
        int PlayerGetChapter (PlayerHandle player, NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_set_chapter")]
        public static extern
        void PlayerSetChapter (PlayerHandle player, int chapter,
                               NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_get_chapter_count")]
        public static extern
        int PlayerGetChapterCount (PlayerHandle player, NativeException ex);

        /* PlayerWillPlay */

        [DllImport ("libvlc.dll",
                EntryPoint="libvlc_media_player_get_chapter_count_for_title")]
        public static extern
        int PlayerGetChapterCountForTitle (PlayerHandle player, int title,
                                           NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_get_title")]
        public static extern
        int PlayerGetTitle (PlayerHandle player, NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_set_title")]
        public static extern
        void PlayerSetTitle (PlayerHandle player, int chapter,
                             NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_get_title_count")]
        public static extern
        int PlayerGetTitleCount (PlayerHandle player, NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_next_chapter")]
        public static extern
        void PlayerNextChapter (PlayerHandle player, NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_previous_chapter")]
        public static extern
        void PlayerPreviousChapter (PlayerHandle player, NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_get_rate")]
        public static extern
        float PlayerGetRate (PlayerHandle player, NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_set_rate")]
        public static extern
        void PlayerSetRate (PlayerHandle player, float rate,
                            NativeException ex);

        /*[DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_get_state")]
        public static extern
        void PlayerSetState (PlayerHandle player, float rate,
                             NativeException ex); */

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_get_fps")]
        public static extern
        float PlayerGetFPS (PlayerHandle player, NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_has_vout")]
        public static extern
        int PlayerHasVout (PlayerHandle player, NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_is_seekable")]
        public static extern
        int PlayerIsSeekable (PlayerHandle player, NativeException ex);

        [DllImport ("libvlc.dll",
                    EntryPoint="libvlc_media_player_can_pause")]
        public static extern
        int PlayerCanPause (PlayerHandle player, NativeException ex);

    };
};
