

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 6.00.0361 */
/* at Mon Jun 25 12:18:49 2007
 */
/* Compiler settings for axvlc.idl:
    Oicf, W1, Zp8, env=Win32 (32b run)
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
//@@MIDL_FILE_HEADING(  )

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif // __RPCNDR_H_VERSION__


#ifndef __axvlc_idl_h__
#define __axvlc_idl_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IVLCControl_FWD_DEFINED__
#define __IVLCControl_FWD_DEFINED__
typedef interface IVLCControl IVLCControl;
#endif 	/* __IVLCControl_FWD_DEFINED__ */


#ifndef __IVLCAudio_FWD_DEFINED__
#define __IVLCAudio_FWD_DEFINED__
typedef interface IVLCAudio IVLCAudio;
#endif 	/* __IVLCAudio_FWD_DEFINED__ */


#ifndef __IVLCInput_FWD_DEFINED__
#define __IVLCInput_FWD_DEFINED__
typedef interface IVLCInput IVLCInput;
#endif 	/* __IVLCInput_FWD_DEFINED__ */


#ifndef __IVLCLog_FWD_DEFINED__
#define __IVLCLog_FWD_DEFINED__
typedef interface IVLCLog IVLCLog;
#endif 	/* __IVLCLog_FWD_DEFINED__ */


#ifndef __IVLCMessage_FWD_DEFINED__
#define __IVLCMessage_FWD_DEFINED__
typedef interface IVLCMessage IVLCMessage;
#endif 	/* __IVLCMessage_FWD_DEFINED__ */


#ifndef __IVLCMessageIterator_FWD_DEFINED__
#define __IVLCMessageIterator_FWD_DEFINED__
typedef interface IVLCMessageIterator IVLCMessageIterator;
#endif 	/* __IVLCMessageIterator_FWD_DEFINED__ */


#ifndef __IVLCMessages_FWD_DEFINED__
#define __IVLCMessages_FWD_DEFINED__
typedef interface IVLCMessages IVLCMessages;
#endif 	/* __IVLCMessages_FWD_DEFINED__ */


#ifndef __IVLCPlaylist_FWD_DEFINED__
#define __IVLCPlaylist_FWD_DEFINED__
typedef interface IVLCPlaylist IVLCPlaylist;
#endif 	/* __IVLCPlaylist_FWD_DEFINED__ */


#ifndef __IVLCVideo_FWD_DEFINED__
#define __IVLCVideo_FWD_DEFINED__
typedef interface IVLCVideo IVLCVideo;
#endif 	/* __IVLCVideo_FWD_DEFINED__ */


#ifndef __IVLCControl2_FWD_DEFINED__
#define __IVLCControl2_FWD_DEFINED__
typedef interface IVLCControl2 IVLCControl2;
#endif 	/* __IVLCControl2_FWD_DEFINED__ */


#ifndef __DVLCEvents_FWD_DEFINED__
#define __DVLCEvents_FWD_DEFINED__
typedef interface DVLCEvents DVLCEvents;
#endif 	/* __DVLCEvents_FWD_DEFINED__ */


#ifndef __IVLCPlaylistItems_FWD_DEFINED__
#define __IVLCPlaylistItems_FWD_DEFINED__
typedef interface IVLCPlaylistItems IVLCPlaylistItems;
#endif 	/* __IVLCPlaylistItems_FWD_DEFINED__ */


#ifndef __VLCPlugin_FWD_DEFINED__
#define __VLCPlugin_FWD_DEFINED__

#ifdef __cplusplus
typedef class VLCPlugin VLCPlugin;
#else
typedef struct VLCPlugin VLCPlugin;
#endif /* __cplusplus */

#endif 	/* __VLCPlugin_FWD_DEFINED__ */


#ifndef __VLCPlugin2_FWD_DEFINED__
#define __VLCPlugin2_FWD_DEFINED__

#ifdef __cplusplus
typedef class VLCPlugin2 VLCPlugin2;
#else
typedef struct VLCPlugin2 VLCPlugin2;
#endif /* __cplusplus */

#endif 	/* __VLCPlugin2_FWD_DEFINED__ */


/* header files for imported files */
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 

void * __RPC_USER MIDL_user_allocate(size_t);
void __RPC_USER MIDL_user_free( void * ); 


#ifndef __AXVLC_LIBRARY_DEFINED__
#define __AXVLC_LIBRARY_DEFINED__

/* library AXVLC */
/* [helpstring][version][uuid] */ 












typedef /* [public] */ 
enum VLCPlaylistMode
    {	VLCPlayListInsert	= 1,
	VLCPlayListInsertAndGo	= 9,
	VLCPlayListReplace	= 2,
	VLCPlayListReplaceAndGo	= 10,
	VLCPlayListAppend	= 4,
	VLCPlayListAppendAndGo	= 12,
	VLCPlayListCheckInsert	= 16
    } 	eVLCPlaylistMode;

#define	VLCPlayListEnd	( -666 )

#define	DISPID_BackColor	( -501 )

#define	DISPID_Visible	( 100 )

#define	DISPID_Playing	( 101 )

#define	DISPID_Position	( 102 )

#define	DISPID_Time	( 103 )

#define	DISPID_Length	( 104 )

#define	DISPID_Volume	( 105 )

#define	DISPID_MRL	( 106 )

#define	DISPID_AutoPlay	( 107 )

#define	DISPID_AutoLoop	( 108 )

#define	DISPID_StartTime	( 109 )

#define	DISPID_BaseURL	( 110 )

#define	DISPID_PlayEvent	( 100 )

#define	DISPID_PauseEvent	( 101 )

#define	DISPID_StopEvent	( 102 )


EXTERN_C const IID LIBID_AXVLC;

#ifndef __IVLCControl_INTERFACE_DEFINED__
#define __IVLCControl_INTERFACE_DEFINED__

/* interface IVLCControl */
/* [object][oleautomation][dual][helpstring][uuid] */ 


