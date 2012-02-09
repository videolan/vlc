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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_PIDS            256
#define MAX_ACTIVE_PIDS     256
#define MAX_DEVICES         16
#define VLC_NOTIFICATION_OBJECT "VLCEyeTVSupport"

#pragma push
#pragma pack(1)

/* Structure for TS-Packets */
typedef struct 
{
    uint32_t    sync_byte : 8,
                transport_error_indicator : 1,
                payload_unit_start_indicator : 1,
                transport_priority : 1,
                PID : 13,
                transport_scrambling_control : 2,
                adaptation_field_control : 2,
                continuity_counter : 4;
    uint8_t     payload[184];

} TransportStreamPacket;

#pragma pop


/* Structure to hold global data to communicate with EyeTV */
typedef struct 
{
    EyeTVPluginCallbackProc     callback;
    /* Structure to hold current active service */
    EyeTVPluginDeviceID         activeDeviceID;
    long                        activePIDsCount; 
    EyeTVPluginPIDInfo          activePIDs[MAX_ACTIVE_PIDS];
} VLCEyeTVPluginGlobals_t;

/* following globals limits us to one VLC instance using EyeTV */
static int i_deviceCount;
static int i_vlcSock;

#pragma mark -

/* initialise the plug-in */
static long VLCEyeTVPluginInitialize(VLCEyeTVPluginGlobals_t** globals, long apiVersion, EyeTVPluginCallbackProc callback)
{
    printf("VLC media player Plug-In: Initialize\n");
    long result = 0;
    
    /* init our own storage */
    i_deviceCount = 0;
    i_vlcSock = -1;
    
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
    long result = 0;
    
    printf("VLC media player Plug-In: Terminate\n");
    
    /* notify a potential VLC instance about our termination */
    CFNotificationCenterPostNotification( CFNotificationCenterGetDistributedCenter (),
                                          CFSTR("PluginQuit"), 
                                          CFSTR(VLC_NOTIFICATION_OBJECT), 
                                          /*userInfo*/ NULL, 
                                          TRUE );
    
    /* remove us from the global notification centre */
    CFNotificationCenterRemoveEveryObserver( CFNotificationCenterGetDistributedCenter(),
                                             (void *)VLCEyeTVPluginGlobalNotificationReceived );
    
    /* close data connection */
    if( i_vlcSock != -1 )
    {
        close( i_vlcSock );
        i_vlcSock = -1;
    }
    
    free( globals );
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
            strcpy( outName, "VLC media player Plug-In");
        }
        
        if( outDescription )
        {
            strcpy( outDescription, "This Plug-In connects EyeTV to the VLC media player for streaming purposes.");
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
    /* when VLC launches after us, we need to inform it about our existance and the current state of available devices */
    if( CFStringCompare( name, CFSTR( "VLCOSXGUIInit" ), 0) == kCFCompareEqualTo )
    {
        /* we're here */
        CFNotificationCenterPostNotification( CFNotificationCenterGetDistributedCenter (),
                                              CFSTR("PluginInit"), 
                                              CFSTR(VLC_NOTIFICATION_OBJECT), 
                                              /*userInfo*/ NULL, 
                                              TRUE );
        if( i_deviceCount > 0 )
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
        if( i_vlcSock == -1 )
        {
            int peerSock;
         
            /* set-up data socket */
            peerSock = socket(AF_UNIX, SOCK_STREAM, 0);
            if( peerSock != -1 )
            {
                struct sockaddr_un peerAddr;
                /* set-up connection address */
                memset(&peerAddr, 0, sizeof(peerAddr));
                peerAddr.sun_family = AF_UNIX;
                strncpy(peerAddr.sun_path, "/tmp/.vlc-eyetv-bridge", sizeof(peerAddr.sun_path)-1);
                
                /* connect */
                printf("data connect in progess...\n");
                if( connect(peerSock, (struct sockaddr *)&peerAddr, sizeof(struct sockaddr_un)) != -1 )
                {
                    printf("data sending switched on\n");
					
                    i_vlcSock = peerSock;
                }
                else
                    printf("connect data socket failed (errno=%d)\n", errno );
            }
            else
                printf("create data socket failed (errno=%d)\n", errno );
        }
    }
    
    /* VLC wants us to stop sending data */
    if( CFStringCompare( name, CFSTR( "VLCAccessStopDataSending" ), 0) == kCFCompareEqualTo )
    {
        if( i_vlcSock != -1 )
        {
            close( i_vlcSock );
            i_vlcSock = -1;
            printf( "data sending switched off\n" );
        }
    }
}

