/**
 * @file surface.c
 * @brief Vulkan platform-specific surface extension module
 */
/*****************************************************************************
 * Copyright © 2018 Niklas Haas
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

#include <stdlib.h>
#include <assert.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>

#include "../placebo_utils.h"
#include "instance.h"
#include "platform.h"

static int Open (vlc_object_t *obj)
{
    vlc_vk_t *vk = (vlc_vk_t *) obj;

    if (vlc_vk_InitPlatform(vk) != VLC_SUCCESS)
        goto error;

    // Initialize Vulkan instance
    vk->ctx = vlc_placebo_Create(VLC_OBJECT(vk));
    if (!vk->ctx)
        goto error;

    vk->instance = pl_vk_inst_create(vk->ctx, &(struct pl_vk_inst_params) {
        .debug = var_InheritBool(vk, "vk-debug"),
        .extensions = (const char *[]) {
            VK_KHR_SURFACE_EXTENSION_NAME,
            vlc_vk_PlatformExt,
        },
        .num_extensions = 2,
    });
    if (!vk->instance)
        goto error;

    // Create the platform-specific surface object
    if (vlc_vk_CreateSurface(vk) != VLC_SUCCESS)
        goto error;

    // Create vulkan device
    char *device_name = var_InheritString(vk, "vk-device");
    vk->vulkan = pl_vulkan_create(vk->ctx, &(struct pl_vulkan_params) {
        .instance = vk->instance->instance,
        .surface = vk->surface,
        .device_name = device_name,
        .allow_software = var_InheritBool(vk, "allow-sw"),
        .async_transfer = var_InheritBool(vk, "async-xfer"),
        .async_compute = var_InheritBool(vk, "async-comp"),
        .queue_count = var_InheritInteger(vk, "queue-count"),
    });
    free(device_name);
    if (!vk->vulkan)
        goto error;

    // Create swapchain for this surface
    struct pl_vulkan_swapchain_params swap_params = {
        .surface = vk->surface,
        .present_mode = var_InheritInteger(vk, "present-mode"),
        .swapchain_depth = var_InheritInteger(vk, "queue-depth"),
    };

    vk->swapchain = pl_vulkan_create_swapchain(vk->vulkan, &swap_params);
    if (!vk->swapchain)
        goto error;

    return VLC_SUCCESS;

error:
    pl_swapchain_destroy(&vk->swapchain);
    if (vk->surface)
        vkDestroySurfaceKHR(vk->instance->instance, vk->surface, NULL);

    pl_vulkan_destroy(&vk->vulkan);
    pl_vk_inst_destroy(&vk->instance);
    pl_context_destroy(&vk->ctx);
    vlc_vk_ClosePlatform(vk);

    return VLC_EGENERIC;
}

static void Close (vlc_object_t *obj)
{
    vlc_vk_t *vk = (vlc_vk_t *) obj;

    pl_swapchain_destroy(&vk->swapchain);
    vkDestroySurfaceKHR(vk->instance->instance, vk->surface, NULL);
    pl_vulkan_destroy(&vk->vulkan);
    pl_vk_inst_destroy(&vk->instance);
    pl_context_destroy(&vk->ctx);
    vlc_vk_ClosePlatform(vk);
}

#define DEBUG_TEXT "Enable API debugging"
#define DEBUG_LONGTEXT "This loads the vulkan standard validation layers, which can help catch API usage errors. Comes at a small performance penalty."

#define DEVICE_TEXT "Device name override"
#define DEVICE_LONGTEXT "If set to something non-empty, only a device with this exact name will be used. To see a list of devices and their names, run vlc -v with this module active."

#define ALLOWSW_TEXT "Allow software devices"
#define ALLOWSW_LONGTEXT "If enabled, allow the use of software emulation devices, which are not real devices and therefore typically very slow. (This option has no effect if forcing a specific device name)"

#define ASYNC_XFER_TEXT "Allow asynchronous transfer"
#define ASYNC_XFER_LONGTEXT "Allows the use of an asynchronous transfer queue if the device has one. Typically this maps to a DMA engine, which can perform texture uploads/downloads without blocking the GPU's compute units. Highly recommended for 4K and above."

#define ASYNC_COMP_TEXT "Allow asynchronous compute"
#define ASYNC_COMP_LONGTEXT "Allows the use of dedicated compute queue families if the device has one. Sometimes these will schedule concurrent compute work better than the main graphics queue. Turn this off if you have any issues."

#define QUEUE_COUNT_TEXT "Queue count"
#define QUEUE_COUNT_LONGTEXT "How many queues to use on the device. Increasing this might improve rendering throughput for GPUs capable of concurrent scheduling. Increasing this past the driver's limit has no effect."

#define QUEUE_DEPTH_TEXT "Maximum frame latency"
#define QUEUE_DEPTH_LONGTEXT "Affects how many frames to render/present in advance. Increasing this can improve performance at the cost of latency, by allowing better pipelining between frames. May have no effect, depending on the VLC clock settings."

static const int present_values[] = {
    VK_PRESENT_MODE_IMMEDIATE_KHR,
    VK_PRESENT_MODE_MAILBOX_KHR,
    VK_PRESENT_MODE_FIFO_KHR,
    VK_PRESENT_MODE_FIFO_RELAXED_KHR,
};

static const char * const present_text[] = {
    "Immediate (non-blocking, tearing)",
    "Mailbox (non-blocking, non-tearing)",
    "FIFO (blocking, non-tearing)",
    "Relaxed FIFO (blocking, tearing)",
};

#define PRESENT_MODE_TEXT "Preferred present mode"
#define PRESENT_MODE_LONGTEXT "Which present mode to use when creating the swapchain. If the chosen mode is not supported, VLC will fall back to using FIFO."

#define XSTR(x) #x
#define STR(x) XSTR(x)

vlc_module_begin ()
    set_shortname ("Vulkan/" STR(PLATFORM_NAME))
    set_description ("Vulkan context (" STR(PLATFORM_NAME) ")")
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("vulkan", 10)
    set_callbacks (Open, Close)

    set_section("Device selection", NULL)
    add_bool("vk-debug", false, DEBUG_TEXT, DEBUG_LONGTEXT, false)
    add_string("vk-device", "", DEVICE_TEXT, DEVICE_LONGTEXT, false)
    add_bool("allow-sw", pl_vulkan_default_params.allow_software,
            ALLOWSW_TEXT, ALLOWSW_LONGTEXT, false)

    set_section("Performance tuning", NULL)
    add_bool("async-xfer", pl_vulkan_default_params.async_transfer,
            ASYNC_XFER_TEXT, ASYNC_XFER_LONGTEXT, false)
    add_bool("async-comp", pl_vulkan_default_params.async_compute,
            ASYNC_COMP_TEXT, ASYNC_COMP_LONGTEXT, false)
    add_integer_with_range("queue-count", pl_vulkan_default_params.queue_count,
            1, 8, QUEUE_COUNT_TEXT, QUEUE_COUNT_LONGTEXT, false)
    add_integer_with_range("queue-depth", 3,
            1, 8, QUEUE_DEPTH_TEXT, QUEUE_DEPTH_LONGTEXT, false)
    add_integer("present-mode", VK_PRESENT_MODE_FIFO_KHR,
            PRESENT_MODE_TEXT, PRESENT_MODE_LONGTEXT, false)
            change_integer_list(present_values, present_text)

vlc_module_end ()