EXTERN_C const IID IID_IVLCControl;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("C2FA41D0-B113-476e-AC8C-9BD14999C1C1")
    IVLCControl : public IDispatch
    {
    public:
        virtual /* [helpstring][bindable][propget][id] */ HRESULT STDMETHODCALLTYPE get_Visible( 
            /* [retval][out] */ VARIANT_BOOL *visible) = 0;
        
        virtual /* [helpstring][bindable][propput][id] */ HRESULT STDMETHODCALLTYPE put_Visible( 
            /* [in] */ VARIANT_BOOL visible) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE play( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE pause( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE stop( void) = 0;
        
        virtual /* [helpstring][propget][hidden][id] */ HRESULT STDMETHODCALLTYPE get_Playing( 
            /* [retval][out] */ VARIANT_BOOL *isPlaying) = 0;
        
        virtual /* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Position( 
            /* [retval][out] */ float *position) = 0;
        
        virtual /* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_Position( 
            /* [in] */ float position) = 0;
        
        virtual /* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Time( 
            /* [retval][out] */ int *seconds) = 0;
        
        virtual /* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_Time( 
            /* [in] */ int seconds) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE shuttle( 
            /* [in] */ int seconds) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE fullscreen( void) = 0;
        
        virtual /* [helpstring][hidden][propget][id] */ HRESULT STDMETHODCALLTYPE get_Length( 
            /* [retval][out] */ int *seconds) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE playFaster( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE playSlower( void) = 0;
        
        virtual /* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Volume( 
            /* [retval][out] */ int *volume) = 0;
        
        virtual /* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_Volume( 
            /* [in] */ int volume) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE toggleMute( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE setVariable( 
            /* [in] */ BSTR name,
            /* [in] */ VARIANT value) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE getVariable( 
            /* [in] */ BSTR name,
            /* [retval][out] */ VARIANT *value) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE addTarget( 
            /* [in] */ BSTR uri,
            /* [in] */ VARIANT options,
            /* [in] */ enum VLCPlaylistMode mode,
            /* [in] */ int position) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_PlaylistIndex( 
            /* [retval][out] */ int *index) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_PlaylistCount( 
            /* [retval][out] */ int *index) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE playlistNext( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE playlistPrev( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE playlistClear( void) = 0;
        
        virtual /* [helpstring][hidden][propget] */ HRESULT STDMETHODCALLTYPE get_VersionInfo( 
            /* [retval][out] */ BSTR *version) = 0;
        
        virtual /* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_MRL( 
            /* [retval][out] */ BSTR *mrl) = 0;
        
        virtual /* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_MRL( 
            /* [in] */ BSTR mrl) = 0;
        
        virtual /* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_AutoPlay( 
            /* [retval][out] */ VARIANT_BOOL *autoplay) = 0;
        
        virtual /* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_AutoPlay( 
            /* [in] */ VARIANT_BOOL autoplay) = 0;
        
        virtual /* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_AutoLoop( 
            /* [retval][out] */ VARIANT_BOOL *autoloop) = 0;
        
        virtual /* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_AutoLoop( 
            /* [in] */ VARIANT_BOOL autoloop) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVLCControlVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVLCControl * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVLCControl * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVLCControl * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVLCControl * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVLCControl * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVLCControl * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVLCControl * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][bindable][propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Visible )( 
            IVLCControl * This,
            /* [retval][out] */ VARIANT_BOOL *visible);
        
        /* [helpstring][bindable][propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_Visible )( 
            IVLCControl * This,
            /* [in] */ VARIANT_BOOL visible);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *play )( 
            IVLCControl * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *pause )( 
            IVLCControl * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *stop )( 
            IVLCControl * This);
        
        /* [helpstring][propget][hidden][id] */ HRESULT ( STDMETHODCALLTYPE *get_Playing )( 
            IVLCControl * This,
            /* [retval][out] */ VARIANT_BOOL *isPlaying);
        
        /* [helpstring][propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Position )( 
            IVLCControl * This,
            /* [retval][out] */ float *position);
        
        /* [helpstring][propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_Position )( 
            IVLCControl * This,
            /* [in] */ float position);
        
        /* [helpstring][propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Time )( 
            IVLCControl * This,
            /* [retval][out] */ int *seconds);
        
        /* [helpstring][propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_Time )( 
            IVLCControl * This,
            /* [in] */ int seconds);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *shuttle )( 
            IVLCControl * This,
            /* [in] */ int seconds);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *fullscreen )( 
            IVLCControl * This);
        
        /* [helpstring][hidden][propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Length )( 
            IVLCControl * This,
            /* [retval][out] */ int *seconds);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *playFaster )( 
            IVLCControl * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *playSlower )( 
            IVLCControl * This);
        
        /* [helpstring][propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Volume )( 
            IVLCControl * This,
            /* [retval][out] */ int *volume);
        
        /* [helpstring][propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_Volume )( 
            IVLCControl * This,
            /* [in] */ int volume);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *toggleMute )( 
            IVLCControl * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *setVariable )( 
            IVLCControl * This,
            /* [in] */ BSTR name,
            /* [in] */ VARIANT value);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *getVariable )( 
            IVLCControl * This,
            /* [in] */ BSTR name,
            /* [retval][out] */ VARIANT *value);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *addTarget )( 
            IVLCControl * This,
            /* [in] */ BSTR uri,
            /* [in] */ VARIANT options,
            /* [in] */ enum VLCPlaylistMode mode,
            /* [in] */ int position);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_PlaylistIndex )( 
            IVLCControl * This,
            /* [retval][out] */ int *index);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_PlaylistCount )( 
            IVLCControl * This,
            /* [retval][out] */ int *index);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *playlistNext )( 
            IVLCControl * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *playlistPrev )( 
            IVLCControl * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *playlistClear )( 
            IVLCControl * This);
        
        /* [helpstring][hidden][propget] */ HRESULT ( STDMETHODCALLTYPE *get_VersionInfo )( 
            IVLCControl * This,
            /* [retval][out] */ BSTR *version);
        
        /* [helpstring][propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_MRL )( 
            IVLCControl * This,
            /* [retval][out] */ BSTR *mrl);
        
        /* [helpstring][propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_MRL )( 
            IVLCControl * This,
            /* [in] */ BSTR mrl);
        
        /* [helpstring][propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_AutoPlay )( 
            IVLCControl * This,
            /* [retval][out] */ VARIANT_BOOL *autoplay);
        
        /* [helpstring][propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_AutoPlay )( 
            IVLCControl * This,
            /* [in] */ VARIANT_BOOL autoplay);
        
        /* [helpstring][propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_AutoLoop )( 
            IVLCControl * This,
            /* [retval][out] */ VARIANT_BOOL *autoloop);
        
        /* [helpstring][propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_AutoLoop )( 
            IVLCControl * This,
            /* [in] */ VARIANT_BOOL autoloop);
        
        END_INTERFACE
    } IVLCControlVtbl;

    interface IVLCControl
    {
        CONST_VTBL struct IVLCControlVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVLCControl_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVLCControl_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVLCControl_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVLCControl_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVLCControl_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVLCControl_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVLCControl_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVLCControl_get_Visible(This,visible)	\
    (This)->lpVtbl -> get_Visible(This,visible)

#define IVLCControl_put_Visible(This,visible)	\
    (This)->lpVtbl -> put_Visible(This,visible)

#define IVLCControl_play(This)	\
    (This)->lpVtbl -> play(This)

#define IVLCControl_pause(This)	\
    (This)->lpVtbl -> pause(This)

#define IVLCControl_stop(This)	\
    (This)->lpVtbl -> stop(This)

#define IVLCControl_get_Playing(This,isPlaying)	\
    (This)->lpVtbl -> get_Playing(This,isPlaying)

#define IVLCControl_get_Position(This,position)	\
    (This)->lpVtbl -> get_Position(This,position)

#define IVLCControl_put_Position(This,position)	\
    (This)->lpVtbl -> put_Position(This,position)

#define IVLCControl_get_Time(This,seconds)	\
    (This)->lpVtbl -> get_Time(This,seconds)

#define IVLCControl_put_Time(This,seconds)	\
    (This)->lpVtbl -> put_Time(This,seconds)

#define IVLCControl_shuttle(This,seconds)	\
    (This)->lpVtbl -> shuttle(This,seconds)

#define IVLCControl_fullscreen(This)	\
    (This)->lpVtbl -> fullscreen(This)

#define IVLCControl_get_Length(This,seconds)	\
    (This)->lpVtbl -> get_Length(This,seconds)

#define IVLCControl_playFaster(This)	\
    (This)->lpVtbl -> playFaster(This)

#define IVLCControl_playSlower(This)	\
    (This)->lpVtbl -> playSlower(This)

#define IVLCControl_get_Volume(This,volume)	\
    (This)->lpVtbl -> get_Volume(This,volume)

#define IVLCControl_put_Volume(This,volume)	\
    (This)->lpVtbl -> put_Volume(This,volume)

#define IVLCControl_toggleMute(This)	\
    (This)->lpVtbl -> toggleMute(This)

#define IVLCControl_setVariable(This,name,value)	\
    (This)->lpVtbl -> setVariable(This,name,value)

#define IVLCControl_getVariable(This,name,value)	\
    (This)->lpVtbl -> getVariable(This,name,value)

#define IVLCControl_addTarget(This,uri,options,mode,position)	\
    (This)->lpVtbl -> addTarget(This,uri,options,mode,position)

#define IVLCControl_get_PlaylistIndex(This,index)	\
    (This)->lpVtbl -> get_PlaylistIndex(This,index)

#define IVLCControl_get_PlaylistCount(This,index)	\
    (This)->lpVtbl -> get_PlaylistCount(This,index)

#define IVLCControl_playlistNext(This)	\
    (This)->lpVtbl -> playlistNext(This)

#define IVLCControl_playlistPrev(This)	\
    (This)->lpVtbl -> playlistPrev(This)

#define IVLCControl_playlistClear(This)	\
    (This)->lpVtbl -> playlistClear(This)

#define IVLCControl_get_VersionInfo(This,version)	\
    (This)->lpVtbl -> get_VersionInfo(This,version)

#define IVLCControl_get_MRL(This,mrl)	\
    (This)->lpVtbl -> get_MRL(This,mrl)

#define IVLCControl_put_MRL(This,mrl)	\
    (This)->lpVtbl -> put_MRL(This,mrl)

#define IVLCControl_get_AutoPlay(This,autoplay)	\
    (This)->lpVtbl -> get_AutoPlay(This,autoplay)

#define IVLCControl_put_AutoPlay(This,autoplay)	\
    (This)->lpVtbl -> put_AutoPlay(This,autoplay)

#define IVLCControl_get_AutoLoop(This,autoloop)	\
    (This)->lpVtbl -> get_AutoLoop(This,autoloop)

#define IVLCControl_put_AutoLoop(This,autoloop)	\
    (This)->lpVtbl -> put_AutoLoop(This,autoloop)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][bindable][propget][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_get_Visible_Proxy( 
    IVLCControl * This,
    /* [retval][out] */ VARIANT_BOOL *visible);


void __RPC_STUB IVLCControl_get_Visible_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][bindable][propput][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_put_Visible_Proxy( 
    IVLCControl * This,
    /* [in] */ VARIANT_BOOL visible);


void __RPC_STUB IVLCControl_put_Visible_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_play_Proxy( 
    IVLCControl * This);


void __RPC_STUB IVLCControl_play_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_pause_Proxy( 
    IVLCControl * This);


void __RPC_STUB IVLCControl_pause_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_stop_Proxy( 
    IVLCControl * This);


void __RPC_STUB IVLCControl_stop_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][hidden][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_get_Playing_Proxy( 
    IVLCControl * This,
    /* [retval][out] */ VARIANT_BOOL *isPlaying);


void __RPC_STUB IVLCControl_get_Playing_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_get_Position_Proxy( 
    IVLCControl * This,
    /* [retval][out] */ float *position);


void __RPC_STUB IVLCControl_get_Position_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_put_Position_Proxy( 
    IVLCControl * This,
    /* [in] */ float position);


void __RPC_STUB IVLCControl_put_Position_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_get_Time_Proxy( 
    IVLCControl * This,
    /* [retval][out] */ int *seconds);


void __RPC_STUB IVLCControl_get_Time_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_put_Time_Proxy( 
    IVLCControl * This,
    /* [in] */ int seconds);


void __RPC_STUB IVLCControl_put_Time_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_shuttle_Proxy( 
    IVLCControl * This,
    /* [in] */ int seconds);


void __RPC_STUB IVLCControl_shuttle_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_fullscreen_Proxy( 
    IVLCControl * This);


void __RPC_STUB IVLCControl_fullscreen_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][hidden][propget][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_get_Length_Proxy( 
    IVLCControl * This,
    /* [retval][out] */ int *seconds);


void __RPC_STUB IVLCControl_get_Length_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_playFaster_Proxy( 
    IVLCControl * This);


void __RPC_STUB IVLCControl_playFaster_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_playSlower_Proxy( 
    IVLCControl * This);


void __RPC_STUB IVLCControl_playSlower_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_get_Volume_Proxy( 
    IVLCControl * This,
    /* [retval][out] */ int *volume);


void __RPC_STUB IVLCControl_get_Volume_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_put_Volume_Proxy( 
    IVLCControl * This,
    /* [in] */ int volume);


void __RPC_STUB IVLCControl_put_Volume_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_toggleMute_Proxy( 
    IVLCControl * This);


void __RPC_STUB IVLCControl_toggleMute_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_setVariable_Proxy( 
    IVLCControl * This,
    /* [in] */ BSTR name,
    /* [in] */ VARIANT value);


void __RPC_STUB IVLCControl_setVariable_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_getVariable_Proxy( 
    IVLCControl * This,
    /* [in] */ BSTR name,
    /* [retval][out] */ VARIANT *value);


void __RPC_STUB IVLCControl_getVariable_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_addTarget_Proxy( 
    IVLCControl * This,
    /* [in] */ BSTR uri,
    /* [in] */ VARIANT options,
    /* [in] */ enum VLCPlaylistMode mode,
    /* [in] */ int position);


void __RPC_STUB IVLCControl_addTarget_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCControl_get_PlaylistIndex_Proxy( 
    IVLCControl * This,
    /* [retval][out] */ int *index);


void __RPC_STUB IVLCControl_get_PlaylistIndex_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCControl_get_PlaylistCount_Proxy( 
    IVLCControl * This,
    /* [retval][out] */ int *index);


void __RPC_STUB IVLCControl_get_PlaylistCount_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_playlistNext_Proxy( 
    IVLCControl * This);


void __RPC_STUB IVLCControl_playlistNext_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_playlistPrev_Proxy( 
    IVLCControl * This);


void __RPC_STUB IVLCControl_playlistPrev_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_playlistClear_Proxy( 
    IVLCControl * This);


void __RPC_STUB IVLCControl_playlistClear_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][hidden][propget] */ HRESULT STDMETHODCALLTYPE IVLCControl_get_VersionInfo_Proxy( 
    IVLCControl * This,
    /* [retval][out] */ BSTR *version);


