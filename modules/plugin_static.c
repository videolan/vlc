#include <stddef.h>
#include <stdint.h>
#define __LIBVLC__
#define __PLUGIN__
#define MODULE_STRING "vlcplugins"
#include <vlc_plugin.h>
typedef int (*vlc_set_cb) (void *, void *, int, ...);
typedef int (*vlc_plugin_entry) (vlc_set_cb, void *);

#define VLC_EXPORT __attribute__((visibility("default")))
static const vlc_plugin_entry vlc_plugin_entries[];

#include <stdlib.h>
#include <stdio.h>

EXTERN_SYMBOL DLL_SYMBOL \
int CDECL_SYMBOL VLC_SYMBOL(vlc_entry)(vlc_set_cb vlc_set, void *opaque)
{
    for(const vlc_plugin_entry *entry=vlc_plugin_entries;
        *entry != NULL; entry++)
    {
        int ret = (*entry)(vlc_set, opaque);
        if (ret != 0)
            return ret;
    }
    return 0;
}

VLC_METADATA_EXPORTS
VLC_MODULE_NAME_HIDDEN_SYMBOL

#define VLC_PLUGIN_ENTRY_NAME(name) vlc_entry__ ## name
#define VLC_DECLARE_PLUGIN_ENTRY(name) \
    VLC_EXPORT extern int VLC_PLUGIN_ENTRY_NAME(name) (vlc_set_cb, void *);
VLC_MODULE_LIST(VLC_DECLARE_PLUGIN_ENTRY)

#define VLC_PLUGIN_ENTRY_LIST(name) VLC_PLUGIN_ENTRY_NAME(name),
static const vlc_plugin_entry vlc_plugin_entries[] = {
    VLC_MODULE_LIST(VLC_PLUGIN_ENTRY_LIST)
    NULL,
};
