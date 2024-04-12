/*****************************************************************************
 * upnp_server.cpp : UPnP server module
 *****************************************************************************
 * Copyright © 2024 VLC authors and VideoLAN
 *
 * Authors: Hamza Parnica <hparnica@gmail.com>
 *          Alaric Senat <alaric@videolabs.io>
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
#include "config.h"
#endif

#include <atomic>
#include <cstring>
#include <services_discovery/upnp-wrapper.hpp>

#include <vlc_common.h>

#include <vlc_addons.h>
#include <vlc_interface.h>
#include <vlc_rand.h>

#include "ml.hpp"
#include "upnp_server.hpp"
#include "utils.hpp"
#include "xml_wrapper.hpp"

#define CDS_ID "urn:upnp-org:serviceId:ContentDirectory"
#define CMS_ID "urn:upnp-org:serviceId:ConnectionManager"

#define UPNP_SERVICE_TYPE(service) "urn:schemas-upnp-org:service:" service ":1"

struct intf_sys_t
{
    ml::MediaLibraryContext p_ml;
    std::unique_ptr<vlc_ml_event_callback_t, std::function<void(vlc_ml_event_callback_t *)>>
        ml_callback_handle;

    std::unique_ptr<char, std::function<void(void *)>> uuid;

    UpnpDevice_Handle p_device_handle;
    std::unique_ptr<UpnpInstanceWrapper, std::function<void(UpnpInstanceWrapper *)>> upnp;

    // This integer is atomically incremented at each medialib modification. It will be sent in each
    // response. If the client notice that the update id has been incremented since the last
    // request, he knows that the server state has changed and hence can refetch the exposed
    // hierarchy.
    std::atomic<unsigned int> upnp_update_id;
};

static void medialibrary_event_callback(void *p_data, const struct vlc_ml_event_t *p_event)
{
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    intf_sys_t *p_sys = p_intf->p_sys;

    switch (p_event->i_type)
    {
        case VLC_ML_EVENT_MEDIA_ADDED:
        case VLC_ML_EVENT_MEDIA_UPDATED:
        case VLC_ML_EVENT_MEDIA_DELETED:
        case VLC_ML_EVENT_ARTIST_ADDED:
        case VLC_ML_EVENT_ARTIST_UPDATED:
        case VLC_ML_EVENT_ARTIST_DELETED:
        case VLC_ML_EVENT_ALBUM_ADDED:
        case VLC_ML_EVENT_ALBUM_UPDATED:
        case VLC_ML_EVENT_ALBUM_DELETED:
        case VLC_ML_EVENT_PLAYLIST_ADDED:
        case VLC_ML_EVENT_PLAYLIST_UPDATED:
        case VLC_ML_EVENT_PLAYLIST_DELETED:
        case VLC_ML_EVENT_GENRE_ADDED:
        case VLC_ML_EVENT_GENRE_UPDATED:
        case VLC_ML_EVENT_GENRE_DELETED:
            p_sys->upnp_update_id++;
            break;
    }
}

static void handle_action_request(UpnpActionRequest *p_request, intf_thread_t *p_intf)
{
    intf_sys_t *sys = p_intf->p_sys;

    IXML_Document *action_result = nullptr;
    const char *service_id = UpnpActionRequest_get_ServiceID_cstr(p_request);
    const char *action_name = UpnpActionRequest_get_ActionName_cstr(p_request);
    const auto client_addr = utils::addr_to_string(UpnpActionRequest_get_CtrlPtIPAddr(p_request));
    msg_Dbg(p_intf,
            "Received action request \"%s\" for service \"%s\" from %s",
            action_name,
            service_id,
            client_addr.c_str());
    if (strcmp(service_id, CMS_ID) == 0)
    {
        static constexpr char cms_type[] = UPNP_SERVICE_TYPE("ConnectionManager");
        if (strcmp(action_name, "GetProtocolInfo") == 0)
        {
            UpnpAddToActionResponse(
                &action_result, action_name, cms_type, "Source", "http-get:*:*:*");
            UpnpAddToActionResponse(&action_result, action_name, cms_type, "Sink", "");
            UpnpActionRequest_set_ActionResult(p_request, action_result);
        }
        else if (strcmp(action_name, "GetCurrentConnectionIDs") == 0)
        {
            UpnpAddToActionResponse(&action_result, action_name, cms_type, "ConnectionIDs", "");
            UpnpActionRequest_set_ActionResult(p_request, action_result);
        }
    }

    else if (strcmp(service_id, CDS_ID) == 0)
    {
        static constexpr char cds_type[] = UPNP_SERVICE_TYPE("ContentDirectory");

        if (strcmp(action_name, "Browse") == 0)
        {
            msg_Err(p_intf, "server: failed to respond to browse action request");
        }
        else if (strcmp(action_name, "GetSearchCapabilities") == 0)
        {
            UpnpAddToActionResponse(&action_result, action_name, cds_type, "SearchCaps", "");
            UpnpActionRequest_set_ActionResult(p_request, action_result);
        }
        else if (strcmp(action_name, "GetSortCapabilities") == 0)
        {
            UpnpAddToActionResponse(&action_result, action_name, cds_type, "SortCaps", "");
            UpnpActionRequest_set_ActionResult(p_request, action_result);
        }
        else if (strcmp(action_name, "GetSystemUpdateID") == 0)
        {
            char *psz_update_id;
            if (asprintf(&psz_update_id, "%d", sys->upnp_update_id.load()) == -1)
                return;
            auto up_update_id = vlc::wrap_cptr(psz_update_id);
            UpnpAddToActionResponse(&action_result, action_name, cds_type, "Id", psz_update_id);
            UpnpActionRequest_set_ActionResult(p_request, action_result);
        }
    }
}

static int Callback(Upnp_EventType event_type, const void *event, void *cookie)
{
    auto *intf = static_cast<intf_thread_t *>(cookie);

    switch (event_type)
    {
        case UPNP_CONTROL_ACTION_REQUEST:
        {
            msg_Dbg(intf, "Action request");
            // We need to const_cast here because the upnp callback has to take a const void* for
            // the event data even if the sdk also expect us to modify it sometimes (in the case of
            // a upnp response for example)
            auto *rq =
                const_cast<UpnpActionRequest *>(static_cast<const UpnpActionRequest *>(event));
            handle_action_request(rq, intf);
        }
        break;
        case UPNP_CONTROL_GET_VAR_REQUEST:
            msg_Dbg(intf, "Var request");
            break;

        case UPNP_EVENT_SUBSCRIPTION_REQUEST:
            msg_Dbg(intf, "Sub request");
            break;

        default:
            msg_Err(intf, "Unhandled event: %d", event_type);
            return UPNP_E_INVALID_ACTION;
    }

    return UPNP_E_SUCCESS;
}

static xml::Document make_server_identity(const char *uuid, const char *server_name)
{
    xml::Document ret;

    const auto service_elem = [&ret](const char *service) -> xml::Element {
        const auto type = std::string("urn:schemas-upnp-org:service:") + service + ":1";
        const auto id = std::string("urn:upnp-org:serviceId:") + service;
        const auto scpd_url = std::string("/") + service + ".xml";
        const auto control_url = std::string("/") + service + "/Control";
        const auto event_url = std::string("/") + service + "/Event";
        return ret.create_element(
            "service",
            ret.create_element("serviceType", ret.create_text_node(type.c_str())),
            ret.create_element("serviceId", ret.create_text_node(id.c_str())),
            ret.create_element("SCPDURL", ret.create_text_node(scpd_url.c_str())),
            ret.create_element("controlURL", ret.create_text_node(control_url.c_str())),
            ret.create_element("eventSubURL", ret.create_text_node(event_url.c_str())));
    };

    const std::string url = utils::get_server_url();

    const auto uuid_attr = std::string("uuid:") + uuid;

    xml::Element dlna_doc = ret.create_element("dlna:X_DLNADOC", ret.create_text_node("DMS-1.50"));
    dlna_doc.set_attribute("xmlns:dlna", "urn:schemas-dlna-org:device-1-0");

    xml::Element root = ret.create_element(
        "root",
        ret.create_element("specVersion",
                           ret.create_element("major", ret.create_text_node("1")),
                           ret.create_element("minor", ret.create_text_node("0"))),
        ret.create_element(
            "device",
            std::move(dlna_doc),

            ret.create_element("deviceType",
                               ret.create_text_node("urn:schemas-upnp-org:device:MediaServer:1")),
            ret.create_element("presentationUrl", ret.create_text_node(url.c_str())),
            ret.create_element("friendlyName", ret.create_text_node(server_name)),
            ret.create_element("manufacturer", ret.create_text_node("VideoLAN")),
            ret.create_element("manufacturerURL", ret.create_text_node("https://videolan.org")),
            ret.create_element("modelDescription", ret.create_text_node("VLC UPNP Media Server")),
            ret.create_element("modelName", ret.create_text_node("VLC")),
            ret.create_element("modelNumber", ret.create_text_node(PACKAGE_VERSION)),
            ret.create_element("modelURL", ret.create_text_node("https://videolan.org/vlc/")),
            ret.create_element("serialNumber", ret.create_text_node("1")),
            ret.create_element("UDN", ret.create_text_node(uuid_attr.c_str())),
            ret.create_element("serviceList",
                               service_elem("ConnectionManager"),
                               service_elem("ContentDirectory"))));
    root.set_attribute("xmlns", "urn:schemas-upnp-org:device-1-0");

    ret.set_entry(std::move(root));
    return ret;
}

static bool init_upnp(intf_thread_t *intf)
{
    intf_sys_t *sys = intf->p_sys;

    addon_uuid_t uuid;
    vlc_rand_bytes(uuid, sizeof(uuid));
    sys->uuid = vlc::wrap_cptr(addons_uuid_to_psz(&uuid), &free);

    int res;
    const auto server_name = vlc::wrap_cptr(var_InheritString(intf, SERVER_PREFIX "name"), &free);
    assert(server_name);
    const auto presentation_doc = make_server_identity(sys->uuid.get(), server_name.get());
    const auto up_presentation_str = presentation_doc.to_wrapped_cstr();
    msg_Dbg(intf, "%s", up_presentation_str.get());
    res = UpnpRegisterRootDevice2(UPNPREG_BUF_DESC,
                                  up_presentation_str.get(),
                                  strlen(up_presentation_str.get()),
                                  1,
                                  Callback,
                                  intf,
                                  &sys->p_device_handle);
    if (res != UPNP_E_SUCCESS)
    {
        msg_Err(intf, "server: registration failed: %s", UpnpGetErrorMessage(res));
        return false;
    }

    res = UpnpSendAdvertisement(sys->p_device_handle, 1800);
    if (res != UPNP_E_SUCCESS)
    {
        msg_Dbg(intf, "Advertisement failed: %s", UpnpGetErrorMessage(res));
        UpnpUnRegisterRootDevice(sys->p_device_handle);
        return false;
    }

    sys->upnp_update_id = 0;

    return true;
}

namespace Server
{

int open(vlc_object_t *p_this)
{
    intf_thread_t *intf = container_of(p_this, intf_thread_t, obj);
    intf_sys_t *sys = new (std::nothrow) intf_sys_t;
    if (unlikely(sys == nullptr))
        return VLC_ENOMEM;
    intf->p_sys = sys;

    sys->p_ml = ml::MediaLibraryContext{vlc_ml_instance_get(p_this)};
    if (!sys->p_ml.handle)
    {
        msg_Err(intf, "Medialibrary not initialized");
        delete sys;
        return VLC_EGENERIC;
    }

    vlc_ml_event_callback_t *cb =
        vlc_ml_event_register_callback(sys->p_ml.handle, medialibrary_event_callback, intf);
    const auto release_cb = [sys](vlc_ml_event_callback_t *cb) {
        vlc_ml_event_unregister_callback(sys->p_ml.handle, cb);
    };
    sys->ml_callback_handle = {cb, release_cb};

    sys->upnp = vlc::wrap_cptr(UpnpInstanceWrapper::get(p_this),
                               [](UpnpInstanceWrapper *p_upnp) { p_upnp->release(); });

    if (!init_upnp(intf))
    {
        delete sys;
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

void close(vlc_object_t *p_this)
{
    intf_thread_t *intf = container_of(p_this, intf_thread_t, obj);
    intf_sys_t *sys = intf->p_sys;
    UpnpUnRegisterRootDevice(sys->p_device_handle);
    delete sys;
}
} // namespace Server
