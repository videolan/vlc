
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include "objects.h"


gl_scene_objects_display_t *loadSceneObjects(vlc_gl_t *gl, const opengl_tex_converter_t *tc)
{
    gl_scene_objects_display_t *p_objDisplay = calloc(1, sizeof(gl_scene_objects_display_t));
    if (unlikely(p_objDisplay == NULL))
        return NULL;

    p_objDisplay->gl = gl;
    p_objDisplay->tc = tc;

    object_loader_t *p_objLoader = objLoader_get((vlc_object_t *)p_objDisplay->gl);
    if (unlikely(p_objLoader == NULL))
    {
        msg_Err(p_objDisplay->gl, "Could not load the 3d object loader");
        goto error;
    }

    char *psz_path = config_GetSysPath(VLC_PKG_DATA_DIR, "VirtualTheater" DIR_SEP "virtualCinemaTargo.json");
    if (unlikely(psz_path == NULL))
    {
        msg_Err(p_objDisplay->gl, "Could not load get the 3d scene path");
        goto error;
    }

    scene_t *p_scene = p_objDisplay->p_scene = objLoader_loadScene(p_objLoader, psz_path);

    if (p_scene == NULL)
    {
        msg_Err(p_objDisplay->gl, "Could not load the 3d scene at path: %s", psz_path);
        goto error;
    }

    objLoader_release(p_objLoader);

    // Allocate buffer ojects indices arrays.
    p_objDisplay->vertex_buffer_object = (GLuint *)malloc(p_scene->nMeshes * sizeof(GLuint));
    if (unlikely(p_objDisplay->vertex_buffer_object == NULL))
        goto error;

    p_objDisplay->normal_buffer_object = (GLuint *)malloc(p_scene->nMeshes * sizeof(GLuint));
    if (unlikely(p_objDisplay->normal_buffer_object == NULL))
        goto error;

    p_objDisplay->tangent_buffer_object = (GLuint *)malloc(p_scene->nMeshes * sizeof(GLuint));
    if (unlikely(p_objDisplay->tangent_buffer_object == NULL))
        goto error;

    p_objDisplay->index_buffer_object = (GLuint *)malloc(p_scene->nMeshes * sizeof(GLuint));
    if (unlikely(p_objDisplay->index_buffer_object == NULL))
        goto error;

    p_objDisplay->texture_buffer_object = (GLuint *)malloc(p_scene->nMeshes * sizeof(GLuint));
    if (unlikely(p_objDisplay->texture_buffer_object == NULL))
        goto error;

    p_objDisplay->texturesBaseColor = (GLuint *)malloc(p_scene->nMaterials * sizeof(GLuint));
    if (unlikely(p_objDisplay->texturesBaseColor == NULL))
        goto error;

    p_objDisplay->texturesMetalness = (GLuint *)malloc(p_scene->nMaterials * sizeof(GLuint));
    if (unlikely(p_objDisplay->texturesMetalness == NULL))
        goto error;

    p_objDisplay->texturesNormal = (GLuint *)malloc(p_scene->nMaterials * sizeof(GLuint));
    if (unlikely(p_objDisplay->texturesNormal == NULL))
        goto error;

    p_objDisplay->texturesRoughness = (GLuint *)malloc(p_scene->nMaterials * sizeof(GLuint));
    if (unlikely(p_objDisplay->texturesRoughness == NULL))
        goto error;

    if (loadBufferObjects(p_objDisplay) != VLC_SUCCESS)
        goto error;

    return p_objDisplay;

error:
    if (p_objDisplay)
        objLoader_release(p_objLoader);
    free(p_objDisplay->texturesRoughness);
    free(p_objDisplay->texturesNormal);
    free(p_objDisplay->texturesMetalness);
    free(p_objDisplay->texturesBaseColor);
    free(p_objDisplay->vertex_buffer_object);
    free(p_objDisplay->normal_buffer_object);
    free(p_objDisplay->tangent_buffer_object);
    free(p_objDisplay->index_buffer_object);
    free(p_objDisplay->texture_buffer_object);
    free(p_objDisplay);
    return NULL;
}


