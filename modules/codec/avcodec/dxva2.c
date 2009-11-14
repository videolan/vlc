/*****************************************************************************
 * dxva.c: DXVA 2 video decoder
 *****************************************************************************
 * Copyright (C) 2009 Geoffroy Couprie
 * $Id$
 *
 * Authors: Geoffroy Couprie <geal@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

#include <windows.h>
#include <windowsx.h>
#include <ole2.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <d3d9.h>

#include "dxva.h"

DEFINE_GUID(DXVA2_ModeMPEG2_MoComp, 0xe6a9f44b, 0x61b0, 0x4563,0x9e,0xa4,0x63,0xd2,0xa3,0xc6,0xfe,0x66);
DEFINE_GUID(DXVA2_ModeMPEG2_IDCT,   0xbf22ad00, 0x03ea, 0x4690,0x80,0x77,0x47,0x33,0x46,0x20,0x9b,0x7e);
DEFINE_GUID(DXVA2_ModeMPEG2_VLD,    0xee27417f, 0x5e28, 0x4e65,0xbe,0xea,0x1d,0x26,0xb5,0x08,0xad,0xc9);
DEFINE_GUID(DXVA2_ModeH264_A,  0x1b81be64, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeH264_B,  0x1b81be65, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeH264_C,  0x1b81be66, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeH264_D,  0x1b81be67, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeH264_E,  0x1b81be68, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeH264_F,  0x1b81be69, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeWMV8_A,  0x1b81be80, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeWMV8_B,  0x1b81be81, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeWMV9_A,  0x1b81be90, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeWMV9_B,  0x1b81be91, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeWMV9_C,  0x1b81be94, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeVC1_A,   0x1b81beA0, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeVC1_B,   0x1b81beA1, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeVC1_C,   0x1b81beA2, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeVC1_D,   0x1b81beA3, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);

const GUID IID_IDirectXVideoDecoderService = {0xfc51a551,0xd5e7,0x11d9, {0xaf,0x55,0x00,0x05,0x4e,0x43,0xff,0x02}};
const GUID IID_IDirectXVideoAccelerationService = {0xfc51a550,0xd5e7,0x11d9,{0xaf,0x55,0x00,0x05,0x4e,0x43,0xff,0x02}};

const D3DFORMAT VIDEO_RENDER_TARGET_FORMAT = D3DFMT_X8R8G8B8;

void format_error();

#define print_error(a) format_error(p_dec, __FILE__, __FUNCTION__, __LINE__,a)

static void *DecodeBlock( decoder_t *p_dec, block_t **pp_block );
LPDIRECTXVIDEODECODER pvid_dec;
LPDIRECT3DSURFACE9 psurfaces = NULL;
/*****************************************************************************
 * decoder_sys_t : dxva decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
	HINSTANCE g_hinst;                          /* This application's HINSTANCE */
	HINSTANCE hd3d9_dll;
	HWND g_hwndChild;                           /* Optional child window */
	LPDIRECT3D9 g_pD3D;
	D3DPRESENT_PARAMETERS d3dpp;
	LPDIRECT3DDEVICE9 g_pd3dDevice;
    IDirect3DSurface9* g_pD3DRT;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder( vlc_object_t * );
