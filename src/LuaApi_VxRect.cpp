#include "LuaApi.h"

#include <VxMath.h>

void LuaApi::RegisterVxRect(sol::state &lua) {
    // ===================================================================
    //  VxRect Registration
    // ===================================================================
    auto rectType = lua.new_usertype<VxRect>(
        "VxRect",
        sol::constructors<VxRect(), VxRect(Vx2DVector &, Vx2DVector &), VxRect(float, float, float, float)>(),

        // Members as properties
        "left", &VxRect::left,
        "top", &VxRect::top,
        "right", &VxRect::right,
        "bottom", &VxRect::bottom,

        // Computed properties
        "width", sol::property(
            [](const VxRect &r) { return r.GetWidth(); },
            [](VxRect &r, float w) { r.SetWidth(w); }
        ),
        "height", sol::property(
            [](const VxRect &r) { return r.GetHeight(); },
            [](VxRect &r, float h) { r.SetHeight(h); }
        ),
        "h_center", sol::property([](const VxRect &r) { return r.GetHCenter(); }),
        "v_center", sol::property([](const VxRect &r) { return r.GetVCenter(); }),
        "size", sol::property(
            [](const VxRect &r) { return r.GetSize(); },
            [](VxRect &r, const Vx2DVector &v) { r.SetSize(v); }
        ),
        "half_size", sol::property(
            [](const VxRect &r) { return r.GetHalfSize(); },
            [](VxRect &r, const Vx2DVector &v) { r.SetHalfSize(v); }
        ),
        "center", sol::property(
            [](const VxRect &r) { return r.GetCenter(); },
            [](VxRect &r, const Vx2DVector &v) { r.SetCenter(v); }
        ),
        "top_left", sol::property(
            [](const VxRect &r) { return r.GetTopLeft(); },
            [](VxRect &r, const Vx2DVector &v) { r.SetTopLeft(v); }
        ),
        "bottom_right", sol::property(
            [](const VxRect &r) { return r.GetBottomRight(); },
            [](VxRect &r, const Vx2DVector &v) { r.SetBottomRight(v); }
        ),

        // Methods
        "clear", &VxRect::Clear,
        "set_corners", sol::overload(
            [](VxRect &r, const Vx2DVector &tl, const Vx2DVector &br) { r.SetCorners(tl, br); },
            [](VxRect &r, float l, float t, float rt, float b) { r.SetCorners(l, t, rt, b); }
        ),
        "set_dimension", sol::overload(
            [](VxRect &r, const Vx2DVector &pos, const Vx2DVector &size) { r.SetDimension(pos, size); },
            [](VxRect &r, float x, float y, float w, float h) { r.SetDimension(x, y, w, h); }
        ),
        "set_center", sol::overload(
            [](VxRect &r, const Vx2DVector &center, const Vx2DVector &halfsize) { r.SetCenter(center, halfsize); },
            [](VxRect &r, float cx, float cy, float hw, float hh) { r.SetCenter(cx, cy, hw, hh); }
        ),
        // "copy_from", &VxRect::CopyFrom,
        // "copy_to", &VxRect::CopyTo,
        "bounding", &VxRect::Bounding,
        "normalize", &VxRect::Normalize,
        "move", &VxRect::Move,
        "translate", &VxRect::Translate,
        "h_move", &VxRect::HMove,
        "v_move", &VxRect::VMove,
        "h_translate", &VxRect::HTranslate,
        "v_translate", &VxRect::VTranslate,
        "transform_to_homogeneous", &VxRect::TransformToHomogeneous,
        "transform_from_homogeneous", sol::overload(
            [](VxRect &r, Vx2DVector &dest, const Vx2DVector &srchom) {
                r.TransformFromHomogeneous(dest, srchom);
            },
            [](VxRect &r, const VxRect &screen) {
                r.TransformFromHomogeneous(screen);
            }
        ),
        "scale", &VxRect::Scale,
        "inflate", &VxRect::Inflate,
        "interpolate", &VxRect::Interpolate,
        "merge", &VxRect::Merge,
        "is_inside", sol::overload(
            [](const VxRect &r, const VxRect &other) -> bool { return r.IsInside(other); },
            [](const VxRect &r, const Vx2DVector &pt) -> bool { return r.IsInside(pt); }
        ),
        "is_outside", [](const VxRect &r, const VxRect &other) -> bool { return r.IsOutside(other); },
        "is_null", [](const VxRect &r) -> bool { return r.IsNull(); },
        "is_empty", [](const VxRect &r) -> bool { return r.IsEmpty(); },
        "clip", sol::overload(
            [](VxRect &r, const VxRect &cliprect) -> bool { return r.Clip(cliprect); },
            [](const VxRect &r, Vx2DVector &pt, bool exclude) { r.Clip(pt, exclude); }
        ),
        "transform", sol::overload(
            [](VxRect &r, const VxRect &dest, const VxRect &src) { r.Transform(dest, src); },
            [](VxRect &r, const Vx2DVector &dest_size, const Vx2DVector &src_size) { r.Transform(dest_size, src_size); }
        ),

        // Operators
        sol::meta_function::equal_to, [](const VxRect &a, const VxRect &b) { return a == b; },
        sol::meta_function::to_string, [](const VxRect &r) {
            return "VxRect(" + std::to_string(r.left) + ", " + std::to_string(r.top) +
                ", " + std::to_string(r.right) + ", " + std::to_string(r.bottom) + ")";
        }
    );
}
