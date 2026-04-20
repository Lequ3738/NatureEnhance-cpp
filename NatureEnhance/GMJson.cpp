#include "Main.h"
#include "json.hpp"
#include <stack>
#include <vector>

using Json = nlohmann::json;
using namespace std;

// --------------------- 全局容器 ---------------------
static int JsonRawIDCounter = 10000000;
unordered_map<int, Json> JsonRawMap;
unordered_map<int, int> JsonRawToDSMap;  // 原始JSON ID -> 解析后的DS根ID映射

dynamic JsonQueryResult;  // JSON Pointer 查询全局结果
int JsonQueryResultType = -5;

// --------------------- TreeNode 容器 ---------------------
struct TreeNode;
unordered_map<int, TreeNode*> JsonDataMap;  // 存放所有由 JsonDecode 创建的树结构根节点
// 存放所有由 JsonDecode 创建的树结构的所有节点
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

bool contains(const vector<int>& vec, int target)
{
	return find(vec.begin(), vec.end(), target) != vec.end();
}

expReal JsonFree()
{
	for (auto& tree : JsonDataMap)
		delete tree.second;

	JsonRawMap.clear();
	JsonRawToDSMap.clear();

	finish;
}

void AddToParent(const StackItem& item, const gm::CGMVariable& value, char type)
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

expReal JsonParse(GMString jsonstr)
{
	try
	{
		Json json = Json::parse(jsonstr, nullptr, true, true);
		int rawID = JsonRawIDCounter++;
		JsonRawMap[rawID] = json;
		return rawID;
	}
	catch (const std::exception& e)
	{
		if (show_error)
		{
			ShowMessage("在执行函数 JsonParse 出现错误：\n" + std::string(jsonstr) + "\n" + e.what(),
				"NatureEnhance Error", MB_OK | MB_ICONERROR);
		}
		return gm::noone;
	}
}

expReal JsonToDS(GMReal rawID)
{
	try
	{
		int rawIDInt = static_cast<int>(rawID);
		if (!JsonRawMap.contains(rawIDInt))
			throw std::runtime_error("无效的原始JSON对象ID");

		Json& json = JsonRawMap[rawIDInt];
		
		TreeNode* treeRoot = new TreeNode(true);  // 存放数据结构的节点树
		
		stack<StackItem> jsonStack;
		jsonStack.push({ .json = &json, .key = "", .tree = treeRoot });

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

		int result = static_cast<int>(gm::ds_map_find_value(treeRoot->data, ""));
		JsonDataMap[result] = treeRoot->children.at(0);
		treeRoot->RemoveNode();  // 删除最顶层的树节点

		JsonRawToDSMap[rawIDInt] = result; // 关联原始ID与DS ID
		
		return result;
	}
	simplecatch("JsonToDS", gm::noone)
}

expReal JsonDecode(GMString jsonstr)
{
	GMReal rawID = JsonParse(jsonstr);
	return (rawID == gm::noone) ? gm::noone : JsonToDS(rawID);
}

expReal JsonDestroy(GMReal rootNode)
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
	simplecatch("JsonDestroy", false)
}

expReal JsonDestroyRaw(GMReal rawID)
{
	try
	{
		int rawIDInt = static_cast<int>(rawID);
		// 级联删除关联的DS结构
		if (JsonRawToDSMap.contains(rawIDInt))
		{
			int dsRootID = JsonRawToDSMap[rawIDInt];
			JsonDestroy(dsRootID);
			JsonRawToDSMap.erase(rawIDInt);
		}
		// 删除原始JSON对象
		if (!JsonRawMap.contains(rawIDInt))
			throw std::runtime_error("无效的原始JSON对象ID");
		JsonRawMap.erase(rawIDInt);
		finish;
	}
	simplecatch("JsonDestroyRaw", 0.0)
}

expReal JsonGetTypeList(GMReal list, GMReal pos)
{
	try
	{
		TreeNode* node = JsonNodeListMap.at((int)list);
		return node->typeList.at((int)pos);
	}
	simplecatch("JsonGetTypeList", -5)
}

expReal JsonGetTypeMap(GMReal map, GMString key)
{
	try
	{
		TreeNode* node = JsonNodeMapMap.at((int)map);
		return node->typeMap.at(key);
	}
	simplecatch("JsonGetTypeMap", -5)
}

expReal JsonQuery(GMReal rawID, GMString pointerStr)
{
	try
	{
		int rawIDInt = static_cast<int>(rawID);
		if (!JsonRawMap.contains(rawIDInt))
		{
			JsonQueryResultType = -5;
			throw std::runtime_error("无效的原始JSON对象ID");
		}

		Json& json = JsonRawMap[rawIDInt];
		nlohmann::json_pointer ptr(pointerStr);
		Json& target = json.at(ptr); // 路径不存在则抛异常

		JsonQueryResultType = -5;
		// 按类型处理结果
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
		else if (target.is_object()) // 解析为ds_map
		{
			TreeNode* treeRoot = new TreeNode(true);
			stack<StackItem> jsonStack;
			jsonStack.push({ .json = &target, .key = "", .tree = treeRoot });
			// 复用DS解析逻辑
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
			}
			int dsID = static_cast<int>(gm::ds_map_find_value(treeRoot->data, ""));
			JsonDataMap[dsID] = treeRoot->children.at(0);
			treeRoot->RemoveNode();
			JsonQueryResult = static_cast<GMReal>(dsID);
			JsonQueryResultType = ds_type_map; // 0=map
		}
		else if (target.is_array()) // 解析为ds_list
		{
			TreeNode* treeRoot = new TreeNode(true);
			stack<StackItem> jsonStack;
			jsonStack.push({ .json = &target, .key = "", .tree = treeRoot });
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
			}
			int dsID = static_cast<int>(gm::ds_map_find_value(treeRoot->data, ""));
			JsonDataMap[dsID] = treeRoot->children.at(0);
			treeRoot->RemoveNode();
			JsonQueryResult = static_cast<GMReal>(dsID);
			JsonQueryResultType = ds_type_list; // 1=list
		}
		else
		{
			JsonQueryResultType = -5;
			throw std::runtime_error("不支持的JSON类型");
		}

		return JsonQueryResultType;
	}
	catch (const std::exception& e)
	{
		if (show_error)
		{
			ShowMessage("JsonQuery 错误：\n路径：" + std::string(pointerStr) + "\n" + e.what(),
				"NatureEnhance Error", MB_OK | MB_ICONERROR);
		}
		JsonQueryResultType = -5;
		return -5;
	}
}

