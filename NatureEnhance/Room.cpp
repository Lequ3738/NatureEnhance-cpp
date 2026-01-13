#include "Main.h"
#include "buffer.h"
#include <vector>
#include <filesystem>

std::vector<int> DrawSpritesList;

expReal ClearDrawSpritesList()
{
	DrawSpritesList.clear();
	finish;
}

expReal PushDrawSpritesList(GMReal list)
{
	DrawSpritesList.push_back(static_cast<int>(list));
	finish;
}

expReal LoadRoomTiles(GMString path, GMReal tileLayerList)
{
	try
	{
		if (!std::filesystem::exists(path))
			fail;

		GMReal buffer = gm::buffer_create();
		gm::buffer_read_from_file(buffer, path);

		UINT version = (UINT)gm::buffer_read_uint8(buffer);

		UINT num;
		if (version == 0)
		{
			num = static_cast<UINT>(gm::buffer_read_uint32(buffer));

			for (UINT i = 0; i < num; ++i)
			{
				gm::buffer_read_int32(buffer);
				gm::buffer_read_string(buffer);
			}
		}
		else if (version >= 2)
		{
			num = static_cast<UINT>(gm::buffer_read_uint16(buffer));
			int layerList = static_cast<int>(tileLayerList);

			gm::ds_list_clear(layerList);
			for (UINT i = 0; i < num; ++i)
				gm::ds_list_add(layerList, gm::buffer_read_int32(buffer));
		}

		std::vector<int> resList;
		std::vector<bool> resExistsList;
		std::string err = "";

		// Tiles
		num = static_cast<UINT>(gm::buffer_read_uint32(buffer));
		resList.reserve(num);
		resExistsList.reserve(num);

		for (UINT i = 0; i < num; ++i)
		{
			std::string name = gm::buffer_read_string(buffer);
			int back = static_cast<int>(GetResource(name));
			resList.push_back(back);

			if (!gm::background_exists(back))
			{
				err += "在 scrLoadRoomTiles() 中，背景 (" + name + ") 不存在。\n";
				resExistsList.push_back(false);
			}
			else
				resExistsList.push_back(true);
		}

		num = static_cast<UINT>(gm::buffer_read_uint32(buffer));
		for (UINT i = 0; i < num; ++i)
		{
			int pos = static_cast<int>(gm::buffer_read_int32(buffer));
			if (!resExistsList[pos])
			{
				gm::buffer_jump(buffer, 9 * 4 + 1);
				if (version >= 1)
					gm::buffer_jump(buffer, 4);

				continue;
			}

			int left = static_cast<int>(gm::buffer_read_int32(buffer));
			int top = static_cast<int>(gm::buffer_read_int32(buffer));
			int width = static_cast<int>(gm::buffer_read_int32(buffer));
			int height = static_cast<int>(gm::buffer_read_int32(buffer));
			GMReal x = gm::buffer_read_int32(buffer);
			GMReal y = gm::buffer_read_int32(buffer);
			int depth = static_cast<int>(gm::buffer_read_int32(buffer));
			GMReal xscale = gm::buffer_read_float32(buffer);
			GMReal yscale = gm::buffer_read_float32(buffer);

			int tile = gm::tile_add(resList[pos], left, top, width, height, x, y, depth);
			gm::tile_set_scale(tile, xscale, yscale);
			gm::tile_set_alpha(tile, gm::buffer_read_uint8(buffer) / 255);

			if (version >= 1)
				gm::tile_set_blend(tile, (int)gm::buffer_read_uint32(buffer));
		}

		resList.clear();
		resExistsList.clear();

		// Sprites
		num = static_cast<UINT>(gm::buffer_read_uint32(buffer));
		resList.reserve(num);
		resExistsList.reserve(num);

		for (UINT i = 0; i < num; ++i)
		{
			std::string name = gm::buffer_read_string(buffer);
			int spr = static_cast<int>(GetResource(name));
			resList.push_back(spr);

			if (!gm::sprite_exists(spr))
			{
				err += "在 scrLoadRoomTiles() 中，Sprite (" + name + ") 不存在。\n";
				resExistsList.push_back(false);
			}
			else
				resExistsList.push_back(true);
		}

		num = static_cast<UINT>(gm::buffer_read_uint32(buffer));
		for (UINT i = 0; i < num; ++i)
		{
			int pos = static_cast<int>(gm::buffer_read_int32(buffer));
			if (!resExistsList[pos])
			{
				gm::buffer_jump(buffer, 8 * 4 + 2);
				if (version >= 1)
					gm::buffer_jump(buffer, 4);

				continue;
			}

			int map = gm::ds_map_create();
			gm::ds_map_add(map, "sprite", resList[pos]);
			gm::ds_map_add(map, "scrollX", gm::buffer_read_float32(buffer));
			gm::ds_map_add(map, "scrollY", gm::buffer_read_float32(buffer));
			gm::ds_map_add(map, "curIndex", gm::buffer_read_int32(buffer));
			gm::ds_map_add(map, "x", gm::buffer_read_int32(buffer));
			gm::ds_map_add(map, "y", gm::buffer_read_int32(buffer));
			gm::ds_map_add(map, "xscale", gm::buffer_read_float32(buffer));
			gm::ds_map_add(map, "yscale", gm::buffer_read_float32(buffer));
			gm::ds_map_add(map, "alpha", gm::buffer_read_uint8(buffer) / 255);
			gm::ds_map_add(map, "speed", gm::buffer_read_float32(buffer));

			if (version >= 1)
				gm::ds_map_add(map, "blend", gm::buffer_read_uint32(buffer));

			if (gm::ds_map_find_value(map, "speed") != 0)
				gm::ds_map_replace(map, "curIndex", 0);

			size_t listPos = static_cast<size_t>(gm::buffer_read_uint8(buffer));
			if (listPos >= DrawSpritesList.size())
			{
				err += "在 scrLoadRoomTiles() 中，层 " + std::to_string(listPos) + " 不存在。\n";
				continue;
			}

			gm::ds_list_add(DrawSpritesList[listPos], map);
		}

		resList.clear();
		resExistsList.clear();

		// Objects
		if (version >= 3)
		{
			num = static_cast<UINT>(gm::buffer_read_uint32(buffer));
			resList.reserve(num);
			resExistsList.reserve(num);

			for (UINT i = 0; i < num; ++i)
			{
				std::string name = gm::buffer_read_string(buffer);
				int obj = static_cast<int>(GetResource(name));
				resList.push_back(obj);

				if (!gm::object_exists(obj))
				{
					err += "在 scrLoadRoomTiles() 中，Object (" + name + ") 不存在。\n";
					resExistsList.push_back(false);
				}
				else
					resExistsList.push_back(true);
			}

			std::string code;

			num = static_cast<UINT>(gm::buffer_read_uint32(buffer));
			for (UINT i = 0; i < num; ++i)
			{
				int pos = static_cast<int>(gm::buffer_read_int32(buffer));
				if (!resExistsList[pos])
				{
					gm::buffer_jump(buffer, 4 * 4);
					gm::buffer_read_string(buffer);
					gm::buffer_read_string(buffer);
					continue;
				}

				GMReal x = gm::buffer_read_int32(buffer);
				GMReal y = gm::buffer_read_int32(buffer);
				GMReal xscale = gm::buffer_read_float32(buffer);
				GMReal yscale = gm::buffer_read_float32(buffer);
				std::string uid = gm::buffer_read_string(buffer);
				std::string icc = gm::buffer_read_string(buffer);

				int id = gm::instance_create(x, y, resList[pos]);
				instance_set_scale(id, xscale, yscale);

				if (!icc.empty())
					code += "with " + std::to_string(id) + " {\n" + icc + "\n}\n";
			}

			gm::execute_string(code);
		}

		gm::buffer_destroy(buffer);

		if (err != "")
			throw std::runtime_error(err);

		finish;
	}
	simplecatch("scrLoadRoomTiles()", 0)
}