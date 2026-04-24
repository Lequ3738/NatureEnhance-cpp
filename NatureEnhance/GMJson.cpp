#include "Main.h"
#include "json.hpp"
#include <stack>
#include <vector>
#include "buffer.h"

using Json = nlohmann::json;
using namespace std;

// --------------------- 全局容器 ---------------------
static int JsonObjIDCounter = 10000000;
unordered_map<int, Json> JsonObjMap;
unordered_map<int, pair<int, string>> JsonRefMap;		// 子引用ID -> {根ID, JSON Pointer路径}
unordered_map<int, bool> JsonIsRootMap;					// ID -> 是否为根ID（快速判断）
unordered_map<int, int> JsonObjToDSMap;					// JSON ID -> 解析后的DS根ID映射

dynamic JsonQueryResult;								// JSON Pointer 查询全局结果
int JsonQueryResultType = -5;

unordered_map<int, vector<int>> JsonRootToChildrenMap;  // 根ID -> 子引用ID列表
unordered_map<int, int> JsonChildToRootMap;             // 子引用ID -> 根ID
unordered_map<int, unordered_map<string, int>> JsonPathCacheMap;  // 子引用路径缓存

// --------------------- TreeNode 容器 ---------------------
struct TreeNode;
unordered_map<int, TreeNode*> JsonDataMap;  // 存放所有由 JsonParse 创建的树结构根节点
// 存放所有由 JsonParse 创建的树结构的所有节点
unordered_map<int, TreeNode*> JsonNodeListMap, JsonNodeMapMap;

struct TreeNode
{
	bool ismap;
	TreeNode* parent;
	vector<TreeNode*> children;
	int data = gm::noone;

	vector<char> typeList;
	unordered_map<string, char> typeMap;

	TreeNode(bool is_map) : parent(nullptr), ismap(is_map)
	{
		if (is_map)
		{
			data = gm::ds_map_create();
			JsonNodeMapMap[data] = this;
		}
		else
		{
			data = gm::ds_list_create();
			JsonNodeListMap[data] = this;
		}
	}

	~TreeNode()
	{
		if (ismap)
		{
			gm::ds_map_destroy(data);
			JsonNodeMapMap.erase(data);
		}
		else
		{
			gm::ds_list_destroy(data);
			JsonNodeListMap.erase(data);
		}

		for (TreeNode* child : children)
			delete child;
	}

	TreeNode* AddChild(bool is_map)
	{
		TreeNode* node = new TreeNode(is_map);
		children.push_back(node);
		node->parent = this;

		return node;
	}

	void RemoveNode()
	{
		for (TreeNode* child : children)
			child->parent = nullptr;

		children.clear();
		delete this;
	}
};

#define getvalue(v) \
	holds_alternative<string>(v) ? get<string>(v) : get<GMReal>(v)

struct StackItem
{
	Json* json;
	dynamic key;
	TreeNode* tree;
};

static bool contains(const vector<int>& vec, int target)
{
	return find(vec.begin(), vec.end(), target) != vec.end();
}

static void AddToParent(const StackItem& item, const gm::CGMVariable& value, char type)
{
	if (holds_alternative<string>(item.key))
	{
		gm::ds_map_add(item.tree->data, get<string>(item.key), value);
		item.tree->typeMap[get<string>(item.key)] = type;
	}
	else
	{
		gm::ds_list_add(item.tree->data, value);
		item.tree->typeList.push_back(type);
	}
}

static optional<reference_wrapper<Json>> GetJsonRef(int id)
{
	if (!JsonIsRootMap.contains(id))
		return nullopt;

	if (JsonIsRootMap[id])  // 根ID
	{
		if (!JsonObjMap.contains(id))
			return nullopt;
		return std::ref(JsonObjMap[id]);
	}
	else  // 子引用ID
	{
		if (!JsonRefMap.contains(id))
			return nullopt;
		auto& [rootID, pointerStr] = JsonRefMap[id];

		auto rootJsonOpt = GetJsonRef(rootID);
		if (!rootJsonOpt)
			return nullopt;

		try
		{
			Json::json_pointer ptr(pointerStr);
			return std::ref(rootJsonOpt->get().at(ptr));
		}
		catch (...)
		{
			return nullopt;
		}
	}
}

