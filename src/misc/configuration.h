/* Internal configuration prototypes and structures */

int  config_CreateDir( vlc_object_t *, const char * );
int  config_AutoSaveConfigFile( vlc_object_t * );

void config_Free( module_t * );

void config_SetCallbacks( module_config_t *, module_config_t *, size_t );
void config_UnsetCallbacks ( module_config_t *, size_t );

#define config_LoadCmdLine(a,b,c,d) __config_LoadCmdLine(VLC_OBJECT(a),b,c,d)
#define config_LoadConfigFile(a,b) __config_LoadConfigFile(VLC_OBJECT(a),b)

int   __config_LoadCmdLine  ( vlc_object_t *, int *, char *[], vlc_bool_t );
char *   config_GetHomeDir     ( void );
char *   config_GetUserDir     ( void );
const char * config_GetDataDir ( const vlc_object_t * );
int    __config_LoadConfigFile ( vlc_object_t *, const char * );
