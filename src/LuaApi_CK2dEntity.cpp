#include "LuaApi.h"

#include <CK2dEntity.h>
#include <CKMaterial.h>

void LuaApi::RegisterCK2dEntity(sol::state &lua) {
    auto ck2dEntityType = lua.new_usertype<CK2dEntity>(
        "CK2dEntity",
        sol::no_constructor,
        sol::base_classes, sol::bases<CKRenderObject, CKBeObject, CKSceneObject, CKObject>(),

        // Position and size
        "get_position", sol::overload(
            [](CK2dEntity &entity, Vx2DVector &vect) { return entity.GetPosition(vect); },
            [](CK2dEntity &entity, Vx2DVector &vect, CKBOOL hom) { return entity.GetPosition(vect, hom); },
            [](CK2dEntity &entity, Vx2DVector &vect, CKBOOL hom, CK2dEntity *ref) {
                return entity.GetPosition(vect, hom, ref);
            }
        ),
        "set_position", sol::overload(
            [](CK2dEntity &entity, const Vx2DVector &vect) { entity.SetPosition(vect); },
            [](CK2dEntity &entity, const Vx2DVector &vect, CKBOOL hom) { entity.SetPosition(vect, hom); },
            [](CK2dEntity &entity, const Vx2DVector &vect, CKBOOL hom, CKBOOL keepChildren) {
                entity.SetPosition(vect, hom, keepChildren);
            },
            [](CK2dEntity &entity, const Vx2DVector &vect, CKBOOL hom, CKBOOL keepChildren, CK2dEntity *ref) {
                entity.SetPosition(vect, hom, keepChildren, ref);
            }
        ),

        "get_size", sol::overload(
            [](CK2dEntity &entity, Vx2DVector &vect) { return entity.GetSize(vect); },
            [](CK2dEntity &entity, Vx2DVector &vect, CKBOOL hom) { return entity.GetSize(vect, hom); }
        ),
        "set_size", sol::overload(
            [](CK2dEntity &entity, const Vx2DVector &vect) { entity.SetSize(vect); },
            [](CK2dEntity &entity, const Vx2DVector &vect, CKBOOL hom) { entity.SetSize(vect, hom); },
            [](CK2dEntity &entity, const Vx2DVector &vect, CKBOOL hom, CKBOOL keepChildren) {
                entity.SetSize(vect, hom, keepChildren);
            }
        ),

        // Rect access
        "set_rect", sol::overload(
            [](CK2dEntity &entity, const VxRect &rect) { entity.SetRect(rect); },
            [](CK2dEntity &entity, const VxRect &rect, CKBOOL keepChildren) { entity.SetRect(rect, keepChildren); }
        ),
        "get_rect", &CK2dEntity::GetRect,
        "set_homogeneous_rect", sol::overload(
            [](CK2dEntity &entity, const VxRect &rect) { return entity.SetHomogeneousRect(rect); },
            [](CK2dEntity &entity, const VxRect &rect, CKBOOL keepChildren) {
                return entity.SetHomogeneousRect(rect, keepChildren);
            }
        ),
        "get_homogeneous_rect", &CK2dEntity::GetHomogeneousRect,

        // Source rect (cropping)
        "set_source_rect", &CK2dEntity::SetSourceRect,
        "get_source_rect", &CK2dEntity::GetSourceRect,
        "use_source_rect", sol::overload(
            [](CK2dEntity &entity) { entity.UseSourceRect(); },
            [](CK2dEntity &entity, CKBOOL use) { entity.UseSourceRect(use); }
        ),
        "is_using_source_rect", &CK2dEntity::IsUsingSourceRect,

        // Properties
        "set_pickable", sol::overload(
            [](CK2dEntity &entity) { entity.SetPickable(); },
            [](CK2dEntity &entity, CKBOOL pick) { entity.SetPickable(pick); }
        ),
        "is_pickable", &CK2dEntity::IsPickable,

        "set_background", sol::overload(
            [](CK2dEntity &entity) { entity.SetBackground(); },
            [](CK2dEntity &entity, CKBOOL back) { entity.SetBackground(back); }
        ),
        "is_background", &CK2dEntity::IsBackground,

        "set_clip_to_parent", sol::overload(
            [](CK2dEntity &entity) { entity.SetClipToParent(); },
            [](CK2dEntity &entity, CKBOOL clip) { entity.SetClipToParent(clip); }
        ),
        "is_clip_to_parent", &CK2dEntity::IsClipToParent,

        // Flags
        "set_flags", &CK2dEntity::SetFlags,
        "modify_flags", sol::overload(
            [](CK2dEntity &entity, CKDWORD add) { entity.ModifyFlags(add); },
            [](CK2dEntity &entity, CKDWORD add, CKDWORD remove) { entity.ModifyFlags(add, remove); }
        ),
        "get_flags", &CK2dEntity::GetFlags,

        // Camera ratio offset
        "enable_ratio_offset", sol::overload(
            [](CK2dEntity &entity) { entity.EnableRatioOffset(); },
            [](CK2dEntity &entity, CKBOOL ratio) { entity.EnableRatioOffset(ratio); }
        ),
        "is_ratio_offset", &CK2dEntity::IsRatioOffset,

        // Parenting
        "set_parent", &CK2dEntity::SetParent,
        "get_parent", &CK2dEntity::GetParent,
        "get_children_count", &CK2dEntity::GetChildrenCount,
        "get_child", &CK2dEntity::GetChild,
        "hierarchy_parser", &CK2dEntity::HierarchyParser,

        // Material
        "set_material", &CK2dEntity::SetMaterial,
        "get_material", &CK2dEntity::GetMaterial,

        // Coordinates
        "set_homogeneous_coordinates", sol::overload(
            [](CK2dEntity &entity) { entity.SetHomogeneousCoordinates(); },
            [](CK2dEntity &entity, CKBOOL coord) { entity.SetHomogeneousCoordinates(coord); }
        ),
        "is_homogeneous_coordinates", &CK2dEntity::IsHomogeneousCoordinates,

        // Clipping
        "enable_clip_to_camera", sol::overload(
            [](CK2dEntity &entity) { entity.EnableClipToCamera(); },
            [](CK2dEntity &entity, CKBOOL clip) { entity.EnableClipToCamera(clip); }
        ),
        "is_clipped_to_camera", &CK2dEntity::IsClippedToCamera,

        // Rendering
        // "render", &CK2dEntity::Render,
        // "draw", &CK2dEntity::Draw,

        // Extents
        "get_extents", &CK2dEntity::GetExtents,
        "set_extents", &CK2dEntity::SetExtents,
        "restore_initial_size", &CK2dEntity::RestoreInitialSize
    );
}
