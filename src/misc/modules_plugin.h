/*****************************************************************************
 * modules_plugin.h : Plugin management functions used by the core application.
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * Automatically generated from src/misc/modules_plugin.h.in by bootstrap.sh
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
 * Inline functions for handling dynamic modules
 *****************************************************************************/

/*****************************************************************************
 * module_load: load a dynamic library
 *****************************************************************************
 * This function loads a dynamically linked library using a system dependant
 * method, and returns a non-zero value on error, zero otherwise.
 *****************************************************************************/
static int module_load( const char * psz_filename, module_handle_t * handle )
{
#ifdef SYS_BEOS
    *handle = load_add_on( psz_filename );
    return( *handle < 0 );

#elif defined(WIN32)
    *handle = LoadLibrary( psz_filename );
    return( *handle == NULL ); 

#elif defined(RTLD_NOW)
#   if defined(SYS_LINUX)
    /* We should NOT open modules with RTLD_GLOBAL, or we are going to get
     * namespace collisions when two modules have common public symbols,
     * but ALSA is being a pest here. */
    if( strstr( psz_filename, "alsa.so" ) )
    {
        *handle = dlopen( psz_filename, RTLD_NOW | RTLD_GLOBAL );
        return( *handle == NULL );
    }
#   endif
    *handle = dlopen( psz_filename, RTLD_NOW );
    return( *handle == NULL );

#else
    *handle = dlopen( psz_filename, DL_LAZY );
    return( *handle == NULL );

#endif
}

/*****************************************************************************
 * module_unload: unload a dynamic library
 *****************************************************************************
 * This function unloads a previously opened dynamically linked library
 * using a system dependant method. No return value is taken in consideration,
 * since some libraries sometimes refuse to close properly.
 *****************************************************************************/
static void module_unload( module_handle_t handle )
{
#ifdef SYS_BEOS
    unload_add_on( handle );

#elif defined(WIN32)
    FreeLibrary( handle );

#else
    dlclose( handle );

#endif
    return;
}

/*****************************************************************************
 * module_getsymbol: get a symbol from a dynamic library
 *****************************************************************************
 * This function queries a loaded library for a symbol specified in a
 * string, and returns a pointer to it. We don't check for dlerror() or
 * similar functions, since we want a non-NULL symbol anyway.
 *****************************************************************************/
static void * _module_getsymbol( module_handle_t handle,
                                 const char * psz_function )
{
#ifdef SYS_BEOS
    void * p_symbol;
    if( B_OK == get_image_symbol( handle, psz_function,
                                  B_SYMBOL_TYPE_TEXT, &p_symbol ) )
    {
        return( p_symbol );
    }
    else
    {
        return( NULL );
    }

#elif defined(WIN32)
    return( (void *)GetProcAddress( handle, psz_function ) );

#else
    return( dlsym( handle, psz_function ) );

#endif
}

static void * module_getsymbol( module_handle_t handle,
                                const char * psz_function )
{
    void * p_symbol = _module_getsymbol( handle, psz_function );

    /* MacOS X dl library expects symbols to begin with "_". So do
     * some other operating systems. That's really lame, but hey, what
     * can we do ? */
    if( p_symbol == NULL )
    {
        char *psz_call = malloc( strlen( psz_function ) + 2 );

        strcpy( psz_call + 1, psz_function );
        psz_call[ 0 ] = '_';
        p_symbol = _module_getsymbol( handle, psz_call );
        free( psz_call );
    }

    return p_symbol;
}

/*****************************************************************************
 * module_error: wrapper for dlerror()
 *****************************************************************************
 * This function returns the error message of the last module operation. It
 * returns the string "failed" on systems which do not have a dlerror() like
 * function. psz_buffer can be used to store temporary data, it is guaranteed
 * to be kept intact until the return value of module_error has been used.
 *****************************************************************************/
