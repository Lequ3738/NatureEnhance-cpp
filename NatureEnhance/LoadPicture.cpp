#include "Main.h"
#include "lodepng.h"
#include <future>

typedef std::future<std::tuple<std::vector<UCHAR>, UINT, UINT>> PNGDecodeFuture;

PNGDecodeFuture AsyncDecodePNG(GMString file)
{
    return std::async(std::launch::async, [file]() {
        std::vector<UCHAR> image, d3dimage;
        UINT width, height;
        UINT error = lodepng::decode(image, width, height, file);
        if (error)
            throw lodepng_error_text(error);

        // 将 RGBA 格式的数据转换为 D3D8 所需的 ARGB 格式
        d3dimage.resize(image.size());
        for (size_t i = 0; i < image.size(); i += 4)
        {
            d3dimage[i] = image[i + 2];     // R -> B
            d3dimage[i + 1] = image[i + 1]; // G
            d3dimage[i + 2] = image[i];     // B -> R
            d3dimage[i + 3] = image[i + 3]; // A
        }

        return std::make_tuple(std::move(d3dimage), width, height);
    });
}

std::unordered_map<UINT, PNGDecodeFuture> AsyncDecodePNGList;
UINT AsyncDecodePNGId = 0;

expReal LoadPNGAsync(GMString file)
{
    AsyncDecodePNGList[AsyncDecodePNGId] = AsyncDecodePNG(file);
    return (GMReal)AsyncDecodePNGId++;
}

expReal ToBackgroundAsync(GMReal id)
{
    try
    {
        PNGDecodeFuture& future = AsyncDecodePNGList.at((UINT)id);
        auto status = future.wait_for(std::chrono::seconds(0));
        if (status != std::future_status::ready)
            return -1;

        PNGDecodeFuture getFuture = std::move(future);
        auto [d3dimage, width, height] = getFuture.get();

        // 新建 GM 的背景资源并返回其纹理和表面
        int back = gm::background_create_color(width, height, 0);
        IDirect3DTexture8* texture = gmapi->Backgrounds[back].GetTexture();

        IDirect3DSurface8* surf = nullptr;
        D3DCheck(texture->GetSurfaceLevel(0, &surf), 1);

        // 载入数据
        RECT rect = { .left = 0, .top = 0, .right = (long)width, .bottom = (long)height };

        D3DCheck(D3DXLoadSurfaceFromMemory(surf, nullptr, &rect, d3dimage.data(),
            D3DFMT_A8R8G8B8, width * 4, nullptr, &rect, D3DX_FILTER_NONE, 0), 2);

        D3DCheck(texture->AddDirtyRect(&rect), 3);
        surf->Release();

        AsyncDecodePNGList.erase((UINT)id);
        return back;
    }
    catch (const std::out_of_range)
    {
        return -2;
    }
    catch (const char* e)
    {
        std::wstring err = L"在 LoadPNGToBackground 中，加载 PNG 文件失败: " +
            std::wstring(e, e + strlen(e));

        if (show_error)
            MessageBox(GMWindowsHandle, err.c_str(), L"NatureEnhance Error", MB_OK | MB_ICONERROR);

        return gm::noone;
    }
    simplecatch(L"LoadPNGToBackground", gm::noone)
}