static int ConvertJsonToDS(Json* jsonObj)
{
	if (!jsonObj->is_object() && !jsonObj->is_array())
		throw runtime_error("JSON 顶层必须为对象或数组，不支持数字/字符串/布尔/null 作为顶层值。");

	TreeNode* treeRoot = new TreeNode(true);  // 存放数据结构的节点树
	stack<StackItem> jsonStack;
	jsonStack.push({ .json = jsonObj, .key = "", .tree = treeRoot });

	while (!jsonStack.empty())
	{
		StackItem item = jsonStack.top();
		jsonStack.pop();

		Json* curr = item.json;
		if (curr->is_object())
		{
			TreeNode* tree = item.tree->AddChild(true);

			for (auto i = curr->rbegin(); i != curr->rend(); ++i)
				jsonStack.push({ .json = &i.value(), .key = i.key(), .tree = tree });

			AddToParent(item, tree->data, (char)ds_type_map);
		}
		else if (curr->is_array())
		{
			TreeNode* tree = item.tree->AddChild(false);

			int num = 0;
			for (auto i = curr->rbegin(); i != curr->rend(); ++i, ++num)
				jsonStack.push({ .json = &i.value(), .key = (GMReal)num, .tree = tree });

			AddToParent(item, tree->data, (char)ds_type_list);
		}
		else if (curr->is_number())
			AddToParent(item, curr->get<GMReal>(), -1);
		else if (curr->is_string())
			AddToParent(item, curr->get<std::string>(), -2);
		else if (curr->is_boolean())
			AddToParent(item, static_cast<GMReal>(curr->get<bool>()), -3);
		else if (curr->is_null())
			AddToParent(item, gm::noone, -4);
		else
			throw runtime_error("不支持的 JSON 数据类型。");
	}

	// 解析完成后，获得的 ds_map 的树结构是这样的：
	//
	// root -> dataroot -> ...
	//
	// 所以要删除最顶层的 root 节点，获得从 json 根部开始的 ds_map 数据。

	// 提取根DS ID并清理临时节点
	int result = static_cast<int>(gm::ds_map_find_value(treeRoot->data, ""));
	JsonDataMap[result] = treeRoot->children.at(0);
	treeRoot->RemoveNode();  // 删除最顶层的树节点

	return result;
}

expReal JsonFree()
{
	for (auto& tree : JsonDataMap)
		delete tree.second;

	JsonObjMap.clear();
	JsonRefMap.clear();
	JsonIsRootMap.clear();
	JsonObjToDSMap.clear();
	JsonRootToChildrenMap.clear();
	JsonChildToRootMap.clear();
	JsonDataMap.clear();
	JsonNodeListMap.clear();
	JsonNodeMapMap.clear();
	JsonPathCacheMap.clear();

	finish;
}

expReal StringToJson(GMString jsonstr)
{
	try
	{
		Json json = Json::parse(jsonstr, nullptr, true, true);
		int objID = JsonObjIDCounter++;
		JsonObjMap[objID] = move(json);
		JsonIsRootMap[objID] = true;
		return objID;
	}
	catch (const std::exception& e)
	{
		if (show_error)
		{
			ShowMessage("在执行函数 StringToJson 出现错误：\n" + std::string(jsonstr) + "\n" + 
				e.what(), "NatureEnhance Error", MB_OK | MB_ICONERROR);
		}
		return gm::noone;
	}
}

