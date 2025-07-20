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
        "children_count", sol::readonly_property(&CK3dEntity::GetChildrenCount),
        "get_child", &CK3dEntity::GetChild,
        "set_parent", sol::overload(
            [](CK3dEntity *entity, CK3dEntity *parent) -> bool { return entity->SetParent(parent); },
            [](CK3dEntity *entity, CK3dEntity *parent, bool keepWorldPos) {
                return entity->SetParent(parent, keepWorldPos);
            }
        ),
        "parent", sol::readonly_property(&CK3dEntity::GetParent),
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
        "flags", sol::property(&CK3dEntity::GetFlags, &CK3dEntity::SetFlags),
        "set_pickable", sol::overload(
            [](CK3dEntity *entity) { entity->SetPickable(); },
            [](CK3dEntity *entity, bool pick) { entity->SetPickable(pick); }
        ),
        "is_pickable", sol::readonly_property([](CK3dEntity *entity) -> bool { return entity->IsPickable(); }),
        "set_render_channels", sol::overload(
            [](CK3dEntity *entity) { entity->SetRenderChannels(); },
            [](CK3dEntity *entity, bool renderChannels) { entity->SetRenderChannels(renderChannels); }
        ),
        "are_render_channels_visible", sol::readonly_property([](CK3dEntity *entity) -> bool {
            return entity->AreRenderChannelsVisible();
        }),

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
        "is_all_inside_frustum", sol::readonly_property([](CK3dEntity *entity) -> bool {return entity->IsAllInsideFrustrum(); }),
        "is_all_outside_frustum", sol::readonly_property([](CK3dEntity *entity) -> bool { return entity->IsAllOutsideFrustrum(); }),

        // Animation control
        "ignore_animations", sol::overload(
            [](CK3dEntity *entity) { entity->IgnoreAnimations(); },
            [](CK3dEntity *entity, bool ignore) { entity->IgnoreAnimations(ignore); }
        ),
        "are_animations_ignored", sol::readonly_property(
            [](CK3dEntity *entity) -> bool { return entity->AreAnimationIgnored(); }
        ),

        // Transparency
        "set_render_as_transparent", sol::overload(
            [](CK3dEntity *entity) { entity->SetRenderAsTransparent(); },
            [](CK3dEntity *entity, bool trans) { entity->SetRenderAsTransparent(trans); }
        ),

        // Moveable flags
        "moveable_flags", sol::property(&CK3dEntity::GetMoveableFlags, &CK3dEntity::SetMoveableFlags),
        "modify_moveable_flags", &CK3dEntity::ModifyMoveableFlags,

        // Meshes
        // "get_current_mesh", &CK3dEntity::GetCurrentMesh,
        // "set_current_mesh", sol::overload(
        //     [](CK3dEntity *entity, CKMesh *mesh) { return entity->SetCurrentMesh(mesh); },
        //     [](CK3dEntity *entity, CKMesh *mesh, bool addIfNotHere) {
        //         return entity->SetCurrentMesh(mesh, addIfNotHere);
        //     }
        // ),
        // "mesh_count", sol::readonly_property(&CK3dEntity::GetMeshCount),
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
            [](CK3dEntity *entity) {
                VxVector pos;
                entity->GetPosition(&pos);
                return pos;
            },
            [](CK3dEntity *entity, CK3dEntity *ref) {
                VxVector pos;
                entity->GetPosition(&pos, ref);
                return pos;
            }
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
            [](CK3dEntity *entity) {
                VxVector dir, up, right;
                entity->GetOrientation(&dir, &up, &right);
                return std::make_tuple(dir, up, right);
            },
            [](CK3dEntity *entity, CK3dEntity *ref) {
                VxVector dir, up, right;
                entity->GetOrientation(&dir, &up, &right, ref);
                return std::make_tuple(dir, up, right);
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
            [](CK3dEntity *entity) {
                VxQuaternion quat;
                entity->GetQuaternion(&quat);
                return quat;
            },
            [](CK3dEntity *entity, CK3dEntity *ref) {
                VxQuaternion quat;
                entity->GetQuaternion(&quat, ref);
                return quat;
            }
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
            [](CK3dEntity *entity) {
                VxVector scale;
                entity->GetScale(&scale);
                return scale;
            },
            [](CK3dEntity *entity, bool local) {
                VxVector scale;
                entity->GetScale(&scale, local);
                return scale;
            }
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
        "local_matrix", sol::readonly_property(&CK3dEntity::GetLocalMatrix),
        "set_world_matrix", sol::overload(
            [](CK3dEntity *entity, const VxMatrix &mat) { entity->SetWorldMatrix(mat); },
            [](CK3dEntity *entity, const VxMatrix &mat, bool keepChildren) {
                entity->SetWorldMatrix(mat, keepChildren);
            }
        ),
        "world_matrix", sol::readonly_property(&CK3dEntity::GetWorldMatrix),
        "inverse_world_matrix", sol::readonly_property(&CK3dEntity::GetInverseWorldMatrix),
        "last_frame_matrix", sol::readonly_property(&CK3dEntity::GetLastFrameMatrix),

        // Transformations
        "transform", sol::overload(
            [](CK3dEntity *entity, const VxVector *src) {
                VxVector dest;
                entity->Transform(&dest, src);
                return dest;
            },
            [](CK3dEntity *entity, const VxVector *src, CK3dEntity *ref) {
                VxVector dest;
                entity->Transform(&dest, src, ref);
                return dest;
            }
        ),
        "inverse_transform", sol::overload(
            [](CK3dEntity *entity, const VxVector *src) {
                VxVector dest;
                entity->InverseTransform(&dest, src);
                return dest;
            },
            [](CK3dEntity *entity, const VxVector *src, CK3dEntity *ref) {
                VxVector dest;
                entity->InverseTransform(&dest, src, ref);
                return dest;
            }
        ),
        "transform_vector", sol::overload(
            [](CK3dEntity *entity, const VxVector *src) {
                VxVector dest;
                entity->TransformVector(&dest, src);
                return dest;
            },
            [](CK3dEntity *entity, const VxVector *src, CK3dEntity *ref) {
                VxVector dest;
                entity->TransformVector(&dest, src, ref);
                return dest;
            }
        ),
        "inverse_transform_vector", sol::overload(
            [](CK3dEntity *entity, const VxVector *src) {
                VxVector dest;
                entity->InverseTransformVector(&dest, src);
                return dest;
            },
            [](CK3dEntity *entity, const VxVector *src, CK3dEntity *ref) {
                VxVector dest;
                entity->InverseTransformVector(&dest, src, ref);
                return dest;
            }
        ),
        // "transform_many", sol::overload(
        //     [](CK3dEntity *entity, VxVector *dest, const VxVector *src, int count) {
        //         entity->TransformMany(dest, src, count);
        //     },
        //     [](CK3dEntity *entity, VxVector *dest, const VxVector *src, int count, CK3dEntity *ref) {
        //         entity->TransformMany(dest, src, count, ref);
        //     }
        // ),
        // "inverse_transform_many", sol::overload(
        //     [](CK3dEntity *entity, VxVector *dest, const VxVector *src, int count) {
        //         entity->InverseTransformMany(dest, src, count);
        //     },
        //     [](CK3dEntity *entity, VxVector *dest, const VxVector *src, int count, CK3dEntity *ref) {
        //         entity->InverseTransformMany(dest, src, count, ref);
        //     }
        // ),

        "change_referential", sol::overload(
            [](CK3dEntity *entity) { entity->ChangeReferential(); },
            [](CK3dEntity *entity, CK3dEntity *ref) { entity->ChangeReferential(ref); }
        ),

        // Places
        // "reference_place", sol::readonly_property(&CK3dEntity::GetReferencePlace),

        // Animations
        // "add_object_animation", &CK3dEntity::AddObjectAnimation,
        // "remove_object_animation", &CK3dEntity::RemoveObjectAnimation,
        // "get_object_animation", &CK3dEntity::GetObjectAnimation,
        // "object_animation_count", sol::readonly_property(&CK3dEntity::GetObjectAnimationCount),

        // Skin
        // "create_skin", &CK3dEntity::CreateSkin,
        // "destroy_skin", &CK3dEntity::DestroySkin,
        // "update_skin", &CK3dEntity::UpdateSkin,
        // "skin", sol::readonly_property(&CK3dEntity::GetSkin),

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
            [](CK3dEntity *entity, const VxBbox *bbox, bool local) -> bool { return entity->SetBoundingBox(bbox, local); }
        ),
        "get_hierarchical_box", sol::overload(
            [](CK3dEntity *entity) { return entity->GetHierarchicalBox(); },
            [](CK3dEntity *entity, bool local) { return entity->GetHierarchicalBox(local); }
        ),
        "get_barycenter", [](CK3dEntity *entity) {
            VxVector pos;
            bool result = entity->GetBaryCenter(&pos);
            if (result) {
                return std::make_tuple(true, pos);
            } else {
                return std::make_tuple(false, VxVector());
            }
        },
        "radius", sol::readonly_property(&CK3dEntity::GetRadius),

        // Ray intersection
        "ray_intersection", sol::overload(
            [](CK3dEntity *entity, const VxVector *pos1, const VxVector *pos2, CK3dEntity *ref) {
                VxIntersectionDesc desc;
                int result = entity->RayIntersection(pos1, pos2, &desc, ref);
                return std::make_tuple(result, desc);
            },
            [](CK3dEntity *entity, const VxVector *pos1, const VxVector *pos2, CK3dEntity *ref, CK_RAYINTERSECTION options) {
                VxIntersectionDesc desc;
                int result = entity->RayIntersection(pos1, pos2, &desc, ref, options);
                return std::make_tuple(result, desc);
            }
        ),

        // Rendering
        // "render", sol::overload(
        //     [](CK3dEntity *entity, CKRenderContext *dev) { return entity->Render(dev); },
        //     [](CK3dEntity *entity, CKRenderContext *dev, CKDWORD flags) { return entity->Render(dev, flags); }
        // ),
        "render_extents", sol::readonly_property([](CK3dEntity *entity) {
            VxRect rect;
            entity->GetRenderExtents(rect);
            return rect;
        })
    );
}