void releaseSceneObjects(gl_scene_objects_display_t *p_objDisplay)
{
    if (p_objDisplay == NULL)
        return;

    if (p_objDisplay->p_scene != NULL)
    {
        p_objDisplay->tc->vt->DeleteTextures(p_objDisplay->p_scene->nMaterials,
                                             p_objDisplay->texturesBaseColor);
        p_objDisplay->tc->vt->DeleteTextures(p_objDisplay->p_scene->nMaterials,
                                             p_objDisplay->texturesMetalness);
        p_objDisplay->tc->vt->DeleteTextures(p_objDisplay->p_scene->nMaterials,
                                             p_objDisplay->texturesNormal);
        p_objDisplay->tc->vt->DeleteTextures(p_objDisplay->p_scene->nMaterials,
                                             p_objDisplay->texturesRoughness);
    }

    free(p_objDisplay->texturesRoughness);
    free(p_objDisplay->texturesNormal);
    free(p_objDisplay->texturesMetalness);
    free(p_objDisplay->texturesBaseColor);
    free(p_objDisplay->vertex_buffer_object);
    free(p_objDisplay->normal_buffer_object);
    free(p_objDisplay->tangent_buffer_object);
    free(p_objDisplay->index_buffer_object);
    free(p_objDisplay->texture_buffer_object);

    scene_Release(p_objDisplay->p_scene);
    free(p_objDisplay);
}


