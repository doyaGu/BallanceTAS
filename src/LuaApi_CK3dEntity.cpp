#include "LuaApi.h"

#include <CK3dEntity.h>
#include <CKObjectAnimation.h>
#include <CKMesh.h>
#include <CKPlace.h>
#include <VxMatrix.h>

void LuaApi::RegisterCK3dEntity(sol::state &lua) {
    // ===================================================================
    //  CK3dEntity - 3D objects with behaviors
    // ===================================================================
    auto ck3dEntityType = lua.new_usertype<CK3dEntity>(
        "CK3dEntity",
        sol::no_constructor,
        sol::base_classes, sol::bases<CKRenderObject, CKBeObject, CKSceneObject, CKObject>(),

        // Hierarchy
        "get_children_count", &CK3dEntity::GetChildrenCount,
        "get_child", &CK3dEntity::GetChild,
        "set_parent", sol::overload(
            [](CK3dEntity *entity, CK3dEntity *parent) -> bool { return entity->SetParent(parent); },
            [](CK3dEntity *entity, CK3dEntity *parent, bool keepWorldPos) {
                return entity->SetParent(parent, keepWorldPos);
            }
        ),
        "get_parent", &CK3dEntity::GetParent,
        "add_child", sol::overload(
            [](CK3dEntity *entity, CK3dEntity *child) -> bool { return entity->AddChild(child); },
            [](CK3dEntity *entity, CK3dEntity *child, bool keepWorldPos) {
                return entity->AddChild(child, keepWorldPos);
            }
        ),
        // "add_children", sol::overload(
        //     [](CK3dEntity *entity, const XObjectPointerArray &children) -> bool { return entity->AddChildren(children); },
        //     [](CK3dEntity *entity, const XObjectPointerArray &children, bool keepWorldPos) {
        //         return entity->AddChildren(children, keepWorldPos);
        //     }
        // ),
        "remove_child", &CK3dEntity::RemoveChild,
        "check_if_same_kind_of_hierarchy", sol::overload(
            [](CK3dEntity *entity, CK3dEntity *mov) -> bool { return entity->CheckIfSameKindOfHierarchy(mov); },
            [](CK3dEntity *entity, CK3dEntity *mov, bool sameOrder) {
                return entity->CheckIfSameKindOfHierarchy(mov, sameOrder);
            }
        ),
        "hierarchy_parser", &CK3dEntity::HierarchyParser,

        // Flags and properties
        "get_flags", &CK3dEntity::GetFlags,
        "set_flags", &CK3dEntity::SetFlags,
        "set_pickable", sol::overload(
            [](CK3dEntity *entity) { entity->SetPickable(); },
            [](CK3dEntity *entity, bool pick) { entity->SetPickable(pick); }
        ),
        "is_pickable", [](CK3dEntity *entity) -> bool { return entity->IsPickable(); },
        "set_render_channels", sol::overload(
            [](CK3dEntity *entity) { entity->SetRenderChannels(); },
            [](CK3dEntity *entity, bool renderChannels) { entity->SetRenderChannels(renderChannels); }
        ),
        "are_render_channels_visible", [](CK3dEntity *entity) -> bool {
            return entity->AreRenderChannelsVisible();
        },

        // View frustum tests
        // "is_in_view_frustum", sol::overload(
        //     [](CK3dEntity *entity, CKRenderContext *dev) -> bool { return entity->IsInViewFrustrum(dev); },
        //     [](CK3dEntity *entity, CKRenderContext *dev, CKDWORD flags) -> bool {
        //         return entity->IsInViewFrustrum(dev, flags);
        //     }
        // ),
        // "is_in_view_frustum_hierarchic", [](CK3dEntity *entity, CKRenderContext *dev) -> bool {
        //     return entity->IsInViewFrustrumHierarchic(dev);
        // },
        "is_all_inside_frustum", [](CK3dEntity *entity) -> bool {return entity->IsAllInsideFrustrum(); },
        "is_all_outside_frustum", [](CK3dEntity *entity) -> bool { return entity->IsAllOutsideFrustrum(); },

        // Animation control
        // "ignore_animations", sol::overload(
        //     [](CK3dEntity *entity) { entity->IgnoreAnimations(); },
        //     [](CK3dEntity *entity, bool ignore) { entity->IgnoreAnimations(ignore); }
        // ),
        // "are_animations_ignored", &CK3dEntity::AreAnimationIgnored,

        // Transparency
        "set_render_as_transparent", sol::overload(
            [](CK3dEntity *entity) { entity->SetRenderAsTransparent(); },
            [](CK3dEntity *entity, bool trans) { entity->SetRenderAsTransparent(trans); }
        ),

        // Moveable flags
        "get_moveable_flags", &CK3dEntity::GetMoveableFlags,
        "set_moveable_flags", &CK3dEntity::SetMoveableFlags,
        "modify_moveable_flags", &CK3dEntity::ModifyMoveableFlags,

        // Meshes
        // "get_current_mesh", &CK3dEntity::GetCurrentMesh,
        // "set_current_mesh", sol::overload(
        //     [](CK3dEntity *entity, CKMesh *mesh) { return entity->SetCurrentMesh(mesh); },
        //     [](CK3dEntity *entity, CKMesh *mesh, bool addIfNotHere) {
        //         return entity->SetCurrentMesh(mesh, addIfNotHere);
        //     }
        // ),
        // "get_mesh_count", &CK3dEntity::GetMeshCount,
        // "get_mesh", &CK3dEntity::GetMesh,
        // "add_mesh", &CK3dEntity::AddMesh,
        // "remove_mesh", &CK3dEntity::RemoveMesh,

        // Position and orientation
        "look_at", sol::overload(
            [](CK3dEntity *entity, const VxVector *pos) { entity->LookAt(pos); },
            [](CK3dEntity *entity, const VxVector *pos, CK3dEntity *ref) { entity->LookAt(pos, ref); },
            [](CK3dEntity *entity, const VxVector *pos, CK3dEntity *ref, bool keepChildren) {
                entity->LookAt(pos, ref, keepChildren);
            }
        ),

        "rotate", sol::overload(
            [](CK3dEntity *entity, const VxVector *axis, float angle) { entity->Rotate(axis, angle); },
            [](CK3dEntity *entity, const VxVector *axis, float angle, CK3dEntity *ref) {
                entity->Rotate(axis, angle, ref);
            },
            [](CK3dEntity *entity, const VxVector *axis, float angle, CK3dEntity *ref, bool keepChildren) {
                entity->Rotate(axis, angle, ref, keepChildren);
            },
            [](CK3dEntity *entity, float x, float y, float z, float angle) { entity->Rotate3f(x, y, z, angle); },
            [](CK3dEntity *entity, float x, float y, float z, float angle, CK3dEntity *ref) {
                entity->Rotate3f(x, y, z, angle, ref);
            },
            [](CK3dEntity *entity, float x, float y, float z, float angle, CK3dEntity *ref, bool keepChildren) {
                entity->Rotate3f(x, y, z, angle, ref, keepChildren);
            }
        ),

        "translate", sol::overload(
            [](CK3dEntity *entity, const VxVector *vect) { entity->Translate(vect); },
            [](CK3dEntity *entity, const VxVector *vect, CK3dEntity *ref) { entity->Translate(vect, ref); },
            [](CK3dEntity *entity, const VxVector *vect, CK3dEntity *ref, bool keepChildren) {
                entity->Translate(vect, ref, keepChildren);
            },
            [](CK3dEntity *entity, float x, float y, float z) { entity->Translate3f(x, y, z); },
            [](CK3dEntity *entity, float x, float y, float z, CK3dEntity *ref) {
                entity->Translate3f(x, y, z, ref);
            },
            [](CK3dEntity *entity, float x, float y, float z, CK3dEntity *ref, bool keepChildren) {
                entity->Translate3f(x, y, z, ref, keepChildren);
            }
        ),
        "set_position", sol::overload(
            [](CK3dEntity *entity, const VxVector *pos) { entity->SetPosition(pos); },
            [](CK3dEntity *entity, const VxVector *pos, CK3dEntity *ref) { entity->SetPosition(pos, ref); },
            [](CK3dEntity *entity, const VxVector *pos, CK3dEntity *ref, bool keepChildren) {
                entity->SetPosition(pos, ref, keepChildren);
            },
            [](CK3dEntity *entity, float x, float y, float z) { entity->SetPosition3f(x, y, z); },
            [](CK3dEntity *entity, float x, float y, float z, CK3dEntity *ref) {
                entity->SetPosition3f(x, y, z, ref);
            },
            [](CK3dEntity *entity, float x, float y, float z, CK3dEntity *ref, bool keepChildren) {
                entity->SetPosition3f(x, y, z, ref, keepChildren);
            }
        ),

        "get_position", sol::overload(
            [](CK3dEntity *entity, VxVector *pos) { entity->GetPosition(pos); },
            [](CK3dEntity *entity, VxVector *pos, CK3dEntity *ref) { entity->GetPosition(pos, ref); }
        ),

        "set_orientation", sol::overload(
            [](CK3dEntity *entity, const VxVector *dir, const VxVector *up) {
                entity->SetOrientation(dir, up);
            },
            [](CK3dEntity *entity, const VxVector *dir, const VxVector *up, const VxVector *right) {
                entity->SetOrientation(dir, up, right);
            },
            [](CK3dEntity *entity, const VxVector *dir, const VxVector *up, const VxVector *right, CK3dEntity *ref) {
                entity->SetOrientation(dir, up, right, ref);
            },
            [](CK3dEntity *entity, const VxVector *dir, const VxVector *up, const VxVector *right, CK3dEntity *ref, bool keepChildren) {
                entity->SetOrientation(dir, up, right, ref, keepChildren);
            }
        ),
        "get_orientation", sol::overload(
            [](CK3dEntity *entity, VxVector *dir, VxVector *up) { entity->GetOrientation(dir, up); },
            [](CK3dEntity *entity, VxVector *dir, VxVector *up, VxVector *right) {
                entity->GetOrientation(dir, up, right);
            },
            [](CK3dEntity *entity, VxVector *dir, VxVector *up, VxVector *right, CK3dEntity *ref) {
                entity->GetOrientation(dir, up, right, ref);
            }
        ),

        // Quaternion
        "set_quaternion", sol::overload(
            [](CK3dEntity *entity, const VxQuaternion *quat) { entity->SetQuaternion(quat); },
            [](CK3dEntity *entity, const VxQuaternion *quat, CK3dEntity *ref) { entity->SetQuaternion(quat, ref); },
            [](CK3dEntity *entity, const VxQuaternion *quat, CK3dEntity *ref, bool keepChildren) {
                entity->SetQuaternion(quat, ref, keepChildren);
            },
            [](CK3dEntity *entity, const VxQuaternion *quat, CK3dEntity *ref, bool keepChildren, bool keepScale) {
                entity->SetQuaternion(quat, ref, keepChildren, keepScale);
            }
        ),
        "get_quaternion", sol::overload(
            [](CK3dEntity *entity, VxQuaternion *quat) { entity->GetQuaternion(quat); },
            [](CK3dEntity *entity, VxQuaternion *quat, CK3dEntity *ref) { entity->GetQuaternion(quat, ref); }
        ),

        // Scale
        "set_scale", sol::overload(
            [](CK3dEntity *entity, const VxVector *scale) { entity->SetScale(scale); },
            [](CK3dEntity *entity, const VxVector *scale, bool keepChildren) {
                entity->SetScale(scale, keepChildren);
            },
            [](CK3dEntity *entity, const VxVector *scale, bool keepChildren, bool local) {
                entity->SetScale(scale, keepChildren, local);
            },
            [](CK3dEntity *entity, float x, float y, float z) { entity->SetScale3f(x, y, z); },
            [](CK3dEntity *entity, float x, float y, float z, bool keepChildren) {
                entity->SetScale3f(x, y, z, keepChildren);
            },
            [](CK3dEntity *entity, float x, float y, float z, bool keepChildren, bool local) {
                entity->SetScale3f(x, y, z, keepChildren, local);
            }
        ),
        "add_scale", sol::overload(
            [](CK3dEntity *entity, const VxVector *scale) { entity->AddScale(scale); },
            [](CK3dEntity *entity, const VxVector *scale, bool keepChildren) {
                entity->AddScale(scale, keepChildren);
            },
            [](CK3dEntity *entity, const VxVector *scale, bool keepChildren, bool local) {
                entity->AddScale(scale, keepChildren, local);
            },
            [](CK3dEntity *entity, float x, float y, float z) { entity->AddScale3f(x, y, z); },
            [](CK3dEntity *entity, float x, float y, float z, bool keepChildren) {
                entity->AddScale3f(x, y, z, keepChildren);
            },
            [](CK3dEntity *entity, float x, float y, float z, bool keepChildren, bool local) {
                entity->AddScale3f(x, y, z, keepChildren, local);
            }
        ),
        "get_scale", sol::overload(
            [](CK3dEntity *entity, VxVector *scale) { entity->GetScale(scale); },
            [](CK3dEntity *entity, VxVector *scale, bool local) { entity->GetScale(scale, local); }
        ),

        // Matrix operations
        "construct_world_matrix", sol::overload(
            [](CK3dEntity *entity, const VxVector *Pos, const VxVector *Scale, const VxQuaternion *Quat) -> bool {
                return entity->ConstructWorldMatrix(Pos, Scale, Quat);
            },
            [](CK3dEntity *entity, const VxVector *Pos, const VxVector *Scale, const VxQuaternion *Quat, const VxQuaternion *Shear, float Sign) -> bool {
                return entity->ConstructWorldMatrixEx(Pos, Scale, Quat, Shear, Sign);
            }
        ),

        "construct_local_matrix", sol::overload(
            [](CK3dEntity *entity, const VxVector *Pos, const VxVector *Scale, const VxQuaternion *Quat) -> bool {
                return entity->ConstructLocalMatrix(Pos, Scale, Quat);
            },
            [](CK3dEntity *entity, const VxVector *Pos, const VxVector *Scale, const VxQuaternion *Quat, const VxQuaternion *Shear, float Sign) -> bool {
                return entity->ConstructLocalMatrixEx(Pos, Scale, Quat, Shear, Sign);
            }
        ),

        "set_local_matrix", sol::overload(
            [](CK3dEntity *entity, const VxMatrix &mat) { entity->SetLocalMatrix(mat); },
            [](CK3dEntity *entity, const VxMatrix &mat, bool keepChildren) {
                entity->SetLocalMatrix(mat, keepChildren);
            }
        ),
        "get_local_matrix", &CK3dEntity::GetLocalMatrix,
        "set_world_matrix", sol::overload(
            [](CK3dEntity *entity, const VxMatrix &mat) { entity->SetWorldMatrix(mat); },
            [](CK3dEntity *entity, const VxMatrix &mat, bool keepChildren) {
                entity->SetWorldMatrix(mat, keepChildren);
            }
        ),
        "get_world_matrix", &CK3dEntity::GetWorldMatrix,
        "get_inverse_world_matrix", &CK3dEntity::GetInverseWorldMatrix,
        "get_last_frame_matrix", &CK3dEntity::GetLastFrameMatrix,

        // Transformations
        "transform", sol::overload(
            [](CK3dEntity *entity, VxVector *dest, const VxVector *src) { entity->Transform(dest, src); },
            [](CK3dEntity *entity, VxVector *dest, const VxVector *src, CK3dEntity *ref) {
                entity->Transform(dest, src, ref);
            }
        ),
        "inverse_transform", sol::overload(
            [](CK3dEntity *entity, VxVector *dest, const VxVector *src) { entity->InverseTransform(dest, src); },
            [](CK3dEntity *entity, VxVector *dest, const VxVector *src, CK3dEntity *ref) {
                entity->InverseTransform(dest, src, ref);
            }
        ),
        "transform_vector", sol::overload(
            [](CK3dEntity *entity, VxVector *dest, const VxVector *src) { entity->TransformVector(dest, src); },
            [](CK3dEntity *entity, VxVector *dest, const VxVector *src, CK3dEntity *ref) {
                entity->TransformVector(dest, src, ref);
            }
        ),
        "inverse_transform_vector", sol::overload(
            [](CK3dEntity *entity, VxVector *dest, const VxVector *src) { entity->InverseTransformVector(dest, src); },
            [](CK3dEntity *entity, VxVector *dest, const VxVector *src, CK3dEntity *ref) {
                entity->InverseTransformVector(dest, src, ref);
            }
        ),
        "transform_many", sol::overload(
            [](CK3dEntity *entity, VxVector *dest, const VxVector *src, int count) {
                entity->TransformMany(dest, src, count);
            },
            [](CK3dEntity *entity, VxVector *dest, const VxVector *src, int count, CK3dEntity *ref) {
                entity->TransformMany(dest, src, count, ref);
            }
        ),
        "inverse_transform_many", sol::overload(
            [](CK3dEntity *entity, VxVector *dest, const VxVector *src, int count) {
                entity->InverseTransformMany(dest, src, count);
            },
            [](CK3dEntity *entity, VxVector *dest, const VxVector *src, int count, CK3dEntity *ref) {
                entity->InverseTransformMany(dest, src, count, ref);
            }
        ),

        "change_referential", sol::overload(
            [](CK3dEntity *entity) { entity->ChangeReferential(); },
            [](CK3dEntity *entity, CK3dEntity *ref) { entity->ChangeReferential(ref); }
        ),

        // Places
        // "get_reference_place", &CK3dEntity::GetReferencePlace,

        // Animations
        // "add_object_animation", &CK3dEntity::AddObjectAnimation,
        // "remove_object_animation", &CK3dEntity::RemoveObjectAnimation,
        // "get_object_animation", &CK3dEntity::GetObjectAnimation,
        // "get_object_animation_count", &CK3dEntity::GetObjectAnimationCount,

        // Skin
        // "create_skin", &CK3dEntity::CreateSkin,
        // "destroy_skin", &CK3dEntity::DestroySkin,
        // "update_skin", &CK3dEntity::UpdateSkin,
        // "get_skin", &CK3dEntity::GetSkin,

        // Bounding box and geometry
        "update_box", sol::overload(
            [](CK3dEntity *entity) { entity->UpdateBox(); },
            [](CK3dEntity *entity, bool world) { entity->UpdateBox(world); }
        ),
        "get_bounding_box", sol::overload(
            [](CK3dEntity *entity) { return entity->GetBoundingBox(); },
            [](CK3dEntity *entity, bool local) { return entity->GetBoundingBox(local); }
        ),
        "set_bounding_box", sol::overload(
            [](CK3dEntity *entity, const VxBbox *bbox) -> bool { return entity->SetBoundingBox(bbox); },
            [](CK3dEntity *entity, const VxBbox *bbox, bool local) { return entity->SetBoundingBox(bbox, local); }
        ),
        "get_hierarchical_box", sol::overload(
            [](CK3dEntity *entity) { return entity->GetHierarchicalBox(); },
            [](CK3dEntity *entity, bool local) { return entity->GetHierarchicalBox(local); }
        ),
        "get_barycenter", &CK3dEntity::GetBaryCenter,
        "get_radius", &CK3dEntity::GetRadius,

        // Ray intersection
        "ray_intersection", sol::overload(
            [](CK3dEntity *entity, const VxVector *pos1, const VxVector *pos2, VxIntersectionDesc *desc,
               CK3dEntity *ref) {
                return entity->RayIntersection(pos1, pos2, desc, ref);
            },
            [](CK3dEntity *entity, const VxVector *pos1, const VxVector *pos2, VxIntersectionDesc *desc,
               CK3dEntity *ref, CK_RAYINTERSECTION options) {
                return entity->RayIntersection(pos1, pos2, desc, ref, options);
            }
        ),

        // Rendering
        // "render", sol::overload(
        //     [](CK3dEntity *entity, CKRenderContext *dev) { return entity->Render(dev); },
        //     [](CK3dEntity *entity, CKRenderContext *dev, CKDWORD flags) { return entity->Render(dev, flags); }
        // ),
        "get_render_extents", &CK3dEntity::GetRenderExtents
    );
}
