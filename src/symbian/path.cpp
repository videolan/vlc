/*****************************************************************************
 * path.cpp : Symbian Application's Provate Path
 *****************************************************************************
 * Copyright © 2010 Pankaj Yadav
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

#include <f32file.h>    /* RFs */
#include <string.h>     /* strlen */
#include <utf.h>        /* CnvUtfConverter */

extern "C" {
    #include "path.h"
}

/*Way to Find AppPrivatePath (A Path where an application can store its private data) */
static TInt GetPrivatePath(TFileName& privatePath)
{
    TFileName KPath;
    RFs fsSession;
    TInt result;

    result = fsSession.Connect();
    if (result != KErrNone)
    {
        return result;
    }
    fsSession.PrivatePath(KPath);
    TFindFile findFile(fsSession);

    privatePath = KPath;
    result = findFile.FindByDir(KPath, KNullDesC);
    if (result == KErrNone)
    {
        privatePath = findFile.File();
    }

    fsSession.Close();
    return result;
}

extern "C" char * GetConstPrivatePath(void)
{
    TFileName privatePath;
    TBuf8<KMaxFileName> privatepathutf8;
    size_t len;
    char carray[KMaxFileName];

    if (GetPrivatePath(privatePath) != KErrNone)
    {
        return strdup("C:\\Data\\Others");
    }

    CnvUtfConverter::ConvertFromUnicodeToUtf8( privatepathutf8, privatePath );

    TInt index = 0;
    for (index = 0; index < privatepathutf8.Length(); index++)
    {
        carray[index] = privatepathutf8[index];
    }
    carray[index] = 0;

    if ((len = strnlen((const char *)carray, KMaxFileName) < KMaxFileName))
    {
        carray[len-1] = '\0';
        return strdup((const char *)carray);
    }
    else
    {
        return strdup("C:\\Data\\Others");
    }
}

