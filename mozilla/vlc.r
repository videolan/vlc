/*****************************************************************************
 * VLC Plugin description for OS X
 *****************************************************************************/

/* Definitions of system resource types */
#include <Types.r>

/* The first string in the array is a plugin description,
 * the second is the plugin name */
resource 'STR#' (126)
{
    {
        "A VLC test plugin... hope it goes somewhere",
        "VLC plugin"
    };
};

/* A description for each MIME type in resource 128 */
resource 'STR#' (127)
{
    {
        "Invoke scriptable sample plugin"
    };
};

/* A series of pairs of strings... first MIME type, then file extension(s) */
resource 'STR#' (128,"MIME Type")
{
    {
        "application/vlc-plugin", ""
    };
};

