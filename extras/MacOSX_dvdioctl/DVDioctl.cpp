/*****************************************************************************
 * DVDioctl.cpp: Linux-like DVD driver for Darwin and MacOS X
 *****************************************************************************
 * Copyright (C) 1998-2000 Apple Computer, Inc. All rights reserved.
 * Copyright (C) 2001 VideoLAN
 * $Id: DVDioctl.cpp,v 1.3 2001/04/04 16:33:07 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *****************************************************************************/

/*****************************************************************************
 * TODO:
 * - add a timeout to waitForService() so that we don't wait forever
 * - find a way to prevent user from ejecting DVD using the GUI while
 *   it is still in use
 *****************************************************************************/

//XXX: uncomment to activate the key exchange ioctls - may hang the machine
//#define ACTIVATE_DANGEROUS_IOCTL 1

/*****************************************************************************
 * Preamble
 *****************************************************************************/
extern "C"
{
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/ioccom.h>
#include <sys/fcntl.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <miscfs/devfs/devfs.h>

#include <mach/mach_types.h>
}

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IODVDMedia.h>
#include <IOKit/storage/IODVDBlockStorageDriver.h>

#include "DVDioctl.h"

/*****************************************************************************
 * Driver class
 *****************************************************************************/
class DVDioctl : public IOService
{
    OSDeclareDefaultStructors( DVDioctl )

public:

    virtual bool       init   ( OSDictionary *dictionary = 0 );
    virtual IOService *probe  ( IOService *provider, SInt32 *score );
    virtual bool       start  ( IOService *provider );
    virtual void       stop   ( IOService *provider );
    virtual void       free   ( void );
};

#define super IOService
OSDefineMetaClassAndStructors( DVDioctl, IOService )

/*****************************************************************************
 * Variable typedefs
 *****************************************************************************/
typedef enum       { DKRTYPE_BUF, DKRTYPE_DIO }      dkrtype_t;
typedef struct dio { dev_t dev; struct uio * uio; }  dio_t;
typedef struct buf                                   buf_t;
typedef void *                                       dkr_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  DVDClose        ( dev_t, int, int, struct proc * );
static int  DVDBlockIoctl   ( dev_t, u_long, caddr_t, int, struct proc * );
static int  DVDOpen         ( dev_t, int, int, struct proc * );
static int  DVDSize         ( dev_t );
static void DVDStrategy     ( buf_t * );
static int  DVDReadWrite    ( dkr_t, dkrtype_t );
static void DVDReadWriteCompletion( void *, void *, IOReturn, UInt64 );

static struct bdevsw device_functions =
{
    DVDOpen, DVDClose, DVDStrategy, DVDBlockIoctl, eno_dump, DVDSize, D_DISK
};

/*****************************************************************************
 * Local variables
 *****************************************************************************/
static DVDioctl * p_this = NULL;

static bool b_inuse;
static int i_major;
static void *p_node;
static IODVDMedia *p_dvd;
static IODVDBlockStorageDriver *p_drive;

/*****************************************************************************
 * DKR_GET_DEV: borrowed from IOMediaBSDClient.cpp
 *****************************************************************************/
static inline dev_t DKR_GET_DEV(dkr_t dkr, dkrtype_t dkrtype)
{
    return (dkrtype == DKRTYPE_BUF)
           ? ((buf_t *)dkr)->b_dev : ((dio_t *)dkr)->dev;
}

/*****************************************************************************
 * DKR_GET_BYTE_COUNT: borrowed from IOMediaBSDClient.cpp
 *****************************************************************************/
static inline UInt64 DKR_GET_BYTE_COUNT(dkr_t dkr, dkrtype_t dkrtype)
{
    return (dkrtype == DKRTYPE_BUF)
           ? ((buf_t *)dkr)->b_bcount : ((dio_t *)dkr)->uio->uio_resid;
}

/*****************************************************************************
 * DKR_GET_BYTE_START: borrowed from IOMediaBSDClient.cpp
 *****************************************************************************/