void __RPC_STUB IVLCControl_get_VersionInfo_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_get_MRL_Proxy( 
    IVLCControl * This,
    /* [retval][out] */ BSTR *mrl);


void __RPC_STUB IVLCControl_get_MRL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_put_MRL_Proxy( 
    IVLCControl * This,
    /* [in] */ BSTR mrl);


void __RPC_STUB IVLCControl_put_MRL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_get_AutoPlay_Proxy( 
    IVLCControl * This,
    /* [retval][out] */ VARIANT_BOOL *autoplay);


void __RPC_STUB IVLCControl_get_AutoPlay_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_put_AutoPlay_Proxy( 
    IVLCControl * This,
    /* [in] */ VARIANT_BOOL autoplay);


void __RPC_STUB IVLCControl_put_AutoPlay_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_get_AutoLoop_Proxy( 
    IVLCControl * This,
    /* [retval][out] */ VARIANT_BOOL *autoloop);


void __RPC_STUB IVLCControl_get_AutoLoop_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_put_AutoLoop_Proxy( 
    IVLCControl * This,
    /* [in] */ VARIANT_BOOL autoloop);


void __RPC_STUB IVLCControl_put_AutoLoop_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVLCControl_INTERFACE_DEFINED__ */


#ifndef __IVLCAudio_INTERFACE_DEFINED__
#define __IVLCAudio_INTERFACE_DEFINED__

/* interface IVLCAudio */
/* [object][oleautomation][dual][helpstring][uuid] */ 


