/*****************************************************************************
 * modules_export.h: macros for exporting vlc symbols to plugins
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
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

#define STORE_SYMBOLS( p_symbols ) \
    (p_symbols)->p_main = p_main; \
    (p_symbols)->p_aout_bank = p_aout_bank; \
    (p_symbols)->p_vout_bank = p_vout_bank; \
    (p_symbols)->main_GetIntVariable = main_GetIntVariable; \
    (p_symbols)->main_GetPszVariable = main_GetPszVariable; \
    (p_symbols)->main_PutIntVariable = main_PutIntVariable; \
    (p_symbols)->main_PutPszVariable = main_PutPszVariable; \
    (p_symbols)->TestProgram = TestProgram; \
    (p_symbols)->TestMethod = TestMethod; \
    (p_symbols)->TestCPU = TestCPU; \
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
    (p_symbols)->input_NetlistInit = input_NetlistInit; \
    (p_symbols)->input_NetlistGetiovec = input_NetlistGetiovec; \
    (p_symbols)->input_NetlistMviovec = input_NetlistMviovec; \
    (p_symbols)->input_NetlistNewPacket = input_NetlistNewPacket; \
    (p_symbols)->input_NetlistNewPtr = input_NetlistNewPtr; \
    (p_symbols)->input_NetlistNewPES = input_NetlistNewPES; \
    (p_symbols)->input_NetlistDeletePacket = input_NetlistDeletePacket; \
    (p_symbols)->input_NetlistDeletePES = input_NetlistDeletePES; \
    (p_symbols)->input_NetlistEnd = input_NetlistEnd; \
    (p_symbols)->aout_CreateFifo = aout_CreateFifo; \
    (p_symbols)->aout_DestroyFifo = aout_DestroyFifo; \
    (p_symbols)->vout_CreateThread = vout_CreateThread; \
    (p_symbols)->vout_CreateSubPicture = vout_CreateSubPicture; \
    (p_symbols)->vout_DestroySubPicture = vout_DestroySubPicture; \
    (p_symbols)->vout_DisplaySubPicture = vout_DisplaySubPicture; \
    (p_symbols)->vout_CreatePicture = vout_CreatePicture; \
    (p_symbols)->vout_DisplayPicture = vout_DisplayPicture; \
    (p_symbols)->vout_DestroyPicture = vout_DestroyPicture; \
    (p_symbols)->vout_DatePicture = vout_DatePicture; \
    (p_symbols)->vout_LinkPicture = vout_LinkPicture; \
    (p_symbols)->vout_UnlinkPicture = vout_UnlinkPicture; \
    (p_symbols)->UnalignedGetBits = UnalignedGetBits; \
    (p_symbols)->UnalignedRemoveBits = UnalignedRemoveBits; \
    (p_symbols)->UnalignedShowBits = UnalignedShowBits; \
    (p_symbols)->DecodeLanguage = DecodeLanguage; \
    (p_symbols)->module_Need = module_Need; \
    (p_symbols)->module_Unneed = module_Unneed;
    
#define STORE_TRACE_SYMBOLS( p_symbols ) \
    (p_symbols)->intf_DbgMsg = _intf_DbgMsg; \
    (p_symbols)->intf_DbgMsgImm = _intf_DbgMsgImm;

#ifdef PLUGIN
#   define p_aout_bank (p_symbols->p_aout_bank)
#   define p_vout_bank (p_symbols->p_vout_bank)

#   define main_GetIntVariable(a,b) p_symbols->main_GetIntVariable(a,b)
#   define main_PutIntVariable(a,b) p_symbols->main_PutIntVariable(a,b)
#   define main_GetPszVariable(a,b) p_symbols->main_GetPszVariable(a,b)
#   define main_PutPszVariable(a,b) p_symbols->main_PutPszVariable(a,b)

#   define TestProgram(a) p_symbols->TestProgram(a)
#   define TestMethod(a,b) p_symbols->TestMethod(a,b)
#   define TestCPU(a) p_symbols->TestCPU(a)

#   define intf_Msg p_symbols->intf_Msg
#   define intf_ErrMsg p_symbols->intf_ErrMsg
#   define intf_StatMsg p_symbols->intf_StatMsg
#   define intf_WarnMsg p_symbols->intf_WarnMsg
#   define intf_WarnMsgImm p_symbols->intf_WarnMsgImm
#ifdef TRACE
#   undef  intf_DbgMsg
#   undef  intf_DbgMsgImm
#   define intf_DbgMsg( format, args... ) \
    p_symbols->intf_DbgMsg( __FILE__, __FUNCTION__, \
                            __LINE__, format, ## args )
#   define intf_DbgMsgImm( format, args... ) \
    p_symbols->intf_DbgMsgImm( __FILE__, __FUNCTION__, \
                               __LINE__, format, ## args )
#endif

#   define intf_PlaylistAdd(a,b,c) p_symbols->intf_PlaylistAdd(a,b,c)
#   define intf_PlaylistDelete(a,b) p_symbols->intf_PlaylistDelete(a,b)
#   define intf_PlaylistNext(a) p_symbols->intf_PlaylistNext(a)
#   define intf_PlaylistPrev(a) p_symbols->intf_PlaylistPrev(a)
#   define intf_PlaylistDestroy(a) p_symbols->intf_PlaylistDestroy(a)
#   define intf_PlaylistJumpto(a,b) p_symbols->intf_PlaylistJumpto(a,b)
#   define intf_UrlDecode(a) p_symbols->intf_UrlDecode(a)

#   define msleep(a) p_symbols->msleep(a)
#   define mdate() p_symbols->mdate()

#   define network_ChannelCreate p_symbols->network_ChannelCreate
#   define network_ChannelJoin p_symbols->network_ChannelJoin

#   define input_SetProgram p_symbols->input_SetProgram
#   define input_SetStatus p_symbols->input_SetStatus
#   define input_Seek p_symbols->input_Seek
#   define input_DumpStream(a) p_symbols->input_DumpStream(a)
#   define input_OffsetToTime(a,b,c) p_symbols->input_OffsetToTime(a,b,c)
#   define input_ChangeES(a,b,c) p_symbols->input_ChangeES(a,b,c)
#   define input_ToggleES(a,b,c) p_symbols->input_ToggleES(a,b,c)
#   define input_ChangeArea(a,b) p_symbols->input_ChangeArea(a,b)
#   define input_FindES p_symbols->input_FindES
#   define input_AddES p_symbols->input_AddES
#   define input_DelES p_symbols->input_DelES
#   define input_SelectES p_symbols->input_SelectES
#   define input_UnselectES p_symbols->input_UnselectES
#   define input_AddProgram p_symbols->input_AddProgram
#   define input_DelProgram p_symbols->input_DelProgram
#   define input_AddArea p_symbols->input_AddArea
#   define input_DelArea p_symbols->input_DelArea

#   define InitBitstream p_symbols->InitBitstream
#   define input_InitStream p_symbols->input_InitStream
#   define input_EndStream p_symbols->input_EndStream

#   define input_ParsePES p_symbols->input_ParsePES
#   define input_GatherPES p_symbols->input_GatherPES
#   define input_DecodePES p_symbols->input_DecodePES
#   define input_ParsePS p_symbols->input_ParsePS
#   define input_DemuxPS p_symbols->input_DemuxPS
#   define input_DemuxTS p_symbols->input_DemuxTS
#   define input_DemuxPSI p_symbols->input_DemuxPSI

#   define input_ClockManageControl p_symbols->input_ClockManageControl

#   define input_NetlistInit p_symbols->input_NetlistInit
#   define input_NetlistGetiovec p_symbols->input_NetlistGetiovec
#   define input_NetlistMviovec p_symbols->input_NetlistMviovec
#   define input_NetlistNewPacket p_symbols->input_NetlistNewPacket
#   define input_NetlistNewPtr p_symbols->input_NetlistNewPtr
#   define input_NetlistNewPES p_symbols->input_NetlistNewPES
#   define input_NetlistDeletePacket p_symbols->input_NetlistDeletePacket
#   define input_NetlistDeletePES p_symbols->input_NetlistDeletePES
#   define input_NetlistEnd p_symbols->input_NetlistEnd

#   define aout_CreateFifo p_symbols->aout_CreateFifo
#   define aout_DestroyFifo p_symbols->aout_DestroyFifo

#   define vout_CreateThread p_symbols->vout_CreateThread
#   define vout_CreateSubPicture p_symbols->vout_CreateSubPicture
#   define vout_DestroySubPicture p_symbols->vout_DestroySubPicture
#   define vout_DisplaySubPicture p_symbols->vout_DisplaySubPicture
#   define vout_CreatePicture p_symbols->vout_CreatePicture
#   define vout_DisplayPicture p_symbols->vout_DisplayPicture
#   define vout_DestroyPicture p_symbols->vout_DestroyPicture
#   define vout_DatePicture p_symbols->vout_DatePicture
#   define vout_LinkPicture p_symbols->vout_LinkPicture
#   define vout_UnlinkPicture p_symbols->vout_UnlinkPicture
    
#   define DecodeLanguage p_symbols->DecodeLanguage

#   define module_Need p_symbols->module_Need
#   define module_Unneed p_symbols->module_Unneed
    
#endif