static inline UInt64 DKR_GET_BYTE_START(dkr_t dkr, dkrtype_t dkrtype)
{
    if (dkrtype == DKRTYPE_BUF)
    {
        buf_t * bp    = (buf_t *)dkr;
        return bp->b_blkno * p_dvd->getPreferredBlockSize();
    }
    return ((dio_t *)dkr)->uio->uio_offset;
}

/*****************************************************************************
 * DKR_IS_READ: borrowed from IOMediaBSDClient.cpp
 *****************************************************************************/
static inline bool DKR_IS_READ(dkr_t dkr, dkrtype_t dkrtype)
{
    return (dkrtype == DKRTYPE_BUF)
           ? ((((buf_t *)dkr)->b_flags & B_READ) == B_READ)
           : ((((dio_t *)dkr)->uio->uio_rw) == UIO_READ);
}

/*****************************************************************************
 * DKR_IS_ASYNCHRONOUS: borrowed from IOMediaBSDClient.cpp
 *****************************************************************************/
static inline bool DKR_IS_ASYNCHRONOUS(dkr_t dkr, dkrtype_t dkrtype)
{
    return (dkrtype == DKRTYPE_BUF) ? true : false;
}

/*****************************************************************************
 * DKR_IS_RAW: borrowed from IOMediaBSDClient.cpp
 *****************************************************************************/
static inline bool DKR_IS_RAW(dkr_t dkr, dkrtype_t dkrtype)
{
    return (dkrtype == DKRTYPE_BUF) ? false : true;
}

/*****************************************************************************
 * DKR_SET_BYTE_COUNT: borrowed from IOMediaBSDClient.cpp
 *****************************************************************************/
static inline void DKR_SET_BYTE_COUNT(dkr_t dkr, dkrtype_t dkrtype, UInt64 bcount)
{
    if (dkrtype == DKRTYPE_BUF)
    {
        ((buf_t *)dkr)->b_resid = ((buf_t *)dkr)->b_bcount - bcount;
    }
    else
    {
        ((dio_t *)dkr)->uio->uio_resid -= bcount;
    }
}

/*****************************************************************************
 * DKR_RUN_COMPLETION: borrowed from IOMediaBSDClient.cpp
 *****************************************************************************/
static inline void DKR_RUN_COMPLETION(dkr_t dkr, dkrtype_t dkrtype, IOReturn status)
{
    if (dkrtype == DKRTYPE_BUF)
    {
        buf_t * bp = (buf_t *)dkr;

        bp->b_error  = p_this->errnoFromReturn(status);
        bp->b_flags |= (status != kIOReturnSuccess) ? B_ERROR : 0;
        biodone(bp);
    }
}

/*****************************************************************************
 * DKR_GET_BUFFER: borrowed from IOMediaBSDClient.cpp
 *****************************************************************************/
static inline IOMemoryDescriptor * DKR_GET_BUFFER(dkr_t dkr, dkrtype_t dkrtype)
{
    if (dkrtype == DKRTYPE_BUF)
    {
        buf_t * bp = (buf_t *)dkr;

        if ( (bp->b_flags & B_VECTORLIST) )
        {
            assert(sizeof(IOPhysicalRange         ) == sizeof(iovec          ));
            assert(sizeof(IOPhysicalRange::address) == sizeof(iovec::iov_base));
            assert(sizeof(IOPhysicalRange::length ) == sizeof(iovec::iov_len ));
            return IOMemoryDescriptor::withPhysicalRanges(
              (IOPhysicalRange *) bp->b_vectorlist,
              (UInt32)            bp->b_vectorcount,
              (bp->b_flags & B_READ) ? kIODirectionIn : kIODirectionOut,
              true );
        }

        return IOMemoryDescriptor::withAddress(
          (vm_address_t) bp->b_data,
          (vm_size_t)    bp->b_bcount,
          (bp->b_flags & B_READ) ? kIODirectionIn : kIODirectionOut,
          (bp->b_flags & B_PHYS) ? current_task() : kernel_task );
    }
    else
    {
        struct uio * uio = ((dio_t *)dkr)->uio;

        assert(sizeof(IOVirtualRange         ) == sizeof(iovec          ));
        assert(sizeof(IOVirtualRange::address) == sizeof(iovec::iov_base));
        assert(sizeof(IOVirtualRange::length ) == sizeof(iovec::iov_len ));

        return IOMemoryDescriptor::withRanges(
        (IOVirtualRange *) uio->uio_iov,
        (UInt32)           uio->uio_iovcnt,
        (uio->uio_rw     == UIO_READ    ) ? kIODirectionIn : kIODirectionOut,
        (uio->uio_segflg != UIO_SYSSPACE) ? current_task() : kernel_task,
        true );
    }
}

