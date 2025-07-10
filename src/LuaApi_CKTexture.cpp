#include "LuaApi.h"

#include <CKTexture.h>
#include <CKRenderContext.h>
#include <CKBitmapData.h>
#include <VxRect.h>

void LuaApi::RegisterCKTexture(sol::state &lua) {
    // ===================================================================
    //  CKTexture - Class for managing textures applied to objects
    // ===================================================================
    auto ckTextureType = lua.new_usertype<CKTexture>(
        "CKTexture",
        sol::no_constructor,
        sol::base_classes, sol::bases<CKBeObject, CKBitmapData, CKSceneObject, CKObject>(),

        // File I/O
        "create", sol::overload(
            [](CKTexture &tex, int width, int height) { return tex.Create(width, height); },
            [](CKTexture &tex, int width, int height, int bpp) { return tex.Create(width, height, bpp); },
            [](CKTexture &tex, int width, int height, int bpp, int slot) { return tex.Create(width, height, bpp, slot); }
        ),
        "load_image", sol::overload(
            [](CKTexture &tex, CKSTRING name) { return tex.LoadImage(name); },
            [](CKTexture &tex, CKSTRING name, int slot) { return tex.LoadImage(name, slot); }
        ),
        "load_movie", &CKTexture::LoadMovie,

        // "set_as_current", sol::overload(
        //     [](CKTexture &tex, CKRenderContext *dev) { return tex.SetAsCurrent(dev); },
        //     [](CKTexture &tex, CKRenderContext *dev, CKBOOL clamping) { return tex.SetAsCurrent(dev, clamping); },
        //     [](CKTexture &tex, CKRenderContext *dev, CKBOOL clamping, int stage) { return tex.SetAsCurrent(dev, clamping, stage); }
        // ),

        // Video memory management
        "restore", sol::overload(
            [](CKTexture &tex) { return tex.Restore(); },
            [](CKTexture &tex, CKBOOL clamp) { return tex.Restore(clamp); }
        ),
        // "system_to_video_memory", sol::overload(
        //     [](CKTexture &tex, CKRenderContext *dev) { return tex.SystemToVideoMemory(dev); },
        //     [](CKTexture &tex, CKRenderContext *dev, CKBOOL clamping) { return tex.SystemToVideoMemory(dev, clamping); }
        // ),
        "free_video_memory", &CKTexture::FreeVideoMemory,
        "is_in_video_memory", &CKTexture::IsInVideoMemory,

        // "copy_context", sol::overload(
        //     [](CKTexture &tex, CKRenderContext *ctx, VxRect *src, VxRect *dest) {
        //         return tex.CopyContext(ctx, src, dest);
        //     },
        //     [](CKTexture &tex, CKRenderContext *ctx, VxRect *src, VxRect *dest, int face) {
        //         return tex.CopyContext(ctx, src, dest, face);
        //     }
        // ),

        // Mipmap management
        "use_mipmap", &CKTexture::UseMipmap,
        "get_mipmap_count", &CKTexture::GetMipmapCount,

        // Texture description
        // "get_video_texture_desc", &CKTexture::GetVideoTextureDesc,
        // "get_video_pixel_format", &CKTexture::GetVideoPixelFormat,
        // "get_system_texture_desc", &CKTexture::GetSystemTextureDesc,

        // Desired video format
        "set_desired_video_format", &CKTexture::SetDesiredVideoFormat,
        "get_desired_video_format", &CKTexture::GetDesiredVideoFormat,

        // User mipmap levels
        "set_user_mip_map_mode", &CKTexture::SetUserMipMapMode,
        "get_user_mip_map_level", &CKTexture::GetUserMipMapLevel,

        // Rasterizer index
        "get_rst_texture_index", &CKTexture::GetRstTextureIndex
    );
}