expReal JsonParse(GMReal objID)
{
	try
	{
		int id = static_cast<int>(objID);
		auto jsonOpt = GetJsonRef(id);
		if (!jsonOpt.has_value())
			throw std::runtime_error("无效的 JSON 对象 ID：" + std::to_string(id));

		Json& json = jsonOpt->get();
		int result = ConvertJsonToDS(&json);
		JsonObjToDSMap[id] = result; // 关联 json 对象 ID 与 DS ID
		
		return result;
	}
	simplecatch("JsonParse", gm::noone)
}

expReal JsonGetDS(GMReal objID)
{
	int id = static_cast<int>(objID);
	if (!JsonObjToDSMap.contains(id))
		return gm::noone;

	return JsonObjToDSMap[id];
}

expReal JsonDSClear(GMReal rootNode)
{
	try
	{
		TreeNode* treeRoot = JsonDataMap.at((int)rootNode);
		JsonDataMap.erase((int)rootNode);
		
		if (treeRoot == nullptr)
			throw runtime_error("传入的 ds_map 引用无效。");
		else if (treeRoot->parent != nullptr)
			throw runtime_error("传入的 ds_map 引用不是由 JsonDecode() 函数生成的根引用。");

		delete treeRoot;
		finish;
	}
	simplecatch("JsonDSClear", false)
}

expReal JsonDestroy(GMReal objID)
{
	try
	{
		int id = static_cast<int>(objID);

		if (!JsonIsRootMap.contains(id) || !JsonIsRootMap[id])
			throw runtime_error("仅根 JSON 对象 ID 可被删除，子引用 ID 无法独立删除。");

		// 清理所有关联的子引用
		if (JsonRootToChildrenMap.contains(id))
		{
			for (int childID : JsonRootToChildrenMap[id])
			{
				// 清理子引用的所有映射
				JsonRefMap.erase(childID);
				JsonIsRootMap.erase(childID);
				JsonChildToRootMap.erase(childID);
				// 清理子引用关联的DS结构
				if (JsonObjToDSMap.contains(childID))
				{
					int dsRootID = JsonObjToDSMap[childID];
					if (JsonDataMap.contains(dsRootID))
					{
						TreeNode* treeRoot = JsonDataMap.at(dsRootID);
						JsonDataMap.erase(dsRootID);
						delete treeRoot;
					}
					JsonObjToDSMap.erase(childID);
				}
			}
			JsonRootToChildrenMap.erase(id);
		}

		// 清理路径缓存
		JsonPathCacheMap.erase(id);

		// 清理根ID关联的DS结构
		if (JsonObjToDSMap.contains(id))
		{
			int dsRootID = JsonObjToDSMap[id];
			if (JsonDataMap.contains(dsRootID))
			{
				TreeNode* treeRoot = JsonDataMap.at(dsRootID);
				JsonDataMap.erase(dsRootID);
				delete treeRoot;
			}
			JsonObjToDSMap.erase(id);
		}

		// 删除根JSON本身
		JsonObjMap.erase(id);
		JsonIsRootMap.erase(id);

		finish;
	}
	simplecatch("JsonDestroy", 0.0)
}

expReal JsonGetDSTypeList(GMReal list, GMReal pos)
{
	try
	{
		TreeNode* node = JsonNodeListMap.at((int)list);
		return node->typeList.at((int)pos);
	}
	simplecatch("JsonGetDSTypeList", -5)
}

expReal JsonGetDSTypeMap(GMReal map, GMString key)
{
	try
	{
		TreeNode* node = JsonNodeMapMap.at((int)map);
		return node->typeMap.at(key);
	}
	simplecatch("JsonGetDSTypeMap", -5)
}

expReal JsonGetType(GMReal objID)
{
	try
	{
		int id = static_cast<int>(objID);
		auto jsonOpt = GetJsonRef(id);
		if (!jsonOpt.has_value())
			throw std::runtime_error("无效的 JSON 对象 ID：" + to_string(id));

		Json& json = jsonOpt->get();
		if (json.is_number()) return -1;
		else if (json.is_string()) return -2;
		else if (json.is_boolean()) return -3;
		else if (json.is_null()) return -4;
		else if (json.is_object()) return ds_type_map;
		else if (json.is_array()) return ds_type_list;
		else return -5;
	}
	simplecatch("JsonGetType", -5)
}

