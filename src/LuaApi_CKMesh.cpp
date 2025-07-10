#include "LuaApi.h"

#include <CKMesh.h>
#include <CKMaterial.h>
#include <CKRenderContext.h>
#include <CK3dEntity.h>
#include <VxVector.h>

void LuaApi::RegisterCKMesh(sol::state &lua) {
    // ===================================================================
    //  CKMesh - Representation of the geometry of a 3D object
    // ===================================================================
    auto ckMeshType = lua.new_usertype<CKMesh>(
        "CKMesh",
        sol::no_constructor,
        sol::base_classes, sol::bases<CKBeObject, CKSceneObject, CKObject>(),

        // Flags
        "is_transparent", &CKMesh::IsTransparent,
        "set_transparent", &CKMesh::SetTransparent,
        "set_wrap_mode", &CKMesh::SetWrapMode,
        "get_wrap_mode", &CKMesh::GetWrapMode,
        "set_lit_mode", &CKMesh::SetLitMode,
        "get_lit_mode", &CKMesh::GetLitMode,
        "get_flags", &CKMesh::GetFlags,
        "set_flags", &CKMesh::SetFlags,

        // Vertex access
        "get_vertex_count", &CKMesh::GetVertexCount,
        "set_vertex_count", &CKMesh::SetVertexCount,

        // Vertex properties
        "set_vertex_color", &CKMesh::SetVertexColor,
        "set_vertex_specular_color", &CKMesh::SetVertexSpecularColor,
        "set_vertex_normal", &CKMesh::SetVertexNormal,
        "set_vertex_position", &CKMesh::SetVertexPosition,
        "set_vertex_texture_coordinates", sol::overload(
            [](CKMesh &mesh, int index, float u, float v) { mesh.SetVertexTextureCoordinates(index, u, v); },
            [](CKMesh &mesh, int index, float u, float v, int channel) { mesh.SetVertexTextureCoordinates(index, u, v, channel); }
        ),

        "get_vertex_color", &CKMesh::GetVertexColor,
        "get_vertex_specular_color", &CKMesh::GetVertexSpecularColor,
        "get_vertex_normal", &CKMesh::GetVertexNormal,
        "get_vertex_position", &CKMesh::GetVertexPosition,
        "get_vertex_texture_coordinates", sol::overload(
            [](CKMesh &mesh, int index, float *u, float *v) { mesh.GetVertexTextureCoordinates(index, u, v); },
            [](CKMesh &mesh, int index, float *u, float *v, int channel) { mesh.GetVertexTextureCoordinates(index, u, v, channel); }
        ),

        // Vertex transformations
        "translate_vertices", &CKMesh::TranslateVertices,
        "scale_vertices", sol::overload(
            [](CKMesh &mesh, VxVector *vec) { mesh.ScaleVertices(vec); },
            [](CKMesh &mesh, VxVector *vec, VxVector *pivot) { mesh.ScaleVertices(vec, pivot); }
            // [](CKMesh &mesh, float x, float y, float z) { mesh.ScaleVertices3f(x, y, z); },
            // [](CKMesh &mesh, float x, float y, float z, VxVector *pivot) { mesh.ScaleVertices3f(x, y, z, pivot); }
        ),
        "rotate_vertices", &CKMesh::RotateVertices,

        // Vertex notifications
        "vertex_move", &CKMesh::VertexMove,
        "uv_changed", &CKMesh::UVChanged,
        "normal_changed", &CKMesh::NormalChanged,
        "color_changed", &CKMesh::ColorChanged,

        // Face access
        "get_face_count", &CKMesh::GetFaceCount,
        "set_face_count", &CKMesh::SetFaceCount,
        // "get_faces_indices", &CKMesh::GetFacesIndices,
        // "get_face_vertex_index", &CKMesh::GetFaceVertexIndex,
        "get_face_material", &CKMesh::GetFaceMaterial,
        "get_face_normal", &CKMesh::GetFaceNormal,
        "get_face_channel_mask", &CKMesh::GetFaceChannelMask,
        "get_face_vertex", &CKMesh::GetFaceVertex,

        "set_face_vertex_index", &CKMesh::SetFaceVertexIndex,
        "set_face_material", &CKMesh::SetFaceMaterial,
        // "set_face_material_ex", &CKMesh::SetFaceMaterialEx,
        "set_face_channel_mask", &CKMesh::SetFaceChannelMask,
        "replace_material", &CKMesh::ReplaceMaterial,
        "change_face_channel_mask", &CKMesh::ChangeFaceChannelMask,
        "apply_global_material", &CKMesh::ApplyGlobalMaterial,
        "dissociate_all_faces", &CKMesh::DissociateAllFaces,

        // Lines
        "set_line_count", &CKMesh::SetLineCount,
        "get_line_count", &CKMesh::GetLineCount,
        // "get_line_indices", &CKMesh::GetLineIndices,
        "set_line", &CKMesh::SetLine,
        "get_line", &CKMesh::GetLine,
        "create_line_strip", &CKMesh::CreateLineStrip,

        // Utilities
        "clean", sol::overload(
            [](CKMesh &mesh) { mesh.Clean(); },
            [](CKMesh &mesh, CKBOOL keep_vertices) { mesh.Clean(keep_vertices); }
        ),
        "inverse_winding", &CKMesh::InverseWinding,
        "consolidate", &CKMesh::Consolidate,
        "un_optimize", &CKMesh::UnOptimize,

        // Bounding volumes
        "get_radius", &CKMesh::GetRadius,
        "get_local_box", &CKMesh::GetLocalBox,
        "get_bary_center", &CKMesh::GetBaryCenter,

        // Material channels
        "get_channel_count", &CKMesh::GetChannelCount,
        "add_channel", sol::overload(
            [](CKMesh &mesh, CKMaterial *mat) { return mesh.AddChannel(mat); },
            [](CKMesh &mesh, CKMaterial *mat, CKBOOL copy_uv) { return mesh.AddChannel(mat, copy_uv); }
        ),
        "remove_channel_by_material", &CKMesh::RemoveChannelByMaterial,
        "remove_channel", &CKMesh::RemoveChannel,
        "get_channel_by_material", &CKMesh::GetChannelByMaterial,
        "activate_channel", sol::overload(
            [](CKMesh &mesh, int index) { mesh.ActivateChannel(index); },
            [](CKMesh &mesh, int index, CKBOOL active) { mesh.ActivateChannel(index, active); }
        ),
        "is_channel_active", &CKMesh::IsChannelActive,
        "activate_all_channels", sol::overload(
            [](CKMesh &mesh) { mesh.ActivateAllChannels(); },
            [](CKMesh &mesh, CKBOOL active) { mesh.ActivateAllChannels(active); }
        ),
        "lit_channel", sol::overload(
            [](CKMesh &mesh, int index) { mesh.LitChannel(index); },
            [](CKMesh &mesh, int index, CKBOOL lit) { mesh.LitChannel(index, lit); }
        ),
        "is_channel_lit", &CKMesh::IsChannelLit,
        "get_channel_flags", &CKMesh::GetChannelFlags,
        "set_channel_flags", &CKMesh::SetChannelFlags,
        "get_channel_material", &CKMesh::GetChannelMaterial,
        "get_channel_source_blend", &CKMesh::GetChannelSourceBlend,
        "get_channel_dest_blend", &CKMesh::GetChannelDestBlend,
        "set_channel_material", &CKMesh::SetChannelMaterial,
        "set_channel_source_blend", &CKMesh::SetChannelSourceBlend,
        "set_channel_dest_blend", &CKMesh::SetChannelDestBlend,

        // Normals
        "build_normals", &CKMesh::BuildNormals,
        "build_face_normals", &CKMesh::BuildFaceNormals,

        // Rendering
        // "render", &CKMesh::Render,

        // Material access
        "get_material_count", &CKMesh::GetMaterialCount,
        "get_material", &CKMesh::GetMaterial,

        // Vertex weights
        "get_vertex_weights_count", &CKMesh::GetVertexWeightsCount,
        "set_vertex_weights_count", &CKMesh::SetVertexWeightsCount,
        // "get_vertex_weights_ptr", &CKMesh::GetVertexWeightsPtr,
        "get_vertex_weight", &CKMesh::GetVertexWeight,
        "set_vertex_weight", &CKMesh::SetVertexWeight

        // Progressive mesh
        // "set_vertices_rendered", &CKMesh::SetVerticesRendered,
        // "get_vertices_rendered", &CKMesh::GetVerticesRendered,
        // "create_pm", &CKMesh::CreatePM,
        // "destroy_pm", &CKMesh::DestroyPM,
        // "is_pm", &CKMesh::IsPM,
        // "enable_pm_geo_morph", &CKMesh::EnablePMGeoMorph,
        // "is_pm_geo_morph_enabled", &CKMesh::IsPMGeoMorphEnabled,
        // "set_pm_geo_morph_step", &CKMesh::SetPMGeoMorphStep,
        // "get_pm_geo_morph_step", &CKMesh::GetPMGeoMorphStep,

        // Callbacks - Note: Callback functions might need special handling
        // "add_pre_render_callback", [](CKMesh &mesh) {
        //     throw sol::error("Mesh render callbacks from Lua not yet implemented");
        // },
        // "remove_pre_render_callback", [](CKMesh &mesh) {
        //     throw sol::error("Mesh render callbacks from Lua not yet implemented");
        // },
        // "add_post_render_callback", [](CKMesh &mesh) {
        //     throw sol::error("Mesh render callbacks from Lua not yet implemented");
        // },
        // "remove_post_render_callback", [](CKMesh &mesh) {
        //     throw sol::error("Mesh render callbacks from Lua not yet implemented");
        // },
        // "set_render_callback", [](CKMesh &mesh) {
        //     throw sol::error("Mesh render callbacks from Lua not yet implemented");
        // },
        // "set_default_render_callback", &CKMesh::SetDefaultRenderCallBack,
        // "remove_all_callbacks", &CKMesh::RemoveAllCallbacks
    );
}