static const char * module_error( char *psz_buffer )
{
#if defined(SYS_BEOS)
    return( "failed" );

#elif defined(WIN32)
    int i, i_error = GetLastError();

    FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, i_error, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPTSTR) psz_buffer, 256, NULL);

    /* Go to the end of the string */
    for( i = 0;
         psz_buffer[i] && psz_buffer[i] != '\r' && psz_buffer[i] != '\n';
         i++ ) {};

    if( psz_buffer[i] )
    {
        snprintf( psz_buffer + i, 256 - i, " (error %i)", i_error );
	psz_buffer[ 255 ] = '\0';
    }
    
    return psz_buffer;

#else
    return( dlerror() );

#endif
}

/*****************************************************************************
 * STORE_SYMBOLS: store known symbols into p_symbols for plugin access.
 *****************************************************************************/
#define STORE_SYMBOLS( p_symbols ) \
    (p_symbols)->aout_OutputNextBuffer_inner = aout_OutputNextBuffer; \
    (p_symbols)->__aout_NewInstance_inner = __aout_NewInstance; \
    (p_symbols)->aout_DeleteInstance_inner = aout_DeleteInstance; \
    (p_symbols)->aout_BufferNew_inner = aout_BufferNew; \
    (p_symbols)->aout_BufferDelete_inner = aout_BufferDelete; \
    (p_symbols)->aout_BufferPlay_inner = aout_BufferPlay; \
    (p_symbols)->aout_FormatToByterate_inner = aout_FormatToByterate; \
    (p_symbols)->__aout_InputNew_inner = __aout_InputNew; \
    (p_symbols)->aout_InputDelete_inner = aout_InputDelete; \
    (p_symbols)->__config_GetInt_inner = __config_GetInt; \
    (p_symbols)->__config_PutInt_inner = __config_PutInt; \
    (p_symbols)->__config_GetFloat_inner = __config_GetFloat; \
    (p_symbols)->__config_PutFloat_inner = __config_PutFloat; \
    (p_symbols)->__config_GetPsz_inner = __config_GetPsz; \
    (p_symbols)->__config_PutPsz_inner = __config_PutPsz; \
    (p_symbols)->__config_LoadCmdLine_inner = __config_LoadCmdLine; \
    (p_symbols)->config_GetHomeDir_inner = config_GetHomeDir; \
    (p_symbols)->__config_LoadConfigFile_inner = __config_LoadConfigFile; \
    (p_symbols)->__config_SaveConfigFile_inner = __config_SaveConfigFile; \
    (p_symbols)->config_FindConfig_inner = config_FindConfig; \
    (p_symbols)->config_Duplicate_inner = config_Duplicate; \
    (p_symbols)->config_SetCallbacks_inner = config_SetCallbacks; \
    (p_symbols)->config_UnsetCallbacks_inner = config_UnsetCallbacks; \
    (p_symbols)->InitBitstream_inner = InitBitstream; \
    (p_symbols)->NextDataPacket_inner = NextDataPacket; \
    (p_symbols)->BitstreamNextDataPacket_inner = BitstreamNextDataPacket; \
    (p_symbols)->UnalignedShowBits_inner = UnalignedShowBits; \
    (p_symbols)->UnalignedRemoveBits_inner = UnalignedRemoveBits; \
    (p_symbols)->UnalignedGetBits_inner = UnalignedGetBits; \
    (p_symbols)->CurrentPTS_inner = CurrentPTS; \
    (p_symbols)->DecoderError_inner = DecoderError; \
    (p_symbols)->__input_SetStatus_inner = __input_SetStatus; \
    (p_symbols)->__input_Seek_inner = __input_Seek; \
    (p_symbols)->__input_Tell_inner = __input_Tell; \
    (p_symbols)->input_DumpStream_inner = input_DumpStream; \
    (p_symbols)->input_OffsetToTime_inner = input_OffsetToTime; \
    (p_symbols)->input_ToggleES_inner = input_ToggleES; \
    (p_symbols)->input_ChangeArea_inner = input_ChangeArea; \
    (p_symbols)->input_ChangeProgram_inner = input_ChangeProgram; \
    (p_symbols)->input_InitStream_inner = input_InitStream; \
    (p_symbols)->input_EndStream_inner = input_EndStream; \
    (p_symbols)->input_FindProgram_inner = input_FindProgram; \
    (p_symbols)->input_AddProgram_inner = input_AddProgram; \
    (p_symbols)->input_DelProgram_inner = input_DelProgram; \
    (p_symbols)->input_SetProgram_inner = input_SetProgram; \
    (p_symbols)->input_AddArea_inner = input_AddArea; \
    (p_symbols)->input_DelArea_inner = input_DelArea; \
    (p_symbols)->input_FindES_inner = input_FindES; \
    (p_symbols)->input_AddES_inner = input_AddES; \
    (p_symbols)->input_DelES_inner = input_DelES; \
    (p_symbols)->input_SelectES_inner = input_SelectES; \
    (p_symbols)->input_UnselectES_inner = input_UnselectES; \
    (p_symbols)->input_DecodePES_inner = input_DecodePES; \
    (p_symbols)->input_ClockManageControl_inner = input_ClockManageControl; \
    (p_symbols)->input_ClockManageRef_inner = input_ClockManageRef; \
    (p_symbols)->input_ClockGetTS_inner = input_ClockGetTS; \
    (p_symbols)->input_InfoCategory_inner = input_InfoCategory; \
    (p_symbols)->input_AddInfo_inner = input_AddInfo; \
    (p_symbols)->input_BuffersEnd_inner = input_BuffersEnd; \
    (p_symbols)->input_NewBuffer_inner = input_NewBuffer; \
    (p_symbols)->input_ReleaseBuffer_inner = input_ReleaseBuffer; \
    (p_symbols)->input_ShareBuffer_inner = input_ShareBuffer; \
    (p_symbols)->input_NewPacket_inner = input_NewPacket; \
    (p_symbols)->input_DeletePacket_inner = input_DeletePacket; \
    (p_symbols)->input_NewPES_inner = input_NewPES; \
    (p_symbols)->input_DeletePES_inner = input_DeletePES; \
    (p_symbols)->input_FillBuffer_inner = input_FillBuffer; \
    (p_symbols)->input_Peek_inner = input_Peek; \
    (p_symbols)->input_SplitBuffer_inner = input_SplitBuffer; \
    (p_symbols)->input_AccessInit_inner = input_AccessInit; \
    (p_symbols)->input_AccessReinit_inner = input_AccessReinit; \
    (p_symbols)->input_AccessEnd_inner = input_AccessEnd; \
    (p_symbols)->__input_FDClose_inner = __input_FDClose; \
    (p_symbols)->__input_FDNetworkClose_inner = __input_FDNetworkClose; \
    (p_symbols)->input_FDRead_inner = input_FDRead; \
    (p_symbols)->input_FDNetworkRead_inner = input_FDNetworkRead; \
    (p_symbols)->input_FDSeek_inner = input_FDSeek; \
    (p_symbols)->__intf_Create_inner = __intf_Create; \
    (p_symbols)->intf_RunThread_inner = intf_RunThread; \
    (p_symbols)->intf_StopThread_inner = intf_StopThread; \
    (p_symbols)->intf_Destroy_inner = intf_Destroy; \
    (p_symbols)->__intf_Eject_inner = __intf_Eject; \
    (p_symbols)->GetLang_1_inner = GetLang_1; \
    (p_symbols)->GetLang_2T_inner = GetLang_2T; \
    (p_symbols)->GetLang_2B_inner = GetLang_2B; \
    (p_symbols)->DecodeLanguage_inner = DecodeLanguage; \
    (p_symbols)->__module_Need_inner = __module_Need; \
    (p_symbols)->__module_Unneed_inner = __module_Unneed; \
    (p_symbols)->mstrtime_inner = mstrtime; \
    (p_symbols)->mdate_inner = mdate; \
    (p_symbols)->mwait_inner = mwait; \
    (p_symbols)->msleep_inner = msleep; \
    (p_symbols)->__network_ChannelJoin_inner = __network_ChannelJoin; \
    (p_symbols)->__network_ChannelCreate_inner = __network_ChannelCreate; \
    (p_symbols)->__sout_NewInstance_inner = __sout_NewInstance; \
    (p_symbols)->sout_DeleteInstance_inner = sout_DeleteInstance; \
    (p_symbols)->__vout_CreateThread_inner = __vout_CreateThread; \
    (p_symbols)->vout_DestroyThread_inner = vout_DestroyThread; \
    (p_symbols)->vout_ChromaCmp_inner = vout_ChromaCmp; \
    (p_symbols)->vout_CreatePicture_inner = vout_CreatePicture; \
    (p_symbols)->vout_AllocatePicture_inner = vout_AllocatePicture; \
    (p_symbols)->vout_DestroyPicture_inner = vout_DestroyPicture; \
    (p_symbols)->vout_DisplayPicture_inner = vout_DisplayPicture; \
    (p_symbols)->vout_DatePicture_inner = vout_DatePicture; \
    (p_symbols)->vout_LinkPicture_inner = vout_LinkPicture; \
    (p_symbols)->vout_UnlinkPicture_inner = vout_UnlinkPicture; \
    (p_symbols)->vout_PlacePicture_inner = vout_PlacePicture; \
    (p_symbols)->vout_CreateSubPicture_inner = vout_CreateSubPicture; \
    (p_symbols)->vout_DestroySubPicture_inner = vout_DestroySubPicture; \
    (p_symbols)->vout_DisplaySubPicture_inner = vout_DisplaySubPicture; \
    (p_symbols)->__msg_Generic_inner = __msg_Generic; \
    (p_symbols)->__msg_Info_inner = __msg_Info; \
    (p_symbols)->__msg_Err_inner = __msg_Err; \
    (p_symbols)->__msg_Warn_inner = __msg_Warn; \
    (p_symbols)->__msg_Dbg_inner = __msg_Dbg; \
    (p_symbols)->__msg_Subscribe_inner = __msg_Subscribe; \
    (p_symbols)->__msg_Unsubscribe_inner = __msg_Unsubscribe; \
    (p_symbols)->__vlc_object_create_inner = __vlc_object_create; \
    (p_symbols)->__vlc_object_destroy_inner = __vlc_object_destroy; \
    (p_symbols)->__vlc_object_attach_inner = __vlc_object_attach; \
    (p_symbols)->__vlc_object_detach_inner = __vlc_object_detach; \
    (p_symbols)->__vlc_object_find_inner = __vlc_object_find; \
    (p_symbols)->__vlc_object_yield_inner = __vlc_object_yield; \
    (p_symbols)->__vlc_object_release_inner = __vlc_object_release; \
    (p_symbols)->__vlc_list_find_inner = __vlc_list_find; \
    (p_symbols)->__vlc_list_release_inner = __vlc_list_release; \
    (p_symbols)->__vlc_liststructure_inner = __vlc_liststructure; \
    (p_symbols)->__vlc_dumpstructure_inner = __vlc_dumpstructure; \
    (p_symbols)->playlist_Command_inner = playlist_Command; \
    (p_symbols)->playlist_Add_inner = playlist_Add; \
    (p_symbols)->playlist_Delete_inner = playlist_Delete; \
    (p_symbols)->__vlc_threads_init_inner = __vlc_threads_init; \
    (p_symbols)->__vlc_threads_end_inner = __vlc_threads_end; \
    (p_symbols)->__vlc_mutex_init_inner = __vlc_mutex_init; \
    (p_symbols)->__vlc_mutex_destroy_inner = __vlc_mutex_destroy; \
    (p_symbols)->__vlc_cond_init_inner = __vlc_cond_init; \
    (p_symbols)->__vlc_cond_destroy_inner = __vlc_cond_destroy; \
    (p_symbols)->__vlc_thread_create_inner = __vlc_thread_create; \
    (p_symbols)->__vlc_thread_ready_inner = __vlc_thread_ready; \
    (p_symbols)->__vlc_thread_join_inner = __vlc_thread_join; \