expReal JsonQuery(GMReal objID, GMString pointerStr)
{
	try
	{
		JsonQueryResultType = -5;
		int id = static_cast<int>(objID);

		// 获取原JSON的引用（可以是根ID或子引用ID）
		auto jsonOpt = GetJsonRef(id);
		if (!jsonOpt.has_value())
		{
			JsonQueryResultType = -5;
			throw std::runtime_error("无效的 JSON 对象 ID：" + to_string(id));
		}
		Json& json = jsonOpt->get();

		// 解析查询路径
		Json::json_pointer ptr(pointerStr);
		Json& target = json.at(ptr);

		if (target.is_number())
		{
			JsonQueryResult = target.get<GMReal>();
			JsonQueryResultType = -1;
		}
		else if (target.is_string())
		{
			JsonQueryResult = target.get<std::string>();
			JsonQueryResultType = -2;
		}
		else if (target.is_boolean())
		{
			JsonQueryResult = static_cast<GMReal>(target.get<bool>());
			JsonQueryResultType = -3;
		}
		else if (target.is_null())
		{
			JsonQueryResult = static_cast<GMReal>(gm::noone);
			JsonQueryResultType = -4;
		}
		else if (target.is_object() || target.is_array())
		{
			// 找到当前ID对应的根ID
			int rootID = id;
			if (!JsonIsRootMap[id])
			{
				if (!JsonChildToRootMap.contains(id))
					throw runtime_error("无效的子引用 ID：" + to_string(id));
				rootID = JsonChildToRootMap[id];
			}

			// 构建完整的JSON Pointer路径（从根开始）
			string fullPointerStr = pointerStr;
			if (!JsonIsRootMap[id])
			{
				// 如果当前是子引用，拼接父路径
				if (!JsonRefMap.contains(id))
					throw runtime_error("无效的子引用 ID：" + to_string(id));
				fullPointerStr = JsonRefMap[id].second + pointerStr;
			}

			int childID = -1;
			// 检查缓存中是否已存在该路径
			if (JsonPathCacheMap.contains(rootID) && JsonPathCacheMap[rootID].contains(fullPointerStr))
			{
				// 缓存命中：直接返回缓存的ID
				childID = JsonPathCacheMap[rootID][fullPointerStr];
				// 额外检查：确保缓存的ID仍然有效
				if (!JsonRefMap.contains(childID))
				{
					// 缓存失效：移除失效缓存，继续生成新ID
					JsonPathCacheMap[rootID].erase(fullPointerStr);
					childID = -1;
				}
			}

			// 缓存未命中或失效：生成新的子引用ID
			if (childID == -1)
			{
				childID = JsonObjIDCounter++;
				// 存储子引用信息（根ID + 完整路径）
				JsonRefMap[childID] = { rootID, fullPointerStr };
				JsonIsRootMap[childID] = false;
				// 记录父子关系
				JsonRootToChildrenMap[rootID].push_back(childID);
				JsonChildToRootMap[childID] = rootID;
				// 加入路径缓存
				JsonPathCacheMap[rootID][fullPointerStr] = childID;
			}

			// 结果存储子引用ID，保持返回值兼容
			JsonQueryResult = static_cast<GMReal>(childID);
			JsonQueryResultType = target.is_object() ? (int)ds_type_map : (int)ds_type_list;
		}
		else
			JsonQueryResultType = -5;

		return JsonQueryResultType;
	}
	catch (...)
	{
		return -5;
	}
}

