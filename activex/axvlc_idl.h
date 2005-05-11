/* this ALWAYS GENERATED file contains the definitions for the interfaces */


/* File created by MIDL compiler version 5.01.0164 */
/* at Tue May 10 21:24:51 2005
 */
/* Compiler settings for axvlc.idl:
    Oicf (OptLev=i2), W1, Zp8, env=Win32, ms_ext, c_ext
    error checks: allocation ref bounds_check enum stub_data 
*/
//@@MIDL_FILE_HEADING(  )


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 440
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __axvlc_idl_h__
#define __axvlc_idl_h__

#ifdef __cplusplus
extern "C"{
#endif 

/* Forward Declarations */ 

#ifndef __IVLCControl_FWD_DEFINED__
#define __IVLCControl_FWD_DEFINED__
typedef interface IVLCControl IVLCControl;
#endif 	/* __IVLCControl_FWD_DEFINED__ */


#ifndef __DVLCEvents_FWD_DEFINED__
#define __DVLCEvents_FWD_DEFINED__
typedef interface DVLCEvents DVLCEvents;
#endif 	/* __DVLCEvents_FWD_DEFINED__ */


#ifndef __VLCPlugin_FWD_DEFINED__
#define __VLCPlugin_FWD_DEFINED__

#ifdef __cplusplus
typedef class VLCPlugin VLCPlugin;
#else
typedef struct VLCPlugin VLCPlugin;
#endif /* __cplusplus */

#endif 	/* __VLCPlugin_FWD_DEFINED__ */


void __RPC_FAR * __RPC_USER MIDL_user_allocate(size_t);
void __RPC_USER MIDL_user_free( void __RPC_FAR * ); 


#ifndef __AXVLC_LIBRARY_DEFINED__
#define __AXVLC_LIBRARY_DEFINED__

/* library AXVLC */
/* [helpstring][version][uuid] */ 




enum VLCPlaylistMode
    {	VLCPlayListInsert	= 1,
	VLCPlayListReplace	= 2,
	VLCPlayListAppend	= 4,
	VLCPlayListGo	= 8,
	VLCPlayListCheckInsert	= 16
    };
#define	VLCPlayListEnd	( -666 )

#define	DISPID_Visible	( 1 )

#define	DISPID_Playing	( 2 )

#define	DISPID_Position	( 3 )

#define	DISPID_Time	( 4 )

#define	DISPID_Length	( 5 )

#define	DISPID_Volume	( 6 )

#define	DISPID_PlayEvent	( 1 )

#define	DISPID_PauseEvent	( 2 )

#define	DISPID_StopEvent	( 3 )


EXTERN_C const IID LIBID_AXVLC;

#ifndef __IVLCControl_INTERFACE_DEFINED__
#define __IVLCControl_INTERFACE_DEFINED__

/* interface IVLCControl */
/* [object][oleautomation][hidden][dual][helpstring][uuid] */ 


EXTERN_C const IID IID_IVLCControl;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("C2FA41D0-B113-476e-AC8C-9BD14999C1C1")
    IVLCControl : public IDispatch
    {
    public:
        virtual /* [helpstring][bindable][propget][id] */ HRESULT STDMETHODCALLTYPE get_Visible( 
            /* [retval][out] */ VARIANT_BOOL __RPC_FAR *visible) = 0;
        
        virtual /* [helpstring][bindable][propput][id] */ HRESULT STDMETHODCALLTYPE put_Visible( 
            /* [in] */ VARIANT_BOOL visible) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE play( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE pause( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE stop( void) = 0;
        
        virtual /* [helpstring][propget][bindable][id] */ HRESULT STDMETHODCALLTYPE get_Playing( 
            /* [retval][out] */ VARIANT_BOOL __RPC_FAR *isPlaying) = 0;
        
        virtual /* [helpstring][propput][bindable][id] */ HRESULT STDMETHODCALLTYPE put_Playing( 
            /* [in] */ VARIANT_BOOL isPlaying) = 0;
        
        virtual /* [helpstring][propget][bindable][id] */ HRESULT STDMETHODCALLTYPE get_Position( 
            /* [retval][out] */ float __RPC_FAR *position) = 0;
        
        virtual /* [helpstring][propput][bindable][id] */ HRESULT STDMETHODCALLTYPE put_Position( 
            /* [in] */ float position) = 0;
        
        virtual /* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_Time( 
            /* [retval][out] */ int __RPC_FAR *seconds) = 0;
        
        virtual /* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_Time( 
            /* [in] */ int seconds) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE shuttle( 
            /* [in] */ int seconds) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE fullscreen( void) = 0;
        
        virtual /* [helpstring][propget][bindable][id] */ HRESULT STDMETHODCALLTYPE get_Length( 
            /* [retval][out] */ int __RPC_FAR *seconds) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE playFaster( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE playSlower( void) = 0;
        
        virtual /* [helpstring][propget][bindable][id] */ HRESULT STDMETHODCALLTYPE get_Volume( 
            /* [retval][out] */ int __RPC_FAR *volume) = 0;
        
        virtual /* [helpstring][propput][bindable][id] */ HRESULT STDMETHODCALLTYPE put_Volume( 
            /* [in] */ int volume) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE toggleMute( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE setVariable( 
            /* [in] */ BSTR name,
            /* [in] */ VARIANT value) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE getVariable( 
            /* [in] */ BSTR name,
            /* [retval][out] */ VARIANT __RPC_FAR *value) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE addTarget( 
            /* [in] */ BSTR uri,
            /* [in] */ VARIANT options,
            /* [in] */ enum VLCPlaylistMode mode,
            /* [in] */ int position) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_PlaylistIndex( 
            /* [retval][out] */ int __RPC_FAR *index) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_PlaylistCount( 
            /* [retval][out] */ int __RPC_FAR *index) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE playlistNext( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE playlistPrev( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE playlistClear( void) = 0;
        
        virtual /* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_VersionInfo( 
            /* [retval][out] */ BSTR __RPC_FAR *version) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IVLCControlVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IVLCControl __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IVLCControl __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IVLCControl __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTypeInfoCount )( 
            IVLCControl __RPC_FAR * This,
            /* [out] */ UINT __RPC_FAR *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTypeInfo )( 
            IVLCControl __RPC_FAR * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetIDsOfNames )( 
            IVLCControl __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID __RPC_FAR *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Invoke )( 
            IVLCControl __RPC_FAR * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams,
            /* [out] */ VARIANT __RPC_FAR *pVarResult,
            /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo,
            /* [out] */ UINT __RPC_FAR *puArgErr);
        
        /* [helpstring][bindable][propget][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_Visible )( 
            IVLCControl __RPC_FAR * This,
            /* [retval][out] */ VARIANT_BOOL __RPC_FAR *visible);
        
        /* [helpstring][bindable][propput][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_Visible )( 
            IVLCControl __RPC_FAR * This,
            /* [in] */ VARIANT_BOOL visible);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *play )( 
            IVLCControl __RPC_FAR * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *pause )( 
            IVLCControl __RPC_FAR * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *stop )( 
            IVLCControl __RPC_FAR * This);
        
        /* [helpstring][propget][bindable][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_Playing )( 
            IVLCControl __RPC_FAR * This,
            /* [retval][out] */ VARIANT_BOOL __RPC_FAR *isPlaying);
        
        /* [helpstring][propput][bindable][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_Playing )( 
            IVLCControl __RPC_FAR * This,
            /* [in] */ VARIANT_BOOL isPlaying);
        
        /* [helpstring][propget][bindable][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_Position )( 
            IVLCControl __RPC_FAR * This,
            /* [retval][out] */ float __RPC_FAR *position);
        
        /* [helpstring][propput][bindable][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_Position )( 
            IVLCControl __RPC_FAR * This,
            /* [in] */ float position);
        
        /* [helpstring][propget][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_Time )( 
            IVLCControl __RPC_FAR * This,
            /* [retval][out] */ int __RPC_FAR *seconds);
        
        /* [helpstring][propput][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_Time )( 
            IVLCControl __RPC_FAR * This,
            /* [in] */ int seconds);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *shuttle )( 
            IVLCControl __RPC_FAR * This,
            /* [in] */ int seconds);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *fullscreen )( 
            IVLCControl __RPC_FAR * This);
        
        /* [helpstring][propget][bindable][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_Length )( 
            IVLCControl __RPC_FAR * This,
            /* [retval][out] */ int __RPC_FAR *seconds);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *playFaster )( 
            IVLCControl __RPC_FAR * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *playSlower )( 
            IVLCControl __RPC_FAR * This);
        
        /* [helpstring][propget][bindable][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_Volume )( 
            IVLCControl __RPC_FAR * This,
            /* [retval][out] */ int __RPC_FAR *volume);
        
        /* [helpstring][propput][bindable][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_Volume )( 
            IVLCControl __RPC_FAR * This,
            /* [in] */ int volume);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *toggleMute )( 
            IVLCControl __RPC_FAR * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *setVariable )( 
            IVLCControl __RPC_FAR * This,
            /* [in] */ BSTR name,
            /* [in] */ VARIANT value);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *getVariable )( 
            IVLCControl __RPC_FAR * This,
            /* [in] */ BSTR name,
            /* [retval][out] */ VARIANT __RPC_FAR *value);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *addTarget )( 
            IVLCControl __RPC_FAR * This,
            /* [in] */ BSTR uri,
            /* [in] */ VARIANT options,
            /* [in] */ enum VLCPlaylistMode mode,
            /* [in] */ int position);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_PlaylistIndex )( 
            IVLCControl __RPC_FAR * This,
            /* [retval][out] */ int __RPC_FAR *index);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_PlaylistCount )( 
            IVLCControl __RPC_FAR * This,
            /* [retval][out] */ int __RPC_FAR *index);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *playlistNext )( 
            IVLCControl __RPC_FAR * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *playlistPrev )( 
            IVLCControl __RPC_FAR * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *playlistClear )( 
            IVLCControl __RPC_FAR * This);
        
        /* [helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_VersionInfo )( 
            IVLCControl __RPC_FAR * This,
            /* [retval][out] */ BSTR __RPC_FAR *version);
        
        END_INTERFACE
    } IVLCControlVtbl;

    interface IVLCControl
    {
        CONST_VTBL struct IVLCControlVtbl __RPC_FAR *lpVtbl;
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

#define IVLCControl_put_Playing(This,isPlaying)	\
    (This)->lpVtbl -> put_Playing(This,isPlaying)

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

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][bindable][propget][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_get_Visible_Proxy( 
    IVLCControl __RPC_FAR * This,
    /* [retval][out] */ VARIANT_BOOL __RPC_FAR *visible);


void __RPC_STUB IVLCControl_get_Visible_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][bindable][propput][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_put_Visible_Proxy( 
    IVLCControl __RPC_FAR * This,
    /* [in] */ VARIANT_BOOL visible);


void __RPC_STUB IVLCControl_put_Visible_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_play_Proxy( 
    IVLCControl __RPC_FAR * This);


void __RPC_STUB IVLCControl_play_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_pause_Proxy( 
    IVLCControl __RPC_FAR * This);


void __RPC_STUB IVLCControl_pause_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_stop_Proxy( 
    IVLCControl __RPC_FAR * This);


void __RPC_STUB IVLCControl_stop_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][bindable][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_get_Playing_Proxy( 
    IVLCControl __RPC_FAR * This,
    /* [retval][out] */ VARIANT_BOOL __RPC_FAR *isPlaying);


void __RPC_STUB IVLCControl_get_Playing_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput][bindable][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_put_Playing_Proxy( 
    IVLCControl __RPC_FAR * This,
    /* [in] */ VARIANT_BOOL isPlaying);


void __RPC_STUB IVLCControl_put_Playing_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][bindable][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_get_Position_Proxy( 
    IVLCControl __RPC_FAR * This,
    /* [retval][out] */ float __RPC_FAR *position);


void __RPC_STUB IVLCControl_get_Position_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput][bindable][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_put_Position_Proxy( 
    IVLCControl __RPC_FAR * This,
    /* [in] */ float position);


void __RPC_STUB IVLCControl_put_Position_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_get_Time_Proxy( 
    IVLCControl __RPC_FAR * This,
    /* [retval][out] */ int __RPC_FAR *seconds);


void __RPC_STUB IVLCControl_get_Time_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_put_Time_Proxy( 
    IVLCControl __RPC_FAR * This,
    /* [in] */ int seconds);


void __RPC_STUB IVLCControl_put_Time_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_shuttle_Proxy( 
    IVLCControl __RPC_FAR * This,
    /* [in] */ int seconds);


void __RPC_STUB IVLCControl_shuttle_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_fullscreen_Proxy( 
    IVLCControl __RPC_FAR * This);


void __RPC_STUB IVLCControl_fullscreen_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][bindable][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_get_Length_Proxy( 
    IVLCControl __RPC_FAR * This,
    /* [retval][out] */ int __RPC_FAR *seconds);


void __RPC_STUB IVLCControl_get_Length_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_playFaster_Proxy( 
    IVLCControl __RPC_FAR * This);


void __RPC_STUB IVLCControl_playFaster_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_playSlower_Proxy( 
    IVLCControl __RPC_FAR * This);


void __RPC_STUB IVLCControl_playSlower_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][bindable][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_get_Volume_Proxy( 
    IVLCControl __RPC_FAR * This,
    /* [retval][out] */ int __RPC_FAR *volume);


void __RPC_STUB IVLCControl_get_Volume_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput][bindable][id] */ HRESULT STDMETHODCALLTYPE IVLCControl_put_Volume_Proxy( 
    IVLCControl __RPC_FAR * This,
    /* [in] */ int volume);


