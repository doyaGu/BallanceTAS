#include "LuaApi.h"

#include <VxMath.h>

void LuaApi::RegisterVxColor(sol::state &lua) {
    // ===================================================================
    //  VxColor Registration
    // ===================================================================
    auto colorType = lua.new_usertype<VxColor>(
        "VxColor",
        sol::constructors<
            VxColor(),
            VxColor(float, float, float, float),
            VxColor(float, float, float),
            VxColor(float),
            VxColor(unsigned long),
            VxColor(int, int, int, int),
            VxColor(int, int, int)
        >(),

        // Members as properties
        "r", &VxColor::r,
        "g", &VxColor::g,
        "b", &VxColor::b,
        "a", &VxColor::a,

        // Computed properties
        "rgba", sol::property(
            [](const VxColor &c) { return c.GetRGBA(); },
            [](VxColor &c, unsigned long rgba) { c.Set(rgba); }
        ),
        "rgb", sol::property(
            [](const VxColor &c) { return c.GetRGB(); }
        ),
        "red", sol::property(
            [](const VxColor &c) { return ColorGetRed(c.GetRGBA()); },
            [](VxColor &c, unsigned long x) { c.Set(ColorSetRed(c.GetRGBA(), x)); }
        ),
        "green", sol::property(
            [](const VxColor &c) { return ColorGetGreen(c.GetRGBA()); },
            [](VxColor &c, unsigned long x) { c.Set(ColorSetGreen(c.GetRGBA(), x)); }
        ),
        "blue", sol::property(
            [](const VxColor &c) { return ColorGetBlue(c.GetRGBA()); },
            [](VxColor &c, unsigned long x) { c.Set(ColorSetBlue(c.GetRGBA(), x)); }
        ),
        "alpha", sol::property(
            [](const VxColor &c) { return ColorGetAlpha(c.GetRGBA()); },
            [](VxColor &c, unsigned long x) { c.Set(ColorSetAlpha(c.GetRGBA(), x)); }
        ),

        // Methods
        "clear", &VxColor::Clear,
        "check", &VxColor::Check,
        "set", sol::overload(
            [](VxColor &c, float r, float g, float b, float a) { c.Set(r, g, b, a); },
            [](VxColor &c, float r, float g, float b) { c.Set(r, g, b); },
            [](VxColor &c, float r) { c.Set(r); },
            [](VxColor &c, unsigned long rgba) { c.Set(rgba); },
            [](VxColor &c, int r, int g, int b, int a) { c.Set(r, g, b, a); },
            [](VxColor &c, int r, int g, int b) { c.Set(r, g, b); }
        ),
        "get_square_distance", &VxColor::GetSquareDistance,

        // Static methods
        "convert", sol::overload(
            [](float r, float g, float b, float a) { return VxColor::Convert(r, g, b, a); },
            [](float r, float g, float b) { return VxColor::Convert(r, g, b); },
            [](int r, int g, int b, int a) { return VxColor::Convert(r, g, b, a); },
            [](int r, int g, int b) { return VxColor::Convert(r, g, b); }
        ),

        // Operators
        sol::meta_function::addition, [](const VxColor &a, const VxColor &b) { return a + b; },
        sol::meta_function::subtraction, [](const VxColor &a, const VxColor &b) { return a - b; },
        sol::meta_function::multiplication,
        sol::overload(
            [](const VxColor &a, const VxColor &b) { return a * b; },
            [](const VxColor &c, float s) { return c * s; }
        ),
        sol::meta_function::division, [](const VxColor &a, const VxColor &b) { return a / b; },
        sol::meta_function::equal_to, [](const VxColor &a, const VxColor &b) { return a == b; },
        sol::meta_function::to_string, [](const VxColor &c) {
            return "VxColor(" + std::to_string(c.r) + ", " + std::to_string(c.g) +
                ", " + std::to_string(c.b) + ", " + std::to_string(c.a) + ")";
        }
    );
}
