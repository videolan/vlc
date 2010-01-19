/*****************************************************************************
 * VLC Java Bindings JNA Glue
 *****************************************************************************
 * Copyright (C) 1998-2009 the VideoLAN team
 *
 * Authors: Filippo Carone <filippo@carone.org>
 *          VLC bindings generator
 *
 *
 * $Id $
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

package org.videolan.jvlc.internal;

import com.sun.jna.Callback;
import com.sun.jna.Library;
import com.sun.jna.Native;
import com.sun.jna.NativeLong;
import com.sun.jna.Platform;
import com.sun.jna.Pointer;
import com.sun.jna.PointerType;
import com.sun.jna.Structure;
import com.sun.jna.Union;


public interface LibVlc extends Library
{

    LibVlc INSTANCE = (LibVlc) Native.loadLibrary(Platform.isWindows() ? "libvlc" : "vlc", LibVlc.class);

    LibVlc SYNC_INSTANCE = (LibVlc) Native.synchronizedLibrary(INSTANCE);

    public static class libvlc_exception_t extends Structure
    {

        public int b_raised;
    }

    public static interface LibVlcCallback extends Callback
    {

        void callback(libvlc_event_t libvlc_event, Pointer userData);
    }

    public static class libvlc_log_message_t extends Structure
    {

        public int sizeof_msg; /* sizeof() of message structure, must be filled in by user */

        public int i_severity; /* 0=INFO, 1=ERR, 2=WARN, 3=DBG */

        public String psz_type; /* module type */

        public String psz_name; /* module name */

        public String psz_header; /* optional header */

        public String psz_message; /* message */
    }

    public static class libvlc_event_t extends Structure
    {

        public int type;

        public Pointer p_obj;

        public event_type_specific event_type_specific;

    }

    public class media_meta_changed extends Structure
    {

        // Enum !
        public Pointer meta_type;
    }

    public class media_subitem_added extends Structure
    {

        public LibVlcMedia new_child;
    }

    public class media_duration_changed extends Structure
    {

        public NativeLong new_duration;
    }

    public class media_preparsed_changed extends Structure
    {

        public int new_status;
    }

    public class media_freed extends Structure
    {

        public LibVlcMedia md;
    }

    public class media_state_changed extends Structure
    {

        // @todo: check this one
        public int new_state;
    }

    /* media instance */

    public class media_player_position_changed extends Structure
    {

        public float new_position;
    }

    public class media_player_time_changed extends Structure
    {

        // @todo: check this one
        public long new_time;
    }

    public class media_player_title_changed extends Structure
    {

        public int new_title;
    }

    public class media_player_seekable_changed extends Structure
    {

        public NativeLong new_seekable;
    }

    public class media_player_pausable_changed extends Structure
    {

        public NativeLong new_pausable;
    }

    /* media list */
    public class media_list_item_added extends Structure
    {

        public LibVlcMedia item;

        public int index;
    }

    public class media_list_will_add_item extends Structure
    {

        public LibVlcMedia item;

        public int index;
    }

    public class media_list_item_deleted extends Structure
    {

        public LibVlcMedia item;

        public int index;
    }

    public class media_list_will_delete_item extends Structure
    {

        public LibVlcMedia item;

        public int index;
    }

    /* media list view */
    public class media_list_view_item_added extends Structure
    {

        public LibVlcMedia item;

        public int index;
    }

    public class media_list_view_will_add_item extends Structure
    {

        public LibVlcMedia item;

        public int index;
    }

    public class media_list_view_item_deleted extends Structure
    {

        public LibVlcMedia item;

        public int index;
    }

    public class media_list_view_will_delete_item extends Structure
    {

        public LibVlcMedia item;

        public int index;
    }

    public class media_list_player_next_item_set extends Structure
    {

        public LibVlcMedia item;
    }

    public class media_player_snapshot_taken extends Structure
    {

        public String psz_filename;
    }

    public class media_player_length_changed extends Structure
    {

        // @todo: check the type
        public long new_length;
    }

    public class vlm_media_event extends Structure
    {

        public String psz_media_name;

        public String psz_instance_name;
    }

    public class event_type_specific extends Union
    {

        public media_meta_changed media_meta_changed;

        public media_subitem_added media_subitem_added;

        public media_duration_changed media_duration_changed;

        public media_preparsed_changed media_preparsed_changed;

        public media_freed media_freed;

        public media_state_changed media_state_changed;

        public media_player_position_changed media_player_position_changed;

        public media_player_time_changed media_player_time_changed;

        public media_player_title_changed media_player_title_changed;

        public media_player_seekable_changed media_player_seekable_changed;

        public media_player_pausable_changed media_player_pausable_changed;

        public media_list_item_added media_list_item_added;

        public media_list_will_add_item media_list_will_add_item;

        public media_list_item_deleted media_list_item_deleted;

        public media_list_will_delete_item media_list_will_delete_item;

        public media_list_view_item_added media_list_view_item_added;

        public media_list_view_will_add_item media_list_view_will_add_item;

        public media_list_view_item_deleted media_list_view_item_deleted;

        public media_list_view_will_delete_item media_list_view_will_delete_item;

        public media_list_player_next_item_set media_list_player_next_item_set;

        public media_player_snapshot_taken media_player_snapshot_taken;

        public media_player_length_changed media_player_length_changed;

        public vlm_media_event vlm_media_event;
    }

    public class LibVlcLog extends PointerType
    {
    }

    public class LibVlcMediaListView extends PointerType
    {
    }

    public class LibVlcTrackDescription extends PointerType
    {
    }

    public class LibVlcMediaListPlayer extends PointerType
    {
    }

    public class LibVlcInstance extends PointerType
    {
    }

    public class LibVlcEventManager extends PointerType
    {
    }

    public class LibVlcMediaLibrary extends PointerType
    {
    }

    public class LibVlcMediaList extends PointerType
    {
    }

    public class LibVlcAudioOutput extends PointerType
    {
    }

    public class LibVlcMediaPlayer extends PointerType
    {
    }

    public class LibVlcMedia extends PointerType
    {
    }

    public class LibVlcMediaDiscoverer extends PointerType
    {
    }

    public class LibVlcLogIterator extends PointerType
    {
    }

    void libvlc_exception_init(libvlc_exception_t p_exception);

    void libvlc_exception_clear(libvlc_exception_t p_exception);

    int libvlc_exception_raised(final libvlc_exception_t exception);
    
    String libvlc_errmsg();

    void libvlc_clearerr();

    LibVlcInstance libvlc_new(int argc, String[] argv, libvlc_exception_t p_e);

    void libvlc_release(LibVlcInstance p_instance);

    void libvlc_retain(LibVlcInstance p_instance);

    int libvlc_add_intf(LibVlcInstance p_instance, String name, libvlc_exception_t p_exception);

    void libvlc_wait(LibVlcInstance p_instance);

    String libvlc_get_version();

    String libvlc_get_compiler();

    String libvlc_get_changeset();

    void libvlc_free(Pointer ptr);

    void libvlc_event_attach(LibVlcEventManager p_event_manager, int i_event_type, LibVlcCallback f_callback,
        Pointer user_data, libvlc_exception_t p_e);

    void libvlc_event_detach(LibVlcEventManager p_event_manager, int i_event_type, LibVlcCallback f_callback,
        Pointer p_user_data, libvlc_exception_t p_e);

    String libvlc_event_type_name(int event_type);

    int libvlc_get_log_verbosity(LibVlcInstance p_instance);

    void libvlc_set_log_verbosity(LibVlcInstance p_instance, int level);

    LibVlcLog libvlc_log_open(LibVlcInstance p_instance, libvlc_exception_t p_e);

    void libvlc_log_close(LibVlcLog p_log);

    int libvlc_log_count(LibVlcLog p_log);

    void libvlc_log_clear(LibVlcLog p_log);

    LibVlcLogIterator libvlc_log_get_iterator(LibVlcLog p_log, libvlc_exception_t p_e);

    void libvlc_log_iterator_free(LibVlcLogIterator p_iter);

    int libvlc_log_iterator_has_next(LibVlcLogIterator p_iter);

    libvlc_log_message_t libvlc_log_iterator_next(LibVlcLogIterator p_iter, libvlc_log_message_t p_buffer,
        libvlc_exception_t p_e);

    LibVlcMediaDiscoverer libvlc_media_discoverer_new_from_name(LibVlcInstance p_inst, String psz_name,
        libvlc_exception_t p_e);

    void libvlc_media_discoverer_release(LibVlcMediaDiscoverer p_mdis);

    String libvlc_media_discoverer_localized_name(LibVlcMediaDiscoverer p_mdis);

    LibVlcMediaList libvlc_media_discoverer_media_list(LibVlcMediaDiscoverer p_mdis);

    LibVlcEventManager libvlc_media_discoverer_event_manager(LibVlcMediaDiscoverer p_mdis);

    int libvlc_media_discoverer_is_running(LibVlcMediaDiscoverer p_mdis);

    LibVlcMedia libvlc_media_new(LibVlcInstance p_instance, String psz_mrl, libvlc_exception_t p_e);

    LibVlcMedia libvlc_media_new_as_node(LibVlcInstance p_instance, String psz_name, libvlc_exception_t p_e);

    void libvlc_media_add_option(LibVlcMedia p_md, String ppsz_options, libvlc_exception_t p_e);

    void libvlc_media_add_option_flag(LibVlcMedia p_md, String ppsz_options, MediaOption i_flags, libvlc_exception_t p_e);

    void libvlc_media_retain(LibVlcMedia p_meta_desc);

    void libvlc_media_release(LibVlcMedia p_meta_desc);

    String libvlc_media_get_mrl(LibVlcMedia p_md, libvlc_exception_t p_e);

    LibVlcMedia libvlc_media_duplicate(LibVlcMedia p_meta_desc);

    String libvlc_media_get_meta(LibVlcMedia p_meta_desc, Meta e_meta, libvlc_exception_t p_e);

    int libvlc_media_get_state(LibVlcMedia p_meta_desc, libvlc_exception_t p_e);

    LibVlcMediaList libvlc_media_subitems(LibVlcMedia p_md, libvlc_exception_t p_e);

    LibVlcEventManager libvlc_media_event_manager(LibVlcMedia p_md, libvlc_exception_t p_e);

    long libvlc_media_get_duration(LibVlcMedia p_md, libvlc_exception_t p_e);

    int libvlc_media_is_preparsed(LibVlcMedia p_md, libvlc_exception_t p_e);

    void libvlc_media_set_user_data(LibVlcMedia p_md, Pointer p_new_user_data, libvlc_exception_t p_e);

    Pointer libvlc_media_get_user_data(LibVlcMedia p_md, libvlc_exception_t p_e);

    LibVlcMediaLibrary libvlc_media_library_new(LibVlcInstance p_inst, libvlc_exception_t p_e);

    void libvlc_media_library_release(LibVlcMediaLibrary p_mlib);

    void libvlc_media_library_retain(LibVlcMediaLibrary p_mlib);

    void libvlc_media_library_load(LibVlcMediaLibrary p_mlib, libvlc_exception_t p_e);

    void libvlc_media_library_save(LibVlcMediaLibrary p_mlib, libvlc_exception_t p_e);

    LibVlcMediaList libvlc_media_library_media_list(LibVlcMediaLibrary p_mlib, libvlc_exception_t p_e);

    LibVlcMediaList libvlc_media_list_new(LibVlcInstance p_libvlc, libvlc_exception_t p_e);

    void libvlc_media_list_release(LibVlcMediaList p_ml);

    void libvlc_media_list_retain(LibVlcMediaList p_ml);

    void libvlc_media_list_set_media(LibVlcMediaList p_ml, LibVlcMedia p_mi, libvlc_exception_t p_e);

    LibVlcMedia libvlc_media_list_media(LibVlcMediaList p_ml, libvlc_exception_t p_e);

    void libvlc_media_list_add_media(LibVlcMediaList p_ml, LibVlcMedia p_mi, libvlc_exception_t p_e);

    void libvlc_media_list_insert_media(LibVlcMediaList p_ml, LibVlcMedia p_mi, int i_pos, libvlc_exception_t p_e);

    void libvlc_media_list_remove_index(LibVlcMediaList p_ml, int i_pos, libvlc_exception_t p_e);

    int libvlc_media_list_count(LibVlcMediaList p_mlist, libvlc_exception_t p_e);

    LibVlcMedia libvlc_media_list_item_at_index(LibVlcMediaList p_ml, int i_pos, libvlc_exception_t p_e);

    int libvlc_media_list_index_of_item(LibVlcMediaList p_ml, LibVlcMedia p_mi, libvlc_exception_t p_e);

    int libvlc_media_list_is_readonly(LibVlcMediaList p_mlist);

    void libvlc_media_list_lock(LibVlcMediaList p_ml);

    void libvlc_media_list_unlock(LibVlcMediaList p_ml);

    LibVlcMediaListView libvlc_media_list_flat_view(LibVlcMediaList p_ml, libvlc_exception_t p_ex);

    LibVlcMediaListView libvlc_media_list_hierarchical_view(LibVlcMediaList p_ml, libvlc_exception_t p_ex);

    LibVlcMediaListView libvlc_media_list_hierarchical_node_view(LibVlcMediaList p_ml, libvlc_exception_t p_ex);

    LibVlcEventManager libvlc_media_list_event_manager(LibVlcMediaList p_ml, libvlc_exception_t p_ex);

    LibVlcMediaListPlayer libvlc_media_list_player_new(LibVlcInstance p_instance, libvlc_exception_t p_e);

    void libvlc_media_list_player_release(LibVlcMediaListPlayer p_mlp);

    LibVlcEventManager libvlc_media_list_player_event_manager(LibVlcMediaListPlayer p_mlp);

    void libvlc_media_list_player_set_media_player(LibVlcMediaListPlayer p_mlp, LibVlcMediaPlayer p_mi,
        libvlc_exception_t p_e);

    void libvlc_media_list_player_set_media_list(LibVlcMediaListPlayer p_mlp, LibVlcMediaList p_mlist,
        libvlc_exception_t p_e);

    void libvlc_media_list_player_play(LibVlcMediaListPlayer p_mlp, libvlc_exception_t p_e);

    void libvlc_media_list_player_pause(LibVlcMediaListPlayer p_mlp, libvlc_exception_t p_e);

    int libvlc_media_list_player_is_playing(LibVlcMediaListPlayer p_mlp, libvlc_exception_t p_e);

    int libvlc_media_list_player_get_state(LibVlcMediaListPlayer p_mlp, libvlc_exception_t p_e);

    void libvlc_media_list_player_play_item_at_index(LibVlcMediaListPlayer p_mlp, int i_index, libvlc_exception_t p_e);

    void libvlc_media_list_player_play_item(LibVlcMediaListPlayer p_mlp, LibVlcMedia p_md, libvlc_exception_t p_e);

    void libvlc_media_list_player_stop(LibVlcMediaListPlayer p_mlp, libvlc_exception_t p_e);

    void libvlc_media_list_player_next(LibVlcMediaListPlayer p_mlp, libvlc_exception_t p_e);

    void libvlc_media_list_player_previous(LibVlcMediaListPlayer p_mlp, libvlc_exception_t p_e);

    void libvlc_media_list_player_set_playback_mode(LibVlcMediaListPlayer p_mlp, PlaybackMode e_mode,
        libvlc_exception_t p_e);

    void libvlc_media_list_view_retain(LibVlcMediaListView p_mlv);

    void libvlc_media_list_view_release(LibVlcMediaListView p_mlv);

    LibVlcEventManager libvlc_media_list_view_event_manager(LibVlcMediaListView p_mlv);

    int libvlc_media_list_view_count(LibVlcMediaListView p_mlv, libvlc_exception_t p_e);

    LibVlcMedia libvlc_media_list_view_item_at_index(LibVlcMediaListView p_mlv, int i_index, libvlc_exception_t p_e);

    LibVlcMediaListView libvlc_media_list_view_children_at_index(LibVlcMediaListView p_mlv, int index,
        libvlc_exception_t p_e);

    LibVlcMediaListView libvlc_media_list_view_children_for_item(LibVlcMediaListView p_mlv, LibVlcMedia p_md,
        libvlc_exception_t p_e);

    LibVlcMediaList libvlc_media_list_view_parent_media_list(LibVlcMediaListView p_mlv, libvlc_exception_t p_e);

    LibVlcMediaPlayer libvlc_media_player_new(LibVlcInstance p_libvlc_instance, libvlc_exception_t p_e);

    LibVlcMediaPlayer libvlc_media_player_new_from_media(LibVlcMedia p_md, libvlc_exception_t p_e);

    void libvlc_media_player_release(LibVlcMediaPlayer p_mi);

    void libvlc_media_player_retain(LibVlcMediaPlayer p_mi);

    void libvlc_media_player_set_media(LibVlcMediaPlayer p_mi, LibVlcMedia p_md, libvlc_exception_t p_e);

    LibVlcMedia libvlc_media_player_get_media(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    LibVlcEventManager libvlc_media_player_event_manager(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    int libvlc_media_player_is_playing(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    void libvlc_media_player_play(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    void libvlc_media_player_pause(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    void libvlc_media_player_stop(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    void libvlc_media_player_set_nsobject(LibVlcMediaPlayer p_mi, Pointer drawable, libvlc_exception_t p_e);

    Pointer libvlc_media_player_get_nsobject(LibVlcMediaPlayer p_mi);

    void libvlc_media_player_set_agl(LibVlcMediaPlayer p_mi, long drawable, libvlc_exception_t p_e);

    long libvlc_media_player_get_agl(LibVlcMediaPlayer p_mi);

    void libvlc_media_player_set_xwindow(LibVlcMediaPlayer p_mi, long drawable, libvlc_exception_t p_e);

    long libvlc_media_player_get_xwindow(LibVlcMediaPlayer p_mi);

    void libvlc_media_player_set_hwnd(LibVlcMediaPlayer p_mi, long drawable, libvlc_exception_t p_e);

    Pointer libvlc_media_player_get_hwnd(LibVlcMediaPlayer p_mi);

    long libvlc_media_player_get_length(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    long libvlc_media_player_get_time(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    void libvlc_media_player_set_time(LibVlcMediaPlayer p_mi, long the, libvlc_exception_t p_e);

    float libvlc_media_player_get_position(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    void libvlc_media_player_set_position(LibVlcMediaPlayer p_mi, float f_pos, libvlc_exception_t p_e);

    void libvlc_media_player_set_chapter(LibVlcMediaPlayer p_mi, int i_chapter, libvlc_exception_t p_e);

    int libvlc_media_player_get_chapter(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    int libvlc_media_player_get_chapter_count(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    int libvlc_media_player_will_play(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    int libvlc_media_player_get_chapter_count_for_title(LibVlcMediaPlayer p_mi, int i_title, libvlc_exception_t p_e);

    void libvlc_media_player_set_title(LibVlcMediaPlayer p_mi, int i_title, libvlc_exception_t p_e);

    int libvlc_media_player_get_title(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    int libvlc_media_player_get_title_count(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    void libvlc_media_player_previous_chapter(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    void libvlc_media_player_next_chapter(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    float libvlc_media_player_get_rate(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    void libvlc_media_player_set_rate(LibVlcMediaPlayer p_mi, float movie, libvlc_exception_t p_e);

    int libvlc_media_player_get_state(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    float libvlc_media_player_get_fps(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    int libvlc_media_player_has_vout(LibVlcMediaPlayer p_md, libvlc_exception_t p_e);

    int libvlc_media_player_is_seekable(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    int libvlc_media_player_can_pause(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    void libvlc_media_player_next_frame(LibVlcMediaPlayer p_input, libvlc_exception_t p_e);

    void libvlc_track_description_release(LibVlcTrackDescription p_track_description);

    void libvlc_toggle_fullscreen(LibVlcMediaPlayer p_mediaplayer, libvlc_exception_t p_e);

    void libvlc_set_fullscreen(LibVlcMediaPlayer p_mediaplayer, int b_fullscreen, libvlc_exception_t p_e);

    int libvlc_get_fullscreen(LibVlcMediaPlayer p_mediaplayer, libvlc_exception_t p_e);

    int libvlc_video_get_height(LibVlcMediaPlayer p_mediaplayer, libvlc_exception_t p_e);

    int libvlc_video_get_width(LibVlcMediaPlayer p_mediaplayer, libvlc_exception_t p_e);

    float libvlc_video_get_scale(LibVlcMediaPlayer p_mediaplayer, libvlc_exception_t p_e);

    void libvlc_video_set_scale(LibVlcMediaPlayer p_mediaplayer, float i_factor, libvlc_exception_t p_e);

    String libvlc_video_get_aspect_ratio(LibVlcMediaPlayer p_mediaplayer, libvlc_exception_t p_e);

    void libvlc_video_set_aspect_ratio(LibVlcMediaPlayer p_mediaplayer, String psz_aspect, libvlc_exception_t p_e);

    int libvlc_video_get_spu(LibVlcMediaPlayer p_mediaplayer, libvlc_exception_t p_e);

    int libvlc_video_get_spu_count(LibVlcMediaPlayer p_mediaplayer, libvlc_exception_t p_e);

    LibVlcTrackDescription libvlc_video_get_spu_description(LibVlcMediaPlayer p_mediaplayer, libvlc_exception_t p_e);

    void libvlc_video_set_spu(LibVlcMediaPlayer p_mediaplayer, int i_spu, libvlc_exception_t p_e);

    int libvlc_video_set_subtitle_file(LibVlcMediaPlayer p_mediaplayer, String psz_subtitle, libvlc_exception_t p_e);

    LibVlcTrackDescription libvlc_video_get_title_description(LibVlcMediaPlayer p_mediaplayer, libvlc_exception_t p_e);

    LibVlcTrackDescription libvlc_video_get_chapter_description(LibVlcMediaPlayer p_mediaplayer, int i_title,
        libvlc_exception_t p_e);

    String libvlc_video_get_crop_geometry(LibVlcMediaPlayer p_mediaplayer, libvlc_exception_t p_e);

    void libvlc_video_set_crop_geometry(LibVlcMediaPlayer p_mediaplayer, String psz_geometry, libvlc_exception_t p_e);

    void libvlc_toggle_teletext(LibVlcMediaPlayer p_mediaplayer, libvlc_exception_t p_e);

    int libvlc_video_get_teletext(LibVlcMediaPlayer p_mediaplayer, libvlc_exception_t p_e);

    void libvlc_video_set_teletext(LibVlcMediaPlayer p_mediaplayer, int i_page, libvlc_exception_t p_e);

    int libvlc_video_get_track_count(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    LibVlcTrackDescription libvlc_video_get_track_description(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    int libvlc_video_get_track(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    void libvlc_video_set_track(LibVlcMediaPlayer p_mi, int i_track, libvlc_exception_t p_e);

    void libvlc_video_take_snapshot(LibVlcMediaPlayer p_mi, String psz_filepath, int i_width, int i_height,
        libvlc_exception_t p_e);

    void libvlc_video_set_deinterlace(LibVlcMediaPlayer p_mi, int b_enable, String psz_mode, libvlc_exception_t p_e);

    int libvlc_video_get_marquee_int(LibVlcMediaPlayer p_mi, VideoMarqueeIntOption option,
        libvlc_exception_t p_e);

    String libvlc_video_get_marquee_string(LibVlcMediaPlayer p_mi, VideoMarqueeStringOption option,
        libvlc_exception_t p_e);

    void libvlc_video_set_marquee_int(LibVlcMediaPlayer p_mi, VideoMarqueeIntOption option, int i_val,
        libvlc_exception_t p_e);

    void libvlc_video_set_marquee_string(LibVlcMediaPlayer p_mi, VideoMarqueeStringOption option,
        String psz_text, libvlc_exception_t p_e);

    LibVlcAudioOutput libvlc_audio_output_list_get(LibVlcInstance p_instance, libvlc_exception_t p_e);

    void libvlc_audio_output_list_release(LibVlcAudioOutput p_list);

    int libvlc_audio_output_set(LibVlcInstance p_instance, String psz_name);

    int libvlc_audio_output_device_count(LibVlcInstance p_instance, String psz_audio_output);

    String libvlc_audio_output_device_longname(LibVlcInstance p_instance, String psz_audio_output, int i_device);

    String libvlc_audio_output_device_id(LibVlcInstance p_instance, String psz_audio_output, int i_device);

    void libvlc_audio_output_device_set(LibVlcInstance p_instance, String psz_audio_output, String psz_device_id);

    int libvlc_audio_output_get_device_type(LibVlcInstance p_instance, libvlc_exception_t p_e);

    void libvlc_audio_output_set_device_type(LibVlcInstance p_instance, int device_type, libvlc_exception_t p_e);

    void libvlc_audio_toggle_mute(LibVlcInstance p_instance, libvlc_exception_t p_e);

    int libvlc_audio_get_mute(LibVlcInstance p_instance, libvlc_exception_t p_e);

    void libvlc_audio_set_mute(LibVlcInstance p_instance, int status, libvlc_exception_t p_e);

    int libvlc_audio_get_volume(LibVlcInstance p_instance, libvlc_exception_t p_e);

    void libvlc_audio_set_volume(LibVlcInstance p_instance, int i_volume, libvlc_exception_t p_e);

    int libvlc_audio_get_track_count(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    LibVlcTrackDescription libvlc_audio_get_track_description(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    int libvlc_audio_get_track(LibVlcMediaPlayer p_mi, libvlc_exception_t p_e);

    void libvlc_audio_set_track(LibVlcMediaPlayer p_mi, int i_track, libvlc_exception_t p_e);

    int libvlc_audio_get_channel(LibVlcInstance p_instance, libvlc_exception_t p_e);

    void libvlc_audio_set_channel(LibVlcInstance p_instance, int channel, libvlc_exception_t p_e);

    void libvlc_vlm_release(LibVlcInstance p_instance, libvlc_exception_t p_e);

    void libvlc_vlm_add_broadcast(LibVlcInstance p_instance, String psz_name, String psz_input, String psz_output,
        int i_options, String[] ppsz_options, int b_enabled, int b_loop, libvlc_exception_t p_e);

    void libvlc_vlm_add_vod(LibVlcInstance p_instance, String psz_name, String psz_input, int i_options,
        String[] ppsz_options, int b_enabled, String psz_mux, libvlc_exception_t p_e);

    void libvlc_vlm_del_media(LibVlcInstance p_instance, String psz_name, libvlc_exception_t p_e);

    void libvlc_vlm_set_enabled(LibVlcInstance p_instance, String psz_name, int b_enabled, libvlc_exception_t p_e);

    void libvlc_vlm_set_output(LibVlcInstance p_instance, String psz_name, String psz_output, libvlc_exception_t p_e);

    void libvlc_vlm_set_input(LibVlcInstance p_instance, String psz_name, String psz_input, libvlc_exception_t p_e);

    void libvlc_vlm_add_input(LibVlcInstance p_instance, String psz_name, String psz_input, libvlc_exception_t p_e);

    void libvlc_vlm_set_loop(LibVlcInstance p_instance, String psz_name, int b_loop, libvlc_exception_t p_e);

    void libvlc_vlm_set_mux(LibVlcInstance p_instance, String psz_name, String psz_mux, libvlc_exception_t p_e);

    void libvlc_vlm_change_media(LibVlcInstance p_instance, String psz_name, String psz_input, String psz_output,
        int i_options, String[] ppsz_options, int b_enabled, int b_loop, libvlc_exception_t p_e);

    void libvlc_vlm_play_media(LibVlcInstance p_instance, String psz_name, libvlc_exception_t p_e);

    void libvlc_vlm_stop_media(LibVlcInstance p_instance, String psz_name, libvlc_exception_t p_e);

    void libvlc_vlm_pause_media(LibVlcInstance p_instance, String psz_name, libvlc_exception_t p_e);

    void libvlc_vlm_seek_media(LibVlcInstance p_instance, String psz_name, float f_percentage, libvlc_exception_t p_e);

    String libvlc_vlm_show_media(LibVlcInstance p_instance, String psz_name, libvlc_exception_t p_e);

    float libvlc_vlm_get_media_instance_position(LibVlcInstance p_instance, String psz_name, int i_instance,
        libvlc_exception_t p_e);

    int libvlc_vlm_get_media_instance_time(LibVlcInstance p_instance, String psz_name, int i_instance,
        libvlc_exception_t p_e);

    int libvlc_vlm_get_media_instance_length(LibVlcInstance p_instance, String psz_name, int i_instance,
        libvlc_exception_t p_e);

    int libvlc_vlm_get_media_instance_rate(LibVlcInstance p_instance, String psz_name, int i_instance,
        libvlc_exception_t p_e);

    int libvlc_vlm_get_media_instance_title(LibVlcInstance p_instance, String psz_name, int i_instance,
        libvlc_exception_t p_e);

    int libvlc_vlm_get_media_instance_chapter(LibVlcInstance p_instance, String psz_name, int i_instance,
        libvlc_exception_t p_e);

    int libvlc_vlm_get_media_instance_seekable(LibVlcInstance p_instance, String psz_name, int i_instance,
        libvlc_exception_t p_e);

    LibVlcEventManager libvlc_vlm_get_event_manager(LibVlcInstance p_instance, libvlc_exception_t p_exception);

}
