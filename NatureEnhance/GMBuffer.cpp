#include "Main.h"
#include "buffer.h"

void D3DCheck(HRESULT result, int pos)
{
    if (SUCCEEDED(result))
        return;

	throw std::runtime_error("位置" + std::to_string(pos) + ": " + d3d::error_text(result));
}

expReal TextureToBuffer(GMReal buffer, GMReal gmtex, GMReal w, GMReal h)
{
    try
    {
        UINT width = (UINT)w, height = (UINT)h;

        void* texture = gm::CGMAPI::GetTextureArray()[(int)gmtex].texture;
        if (Device == nullptr)
        {
            D3DCheck(d3d::get_device_from_texture(texture, &Device), 1);
            d3d::ensure_version(Device, nullptr);   // 惰性拿到设备后补一次后端判定
        }

        void* surf = nullptr;
        void* surfTemp = nullptr;
        D3DCheck(d3d::get_surface_level(texture, 0, &surf), 2);

        // 因为 GameMaker 的纹理被设置为 D3DPOOL_DEFAULT，不能直接读取数据信息
        // 所以要创建一个额外的可锁定表面，将里面的数据复制过来
        D3DCheck(d3d::create_image_surface(width, height, D3DFMT_A8R8G8B8, &surfTemp), 3);
        D3DCheck(d3d::load_surface_from_surface(surfTemp, surf), 4);

        // 获取纹理数据信息
        d3d::LockedRect lock;
        D3DCheck(d3d::lock_rect(surfTemp, &lock, nullptr, 0), 5);
        char* src = (char*)lock.pBits;

        // 传入的 buffer 直接操作其指向的内存，提高效率
        if (!gm::buffer_exists(buffer))
            throw std::runtime_error("传入无效的 buffer 引用。");

        gm::buffer_set_size(buffer, width * height * 4);
        gm::buffer_set_pos(buffer, 0);
        char* dest = (char*)(int)gm::buffer_get_address(buffer, false);

        if (dest == nullptr)
            throw std::runtime_error("传入无效的 buffer 引用。");

        // 复制纹理信息到 buffer 中。因为该纹理是非压缩纹理，所以要按行复制
        UINT srcPos = 0, destPos = 0, bufferStride = width * 4;
        for (UINT i = 0; i < height; ++i)
        {
            memcpy(&dest[destPos], &src[srcPos], bufferStride);
            srcPos += lock.Pitch;
            destPos += bufferStride;
        }

        // 结束，释放内存
        D3DCheck(d3d::unlock_rect(surfTemp), 6);
        d3d::release(surfTemp);
        d3d::release(surf);

        finish;
    }
    simplecatch("TextureToBuffer", 0.0)
}

expReal SurfaceToBuffer(GMReal buffer, GMReal surface)
{
    int gmtex = gm::surface_get_texture((int)surface);
    int width = gm::surface_get_width((int)surface);
    int height = gm::surface_get_height((int)surface);

    return TextureToBuffer(buffer, gmtex, width, height);
}

expReal BufferToTexture(GMReal buffer, GMReal gmtex, GMReal w, GMReal h)
{
    try
    {
        UINT width = (UINT)w, height = (UINT)h;
        char* src = (char*)(int)gm::buffer_get_address(buffer, false);
        if (src == nullptr)
            throw std::runtime_error("传入无效的 buffer 引用。");

        void* texture = gm::CGMAPI::GetTextureArray()[(int)gmtex].texture;

        void* surf = nullptr;
        D3DCheck(d3d::get_surface_level(texture, 0, &surf), 1);

        RECT rect = { .left = 0, .top = 0, .right = (long)width, .bottom = (long)height };

        D3DCheck(d3d::load_surface_from_memory(surf, &rect, src, D3DFMT_A8R8G8B8,
            width * 4, &rect), 2);

        D3DCheck(d3d::add_dirty_rect(texture, &rect), 3);
        d3d::release(surf);

        finish;
    }
    simplecatch("BufferToTexture", 0.0)
}

expReal BufferToSurface(GMReal buffer, GMReal surface)
{
    int gmtex = gm::surface_get_texture((int)surface);
    int width = gm::surface_get_width((int)surface);
    int height = gm::surface_get_height((int)surface);

    return BufferToTexture(buffer, gmtex, width, height);
}

expReal ARGBGetColor(GMReal color) { return ((UINT)color) & 0x00ffffff; }
expReal ARGBGetAlpha(GMReal color)
{
    return ((double)((((UINT)color) & 0xff000000) >> 24)) / 0xff;
}

expReal BufferPeekReal(GMReal buffer, GMReal offset, GMReal type)
{
    int t = (int)type;
    if (t == buffer_string || t == buffer_string_part || t == buffer_hex)
        fail;

    if (!gm::buffer_exists(buffer))
        fail;

    GMReal oldPos = gm::buffer_get_pos(buffer);
    gm::buffer_set_pos(buffer, offset);
    dynamic result = gm::buffer_read((int)buffer, t);
    gm::buffer_set_pos(buffer, oldPos);

    return std::get<GMReal>(result);
}

expString BufferPeekString(GMReal buffer, GMReal offset, GMReal type)
{
    int t = (int)type;
    if (t != buffer_string && t != buffer_string_part && t != buffer_hex)
        return "";

    if (!gm::buffer_exists(buffer))
        return "";

    GMReal oldPos = gm::buffer_get_pos(buffer);
    gm::buffer_set_pos(buffer, offset);
    dynamic result = gm::buffer_read((int)buffer, t);
    gm::buffer_set_pos(buffer, oldPos);

	GMReturnString = std::get<std::string>(result);
    return GMReturnString.c_str();
}

expReal BufferPokeReal(GMReal buffer, GMReal offset, GMReal type, GMReal value)
{
    int t = (int)type;
    if (t == buffer_string || t == buffer_string_part || t == buffer_hex)
        fail;

    if (!gm::buffer_exists(buffer))
        fail;

    GMReal oldPos = gm::buffer_get_pos(buffer);
    gm::buffer_set_pos(buffer, offset);
    gm::buffer_write((int)buffer, t, value);
    gm::buffer_set_pos(buffer, oldPos);

    finish;
}

expReal BufferPokeString(GMReal buffer, GMReal offset, GMReal type, GMString value)
{
    int t = (int)type;
    if (t != buffer_string && t != buffer_string_part && t != buffer_hex)
        fail;

    if (!gm::buffer_exists(buffer))
        fail;

    GMReal oldPos = gm::buffer_get_pos(buffer);
    gm::buffer_set_pos(buffer, offset);
    gm::buffer_write((int)buffer, t, value);
    gm::buffer_set_pos(buffer, oldPos);

    finish;
}