/*****************************************************************************
 * DVDioctl::init: initialize the driver structure
 *****************************************************************************/
bool DVDioctl::init( OSDictionary *p_dict = 0 )
{
    //IOLog( "DVD ioctl: initializing\n" );

    p_this = this;

    p_node  = NULL;
    p_dvd   = NULL;
    p_drive = NULL;
    i_major = -1;
    b_inuse = false;

    bool res = super::init( p_dict );

    return res;
}
    
/*****************************************************************************
 * DVDioctl::probe: check whether the driver can be safely activated
 *****************************************************************************/
IOService * DVDioctl::probe( IOService *provider, SInt32 *score )
{
    //IOLog( "DVD ioctl: probing\n" );
    IOService * res = super::probe( provider, score );

    return res;
}

/*****************************************************************************
 * DVDioctl::start: start the driver
 *****************************************************************************/
bool DVDioctl::start( IOService *provider )
{
    //IOLog( "DVD ioctl: starting\n" );

    if( !super::start( provider ) )
    {
        return false;
    }

    //IOLog( "DVD ioctl: creating device\n" );

    i_major = bdevsw_add( -1, &device_functions );

    if( i_major == -1 )
    {
        //log(LOG_INFO, "DVD ioctl: failed to allocate a major number\n");
        return false;
    }

    p_node = devfs_make_node ( makedev( i_major, 0 ), DEVFS_BLOCK,
                               UID_ROOT, GID_WHEEL, 0666, "dvd" );

    if( p_node == NULL )
    {
        //log( LOG_INFO, "DVD ioctl: failed creating node\n" );

        if( bdevsw_remove(i_major, &device_functions) == -1 )
        {
            //log( LOG_INFO, "DVD ioctl: bdevsw_remove failed\n" );
        }

        return false;
    }

    return true;
}

/*****************************************************************************
 * DVDioctl::stop: stop the driver
 *****************************************************************************/
void DVDioctl::stop( IOService *provider )
{
    //IOLog( "DVD ioctl: removing device\n" );

    if( p_node != NULL )
    {
        devfs_remove( p_node );
    }

    if( i_major != -1 )
    {
        if( bdevsw_remove(i_major, &device_functions) == -1 )
        {
            //log( LOG_INFO, "DVD ioctl: bdevsw_remove failed\n" );
        }
    }

    //IOLog( "DVD ioctl: stopping\n" );
    super::stop( provider );
}
  
/*****************************************************************************
 * DVDioctl::free: free all resources allocated by the driver
 *****************************************************************************/
void DVDioctl::free( void )
{
    //IOLog( "DVD ioctl: freeing\n" );
    super::free( );
}

/* following functions are local */

/*****************************************************************************
 * DVDOpen: look for an IODVDMedia object and open it
 *****************************************************************************/
