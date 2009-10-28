/**
 * @file udev.c
 * @brief List of multimedia devices for VLC media player
 */
/*****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <libudev.h>
#include <vlc_common.h>
#include <vlc_services_discovery.h>
#include <vlc_plugin.h>
#include <search.h>
#include <poll.h>
#include <errno.h>

static int OpenV4L (vlc_object_t *);
static int OpenDisc (vlc_object_t *);
static void Close (vlc_object_t *);

/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("Devices"))
    set_description (N_("Capture devices"))
    set_category (CAT_PLAYLIST)
    set_subcategory (SUBCAT_PLAYLIST_SD)
    set_capability ("services_discovery", 0)
    set_callbacks (OpenV4L, Close)
    add_shortcut ("v4l")

    add_submodule ()
    set_shortname (N_("Discs"))
    set_description (N_("Discs"))
    set_category (CAT_PLAYLIST)
    set_subcategory (SUBCAT_PLAYLIST_SD)
    set_capability ("services_discovery", 0)
    set_callbacks (OpenDisc, Close)
    add_shortcut ("disc")

vlc_module_end ()

struct device
{
    dev_t devnum; /* must be first */
    input_item_t *item;
    services_discovery_t *sd;
};

struct subsys
{
    const char *name;
    char * (*get_mrl) (struct udev_device *dev);
    char * (*get_name) (struct udev_device *dev);
    char * (*get_cat) (struct udev_device *dev);
    int item_type;
};

struct services_discovery_sys_t
{
    const struct subsys *subsys;
    struct udev_monitor *monitor;
    vlc_thread_t         thread;
    void                *root;
};

/**
 * Compares two devices (to support binary search).
 */
static int cmpdev (const void *a, const void *b)
{
    const dev_t *da = a, *db = b;
    dev_t delta = *da - *db;

    if (sizeof (delta) > sizeof (int))
        return delta ? ((delta > 0) ? 1 : -1) : 0;
    return (signed)delta;
}

static void DestroyDevice (void *data)
{
    struct device *d = data;

    if (d->sd)
        services_discovery_RemoveItem (d->sd, d->item);
    vlc_gc_decref (d->item);
    free (d);
}

static char *decode_property (struct udev_device *, const char *);

/**
 * Adds a udev device.
 */
static int AddDevice (services_discovery_t *sd, struct udev_device *dev)
{
    services_discovery_sys_t *p_sys = sd->p_sys;

    char *mrl = p_sys->subsys->get_mrl (dev);
    if (mrl == NULL)
        return 0; /* don't know if it was an error... */
    char *name = p_sys->subsys->get_name (dev);
    input_item_t *item = input_item_NewWithType (VLC_OBJECT (sd), mrl,
                                                 name ? name : mrl,
                                                 0, NULL, 0, -1,
                                                 p_sys->subsys->item_type);
    msg_Dbg (sd, "adding %s (%s)", mrl, name);
    free (name);
    free (mrl);
    if (item == NULL)
        return -1;

    struct device *d = malloc (sizeof (*d));
    if (d == NULL)
    {
        vlc_gc_decref (item);
        return -1;
    }
    d->devnum = udev_device_get_devnum (dev);
    d->item = item;
    d->sd = NULL;

    struct device **dp = tsearch (d, &p_sys->root, cmpdev);
    if (dp == NULL) /* Out-of-memory */
    {
        DestroyDevice (d);
        return -1;
    }
    if (*dp != d) /* Overwrite existing device */
    {
        DestroyDevice (*dp);
        *dp = d;
    }

    name = p_sys->subsys->get_cat (dev);
    services_discovery_AddItem (sd, item, name ? name : "Generic");
    d->sd = sd;
    free (name);
    return 0;
}

/**
 * Removes a udev device (if present).
 */
static void RemoveDevice (services_discovery_t *sd, struct udev_device *dev)
{
    services_discovery_sys_t *p_sys = sd->p_sys;

    dev_t num = udev_device_get_devnum (dev);
    struct device **dp = tfind (&(dev_t){ num }, &p_sys->root, cmpdev);
    if (dp == NULL)
        return;

    struct device *d = *dp;
    tdelete (d, &p_sys->root, cmpdev);
    DestroyDevice (d);
}

static void *Run (void *);

/**
 * Probes and initializes.
 */