static void setTextureParameters(const opengl_tex_converter_t *tc, picture_t *p_pic, GLuint tex)
{
    const opengl_vtable_t *vt = tc->vt;

    vt->BindTexture(tc->tex_target, tex);

#if !defined(USE_OPENGL_ES2)
    /* Set the texture parameters */
    vt->TexParameterf(tc->tex_target, GL_TEXTURE_PRIORITY, 1.0);
    vt->TexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif

    vt->TexParameteri(tc->tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    vt->TexParameteri(tc->tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    vt->TexParameteri(tc->tex_target, GL_TEXTURE_WRAP_S, GL_REPEAT);
    vt->TexParameteri(tc->tex_target, GL_TEXTURE_WRAP_T, GL_REPEAT);

    GLsizei tex_width = p_pic->format.i_width;
    GLsizei tex_height = p_pic->format.i_height;

    vt->TexImage2D(tc->tex_target, 0, tc->texs[0].internal, tex_width, tex_height, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, p_pic->p[0].p_pixels);
    vt->GenerateMipmap(GL_TEXTURE_2D);
}


int loadBufferObjects(gl_scene_objects_display_t *p_objDisplay)
{
    const opengl_tex_converter_t *tc = p_objDisplay->tc;
    const opengl_vtable_t *vt = tc->vt;

    unsigned nMeshes = p_objDisplay->p_scene->nMeshes;
    unsigned nMaterials = p_objDisplay->p_scene->nMaterials;
    unsigned nLights = p_objDisplay->p_scene->nLights;

    vt->GenBuffers(nMeshes, p_objDisplay->vertex_buffer_object);
    vt->GenBuffers(nMeshes, p_objDisplay->normal_buffer_object);
    vt->GenBuffers(nMeshes, p_objDisplay->tangent_buffer_object);
    vt->GenBuffers(nMeshes, p_objDisplay->index_buffer_object);
    vt->GenBuffers(nMeshes, p_objDisplay->texture_buffer_object);


    for (unsigned i = 0; i < nMeshes; ++i)
    {
        scene_mesh_t *p_mesh = p_objDisplay->p_scene->meshes[i];
#if 0
        for(unsigned j = 0; j < p_mesh->nVertices; ++j)
            msg_Err(p_objDisplay->gl, "%f %f %f - %f %f",
                    p_mesh->vCoords[3*j], p_mesh->vCoords[3*j + 1], p_mesh->vCoords[3*j + 2],
                    p_mesh->tCoords[2*j], p_mesh->tCoords[2*j + 1]);
#endif

        vt->BindBuffer(GL_ARRAY_BUFFER, p_objDisplay->vertex_buffer_object[i]);
        vt->BufferData(GL_ARRAY_BUFFER, p_mesh->nVertices * 3 * sizeof(*p_mesh->vCoords),
                       p_mesh->vCoords, GL_STATIC_DRAW);

        vt->BindBuffer(GL_ARRAY_BUFFER, p_objDisplay->normal_buffer_object[i]);
        vt->BufferData(GL_ARRAY_BUFFER, p_mesh->nVertices * 3 * sizeof(*p_mesh->nCoords),
                       p_mesh->nCoords, GL_STATIC_DRAW);

        vt->BindBuffer(GL_ARRAY_BUFFER, p_objDisplay->tangent_buffer_object[i]);
        vt->BufferData(GL_ARRAY_BUFFER, p_mesh->nVertices * 3 * sizeof(*p_mesh->tanCoords),
                       p_mesh->tanCoords, GL_STATIC_DRAW);

        vt->BindBuffer(GL_ARRAY_BUFFER, p_objDisplay->texture_buffer_object[i]);
        vt->BufferData(GL_ARRAY_BUFFER,  p_mesh->nVertices * 2 * sizeof(*p_mesh->tCoords),
                       p_mesh->tCoords, GL_STATIC_DRAW);

        vt->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, p_objDisplay->index_buffer_object[i]);
        vt->BufferData(GL_ELEMENT_ARRAY_BUFFER, p_mesh->nFaces * 3 * sizeof(*p_mesh->faces),
                       p_mesh->faces, GL_STATIC_DRAW);
    }


    vt->GenTextures(nMaterials, p_objDisplay->texturesBaseColor);
    vt->GenTextures(nMaterials, p_objDisplay->texturesMetalness);
    vt->GenTextures(nMaterials, p_objDisplay->texturesNormal);
    vt->GenTextures(nMaterials, p_objDisplay->texturesRoughness);

    tc->vt->PixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    for (unsigned i = 0; i < nMaterials; i++)
    {
        scene_material_t *p_material = p_objDisplay->p_scene->materials[i];

        if (p_material->p_baseColorTex != NULL)
            setTextureParameters(tc, p_material->p_baseColorTex, p_objDisplay->texturesBaseColor[i]);
        if (p_material->p_metalnessTex != NULL)
            setTextureParameters(tc, p_material->p_metalnessTex, p_objDisplay->texturesMetalness[i]);
        if (p_material->p_normalTex != NULL)
            setTextureParameters(tc, p_material->p_normalTex, p_objDisplay->texturesNormal[i]);
        if (p_material->p_roughnessTex != NULL)
            setTextureParameters(tc, p_material->p_roughnessTex, p_objDisplay->texturesRoughness[i]);
    }

    p_objDisplay->light_count = nLights;

    for (unsigned i = 0; i < nLights; ++i)
    {
        memcpy(p_objDisplay->lights.position[i], p_objDisplay->p_scene->lights[i]->position, 3 * sizeof(float));
        memcpy(p_objDisplay->lights.ambient[i], p_objDisplay->p_scene->lights[i]->colorAmbient, 3 * sizeof(float));
        memcpy(p_objDisplay->lights.diffuse[i], p_objDisplay->p_scene->lights[i]->colorDiffuse, 3 * sizeof(float));
        memcpy(p_objDisplay->lights.specular[i], p_objDisplay->p_scene->lights[i]->colorSpecular, 3 * sizeof(float));
        memcpy(p_objDisplay->lights.direction[i], p_objDisplay->p_scene->lights[i]->direction, 3 * sizeof(float));

        p_objDisplay->lights.k_c[i] = p_objDisplay->p_scene->lights[i]->attenuationConstant;
        p_objDisplay->lights.k_l[i] = p_objDisplay->p_scene->lights[i]->attenuationLinear;
        p_objDisplay->lights.k_q[i] = p_objDisplay->p_scene->lights[i]->attenuationQuadratic;
    }

    unsigned nObjects = p_objDisplay->p_scene->nObjects;

    // TODO: check MapBufferAvailability
    vt->GenBuffers(1, &p_objDisplay->transform_buffer_object);
    vt->BindBuffer(GL_ARRAY_BUFFER, p_objDisplay->transform_buffer_object);

    // Initialize the vertex buffer object storage and map it into memory to fill it
    vt->BufferData(GL_ARRAY_BUFFER, 16*nObjects*sizeof(GLfloat),
                 NULL, GL_STATIC_DRAW);
    float *transform_buffer = vt->MapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

    assert(transform_buffer != NULL);

    for (unsigned i = 0; i < nObjects; ++i)
    {
        scene_object_t* p_object = p_objDisplay->p_scene->objects[i];
        memcpy(transform_buffer + 16*i, p_object->transformMatrix, 16*sizeof(float));
    }
    vt->UnmapBuffer(GL_ARRAY_BUFFER);

    return VLC_SUCCESS;
}