static int DVDOpen( dev_t dev, int flags, int devtype, struct proc * )
{
    IOStorageAccess level;

    /* Check that the device hasn't already been opened */
    if( b_inuse )
    {
        //log( LOG_INFO, "DVD ioctl: already opened\n" );
        return EBUSY;
    }
    else
    {
        b_inuse = true;
    }

    IOService * p_root = IOService::getServiceRoot();
   
    if( p_root == NULL )
    {
        //log( LOG_INFO, "DVD ioctl: couldn't find root\n" );
        b_inuse = false;
        return ENXIO;
    }

    OSDictionary * p_dict = p_root->serviceMatching( kIODVDMediaClass );

    if( p_dict == NULL )
    {
        //log( LOG_INFO, "DVD ioctl: couldn't find dictionary\n" );
        b_inuse = false;
        return ENXIO;
    }

    p_dvd = OSDynamicCast( IODVDMedia, p_root->waitForService( p_dict ) );

    if( p_dvd == NULL )
    {
        //log( LOG_INFO, "DVD ioctl: couldn't find service\n" );
        b_inuse = false;
        return ENXIO;
    }

    //log( LOG_INFO, "DVD ioctl: found DVD\n" );

    level = (flags & FWRITE) ? kIOStorageAccessReaderWriter
                             : kIOStorageAccessReader;

    if( ! p_dvd->open( p_this, 0, level) )
    {
        log( LOG_INFO, "DVD ioctl: IODVDMedia object busy\n" );
        b_inuse = false;
        return EBUSY;
    }

    p_drive = p_dvd->getProvider();

    log( LOG_INFO, "DVD ioctl: IODVDMedia->open()\n" );

    return 0;
}

/*****************************************************************************
 * DVDClose: close the IODVDMedia object
 *****************************************************************************/
static int DVDClose( dev_t dev, int flags, int devtype, struct proc * )
{
    /* Release the device */
    p_dvd->close( p_this );

    p_dvd   = NULL;
    p_drive = NULL;
    b_inuse = false;

    log( LOG_INFO, "DVD ioctl: IODVDMedia->close()\n" );

    return 0;
}

/*****************************************************************************
 * DVDSize: return the device size
 *****************************************************************************/
static int DVDSize( dev_t dev )
{
    return p_dvd->getPreferredBlockSize();
}

/*****************************************************************************
 * DVDStrategy: perform read or write operations
 *****************************************************************************/
static void DVDStrategy( buf_t * bp )
{
    DVDReadWrite(bp, DKRTYPE_BUF);
    return;
}

/*****************************************************************************
 * DVDBlockIoctl: issue an ioctl on the block device
 *****************************************************************************/
static int DVDBlockIoctl( dev_t dev, u_long cmd, caddr_t addr, int flags,
                          struct proc *p )
{
    dvdioctl_data_t * p_data = (dvdioctl_data_t *)addr;

    switch( cmd )
    {
        case IODVD_READ_STRUCTURE:

            log( LOG_INFO, "DVD ioctl: IODVD_READ_STRUCTURE\n" );

            return 0;

        case IODVD_SEND_KEY:

            log( LOG_INFO, "DVD ioctl: send key to `%s', "
                 "buf %d, format %d, class %d, agid %d\n",
                 p_drive->getDeviceTypeName(),
                 (int)p_data->p_buffer, p_data->i_keyformat,
                 p_data->i_keyclass, p_data->i_agid );

#ifdef ACTIVATE_DANGEROUS_IOCTL
            return p_drive->sendKey( (IOMemoryDescriptor *)p_data->p_buffer,
                                     (DVDKeyClass)p_data->i_keyclass,
                                     p_data->i_agid,
                                     (DVDKeyFormat)p_data->i_keyformat );
#else
            return -1;
#endif

        case IODVD_REPORT_KEY:

            log( LOG_INFO, "DVD ioctl: report key from `%s', "
                 p_drive->getDeviceTypeName(),
                 "buf %d, class %d, lba %d, agid %d, format %d\n",
                 (int)p_data->p_buffer, p_data->i_keyclass, p_data->i_lba,
                 p_data->i_agid, p_data->i_keyformat );

#ifdef ACTIVATE_DANGEROUS_IOCTL
            return p_drive->reportKey( (IOMemoryDescriptor *)p_data->p_buffer,
                                       (DVDKeyClass)p_data->i_keyclass,
                                       p_data->i_lba, p_data->i_agid,
                                       (DVDKeyFormat)p_data->i_keyformat );
#else
            return -1;
#endif

        default:

            log( LOG_INFO, "DVD ioctl: unknown ioctl\n" );

            return EINVAL;
    }
}

