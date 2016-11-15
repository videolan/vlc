" vlc specific stuff
au Syntax c call VlcSyntax()
au Syntax cpp call VlcSyntax()

function VlcSyntax()
  " Look for a VideoLAN copyright in the first 15 lines
  let line=1
  let vlc=0
  while(line<=15)
    if match(getline(line), ".*Copyright.*VideoLAN.*") > -1
      let vlc=1
      break
    endif
    let line=line+1
  endwhile
  if vlc==0
    return
  endif
  " true/false
  syn keyword cConstant VLC_TRUE VLC_FALSE
  " return values
  syn keyword cConstant VLC_SUCCESS VLC_EGENERIC VLC_ENOMEM VLC_ETHREAD
  syn keyword cConstant VLC_ESTATUS VLC_EEXIT VLC_EMODULE VLC_EOBJECT
  syn keyword cConstant VLC_ENOOBJ VLC_ENOMOD VLC_VAR_ADDRESS
  " custom types
  syn keyword cType vlc_fixed_t mtime_t byte_t dctelem_t ssize_t off_t
  syn keyword cType vlc_bool_t vlc_fourcc_t vlc_value_t
  " Core custom structures
  syn keyword cType vlc_t libvlc_t vlc_object_t vlc_error_t vlc_status_t 
  syn keyword cType variable_t date_t
  syn keyword cType vlc_thread_t vlc_cond_t vlc_mutex_t vlc_list_t
  " Objects, modules, configurations
  syn keyword cType module_bank_t module_t module_config_t module_symbols_t
  syn keyword cType module_cache_t config_category_t
  " Playlist
  syn keyword cType playlist_t playlist_item_t
  syn keyword cType services_discovery_t services_discovery_sys_t
  syn keyword cType item_info_t item_info_category_t 
  syn keyword cType sout_format_t playlist_export_t playlist_import_t
  " Intf
  syn keyword cType intf_thread_t intf_sys_t intf_console_t intf_msg_t
  syn keyword cType interaction_t interaction_dialog_t user_widget_t
  syn keyword cType msg_bank_t msg_subscription_t
  " Input
  syn keyword cType input_thread_t input_thread_sys_t input_item_t
  syn keyword cType access_t access_sys_t stream_t stream_sys_t 
  syn keyword cType demux_t demux_sys_t es_out_t es_out_id_t
  syn keyword cType es_out_sys_t  es_descriptor_t
  syn keyword cType seekpoint_t info_t info_category_t
  " Formats
  syn keyword cType audio_format_t video_format_t subs_format_t
  syn keyword cType es_format_t video_palette_t
  " Aout
  syn keyword cType audio_output_t aout_sys_t
  syn keyword cType aout_fifo_t audio_sample_format_t
  syn keyword cType aout_mixer_sys_t aout_filter_sys_t audio_volume_t
  syn keyword cType aout_mixer_t aout_output_t audio_date_t 
  syn keyword cType aout_filter_t
  " Vout
  syn keyword cType vout_thread_t  vout_sys_t vout_synchro_t
  syn keyword cType chroma_sys_t picture_t picture_sys_t picture_heap_t
  syn keyword cType video_frame_format_t
  " SPU
  syn keyword cType spu_t subpicture_t
  syn keyword cType subpicture_region_t text_style_t
  " Images
  syn keyword cType image_handler_t
  " Sout
  syn keyword cType sout_instance_t sout_cfg_t
  syn keyword cType sout_input_t sout_packetizer_input_t
  syn keyword cType sout_access_out_t sout_access_out_sys_t 
  syn keyword cType sout_mux_t sout_mux_sys_t
  syn keyword cType sout_stream_t sout_stream_sys_t
  " Sout - announce
  syn keyword cType session_descriptor_t
  syn keyword cType sap_address_t sap_handler_t sap_session_t
  " Decoders
  syn keyword cType decoder_t decoder_sys_t encoder_t encoder_sys_t
  " Filters
  syn keyword cType filter_t filter_sys_t
  " Blocks
  syn keyword cType block_t block_fifo_t
  " Network
  syn keyword cType network_socket_t vlc_acl_t 
  " HTTPD
  syn keyword cType httpd_t httpd_host_t httpd_url_t httpd_client_t
  syn keyword cType httpd_callback_sys_t httpd_message_t httpd_callback_t
  syn keyword cType httpd_file_t httpd_file_sys_t httpd_file_callback_t
  syn keyword cType httpd_handler_t httpd_handler_sys_t
  syn keyword cType httpd_handler_callback_t
  syn keyword cType httpd_redirect_t httpd_stream_t
  " TLS
  syn keyword cType tls_t tls_server_t tls_session_t
  " XML
  syn keyword cType xml_t xml_sys_t xml_reader_t xml_reader_sys_t
  " VoD
  syn keyword cType vod_t vod_sys_t vod_media_t
  " OpenGL
  syn keyword cType opengl_t opengl_sys_t
  " VLM
  syn keyword cType vlm_t vlm_message_t vlm_media_t vlm_schedule_t
  " Misc
  syn keyword cType md5_t vlc_meta_t vlc_callback_t iso639_lang_t
  
  " misc macros
  syn keyword cOperator VLC_OBJECT VLC_EXPORT VLC_COMMON_MEMBERS
  " don't use these any more, please
  syn keyword cError u8 s8 u16 s16 u32 s32 u64 s64
  " don't put trailing spaces! DON'T USE TABS!!!
  syn match cSpaceError display excludenl "\s\+$"
  syn match cSpaceError display "\t"

  " Todo
  syn keyword cTodo	contained TODO FIXME XXX \todo \bug
endfun