EXTERN_C const IID IID_IVLCAudio;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("9E0BD17B-2D3C-4656-B94D-03084F3FD9D4")
    IVLCAudio : public IDispatch
    {
    public:
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_mute( 
            /* [retval][out] */ VARIANT_BOOL *muted) = 0;
        
        virtual /* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE put_mute( 
            /* [in] */ VARIANT_BOOL muted) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_volume( 
            /* [retval][out] */ long *volume) = 0;
        
        virtual /* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE put_volume( 
            /* [in] */ long volume) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE toggleMute( void) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_track( 
            /* [retval][out] */ long *track) = 0;
        
        virtual /* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE put_track( 
            /* [in] */ long track) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_channel( 
            /* [retval][out] */ long *channel) = 0;
        
        virtual /* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE put_channel( 
            /* [in] */ long channel) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVLCAudioVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVLCAudio * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVLCAudio * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVLCAudio * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVLCAudio * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVLCAudio * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVLCAudio * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVLCAudio * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_mute )( 
            IVLCAudio * This,
            /* [retval][out] */ VARIANT_BOOL *muted);
        
        /* [helpstring][propput] */ HRESULT ( STDMETHODCALLTYPE *put_mute )( 
            IVLCAudio * This,
            /* [in] */ VARIANT_BOOL muted);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_volume )( 
            IVLCAudio * This,
            /* [retval][out] */ long *volume);
        
        /* [helpstring][propput] */ HRESULT ( STDMETHODCALLTYPE *put_volume )( 
            IVLCAudio * This,
            /* [in] */ long volume);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *toggleMute )( 
            IVLCAudio * This);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_track )( 
            IVLCAudio * This,
            /* [retval][out] */ long *track);
        
        /* [helpstring][propput] */ HRESULT ( STDMETHODCALLTYPE *put_track )( 
            IVLCAudio * This,
            /* [in] */ long track);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_channel )( 
            IVLCAudio * This,
            /* [retval][out] */ long *channel);
        
        /* [helpstring][propput] */ HRESULT ( STDMETHODCALLTYPE *put_channel )( 
            IVLCAudio * This,
            /* [in] */ long channel);
        
        END_INTERFACE
    } IVLCAudioVtbl;

    interface IVLCAudio
    {
        CONST_VTBL struct IVLCAudioVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVLCAudio_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVLCAudio_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVLCAudio_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVLCAudio_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVLCAudio_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVLCAudio_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVLCAudio_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVLCAudio_get_mute(This,muted)	\
    (This)->lpVtbl -> get_mute(This,muted)

#define IVLCAudio_put_mute(This,muted)	\
    (This)->lpVtbl -> put_mute(This,muted)

#define IVLCAudio_get_volume(This,volume)	\
    (This)->lpVtbl -> get_volume(This,volume)

#define IVLCAudio_put_volume(This,volume)	\
    (This)->lpVtbl -> put_volume(This,volume)

#define IVLCAudio_toggleMute(This)	\
    (This)->lpVtbl -> toggleMute(This)

#define IVLCAudio_get_track(This,track)	\
    (This)->lpVtbl -> get_track(This,track)

#define IVLCAudio_put_track(This,track)	\
    (This)->lpVtbl -> put_track(This,track)

#define IVLCAudio_get_channel(This,channel)	\
    (This)->lpVtbl -> get_channel(This,channel)

#define IVLCAudio_put_channel(This,channel)	\
    (This)->lpVtbl -> put_channel(This,channel)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCAudio_get_mute_Proxy( 
    IVLCAudio * This,
    /* [retval][out] */ VARIANT_BOOL *muted);


void __RPC_STUB IVLCAudio_get_mute_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE IVLCAudio_put_mute_Proxy( 
    IVLCAudio * This,
    /* [in] */ VARIANT_BOOL muted);


void __RPC_STUB IVLCAudio_put_mute_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCAudio_get_volume_Proxy( 
    IVLCAudio * This,
    /* [retval][out] */ long *volume);


void __RPC_STUB IVLCAudio_get_volume_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE IVLCAudio_put_volume_Proxy( 
    IVLCAudio * This,
    /* [in] */ long volume);


void __RPC_STUB IVLCAudio_put_volume_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCAudio_toggleMute_Proxy( 
    IVLCAudio * This);


void __RPC_STUB IVLCAudio_toggleMute_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCAudio_get_track_Proxy( 
    IVLCAudio * This,
    /* [retval][out] */ long *track);


void __RPC_STUB IVLCAudio_get_track_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE IVLCAudio_put_track_Proxy( 
    IVLCAudio * This,
    /* [in] */ long track);


void __RPC_STUB IVLCAudio_put_track_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCAudio_get_channel_Proxy( 
    IVLCAudio * This,
    /* [retval][out] */ long *channel);


void __RPC_STUB IVLCAudio_get_channel_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE IVLCAudio_put_channel_Proxy( 
    IVLCAudio * This,
    /* [in] */ long channel);


void __RPC_STUB IVLCAudio_put_channel_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVLCAudio_INTERFACE_DEFINED__ */


#ifndef __IVLCInput_INTERFACE_DEFINED__
#define __IVLCInput_INTERFACE_DEFINED__

/* interface IVLCInput */
/* [object][oleautomation][dual][helpstring][uuid] */ 


EXTERN_C const IID IID_IVLCInput;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("49E0DBD1-9440-466C-9C97-95C67190C603")
    IVLCInput : public IDispatch
    {
    public:
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_length( 
            /* [retval][out] */ double *length) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_position( 
            /* [retval][out] */ double *position) = 0;
        
        virtual /* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE put_position( 
            /* [in] */ double position) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_time( 
            /* [retval][out] */ double *time) = 0;
        
        virtual /* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE put_time( 
            /* [in] */ double time) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_state( 
            /* [retval][out] */ long *state) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_rate( 
            /* [retval][out] */ double *rate) = 0;
        
        virtual /* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE put_rate( 
            /* [in] */ double rate) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_fps( 
            /* [retval][out] */ double *fps) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_hasVout( 
            /* [retval][out] */ VARIANT_BOOL *hasVout) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVLCInputVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVLCInput * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVLCInput * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVLCInput * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVLCInput * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVLCInput * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVLCInput * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVLCInput * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_length )( 
            IVLCInput * This,
            /* [retval][out] */ double *length);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_position )( 
            IVLCInput * This,
            /* [retval][out] */ double *position);
        
        /* [helpstring][propput] */ HRESULT ( STDMETHODCALLTYPE *put_position )( 
            IVLCInput * This,
            /* [in] */ double position);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_time )( 
            IVLCInput * This,
            /* [retval][out] */ double *time);
        
        /* [helpstring][propput] */ HRESULT ( STDMETHODCALLTYPE *put_time )( 
            IVLCInput * This,
            /* [in] */ double time);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_state )( 
            IVLCInput * This,
            /* [retval][out] */ long *state);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_rate )( 
            IVLCInput * This,
            /* [retval][out] */ double *rate);
        
        /* [helpstring][propput] */ HRESULT ( STDMETHODCALLTYPE *put_rate )( 
            IVLCInput * This,
            /* [in] */ double rate);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_fps )( 
            IVLCInput * This,
            /* [retval][out] */ double *fps);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_hasVout )( 
            IVLCInput * This,
            /* [retval][out] */ VARIANT_BOOL *hasVout);
        
        END_INTERFACE
    } IVLCInputVtbl;

    interface IVLCInput
    {
        CONST_VTBL struct IVLCInputVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVLCInput_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVLCInput_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVLCInput_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVLCInput_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVLCInput_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVLCInput_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVLCInput_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVLCInput_get_length(This,length)	\
    (This)->lpVtbl -> get_length(This,length)

#define IVLCInput_get_position(This,position)	\
    (This)->lpVtbl -> get_position(This,position)

#define IVLCInput_put_position(This,position)	\
    (This)->lpVtbl -> put_position(This,position)

#define IVLCInput_get_time(This,time)	\
    (This)->lpVtbl -> get_time(This,time)

#define IVLCInput_put_time(This,time)	\
    (This)->lpVtbl -> put_time(This,time)

#define IVLCInput_get_state(This,state)	\
    (This)->lpVtbl -> get_state(This,state)

#define IVLCInput_get_rate(This,rate)	\
    (This)->lpVtbl -> get_rate(This,rate)

#define IVLCInput_put_rate(This,rate)	\
    (This)->lpVtbl -> put_rate(This,rate)

#define IVLCInput_get_fps(This,fps)	\
    (This)->lpVtbl -> get_fps(This,fps)

#define IVLCInput_get_hasVout(This,hasVout)	\
    (This)->lpVtbl -> get_hasVout(This,hasVout)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCInput_get_length_Proxy( 
    IVLCInput * This,
    /* [retval][out] */ double *length);


void __RPC_STUB IVLCInput_get_length_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCInput_get_position_Proxy( 
    IVLCInput * This,
    /* [retval][out] */ double *position);


void __RPC_STUB IVLCInput_get_position_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE IVLCInput_put_position_Proxy( 
    IVLCInput * This,
    /* [in] */ double position);


void __RPC_STUB IVLCInput_put_position_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCInput_get_time_Proxy( 
    IVLCInput * This,
    /* [retval][out] */ double *time);


void __RPC_STUB IVLCInput_get_time_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE IVLCInput_put_time_Proxy( 
    IVLCInput * This,
    /* [in] */ double time);


void __RPC_STUB IVLCInput_put_time_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCInput_get_state_Proxy( 
    IVLCInput * This,
    /* [retval][out] */ long *state);


void __RPC_STUB IVLCInput_get_state_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCInput_get_rate_Proxy( 
    IVLCInput * This,
    /* [retval][out] */ double *rate);


void __RPC_STUB IVLCInput_get_rate_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE IVLCInput_put_rate_Proxy( 
    IVLCInput * This,
    /* [in] */ double rate);


void __RPC_STUB IVLCInput_put_rate_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCInput_get_fps_Proxy( 
    IVLCInput * This,
    /* [retval][out] */ double *fps);


void __RPC_STUB IVLCInput_get_fps_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCInput_get_hasVout_Proxy( 
    IVLCInput * This,
    /* [retval][out] */ VARIANT_BOOL *hasVout);


void __RPC_STUB IVLCInput_get_hasVout_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVLCInput_INTERFACE_DEFINED__ */


#ifndef __IVLCLog_INTERFACE_DEFINED__
#define __IVLCLog_INTERFACE_DEFINED__

/* interface IVLCLog */
/* [object][oleautomation][dual][helpstring][uuid] */ 


EXTERN_C const IID IID_IVLCLog;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8E3BC3D9-62E9-48FB-8A6D-993F9ABC4A0A")
    IVLCLog : public IDispatch
    {
    public:
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_messages( 
            /* [retval][out] */ IVLCMessages **iter) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_verbosity( 
            /* [retval][out] */ long *level) = 0;
        
        virtual /* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE put_verbosity( 
            /* [in] */ long level) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVLCLogVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVLCLog * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVLCLog * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVLCLog * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVLCLog * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVLCLog * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVLCLog * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVLCLog * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_messages )( 
            IVLCLog * This,
            /* [retval][out] */ IVLCMessages **iter);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_verbosity )( 
            IVLCLog * This,
            /* [retval][out] */ long *level);
        
        /* [helpstring][propput] */ HRESULT ( STDMETHODCALLTYPE *put_verbosity )( 
            IVLCLog * This,
            /* [in] */ long level);
        
        END_INTERFACE
    } IVLCLogVtbl;

    interface IVLCLog
    {
        CONST_VTBL struct IVLCLogVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVLCLog_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVLCLog_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVLCLog_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVLCLog_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVLCLog_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVLCLog_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVLCLog_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVLCLog_get_messages(This,iter)	\
    (This)->lpVtbl -> get_messages(This,iter)

#define IVLCLog_get_verbosity(This,level)	\
    (This)->lpVtbl -> get_verbosity(This,level)

#define IVLCLog_put_verbosity(This,level)	\
    (This)->lpVtbl -> put_verbosity(This,level)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCLog_get_messages_Proxy( 
    IVLCLog * This,
    /* [retval][out] */ IVLCMessages **iter);


void __RPC_STUB IVLCLog_get_messages_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCLog_get_verbosity_Proxy( 
    IVLCLog * This,
    /* [retval][out] */ long *level);


void __RPC_STUB IVLCLog_get_verbosity_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE IVLCLog_put_verbosity_Proxy( 
    IVLCLog * This,
    /* [in] */ long level);


void __RPC_STUB IVLCLog_put_verbosity_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVLCLog_INTERFACE_DEFINED__ */


#ifndef __IVLCMessage_INTERFACE_DEFINED__
#define __IVLCMessage_INTERFACE_DEFINED__

/* interface IVLCMessage */
/* [object][oleautomation][dual][helpstring][uuid] */ 


EXTERN_C const IID IID_IVLCMessage;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("9ED00AFA-7BCD-4FFF-8D48-7DD4DB2C800D")
    IVLCMessage : public IDispatch
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get__Value( 
            /* [retval][out] */ VARIANT *message) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_severity( 
            /* [retval][out] */ long *level) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_type( 
            /* [retval][out] */ BSTR *type) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_name( 
            /* [retval][out] */ BSTR *name) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_header( 
            /* [retval][out] */ BSTR *header) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_message( 
            /* [retval][out] */ BSTR *message) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVLCMessageVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVLCMessage * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVLCMessage * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVLCMessage * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVLCMessage * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVLCMessage * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVLCMessage * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVLCMessage * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get__Value )( 
            IVLCMessage * This,
            /* [retval][out] */ VARIANT *message);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_severity )( 
            IVLCMessage * This,
            /* [retval][out] */ long *level);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_type )( 
            IVLCMessage * This,
            /* [retval][out] */ BSTR *type);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_name )( 
            IVLCMessage * This,
            /* [retval][out] */ BSTR *name);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_header )( 
            IVLCMessage * This,
            /* [retval][out] */ BSTR *header);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_message )( 
            IVLCMessage * This,
            /* [retval][out] */ BSTR *message);
        
        END_INTERFACE
    } IVLCMessageVtbl;

    interface IVLCMessage
    {
        CONST_VTBL struct IVLCMessageVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVLCMessage_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVLCMessage_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVLCMessage_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVLCMessage_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVLCMessage_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVLCMessage_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVLCMessage_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVLCMessage_get__Value(This,message)	\
    (This)->lpVtbl -> get__Value(This,message)

#define IVLCMessage_get_severity(This,level)	\
    (This)->lpVtbl -> get_severity(This,level)

#define IVLCMessage_get_type(This,type)	\
    (This)->lpVtbl -> get_type(This,type)

#define IVLCMessage_get_name(This,name)	\
    (This)->lpVtbl -> get_name(This,name)

#define IVLCMessage_get_header(This,header)	\
    (This)->lpVtbl -> get_header(This,header)

#define IVLCMessage_get_message(This,message)	\
    (This)->lpVtbl -> get_message(This,message)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [propget][id] */ HRESULT STDMETHODCALLTYPE IVLCMessage_get__Value_Proxy( 
    IVLCMessage * This,
    /* [retval][out] */ VARIANT *message);


void __RPC_STUB IVLCMessage_get__Value_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCMessage_get_severity_Proxy( 
    IVLCMessage * This,
    /* [retval][out] */ long *level);


void __RPC_STUB IVLCMessage_get_severity_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCMessage_get_type_Proxy( 
    IVLCMessage * This,
    /* [retval][out] */ BSTR *type);


void __RPC_STUB IVLCMessage_get_type_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCMessage_get_name_Proxy( 
    IVLCMessage * This,
    /* [retval][out] */ BSTR *name);


void __RPC_STUB IVLCMessage_get_name_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCMessage_get_header_Proxy( 
    IVLCMessage * This,
    /* [retval][out] */ BSTR *header);


void __RPC_STUB IVLCMessage_get_header_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCMessage_get_message_Proxy( 
    IVLCMessage * This,
    /* [retval][out] */ BSTR *message);


void __RPC_STUB IVLCMessage_get_message_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVLCMessage_INTERFACE_DEFINED__ */


#ifndef __IVLCMessageIterator_INTERFACE_DEFINED__
#define __IVLCMessageIterator_INTERFACE_DEFINED__

/* interface IVLCMessageIterator */
/* [object][oleautomation][dual][helpstring][uuid] */ 


EXTERN_C const IID IID_IVLCMessageIterator;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("15179CD8-CC12-4242-A58E-E412217FF343")
    IVLCMessageIterator : public IDispatch
    {
    public:
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_hasNext( 
            /* [retval][out] */ VARIANT_BOOL *hasNext) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE next( 
            /* [retval][out] */ IVLCMessage **msg) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVLCMessageIteratorVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVLCMessageIterator * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVLCMessageIterator * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVLCMessageIterator * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVLCMessageIterator * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVLCMessageIterator * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVLCMessageIterator * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVLCMessageIterator * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_hasNext )( 
            IVLCMessageIterator * This,
            /* [retval][out] */ VARIANT_BOOL *hasNext);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *next )( 
            IVLCMessageIterator * This,
            /* [retval][out] */ IVLCMessage **msg);
        
        END_INTERFACE
    } IVLCMessageIteratorVtbl;

    interface IVLCMessageIterator
    {
        CONST_VTBL struct IVLCMessageIteratorVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVLCMessageIterator_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVLCMessageIterator_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVLCMessageIterator_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVLCMessageIterator_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVLCMessageIterator_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVLCMessageIterator_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVLCMessageIterator_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVLCMessageIterator_get_hasNext(This,hasNext)	\
    (This)->lpVtbl -> get_hasNext(This,hasNext)

#define IVLCMessageIterator_next(This,msg)	\
    (This)->lpVtbl -> next(This,msg)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCMessageIterator_get_hasNext_Proxy( 
    IVLCMessageIterator * This,
    /* [retval][out] */ VARIANT_BOOL *hasNext);


void __RPC_STUB IVLCMessageIterator_get_hasNext_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCMessageIterator_next_Proxy( 
    IVLCMessageIterator * This,
    /* [retval][out] */ IVLCMessage **msg);


void __RPC_STUB IVLCMessageIterator_next_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVLCMessageIterator_INTERFACE_DEFINED__ */


#ifndef __IVLCMessages_INTERFACE_DEFINED__
#define __IVLCMessages_INTERFACE_DEFINED__

/* interface IVLCMessages */
/* [object][oleautomation][dual][helpstring][uuid] */ 


EXTERN_C const IID IID_IVLCMessages;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("6C5CE55D-2D6C-4AAD-8299-C62D2371F106")
    IVLCMessages : public IDispatch
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get__NewEnum( 
            /* [retval][out] */ IUnknown **_NewEnum) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE clear( void) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_count( 
            /* [retval][out] */ long *count) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE iterator( 
            /* [retval][out] */ IVLCMessageIterator **iter) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVLCMessagesVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVLCMessages * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVLCMessages * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVLCMessages * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVLCMessages * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVLCMessages * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVLCMessages * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVLCMessages * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get__NewEnum )( 
            IVLCMessages * This,
            /* [retval][out] */ IUnknown **_NewEnum);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *clear )( 
            IVLCMessages * This);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_count )( 
            IVLCMessages * This,
            /* [retval][out] */ long *count);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *iterator )( 
            IVLCMessages * This,
            /* [retval][out] */ IVLCMessageIterator **iter);
        
        END_INTERFACE
    } IVLCMessagesVtbl;

    interface IVLCMessages
    {
        CONST_VTBL struct IVLCMessagesVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVLCMessages_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVLCMessages_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVLCMessages_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVLCMessages_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVLCMessages_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVLCMessages_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVLCMessages_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVLCMessages_get__NewEnum(This,_NewEnum)	\
    (This)->lpVtbl -> get__NewEnum(This,_NewEnum)

#define IVLCMessages_clear(This)	\
    (This)->lpVtbl -> clear(This)

#define IVLCMessages_get_count(This,count)	\
    (This)->lpVtbl -> get_count(This,count)

#define IVLCMessages_iterator(This,iter)	\
    (This)->lpVtbl -> iterator(This,iter)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [propget][id] */ HRESULT STDMETHODCALLTYPE IVLCMessages_get__NewEnum_Proxy( 
    IVLCMessages * This,
    /* [retval][out] */ IUnknown **_NewEnum);


void __RPC_STUB IVLCMessages_get__NewEnum_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCMessages_clear_Proxy( 
    IVLCMessages * This);


void __RPC_STUB IVLCMessages_clear_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCMessages_get_count_Proxy( 
    IVLCMessages * This,
    /* [retval][out] */ long *count);


void __RPC_STUB IVLCMessages_get_count_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCMessages_iterator_Proxy( 
    IVLCMessages * This,
    /* [retval][out] */ IVLCMessageIterator **iter);


void __RPC_STUB IVLCMessages_iterator_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVLCMessages_INTERFACE_DEFINED__ */


#ifndef __IVLCPlaylist_INTERFACE_DEFINED__
#define __IVLCPlaylist_INTERFACE_DEFINED__

/* interface IVLCPlaylist */
/* [object][oleautomation][dual][helpstring][uuid] */ 


EXTERN_C const IID IID_IVLCPlaylist;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("54613049-40BF-4035-9E70-0A9312C0188D")
    IVLCPlaylist : public IDispatch
    {
    public:
        virtual /* [helpstring][propget][hidden] */ HRESULT STDMETHODCALLTYPE get_itemCount( 
            /* [retval][out] */ long *count) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_isPlaying( 
            /* [retval][out] */ VARIANT_BOOL *playing) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE add( 
            /* [in] */ BSTR uri,
            /* [optional][in] */ VARIANT name,
            /* [optional][in] */ VARIANT options,
            /* [retval][out] */ long *itemId) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE play( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE playItem( 
            /* [in] */ long itemId) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE togglePause( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE stop( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE next( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE prev( void) = 0;
        
        virtual /* [helpstring][hidden] */ HRESULT STDMETHODCALLTYPE clear( void) = 0;
        
        virtual /* [helpstring][hidden] */ HRESULT STDMETHODCALLTYPE removeItem( 
            /* [in] */ long item) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_items( 
            /* [retval][out] */ IVLCPlaylistItems **obj) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVLCPlaylistVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVLCPlaylist * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVLCPlaylist * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVLCPlaylist * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVLCPlaylist * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVLCPlaylist * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVLCPlaylist * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVLCPlaylist * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][propget][hidden] */ HRESULT ( STDMETHODCALLTYPE *get_itemCount )( 
            IVLCPlaylist * This,
            /* [retval][out] */ long *count);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_isPlaying )( 
            IVLCPlaylist * This,
            /* [retval][out] */ VARIANT_BOOL *playing);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *add )( 
            IVLCPlaylist * This,
            /* [in] */ BSTR uri,
            /* [optional][in] */ VARIANT name,
            /* [optional][in] */ VARIANT options,
            /* [retval][out] */ long *itemId);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *play )( 
            IVLCPlaylist * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *playItem )( 
            IVLCPlaylist * This,
            /* [in] */ long itemId);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *togglePause )( 
            IVLCPlaylist * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *stop )( 
            IVLCPlaylist * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *next )( 
            IVLCPlaylist * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *prev )( 
            IVLCPlaylist * This);
        
        /* [helpstring][hidden] */ HRESULT ( STDMETHODCALLTYPE *clear )( 
            IVLCPlaylist * This);
        
        /* [helpstring][hidden] */ HRESULT ( STDMETHODCALLTYPE *removeItem )( 
            IVLCPlaylist * This,
            /* [in] */ long item);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_items )( 
            IVLCPlaylist * This,
            /* [retval][out] */ IVLCPlaylistItems **obj);
        
        END_INTERFACE
    } IVLCPlaylistVtbl;

    interface IVLCPlaylist
    {
        CONST_VTBL struct IVLCPlaylistVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVLCPlaylist_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVLCPlaylist_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVLCPlaylist_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVLCPlaylist_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVLCPlaylist_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVLCPlaylist_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVLCPlaylist_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVLCPlaylist_get_itemCount(This,count)	\
    (This)->lpVtbl -> get_itemCount(This,count)

#define IVLCPlaylist_get_isPlaying(This,playing)	\
    (This)->lpVtbl -> get_isPlaying(This,playing)

#define IVLCPlaylist_add(This,uri,name,options,itemId)	\
    (This)->lpVtbl -> add(This,uri,name,options,itemId)

#define IVLCPlaylist_play(This)	\
    (This)->lpVtbl -> play(This)

#define IVLCPlaylist_playItem(This,itemId)	\
    (This)->lpVtbl -> playItem(This,itemId)

#define IVLCPlaylist_togglePause(This)	\
    (This)->lpVtbl -> togglePause(This)

#define IVLCPlaylist_stop(This)	\
    (This)->lpVtbl -> stop(This)

#define IVLCPlaylist_next(This)	\
    (This)->lpVtbl -> next(This)

#define IVLCPlaylist_prev(This)	\
    (This)->lpVtbl -> prev(This)

#define IVLCPlaylist_clear(This)	\
    (This)->lpVtbl -> clear(This)

#define IVLCPlaylist_removeItem(This,item)	\
    (This)->lpVtbl -> removeItem(This,item)

#define IVLCPlaylist_get_items(This,obj)	\
    (This)->lpVtbl -> get_items(This,obj)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][propget][hidden] */ HRESULT STDMETHODCALLTYPE IVLCPlaylist_get_itemCount_Proxy( 
    IVLCPlaylist * This,
    /* [retval][out] */ long *count);


