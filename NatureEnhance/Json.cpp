#include "Main.h"
#include "json.hpp"
#include <stack>
#include <map>
#include <vector>

using Json = nlohmann::json;

struct Tree
{
	bool ismap;
	Tree* parent;
	std::vector<std::unique_ptr<Tree>> children;

	int gmid;
	dynamic key;
	Json* current;

	explicit Tree(bool is_map) : ismap(is_map) {}
	std::unique_ptr<Tree> AddChild(bool is_map)
	{
		auto child = std::make_unique<Tree>(Tree(is_map));
		child->parent = this;
		children.push_back(child);
		return child;
	}
};