expReal JsonQueryGetReal()
{
	if (JsonQueryResultType == -5) return gm::noone;
	// Real/Bool/DS ID 均为Real类型
	if (JsonQueryResultType == -1 || JsonQueryResultType == -3 || JsonQueryResultType >= 0)
		return std::get<GMReal>(JsonQueryResult);

	return gm::noone;
}

expString JsonQueryGetString()
{
	if (JsonQueryResultType == -2)
	{
		GMReturnString = std::get<std::string>(JsonQueryResult);
		return GMReturnString.c_str();
	}
	return "";
}

expReal JsonToBuffer(GMReal objID, GMReal bufferID)
{
	try
	{
		int id = static_cast<int>(objID);
		auto jsonOpt = GetJsonRef(id);
		if (!jsonOpt.has_value())
			throw std::runtime_error("无效的 JSON 对象 ID：" + to_string(id));
		if (!gm::buffer_exists(bufferID))
			throw std::runtime_error("无效的 Buffer ID：" + to_string(bufferID));

		const Json& json = jsonOpt->get();
		vector<uint8_t> binData = Json::to_msgpack(json); // 序列化为MessagePack

		// 写入Buffer
		gm::buffer_set_size(bufferID, binData.size());
		gm::buffer_set_pos(bufferID, 0);
		char* dest = (char*)(int)gm::buffer_get_address(bufferID, false);
		if (dest == nullptr)
			throw std::runtime_error("Buffer 地址无效。");
		memcpy(dest, binData.data(), binData.size());

		finish;
	}
	simplecatch("JsonToBuffer", 0.0)
}

// 从Buffer读取二进制解析为原始JSON对象
expReal BufferToJson(GMReal bufferID)
{
	try
	{
		if (!gm::buffer_exists(bufferID))
			throw std::runtime_error("无效的 Buffer ID：" + to_string(bufferID));

		GMReal bufferSize = gm::buffer_get_size(bufferID);
		if (bufferSize <= 0)
			throw std::runtime_error("Buffer 为空。");

		gm::buffer_set_pos(bufferID, 0);
		char* src = (char*)(int)gm::buffer_get_address(bufferID, false);
		if (src == nullptr)
			throw std::runtime_error("Buffer 地址无效。");

		vector<uint8_t> binData(reinterpret_cast<uint8_t*>(src),
			reinterpret_cast<uint8_t*>(src) + static_cast<size_t>(bufferSize));
		Json json = Json::from_msgpack(binData);

		int rawID = JsonObjIDCounter++;
		JsonObjMap[rawID] = json;
		JsonIsRootMap[rawID] = true;
		return rawID;
	}
	simplecatch("BufferToJson", gm::noone)
}

expReal JsonFlatten(GMReal objID)
{
	try
	{
		int id = static_cast<int>(objID);
		auto jsonOpt = GetJsonRef(id);
		if (!jsonOpt.has_value())
			throw std::runtime_error("无效的 JSON 对象 ID：" + to_string(id));

		Json& json = jsonOpt->get();
		Json flatJson = json.flatten();

		id = JsonObjIDCounter++;
		JsonObjMap[id] = move(flatJson);
		JsonIsRootMap[id] = true;
		return id;
	}
	simplecatch("JsonFlatten", gm::noone)
}

expReal JsonUnflatten(GMReal objID)
{
	try
	{
		int id = static_cast<int>(objID);
		auto jsonOpt = GetJsonRef(id);
		if (!jsonOpt.has_value())
			throw std::runtime_error("无效的 JSON 对象 ID：" + to_string(id));

		Json& json = jsonOpt->get();
		Json flatJson = json.unflatten();

		id = JsonObjIDCounter++;
		JsonObjMap[id] = move(flatJson);
		JsonIsRootMap[id] = true;
		return id;
	}
	simplecatch("JsonUnflatten", gm::noone)
}