void __RPC_STUB IVLCControl_put_Volume_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_toggleMute_Proxy( 
    IVLCControl __RPC_FAR * This);


void __RPC_STUB IVLCControl_toggleMute_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_setVariable_Proxy( 
    IVLCControl __RPC_FAR * This,
    /* [in] */ BSTR name,
    /* [in] */ VARIANT value);


void __RPC_STUB IVLCControl_setVariable_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_getVariable_Proxy( 
    IVLCControl __RPC_FAR * This,
    /* [in] */ BSTR name,
    /* [retval][out] */ VARIANT __RPC_FAR *value);


void __RPC_STUB IVLCControl_getVariable_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_addTarget_Proxy( 
    IVLCControl __RPC_FAR * This,
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
    IVLCControl __RPC_FAR * This,
    /* [retval][out] */ int __RPC_FAR *index);


void __RPC_STUB IVLCControl_get_PlaylistIndex_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCControl_get_PlaylistCount_Proxy( 
    IVLCControl __RPC_FAR * This,
    /* [retval][out] */ int __RPC_FAR *index);


void __RPC_STUB IVLCControl_get_PlaylistCount_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_playlistNext_Proxy( 
    IVLCControl __RPC_FAR * This);


void __RPC_STUB IVLCControl_playlistNext_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_playlistPrev_Proxy( 
    IVLCControl __RPC_FAR * This);


