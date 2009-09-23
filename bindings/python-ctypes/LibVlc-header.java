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
    LibVlc INSTANCE = (LibVlc) Native.loadLibrary(Platform.isWindows()? "libvlc" : "vlc", LibVlc.class);

    LibVlc SYNC_INSTANCE = (LibVlc) Native.synchronizedLibrary(INSTANCE);

    public static class libvlc_exception_t extends Structure
    {
        public int b_raised;
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
