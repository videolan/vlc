#ifndef VLC_OPENGL_OBEJCTS_H
#define VLC_OPENGL_OBEJCTS_H

#include <vlc_object_loader.h>

#include "vout_helper.h"
#include "converter.h"

typedef struct
{
    vlc_gl_t *gl;

    const opengl_tex_converter_t *tc;

    scene_t *p_scene;

    GLuint *vertex_buffer_object;
    GLuint *index_buffer_object;
    GLuint *texture_buffer_object;

    GLuint *texturesBaseColor;
    GLuint *texturesMetalness;
    GLuint *texturesNormal;
    GLuint *texturesRoughness;

} gl_scene_objects_display_t;


gl_scene_objects_display_t *loadSceneObjects(const char *psz_path, vlc_gl_t *gl,
                                             const opengl_tex_converter_t *tc);
void releaseSceneObjects(gl_scene_objects_display_t *p_objDisplay);
int loadBufferObjects(gl_scene_objects_display_t *p_objDisplay);



#endif // VLC_OPENGL_OBEJCTS_H
