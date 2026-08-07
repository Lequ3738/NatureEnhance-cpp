#include "LoadPicture.h"
#include <algorithm>

struct image_rect
{
	int left;
	int top;
	int right;
	int bottom;
};

static image_rect crop_blank_area(std::vector<UCHAR>& image, uint width, uint height)
{
	int top = 0, bottom = 0, left = 0, right = 0;
	
	for (int y = 0; y < (int)height; ++y)
	{
		for (int x = 0; x < (int)width; ++x)
		{
			uint idx = (y * width + x) * 4;
			if (image[idx + 3] > 0)
			{
				top = y;
				goto calc_bottom;
			}
		}
	}
	return { 0, 0, 0, 0 };

calc_bottom:
	for (int y = (int)height - 1; y >= top; --y)
	{
		for (int x = 0; x < (int)width; ++x)
		{
			uint idx = (y * width + x) * 4;
			if (image[idx + 3] > 0)
			{
				bottom = y;
				goto calc_left;
			}
		}
	}
calc_left:
	for (int x = 0; x < (int)width; ++x)
	{
		for (int y = top; y <= bottom; ++y)
		{
			uint idx = (y * width + x) * 4;
			if (image[idx + 3] > 0)
			{
				left = x;
				goto calc_right;
			}
		}
	}
calc_right:
	for (int x = (int)width - 1; x >= left; --x)
	{
		for (int y = top; y <= bottom; ++y)
		{
			uint idx = (y * width + x) * 4;
			if (image[idx + 3] > 0)
			{
				right = x;
				goto calc_end;
			}
		}
	}
calc_end:
	return { left, top, right, bottom };
}

static void apply_color_bleeding(std::vector<UCHAR>& image_data, uint width, uint height)
{
	image_rect cropped_rect = crop_blank_area(image_data, width, height);
	if (cropped_rect.right < cropped_rect.left || cropped_rect.bottom < cropped_rect.top)
		return;

	std::vector<UCHAR> buffer = image_data;

	int process_left = cropped_rect.left - 1;
	int process_top = cropped_rect.top - 1;
	int process_right = cropped_rect.right + 1;
	int process_bottom = cropped_rect.bottom + 1;

	process_left = std::max(process_left, 0);
	process_top = std::max(process_top, 0);
	process_right = std::min(process_right, (int)width - 1);
	process_bottom = std::min(process_bottom, (int)height - 1);

	for (int y = process_top; y <= process_bottom; ++y)
	{
		for (int x = process_left; x <= process_right; ++x)
		{
			uint idx = (y * (int)width + x) * 4;
			UCHAR a = buffer[idx + 3];

			if (a == 0)
			{
				int b_sum = 0, g_sum = 0, r_sum = 0;
				int count = 0;

				for (int dy = -1; dy <= 1; ++dy)
				{
					for (int dx = -1; dx <= 1; ++dx)
					{
						if (dx == 0 && dy == 0) continue;

						int nx = x + dx;
						int ny = y + dy;

						if (nx >= 0 && nx < (int)width && ny >= 0 && ny < (int)height)
						{
							uint n_idx = (ny * (int)width + nx) * 4;
							UCHAR n_a = buffer[n_idx + 3];

							if (n_a > 0)
							{
								b_sum += buffer[n_idx];
								g_sum += buffer[n_idx + 1];
								r_sum += buffer[n_idx + 2];
								count++;
							}
						}
					}
				}

				if (count > 0)
				{
					image_data[idx] = (UCHAR)(b_sum / count);
					image_data[idx + 1] = (UCHAR)(g_sum / count);
					image_data[idx + 2] = (UCHAR)(r_sum / count);
				}
			}
		}
	}
}

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

		apply_color_bleeding(d3dimage, width, height);  // 执行边缘膨胀

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
		AsyncDecodePNGList.erase((UINT)id);

        auto [d3dimage, width, height] = getFuture.get();

        // 新建 GM 的背景资源并返回其纹理和表面
        int back = gm::background_create_color(width, height, 0);
        void* texture = gmapi->Backgrounds[back].GetTexture();

        void* surf = nullptr;
        D3DCheck(d3d::get_surface_level(texture, 0, &surf), 1);

        // 载入数据
        RECT rect = { .left = 0, .top = 0, .right = (long)width, .bottom = (long)height };

        D3DCheck(d3d::load_surface_from_memory(surf, &rect, d3dimage.data(),
            D3DFMT_A8R8G8B8, width * 4, &rect), 2);

        D3DCheck(d3d::add_dirty_rect(texture, &rect), 3);
        d3d::release(surf);

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

		apply_color_bleeding(image, width, height);  // 执行边缘膨胀

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