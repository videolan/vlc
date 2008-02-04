/* This is public domain code developed by Elgato Systems GmbH. No GPL-covered 
 * changes were added to this file by any members of the VideoLAN team. If you
 * want to do so, add a modified GPL header here, but keep a message emphasising
 * the non-licensed parts of this header file. 
 * Ideally, VideoLAN-related changes should only go to eyetvplugin.h.
 * 
 * $Id$
 */


#pragma once

/*

	The EyeTV Plugin API
	====================
	
	The EyeTV Software gives third parties access to the incoming MPEG-2 transport stream. 
	At this time the API is available for the following products:
		
		- EyeTV 200 (analog)
		- EyeTV 300 (DVB-S) 
		- EyeTV 400 (DVB-T)
		
	A plugin receives device plugged/unplugged notifications, it can request or release 
	individual PIDs and most importantly it has access to transport stream packets in 
	real time, as they arrive from the device.  Note that the plugin is called before EyeTV 
	itself looks at the packets, so it is even possible to modify the data.
	
	Plugins currently live in EyeTV.app/Contens/Plugins/
	
	A plugin is packaged as a bundle with a single entry point:
	
		long EyeTVPluginDispatcher(EyeTVPluginSelector 			selector,  
										void 					*refCon, 
										EyeTVPluginDeviceID		deviceID, 
						   				long 					param1, 
						   				long 					param2,  
						   				long 					param3, 
						   				long 					param4);

	
	PID Filtering
	=============
	
	EyeTV employs both hardware and software PID filtering.  A plugin's dispatch routine 
	is called with the kEyeTVPluginSelector_PacketsArrived selector after the hardware 
	PID filter (naturally) but prior to the software PID filter, so the plugin has access 
	to all packets that are delivered by the hardware.  
	
	A plugin can request PIDs that are not needed by EyeTV from the hardware PID filter by 
	means of a callback routine, see eyeTVPluginSelector_SetCallback.

	Note that hardware PID filtering is on for single processor machines (to reduce the CPU 
	load), but off for multi-processor machines (to improve channel switch times).  
	This behaviour is controlled by the "hardware PID filter" key in com.elgato.eyetv.plist, 
	which defaults to "Auto" ("Off" on MP machines, "On" on single-processor machines).  EyeTV
	does not offer GUI to change this setting.  A plugin hence needs to be prepared to handle 
	both an entire transponder or multiplex and to request PIDs it might need.
	
	Note that the plugin is called on the real-time thread that receives the transport stream
	packets and that the packet buffers passed to the plugin are the actual hardware DMA buffers.
	Please return as quickly as possible from the kEyeTVPluginSelector_PacketsArrived call and
	avoid blocking the calling thread.





	Revision History:

	02/27/2004:		Initial Release.

*/
	


#define EYETV_PLUGIN_API_VERSION				0x04021901
#define EYETV_PLUGIN_API_MIN_VERSION			0x04021901

typedef long long 	EyeTVPluginDeviceID;
typedef long		EyeTVPluginDeviceType;
typedef long		EyeTVPluginSelector;

enum {
	kEyeTVPIDType_Video 		= 0,
	kEyeTVPIDType_MPEGAudio 	= 1,
	kEyeTVPIDType_VBI 			= 2, /* teletext */
	kEyeTVPIDType_PCR 			= 3,
	kEyeTVPIDType_PMT 			= 4,
	kEyeTVPIDType_Unknown 		= 5,
	kEyeTVPIDType_AC3Audio 		= 6
};


typedef struct EyeTVPluginPIDInfo EyeTVPluginPIDInfo;
struct EyeTVPluginPIDInfo {
	long					pid;
	long					pidType;	
};



/***********************************************************************************
*
*	EyeTVPluginCallbackParams,
*
***********************************************************************************/
enum {
	kEyeTVPluginCallbackSelector_RetainPIDs = 0,	
	kEyeTVPluginCallbackSelector_ReleasePIDs = 1		
};

typedef struct EyeTVPluginCallbackParams EyeTVPluginCallbackParams;  
struct EyeTVPluginCallbackParams {
	EyeTVPluginDeviceID		deviceID;	//	the deviceID
	long					selector;	// 	callback selector, see above
	long					*pids;		// 	list of pids to release/retain
	long					pidsCount;	//	count of pids
};


