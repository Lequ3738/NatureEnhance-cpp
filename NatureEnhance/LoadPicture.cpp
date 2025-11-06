#include "Main.h"
#include "lodepng.h"
#include "buffer.h"
#include <future>

typedef std::future<std::tuple<std::vector<UCHAR>, UINT, UINT>> PNGDecodeFuture;

PNGDecodeFuture AsyncDecodePNG(GMString file)
{
    return std::async(std::launch::async, [file]() {
        std::vector<UCHAR> image, d3dimage;
        UINT width, height;
        UINT error = lodepng::decode(image, width, height, file);
        if (error)
            throw std::runtime_error(lodepng_error_text(error));

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
    simplecatch("ToBackgroundAsync", gm::noone)
}

PNGDecodeFuture AsyncDecodeGMBCK(GMString file)
{
    return std::async(std::launch::async, [file]() {
        GMReal buffer = gm::buffer_create();
		bool result = (bool)gm::buffer_read_from_file(buffer, file);
        if (!result)
			throw std::runtime_error("无法打开指定的文件：" + std::string(file));

        gm::buffer_set_pos(buffer, 4);  // 跳过 1234321
		UINT size = (UINT)gm::buffer_read_int32(buffer);

        GMReal data = gm::buffer_create();
        gm::buffer_write_buffer_part(data, buffer, gm::buffer_get_pos(buffer), size);
        gm::buffer_destroy(buffer);

        gm::buffer_zlib_uncompress(data);

        // 这 36 个字节分别为 (4 字节对齐)：
		// 版本号 710、是否作为贴图使用、贴图宽、贴图高、垂直位移、水平位移、垂直步宽、水平步宽、版本号 800
		gm::buffer_set_pos(data, sizeof(UINT) * 9);  // 跳过头部 36 字节
        UINT width = (UINT)gm::buffer_read_int32(data);
        UINT height = (UINT)gm::buffer_read_int32(data);
        size = (UINT)gm::buffer_read_int32(data);
        if (size == 0)
			throw std::runtime_error("无效的图片数据块大小。");

        UCHAR* imageData = (UCHAR*)(int)gm::buffer_get_address(data, false);
        if (imageData == nullptr)
			throw std::runtime_error("无效的图片数据块。");

		imageData += sizeof(UINT) * 12;  // 跳过头部数据，直接指向图片数据

		// 剩下的数据为 D3D8 的 ARGB 格式数据，无需转换
        std::vector<UCHAR> image;
		image.resize(size);

        memcpy(image.data(), imageData, size);
        gm::buffer_destroy(data);

		return std::make_tuple(std::move(image), width, height);
    });
}

expReal LoadGMBCKAsync(GMString file)
{
    AsyncDecodePNGList[AsyncDecodePNGId] = AsyncDecodeGMBCK(file);
    return (GMReal)AsyncDecodePNGId++;
}

expReal LoadBackgroundAsync(GMString file)
{
    GMString ext = gm::filename_ext(file);
    
    if (strcmp(ext, ".png") == 0)
        return LoadPNGAsync(file);
    else if (strcmp(ext, ".gmbck") == 0)
        return LoadGMBCKAsync(file);
    else
		return -1;  // 不支持的格式
}