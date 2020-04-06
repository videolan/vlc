/*****************************************************************************
 * dirs.c: directories configuration
 *****************************************************************************
 * Copyright (C) 2001-2010 VLC authors and VideoLAN
 * Copyright © 2007-2012 Rémi Denis-Courmont
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define COBJMACROS
#define INITGUID

#ifndef UNICODE
#define UNICODE
#endif
#include <vlc_common.h>

#include <vlc_charset.h>

#include <assert.h>

#include <winstring.h>
#include <windows.storage.h>
#include <roapi.h>

static char * GetFolderName(IStorageFolder *folder)
{
    HRESULT hr;
    IStorageItem *item;
    hr = IStorageFolder_QueryInterface(folder, &IID_IStorageItem, (void**)&item);
    if (FAILED(hr))
        return NULL;

    char *result = NULL;
    HSTRING path;
    hr = IStorageItem_get_Path(item, &path);
    if (SUCCEEDED(hr))
    {
        PCWSTR pszPathTemp = WindowsGetStringRawBuffer(path, NULL);
        result = FromWide(pszPathTemp);
        WindowsDeleteString(path);
    }
    IStorageItem_Release(item);
    IStorageFolder_Release(folder);
    return result;
}

static char *config_GetShellDir(vlc_userdir_t csidl)
{
    HRESULT hr;
    IStorageFolder *folder = NULL;

    IKnownFoldersStatics *knownFoldersStatics = NULL;
    static const WCHAR *className = L"Windows.Storage.KnownFolders";
    const UINT32 clen = wcslen(className);

    HSTRING hClassName = NULL;
    HSTRING_HEADER header;
    hr = WindowsCreateStringReference(className, clen, &header, &hClassName);
    if (FAILED(hr))
        goto end_other;

    hr = RoGetActivationFactory(hClassName, &IID_IKnownFoldersStatics, (void**)&knownFoldersStatics);

    if (FAILED(hr))
        goto end_other;

    if (!knownFoldersStatics) {
        hr = E_FAIL;
        goto end_other;
    }

    switch (csidl) {
    case VLC_HOME_DIR:
        hr = IKnownFoldersStatics_get_DocumentsLibrary(knownFoldersStatics, &folder);
        break;
    case VLC_MUSIC_DIR:
        hr = IKnownFoldersStatics_get_MusicLibrary(knownFoldersStatics, &folder);
        break;
    case VLC_PICTURES_DIR:
        hr = IKnownFoldersStatics_get_PicturesLibrary(knownFoldersStatics, &folder);
        break;
    case VLC_VIDEOS_DIR:
        hr = IKnownFoldersStatics_get_VideosLibrary(knownFoldersStatics, &folder);
        break;
    default: vlc_assert_unreachable();
    }

end_other:
    WindowsDeleteString(hClassName);
    if (knownFoldersStatics)
        IKnownFoldersStatics_Release(knownFoldersStatics);

    if( FAILED(hr) || folder == NULL )
        return NULL;

    return GetFolderName(folder);
}

static char *config_GetDataDir(void)
{
    const char *path = getenv ("VLC_DATA_PATH");
    return (path != NULL) ? strdup (path) : NULL;
}

char *config_GetSysPath(vlc_sysdir_t type, const char *filename)
{
    char *dir = NULL;

    switch (type)
    {
        case VLC_PKG_DATA_DIR:
            dir = config_GetDataDir();
            break;
        case VLC_PKG_LIB_DIR:
        case VLC_PKG_LIBEXEC_DIR:
        case VLC_SYSDATA_DIR:
            return NULL;
        case VLC_LOCALE_DIR:
            dir = config_GetSysPath(VLC_PKG_DATA_DIR, "locale");
            break;
        default:
            vlc_assert_unreachable();
    }

    if (unlikely(dir == NULL))
        return NULL;

    if (filename == NULL)
        return dir;

    char *path;
    if (unlikely(asprintf(&path, "%s/%s", dir, filename) == -1))
        path = NULL;
    free(dir);
    return path;
}

static char *config_GetAppDir (void)
{
    char *psz_dir = NULL;

    HRESULT hr;
    IStorageFolder *folder = NULL;

    IApplicationDataStatics *appDataStatics = NULL;
    IApplicationData *appData = NULL;
    static const WCHAR *className = L"Windows.Storage.ApplicationData";
    const UINT32 clen = wcslen(className);

    HSTRING hClassName = NULL;
    HSTRING_HEADER header;
    hr = WindowsCreateStringReference(className, clen, &header, &hClassName);
    if (FAILED(hr))
        goto end_appdata;

    hr = RoGetActivationFactory(hClassName, &IID_IApplicationDataStatics, (void**)&appDataStatics);

    if (FAILED(hr))
        goto end_appdata;

    if (!appDataStatics) {
        hr = E_FAIL;
        goto end_appdata;
    }

    hr = IApplicationDataStatics_get_Current(appDataStatics, &appData);

    if (FAILED(hr))
        goto end_appdata;

    if (!appData) {
        hr = E_FAIL;
        goto end_appdata;
    }

    hr = IApplicationData_get_LocalFolder(appData, &folder);

    if( SUCCEEDED(hr) && folder != NULL )
    {
        char *psz_parent = GetFolderName(folder);
        if (psz_parent == NULL
         || asprintf (&psz_dir, "%s\\vlc", psz_parent) == -1)
            psz_dir = NULL;
        free(psz_parent);
    }

end_appdata:
    WindowsDeleteString(hClassName);
    if (appDataStatics)
        IApplicationDataStatics_Release(appDataStatics);
    if (appData)
        IApplicationData_Release(appData);

    return psz_dir;
}

#ifdef HAVE_IAPPLICATIONDATA2
static char *config_GetCacheDir (void)
{
    HRESULT hr;
    IStorageFolder *folder = NULL;
    IApplicationDataStatics *appDataStatics = NULL;
    IApplicationData *appData = NULL;
    IApplicationData2 *appData2 = NULL;
    static const WCHAR *className = L"Windows.Storage.ApplicationData";
    const UINT32 clen = wcslen(className);

    HSTRING hClassName = NULL;
    HSTRING_HEADER header;
    hr = WindowsCreateStringReference(className, clen, &header, &hClassName);
    if (FAILED(hr))
        goto end_appdata;

    hr = RoGetActivationFactory(hClassName, &IID_IApplicationDataStatics, (void**)&appDataStatics);

    if (FAILED(hr))
        goto end_appdata;

    if (!appDataStatics) {
        hr = E_FAIL;
        goto end_appdata;
    }

    hr = IApplicationDataStatics_get_Current(appDataStatics, &appData);

    if (FAILED(hr))
        goto end_appdata;

    if (!appData) {
        hr = E_FAIL;
        goto end_appdata;
    }

    IApplicationData_QueryInterface(appData, &IID_IApplicationData2, (void**)&appData2);
    if (!appData2) {
        hr = E_FAIL;
        goto end_appdata;
    }

    hr = IApplicationData2_get_LocalCacheFolder(appData2, &folder);

end_appdata:
    WindowsDeleteString(hClassName);
    if (appDataStatics)
        IApplicationDataStatics_Release(appDataStatics);
    if (appData2)
        IApplicationData2_Release(appData2);
    if (appData)
        IApplicationData_Release(appData);

    if( FAILED(hr) || folder == NULL )
        return NULL;

    return GetFolderName(folder);
}
#endif // HAVE_IAPPLICATIONDATA2

char *config_GetUserDir (vlc_userdir_t type)
{
    switch (type)
    {
        case VLC_HOME_DIR:
        case VLC_DESKTOP_DIR:
        case VLC_DOWNLOAD_DIR:
        case VLC_TEMPLATES_DIR:
        case VLC_PUBLICSHARE_DIR:
        case VLC_DOCUMENTS_DIR:
            return config_GetShellDir (VLC_HOME_DIR);
        case VLC_CONFIG_DIR:
        case VLC_USERDATA_DIR:
            return config_GetAppDir ();
        case VLC_CACHE_DIR:
#ifdef HAVE_IAPPLICATIONDATA2
            return config_GetCacheDir ();
#else // !HAVE_IAPPLICATIONDATA2
            return config_GetAppDir();
#endif // !HAVE_IAPPLICATIONDATA2
        case VLC_MUSIC_DIR:
            return config_GetShellDir (VLC_MUSIC_DIR);
        case VLC_PICTURES_DIR:
            return config_GetShellDir (VLC_PICTURES_DIR);
        case VLC_VIDEOS_DIR:
            return config_GetShellDir (VLC_VIDEOS_DIR);
        default:
            vlc_assert_unreachable ();
    }
}