static void CloseDecoder( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("DXVA 2 video decoder") )
    set_capability( "decoder", 150 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_callbacks( OpenDecoder, CloseDecoder )
    add_shortcut( "dxva" )
vlc_module_end ()

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

switch( p_dec->fmt_in.i_codec )
    {
    case VLC_FOURCC('h','2','6','4'):
    //case VLC_FOURCC('m','p','g','2'):
    //case VLC_FOURCC('m','p','g','v'):
    //case VLC_CODEC_WMVA:
    //case VLC_CODEC_VC1:
        break;
    default:
        //if( p_dec->fmt_in.i_original_fourcc )
            return VLC_EGENERIC;
        //break;
    }

    /* Allocate the memory needed to store the decoder's structure */
    p_sys = malloc(sizeof(decoder_sys_t));
    if( p_sys == NULL )
        return VLC_ENOMEM;

/************** VLC codec stuff **********************/
    char fourcc[5];
    char original_fourcc[5];
    fourcc[5] = 0;
    original_fourcc[5] = 0;
    vlc_fourcc_to_char(p_dec->fmt_in.i_codec, fourcc);
    vlc_fourcc_to_char(p_dec->fmt_in.i_original_fourcc, original_fourcc);
    msg_Dbg(p_dec, "Input format:\n\tcategory:%u\n\tcodec:%s\n\toriginal fourcc:%s\n\tbitrate:%u",
            p_dec->fmt_in.i_cat,fourcc,original_fourcc, p_dec->fmt_in.i_bitrate);     
    msg_Dbg(p_dec, "Input format:\n\twidth:%u\n\theight:%u\n\tratio:%d",
            p_dec->fmt_in.video.i_width,p_dec->fmt_in.video.i_height,p_dec->fmt_in.video.i_aspect);
/************** D3D stuff **********************/

    ZeroMemory( &p_sys->d3dpp, sizeof(D3DPRESENT_PARAMETERS) );

    p_sys->d3dpp.Windowed = TRUE;
    p_sys->d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    p_sys->d3dpp.BackBufferFormat = VIDEO_RENDER_TARGET_FORMAT;
    p_sys->d3dpp.BackBufferWidth  = p_dec->fmt_in.video.i_width;
    p_sys->d3dpp.BackBufferHeight = p_dec->fmt_in.video.i_height;
    p_sys->d3dpp.BackBufferCount = 2;
    p_sys->d3dpp.Flags = D3DPRESENTFLAG_VIDEO;

    LPDIRECT3D9 (WINAPI *OurDirect3DCreate9)(UINT SDKVersion);

    msg_Dbg( p_dec, "loading d3d dll" );
    p_sys->hd3d9_dll = LoadLibrary(TEXT("D3D9.DLL"));
    if( NULL == p_sys->hd3d9_dll )
    {
        msg_Warn( p_dec, "cannot load d3d9.dll, aborting" );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_dec, "loading d3d function" );
    OurDirect3DCreate9 =
      (void *)GetProcAddress( p_sys->hd3d9_dll,
                              TEXT("Direct3DCreate9") );
    if( OurDirect3DCreate9 == NULL )
    {
        msg_Err( p_dec, "Cannot locate reference to Direct3DCreate9 ABI in DLL" );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_dec, "creating d3d" );
    if( NULL == ( p_sys->g_pD3D = OurDirect3DCreate9( D3D_SDK_VERSION ) ) )
    {
        msg_Err( p_dec, "Direct3DCreate9 failed" );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_dec, "creating d3d device" );

    /* Direct3D needs a HWND to create a device, even without using ::Present
    this HWND is used to alert Direct3D when there's a change of focus window.
    For now, use GetShellWindow, as it looks harmless */
    if( FAILED( IDirect3D9_CreateDevice( p_sys->g_pD3D, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, GetShellWindow(),
                              D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                              &p_sys->d3dpp, &p_sys->g_pd3dDevice ) ) )
    {
        print_error("IDirect3D9_CreateDevice failed");
        return VLC_EGENERIC;
    }
/*****************END OF D3D****************************/

/*****************DXVA STUFF****************************/

    HRESULT hr;
    HINSTANCE hdxva2_dll;

    HRESULT (WINAPI *MyDXVA2CreateVideoService)
        (
        IDirect3DDevice9 *pDD,
        REFIID riid,
        void **ppService
        );
    HRESULT (WINAPI *MyDXVA2CreateDirect3DDeviceManager9)
        (
            UINT *pResetToken,
            IDirect3DDeviceManager9 **ppDXVAManager
        );


    hdxva2_dll = LoadLibrary(TEXT("DXVA2.DLL"));
    if( NULL == hdxva2_dll )
    {
        msg_Err( p_dec, "cannot load DXVA2\n");
        return 3;
    }

    MyDXVA2CreateVideoService =
      (void *)GetProcAddress(hdxva2_dll,
                              TEXT("DXVA2CreateVideoService") );

    if( MyDXVA2CreateVideoService == NULL )
    {
        msg_Err( p_dec, "cannot load function\n");
        return 4;
    }
    else
        msg_Info( p_dec, "DXVA2CreateVideoService Success!");

    MyDXVA2CreateDirect3DDeviceManager9 =
      (void *)GetProcAddress(hdxva2_dll,
                              TEXT("DXVA2CreateDirect3DDeviceManager9") );

    if( MyDXVA2CreateDirect3DDeviceManager9 == NULL )
    {
        msg_Err( p_dec, "cannot load function\n");
        return 4;
    }
    else
        msg_Info( p_dec, "DXVA2CreateDirect3DDeviceManager9 Success!");

    UINT reset_token;
    LPDIRECT3DDEVICEMANAGER9 p_devman;
    if( FAILED(  MyDXVA2CreateDirect3DDeviceManager9 ( &reset_token,&p_devman ) ) )
    {
        msg_Err( p_dec, " MyDXVA2CreateDirect3DDeviceManager9 failed");
        return 5;
    }
    else
        msg_Info( p_dec, "obtained IDirect3DDeviceManager9");   

    HRESULT bb = IDirect3DDeviceManager9_ResetDevice( p_devman, p_sys->g_pd3dDevice, reset_token);
    msg_Err( p_dec, "IDirect3DDeviceManager9_ResetDevice result: %08x", bb);

    LPDIRECTXVIDEODECODERSERVICE g_pdxva_vs;
    HANDLE hDevice;
    
    HRESULT hret1 = IDirect3DDeviceManager9_OpenDeviceHandle( p_devman, &hDevice);
    
    HRESULT hret2 = IDirect3DDeviceManager9_GetVideoService( p_devman, hDevice, &IID_IDirectXVideoDecoderService,
        (void **) &g_pdxva_vs );

    /*****************configuration dxva****************************/

    UINT nbguid = 0;
    GUID* pdecoderguids = NULL;

    if( FAILED( IDirectXVideoDecoderService_GetDecoderDeviceGuids ( g_pdxva_vs, &nbguid, &pdecoderguids ) ) )
    {
        msg_Err( p_dec, "IDirectXVideoDecoderService_GetDecoderDeviceGuids failed");
        return 6;
    }

    UINT iGuid;
    // Look for the decoder GUIDs we want.
    for (iGuid = 0; iGuid < nbguid; iGuid++)
    {
        //liste des GUID à afficher plus tard en Dbg (pour savoir ce qu'on peut décoder)
        msg_Dbg( p_dec, "GUID = %08X-%04x-%04x-XXXX\n", pdecoderguids[iGuid].Data1,pdecoderguids[iGuid].Data2,pdecoderguids[iGuid].Data3);
        
    }
    
   /* on prend pour les tests pdecoderguids[6] = DXVA2_ModeH264_E => H.264 VLD, no FGT.
   On cherche parmi les GUID qu'on a récupérés celui qui correspond au codec de la vidéo
   cf http://msdn.microsoft.com/en-us/library/ms697067%28VS.85%29.aspx pour une correspondance entre les GUID et les codecs
   if( p_dec->fmt_in.i_original_fourcc == VLC_FOURCC('h','2','6','4'))
    {
        for (iGuid = 0; iGuid < nbguid; iGuid++)
        {
   
        }
    }*/
   
    // FAILS probably because of the unusual d3d device
    UINT nb_render = 0;
    D3DFORMAT *prender_targets = NULL;

    if( FAILED( IDirectXVideoDecoderService_GetDecoderRenderTargets ( g_pdxva_vs,
                &pdecoderguids[6], &nb_render, &prender_targets) ) )
    {
        msg_Err( p_dec, "IDirectXVideoDecoderService_GetDecoderRenderTargets failed");
        return 7;
    } 
    
    fourcc[5] = 0;
    vlc_fourcc_to_char(prender_targets[0], fourcc);
    msg_Info( p_dec, "we got %d decoder formats, choosing d3dformat n° %d test desc:%s", 
        nb_render,prender_targets[0],fourcc);

/* Only one decoder format, for now */    
/* Ici, il faut faire la même chose que les decoder GUIDs: on regarde les différents formats, et on en choisit un qui correspond*/
    D3DFORMAT fmt = prender_targets[0];
    DXVA2_VideoDesc vid_desc;
    DXVA2_ExtendedFormat ext_fmt;
   // ext_fmt = 
    vid_desc.SampleWidth = p_dec->fmt_in.video.i_width;//???
    vid_desc.SampleHeight = p_dec->fmt_in.video.i_height;//???

    vid_desc.Format = prender_targets[0];
    vid_desc.InputSampleFreq.Numerator = 60;
    vid_desc.InputSampleFreq.Denominator = 1;
    vid_desc.OutputFrameFreq.Numerator = 60;
    vid_desc.OutputFrameFreq.Denominator = 1;
    vid_desc.OutputFrameFreq = vid_desc.InputSampleFreq;
    vid_desc.UABProtectionLevel = false;
    vid_desc.Reserved = 0;

    UINT nb_dec_conf=0;
    DXVA2_ConfigPictureDecode* pdec_conf = NULL;
    if( FAILED( IDirectXVideoDecoderService_GetDecoderConfigurations ( g_pdxva_vs,
                &pdecoderguids[0], &vid_desc, NULL, &nb_dec_conf, &pdec_conf )))
    {
        msg_Err( p_dec, "IDirectXVideoDecoderService_GetDecoderConfigurations failed\n");
        return 7;
    }           
    
    msg_Info( p_dec, "we got %d decoder configurations", nb_dec_conf);
 
/* Création de la surface de rendering*/
    if( FAILED( IDirectXVideoDecoderService_CreateSurface ( g_pdxva_vs,  p_dec->fmt_in.video.i_width, p_dec->fmt_in.video.i_height,
        1, prender_targets[0], D3DPOOL_DEFAULT, 0, 0/*DXVA2_VideoDecoderRenderTarget*/, &psurfaces, NULL )))
    {
        msg_Err( p_dec, "IDirectXVideoAccelerationService_CreateSurface failed\n");
        return 7;
    } 
    
    msg_Info( p_dec, "we got d3d surfaces\n");
    

    HRESULT aa;
    aa = IDirectXVideoDecoderService_CreateVideoDecoder ( g_pdxva_vs,
                &pdecoderguids[6], &vid_desc, &pdec_conf[1], &psurfaces, 2,
                &pvid_dec);
   // msg_Err( p_dec,"IDirectXVideoDecoderService_CreateVideoDecoder1: %08x\n", aa);

    HRESULT cc = IDirect3DDeviceManager9_ResetDevice( p_devman, p_sys->g_pd3dDevice, reset_token);
    //msg_Err( p_dec, "IDirect3DDeviceManager9_ResetDevice result: %08x\n", cc);
    
    aa = IDirectXVideoDecoderService_CreateVideoDecoder ( g_pdxva_vs,
                &pdecoderguids[6], &vid_desc, &pdec_conf[1], &psurfaces, 2,
                &pvid_dec);
    //msg_Err( p_dec, "IDirectXVideoDecoderService_CreateVideoDecoder2: %08x\n", aa);

    /*if( FAILED( IDirectXVideoDecoderService_CreateVideoDecoder ( g_pdxva_vs,
                &pdecoderguids[6], &vid_desc, &pdec_conf[0], &psurfaces, 6,
                &pvid_dec)))
    {
        print_error("IDirectXVideoDecoderService_CreateVideoDecoder failed");
        return 9;  
    }*/
    if( aa == 0)
    {
        p_dec->pf_decode_video = DecodeBlock;
        p_dec->fmt_out.i_cat = VIDEO_ES;
        p_dec->fmt_out.i_codec = VLC_CODEC_I420; //???
        p_dec->fmt_out.video.i_width = p_dec->fmt_in.video.i_width;//pas sur
        p_dec->fmt_out.video.i_height = p_dec->fmt_in.video.i_height;//???
        p_dec->fmt_out.video.i_visible_width = p_dec->fmt_in.video.i_width;//???
        p_dec->fmt_out.video.i_visible_height = p_dec->fmt_in.video.i_height;//???
        p_dec->fmt_out.video.i_aspect = 1;//p_dec->fmt_in.video.i_aspect;//?????
        p_dec->fmt_out.video.i_sar_num = 1;
        p_dec->fmt_out.video.i_sar_den = 1;
        return VLC_SUCCESS;
    }
    else
    {
/**************END OF DXVA******************************/

    //CoTaskMemFree(pdecoderguids);
    return VLC_EGENERIC;
    }
}



