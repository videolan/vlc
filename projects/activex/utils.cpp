/*****************************************************************************
 * utils.cpp: ActiveX control for VLC
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 *
 * Authors: Damien Fouilleul <Damien.Fouilleul@laposte.net>
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

#include "utils.h"

#include <wchar.h>
#include <wctype.h>

/*
** conversion facilities
*/

using namespace std;

char *CStrFromWSTR(UINT codePage, LPCWSTR wstr, UINT len)
{
    if( len > 0 )
    {
        size_t mblen = WideCharToMultiByte(codePage,
                0, wstr, len, NULL, 0, NULL, NULL);
        if( mblen > 0 )
        {
            char *buffer = (char *)CoTaskMemAlloc(mblen+1);
            ZeroMemory(buffer, mblen+1);
            if( WideCharToMultiByte(codePage, 0, wstr, len, buffer, mblen, NULL, NULL) )
            {
                buffer[mblen] = '\0';
                return buffer;
            }
        }
    }
    return NULL;
};

char *CStrFromBSTR(UINT codePage, BSTR bstr)
{
    return CStrFromWSTR(codePage, bstr, SysStringLen(bstr));
};

BSTR BSTRFromCStr(UINT codePage, LPCSTR s)
{
    int wideLen = MultiByteToWideChar(codePage, 0, s, -1, NULL, 0);
    if( wideLen > 0 )
    {
        WCHAR* wideStr = (WCHAR*)CoTaskMemAlloc(wideLen*sizeof(WCHAR));
        if( NULL != wideStr )
        {
            BSTR bstr;

            ZeroMemory(wideStr, wideLen*sizeof(WCHAR));
            MultiByteToWideChar(codePage, 0, s, -1, wideStr, wideLen);
            bstr = SysAllocStringLen(wideStr, wideLen-1);
            CoTaskMemFree(wideStr);

            return bstr;
        }
    }
    return NULL;
};

/*
**  properties
*/

HRESULT GetObjectProperty(LPUNKNOWN object, DISPID dispID, VARIANT& v)
{
    IDispatch *pDisp;
    HRESULT hr = object->QueryInterface(IID_IDispatch, (LPVOID *)&pDisp);
    if( SUCCEEDED(hr) )
    {
        DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
        VARIANT vres;
        VariantInit(&vres);
        hr = pDisp->Invoke(dispID, IID_NULL, LOCALE_USER_DEFAULT,
                DISPATCH_PROPERTYGET, &dispparamsNoArgs, &vres, NULL, NULL);
        if( SUCCEEDED(hr) )
        {
            if( V_VT(&v) != V_VT(&vres) )
            {
                hr = VariantChangeType(&v, &vres, 0, V_VT(&v));
                VariantClear(&vres);
            }
            else
            {
                v = vres;
            }
        }
        pDisp->Release();
    }
    return hr;
};

HDC CreateDevDC(DVTARGETDEVICE *ptd)
{
    HDC hdc;
    if( NULL == ptd )
    {
        hdc = CreateDC(TEXT("DISPLAY"), NULL, NULL, NULL);
    }
    else
    {
        LPDEVNAMES lpDevNames = (LPDEVNAMES) ptd; // offset for size field
        LPDEVMODE  lpDevMode  = NULL;

        if (ptd->tdExtDevmodeOffset != 0)
            lpDevMode  = (LPDEVMODE) ((LPTSTR)ptd + ptd->tdExtDevmodeOffset);

        hdc = CreateDC( (LPTSTR) lpDevNames + ptd->tdDriverNameOffset,
                        (LPTSTR) lpDevNames + ptd->tdDeviceNameOffset,
                        (LPTSTR) lpDevNames + ptd->tdPortNameOffset,
                        lpDevMode );
    }
    return hdc;
};

#define HIMETRIC_PER_INCH 2540

void DPFromHimetric(HDC hdc, LPPOINT pt, int count)
{
    LONG lpX = GetDeviceCaps(hdc, LOGPIXELSX);
    LONG lpY = GetDeviceCaps(hdc, LOGPIXELSY);
    while( count-- )
    {
        pt->x = pt->x*lpX/HIMETRIC_PER_INCH;
        pt->y = pt->y*lpY/HIMETRIC_PER_INCH;
        ++pt;
    }
};

