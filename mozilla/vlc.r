/*****************************************************************************
 * VLC Plugin description for OS X
 *****************************************************************************/

/* Definitions of system resource types */

data 'carb' (0)
{
};

/* The first string in the array is a plugin description,
 * the second is the plugin name */
resource 'STR#' (126)
{
    {
        "VideoLAN WWW: http://videolan.org"
        "VLC multimedia plugin"
    };
};

/* A description for each MIME type in resource 128 */
resource 'STR#' (127)
{
    {
        "MPEG audio",
        "MPEG audio",
        "MPEG video",
        "MPEG video",
        "MPEG video",
        "MPEG video",
        "MPEG-4 video",
        "MPEG-4 audio",
        "MPEG-4 video",
        "MPEG-4 video",
        "AVI video",
        "QuickTime video",
        "Ogg stream",
        "Ogg stream",
        "VLC plugin",
        "ASF stream",
        "ASF stream",
        "",
        "",
        "Google VLC Plugin"
    };
};

/* A series of pairs of strings... first MIME type, then file extension(s) */
resource 'STR#' (128,"MIME Type")
{
    {
        "audio/mpeg", "mp2,mp3,mpga,mpega",
        "audio/x-mpeg", "mp2,mp3,mpga,mpega",
        "video/mpeg", "mpg,mpeg,mpe",
        "video/x-mpeg", "mpg,mpeg,mpe",
        "video/mpeg-system", "mpg,mpeg,vob",
        "video/x-mpeg-system", "mpg,mpeg,vob",
        "video/mpeg4", "mp4,mpg4",
        "audio/mpeg4", "mp4,mpg4",
        "application/mpeg4-iod", "mp4,mpg4",
        "application/mpeg4-muxcodetable", "mp4,mpg4",
        "video/x-msvideo", "avi",
        "video/quicktime", "mov, qt",
        "application/ogg", "ogg",
        "application/x-ogg", "ogg",
        "application/x-vlc-plugin", "vlc",
        "video/x-ms-asf-plugin", "",
        "video/x-ms-asf", "",
        "application/x-mplayer2", "",
        "video/x-ms-wmv", "",
        "video/x-google-vlc-plugin", "",
    };
};