void __RPC_STUB IVLCControl_playlistPrev_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IVLCControl_playlistClear_Proxy( 
    IVLCControl __RPC_FAR * This);


void __RPC_STUB IVLCControl_playlistClear_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget] */ HRESULT STDMETHODCALLTYPE IVLCControl_get_VersionInfo_Proxy( 
    IVLCControl __RPC_FAR * This,
    /* [retval][out] */ BSTR __RPC_FAR *version);


void __RPC_STUB IVLCControl_get_VersionInfo_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IVLCControl_INTERFACE_DEFINED__ */


#ifndef __DVLCEvents_DISPINTERFACE_DEFINED__
#define __DVLCEvents_DISPINTERFACE_DEFINED__

/* dispinterface DVLCEvents */
/* [hidden][helpstring][uuid] */ 


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
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            DVLCEvents __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            DVLCEvents __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            DVLCEvents __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTypeInfoCount )( 
            DVLCEvents __RPC_FAR * This,
            /* [out] */ UINT __RPC_FAR *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTypeInfo )( 
            DVLCEvents __RPC_FAR * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetIDsOfNames )( 
            DVLCEvents __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID __RPC_FAR *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Invoke )( 
            DVLCEvents __RPC_FAR * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams,
            /* [out] */ VARIANT __RPC_FAR *pVarResult,
            /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo,
            /* [out] */ UINT __RPC_FAR *puArgErr);
        
        END_INTERFACE
    } DVLCEventsVtbl;

    interface DVLCEvents
    {
        CONST_VTBL struct DVLCEventsVtbl __RPC_FAR *lpVtbl;
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


EXTERN_C const CLSID CLSID_VLCPlugin;

#ifdef __cplusplus

class DECLSPEC_UUID("E23FE9C6-778E-49D4-B537-38FCDE4887D8")
VLCPlugin;
#endif
#endif /* __AXVLC_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif
