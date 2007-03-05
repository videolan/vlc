/*****************************************************************************
* eyetvplugin.c: Plug-In for the EyeTV software to connect to VLC
*****************************************************************************
* Copyright (C) 2006-2007 the VideoLAN team
* $Id$
*
* Authors: Felix Kühne <fkuehne at videolan dot org>
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

#include "eyetvplugin.h"

#define MAX_PIDS			256
#define MAX_ACTIVE_PIDS		256
#define MAX_DEVICES			16
#define VLC_NOTIFICATION_OBJECT "VLCEyeTVSupport"

#pragma push
#pragma pack(1)


/* Structure for TS-Packets */
typedef struct 
{
	unsigned long			sync_byte : 8,
							transport_error_indicator : 1,
							payload_unit_start_indicator : 1,
							transport_priority : 1,
							PID : 13,
							transport_scrambling_control : 2,
							adaptation_field_control : 2,
							continuity_counter : 4;

	unsigned char			data[188-4];

} TransportStreamPacket;

#pragma pop


/* Structure to hold Information on devices */
typedef struct
{
	EyeTVPluginDeviceID			deviceID;
	EyeTVPluginDeviceType		deviceType;

	long						headendID;
	long						transponderID;
	long						serviceID;

	long						pidsCount;
	long						pids[MAX_PIDS];

	EyeTVPluginPIDInfo			activePIDs[MAX_ACTIVE_PIDS];
	long						activePIDsCount;

} DeviceInfo;


/* Structure to hold global data to communicate with EyeTV */
typedef struct 
{
	EyeTVPluginCallbackProc			callback;
	long							deviceCount;
	DeviceInfo						devices[MAX_DEVICES];
	long long						packetCount;

} VLCEyeTVPluginGlobals_t;

/* 2nd structure to store our own global data which isn't shared with EyeTV
 * a bit empty at the moment, but it will get larger as development progresses */
typedef struct
{
    int                     i_deviceCount;
    CFMessagePortRef        messagePortToVLC;
    bool                    b_msgPortOpen;
} VLCEyeTVPluginOwnGlobals_t;

VLCEyeTVPluginOwnGlobals_t *nativeGlobals;


/* return the DeviceInfo with ID deviceID */
static DeviceInfo *GetDeviceInfo(VLCEyeTVPluginGlobals_t *globals, EyeTVPluginDeviceID deviceID)
{
	int i;
	
	if( globals ) 
	{
		for( i=0; i<globals->deviceCount; i++) 
		{
			if( globals->devices[i].deviceID == deviceID ) 
			{
				return &globals->devices[i];
			}
		}
	}

	return NULL;
}

#pragma mark -

/* initialise the plug-in */
static long VLCEyeTVPluginInitialize(VLCEyeTVPluginGlobals_t** globals, long apiVersion, EyeTVPluginCallbackProc callback)
{
	printf("VLC media player Plug-In: Initialize\n");
	long result = 0;
    
    /* init our own storage */
    extern VLCEyeTVPluginOwnGlobals_t *nativeGlobals;
    nativeGlobals = malloc( sizeof( VLCEyeTVPluginOwnGlobals_t ) );
    
    /* notify a potential VLC instance about our initialisation */
    CFNotificationCenterPostNotification( CFNotificationCenterGetDistributedCenter(),
                                          CFSTR("PluginInit"), 
                                          CFSTR(VLC_NOTIFICATION_OBJECT), 
                                          /*userInfo*/ NULL, 
                                          TRUE );
    
    /* init our notification support */
    CFNotificationCenterAddObserver( CFNotificationCenterGetDistributedCenter(),
                                     /* observer */ NULL, 
                                     /* callBack */ VLCEyeTVPluginGlobalNotificationReceived,
                                     /* name, NULL==all */ NULL,
                                     CFSTR(VLC_NOTIFICATION_OBJECT), 
                                     CFNotificationSuspensionBehaviorDeliverImmediately );
	
	*globals = (VLCEyeTVPluginGlobals_t *) calloc(1, sizeof( VLCEyeTVPluginGlobals_t ) );
	( *globals )->callback = callback;
		
	return result;
}

/* we will be terminated soon, clean up */
static long VLCEyeTVPluginTerminate(VLCEyeTVPluginGlobals_t *globals)
{
    extern VLCEyeTVPluginOwnGlobals_t *nativeGlobals;
    
	printf("VLC media player Plug-In: Terminate\n");
	
	long result = 0;
	
    /* notify a potential VLC instance about our termination */
    CFNotificationCenterPostNotification( CFNotificationCenterGetDistributedCenter (),
                                          CFSTR("PluginQuit"), 
                                          CFSTR(VLC_NOTIFICATION_OBJECT), 
                                          /*userInfo*/ NULL, 
                                          TRUE );
    
    /* remove us from the global notification centre */
    CFNotificationCenterRemoveEveryObserver( CFNotificationCenterGetDistributedCenter(),
                                             (void *)VLCEyeTVPluginGlobalNotificationReceived );
    
    /* invalidate and free msg port */
    if( nativeGlobals->messagePortToVLC )
    {
        CFMessagePortInvalidate( nativeGlobals->messagePortToVLC );
    	free( nativeGlobals->messagePortToVLC );
        printf( "msgport invalidated and freed\n" );
    }
    else
        printf( "no msgport to free\n" );
    
	if( globals ) 
	{
		free( globals );
	}

    if( nativeGlobals )
        free( nativeGlobals );
    
	return result;
}

/* called when EyeTV asks various stuff about us */
static long VLCEyeTVPluginGetInformation(VLCEyeTVPluginGlobals_t *globals, long* outAPIVersion, char* outName, char *outDescription)
{
	printf("VLC media player Plug-In: GetInfo\n");
	long result = 0;
	
	if( globals ) 
	{
		if( outAPIVersion )
		{
			*outAPIVersion = EYETV_PLUGIN_API_VERSION;
		}
		
		if( outName )
		{
			char* name = "VLC media player Plug-In";
			strcpy( &outName[0], name);
		}
		
		if( outDescription )
		{
			char* desc = "This Plug-In connects EyeTV to the VLC media player for streaming purposes.";
			strcpy( &outDescription[0], desc);
		}
	}
	
	return result;
}

/* called if we received a global notification */
void VLCEyeTVPluginGlobalNotificationReceived( CFNotificationCenterRef center, 
                                              void *observer, 
                                              CFStringRef name, 
                                              const void *object, 
                                              CFDictionaryRef userInfo )
{
    CFIndex maxlen;
    char *theName, *theObject;
    extern VLCEyeTVPluginOwnGlobals_t *nativeGlobals;

    maxlen = CFStringGetMaximumSizeForEncoding( CFStringGetLength( name ),
                                                kCFStringEncodingUTF8) + 1;
    theName = malloc(maxlen);
    CFStringGetCString( name, 
                        theName, 
                        maxlen,
                        kCFStringEncodingUTF8);
    
    maxlen = CFStringGetMaximumSizeForEncoding( CFStringGetLength( name ),
                                                kCFStringEncodingUTF8) + 1;
    theObject = malloc(maxlen);
    CFStringGetCString( object, 
                        theObject, 
                        maxlen,
                        kCFStringEncodingUTF8);
    printf( "notication received with name: %s and object: %s\n", theName, theObject );
    
    /* when VLC launches after us, we need to inform it about our existance and the current state of available devices */
    if( CFStringCompare( name, CFSTR( "VLCOSXGUIInit" ), 0) == kCFCompareEqualTo )
    {
        /* we're here */
        CFNotificationCenterPostNotification( CFNotificationCenterGetDistributedCenter (),
                                              CFSTR("PluginInit"), 
                                              CFSTR(VLC_NOTIFICATION_OBJECT), 
                                              /*userInfo*/ NULL, 
                                              TRUE );
        if( nativeGlobals && ( nativeGlobals->i_deviceCount > 0 ) )
        {
            /* at least one device is apparently connected */
            CFNotificationCenterPostNotification( CFNotificationCenterGetDistributedCenter (),
                                                  CFSTR("DeviceAdded"), 
                                                  CFSTR(VLC_NOTIFICATION_OBJECT), 
                                                  /*userInfo*/ NULL, 
                                                  TRUE );
        }
    }
    
    /* VLC wants us to start sending data */
    if( CFStringCompare( name, CFSTR( "VLCAccessStartDataSending" ), 0) == kCFCompareEqualTo )
    {
        nativeGlobals->messagePortToVLC = CFMessagePortCreateRemote( kCFAllocatorDefault,
                                                                     CFSTR("VLCEyeTVMsgPort") );
        if( nativeGlobals->messagePortToVLC == NULL )
            printf( "getting messagePortToVLC failed!\n" );
        else
        {
            nativeGlobals->b_msgPortOpen = TRUE;
            printf( "msg port opened / data sending switched on\n" );
        }
    }
    
    /* VLC wants us to stop sending data */
    if( CFStringCompare( name, CFSTR( "VLCAccessStopDataSending" ), 0) == kCFCompareEqualTo )
    {
        nativeGlobals->b_msgPortOpen = FALSE;
        printf( "data sending switched off\n" );
    }
}

/* called if a device is added */
static long VLCEyeTVPluginDeviceAdded(VLCEyeTVPluginGlobals_t *globals, EyeTVPluginDeviceID deviceID, EyeTVPluginDeviceType deviceType)
{
	printf("VLC media player Plug-In: Device with type %i and ID %i added\n", (int)deviceType, (int)deviceID);
    
	long result = 0;
	DeviceInfo *deviceInfo;
    extern VLCEyeTVPluginOwnGlobals_t *nativeGlobals;
    
	
	if( globals ) 
	{
		if( globals->deviceCount < MAX_DEVICES ) 
		{
			deviceInfo = &( globals->devices[globals->deviceCount] );
			memset(deviceInfo, 0, sizeof(DeviceInfo));
			
			deviceInfo->deviceID = deviceID;
			deviceInfo->deviceType = deviceType;

			globals->deviceCount++;

            if( nativeGlobals )
                nativeGlobals->i_deviceCount = globals->deviceCount;

            /* notify a potential VLC instance about the addition */
            CFNotificationCenterPostNotification( CFNotificationCenterGetDistributedCenter(),
                                                  CFSTR("DeviceAdded"), 
                                                  CFSTR(VLC_NOTIFICATION_OBJECT), 
                                                  /*userInfo*/ NULL, 
                                                  TRUE );
		}
	}

	return result;
}

/* called if a device is removed */
static long VLCEyeTVPluginDeviceRemoved(VLCEyeTVPluginGlobals_t *globals, EyeTVPluginDeviceID deviceID)
{
	printf("VLC media player Plug-In: DeviceRemoved\n");
    
    extern VLCEyeTVPluginOwnGlobals_t *nativeGlobals;
	long result = 0;
	int i;
	
	if( globals ) 
	{
		for( i = 0; i < globals->deviceCount; i++ )
		{
			if ( globals->devices[i].deviceID == deviceID ) 
			{
				globals->deviceCount--;

				if( i<globals->deviceCount )
				{
					globals->devices[i] = globals->devices[globals->deviceCount];
				}
                
                if( nativeGlobals )
                    nativeGlobals->i_deviceCount = globals->deviceCount;
                
                /* notify a potential VLC instance about the removal */
                CFNotificationCenterPostNotification( CFNotificationCenterGetDistributedCenter(),
                                                      CFSTR("DeviceRemoved"), 
                                                      CFSTR(VLC_NOTIFICATION_OBJECT), 
                                                      /*userInfo*/ NULL, 
                                                      TRUE );
			}
		}
	}
	
	return result;
}

/* This function is called, whenever packets are received by EyeTV. For reasons of performance,
 * the data is the original data, not a copy. That means, EyeTV waits until this method is 
 * finished. Therefore all in this method should be as fast as possible. */
int i=0;
static long VLCEyeTVPluginPacketsArrived(VLCEyeTVPluginGlobals_t *globals, EyeTVPluginDeviceID deviceID, long **packets, long packetsCount)
{
	long                                result = 0;
	int                                 i, j, isNewPID;
	TransportStreamPacket               *packet;
    extern VLCEyeTVPluginOwnGlobals_t   *nativeGlobals;
    SInt32                              i_returnValue;
    CFMutableDataRef                    theMutableRef;
    uint8_t                             *p_bufferForSending = malloc(4);
    bool                                b_nonSendData;
    int                                 i_lastSentPacket;
    
    if( globals && nativeGlobals ) 
	{
		DeviceInfo *deviceInfo = GetDeviceInfo(globals, deviceID);

		if( deviceInfo ) 
		{
            /* alloc the buffer if wanted */
            if( nativeGlobals->b_msgPortOpen == TRUE )
                theMutableRef = CFDataCreateMutable( kCFAllocatorDefault, (188) );
            
            for( i = 0; i < packetsCount; i++ ) 
			{
				packet = ( TransportStreamPacket* )packets[i];
				isNewPID = 1;
				
				/* search for PID */
				for( j = 0; j < deviceInfo->pidsCount; j++ ) 
				{
					if( packet->PID == deviceInfo->pids[j] ) 
					{
						isNewPID = 0;
						break;
					}
				}
                
				/* add new PIDs to the DeviceInfo */
				if( isNewPID ) 
				{
					printf ("VLC media player Plug-In: SamplePacketsArrived, newPID = %6d\n", packet->PID);
					
					if( deviceInfo->pidsCount < MAX_PIDS ) 
					{
						deviceInfo->pids[deviceInfo->pidsCount++] = packet->PID;
					}
				}
				else
                {
                    /* forward data to VLC if wanted */
                    /* FIXME: we only receive ARD for now */
                    if( nativeGlobals->b_msgPortOpen == TRUE && (
                        packet->PID == 1401 ||
                        packet->PID == 1402 ||
                        packet->PID == 1400 ||
                        packet->PID == 1404 ||
                        packet->PID == 3070 ||
                        packet->PID == 3072 ||
                        packet->PID == 3074 ||
                        packet->PID == 5074 ||
                        packet->PID == 0 ||
                        packet->PID == 17 ||
                        packet->PID == 19 ||
                        packet->PID == 20 ) )
                    {
                        /* in a good world, this wouldn't be necessary */
                        if( theMutableRef == NULL )
                            theMutableRef = CFDataCreateMutable( kCFAllocatorDefault, (188) );
                            
                        /* collect data to send larger packets */
                        
                        /* enlarge buffer if necessary */
                        if( i > 0 )
                            CFDataIncreaseLength( theMutableRef, 188 );
                        
                        /* add missing header */
                        memcpy( p_bufferForSending, packet, 4 );
                        CFDataAppendBytes( theMutableRef, p_bufferForSending, sizeof(p_bufferForSending) );

                        free( p_bufferForSending );
                        p_bufferForSending = malloc(4);
                        
                        /* add payload */
                        CFDataAppendBytes( theMutableRef, packet->data, sizeof(packet->data) );
                        
                        b_nonSendData = TRUE;
                        
                    }
                }
                
                globals->packetCount++;
                
                if( globals->packetCount%10000 == 0 ) 
                    printf("->  %lld Packets received so far...\n",globals->packetCount);
            }

            if( nativeGlobals->b_msgPortOpen == TRUE )
            {
                printf( "sending %i bytes of data\n", CFDataGetLength( theMutableRef ) );
                i_returnValue = CFMessagePortSendRequest( nativeGlobals->messagePortToVLC,
                                                          /* arbitrary int val */ globals->packetCount,
                                                          /* the data */ theMutableRef,
                                                          /* no timeout for sending */ 0,
                                                          /* no timeout for resp */ 0,
                                                          /* no resp. wanted */ NULL,
                                                          NULL );
                b_nonSendData = FALSE;
                i_lastSentPacket = globals->packetCount;
                if( i_returnValue == kCFMessagePortSendTimeout )
                    printf( "time out while sending\n" );
                else if( i_returnValue == kCFMessagePortReceiveTimeout )
                    printf( "time out while waiting for resp\n" );
                else if( i_returnValue == kCFMessagePortIsInvalid )
                {
                    /* suppress any further attemps */ 
                    printf( "message port is invalid!\n" );
                    nativeGlobals->b_msgPortOpen = FALSE;
                }
                else if( i_returnValue == kCFMessagePortTransportError ) 
                    printf( "transport error while sending!\n" );
                else
                {
                    //printf( "success, freeing resources\n" );
                    free( theMutableRef );
                    theMutableRef = CFDataCreateMutable( kCFAllocatorDefault, (188) );
                }
            }

        }
	}
    else
        printf( "warning: either globals or nativeGlobals are NIL in VLCEyeTVPluginPacketsArrived" );

    /* clean up before leaving function */
    //if( nativeGlobals->b_msgPortOpen == TRUE )
     //   free( theMutableRef );
    
    free( p_bufferForSending );
    
	return result;
}

/*	VLCEyeTVPluginServiceChanged,
 *
 *	- *globals		: The plug-in Globals
 *	- deviceID		: Identifies the active Device
 *   - headendID		: The HeadendID, for e300 it's the orbital position of the satelite in 
 *					  tenth degrees east
 *   - transponderID : The Frequency in kHz
 *   - serviceID		: original ServiceID from the DVB-Stream (e300, e400)
 *	- pidList		: List of active PIDs	
 *
 *	Whenever a service changes, this function is called. Service-related plug-in data should be updated here.
 */
static long VLCEyeTVPluginServiceChanged(VLCEyeTVPluginGlobals_t *globals, 
											EyeTVPluginDeviceID deviceID, 
											long headendID, 
											long transponderID, 
											long serviceID, 
											EyeTVPluginPIDInfo *pidList, 
											long pidsCount)
{
	long result = 0;
	int	i;
	
	printf("\nVLC media player Plug-In: ServiceChanged:\n");
	printf(  "=====================================\n");
	
	if( globals ) 
	{
		DeviceInfo *deviceInfo = GetDeviceInfo( globals, deviceID );
		if( deviceInfo ) 
		{
			deviceInfo->headendID = headendID;
			printf("HeadendID: %ld, ", headendID);
			
			deviceInfo->transponderID = transponderID;
			printf("TransponderID: %ld, ", transponderID);
			
			deviceInfo->serviceID = serviceID;
			printf("ServiceID: %ld\n\n", serviceID);
			
			deviceInfo->activePIDsCount = pidsCount;

			for( i = 0; i < pidsCount; i++ )
			{
				deviceInfo->activePIDs[i] = pidList[i];
				printf("Active PID: %ld, type: %ld\n", pidList[i].pid, pidList[i].pidType);
			}

			deviceInfo->pidsCount = 0;
			
		}
	}
	printf(  "=====================================\n");
	
    /* notify a potential VLC instance about the service change */
    CFNotificationCenterPostNotification( CFNotificationCenterGetDistributedCenter(),
                                          CFSTR("ServiceChanged"), 
                                          CFSTR(VLC_NOTIFICATION_OBJECT), 
                                          /*userInfo*/ NULL, 
                                          TRUE );
    
    return result;
}


#pragma mark -
/* EyeTVPluginDispatcher,
 *
 *	- selector : See 'EyeTVPluginDefs.h'
 *	- *refCon :  The RefCon to the plug-in-related Data
 *	- deviceID : Identifies the Device
 *	- params : Parameters for functioncall
 *
 * This function is a part of the interface for the communication with EyeTV. If something happens,
 * EyeTV thinks, we should know of, it calls this function with the corresponding selector. */

#pragma export on

long EyeTVPluginDispatcher( EyeTVPluginParams* params )
{
	long result = 0;

	switch( params->selector ) 
	{
		case kEyeTVPluginSelector_Initialize:
			result = VLCEyeTVPluginInitialize((VLCEyeTVPluginGlobals_t**)params->refCon, 
									params->initialize.apiVersion, params->initialize.callback);
			break;
			
		case kEyeTVPluginSelector_Terminate:
			result = VLCEyeTVPluginTerminate((VLCEyeTVPluginGlobals_t*)params->refCon);
			break;

		case kEyeTVPluginSelector_GetInfo:
			result = VLCEyeTVPluginGetInformation((VLCEyeTVPluginGlobals_t*)params->refCon, 
									params->info.pluginAPIVersion, params->info.pluginName, params->info.description);
			break;

		case kEyeTVPluginSelector_DeviceAdded:
			result = VLCEyeTVPluginDeviceAdded((VLCEyeTVPluginGlobals_t*)params->refCon, 
									params->deviceID, params->deviceAdded.deviceType);
			break;
		
		case kEyeTVPluginSelector_DeviceRemoved:
			result = VLCEyeTVPluginDeviceRemoved((VLCEyeTVPluginGlobals_t*)params->refCon, params->deviceID);
			break;

		case kEyeTVPluginSelector_PacketsArrived:
			result = VLCEyeTVPluginPacketsArrived((VLCEyeTVPluginGlobals_t*)params->refCon, params->deviceID, 
									params->packetsArrived.packets, params->packetsArrived.packetCount);
			break;

		case kEyeTVPluginSelector_ServiceChanged:
			result = VLCEyeTVPluginServiceChanged((VLCEyeTVPluginGlobals_t*)params->refCon, 
									params->deviceID, params->serviceChanged.headendID, 
									params->serviceChanged.transponderID, params->serviceChanged.serviceID, 
									params->serviceChanged.pidList, params->serviceChanged.pidCount);
			break;
	}

	return result;
}