/*****************************************************************************
 * DVDReadWrite: borrowed from IOMediaBSDClient.cpp
 *****************************************************************************/
static int DVDReadWrite(dkr_t dkr, dkrtype_t dkrtype)
{
    IOMemoryDescriptor * buffer;
    register UInt64      byteCount;
    register UInt64      byteStart;
    UInt64               mediaSize;
    IOReturn             status;

    byteCount = DKR_GET_BYTE_COUNT(dkr, dkrtype);
    byteStart = DKR_GET_BYTE_START(dkr, dkrtype);
    mediaSize = p_dvd->getSize();

    if ( byteStart >= mediaSize )
    {
        status = DKR_IS_READ(dkr,dkrtype) ? kIOReturnSuccess : kIOReturnIOError;        goto dkreadwriteErr;
    }

    if ( DKR_IS_RAW(dkr, dkrtype) )
    {
        UInt64 mediaBlockSize = p_dvd->getPreferredBlockSize();

        if ( (byteStart % mediaBlockSize) || (byteCount % mediaBlockSize) )
        {
            status = kIOReturnNotAligned;
            goto dkreadwriteErr;
        }
    }

    buffer = DKR_GET_BUFFER(dkr, dkrtype);

    if ( buffer == 0 )
    {
        status = kIOReturnNoMemory;
        goto dkreadwriteErr;
    }

    if ( byteCount > mediaSize - byteStart )
    {
        IOMemoryDescriptor * originalBuffer = buffer;

        buffer = IOMemoryDescriptor::withSubRange( originalBuffer, 0,
                     mediaSize - byteStart, originalBuffer->getDirection() );
        originalBuffer->release();
        if ( buffer == 0 )
        {
            status = kIOReturnNoMemory;
            goto dkreadwriteErr;
        }
    }

    if ( DKR_IS_ASYNCHRONOUS(dkr, dkrtype) )
    {
        IOStorageCompletion completion;

        completion.target    = dkr;
        completion.action    = DVDReadWriteCompletion;
        completion.parameter = (void *) dkrtype;

        if ( DKR_IS_READ(dkr, dkrtype) )
        {
            p_dvd->read(  p_this, byteStart, buffer, completion );
        }
        else
        {
            p_dvd->write( p_this, byteStart, buffer, completion );
        }

        status = kIOReturnSuccess;
    }
    else
    {
        if ( DKR_IS_READ(dkr, dkrtype) )
        {
            status = p_dvd->IOStorage::read( p_this, byteStart,
                                             buffer, &byteCount );
        }
        else
        {
            status = p_dvd->IOStorage::write( p_this, byteStart,
                                              buffer, &byteCount );
        }

        DVDReadWriteCompletion(dkr, (void *)dkrtype, status, byteCount);
    }

    buffer->release();
    return p_this->errnoFromReturn(status);
dkreadwriteErr:

    DVDReadWriteCompletion(dkr, (void *)dkrtype, status, 0);

    return p_this->errnoFromReturn(status);
}

/*****************************************************************************
 * DVDReadWriteCompletion: borrowed from IOMediaBSDClient.cpp
 *****************************************************************************/
static void DVDReadWriteCompletion( void *   target,
                                    void *   parameter,
                                    IOReturn status,
                                    UInt64   actualByteCount )
{
    dkr_t     dkr      = (dkr_t) target;
    dkrtype_t dkrtype  = (dkrtype_t) (int) parameter;
    dev_t     dev      = DKR_GET_DEV(dkr, dkrtype);

    if ( status != kIOReturnSuccess )
    {
        IOLog( "DVD ioctl: %s (is the disc authenticated ?)\n",
               p_this->stringFromReturn(status) );
    }

    DKR_SET_BYTE_COUNT(dkr, dkrtype, actualByteCount);
    DKR_RUN_COMPLETION(dkr, dkrtype, status);
}

