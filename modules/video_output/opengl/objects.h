#ifndef VLC_OPENGL_OBEJCTS_H
#define VLC_OPENGL_OBEJCTS_H

#include <vlc_object_loader.h>

#include "vout_helper.h"
#include "converter.h"

#define VLC_SCENE_MAX_LIGHT 30

typedef struct vlc_scene_lights_t {
    // light center
    float position[3][VLC_SCENE_MAX_LIGHT];
    // light intensity parameters
    float ambiant[VLC_SCENE_MAX_LIGHT];
    float diffuse[VLC_SCENE_MAX_LIGHT];
    float specular[VLC_SCENE_MAX_LIGHT];
    // distance attenuation factor (constant, linear, quadratic)
    float k_c[VLC_SCENE_MAX_LIGHT];
    float k_l[VLC_SCENE_MAX_LIGHT];
    float k_q[VLC_SCENE_MAX_LIGHT];
    // spot direction if in spot mode
    float spot_direction[3][VLC_SCENE_MAX_LIGHT];
    // spot cutoff in range [0, 90];
    float cutoff[VLC_SCENE_MAX_LIGHT];
};

typedef struct
{
    vlc_gl_t *gl;

    const opengl_tex_converter_t *tc;

    scene_t *p_scene;

    GLuint *vertex_buffer_object;
    GLuint *normal_buffer_object;
    GLuint *tangent_buffer_object;
    GLuint *index_buffer_object;
    GLuint *texture_buffer_object;

    GLuint *texturesBaseColor;
    GLuint *texturesMetalness;
    GLuint *texturesNormal;
    GLuint *texturesRoughness;

    vlc_scene_lights_t lights;

} gl_scene_objects_display_t;


gl_scene_objects_display_t *loadSceneObjects(const char *psz_path, vlc_gl_t *gl,
                                             const opengl_tex_converter_t *tc);
void releaseSceneObjects(gl_scene_objects_display_t *p_objDisplay);
int loadBufferObjects(gl_scene_objects_display_t *p_objDisplay);



#endif // VLC_OPENGL_OBEJCTS_H