/*****************************************************************************
 * DecodeBlock: dxva decoder
 *****************************************************************************/
static void *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t   *p_sys = p_dec->p_sys;

    picture_t       *p_pic;

    block_t *p_block;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;
    uint8_t * buf = p_block->p_buffer;

/*****************************************************************************/
/* Etapes du décodage: http://msdn.microsoft.com/en-us/library/aa965245%28VS.85%29.aspx#Decoding */

    if( 0 != IDirectXVideoDecoder_BeginFrame(pvid_dec,psurfaces,NULL ) )
    {
        print_error("IDirectXVideoDecoder_BeginFrame failed");
    }

    UINT BufferType = DXVA2_PictureParametersBufferType;
    void *pBuffer;
    UINT BufferSize;
    if( 0 != IDirectXVideoDecoder_GetBuffer(pvid_dec, BufferType, &pBuffer, &BufferSize))
    {
        print_error("IDirectXVideoDecoder_GetBuffer failed");
    }
    else
    {
    //    msg_Dbg( p_dec, "IDirectXVideoDecoder_GetBuffer -> buffer size=%u\n", BufferSize);
   
/*************** FILLING THE BUFFER *****************************/   
        /*here fill pbuffer with video*/
        memcpy( pBuffer, p_block->p_buffer, BufferSize );

/*************** BUFFER FILLED ***********************************/
        if( 0 != IDirectXVideoDecoder_ReleaseBuffer(pvid_dec, BufferType) ) 
        {
            print_error("IDirectXVideoDecoder_ReleaseBuffer failed");
        }
/*************** DECODE VIDEO ***********************/
        
        DXVA2_DecodeExecuteParams params;
        params.NumCompBuffers = 1;
        
        DXVA2_DecodeBufferDesc bufferdesc[2];
        bufferdesc[0].CompressedBufferType = BufferType;
        bufferdesc[0].BufferIndex = 0; // Reserved
        bufferdesc[0].DataOffset = 0;
        bufferdesc[0].DataSize = BufferSize;
        bufferdesc[0].FirstMBaddress = 0; //??????
        bufferdesc[0].NumMBsInBuffer = 1; //??????
        bufferdesc[0].Width = 0; // Reserved
        bufferdesc[0].Height = 0; // Reserved
        bufferdesc[0].Stride = 0; // Reserved
        bufferdesc[0].ReservedBits = 0; // Reserved
        bufferdesc[0].pvPVPState = NULL; // No encrypted data

        params.pCompressedBuffers = bufferdesc;
        params.pExtensionData = NULL; // no private data to send to the driver
        HRESULT hr2 = IDirectXVideoDecoder_Execute(pvid_dec, &params);
        if(hr2 != (HRESULT)0x00000000L)
        {
            //print_error("");
        //    msg_Err( p_dec, "IDirectXVideoDecoder_Execute failed with error %08x\n", hr2);
        }
        else
        {
        //    msg_Err( p_dec, "\n\nIDirectXVideoDecoder_Execute SUCCESS\n\n");
        }
        
        bufferdesc[1].CompressedBufferType = DXVA2_BitStreamDateBufferType;
        bufferdesc[1].BufferIndex = 0; // Reserved
        bufferdesc[1].DataOffset = 0;
        bufferdesc[1].DataSize = BufferSize;
        bufferdesc[1].FirstMBaddress = 0; //??????
        bufferdesc[1].NumMBsInBuffer = 1; //??????
        bufferdesc[1].Width = 0; // Reserved
        bufferdesc[1].Height = 0; // Reserved
        bufferdesc[1].Stride = 0; // Reserved
        bufferdesc[1].ReservedBits = 0; // Reserved
        bufferdesc[1].pvPVPState = NULL; // No encrypted data
        
        DXVA2_DecodeExecuteParams params2;
        params2.NumCompBuffers = 1;
        params2.pCompressedBuffers = bufferdesc + 1;
        params2.pExtensionData = NULL; // no private data to send to the driver
        void *pBuffer2;
        UINT BufferSize2;
        
        if( 0 != IDirectXVideoDecoder_GetBuffer(pvid_dec, DXVA2_BitStreamDateBufferType, &pBuffer2, &BufferSize2))
        {
        //    print_error("IDirectXVideoDecoder_GetBuffer failed");
        }
        else
        {
        //    msg_Dbg( p_dec, "IDirectXVideoDecoder_GetBuffer bitstream -> buffer size=%u\n", BufferSize2);
            hr2 = IDirectXVideoDecoder_Execute(pvid_dec, &params2);
            if(hr2 != (HRESULT)0x00000000L)
            {
                //print_error("");
         //       msg_Err( p_dec, "IDirectXVideoDecoder_Execute bitstream failed with error %08x\n", hr2);
            }
            else
            {
        //        msg_Err( p_dec, "\n\nIDirectXVideoDecoder_Execute bitstream SUCCESS\n\n");
            }            
        }
/*************** END OF DECODING ***********************/
    }

    
    HANDLE endframeout;
    if( 0 != IDirectXVideoDecoder_EndFrame(pvid_dec,&endframeout) )
    {
        print_error("IDirectXVideoDecoder_EndFrame");

    }
    
    //msg_Dbg( p_dec, "New picture");
    /* Get a new picture */
    p_pic = decoder_NewPicture( p_dec );
    if( !p_pic ) return NULL;
    //msg_Dbg( p_dec, "Got a new picture");

     /*access the surface's memory*/
    D3DLOCKED_RECT d3dlock;
    if( FAILED( IDirect3DSurface9_LockRect( psurfaces, &d3dlock, NULL, 0/*??*/)) )
    {
        print_error("");
    }
    //Copy in picture_t, not block
    //memcpy( p_block->p_buffer, d3dlock.pBits, BufferSize );
    if( FAILED(IDirect3DSurface9_UnlockRect( psurfaces )))
    {
        print_error("");
    }
    
    /* Error handling, dates des samples, bloc_Release? */
    block_Release( p_block );
    return NULL;
}

/*****************************************************************************
 * CloseDecoder: dxva decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->g_pd3dDevice != NULL)
        IDirect3DDevice9_Release( p_sys->g_pd3dDevice );
    if( p_sys->g_pD3D != NULL)
        IDirect3D9_Release( p_sys->g_pD3D );

    free( p_sys );
}

void format_error(decoder_t *p_dec, char * file, char * function, int line, char * msg)
{
		LPVOID lpMsgBuf;
        DWORD err = GetLastError();
		FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
						NULL, /* lpSource */
						err, /*dwMessageId */
						MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* dwLanguageId */
						(LPTSTR) &lpMsgBuf,
						0, NULL );
		msg_Err( p_dec, "Error in %s:%d - %s : %08x-%s | %s", file, line, function, err, lpMsgBuf, msg);
		LocalFree(lpMsgBuf);
        fflush(stdout);
		return;
}