void HimetricFromDP(HDC hdc, LPPOINT pt, int count)
{
    LONG lpX = GetDeviceCaps(hdc, LOGPIXELSX);
    LONG lpY = GetDeviceCaps(hdc, LOGPIXELSY);
    while( count-- )
    {
        pt->x = pt->x*HIMETRIC_PER_INCH/lpX;
        pt->y = pt->y*HIMETRIC_PER_INCH/lpY;
        ++pt;
    }
};


LPWSTR CombineURL(LPCWSTR baseUrl, LPCWSTR url)
{
    if( NULL != url )
    {
        // check whether URL is already absolute
        const wchar_t *end=wcschr(url, L':');
        if( (NULL != end) && (end != url) )
        {
            // validate protocol header
            const wchar_t *start = url;
            wchar_t c = *start;
            if( iswalpha(c) )
            {
                ++start;
                while( start != end )
                {
                    c = *start;
                    if( ! (iswalnum(c)
                       || (L'-' == c)
                       || (L'+' == c)
                       || (L'.' == c)
                       || (L'/' == c)) ) /* VLC uses / to allow user to specify a demuxer */
                        // not valid protocol header, assume relative URL
                        goto relativeurl;
                    ++start;
                }
                /* we have a protocol header, therefore URL is absolute */
                UINT len = wcslen(url);
                wchar_t *href = (LPWSTR)CoTaskMemAlloc((len+1)*sizeof(wchar_t));
                if( href )
                {
                    memcpy(href, url, len*sizeof(wchar_t));
                    href[len] = L'\0';
                }
                return href;
            }
        }

relativeurl:

        if( baseUrl )
        {
            size_t baseLen = wcslen(baseUrl);
            wchar_t *href = (LPWSTR)CoTaskMemAlloc((baseLen+wcslen(url)+1)*sizeof(wchar_t));
            if( href )
            {
                /* prepend base URL */
                wcscpy(href, baseUrl);

                /*
                ** relative url could be empty,
                ** in which case return base URL
                */
                if( L'\0' == *url )
                    return href;

                /*
                ** locate pathname part of base URL
                */

                /* skip over protocol part  */
                wchar_t *pathstart = wcschr(href, L':');
                wchar_t *pathend;
                if( pathstart )
                {
                    if( L'/' == *(++pathstart) )
                    {
                        if( L'/' == *(++pathstart) )
                        {
                            ++pathstart;
                        }
                    }
                    /* skip over host part */
                    pathstart = wcschr(pathstart, L'/');
                    pathend = href+baseLen;
                    if( ! pathstart )
                    {
                        // no path, add a / past end of url (over '\0')
                        pathstart = pathend;
                        *pathstart = L'/';
                    }
                }
                else
                {
                    /* baseURL is just a UNIX file path */
                    if( L'/' != *href )
                    {
                        /* baseURL is not an absolute path */
                        return NULL;
                    }
                    pathstart = href;
                    pathend = href+baseLen;
                }

                /* relative URL made of an absolute path ? */
                if( L'/' == *url )
                {
                    /* replace path completely */
                    wcscpy(pathstart, url);
                    return href;
                }

                /* find last path component and replace it */
                while( L'/' != *pathend )
                    --pathend;

                /*
                ** if relative url path starts with one or more './' or '../',
                ** factor them out of href so that we return a
                ** normalized URL
                */
                while( pathend > pathstart )
                {
                    const wchar_t *p = url;
                    if( L'.' != *p )
                        break;
                    ++p;
                    if( L'\0' == *p  )
                    {
                        /* relative url is just '.' */
                        url = p;
                        break;
                    }
                    if( L'/' == *p  )
                    {
                        /* relative url starts with './' */
                        url = ++p;
                        continue;
                    }
                    if( L'.' != *p )
                        break;
                    ++p;
                    if( L'\0' == *p )
                    {
                        /* relative url is '..' */
                    }
                    else
                    {
                        if( L'/' != *p )
                            break;
                        /* relative url starts with '../' */
                        ++p;
                    }
                    url = p;
                    do
                    {
                        --pathend;
                    }
                    while( L'/' != *pathend );
                }
                /* skip over '/' separator */
                ++pathend;
                /* concatenate remaining base URL and relative URL */
                wcscpy(pathend, url);
            }
            return href;
        }
    }
    return NULL;
}

