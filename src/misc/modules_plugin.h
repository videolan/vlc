/*****************************************************************************
 * modules_plugin.h : Plugin management functions used by the core application.
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: modules_plugin.h,v 1.1 2001/12/30 07:09:56 sam Exp $
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
static __inline__ int
module_load( char * psz_filename, module_handle_t * handle )
{
#ifdef SYS_BEOS
    *handle = load_add_on( psz_filename );
    return( *handle < 0 );

#elif defined(WIN32)
    *handle = LoadLibrary( psz_filename );
    return( *handle == NULL ); 

#elif defined(RTLD_NOW)
    /* Do not open modules with RTLD_GLOBAL, or we are going to get namespace
     * collisions when two modules have common public symbols */
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
static __inline__ void
module_unload( module_handle_t handle )
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
 * string, and returns a pointer to it.
 * FIXME: under Unix we should maybe check for dlerror() instead of the
 * return value of dlsym, since we could have loaded a symbol really set
 * to NULL (quite unlikely, though).
 *****************************************************************************/
static __inline__ void *
module_getsymbol( module_handle_t handle, char * psz_function )
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

#elif defined( SYS_DARWIN )
    /* MacOS X dl library expects symbols to begin with "_". That's
     * really lame, but hey, what can we do ? */
    char *  psz_call = malloc( strlen( psz_function ) + 2 );
    void *  p_return;
    strcpy( psz_call + 1, psz_function );
    psz_call[ 0 ] = '_';

    p_return = dlsym( handle, psz_call );

    free( psz_call );
    return( p_return );

#elif defined(WIN32)
    return( (void *)GetProcAddress( handle, psz_function ) );

#else
    return( dlsym( handle, psz_function ) );

#endif
}

/*****************************************************************************
 * module_error: wrapper for dlerror()
 *****************************************************************************
 * This function returns the error message of the last module operation. It
 * returns the string "failed" on systems which do not have the dlerror()
 * function.
 *****************************************************************************/
static __inline__ const char *
module_error( void )
{
#if defined(SYS_BEOS) || defined(WIN32)
    return( "failed" );

#else
    return( dlerror() );

#endif
}

/*****************************************************************************
 * STORE_SYMBOLS: store known symbols into p_symbols for plugin access.
 *****************************************************************************/
#ifdef TRACE
#   define STORE_TRACE_SYMBOLS( p_symbols ) \
        (p_symbols)->intf_DbgMsg = _intf_DbgMsg; \
        (p_symbols)->intf_DbgMsgImm = _intf_DbgMsgImm;
#else
#   define STORE_TRACE_SYMBOLS( p_symbols )
#endif

