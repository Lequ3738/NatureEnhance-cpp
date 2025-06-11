#include "Main.h"
#include "json.hpp"
#include <stack>
#include <map>
#include <vector>
#include <variant>

using Json = nlohmann::json;
using namespace std;

class Tree
{
public:
	bool ismap;
	Tree* parent;
	vector<Tree*> children;

	Tree(bool is_map) : parent(nullptr), ismap(is_map) {}
	Tree(Tree* p, bool is_map) : parent(p), ismap(is_map)
	{
		parent->children.push_back(this);
	}

	~Tree()
	{
		for (Tree* child : children)
			delete child;
	}

	void Delete()
	{
		for (Tree* child : children)
			child->parent = nullptr;

		children.clear();
		delete this;
	}
};

typedef variant<string, GMReal> dynamic;
#define getvalue(v) \
	holds_alternative<string>(v) ? get<string>(v) : get<GMReal>(v)

struct StackItem
{
	Json* current;
	dynamic key;
	int parent;
	Tree* tree;
};

// 用于控制销毁的 Tree 与 存放数据的 ds_map 之间的映射表
map<Tree*, int>* JsonDeleteMap;
// 用于控制销毁的 Tree 与 存放数据的 ds_list 之间的映射表
map<Tree*, int>* JsonDeleteList;

fnReal JsonInit()
{
	JsonDeleteMap = new map<Tree*, int>();
	JsonDeleteList = new map<Tree*, int>();

	finish;
}

fnReal JsonFree()
{
	// 销毁 JsonDeleteMap 中的所有 ds_map 引用
	for (auto it = JsonDeleteMap->begin(); it != JsonDeleteMap->end(); )
	{
		if (it->second >= 0)
		{
			gm::ds_map_destroy(it->second);
			it = JsonDeleteMap->erase(it);
		}
		else
			++it;
	}

	// 销毁 JsonDeleteList 中的所有 ds_list 引用
	for (auto it = JsonDeleteList->begin(); it != JsonDeleteList->end(); )
	{
		if (it->second >= 0)
		{
			gm::ds_list_destroy(it->second);
			it = JsonDeleteList->erase(it);
		}
		else
			++it;
	}

	delete JsonDeleteMap;
	delete JsonDeleteList;

	finish;
}

#define AddToParent(value) \
	if (holds_alternative<string>(item.key)) \
		gm::ds_map_add(item.parent, get<string>(item.key), value); \
	else \
		gm::ds_list_add(item.parent, value)

static Tree* GetTreeNode(GMReal id)
{
	auto it = std::find_if(JsonDeleteMap->begin(), JsonDeleteMap->end(),
		[&](const auto& pair) { return pair.second == static_cast<int>(id); });

	if (it == JsonDeleteMap->end())
	{
		it = std::find_if(JsonDeleteList->begin(), JsonDeleteList->end(),
			[&](const auto& pair) { return pair.second == static_cast<int>(id); });

		if (it == JsonDeleteList->end())
			return nullptr;
	}

	return it->first;
}

fnReal JsonDecode(GMString jsonstr)
{
	try
	{
		if (JsonDeleteMap == nullptr || JsonDeleteList == nullptr)
			throw L"JsonDeleteMap 未初始化，请先调用 JsonInit 函数。";
		
		// Json 支持注释
		Json json = Json::parse(jsonstr, nullptr, true, true);
		int root = gm::ds_map_create();
		
		Tree* tree = new Tree(true);  // 删除时用到的节点树
		
		stack<StackItem> jsonStack;
		jsonStack.push(StackItem{ &json, "", root, tree });
		
		(*JsonDeleteMap)[tree] = root;

		while (!jsonStack.empty())
		{
			StackItem item = jsonStack.top();  // 程序运行到这里就崩溃了
			jsonStack.pop();
			
			Json* curr = item.current;
			Tree* curTree = item.tree;
			
			if (curr->is_object())
			{
				int tempMap = gm::ds_map_create();
				Tree* delTree = new Tree(curTree, true);
				(*JsonDeleteMap)[delTree] = tempMap;

				for (auto i = curr->rbegin(); i != curr->rend(); ++i)
					jsonStack.push(StackItem{ &i.value(), i.key(), tempMap, delTree });

				AddToParent(tempMap);
			}
			else if (curr->is_array())
			{
				int tempList = gm::ds_list_create();
				Tree* delTree = new Tree(curTree, false);
				(*JsonDeleteList)[delTree] = tempList;

				int num = 0;
				for (auto i = curr->rbegin(); i != curr->rend(); ++i, ++num)
					jsonStack.push(StackItem{ &i.value(), (GMReal)num, tempList, delTree });

				AddToParent(tempList);
			}
			else if (curr->is_number())
			{
				AddToParent(curr->get<GMReal>());
			}
			else if (curr->is_string())
			{
				AddToParent(curr->get<std::string>());
			}
			else if (curr->is_boolean())
			{
				AddToParent(static_cast<GMReal>(curr->get<bool>()));
			}
			else if (curr->is_null())
			{
				AddToParent(gm::noone);
			}
			else
				throw L"不支持的 JSON 数据类型。";
		}

		// 解析完成后，获得的 ds_map 的树结构是这样的：
		//
		// root -> dataroot -> ...
		//
		// 所以要删除最顶层的 root 节点，获得从 json 根部开始的 ds_map 数据。
		
		int result = static_cast<int>(gm::ds_map_find_value(root, ""));
		gm::ds_map_destroy(root);
		
		tree->Delete();  // 删除最顶层的树节点
		
		return result;
	}
	catch (const Json::parse_error& e)
	{
		if (show_error)
		{
			string errorMsg = "函数 JsonDecode 出现解析错误：\n" + string(e.what());
			wstring werror(errorMsg.begin(), errorMsg.end());

			MessageBox(NULL, werror.c_str(), L"NatureEnhance Error",
				MB_OK | MB_ICONERROR);
		}

		reterror;
	}
	catch (const wchar_t* e)
	{
		if (show_error)
		{
			wstring errorMsg = L"函数 JsonDecode 出现运行错误：\n" + wstring(e);

			MessageBox(NULL, errorMsg.c_str(), L"NatureEnhance Error",
				MB_OK | MB_ICONERROR);
		}

		reterror;
	}
	catch (...)
	{
		if (show_error)
		{
			MessageBox(NULL, L"函数 JsonDecode 出现未知错误。", L"NatureEnhance Error",
				MB_OK | MB_ICONERROR);
		}

		reterror;
	}
}



