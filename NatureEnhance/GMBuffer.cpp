#include "Main.h"
#include "buffer.h"

void D3DCheck(HRESULT result, int pos)
{
    if (SUCCEEDED(result))
        return;

    std::wstring err = L"位置" + std::to_wstring(pos) + L": " + DXGetErrorDescription8(result);
    throw err.c_str();
}

expReal TextureToBuffer(GMReal buffer, GMReal gmtex, GMReal w, GMReal h)
{
    try
    {
        UINT width = (UINT)w, height = (UINT)h;

        IDirect3DTexture8* texture = gm::CGMAPI::GetTextureArray()[(int)gmtex].texture;
        if (Device == nullptr)
            D3DCheck(texture->GetDevice(&Device), 1);

        IDirect3DSurface8* surf = nullptr;
        IDirect3DSurface8* surfTemp = nullptr;
        D3DCheck(texture->GetSurfaceLevel(0, &surf), 2);

        // 因为 GameMaker 的纹理被设置为 D3DPOOL_DEFAULT，不能直接读取数据信息
        // 所以要创建一个额外的 IDirect3DSurface8，将里面的数据复制过来
        D3DCheck(Device->CreateImageSurface(width, height, D3DFMT_A8R8G8B8, &surfTemp), 3);
        D3DCheck(D3DXLoadSurfaceFromSurface(surfTemp, nullptr, nullptr, surf, nullptr,
            nullptr, D3DX_FILTER_NONE, 0), 4);

        // 获取纹理数据信息
        D3DLOCKED_RECT lock;
        D3DCheck(surfTemp->LockRect(&lock, nullptr, 0), 5);
        char* src = (char*)lock.pBits;

        // 传入的 buffer 直接操作其指向的内存，提高效率
        if (!gm::buffer_exists(buffer))
            throw L"传入无效的 buffer 引用。";

        gm::buffer_set_size(buffer, width * height * 4);
        gm::buffer_set_pos(buffer, 0);
        char* dest = (char*)(int)gm::buffer_get_address(buffer, false);

        if (dest == nullptr)
            throw L"传入无效的 buffer 引用。";

        // 复制纹理信息到 buffer 中。因为该纹理是非压缩纹理，所以要按行复制
        UINT srcPos = 0, destPos = 0, bufferStride = width * 4;
        for (UINT i = 0; i < height; ++i)
        {
            memcpy(&dest[destPos], &src[srcPos], bufferStride);
            srcPos += lock.Pitch;
            destPos += bufferStride;
        }

        // 结束，释放内存
        D3DCheck(surfTemp->UnlockRect(), 6);
        surfTemp->Release();
        surf->Release();

        finish;
    }
    simplecatch(L"TextureToBuffer", 0.0)
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
            throw L"传入无效的 buffer 引用。";

        IDirect3DTexture8* texture = gm::CGMAPI::GetTextureArray()[(int)gmtex].texture;

        IDirect3DSurface8* surf = nullptr;
        D3DCheck(texture->GetSurfaceLevel(0, &surf), 1);

        RECT rect = { .left = 0, .top = 0, .right = (long)width, .bottom = (long)height };

        D3DCheck(D3DXLoadSurfaceFromMemory(surf, nullptr, &rect, src, D3DFMT_A8R8G8B8,
            width * 4, nullptr, &rect, D3DX_FILTER_NONE, 0), 2);

        D3DCheck(texture->AddDirtyRect(&rect), 3);
        surf->Release();

        finish;
    }
    simplecatch(L"BufferToTexture", 0.0)
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

    return string_to_cstr(std::get<std::string>(result));
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