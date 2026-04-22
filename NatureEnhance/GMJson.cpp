#include "Main.h"
#include "json.hpp"
#include <stack>
#include <vector>
#include <buffer.h>

using Json = nlohmann::json;
using namespace std;

// --------------------- 全局容器 ---------------------
static int JsonObjIDCounter = 10000000;
unordered_map<int, Json> JsonObjMap;
unordered_map<int, int> JsonObjToDSMap;  // JSON ID -> 解析后的DS根ID映射

dynamic JsonQueryResult;  // JSON Pointer 查询全局结果
int JsonQueryResultType = -5;

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
	JsonObjToDSMap.clear();

	finish;
}

expReal StringToJson(GMString jsonstr)
{
	try
	{
		Json json = Json::parse(jsonstr, nullptr, true, true);
		int objID = JsonObjIDCounter++;
		JsonObjMap[objID] = json;
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
		if (!JsonObjMap.contains(id))
			throw std::runtime_error("无效的 JSON 对象 ID。");

		Json& json = JsonObjMap[id];
		int result = ConvertJsonToDS(&json);
		JsonObjToDSMap[id] = result; // 关联 json 对象 ID 与 DS ID
		
		return result;
	}
	simplecatch("JsonParse", gm::noone)
}

expReal JsonGetDS(GMReal objID)
{
	int id = static_cast<int>(objID);
	if (JsonObjToDSMap.contains(id))
		return JsonObjToDSMap[id];

	return gm::noone;
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
		int rawIDInt = static_cast<int>(objID);

		// 级联删除关联的 DS 结构
		if (JsonObjToDSMap.contains(rawIDInt))
		{
			int dsRootID = JsonObjToDSMap[rawIDInt];
			if (JsonDataMap.contains(dsRootID))
				JsonDSClear(dsRootID);

			JsonObjToDSMap.erase(rawIDInt);
		}

		// 删除 JSON 对象
		if (!JsonObjMap.contains(rawIDInt))
			throw std::runtime_error("无效的 JSON 对象 ID。");
		JsonObjMap.erase(rawIDInt);

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
	simplecatch("JsonGetTypeList", -5)
}

expReal JsonGetDSTypeMap(GMReal map, GMString key)
{
	try
	{
		TreeNode* node = JsonNodeMapMap.at((int)map);
		return node->typeMap.at(key);
	}
	simplecatch("JsonGetTypeMap", -5)
}

expReal JsonGetType(GMReal objID)
{
	try
	{
		int id = static_cast<int>(objID);
		if (!JsonObjMap.contains(id))
			throw std::runtime_error("无效的 JSON 对象 ID。");

		Json& json = JsonObjMap[id];
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
		if (!JsonObjMap.contains(id))
		{
			JsonQueryResultType = -5;
			throw std::runtime_error("无效的 JSON 对象 ID。");
		}

		Json& json = JsonObjMap[id];
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
			int dsID = ConvertJsonToDS(&target);
			JsonQueryResult = static_cast<GMReal>(dsID);
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
		if (!JsonObjMap.contains(id))
			throw std::runtime_error("无效的 JSON 对象 ID。");
		if (!gm::buffer_exists(bufferID))
			throw std::runtime_error("无效的 Buffer ID。");

		const Json& json = JsonObjMap[id];
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
			throw std::runtime_error("无效的 Buffer ID。");

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
		return rawID;
	}
	simplecatch("BufferToJson", gm::noone)
}

expReal JsonFlatten(GMReal objID)
{
	try
	{
		int id = static_cast<int>(objID);
		if (!JsonObjMap.contains(id))
			throw std::runtime_error("无效的 JSON 对象 ID。");

		Json& json = JsonObjMap[id];
		Json flatJson = json.flatten();

		id = JsonObjIDCounter++;
		JsonObjMap[id] = move(flatJson);
		return id;
	}
	simplecatch("JsonFlatten", gm::noone)
}

expReal JsonUnflatten(GMReal objID)
{
	try
	{
		int id = static_cast<int>(objID);
		if (!JsonObjMap.contains(id))
			throw std::runtime_error("无效的 JSON 对象 ID。");

		Json& json = JsonObjMap[id];
		Json flatJson = json.unflatten();

		id = JsonObjIDCounter++;
		JsonObjMap[id] = move(flatJson);
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

		if (!JsonObjMap.contains(id1))
			throw runtime_error("无效的 JSON 对象1 ID：" + to_string(id1));
		if (!JsonObjMap.contains(id2))
			throw runtime_error("无效的 JSON 对象2 ID：" + to_string(id2));

		const Json& json1 = JsonObjMap[id1];
		const Json& json2 = JsonObjMap[id2];
		Json patch = Json::diff(json1, json2);

		int patchID = JsonObjIDCounter++;
		JsonObjMap[patchID] = move(patch);
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

		if (!JsonObjMap.contains(id))
			throw runtime_error("无效的目标 JSON 对象 ID：" + to_string(id));
		if (!JsonObjMap.contains(patchID))
			throw runtime_error("无效的 Patch JSON 对象 ID：" + to_string(patchID));

		Json& json = JsonObjMap[id];
		const Json& patch = JsonObjMap[patchID];
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

		if (!JsonObjMap.contains(id))
			throw runtime_error("无效的目标 JSON 对象 ID：" + to_string(id));
		if (!JsonObjMap.contains(patchID))
			throw runtime_error("无效的 Patch JSON 对象 ID：" + to_string(patchID));

		Json& json = JsonObjMap[id];
		const Json& patch = JsonObjMap[patchID];
		json.merge_patch(patch);

		finish;
	}
	simplecatch("JsonPatch", 0.0)
}

expString JsonToString(GMReal objID, GMReal indent, GMReal indentChar, GMReal ensureASCII)
{
	try
	{
		int id = static_cast<int>(objID);
		if (!JsonObjMap.contains(id))
			throw runtime_error("无效的 JSON 对象 ID：" + to_string(id));

		GMReturnString = JsonObjMap[id].dump((int)indent, (char)indentChar, (bool)ensureASCII);
		return GMReturnString.c_str();
	}
	simplecatch("JsonToString", "")
}