/* called if a device is added */
static long VLCEyeTVPluginDeviceAdded(VLCEyeTVPluginGlobals_t *globals, EyeTVPluginDeviceID deviceID, EyeTVPluginDeviceType deviceType)
{
    printf("VLC media player Plug-In: Device with type %i and ID %i added\n", (int)deviceType, (int)deviceID);
    
    long result = 0;
    
    if( globals ) 
    {
        ++i_deviceCount;
        if( 1 == i_deviceCount )
        {                
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
    
    long result = 0;
        
    --i_deviceCount;
    if( 0 == i_deviceCount )
    {                
        /* notify a potential VLC instance about the removal */
        CFNotificationCenterPostNotification( CFNotificationCenterGetDistributedCenter(),
                                              CFSTR("DeviceRemoved"), 
                                              CFSTR(VLC_NOTIFICATION_OBJECT), 
                                              /*userInfo*/ NULL, 
                                              TRUE );
    }
    if( (i_vlcSock != -1) && (deviceID == globals->activeDeviceID) )
    {
        close(i_vlcSock);
        i_vlcSock = -1;
        printf( "data sending switched off\n" );
    }
    
    return result;
}

/* This function is called, whenever packets are received by EyeTV. For reasons of performance,
 * the data is the original data, not a copy. That means, EyeTV waits until this method is 
 * finished. Therefore all in this method should be as fast as possible. */
static long VLCEyeTVPluginPacketsArrived(VLCEyeTVPluginGlobals_t *globals, EyeTVPluginDeviceID deviceID, long **packets, long packetsCount)
{
    if( globals ) 
    {
        /* check if data connection is active */
        if( i_vlcSock != -1 )
        {
            if( deviceID == globals->activeDeviceID ) 
            {
                long pidCount = globals->activePIDsCount;
                if( pidCount )
                {
                    uint8_t packetBuffer[sizeof(TransportStreamPacket)*20];
                    int packetBufferSize = 0;
                    while( packetsCount )
                    {
                        /* apply PID filtering, only PIDs in active service for device are sent through */
                        long pid = ntohl(**packets)>>8 & 0x1FFFL;
                        /* ignore NULL packets */
                        if( 0x1FFFL != pid )
                        {
                            long i;
                            for( i=0; i<pidCount; ++i )
                            {
                                if( globals->activePIDs[i].pid == pid )
                                {
                                    memcpy(packetBuffer+packetBufferSize, *packets, sizeof(TransportStreamPacket));
                                    packetBufferSize += sizeof(TransportStreamPacket);
                                    if( packetBufferSize > (sizeof(packetBuffer)-sizeof(TransportStreamPacket)) )
                                    {
                                        /* flush buffer to VLC */
                                        ssize_t sent = write(i_vlcSock, packetBuffer, packetBufferSize);
                                        if( sent != packetBufferSize )
                                        {
                                            if( sent == -1 )
                                                printf("data sending failed (errno=%d)\n", errno);
                                            else
                                                printf("data sending incomplete (sent=%zd)\n", sent);
                                            close(i_vlcSock);
                                            i_vlcSock = -1;
                                            return 0;
                                        }
                                        packetBufferSize = 0;
                                    }

                                    if( i > 0 )
                                    {
                                       /* if we assume that consecutive packets would have the same PID in most cases,
                                          it would therefore speed up filtering to reorder activePIDs list based on pid
                                          occurrences */
                                        EyeTVPluginPIDInfo swap = globals->activePIDs[i];
                                        do
                                        {
                                            register int c = i--;
                                            globals->activePIDs[c] = globals->activePIDs[i];
                                        }
                                        while( i );
                                        globals->activePIDs[i] = swap;
                                    }

                                    if( pid && globals->activePIDs[0].pidType != kEyeTVPIDType_PMT )
                                    {
                                        /* to save on CPU, prevent EyeTV from mirroring that program by blocking video & audio packets
                                           by changing all packets but PAT and PMT to NULL PID */
#if defined(WORDS_BIGENDIAN)
                                        **packets |= 0x001FFF00L;
#else
                                        **packets |= 0x00FFF800L;
#endif
                                    }
                                    /* done filtering on this packet, move on to next packet */
                                    break;
                                }
                            }
                        }
                        --packetsCount;
                        ++packets;
                    }
                    if( packetBufferSize )
                    {
                        /* flush buffer to VLC */
                        ssize_t sent = write(i_vlcSock, packetBuffer, packetBufferSize);
                        if( sent != packetBufferSize )
                        {
                            if( sent == -1 )
                                printf("data sending failed (errno=%d)\n", errno);
                            else
                                printf("data sending incomplete (sent=%zd)\n", sent);
                            close(i_vlcSock);
                            i_vlcSock = -1;
                            return 0;
                        }
                    }
                }
            }
        }
    }
    return 0;
}

/*  VLCEyeTVPluginServiceChanged,
 *
 *  - *globals      : The plug-in Globals
 *  - deviceID      : Identifies the active Device
 *   - headendID        : The HeadendID, for e300 it's the orbital position of the satelite in 
 *                    tenth degrees east
 *   - transponderID : The Frequency in kHz
 *   - serviceID        : original ServiceID from the DVB-Stream (e300, e400)
 *  - pidList       : List of active PIDs   
 *
 *  Whenever a service changes, this function is called. Service-related plug-in data should be updated here.
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
    int i;
    
    printf("\nVLC media player Plug-In: ServiceChanged:\n");
    printf(  "=====================================\n");
    
    if( globals ) 
    {
        printf("DeviceID: %ld, ", (long)deviceID);
        printf("HeadendID: %ld, ", headendID);
        printf("TransponderID: %ld, ", transponderID);
        printf("ServiceID: %ld\n\n", serviceID);
        
        globals->activeDeviceID = deviceID;
        globals->activePIDsCount = pidsCount;

        /* need active PIDs for packet filtering */
        for( i = 0; i < pidsCount; i++ )
        {
            globals->activePIDs[i] = pidList[i];
            printf("Active PID: %ld, type: %ld\n", pidList[i].pid, pidList[i].pidType);
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
 *  - selector : See 'EyeTVPluginDefs.h'
 *  - *refCon :  The RefCon to the plug-in-related Data
 *  - deviceID : Identifies the Device
 *  - params : Parameters for functioncall
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