expReal JsonFromDSMap(GMReal mapID)
{
	try
	{
		int map = static_cast<int>(mapID);
		Json json = Json::object();

		gm::CGMVariable key = gm::ds_map_find_first(map);
		for (int i = 0; i < gm::ds_map_size(map); ++i)
		{
			if (key.IsString())
			{
				gm::CGMVariable value = gm::ds_map_find_value(map, key);
				if (value.IsString())
					json[key.c_str()] = value.c_str();
				else
					json[key.c_str()] = value.real();
			}

			key = gm::ds_map_find_next(map, key);
		}

		int id = JsonObjIDCounter++;
		JsonObjMap[id] = move(json);
		JsonIsRootMap[id] = true;
		return id;
	}
	simplecatch("JsonFromDSMap", gm::noone)
}

expReal JsonFromDSList(GMReal listID)
{
	try
	{
		int list = static_cast<int>(listID);
		Json json = Json::array();

		for (int i = 0; i < gm::ds_list_size(list); ++i)
		{
			gm::CGMVariable value = gm::ds_list_find_value(list, i);
			if (value.IsString())
				json.push_back(value.c_str());
			else
				json.push_back(value.real());
		}

		int id = JsonObjIDCounter++;
		JsonObjMap[id] = move(json);
		JsonIsRootMap[id] = true;
		return id;
	}
	simplecatch("JsonFromDSList", gm::noone)
}

expReal JsonDiff(GMReal objID1, GMReal objID2)
{
	try
	{
		int id1 = static_cast<int>(objID1);
		int id2 = static_cast<int>(objID2);
		auto jsonOpt1 = GetJsonRef(id1);
		auto jsonOpt2 = GetJsonRef(id2);

		if (!jsonOpt1.has_value())
			throw runtime_error("无效的 JSON 对象1 ID：" + to_string(id1));
		if (!jsonOpt2.has_value())
			throw runtime_error("无效的 JSON 对象2 ID：" + to_string(id2));

		const Json& json1 = jsonOpt1->get();
		const Json& json2 = jsonOpt2->get();
		Json patch = Json::diff(json1, json2);

		int patchID = JsonObjIDCounter++;
		JsonObjMap[patchID] = move(patch);
		JsonIsRootMap[patchID] = true;
		return patchID;
	}
	simplecatch("JsonDiff", gm::noone)
}

expReal JsonPatch(GMReal objID, GMReal patchObjID)
{
	try
	{
		int id = static_cast<int>(objID);
		int patchID = static_cast<int>(patchObjID);
		auto jsonOpt = GetJsonRef(id);
		auto patchOpt = GetJsonRef(patchID);

		if (!jsonOpt.has_value())
			throw runtime_error("无效的目标 JSON 对象 ID：" + to_string(id));
		if (!patchOpt.has_value())
			throw runtime_error("无效的 Patch JSON 对象 ID：" + to_string(patchID));

		Json& json = jsonOpt->get();
		const Json& patch = patchOpt->get();
		json.patch_inplace(patch);

		finish;
	}
	simplecatch("JsonPatch", 0.0)
}

expReal JsonMergePatch(GMReal objID, GMReal patchObjID)
{
	try
	{
		int id = static_cast<int>(objID);
		int patchID = static_cast<int>(patchObjID);
		auto jsonOpt = GetJsonRef(id);
		auto patchOpt = GetJsonRef(patchID);

		if (!jsonOpt.has_value())
			throw runtime_error("无效的目标 JSON 对象 ID：" + to_string(id));
		if (!patchOpt.has_value())
			throw runtime_error("无效的 Patch JSON 对象 ID：" + to_string(patchID));

		Json& json = jsonOpt->get();
		const Json& patch = patchOpt->get();
		json.merge_patch(patch);

		finish;
	}
	simplecatch("JsonMergePatch", 0.0)
}

expString JsonToString(GMReal objID, GMReal indent, GMReal indentChar, GMReal ensureASCII)
{
	try
	{
		int id = static_cast<int>(objID);
		auto jsonOpt = GetJsonRef(id);
		if (!jsonOpt.has_value())
			throw runtime_error("无效的 JSON 对象 ID：" + to_string(id));

		GMReturnString = jsonOpt->get().dump((int)indent, (char)indentChar, (bool)ensureASCII);
		return GMReturnString.c_str();
	}
	simplecatch("JsonToString", "")
}