static int Open (vlc_object_t *obj, const struct subsys *subsys)
{
    services_discovery_t *sd = (services_discovery_t *)obj;
    services_discovery_sys_t *p_sys = malloc (sizeof (*p_sys));

    if (p_sys == NULL)
        return VLC_ENOMEM;
    sd->p_sys = p_sys;
    p_sys->subsys = subsys;
    p_sys->root = NULL;

    struct udev_monitor *mon = NULL;
    struct udev *udev = udev_new ();
    if (udev == NULL)
        goto error;

    mon = udev_monitor_new_from_netlink (udev, "udev");
    if (mon == NULL
     || udev_monitor_filter_add_match_subsystem_devtype (mon, subsys->name,
                                                         NULL))
        goto error;
    p_sys->monitor = mon;

    /* Enumerate existing devices */
    struct udev_enumerate *devenum = udev_enumerate_new (udev);
    if (devenum == NULL)
        goto error;
    if (udev_enumerate_add_match_subsystem (devenum, subsys->name))
    {
        udev_enumerate_unref (devenum);
        goto error;
    }

    udev_monitor_enable_receiving (mon);
    /* Note that we enumerate _after_ monitoring is enabled so that we do not
     * loose device events occuring while we are enumerating. We could still
     * loose events if the Netlink socket receive buffer overflows. */
    udev_enumerate_scan_devices (devenum);
    struct udev_list_entry *devlist = udev_enumerate_get_list_entry (devenum);
    struct udev_list_entry *deventry;
    udev_list_entry_foreach (deventry, devlist)
    {
        const char *path = udev_list_entry_get_name (deventry);
        struct udev_device *dev = udev_device_new_from_syspath (udev, path);
        AddDevice (sd, dev);
        udev_device_unref (dev);
    }
    udev_enumerate_unref (devenum);

    if (vlc_clone (&p_sys->thread, Run, sd, VLC_THREAD_PRIORITY_LOW))
    {   /* Fallback without thread */
        udev_monitor_unref (mon);
        udev_unref (udev);
        p_sys->monitor = NULL;
    }
    return VLC_SUCCESS;

error:
    if (mon != NULL)
        udev_monitor_unref (mon);
    if (udev != NULL)
        udev_unref (udev);
    free (p_sys);
    return VLC_EGENERIC;
}

/**
 * Releases resources
 */
static void Close (vlc_object_t *obj)
{
    services_discovery_t *sd = (services_discovery_t *)obj;
    services_discovery_sys_t *p_sys = sd->p_sys;

    if (p_sys->monitor != NULL)
    {
        struct udev *udev = udev_monitor_get_udev (p_sys->monitor);

        vlc_cancel (p_sys->thread);
        vlc_join (p_sys->thread, NULL);
        udev_monitor_unref (p_sys->monitor);
        udev_unref (udev);
    }

    tdestroy (p_sys->root, DestroyDevice);
    free (p_sys);
}

static void *Run (void *data)
{
    services_discovery_t *sd = data;
    services_discovery_sys_t *p_sys = sd->p_sys;
    struct udev_monitor *mon = p_sys->monitor;

    int fd = udev_monitor_get_fd (mon);
    struct pollfd ufd = { .fd = fd, .events = POLLIN, };

    for (;;)
    {
        while (poll (&ufd, 1, -1) == -1)
            if (errno != EINTR)
                break;

        int canc = vlc_savecancel ();
        struct udev_device *dev = udev_monitor_receive_device (mon);
        if (dev == NULL)
            continue;

        const char *action = udev_device_get_action (dev);
        if (!strcmp (action, "add"))
            AddDevice (sd, dev);
        else if (!strcmp (action, "remove"))
            RemoveDevice (sd, dev);
        else if (!strcmp (action, "change"))
        {
            RemoveDevice (sd, dev);
            AddDevice (sd, dev);
        }
        udev_device_unref (dev);
        vlc_restorecancel (canc);
    }
    return NULL;
}

/**
 * Converts an hexadecimal digit to an integer.
 */
static int hex (char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c + 10 - 'A';
    if (c >= 'a' && c <= 'f')
        return c + 10 - 'a';
    return -1;
}

/**
 * Decodes a udev hexadecimal-encoded property.
 */