void __RPC_STUB IVLCPlaylist_get_itemCount_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCPlaylist_get_isPlaying_Proxy( 
    IVLCPlaylist * This,
    /* [retval][out] */ VARIANT_BOOL *playing);


void __RPC_STUB IVLCPlaylist_get_isPlaying_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCPlaylist_add_Proxy( 
    IVLCPlaylist * This,
    /* [in] */ BSTR uri,
    /* [optional][in] */ VARIANT name,
    /* [optional][in] */ VARIANT options,
    /* [retval][out] */ long *itemId);


void __RPC_STUB IVLCPlaylist_add_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCPlaylist_play_Proxy( 
    IVLCPlaylist * This);


void __RPC_STUB IVLCPlaylist_play_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCPlaylist_playItem_Proxy( 
    IVLCPlaylist * This,
    /* [in] */ long itemId);


void __RPC_STUB IVLCPlaylist_playItem_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCPlaylist_togglePause_Proxy( 
    IVLCPlaylist * This);


void __RPC_STUB IVLCPlaylist_togglePause_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCPlaylist_stop_Proxy( 
    IVLCPlaylist * This);


void __RPC_STUB IVLCPlaylist_stop_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCPlaylist_next_Proxy( 
    IVLCPlaylist * This);


void __RPC_STUB IVLCPlaylist_next_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCPlaylist_prev_Proxy( 
    IVLCPlaylist * This);


