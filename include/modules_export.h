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

typedef struct module_symbols_s
{
    struct main_s* p_main;
    struct aout_bank_s* p_aout_bank;
    struct vout_bank_s* p_vout_bank;

    int    ( * main_GetIntVariable ) ( char *, int );
    char * ( * main_GetPszVariable ) ( char *, char * );
    void   ( * main_PutIntVariable ) ( char *, int );
    void   ( * main_PutPszVariable ) ( char *, char * );

    int  ( * TestProgram )  ( char * );
    int  ( * TestMethod )   ( char *, char * );
    int  ( * TestCPU )      ( int );

    int  ( * intf_ProcessKey ) ( struct intf_thread_s *, int );
    void ( * intf_AssignKey )  ( struct intf_thread_s *, int, int, int );

    void ( * intf_Msg )     ( char *, ... );
    void ( * intf_ErrMsg )  ( char *, ... );
    void ( * intf_WarnMsg ) ( int, char *, ... );

    int  ( * intf_PlaylistAdd )     ( struct playlist_s *, int, const char* );
    int  ( * intf_PlaylistDelete )  ( struct playlist_s *, int );
    void ( * intf_PlaylistNext )    ( struct playlist_s * );
    void ( * intf_PlaylistPrev )    ( struct playlist_s * );
    void ( * intf_PlaylistDestroy ) ( struct playlist_s * );
    void ( * intf_PlaylistJumpto )  ( struct playlist_s *, int );
    void ( * intf_UrlDecode )       ( char * );

    void    ( * msleep )         ( mtime_t );
    mtime_t ( * mdate )          ( void );

    int  ( * network_ChannelCreate )( void );
    int  ( * network_ChannelJoin )  ( int );

    void ( * input_SetStatus )      ( struct input_thread_s *, int );
    void ( * input_SetRate )        ( struct input_thread_s *, int );
    void ( * input_Seek )           ( struct input_thread_s *, off_t );
    void ( * input_DumpStream )     ( struct input_thread_s * );
    char * ( * input_OffsetToTime ) ( struct input_thread_s *, char *, off_t );
    int  ( * input_ChangeES )       ( struct input_thread_s *,
                                      struct es_descriptor_s *, u8 );
    int  ( * input_ToggleES )       ( struct input_thread_s *,
                                      struct es_descriptor_s *, boolean_t );
    int  ( * input_ChangeArea )     ( struct input_thread_s *,
                                      struct input_area_s * );

} module_symbols_t;

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
    (p_symbols)->intf_AssignKey = intf_AssignKey; \
    (p_symbols)->intf_ProcessKey = intf_ProcessKey; \
    (p_symbols)->intf_Msg = intf_Msg; \
    (p_symbols)->intf_ErrMsg = intf_ErrMsg; \
    (p_symbols)->intf_WarnMsg = intf_WarnMsg; \
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
    (p_symbols)->input_SetStatus = input_SetStatus; \
    (p_symbols)->input_SetRate = input_SetRate; \
    (p_symbols)->input_Seek = input_Seek; \
    (p_symbols)->input_DumpStream = input_DumpStream; \
    (p_symbols)->input_OffsetToTime = input_OffsetToTime; \
    (p_symbols)->input_ChangeES = input_ChangeES; \
    (p_symbols)->input_ToggleES = input_ToggleES; \
    (p_symbols)->input_ChangeArea = input_ChangeArea;
    
#ifdef PLUGIN
extern module_symbols_t* p_symbols;

#   define p_main (p_symbols->p_main)
#   define p_aout_bank (p_symbols->p_aout_bank)
#   define p_vout_bank (p_symbols->p_vout_bank)

#   define main_GetIntVariable(a,b) p_symbols->main_GetIntVariable(a,b)
#   define main_PutIntVariable(a,b) p_symbols->main_PutIntVariable(a,b)
#   define main_GetPszVariable(a,b) p_symbols->main_GetPszVariable(a,b)
#   define main_PutPszVariable(a,b) p_symbols->main_PutPszVariable(a,b)

#   define TestProgram(a) p_symbols->TestProgram(a)
#   define TestMethod(a,b) p_symbols->TestMethod(a,b)
#   define TestCPU(a,b) p_symbols->TestCPU(a)

#   define intf_AssignKey(a,b,c,d) p_symbols->intf_AssignKey(a,b,c,d)
#   define intf_ProcessKey(a,b) p_symbols->intf_ProcessKey(a,b)

#   define intf_Msg(a,b...) p_symbols->intf_Msg(a, ## b)
#   define intf_ErrMsg(a,b...) p_symbols->intf_ErrMsg(a, ## b)
#   define intf_WarnMsg(a,b,c...) p_symbols->intf_WarnMsg(a,b, ## c)

#   define intf_PlaylistAdd(a,b,c) p_symbols->intf_PlaylistAdd(a,b,c)
#   define intf_PlaylistDelete(a,b) p_symbols->intf_PlaylistDelete(a,b)
#   define intf_PlaylistNext(a) p_symbols->intf_PlaylistNext(a)
#   define intf_PlaylistPrev(a) p_symbols->intf_PlaylistPrev(a)
#   define intf_PlaylistDestroy(a) p_symbols->intf_PlaylistDestroy(a)
#   define intf_PlaylistJumpto(a,b) p_symbols->intf_PlaylistJumpto(a,b)
#   define intf_UrlDecode(a) p_symbols->intf_UrlDecode(a)

#   define msleep(a) p_symbols->msleep(a)
#   define mdate() p_symbols->mdate()

#   define network_ChannelCreate() p_symbols->network_ChannelCreate()
#   define network_ChannelJoin(a) p_symbols->network_ChannelJoin(a)

#   define input_SetStatus(a,b) p_symbols->input_SetStatus(a,b)
#   define input_SetRate(a,b) p_symbols->input_SetRate(a,b)
#   define input_Seek(a,b) p_symbols->input_Seek(a,b)
#   define input_DumpStream(a) p_symbols->input_DumpStream(a)
#   define input_OffsetToTime(a,b,c) p_symbols->input_OffsetToTime(a,b,c)
#   define input_ChangeES(a,b,c) p_symbols->input_ChangeES(a,b,c)
#   define input_ToggleES(a,b,c) p_symbols->input_ToggleES(a,b,c)
#   define input_ChangeArea(a,b) p_symbols->input_ChangeArea(a,b)

#endif

