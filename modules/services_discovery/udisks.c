/*****************************************************************************
 * udisks.c: file system services discovery module
 *****************************************************************************
 * Copyright © 2022 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Juliane de Sartiges <jill@videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_configuration.h>
#include <vlc_services_discovery.h>
#include <vlc_url.h>

#include <poll.h>

#include SDBUS_HEADER

#define DBUS_INTERFACE_UDISKS2 "org.freedesktop.UDisks2"
#define DBUS_INTERFACE_UDISKS2_BLOCKS DBUS_INTERFACE_UDISKS2".Block"
#define DBUS_INTERFACE_UDISKS2_DRIVE DBUS_INTERFACE_UDISKS2".Drive"
#define DBUS_INTERFACE_UDISKS2_FILESYSTEM DBUS_INTERFACE_UDISKS2".Filesystem"
#define DBUS_INTERFACE_UDISKS2_MANAGER DBUS_INTERFACE_UDISKS2".Manager"
#define DBUS_INTERFACE_UDISKS2_PARTITION DBUS_INTERFACE_UDISKS2".Partition"
#define DBUS_INTERFACE_UDISKS2_LOOP DBUS_INTERFACE_UDISKS2".Loop"

#define DBUS_PATH_UDISKS2 "/org/freedesktop/UDisks2"
#define DBUS_PATH_UDISKS2_DRIVES DBUS_PATH_UDISKS2"/drives"
#define DBUS_PATH_UDISKS2_BLOCK_DEV DBUS_PATH_UDISKS2"/block_devices"
#define DBUS_PATH_UDISKS2_MANAGER DBUS_PATH_UDISKS2"/Manager"

struct fs_properties_changed_param
{
    services_discovery_t *sd;
    char *object_path;
};

struct device_info
{
    sd_bus_slot *slot;
    input_item_t *item;
    char *label;
    uint64_t size;
    bool removable;
    struct fs_properties_changed_param *param;
};

struct discovery_sys
{
    sd_bus *bus;
    sd_bus_slot *interface_added_slot;
    sd_bus_slot *interface_removed_slot;
    vlc_thread_t thread;
    vlc_dictionary_t entries;
};

static const char * const binary_prefixes[] = { N_("B"), N_("KiB"), N_("MiB"), N_("GiB"), N_("TiB") };

static void release_device_info(services_discovery_t *sd, struct device_info* info)
{
    if(info->param)
    {
        free(info->param->object_path);
        free(info->param);
    }
    if(info->slot)
        sd_bus_slot_unref(info->slot);
    free(info->label);
    if(info->item)
    {
        services_discovery_RemoveItem(sd, info->item);
        input_item_Release(info->item);
    }
    free(info);
}

static int human(uint64_t *i)
{
    if (i == 0)
        return 0;
    unsigned exp = (63 - clz(*i)) / 10;
    exp = (exp < ARRAY_SIZE(binary_prefixes)) ? exp : ARRAY_SIZE(binary_prefixes);
    *i >>= (10 * exp);
    return exp;
}

static const char *print_label(const char *drive_label, bool removable)
{
    if(drive_label != NULL && drive_label[0] != '\0')
        return drive_label;
    if(removable)
        return _("Removable Drive");
    return _("Internal Drive");
}

static input_item_t *input_item_NewDrive(const char *drive_label, const char *path, uint64_t size, bool removable)
{
    int r = 0;
    input_item_t *ret = NULL;
    char *uri = NULL;
    char *label = NULL;

    uri = vlc_path2uri(path, "file");
    if(!uri)
        return NULL;

    int prefix = human(&size);
    r = asprintf(&label, "%s (%ld %s)", print_label(drive_label, removable), size, vlc_gettext(binary_prefixes[prefix]));
    if(r == -1)
    {
        free(uri);
        return NULL;
    }
    ret = input_item_NewDirectory(uri, label, ITEM_LOCAL);
    free(uri);
    free(label);
    return ret;
}

static int fs_properties_changed(sd_bus_message *m, void *userdata, sd_bus_error *err)
{
    VLC_UNUSED(err);
    int r;
    struct fs_properties_changed_param *param = userdata;
    services_discovery_t *sd = param->sd;
    struct discovery_sys *p_sys = sd->p_sys;

    const char *interface_name;
    r = sd_bus_message_read(m, "s", &interface_name);
    if(r < 0)
        return r;
    if(strcmp(interface_name, DBUS_INTERFACE_UDISKS2_FILESYSTEM) != 0)
        return 0;

    size_t mount_path_len;
    const char *mount_path = NULL;

    char *property_name = NULL;

    r = sd_bus_message_enter_container(m, 'a', "{sv}");
    if(r < 0)
        return r;
    for(;;)
    {
        r = sd_bus_message_enter_container(m, 'e', "sv");
        if (r < 0)
            return r;
        r = sd_bus_message_read(m, "s", &property_name);
        if(r < 0)
            return r;
        if (r == 0)
            return 0;
        if(strcmp(property_name, "MountPoints") == 0)
            break;
    }
    r = sd_bus_message_enter_container(m, 'v', "aay");
    if(r < 0)
        return r;
    r = sd_bus_message_enter_container(m, 'a', "ay");
    if(r < 0)
        return r;
    const void *path;
    r = sd_bus_message_read_array(m, 'y', &path, &mount_path_len);
    mount_path = path;
    if(r < 0)
        return r;
    struct device_info *info = vlc_dictionary_value_for_key(&p_sys->entries, param->object_path);
    if(!info)
        return -1;
    if(mount_path_len)
    {
        info->item = input_item_NewDrive(info->label, mount_path, info->size, info->removable);
        services_discovery_AddItem(sd, info->item);
    }
    else
        services_discovery_RemoveItem(sd, info->item);
    return 1;
}

static int get_info_from_block_device(services_discovery_t *sd, const char *block_path, struct device_info **info_ret)
{
    struct discovery_sys *p_sys = sd->p_sys;
    sd_bus *bus = p_sys->bus;
    sd_bus_error err = SD_BUS_ERROR_NULL;
    int r = 0;

    struct device_info *info = NULL;
    size_t mount_path_len;
    const char *drive_path = NULL;
    const char *mount_path = NULL;

    sd_bus_message *drive_reply = NULL;
    sd_bus_message *mounts_reply = NULL;

    int autoclear;
    r = sd_bus_get_property_trivial(bus, DBUS_INTERFACE_UDISKS2,
                            block_path, DBUS_INTERFACE_UDISKS2_LOOP,
                            "Autoclear", &err, 'b', &autoclear);
    if(r == 0)
    {
        msg_Dbg(sd, "Ignoring loop device: %s\n", block_path);
        goto error;
    }
    if(r < 0 && r != -EINVAL)
    {
        msg_Err(sd, "%s: %s\n", err.name, err.message);
        goto error;
    }
    sd_bus_error_free( &err );
    err = SD_BUS_ERROR_NULL;

    r = sd_bus_get_property(bus, DBUS_INTERFACE_UDISKS2,
                            block_path, DBUS_INTERFACE_UDISKS2_FILESYSTEM,
                            "MountPoints", &err, &mounts_reply, "aay");
    if(r == -EINVAL)
    {
        r = 0;
        msg_Dbg(sd, "%s block device does not contain any file system", block_path);
        goto error;
    }
    if(r < 0)
    {
        msg_Err(sd, "%s: %s\n", err.name, err.message);
        goto error;
    }

    r = sd_bus_message_enter_container(mounts_reply, 'a', "ay");
    if(r < 0)
        goto error;

    const void *path;
    r = sd_bus_message_read_array(mounts_reply, 'y', &path, &mount_path_len);
    mount_path = path;
    if(r < 0)
        goto error;

    info = calloc(1, sizeof(struct device_info));
    if(!info)
        goto generic_error;

    info->param = malloc(sizeof(struct fs_properties_changed_param));
    if(!info->param)
        goto generic_error;

    info->param->sd = sd;
    info->param->object_path = strdup(block_path);

    if(!info->param->object_path)
        goto generic_error;

    r = sd_bus_match_signal(bus, &info->slot,
                            NULL, block_path,
                            "org.freedesktop.DBus.Properties", "PropertiesChanged",
                            fs_properties_changed, info->param);
    if(r < 0)
        goto error;

    r = sd_bus_get_property_trivial(bus, DBUS_INTERFACE_UDISKS2,
                                      block_path,
                                      DBUS_INTERFACE_UDISKS2_BLOCKS, "Size",
                                      &err, 't', &info->size);
    if(r < 0)
    {
        msg_Err(sd, "%s: %s\n", err.name, err.message);
        goto error;
    }

    r = sd_bus_get_property(bus, DBUS_INTERFACE_UDISKS2,
                                block_path, DBUS_INTERFACE_UDISKS2_BLOCKS,
                                "Drive", &err, &drive_reply, "o");
    if(r < 0)
    {
        msg_Err(sd, "%s: %s\n", err.name, err.message);
        goto error;
    }

    r = sd_bus_message_read(drive_reply, "o", &drive_path);
    if(r < 0)
        goto error;

    if(strcmp(drive_path, "/") != 0)
    {
        r = sd_bus_get_property_trivial(bus, DBUS_INTERFACE_UDISKS2,
                                          drive_path,
                                          DBUS_INTERFACE_UDISKS2_DRIVE, "Removable",
                                          &err, 'b', &info->removable);
        if(r < 0)
        {
            msg_Err(sd, "%s: %s\n", err.name, err.message);
            goto error;
        }
    }

    r = sd_bus_get_property_string(bus, DBUS_INTERFACE_UDISKS2,
                                      block_path,
                                      DBUS_INTERFACE_UDISKS2_BLOCKS, "IdLabel",
                                      &err, &info->label);
    if(r < 0)
    {
        msg_Err(sd, "%s: %s\n", err.name, err.message);
        goto error;
    }

    if(mount_path)
    {
        info->item = input_item_NewDrive(info->label, mount_path, info->size, info->removable);
        if(!info->item)
            goto generic_error;
    }
    sd_bus_message_unref(drive_reply);
    sd_bus_error_free(&err);
    *info_ret = info;
    return 1;
generic_error:
    r = -1;
error:
    sd_bus_error_free(&err);
    if(drive_reply)
        sd_bus_message_unref(drive_reply);
    if(mounts_reply)
        sd_bus_message_unref(mounts_reply);
    if(info)
        release_device_info(sd, info);
    return r;
}

static void FreeEntries(void *p_value, void *p_obj)
{
    services_discovery_t *sd = p_obj;
    struct device_info *info = p_value;
    if(info)
        release_device_info(sd, info);
}

static int interfaces_added_cb(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    VLC_UNUSED(ret_error);
    int r;
    services_discovery_t *sd = userdata;
    struct discovery_sys *p_sys = sd->p_sys;
    struct device_info *info = NULL;
    const char *path = NULL;
    r = sd_bus_message_read(m, "o", &path);
    if(r < 0)
        return r;
    if(strncmp(path, DBUS_PATH_UDISKS2_BLOCK_DEV, strlen(DBUS_PATH_UDISKS2_BLOCK_DEV)) != 0)
        return 0;
    r = get_info_from_block_device(sd, path, &info);
    if(r < 0)
    {
        if(info)
            release_device_info(sd, info);
        return r;
    }
    if(!info)
        return 0;
    vlc_dictionary_insert(&p_sys->entries, path, info);
    if(info->item)
        services_discovery_AddItem(sd, info->item);
    return 1;
}

static int interfaces_removed_cb(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    VLC_UNUSED(ret_error);
    int r;
    services_discovery_t *sd = userdata;
    struct discovery_sys *p_sys = sd->p_sys;
    const char *path;
    r = sd_bus_message_read(m, "o", &path);
    if(r < 0)
        return r;
    if(strncmp(path, DBUS_PATH_UDISKS2_BLOCK_DEV, strlen(DBUS_PATH_UDISKS2_BLOCK_DEV)) != 0)
        return 0;
    struct device_info *info = vlc_dictionary_value_for_key(&p_sys->entries, path);
    if(!info)
        return 0;
    vlc_dictionary_remove_value_for_key(&p_sys->entries, path, FreeEntries, sd);
    return 1;
}

static void *Run(void *p_obj)
{
    int r;
    int canc = vlc_savecancel();
    services_discovery_t *sd = p_obj;
    struct discovery_sys *p_sys = sd->p_sys;
    sd_bus *bus = p_sys->bus;

    r = sd_bus_match_signal(bus, &p_sys->interface_added_slot,
                            DBUS_INTERFACE_UDISKS2, DBUS_PATH_UDISKS2,
                            "org.freedesktop.DBus.ObjectManager", "InterfacesAdded",
                            interfaces_added_cb, sd);
    if(r < 0)
    {
        return NULL;
    }

    r = sd_bus_match_signal(bus, &p_sys->interface_removed_slot,
                            DBUS_INTERFACE_UDISKS2, DBUS_PATH_UDISKS2,
                            "org.freedesktop.DBus.ObjectManager", "InterfacesRemoved",
                            interfaces_removed_cb, sd);
    if(r < 0)
    {
        sd_bus_slot_unref(p_sys->interface_added_slot);
        return NULL;
    }

    struct pollfd ufd[1];
    ufd[0].fd = sd_bus_get_fd(bus);
    ufd[0].events = sd_bus_get_events(bus);

    for(;;)
    {
        vlc_restorecancel(canc);

        while(poll(ufd, 1, -1) < 0);

        canc = vlc_savecancel();

        sd_bus_message *msg = NULL;
        r = sd_bus_process(bus, &msg);
        if(r < 0)
        {
            msg_Err(sd, "Couldn't process new d-bus event : %d", -r);
            break;
        }
        sd_bus_message_unref(msg);
    }
    sd_bus_slot_unref(p_sys->interface_added_slot);
    sd_bus_slot_unref(p_sys->interface_removed_slot);
    return NULL;
}

static int Open(vlc_object_t *p_obj)
{
    services_discovery_t *sd = (services_discovery_t *)p_obj;
    struct discovery_sys *p_sys = vlc_alloc(1, sizeof(struct discovery_sys));
    struct device_info *info = NULL;
    if (!p_sys)
        return VLC_ENOMEM;
    sd->p_sys = p_sys;

    sd->description = _("Local drives discovery");
    vlc_dictionary_init(&p_sys->entries, 0);

    int r;

    /* connect to the session bus */
    r =  sd_bus_open_system(&p_sys->bus);
    if(r < 0)
        goto error;
   sd_bus *bus = p_sys->bus;
   sd_bus_message *reply = NULL;
   sd_bus_error err = SD_BUS_ERROR_NULL;
   r = sd_bus_call_method(bus, DBUS_INTERFACE_UDISKS2, DBUS_PATH_UDISKS2_MANAGER,
                          DBUS_INTERFACE_UDISKS2_MANAGER, "GetBlockDevices",
                          &err, &reply, "a{sv}", NULL);
    if (r < 0)
    {
        msg_Err(sd, "%s: %s\n", err.name, err.message);
        goto error;
    }

    r = sd_bus_message_enter_container(reply, 'a', "o");
    if (r < 0)
        goto error;

    char *block_path = NULL;
    while ((r = sd_bus_message_read(reply, "o", &block_path)) != 0)
    {
        info = NULL;
        if(r < 0)
            goto error;
        r = get_info_from_block_device(sd, block_path, &info);
        if(r < 0)
            goto error;
        if(!info)
            continue;
        vlc_dictionary_insert(&p_sys->entries, block_path, info);
        if(info->item)
            services_discovery_AddItem(sd, info->item);
    }
    sd_bus_message_unref(reply);

    if (vlc_clone(&p_sys->thread, Run, sd))
        goto error;
    sd_bus_error_free(&err);
    return VLC_SUCCESS;