void __RPC_STUB IVLCPlaylist_prev_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][hidden] */ HRESULT STDMETHODCALLTYPE IVLCPlaylist_clear_Proxy( 
    IVLCPlaylist * This);


void __RPC_STUB IVLCPlaylist_clear_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][hidden] */ HRESULT STDMETHODCALLTYPE IVLCPlaylist_removeItem_Proxy( 
    IVLCPlaylist * This,
    /* [in] */ long item);


void __RPC_STUB IVLCPlaylist_removeItem_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCPlaylist_get_items_Proxy( 
    IVLCPlaylist * This,
    /* [retval][out] */ IVLCPlaylistItems **obj);


void __RPC_STUB IVLCPlaylist_get_items_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVLCPlaylist_INTERFACE_DEFINED__ */


#ifndef __IVLCVideo_INTERFACE_DEFINED__
#define __IVLCVideo_INTERFACE_DEFINED__

/* interface IVLCVideo */
/* [object][oleautomation][dual][helpstring][uuid] */ 


EXTERN_C const IID IID_IVLCVideo;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("0AAEDF0B-D333-4B27-A0C6-BBF31413A42E")
    IVLCVideo : public IDispatch
    {
    public:
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_fullscreen( 
            /* [retval][out] */ VARIANT_BOOL *fullscreen) = 0;
        
        virtual /* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE put_fullscreen( 
            /* [in] */ VARIANT_BOOL fullscreen) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_width( 
            /* [retval][out] */ long *width) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_height( 
            /* [retval][out] */ long *height) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_aspectRatio( 
            /* [retval][out] */ BSTR *aspect) = 0;
        
        virtual /* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE put_aspectRatio( 
            /* [in] */ BSTR aspect) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_subtitle( 
            /* [retval][out] */ long *spu) = 0;
        
        virtual /* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE put_subtitle( 
            /* [in] */ long spu) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_crop( 
            /* [retval][out] */ BSTR *geometry) = 0;
        
        virtual /* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE put_crop( 
            /* [in] */ BSTR geometry) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE toggleFullscreen( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE takeSnapshot( 
            /* [retval][out] */ IPictureDisp **picture) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVLCVideoVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVLCVideo * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVLCVideo * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVLCVideo * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVLCVideo * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVLCVideo * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVLCVideo * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVLCVideo * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_fullscreen )( 
            IVLCVideo * This,
            /* [retval][out] */ VARIANT_BOOL *fullscreen);
        
        /* [helpstring][propput] */ HRESULT ( STDMETHODCALLTYPE *put_fullscreen )( 
            IVLCVideo * This,
            /* [in] */ VARIANT_BOOL fullscreen);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_width )( 
            IVLCVideo * This,
            /* [retval][out] */ long *width);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_height )( 
            IVLCVideo * This,
            /* [retval][out] */ long *height);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_aspectRatio )( 
            IVLCVideo * This,
            /* [retval][out] */ BSTR *aspect);
        
        /* [helpstring][propput] */ HRESULT ( STDMETHODCALLTYPE *put_aspectRatio )( 
            IVLCVideo * This,
            /* [in] */ BSTR aspect);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_subtitle )( 
            IVLCVideo * This,
            /* [retval][out] */ long *spu);
        
        /* [helpstring][propput] */ HRESULT ( STDMETHODCALLTYPE *put_subtitle )( 
            IVLCVideo * This,
            /* [in] */ long spu);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_crop )( 
            IVLCVideo * This,
            /* [retval][out] */ BSTR *geometry);
        
        /* [helpstring][propput] */ HRESULT ( STDMETHODCALLTYPE *put_crop )( 
            IVLCVideo * This,
            /* [in] */ BSTR geometry);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *toggleFullscreen )( 
            IVLCVideo * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *takeSnapshot )( 
            IVLCVideo * This,
            /* [retval][out] */ IPictureDisp **picture);
        
        END_INTERFACE
    } IVLCVideoVtbl;

    interface IVLCVideo
    {
        CONST_VTBL struct IVLCVideoVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVLCVideo_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVLCVideo_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVLCVideo_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVLCVideo_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVLCVideo_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVLCVideo_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVLCVideo_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVLCVideo_get_fullscreen(This,fullscreen)	\
    (This)->lpVtbl -> get_fullscreen(This,fullscreen)

#define IVLCVideo_put_fullscreen(This,fullscreen)	\
    (This)->lpVtbl -> put_fullscreen(This,fullscreen)

#define IVLCVideo_get_width(This,width)	\
    (This)->lpVtbl -> get_width(This,width)

#define IVLCVideo_get_height(This,height)	\
    (This)->lpVtbl -> get_height(This,height)

#define IVLCVideo_get_aspectRatio(This,aspect)	\
    (This)->lpVtbl -> get_aspectRatio(This,aspect)

#define IVLCVideo_put_aspectRatio(This,aspect)	\
    (This)->lpVtbl -> put_aspectRatio(This,aspect)

#define IVLCVideo_get_subtitle(This,spu)	\
    (This)->lpVtbl -> get_subtitle(This,spu)

#define IVLCVideo_put_subtitle(This,spu)	\
    (This)->lpVtbl -> put_subtitle(This,spu)

#define IVLCVideo_get_crop(This,geometry)	\
    (This)->lpVtbl -> get_crop(This,geometry)

#define IVLCVideo_put_crop(This,geometry)	\
    (This)->lpVtbl -> put_crop(This,geometry)

#define IVLCVideo_toggleFullscreen(This)	\
    (This)->lpVtbl -> toggleFullscreen(This)

#define IVLCVideo_takeSnapshot(This,picture)	\
    (This)->lpVtbl -> takeSnapshot(This,picture)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCVideo_get_fullscreen_Proxy( 
    IVLCVideo * This,
    /* [retval][out] */ VARIANT_BOOL *fullscreen);


void __RPC_STUB IVLCVideo_get_fullscreen_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE IVLCVideo_put_fullscreen_Proxy( 
    IVLCVideo * This,
    /* [in] */ VARIANT_BOOL fullscreen);


void __RPC_STUB IVLCVideo_put_fullscreen_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCVideo_get_width_Proxy( 
    IVLCVideo * This,
    /* [retval][out] */ long *width);


void __RPC_STUB IVLCVideo_get_width_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCVideo_get_height_Proxy( 
    IVLCVideo * This,
    /* [retval][out] */ long *height);


void __RPC_STUB IVLCVideo_get_height_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCVideo_get_aspectRatio_Proxy( 
    IVLCVideo * This,
    /* [retval][out] */ BSTR *aspect);


void __RPC_STUB IVLCVideo_get_aspectRatio_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE IVLCVideo_put_aspectRatio_Proxy( 
    IVLCVideo * This,
    /* [in] */ BSTR aspect);


void __RPC_STUB IVLCVideo_put_aspectRatio_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCVideo_get_subtitle_Proxy( 
    IVLCVideo * This,
    /* [retval][out] */ long *spu);


void __RPC_STUB IVLCVideo_get_subtitle_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE IVLCVideo_put_subtitle_Proxy( 
    IVLCVideo * This,
    /* [in] */ long spu);


void __RPC_STUB IVLCVideo_put_subtitle_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCVideo_get_crop_Proxy( 
    IVLCVideo * This,
    /* [retval][out] */ BSTR *geometry);


void __RPC_STUB IVLCVideo_get_crop_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput] */ HRESULT STDMETHODCALLTYPE IVLCVideo_put_crop_Proxy( 
    IVLCVideo * This,
    /* [in] */ BSTR geometry);