expReal JsonToBuffer(GMReal rawID, GMReal bufferID)
{
	try
	{
		int rawIDInt = static_cast<int>(rawID);
		if (!JsonRawMap.contains(rawIDInt))
			throw std::runtime_error("无效的原始JSON对象ID");
		if (!gm::buffer_exists(bufferID))
			throw std::runtime_error("无效的Buffer ID");

		const Json& json = JsonRawMap[rawIDInt];
		vector<uint8_t> binData = Json::to_msgpack(json); // 序列化为MessagePack

		// 写入Buffer
		gm::buffer_set_size(bufferID, binData.size());
		gm::buffer_set_pos(bufferID, 0);
		char* dest = (char*)(int)gm::buffer_get_address(bufferID, false);
		if (dest == nullptr)
			throw std::runtime_error("Buffer地址无效");
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
			throw std::runtime_error("无效的Buffer ID");

		GMReal bufferSize = gm::buffer_get_size(bufferID);
		if (bufferSize <= 0)
			throw std::runtime_error("Buffer为空");

		gm::buffer_set_pos(bufferID, 0);
		char* src = (char*)(int)gm::buffer_get_address(bufferID, false);
		if (src == nullptr)
			throw std::runtime_error("Buffer地址无效");

		vector<uint8_t> binData(reinterpret_cast<uint8_t*>(src),
			reinterpret_cast<uint8_t*>(src) + static_cast<size_t>(bufferSize));
		Json json = Json::from_msgpack(binData); // 解析MessagePack

		int rawID = JsonRawIDCounter++;
		JsonRawMap[rawID] = json;
		return rawID;
	}
	simplecatch("BufferToJson", gm::noone)
}

// -------------------------- 新增功能5：拍平/还原 --------------------------
// 拍平JSON为单层ds_map（key=JSON Pointer路径）
expReal JsonFlatten(GMReal rawID)
{
	try
	{
		int rawIDInt = static_cast<int>(rawID);
		if (!JsonRawMap.contains(rawIDInt))
			throw std::runtime_error("无效的原始JSON对象ID");

		Json& json = JsonRawMap[rawIDInt];
		Json flatJson = json.flatten(); // nlohmann原生拍平

		TreeNode* treeRoot = new TreeNode(true);
		stack<StackItem> jsonStack;
		jsonStack.push({ .json = &flatJson, .key = "", .tree = treeRoot });

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
			else if (curr->is_number())
				AddToParent(item, curr->get<GMReal>(), -1);
			else if (curr->is_string())
				AddToParent(item, curr->get<std::string>(), -2);
			else if (curr->is_boolean())
				AddToParent(item, static_cast<GMReal>(curr->get<bool>()), -3);
			else if (curr->is_null())
				AddToParent(item, gm::noone, -4);
		}

		int result = static_cast<int>(gm::ds_map_find_value(treeRoot->data, ""));
		JsonDataMap[result] = treeRoot->children.at(0);
		treeRoot->RemoveNode();
		return result;
	}
	simplecatch("JsonFlatten", gm::noone)
}

// 从单层ds_map还原为嵌套JSON对象（需实现GM ds_map遍历）
expReal JsonUnflatten(GMReal dsMapID)
{
	try
	{
		int dsID = static_cast<int>(dsMapID);
		if (!JsonNodeMapMap.contains(dsID) || !JsonNodeMapMap[dsID]->ismap)
			throw std::runtime_error("无效的拍平ds_map ID");

		TreeNode* node = JsonNodeMapMap[dsID];
		Json flatJson;

		// 遍历ds_map（需绑定GM的ds_map遍历API，示例逻辑）
		// 请根据实际GM API替换以下遍历逻辑！
		GMString key = gm::ds_map_get_first_key(node->data);
		while (key != nullptr && key[0] != '\0')
		{
			dynamic val = gm::ds_map_get_value(node->data, key);
			if (holds_alternative<GMReal>(val))
				flatJson[key] = get<GMReal>(val);
			else if (holds_alternative<string>(val))
				flatJson[key] = get<string>(val);
			key = gm::ds_map_get_next_key(node->data, key);
		}

		// 还原为嵌套JSON
		Json json = flatJson.unflatten();
		int rawID = JsonRawIDCounter++;
		JsonRawMap[rawID] = json;
		return rawID;
	}
	simplecatch("JsonUnflatten", gm::noone)
}