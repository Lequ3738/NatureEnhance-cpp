#include "Main.h"
#include <vector>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

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

std::vector<int> GroupElementList;

/// <summary>
/// 判断某项是否为组
/// </summary>
/// <param name="id">项目 ID</param>
/// <returns>若为组，则返回 true，否则返回 false。</returns>
bool IsGroup(int id)
{
	for (int ele : GroupElementList)
	{
		if (ele == id)
			return true;
	}

	return id < 10000;
}

static std::string join_vec(const std::vector<int>& v, const std::string& sep = "|")
{
	std::string out;
	for (size_t i = 0; i < v.size(); ++i)
	{
		if (i) out += sep;
		out += std::to_string(v[i]);
	}
	return out;
}

expReal ExpandIngredientList(GMReal ingreList, GMReal ingreTypeMap)
{
	try
	{
		GroupElementList.clear();

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

		// 统计在输入中出现的组，以及该组是否有元素形式出现
		std::vector<int> groups_in_input, two_choice_groups;

		{
			std::unordered_map<int, std::vector<int>> pos_by_group;
			// 记录每个组中以元素形式出现的项目及其在输入中的位置
			std::unordered_map<int, std::vector<std::pair<int, int>>> elem_items_by_group;

			for (size_t i = 0; i < ingres.size(); ++i)
			{
				// 获取该元素所属组
				int group;
				auto group_opt = ItemToGroup(ingres[i]);
				if (group_opt.has_value())
					group = group_opt.value();
				else  // 若该元素不属于任何组，则将其作为组来处理
				{
					group = ingres[i];
					std::unordered_set<int> groups_set(GroupElementList.begin(),
						GroupElementList.end());

					if (!groups_set.count(group))
						GroupElementList.push_back(group);
				}

				// 统计在输入中出现的组
				if (!pos_by_group.count(group))
					groups_in_input.push_back(group);

				pos_by_group[group].push_back(i);

				// 统计在输入中出现的元素
				if (ingres[i] != group)
					elem_items_by_group[group].push_back({ i, ingres[i] });
			}

			// 获取有两种展开选择的项目：在输入中，组同时出现了元素
			for (int g : groups_in_input)
			{
				if (!elem_items_by_group[g].empty())
					two_choice_groups.push_back(g);
			}
		}

		std::unordered_set<std::string> seen;
		std::vector<std::vector<int>> all_variants;

		// 生成所有可能的展开组合
		int total_masks = 1 << two_choice_groups.size();
		for (int mask = 0; mask < total_masks; ++mask)
		{
			std::unordered_map<int, bool> keep_elements;
			for (int g : groups_in_input)
				keep_elements[g] = false; // 默认折叠
			
			// 根据掩码，将 有两种展开选择的项目 进行展开选择
			for (int i = 0; i < two_choice_groups.size(); ++i)
				keep_elements[two_choice_groups[i]] = ((mask >> i) & 1) != 0;

			std::unordered_map<int, bool> emitted_group_once;
			std::vector<int> out;
			out.reserve(ingres.size());

			for (int i = 0; i < (int)ingres.size(); ++i)
			{
				auto group_opt = ItemToGroup(ingres[i]);
				int group = group_opt.has_value() ? group_opt.value() : ingres[i];
				
				bool keepElems = keep_elements[group]; // 没在 map 的组默认 false（折叠）
				if (!keepElems)
				{
					if (!emitted_group_once[group])
					{
						out.push_back(group);
						emitted_group_once[group] = true;
					}
				}
				else
				{
					// 保留元素：只有当元素不是裸组时才输出
					if (ingres[i] != group)
						out.push_back(ingres[i]);
				}
			}

			// 去重，并加入到结果列表中
			std::string key = join_vec(out);
			if (!seen.count(key))
			{
				seen.insert(key);
				all_variants.push_back(move(out));
			}
		}

		// 排序：按保留元素数量降序 -> 总长度降序 -> 字典序（字符串 key）升序
		auto countElements = [&](const std::vector<int>& v) -> int
		{
			int cnt = 0;
			for (int s : v)
			{
				if (!IsGroup(s))
					++cnt;
			}
			return cnt;
		};

		sort(all_variants.begin(), all_variants.end(), 
			[&](const std::vector<int>& a, const std::vector<int>& b)
			{
				int ca = countElements(a);
				int cb = countElements(b);
				if (ca != cb) return ca > cb;
				if (a.size() != b.size()) return a.size() > b.size();
				return join_vec(a) < join_vec(b);
			});

		// 将结果写回 ds_list
		int gmResult = gm::ds_list_create();
		for (auto& variants : all_variants)
		{
			int sublist = gm::ds_list_create();
			for (int v : variants)
				gm::ds_list_add(sublist, v);

			gm::ds_list_add(gmResult, sublist);
		}

		GroupElementList.clear();
		return gmResult;
	}
	simplecatch("ExpandIngredientList", gm::noone);
}