void __RPC_STUB IVLCVideo_put_crop_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCVideo_toggleFullscreen_Proxy( 
    IVLCVideo * This);


void __RPC_STUB IVLCVideo_toggleFullscreen_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCVideo_takeSnapshot_Proxy( 
    IVLCVideo * This,
    /* [retval][out] */ IPictureDisp **picture);


void __RPC_STUB IVLCVideo_takeSnapshot_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVLCVideo_INTERFACE_DEFINED__ */


#ifndef __IVLCControl2_INTERFACE_DEFINED__
#define __IVLCControl2_INTERFACE_DEFINED__

/* interface IVLCControl2 */
/* [object][oleautomation][dual][helpstring][uuid] */ 


EXTERN_C const IID IID_IVLCControl2;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2D719729-5333-406C-BF12-8DE787FD65E3")
    IVLCControl2 : public IDispatch
    {
    public:
        virtual /* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_AutoLoop( 
            /* [retval][out] */ VARIANT_BOOL *autoloop) = 0;
        
        virtual /* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_AutoLoop( 
            /* [in] */ VARIANT_BOOL autoloop) = 0;
        
        virtual /* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_AutoPlay( 
            /* [retval][out] */ VARIANT_BOOL *autoplay) = 0;
        
        virtual /* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_AutoPlay( 
            /* [in] */ VARIANT_BOOL autoplay) = 0;
        
        virtual /* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_BaseURL( 
            /* [retval][out] */ BSTR *url) = 0;
        
        virtual /* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_BaseURL( 
            /* [in] */ BSTR url) = 0;
        
        virtual /* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_StartTime( 
            /* [retval][out] */ long *seconds) = 0;
        
        virtual /* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_StartTime( 
            /* [in] */ long seconds) = 0;
        
        virtual /* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_MRL( 
            /* [retval][out] */ BSTR *mrl) = 0;
        
        virtual /* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_MRL( 
            /* [in] */ BSTR mrl) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_VersionInfo( 
            /* [retval][out] */ BSTR *version) = 0;
        
        virtual /* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Visible( 
            /* [retval][out] */ VARIANT_BOOL *visible) = 0;
        
        virtual /* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_Visible( 
            /* [in] */ VARIANT_BOOL visible) = 0;
        
        virtual /* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Volume( 
            /* [retval][out] */ long *volume) = 0;
        
        virtual /* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_Volume( 
            /* [in] */ long volume) = 0;
        
        virtual /* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_BackColor( 
            /* [retval][out] */ OLE_COLOR *backcolor) = 0;
        
        virtual /* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_BackColor( 
            /* [in] */ OLE_COLOR backcolor) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_audio( 
            /* [retval][out] */ IVLCAudio **obj) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_input( 
            /* [retval][out] */ IVLCInput **obj) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_log( 
            /* [retval][out] */ IVLCLog **obj) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_playlist( 
            /* [retval][out] */ IVLCPlaylist **obj) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_video( 
            /* [retval][out] */ IVLCVideo **obj) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVLCControl2Vtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVLCControl2 * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVLCControl2 * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVLCControl2 * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVLCControl2 * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVLCControl2 * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVLCControl2 * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVLCControl2 * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_AutoLoop )( 
            IVLCControl2 * This,
            /* [retval][out] */ VARIANT_BOOL *autoloop);
        
        /* [helpstring][propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_AutoLoop )( 
            IVLCControl2 * This,
            /* [in] */ VARIANT_BOOL autoloop);
        
        /* [helpstring][propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_AutoPlay )( 
            IVLCControl2 * This,
            /* [retval][out] */ VARIANT_BOOL *autoplay);
        
        /* [helpstring][propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_AutoPlay )( 
            IVLCControl2 * This,
            /* [in] */ VARIANT_BOOL autoplay);
        
        /* [helpstring][propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_BaseURL )( 
            IVLCControl2 * This,
            /* [retval][out] */ BSTR *url);
        
        /* [helpstring][propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_BaseURL )( 
            IVLCControl2 * This,
            /* [in] */ BSTR url);
        
        /* [helpstring][propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_StartTime )( 
            IVLCControl2 * This,
            /* [retval][out] */ long *seconds);
        
        /* [helpstring][propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_StartTime )( 
            IVLCControl2 * This,
            /* [in] */ long seconds);
        
        /* [helpstring][propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_MRL )( 
            IVLCControl2 * This,
            /* [retval][out] */ BSTR *mrl);
        
        /* [helpstring][propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_MRL )( 
            IVLCControl2 * This,
            /* [in] */ BSTR mrl);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_VersionInfo )( 
            IVLCControl2 * This,
            /* [retval][out] */ BSTR *version);
        
        /* [helpstring][propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Visible )( 
            IVLCControl2 * This,
            /* [retval][out] */ VARIANT_BOOL *visible);
        
        /* [helpstring][propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_Visible )( 
            IVLCControl2 * This,
            /* [in] */ VARIANT_BOOL visible);
        
        /* [helpstring][propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Volume )( 
            IVLCControl2 * This,
            /* [retval][out] */ long *volume);
        
        /* [helpstring][propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_Volume )( 
            IVLCControl2 * This,
            /* [in] */ long volume);
        
        /* [helpstring][propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_BackColor )( 
            IVLCControl2 * This,
            /* [retval][out] */ OLE_COLOR *backcolor);
        
        /* [helpstring][propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_BackColor )( 
            IVLCControl2 * This,
            /* [in] */ OLE_COLOR backcolor);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_audio )( 
            IVLCControl2 * This,
            /* [retval][out] */ IVLCAudio **obj);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_input )( 
            IVLCControl2 * This,
            /* [retval][out] */ IVLCInput **obj);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_log )( 
            IVLCControl2 * This,
            /* [retval][out] */ IVLCLog **obj);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_playlist )( 
            IVLCControl2 * This,
            /* [retval][out] */ IVLCPlaylist **obj);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_video )( 
            IVLCControl2 * This,
            /* [retval][out] */ IVLCVideo **obj);
        
        END_INTERFACE
    } IVLCControl2Vtbl;

    interface IVLCControl2
    {
        CONST_VTBL struct IVLCControl2Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVLCControl2_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVLCControl2_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVLCControl2_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVLCControl2_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVLCControl2_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVLCControl2_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVLCControl2_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVLCControl2_get_AutoLoop(This,autoloop)	\
    (This)->lpVtbl -> get_AutoLoop(This,autoloop)

#define IVLCControl2_put_AutoLoop(This,autoloop)	\
    (This)->lpVtbl -> put_AutoLoop(This,autoloop)

#define IVLCControl2_get_AutoPlay(This,autoplay)	\
    (This)->lpVtbl -> get_AutoPlay(This,autoplay)

#define IVLCControl2_put_AutoPlay(This,autoplay)	\
    (This)->lpVtbl -> put_AutoPlay(This,autoplay)

#define IVLCControl2_get_BaseURL(This,url)	\
    (This)->lpVtbl -> get_BaseURL(This,url)

#define IVLCControl2_put_BaseURL(This,url)	\
    (This)->lpVtbl -> put_BaseURL(This,url)

#define IVLCControl2_get_StartTime(This,seconds)	\
    (This)->lpVtbl -> get_StartTime(This,seconds)

#define IVLCControl2_put_StartTime(This,seconds)	\
    (This)->lpVtbl -> put_StartTime(This,seconds)

#define IVLCControl2_get_MRL(This,mrl)	\
    (This)->lpVtbl -> get_MRL(This,mrl)

#define IVLCControl2_put_MRL(This,mrl)	\
    (This)->lpVtbl -> put_MRL(This,mrl)

#define IVLCControl2_get_VersionInfo(This,version)	\
    (This)->lpVtbl -> get_VersionInfo(This,version)

#define IVLCControl2_get_Visible(This,visible)	\
    (This)->lpVtbl -> get_Visible(This,visible)

#define IVLCControl2_put_Visible(This,visible)	\
    (This)->lpVtbl -> put_Visible(This,visible)

#define IVLCControl2_get_Volume(This,volume)	\
    (This)->lpVtbl -> get_Volume(This,volume)

#define IVLCControl2_put_Volume(This,volume)	\
    (This)->lpVtbl -> put_Volume(This,volume)

#define IVLCControl2_get_BackColor(This,backcolor)	\
    (This)->lpVtbl -> get_BackColor(This,backcolor)

#define IVLCControl2_put_BackColor(This,backcolor)	\
    (This)->lpVtbl -> put_BackColor(This,backcolor)

#define IVLCControl2_get_audio(This,obj)	\
    (This)->lpVtbl -> get_audio(This,obj)

#define IVLCControl2_get_input(This,obj)	\
    (This)->lpVtbl -> get_input(This,obj)

#define IVLCControl2_get_log(This,obj)	\
    (This)->lpVtbl -> get_log(This,obj)

#define IVLCControl2_get_playlist(This,obj)	\
    (This)->lpVtbl -> get_playlist(This,obj)

#define IVLCControl2_get_video(This,obj)	\
    (This)->lpVtbl -> get_video(This,obj)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE IVLCControl2_get_AutoLoop_Proxy( 
    IVLCControl2 * This,
    /* [retval][out] */ VARIANT_BOOL *autoloop);


void __RPC_STUB IVLCControl2_get_AutoLoop_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE IVLCControl2_put_AutoLoop_Proxy( 
    IVLCControl2 * This,
    /* [in] */ VARIANT_BOOL autoloop);


void __RPC_STUB IVLCControl2_put_AutoLoop_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE IVLCControl2_get_AutoPlay_Proxy( 
    IVLCControl2 * This,
    /* [retval][out] */ VARIANT_BOOL *autoplay);


void __RPC_STUB IVLCControl2_get_AutoPlay_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE IVLCControl2_put_AutoPlay_Proxy( 
    IVLCControl2 * This,
    /* [in] */ VARIANT_BOOL autoplay);


void __RPC_STUB IVLCControl2_put_AutoPlay_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE IVLCControl2_get_BaseURL_Proxy( 
    IVLCControl2 * This,
    /* [retval][out] */ BSTR *url);


void __RPC_STUB IVLCControl2_get_BaseURL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE IVLCControl2_put_BaseURL_Proxy( 
    IVLCControl2 * This,
    /* [in] */ BSTR url);


void __RPC_STUB IVLCControl2_put_BaseURL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE IVLCControl2_get_StartTime_Proxy( 
    IVLCControl2 * This,
    /* [retval][out] */ long *seconds);


void __RPC_STUB IVLCControl2_get_StartTime_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE IVLCControl2_put_StartTime_Proxy( 
    IVLCControl2 * This,
    /* [in] */ long seconds);


void __RPC_STUB IVLCControl2_put_StartTime_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE IVLCControl2_get_MRL_Proxy( 
    IVLCControl2 * This,
    /* [retval][out] */ BSTR *mrl);


void __RPC_STUB IVLCControl2_get_MRL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE IVLCControl2_put_MRL_Proxy( 
    IVLCControl2 * This,
    /* [in] */ BSTR mrl);


void __RPC_STUB IVLCControl2_put_MRL_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCControl2_get_VersionInfo_Proxy( 
    IVLCControl2 * This,
    /* [retval][out] */ BSTR *version);


void __RPC_STUB IVLCControl2_get_VersionInfo_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE IVLCControl2_get_Visible_Proxy( 
    IVLCControl2 * This,
    /* [retval][out] */ VARIANT_BOOL *visible);


void __RPC_STUB IVLCControl2_get_Visible_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE IVLCControl2_put_Visible_Proxy( 
    IVLCControl2 * This,
    /* [in] */ VARIANT_BOOL visible);


void __RPC_STUB IVLCControl2_put_Visible_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE IVLCControl2_get_Volume_Proxy( 
    IVLCControl2 * This,
    /* [retval][out] */ long *volume);


void __RPC_STUB IVLCControl2_get_Volume_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE IVLCControl2_put_Volume_Proxy( 
    IVLCControl2 * This,
    /* [in] */ long volume);


void __RPC_STUB IVLCControl2_put_Volume_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE IVLCControl2_get_BackColor_Proxy( 
    IVLCControl2 * This,
    /* [retval][out] */ OLE_COLOR *backcolor);


void __RPC_STUB IVLCControl2_get_BackColor_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE IVLCControl2_put_BackColor_Proxy( 
    IVLCControl2 * This,
    /* [in] */ OLE_COLOR backcolor);


void __RPC_STUB IVLCControl2_put_BackColor_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCControl2_get_audio_Proxy( 
    IVLCControl2 * This,
    /* [retval][out] */ IVLCAudio **obj);


void __RPC_STUB IVLCControl2_get_audio_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCControl2_get_input_Proxy( 
    IVLCControl2 * This,
    /* [retval][out] */ IVLCInput **obj);


void __RPC_STUB IVLCControl2_get_input_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCControl2_get_log_Proxy( 
    IVLCControl2 * This,
    /* [retval][out] */ IVLCLog **obj);


void __RPC_STUB IVLCControl2_get_log_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCControl2_get_playlist_Proxy( 
    IVLCControl2 * This,
    /* [retval][out] */ IVLCPlaylist **obj);


void __RPC_STUB IVLCControl2_get_playlist_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCControl2_get_video_Proxy( 
    IVLCControl2 * This,
    /* [retval][out] */ IVLCVideo **obj);


void __RPC_STUB IVLCControl2_get_video_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVLCControl2_INTERFACE_DEFINED__ */


#ifndef __DVLCEvents_DISPINTERFACE_DEFINED__
#define __DVLCEvents_DISPINTERFACE_DEFINED__

/* dispinterface DVLCEvents */
/* [helpstring][uuid] */ 


EXTERN_C const IID DIID_DVLCEvents;

#if defined(__cplusplus) && !defined(CINTERFACE)

    MIDL_INTERFACE("DF48072F-5EF8-434e-9B40-E2F3AE759B5F")
    DVLCEvents : public IDispatch
    {
    };
    
#else 	/* C style interface */

    typedef struct DVLCEventsVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            DVLCEvents * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            DVLCEvents * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            DVLCEvents * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            DVLCEvents * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            DVLCEvents * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            DVLCEvents * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            DVLCEvents * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        END_INTERFACE
    } DVLCEventsVtbl;

    interface DVLCEvents
    {
        CONST_VTBL struct DVLCEventsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define DVLCEvents_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define DVLCEvents_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define DVLCEvents_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define DVLCEvents_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define DVLCEvents_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define DVLCEvents_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define DVLCEvents_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)