#define STORE_SYMBOLS( p_symbols ) \
    STORE_TRACE_SYMBOLS( p_symbols ) \
    (p_symbols)->p_main = p_main; \
    (p_symbols)->p_aout_bank = p_aout_bank; \
    (p_symbols)->p_vout_bank = p_vout_bank; \
    (p_symbols)->main_GetIntVariable = main_GetIntVariable; \
    (p_symbols)->main_GetPszVariable = main_GetPszVariable; \
    (p_symbols)->main_PutIntVariable = main_PutIntVariable; \
    (p_symbols)->main_PutPszVariable = main_PutPszVariable; \
    (p_symbols)->intf_Msg = intf_Msg; \
    (p_symbols)->intf_ErrMsg = intf_ErrMsg; \
    (p_symbols)->intf_StatMsg = intf_StatMsg;\
    (p_symbols)->intf_WarnMsg = intf_WarnMsg; \
    (p_symbols)->intf_WarnMsgImm = intf_WarnMsgImm; \
    (p_symbols)->intf_PlaylistAdd = intf_PlaylistAdd; \
    (p_symbols)->intf_PlaylistDelete = intf_PlaylistDelete; \
    (p_symbols)->intf_PlaylistNext = intf_PlaylistNext; \
    (p_symbols)->intf_PlaylistPrev = intf_PlaylistPrev; \
    (p_symbols)->intf_PlaylistDestroy = intf_PlaylistDestroy; \
    (p_symbols)->intf_PlaylistJumpto = intf_PlaylistJumpto; \
    (p_symbols)->intf_UrlDecode = intf_UrlDecode; \
    (p_symbols)->msleep = msleep; \
    (p_symbols)->mdate = mdate; \
    (p_symbols)->network_ChannelCreate = network_ChannelCreate; \
    (p_symbols)->network_ChannelJoin = network_ChannelJoin; \
    (p_symbols)->input_SetProgram = input_SetProgram; \
    (p_symbols)->input_SetStatus = input_SetStatus; \
    (p_symbols)->input_Seek = input_Seek; \
    (p_symbols)->input_DumpStream = input_DumpStream; \
    (p_symbols)->input_OffsetToTime = input_OffsetToTime; \
    (p_symbols)->input_ChangeES = input_ChangeES; \
    (p_symbols)->input_ToggleES = input_ToggleES; \
    (p_symbols)->input_ChangeArea = input_ChangeArea; \
    (p_symbols)->input_FindES = input_FindES; \
    (p_symbols)->input_AddES = input_AddES; \
    (p_symbols)->input_DelES = input_DelES; \
    (p_symbols)->input_SelectES = input_SelectES; \
    (p_symbols)->input_UnselectES = input_UnselectES; \
    (p_symbols)->input_AddProgram = input_AddProgram; \
    (p_symbols)->input_DelProgram = input_DelProgram; \
    (p_symbols)->input_AddArea = input_AddArea; \
    (p_symbols)->input_DelArea = input_DelArea; \
    (p_symbols)->InitBitstream = InitBitstream; \
    (p_symbols)->DecoderError = DecoderError; \
    (p_symbols)->input_InitStream = input_InitStream; \
    (p_symbols)->input_EndStream = input_EndStream; \
    (p_symbols)->input_ParsePES = input_ParsePES; \
    (p_symbols)->input_GatherPES = input_GatherPES; \
    (p_symbols)->input_DecodePES = input_DecodePES; \
    (p_symbols)->input_ParsePS = input_ParsePS; \
    (p_symbols)->input_DemuxPS = input_DemuxPS; \
    (p_symbols)->input_DemuxTS = input_DemuxTS; \
    (p_symbols)->input_DemuxPSI = input_DemuxPSI; \
    (p_symbols)->input_ClockManageControl = input_ClockManageControl; \
    (p_symbols)->aout_CreateFifo = aout_CreateFifo; \
    (p_symbols)->aout_DestroyFifo = aout_DestroyFifo; \
    (p_symbols)->vout_CreateThread = vout_CreateThread; \
    (p_symbols)->vout_DestroyThread = vout_DestroyThread; \
    (p_symbols)->vout_CreateSubPicture = vout_CreateSubPicture; \
    (p_symbols)->vout_DestroySubPicture = vout_DestroySubPicture; \
    (p_symbols)->vout_DisplaySubPicture = vout_DisplaySubPicture; \
    (p_symbols)->vout_CreatePicture = vout_CreatePicture; \
    (p_symbols)->vout_AllocatePicture = vout_AllocatePicture; \
    (p_symbols)->vout_DisplayPicture = vout_DisplayPicture; \
    (p_symbols)->vout_DestroyPicture = vout_DestroyPicture; \
    (p_symbols)->vout_DatePicture = vout_DatePicture; \
    (p_symbols)->vout_LinkPicture = vout_LinkPicture; \
    (p_symbols)->vout_UnlinkPicture = vout_UnlinkPicture; \
    (p_symbols)->vout_PlacePicture = vout_PlacePicture; \
    (p_symbols)->UnalignedGetBits = UnalignedGetBits; \
    (p_symbols)->UnalignedRemoveBits = UnalignedRemoveBits; \
    (p_symbols)->UnalignedShowBits = UnalignedShowBits; \
    (p_symbols)->DecodeLanguage = DecodeLanguage; \
    (p_symbols)->module_Need = module_Need; \
    (p_symbols)->module_Unneed = module_Unneed;
    