expReal JsonSize(GMReal objID)
{
	try
	{
		int id = static_cast<int>(objID);
		auto jsonOpt = GetJsonRef(id);
		if (!jsonOpt.has_value())
			throw runtime_error("无效的 JSON 对象 ID：" + to_string(id));

		return jsonOpt->get().size();
	}
	simplecatch("JsonSize", 0)
}

expString JsonFindFirstKey(GMReal objID)
{
	try
	{
		int id = static_cast<int>(objID);
		auto jsonOpt = GetJsonRef(id);
		if (!jsonOpt.has_value())
			throw runtime_error("无效的 JSON 对象 ID：" + to_string(id));
		Json& target = jsonOpt->get();

		if (!target.is_object() || target.empty())
			return "";

		GMReturnString = target.begin().key();
		return GMReturnString.c_str();
	}
	simplecatch("JsonFindFirstKey", "")
}

expString JsonFindNextKey(GMReal objID, GMString key)
{
	try
	{
		int id = static_cast<int>(objID);
		auto jsonOpt = GetJsonRef(id);
		if (!jsonOpt.has_value())
			throw runtime_error("无效的 JSON 对象 ID：" + to_string(id));
		Json& target = jsonOpt->get();

		if (!target.is_object() || target.empty())
			return "";

		auto it = target.find(key);
		if (it == target.end() || ++it == target.end())
			return "";

		GMReturnString = it.key();
		return GMReturnString.c_str();
	}
	simplecatch("JsonFindNextKey", "")
}

expString JsonFindPrevKey(GMReal objID, GMString key)
{
	try
	{
		int id = static_cast<int>(objID);
		auto jsonOpt = GetJsonRef(id);
		if (!jsonOpt.has_value())
			throw runtime_error("无效的 JSON 对象 ID：" + to_string(id));
		Json& target = jsonOpt->get();

		if (!target.is_object() || target.empty())
			return "";

		auto it = target.find(key);
		if (it == target.end() || it == target.begin())
			return "";

		GMReturnString = (--it).key();
		return GMReturnString.c_str();
	}
	simplecatch("JsonFindPrevKey", "")
}

expString JsonFindLastKey(GMReal objID)
{
	try
	{
		int id = static_cast<int>(objID);
		auto jsonOpt = GetJsonRef(id);
		if (!jsonOpt.has_value())
			throw runtime_error("无效的 JSON 对象 ID：" + to_string(id));
		Json& target = jsonOpt->get();

		if (!target.is_object() || target.empty())
			return "";

		GMReturnString = (--target.end()).key();
		return GMReturnString.c_str();
	}
	simplecatch("JsonFindLastKey", "")
}

expReal JsonSetString(GMReal objID, GMString path, GMString value)
{
	try
	{
		int id = static_cast<int>(objID);
		auto jsonOpt = GetJsonRef(id);
		if (!jsonOpt.has_value())
			throw runtime_error("无效的 JSON 对象 ID：" + to_string(id));
		Json& json = jsonOpt->get();

		Json::json_pointer ptr(path);
		json[ptr] = value;
		finish;
	}
	simplecatch("JsonSetString", 0.0)
}

expReal JsonSetReal(GMReal objID, GMString path, GMReal value)
{
	try
	{
		int id = static_cast<int>(objID);
		auto jsonOpt = GetJsonRef(id);
		if (!jsonOpt.has_value())
			throw runtime_error("无效的 JSON 对象 ID：" + to_string(id));
		Json& json = jsonOpt->get();

		Json::json_pointer ptr(path);
		json[ptr] = value;
		finish;
	}
	simplecatch("JsonSetReal", 0.0)
}