#endif /* COBJMACROS */


#endif 	/* C style interface */


#endif 	/* __DVLCEvents_DISPINTERFACE_DEFINED__ */


#ifndef __IVLCPlaylistItems_INTERFACE_DEFINED__
#define __IVLCPlaylistItems_INTERFACE_DEFINED__

/* interface IVLCPlaylistItems */
/* [object][oleautomation][dual][helpstring][uuid] */ 


EXTERN_C const IID IID_IVLCPlaylistItems;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("FD37FE32-82BC-4A25-B056-315F4DBB194D")
    IVLCPlaylistItems : public IDispatch
    {
    public:
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_count( 
            /* [retval][out] */ long *count) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE clear( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE remove( 
            /* [in] */ long itemId) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVLCPlaylistItemsVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IVLCPlaylistItems * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IVLCPlaylistItems * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IVLCPlaylistItems * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IVLCPlaylistItems * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IVLCPlaylistItems * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IVLCPlaylistItems * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IVLCPlaylistItems * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_count )( 
            IVLCPlaylistItems * This,
            /* [retval][out] */ long *count);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *clear )( 
            IVLCPlaylistItems * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE *remove )( 
            IVLCPlaylistItems * This,
            /* [in] */ long itemId);
        
        END_INTERFACE
    } IVLCPlaylistItemsVtbl;

    interface IVLCPlaylistItems
    {
        CONST_VTBL struct IVLCPlaylistItemsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IVLCPlaylistItems_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IVLCPlaylistItems_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IVLCPlaylistItems_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IVLCPlaylistItems_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IVLCPlaylistItems_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IVLCPlaylistItems_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IVLCPlaylistItems_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IVLCPlaylistItems_get_count(This,count)	\
    (This)->lpVtbl -> get_count(This,count)

#define IVLCPlaylistItems_clear(This)	\
    (This)->lpVtbl -> clear(This)

#define IVLCPlaylistItems_remove(This,itemId)	\
    (This)->lpVtbl -> remove(This,itemId)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCPlaylistItems_get_count_Proxy( 
    IVLCPlaylistItems * This,
    /* [retval][out] */ long *count);


void __RPC_STUB IVLCPlaylistItems_get_count_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCPlaylistItems_clear_Proxy( 
    IVLCPlaylistItems * This);


void __RPC_STUB IVLCPlaylistItems_clear_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCPlaylistItems_remove_Proxy( 
    IVLCPlaylistItems * This,
    /* [in] */ long itemId);


void __RPC_STUB IVLCPlaylistItems_remove_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVLCPlaylistItems_INTERFACE_DEFINED__ */


EXTERN_C const CLSID CLSID_VLCPlugin;

#ifdef __cplusplus

class DECLSPEC_UUID("E23FE9C6-778E-49D4-B537-38FCDE4887D8")
VLCPlugin;
#endif

EXTERN_C const CLSID CLSID_VLCPlugin2;

#ifdef __cplusplus

class DECLSPEC_UUID("9BE31822-FDAD-461B-AD51-BE1D1C159921")
VLCPlugin2;
#endif
#endif /* __AXVLC_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