fnReal JsonDestroy(GMReal rootNode)
{
	try
	{
		Tree* treeRoot = GetTreeNode(rootNode);
		
		if (treeRoot == nullptr)
			throw L"传入的 ds_map 引用无效。";
		else if (treeRoot->parent != nullptr)
			throw L"传入的 ds_map 引用不是由 JsonDecode() 函数生成的根引用。";

		stack<Tree*> treeStack;
		treeStack.push(treeRoot);

		while (!treeStack.empty())
		{
			Tree* curTree = treeStack.top();
			treeStack.pop();

			for (Tree* child : curTree->children)
				treeStack.push(child);

			if (curTree->ismap)
			{
				gm::ds_map_destroy((*JsonDeleteMap)[curTree]);
				(*JsonDeleteMap)[curTree] = gm::noone;  // 清除映射表中的引用
			}
			else
			{
				gm::ds_list_destroy((*JsonDeleteList)[curTree]);
				(*JsonDeleteList)[curTree] = gm::noone;  // 清除映射表中的引用
			}
		}

		// 清理 JsonDeleteMap 中的无效引用
		for (auto it = JsonDeleteMap->begin(); it != JsonDeleteMap->end(); )
		{
			if (it->second == gm::noone)
				it = JsonDeleteMap->erase(it);
			else
				++it;
		}

		// 清理 JsonDeleteList 中的无效引用
		for (auto it = JsonDeleteList->begin(); it != JsonDeleteList->end(); )
		{
			if (it->second == gm::noone)
				it = JsonDeleteList->erase(it);
			else
				++it;
		}

		delete treeRoot;
		finish;
	}
	catch (const wchar_t* e)
	{
		if (show_error)
		{
			wstring errorMsg = L"函数 JsonDestroy 出现运行错误：\n" + wstring(e);

			MessageBox(NULL, errorMsg.c_str(), L"NatureEnhance Error",
				MB_OK | MB_ICONERROR);
		}

		fail;
	}
	catch (...)
	{
		if (show_error)
		{
			MessageBox(NULL, L"函数 JsonDestroy 出现未知错误。", L"NatureEnhance Error",
				MB_OK | MB_ICONERROR);
		}

		fail;
	}
}

#define ds_type_map 0
#define ds_type_list 1

fnReal JsonGetDsType(GMReal rootNode, GMReal list)
{
	try
	{
		int node = static_cast<int>(rootNode);
		int id = static_cast<int>(list);
		int size = gm::ds_list_size(id);
		for (int i = 0; i < size; ++i)
		{
			dynamic path = gm::ds_list_find_value(id, i);
			if (holds_alternative<string>(path))
			{
				dynamic value = gm::ds_map_find_value(node, get<string>(path));
				if (holds_alternative<string>(value))
					return gm::noone;
				else if (fmod(get<GMReal>(value), 1) != 0)
					return gm::noone;

				id = static_cast<int>(get<GMReal>(value));
			}
			else
			{
				dynamic value = gm::ds_list_find_value(node, static_cast<int>(get<GMReal>(path)));
				if (holds_alternative<string>(value))
					return gm::noone;
				else if (fmod(get<GMReal>(value), 1) != 0)
					return gm::noone;

				id = static_cast<int>(get<GMReal>(value));
			}
		}

		Tree* treeNode = GetTreeNode(node);
		if (treeNode == nullptr)
			return gm::noone;

		return treeNode->ismap ? ds_type_map : ds_type_list;
	}
	catch (const wchar_t* e)
	{
		if (show_error)
		{
			wstring errorMsg = L"函数 JsonGetDsType 出现运行错误：\n" + wstring(e);

			MessageBox(NULL, errorMsg.c_str(), L"NatureEnhance Error",
				MB_OK | MB_ICONERROR);
		}

		return -1;
	}
	catch (...)
	{
		if (show_error)
		{
			MessageBox(NULL, L"函数 JsonGetDsType 出现未知错误。", L"NatureEnhance Error",
				MB_OK | MB_ICONERROR);
		}

		return -1;
	}
}