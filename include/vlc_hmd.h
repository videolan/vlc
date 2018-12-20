#ifndef VLC_HMD_H
#define VLC_HMD_H

#include <vlc_common.h>

enum vlc_hmd_state_e
{
    VLC_HMD_STATE_DISCONNECTED,
    VLC_HMD_STATE_DISABLED,
    VLC_HMD_STATE_ENABLED,
};
typedef enum vlc_hmd_state_e vlc_hmd_state_e;

enum vlc_hmd_param_e
{
    VLC_HMD_PARAM_TRANSFORMATION,
};


struct vlc_hmd_cfg_t
{
    int i_screen_width;
    int i_screen_height;

    float distorsion_coefs[4];
    float viewport_scale[2];
    float aberr_scale[3];

    struct {
        float projection[16];
        float modelview[16];
        float lens_center[2];
    } left, right;

    float warp_scale;
    float warp_adj;
};

/**
 * Holds the headset module itself
 */
struct vlc_hmd_driver_t
{
    struct vlc_common_members obj;

    /* Module */
    module_t* module;

    /* Private structure */
    void* sys;

    /* HMD parameters */
    vlc_hmd_cfg_t cfg;

    int listeners_count;
    struct vlc_hmd_interface **listeners;

    vlc_viewpoint_t (*get_viewpoint)(vlc_hmd_driver_t *driver);
    vlc_hmd_state_e (*get_state)(vlc_hmd_driver_t *driver);
    vlc_hmd_cfg_t   (*get_config)(vlc_hmd_driver_t *driver);
};

/**
 * Holds the headset device representation
 */
struct vlc_hmd_device_t
{
    struct vlc_common_members obj;
};

/**
 * These callbacks are called when the HMD has been sampled and the listener is
 * ready to process these events.
 */
struct vlc_hmd_interface_cbs_t
{
    /**
     * Called when the state of the headset is changed, for example when it is
     * disconnected or disabled by the user.
     */
    void (*state_changed)(struct vlc_hmd_interface_t *hmd,
                          enum vlc_hmd_state_e state,
                          void *userdata);

    /**
     * Called when the device's virtual screen has changed.
     *
     * TODO: screen shouldn't be represented by a screen num only, but should
     * use an opaque structure.
     */
    void (*screen_changed)(struct vlc_hmd_interface_t *hmd,
                           int screen_num,
                           void *userdata);

    /**
     * Called when an HMD parameter has changed, for example transformation
     * matrix or interpupillary distance.
     */
    void (*config_changed)(struct vlc_hmd_interface_t *hmd,
                           vlc_hmd_cfg_t cfg,
                           void *userdata);
};

/**
 * Listen to the headset state and triggers events
 * TODO: should be an opaque type?
 */
struct vlc_hmd_interface_t
{
    // TODO: must be private, new state would be pushed by the HMD in the back
    // state buffer
    //struct
    //{
    //    enum vlc_hmd_state_e state;
    //    vlc_viewpoint_t viewpoint;
    //    int screen_num;
    //    // TODO: params
    //} current_state, next_state;
};

/**
 * Triggers events from the HMD
 */
VLC_API int vlc_hmd_ReadEvents(vlc_hmd_interface_t *hmd);

VLC_API vlc_viewpoint_t vlc_hmd_ReadViewpoint(vlc_hmd_interface_t *hmd);

VLC_API
vlc_hmd_interface_t *vlc_hmd_MapDevice(vlc_hmd_device_t *hmd,
                                       const vlc_hmd_interface_cbs_t *cbs,
                                       void *userdata);

VLC_API void vlc_hmd_UnmapDevice(vlc_hmd_interface_t *hmd);

VLC_API vlc_hmd_device_t *vlc_hmd_FindDevice(vlc_object_t *parent,
                                             const char *modules,
                                             const char *name);



#endif
