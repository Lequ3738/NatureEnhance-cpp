#include "Main.h"
#include "buffer.h"
#include <vector>
#include <filesystem>
#include <string>
#include <unordered_map>

std::vector<int> DrawSpritesList;

// 同房间热重载会话追踪：记录本房间内由 .bin 生成的内容，重载时先清除再重建。
// 房间切换（正常 Room Start）只丢弃记录不触碰内容，防止操作已销毁的列表。
static std::vector<int> LiveTiles;
static std::vector<int> LiveInstances;
struct LiveSpriteRef
{
	int list;
	int pos;
	int map;
};
static std::vector<LiveSpriteRef> LiveSprites;

// Live 增量协议（编辑器 delta v2）：编辑器会话 id → 游戏侧句柄/位置
static std::unordered_map<std::string, int> s_resCache;          // 资源名 → id（GetResource 结果缓存，execute_string 开销大头）
static std::unordered_map<int, int> s_tileById;                  // id → tile handle
static std::unordered_map<int, LiveSpriteRef> s_sprRefById;      // id → 绘制列表引用
static std::unordered_map<int, int> s_instById;                  // id → 实例 id
static unsigned int s_expectedSeq = 0;                           // 下一 delta 的序号（全量后清 0）

// 资源名解析缓存：同名资源每拍重复解析是全量/增量共同的执行字符串开销大头
static int GetResourceCached(const std::string& name)
{
	auto it = s_resCache.find(name);
	if (it != s_resCache.end())
		return it->second;
	int id = static_cast<int>(GetResource(name));
	s_resCache.emplace(name, id);
	return id;
}

static void RoomTilesSessionReset()
{
	LiveTiles.clear();
	LiveInstances.clear();
	LiveSprites.clear();
	s_tileById.clear();
	s_sprRefById.clear();
	s_instById.clear();
	s_expectedSeq = 0;
}

// delta 删除一个精灵表项后，同一列表中更高位置的记录位置前移
static void AdjustSpritePositionsAfterDelete(int list, int pos)
{
	for (LiveSpriteRef& r : LiveSprites)
		if (r.list == list && r.pos > pos)
			r.pos -= 1;
	for (auto& kv : s_sprRefById)
		if (kv.second.list == list && kv.second.pos > pos)
			kv.second.pos -= 1;
}

// 静默销毁：直接置 runner 的实例 destroyed 标记（GMINSTANCE+0x108）。
// IDA 实证（GM8 runner 空工程.exe）：Inner_instance_destroy(0x4E5310) =
// 「InnerInstanceDestroy 执行 ev_destroy 事件」+「*(BYTE*)(inst+264)=1」两步，
// 标记即 runner 的延迟回收机制——只置标记 = instance_destroy 去掉事件。
// 勿用 instance_change：INNER_instance_change(0x4E5320) 的 perf=false 是
// 「标记旧实例 + gm_room_instance_add 新建空壳（新 id，仅拷 x/y 等字段）」，
// 并非就地变形；且对已标记实例再调 instance_destroy 时事件照常执行
// （InstDestroy 不检查标记）——会触发敌人死亡掉落。
static void SilentDestroyById(int id)
{
	gm::PGMINSTANCE inst = gmapi->GetInstancePtr(id);
	if (inst)
		inst->destroyed = true;
}

static void LiveRoomCleanup()
{
	for (int tile : LiveTiles)
		if (gm::tile_exists(tile))
			gm::tile_delete(tile);

	// 从后往前按记录位置移除；位置内容对不上时跳过，防误删外部条目
	for (int i = (int)LiveSprites.size() - 1; i >= 0; --i)
	{
		int list = LiveSprites[i].list;
		int pos = LiveSprites[i].pos;
		int map = LiveSprites[i].map;
		if (pos < 0 || pos >= gm::ds_list_size(list))
			continue;
		if ((int)gm::ds_list_find_value(list, pos) != map)
			continue;
		gm::ds_list_delete(list, pos);
		gm::ds_map_destroy(map);
	}

	if (!LiveInstances.empty())
	{
		for (int id : LiveInstances)
			SilentDestroyById(id);
	}

	RoomTilesSessionReset();
}

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

expReal RoomtilesLiveReset()
{
	RoomTilesSessionReset();
	finish;
}

static void LoadRoomTilesParse(GMReal buffer, GMReal tileLayerList, std::string& err)
{
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

	// Tiles
	num = static_cast<UINT>(gm::buffer_read_uint32(buffer));
	resList.reserve(num);
	resExistsList.reserve(num);

	for (UINT i = 0; i < num; ++i)
	{
		std::string name = gm::buffer_read_string(buffer);
		int back = static_cast<int>(GetResourceCached(name));
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

		LiveTiles.push_back(tile);
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
		int spr = static_cast<int>(GetResourceCached(name));
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
		LiveSprites.push_back({ DrawSpritesList[listPos],
			gm::ds_list_size(DrawSpritesList[listPos]) - 1, map });
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
			int obj = static_cast<int>(GetResourceCached(name));
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
			LiveInstances.push_back(id);

			if (!icc.empty())
				code += "with " + std::to_string(id) + " {\n" + icc + "\n}\n";
		}

		gm::execute_string(code);
	}
}