static char *decode (const char *enc)
{
    char *ret = enc ? strdup (enc) : NULL;
    if (ret == NULL)
        return NULL;

    char *out = ret;
    for (const char *in = ret; *in; out++)
    {
        int h1, h2;

        if ((in[0] == '\\') && (in[1] == 'x')
         && ((h1 = hex (in[2])) != -1)
         && ((h2 = hex (in[3])) != -1))
        {
            *out = (h1 << 4) | h2;
            in += 4;
        }
        else
        {
            *out = *in;
            in++;
        }
    }
    *out = 0;
    return ret;
}

static char *decode_property (struct udev_device *dev, const char *name)
{
    return decode (udev_device_get_property_value (dev, name));
}

/*** Video4Linux support ***/
static bool is_v4l_legacy (struct udev_device *dev)
{
    const char *version;

    version = udev_device_get_property_value (dev, "ID_V4L_VERSION");
    return version && !strcmp (version, "1");
}

static char *v4l_get_mrl (struct udev_device *dev)
{
    /* Determine media location */
    const char *scheme = "v4l2";
    if (is_v4l_legacy (dev))
        scheme = "v4l";
    const char *node = udev_device_get_devnode (dev);
    char *mrl;

    if (asprintf (&mrl, "%s://%s", scheme, node) == -1)
        mrl = NULL;
    return mrl;
}

static char *v4l_get_name (struct udev_device *dev)
{
    const char *prd = udev_device_get_property_value (dev, "ID_V4L_PRODUCT");
    return prd ? strdup (prd) : NULL;
}

static char *v4l_get_cat (struct udev_device *dev)
{
    return decode_property (dev, "ID_VENDOR_ENC");
}

int OpenV4L (vlc_object_t *obj)
{
    static const struct subsys subsys = {
        "video4linux", v4l_get_mrl, v4l_get_name, v4l_get_cat, ITEM_TYPE_CARD,
    };

    return Open (obj, &subsys);
}

/*** Discs support ***/
static char *disc_get_mrl (struct udev_device *dev)
{
    const char *val;

    val = udev_device_get_property_value (dev, "ID_CDROM");
    if (val == NULL)
        return NULL; /* Ignore non-optical block devices */

    val = udev_device_get_property_value (dev, "ID_CDROM_MEDIA_STATE");
    if ((val == NULL) || !strcmp (val, "blank"))
        return NULL; /* ignore empty drives and virgin recordable discs */

    const char *scheme = "file";
    val = udev_device_get_property_value (dev,
                                          "ID_CDROM_MEDIA_TRACK_COUNT_AUDIO");
    if (val && atoi (val))
        scheme = "cdda"; /* Audio CD rather than file system */
#if 0 /* we can use file:// for DVDs */
    val = udev_device_get_property_value (dev, "ID_CDROM_MEDIA_DVD");
    if (val && atoi (val))
        scheme = "dvd";
#endif
    val = udev_device_get_property_value (dev, "ID_CDROM_MEDIA_BD");
    if (val && atoi (val))
        scheme = "bd";
#ifdef LOL
    val = udev_device_get_property_value (dev, "ID_CDROM_MEDIA_HDDVD");
    if (val && atoi (val))
        scheme = "hddvd";
#endif

    val = udev_device_get_devnode (dev);
    char *mrl;

    if (asprintf (&mrl, "%s://%s", scheme, val) == -1)
        mrl = NULL;
    return mrl;
}

static char *disc_get_name (struct udev_device *dev)
{
    return decode_property (dev, "ID_FS_LABEL_ENC");
}

static char *disc_get_cat (struct udev_device *dev)
{
    const char *val;
    const char *cat = "Unknown";

    val = udev_device_get_property_value (dev, "ID_CDROM_MEDIA_CD");
    if (val && atoi (val))
        cat = "CD";
    val = udev_device_get_property_value (dev, "ID_CDROM_MEDIA_DVD");
    if (val && atoi (val))
        cat = "DVD";
    val = udev_device_get_property_value (dev, "ID_CDROM_MEDIA_BD");
    if (val && atoi (val))
        cat = "Blue-ray disc";
    val = udev_device_get_property_value (dev, "ID_CDROM_MEDIA_HDDVD");
    if (val && atoi (val))
        cat = "HD DVD";

    return strdup (cat);
}

int OpenDisc (vlc_object_t *obj)
{
    static const struct subsys subsys = {
        "block", disc_get_mrl, disc_get_name, disc_get_cat, ITEM_TYPE_DISC,
    };

    return Open (obj, &subsys);
}
