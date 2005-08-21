/* this file contains the actual definitions of */
/* the IIDs and CLSIDs */

/* link this file in with the server and any clients */


/* File created by MIDL compiler version 5.01.0164 */
/* at Sun Aug 21 17:16:22 2005
 */
/* Compiler settings for axvlc.idl:
    Oicf (OptLev=i2), W1, Zp8, env=Win32, ms_ext, c_ext
    error checks: allocation ref bounds_check enum stub_data 
*/
//@@MIDL_FILE_HEADING(  )
#ifdef __cplusplus
extern "C"{
#endif 


#ifndef __IID_DEFINED__
#define __IID_DEFINED__

typedef struct _IID
{
    unsigned long x;
    unsigned short s1;
    unsigned short s2;
    unsigned char  c[8];
} IID;

#endif // __IID_DEFINED__

#ifndef CLSID_DEFINED
#define CLSID_DEFINED
typedef IID CLSID;
#endif // CLSID_DEFINED

const IID LIBID_AXVLC = {0xDF2BBE39,0x40A8,0x433b,{0xA2,0x79,0x07,0x3F,0x48,0xDA,0x94,0xB6}};


const IID IID_IVLCControl = {0xC2FA41D0,0xB113,0x476e,{0xAC,0x8C,0x9B,0xD1,0x49,0x99,0xC1,0xC1}};


const IID DIID_DVLCEvents = {0xDF48072F,0x5EF8,0x434e,{0x9B,0x40,0xE2,0xF3,0xAE,0x75,0x9B,0x5F}};


const CLSID CLSID_VLCPlugin = {0xE23FE9C6,0x778E,0x49D4,{0xB5,0x37,0x38,0xFC,0xDE,0x48,0x87,0xD8}};


#ifdef __cplusplus
}
#endif

