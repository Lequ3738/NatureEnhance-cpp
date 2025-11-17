#include "Main.h"
#include <vector>
#include <optional>
#include <unordered_map>

std::vector<int> DSListToVector(GMReal list)
{
	std::vector<int> result;
	int listID = (int)list;

	for (int i = 0; i < gm::ds_list_size(listID); ++i)
	{
		gm::CGMVariable val = gm::ds_list_find_value(listID, i);
		result.push_back(static_cast<int>(val));
	}

	return result;
}

std::unordered_map<int, int> DSMapToCPPMap(GMReal map)
{
	std::unordered_map<int, int> result;
	int mapID = (int)map;

	int key = (int)gm::ds_map_find_first(mapID);
	int last = (int)gm::ds_map_find_last(mapID);
	while (!gm::ds_map_empty(mapID))
	{
		result[key] = (int)gm::ds_map_find_value(mapID, key);

		if (key == last)
			break;
		key = (int)gm::ds_map_find_next(mapID, key);
	}

	return result;
}

/// <summary>
/// 判断某项是否为组
/// </summary>
/// <param name="id">项目 ID</param>
/// <returns>若为组，则返回 true，否则返回 false。</returns>
bool IsGroup(int id) { return id < 10000; }

expReal ExpandIngredientList(GMReal ingreList, GMReal ingreTypeMap)
{
	try
	{
		auto ingres = DSListToVector(ingreList);
		auto ingreType = DSMapToCPPMap(ingreTypeMap);

		// 将单项映射到组
		auto ItemToGroup = [&](int id) -> std::optional<int>
		{
			if (IsGroup(id))  // 本身即为组
				return id;

			auto it = ingreType.find(id);
			if (it != ingreType.end())  // 明确映射到组
				return it->second;

			return std::nullopt;  // 未匹配
		};

		for (size_t i = 0; i < ingres.size(); ++i)
		{
			int group;
			auto group_opt = ItemToGroup(ingres[i]);
			if (group_opt.has_value())
				group = group_opt.value();
			else
			{

			}
		}
	}
	simplecatch("ExpandIngredientList", gm::noone);
}