expReal LoadRoomTiles(GMString path, GMReal tileLayerList)
{
	try
	{
		if (!std::filesystem::exists(path))
			fail;

		RoomTilesSessionReset();

		GMReal buffer = gm::buffer_create();
		gm::buffer_read_from_file(buffer, path);

		std::string err = "";
		LoadRoomTilesParse(buffer, tileLayerList, err);

		gm::buffer_destroy(buffer);

		if (err != "")
			throw std::runtime_error(err);

		finish;
	}
	simplecatch("scrLoadRoomTiles()", 0)
}

// 同房间热重载：清除上次生成的内容后，从内存 buffer 解析整房数据。
// buffer 由游戏侧持有（socket 收到的消息），DLL 不负责销毁。
expReal LoadRoomTilesBuffer(GMReal buffer, GMReal tileLayerList)
{
	try
	{
		LiveRoomCleanup();

		std::string err = "";
		LoadRoomTilesParse(buffer, tileLayerList, err);

		if (err != "")
			throw std::runtime_error(err);

		finish;
	}
	simplecatch("LoadRoomTilesBuffer()", 0)
}

// Live 增量应用：buffer 位置位于 delta 头（游戏侧已读过 type 与房间名）。
// 头 = u32 seq + u32 opCount + ops；op 字段序与编辑器 liveRoom.ts 编码严格一致。
// 返回：1 = 已应用；2 = 序号失步（调用方发请求，编辑器回全量）；0 = 应用出错（同上）。
// 实体更新（编辑器以删+加同 id 表达）：瓦片/精灵定向重建，对象重建会重跑 ICC。
expReal LoadRoomTilesDelta(GMReal buffer, GMReal tileLayerList)
{
	try
	{
		UINT seq = static_cast<UINT>(gm::buffer_read_uint32(buffer));
		if (seq != s_expectedSeq + 1)
			return 2;
		s_expectedSeq = seq;

		std::string err = "";
		std::string gmlCode;
		UINT opCount = static_cast<UINT>(gm::buffer_read_uint32(buffer));

		for (UINT i = 0; i < opCount; ++i)
		{
			UINT kind = static_cast<UINT>(gm::buffer_read_uint8(buffer));

			if (kind == 0) // addTile
			{
				int id = static_cast<int>(gm::buffer_read_uint32(buffer));
				std::string reso = gm::buffer_read_string(buffer);
				int left = static_cast<int>(gm::buffer_read_int32(buffer));
				int top = static_cast<int>(gm::buffer_read_int32(buffer));
				int width = static_cast<int>(gm::buffer_read_int32(buffer));
				int height = static_cast<int>(gm::buffer_read_int32(buffer));
				GMReal x = gm::buffer_read_int32(buffer);
				GMReal y = gm::buffer_read_int32(buffer);
				int depth = static_cast<int>(gm::buffer_read_int32(buffer));
				GMReal xscale = gm::buffer_read_float32(buffer);
				GMReal yscale = gm::buffer_read_float32(buffer);
				GMReal alpha = gm::buffer_read_uint8(buffer) / 255;
				int blend = static_cast<int>(gm::buffer_read_uint32(buffer));

				int back = GetResourceCached(reso);
				if (!gm::background_exists(back))
				{
					err += "delta addTile 背景 (" + reso + ") 不存在。\n";
					continue;
				}
				int tile = gm::tile_add(back, left, top, width, height, x, y, depth);
				gm::tile_set_scale(tile, xscale, yscale);
				gm::tile_set_alpha(tile, alpha);
				gm::tile_set_blend(tile, blend);
				LiveTiles.push_back(tile);
				s_tileById[id] = tile;
			}
			else if (kind == 1) // delTile
			{
				int id = static_cast<int>(gm::buffer_read_uint32(buffer));
				auto it = s_tileById.find(id);
				if (it != s_tileById.end())
				{
					if (gm::tile_exists(it->second))
						gm::tile_delete(it->second);
					s_tileById.erase(it);
				}
			}
			else if (kind == 2) // addSprite
			{
				int id = static_cast<int>(gm::buffer_read_uint32(buffer));
				std::string reso = gm::buffer_read_string(buffer);
				GMReal scrollX = gm::buffer_read_float32(buffer);
				GMReal scrollY = gm::buffer_read_float32(buffer);
				int curIndex = static_cast<int>(gm::buffer_read_int32(buffer));
				int x = static_cast<int>(gm::buffer_read_int32(buffer));
				int y = static_cast<int>(gm::buffer_read_int32(buffer));
				GMReal xscale = gm::buffer_read_float32(buffer);
				GMReal yscale = gm::buffer_read_float32(buffer);
				GMReal alpha = gm::buffer_read_uint8(buffer) / 255;
				GMReal speed = gm::buffer_read_float32(buffer);
				int blend = static_cast<int>(gm::buffer_read_uint32(buffer));
				size_t listPos = static_cast<size_t>(gm::buffer_read_uint8(buffer));

				int spr = GetResourceCached(reso);
				if (!gm::sprite_exists(spr))
				{
					err += "delta addSprite 精灵 (" + reso + ") 不存在。\n";
					continue;
				}
				if (listPos >= DrawSpritesList.size())
				{
					err += "delta addSprite 层 " + std::to_string(listPos) + " 不存在。\n";
					continue;
				}

				int map = gm::ds_map_create();
				gm::ds_map_add(map, "sprite", spr);
				gm::ds_map_add(map, "scrollX", scrollX);
				gm::ds_map_add(map, "scrollY", scrollY);
				gm::ds_map_add(map, "curIndex", curIndex);
				gm::ds_map_add(map, "x", x);
				gm::ds_map_add(map, "y", y);
				gm::ds_map_add(map, "xscale", xscale);
				gm::ds_map_add(map, "yscale", yscale);
				gm::ds_map_add(map, "alpha", alpha);
				gm::ds_map_add(map, "speed", speed);
				gm::ds_map_add(map, "blend", blend);
				if (speed != 0)
					gm::ds_map_replace(map, "curIndex", 0);

				gm::ds_list_add(DrawSpritesList[listPos], map);
				LiveSpriteRef ref = { DrawSpritesList[listPos],
					gm::ds_list_size(DrawSpritesList[listPos]) - 1, map };
				LiveSprites.push_back(ref);
				s_sprRefById[id] = ref;
			}
			else if (kind == 3) // delSprite
			{
				int id = static_cast<int>(gm::buffer_read_uint32(buffer));
				auto it = s_sprRefById.find(id);
				if (it != s_sprRefById.end())
				{
					int list = it->second.list;
					int pos = it->second.pos;
					int map = it->second.map;
					if (pos >= 0 && pos < gm::ds_list_size(list)
						&& (int)gm::ds_list_find_value(list, pos) == map)
					{
						gm::ds_list_delete(list, pos);
						gm::ds_map_destroy(map);
						AdjustSpritePositionsAfterDelete(list, pos);
					}
					// 位置对不上：条目已漂移，放弃定向删除（保留下来的 map 交由下次全量清理）
					s_sprRefById.erase(it);
				}
			}
			else if (kind == 4) // addObject
			{
				int id = static_cast<int>(gm::buffer_read_uint32(buffer));
				std::string reso = gm::buffer_read_string(buffer);
				GMReal x = gm::buffer_read_int32(buffer);
				GMReal y = gm::buffer_read_int32(buffer);
				GMReal xscale = gm::buffer_read_float32(buffer);
				GMReal yscale = gm::buffer_read_float32(buffer);
				std::string uid = gm::buffer_read_string(buffer);
				std::string icc = gm::buffer_read_string(buffer);

				int obj = GetResourceCached(reso);
				if (!gm::object_exists(obj))
				{
					err += "delta addObject 对象 (" + reso + ") 不存在。\n";
					continue;
				}
				int iid = gm::instance_create(x, y, obj);
				instance_set_scale(iid, xscale, yscale);
				LiveInstances.push_back(iid);
				s_instById[id] = iid;
				if (!icc.empty())
					gmlCode += "with " + std::to_string(iid) + " {\n" + icc + "\n}\n";
			}
			else if (kind == 5) // delObject
			{
				int id = static_cast<int>(gm::buffer_read_uint32(buffer));
				auto it = s_instById.find(id);
				if (it != s_instById.end())
				{
					SilentDestroyById(it->second);
					s_instById.erase(it);
				}
			}
			else if (kind == 6) // setLayers
			{
				int layerList = static_cast<int>(tileLayerList);
				UINT n = static_cast<UINT>(gm::buffer_read_uint16(buffer));
				gm::ds_list_clear(layerList);
				for (UINT k = 0; k < n; ++k)
					gm::ds_list_add(layerList, gm::buffer_read_int32(buffer));
			}
			else
			{
				err += "delta 未知 op kind " + std::to_string(kind) + "。\n";
			}
		}

		if (!gmlCode.empty())
			gm::execute_string(gmlCode);

		if (err != "")
			throw std::runtime_error(err);

		finish;
	}
	simplecatch("LoadRoomTilesDelta()", 0)
}
