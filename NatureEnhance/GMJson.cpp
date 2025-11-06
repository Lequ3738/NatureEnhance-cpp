#include "Main.h"
#include "json.hpp"
#include <stack>
#include <map>
#include <vector>

using Json = nlohmann::json;
using namespace std;

struct TreeNode
{
	bool ismap;
	TreeNode* parent;
	vector<TreeNode*> children;
	int data = gm::noone;

	TreeNode(bool is_map) : parent(nullptr), ismap(is_map)
	{
		if (is_map)
			data = gm::ds_map_create();
		else
			data = gm::ds_list_create();
	}

	~TreeNode()
	{
		if (ismap)
			gm::ds_map_destroy(data);
		else
			gm::ds_list_destroy(data);

		for (TreeNode* child : children)
			delete child;
	}

	TreeNode* AddChild(bool is_map)
	{
		TreeNode* node = new TreeNode(is_map);
		children.push_back(node);

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

unordered_map<int, TreeNode*> JsonDataMap;

bool contains(const vector<int>& vec, int target)
{
	return find(vec.begin(), vec.end(), target) != vec.end();
}

expReal JsonFree()
{
	for (auto& tree : JsonDataMap)
		delete tree.second;

	finish;
}

void AddToParent(const StackItem& item, const gm::CGMVariable& value)
{
	if (holds_alternative<string>(item.key))
		gm::ds_map_add(item.tree->data, get<string>(item.key), value);
	else
		gm::ds_list_add(item.tree->data, value);
}

expReal JsonDecode(GMString jsonstr)
{
	try
	{
		// Json 支持注释
		Json json = Json::parse(jsonstr, nullptr, true, true);
		
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

				AddToParent(item, tree->data);
			}
			else if (curr->is_array())
			{
				TreeNode* tree = item.tree->AddChild(false);

				int num = 0;
				for (auto i = curr->rbegin(); i != curr->rend(); ++i, ++num)
					jsonStack.push({ .json = &i.value(), .key = (GMReal)num, .tree = tree });

				AddToParent(item, tree->data);
			}
			else if (curr->is_number())
				AddToParent(item, curr->get<GMReal>());
			else if (curr->is_string())
				AddToParent(item, curr->get<std::string>());
			else if (curr->is_boolean())
				AddToParent(item, static_cast<GMReal>(curr->get<bool>()));
			else if (curr->is_null())
				AddToParent(item, gm::noone);
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
		
		return result;
	}
	simplecatch("JsonDecode", gm::noone)
}

expReal JsonDestroy(GMReal rootNode)
{
	try
	{
		TreeNode* treeRoot = JsonDataMap.at((int)rootNode);
		
		if (treeRoot == nullptr)
			throw runtime_error("传入的 ds_map 引用无效。");
		else if (treeRoot->parent != nullptr)
			throw runtime_error("传入的 ds_map 引用不是由 JsonDecode() 函数生成的根引用。");

		delete treeRoot;
		finish;
	}
	simplecatch("JsonDestroy", false)
}