/***********************************************************************************
*
*	typedef for the callback function,
*
***********************************************************************************/
typedef long(* EyeTVPluginCallbackProc)(EyeTVPluginCallbackParams *params);



/***********************************************************************************
*
*	EyeTVPluginParamStructs
*
***********************************************************************************/


typedef struct EyeTVPluginInitializeParams	EyeTVPluginInitializeParams;
struct EyeTVPluginInitializeParams {
	long							apiVersion;		// version of the EyeTV_PLUGIN_API
	EyeTVPluginCallbackProc			callback;		// the callback
}; /* 8 bytes */



typedef struct EyeTVPluginGetInfoParams	EyeTVPluginGetInfoParams;
struct EyeTVPluginGetInfoParams {
	long							*pluginAPIVersion;		// <- EYETV_PLUGIN_API_VERSION
	char							*pluginName;		// <- points to a 128-byte buffer, the plugin is expected to fill the buffer with
														// 	  a UTF-8 encoded string.	
	char							*description;		// <- points to a 1024-byte buffer, the plugin is expected to fill the buffer with
														//	  a UTF-8 encoded string describing the plugin.
}; /* 12 bytes */



enum {
	kEyeTVPluginDeviceType_Unknown = 0,	
	kEyeTVPluginDeviceType_e200 = 1,
	kEyeTVPluginDeviceType_e300 = 2,
	kEyeTVPluginDeviceType_e400 = 3
} ;

typedef struct EyeTVPluginDeviceAddedParams EyeTVPluginDeviceAddedParams;
struct EyeTVPluginDeviceAddedParams {
	EyeTVPluginDeviceType			deviceType;		
}; /* 4 bytes */


typedef struct EyeTVPluginPacketsArrivedParams EyeTVPluginPacketsArrivedParams;
struct EyeTVPluginPacketsArrivedParams {
	long							**packets;		// points to an array of packets
	long							packetCount;	
}; /* 8 bytes */



typedef struct EyeTVPluginServiceChangedParams EyeTVPluginServiceChangedParams;
struct EyeTVPluginServiceChangedParams {
	long							headendID;		// new headend ID. For E300 it's the orbital position of the satellite
													// in tenth of a degree
	long							transponderID;	// new transponder ID (The Frequency in kHz)
	long							serviceID;		// new service ID (the ID of the used service as included in the DVB Stream)
	EyeTVPluginPIDInfo				*pidList;		// points to the list of active PIDs;
	long							pidCount;		// the length of pidList
}; /* 20 bytes */




/***********************************************************************************
*
*	EyeTVPluginParams
*		
***********************************************************************************/
typedef struct EyeTVPluginParams EyeTVPluginParams;
struct EyeTVPluginParams {
	EyeTVPluginDeviceID		deviceID;	// ID of the device
	EyeTVPluginSelector		selector;	// selector
	void					*refCon;	// refCon

	union {												
		EyeTVPluginInitializeParams			initialize;			// kEyeTVPluginSelector_Initialize
																// kEyeTVPluginSelector_Terminate, no additional parameters
		EyeTVPluginGetInfoParams			info;				// kEyeTVPluginSelector_GetInfo
		EyeTVPluginDeviceAddedParams		deviceAdded;		// kEyeTVPluginSelector_DeviceAdded
																// kEyeTVPluginSelector_DeviceRemoved, no additional parameters
		EyeTVPluginPacketsArrivedParams 	packetsArrived;		// kEyeTVPluginSelector_PacketsArrived
		EyeTVPluginServiceChangedParams		serviceChanged;		// kEyeTVPluginSelector_ServiceChanged
	
	};
};


enum {		// EyeTVPluginSelector
	kEyeTVPluginSelector_Initialize = 0,		
	kEyeTVPluginSelector_Terminate = 1,			
	kEyeTVPluginSelector_GetInfo = 2,			
	kEyeTVPluginSelector_DeviceAdded = 3,		
	kEyeTVPluginSelector_DeviceRemoved = 4,		
	kEyeTVPluginSelector_PacketsArrived = 5,	
	kEyeTVPluginSelector_ServiceChanged = 6	
};						



/***********************************************************************************
*
*	EyeTVPluginEntryProc
*
***********************************************************************************/
typedef long(* EyeTVPluginEntryProc)(EyeTVPluginParams *params);