error:
    if(info)
        release_device_info(sd, info);
    vlc_dictionary_clear(&p_sys->entries, FreeEntries, sd);
    if(p_sys->bus)
    {
        sd_bus_flush(p_sys->bus);
        sd_bus_close(p_sys->bus);
    }
    sd_bus_error_free(&err);
    free(p_sys);
    return VLC_EGENERIC;
}

static void Close(vlc_object_t *p_obj)
{
    services_discovery_t *sd = (services_discovery_t *)p_obj;
    struct discovery_sys *p_sys = sd->p_sys;
    if(p_sys->interface_added_slot)
        sd_bus_slot_unref(p_sys->interface_added_slot);
    sd_bus_flush(p_sys->bus);
    sd_bus_close(p_sys->bus);
    vlc_cancel(p_sys->thread);
    vlc_join(p_sys->thread, NULL);
    vlc_dictionary_clear(&p_sys->entries, FreeEntries, sd);
    free(p_sys);
}

VLC_SD_PROBE_HELPER("udisks", N_("UDisks2"), SD_CAT_DEVICES)

/*
 * Module descriptor
 */
vlc_module_begin()
    set_shortname( "UDisks" )
    set_description( N_( "Local Drives (UDisks)" ) )
    set_subcategory( SUBCAT_PLAYLIST_SD )
    set_capability( "services_discovery", 0 )
    set_callbacks( Open, Close )
    add_shortcut( "udisks" )
    VLC_SD_PROBE_SUBMODULE
vlc_module_end ()
