#include "Main.h"
#include "srell.hpp"
#include <string>

// ================= 正则表达式（srell 引擎，ECMAScript 语法，UTF-8） =================
//
// 约定：
//  - 无句柄、无状态，每次调用内部编译 pattern。
//  - 结果以「新建 GM8 原生 ds_list 的 id」返回，调用方负责 ds_list_destroy()。
//  - 无匹配时集合类函数返回 -1（GM8 的 ds_list id 为 0,1,2...，-1 无歧义）。
//  - pattern 非法时按全局 show_error 决定是否弹窗，返回 0 / 原串 / -1。

expReal regex_test(GMString pattern, GMString str)
{
	try
	{
		srell::u8cregex re(pattern);
		std::string s(str);

		return srell::regex_search(s, re) ? 1.0 : 0.0;
	}
	simplecatch("regex_test", 0);
}

expReal regex_search(GMString pattern, GMString str)
{
	try
	{
		srell::u8cregex re(pattern);
		std::string s(str);
		srell::u8csmatch m;

		if (!srell::regex_search(s, m, re))
			return -1.0;

		int id = gm::ds_list_create();
		for (size_t i = 0; i < m.size(); ++i)
		{
			std::string group = m[i].matched ? std::string(m[i].first, m[i].second) : std::string();
			gm::ds_list_add(id, group);
		}

		return (GMReal)id;
	}
	simplecatch("regex_search", -1);
}

expReal regex_match_all(GMString pattern, GMString str)
{
	try
	{
		srell::u8cregex re(pattern);
		std::string s(str);

		int id = gm::ds_list_create();
		bool any = false;

		for (srell::u8csregex_iterator it(s.begin(), s.end(), re), end; it != end; ++it)
		{
			any = true;
			gm::ds_list_add(id, std::string((*it)[0].first, (*it)[0].second));
		}

		if (!any)
		{
			gm::ds_list_destroy(id);
			return -1.0;
		}

		return (GMReal)id;
	}
	simplecatch("regex_match_all", -1);
}

expString regex_replace(GMString pattern, GMString str, GMString replacement)
{
	try
	{
		srell::u8cregex re(pattern);
		std::string s(str);
		std::string fmt(replacement);

		GMReturnString = srell::regex_replace(s, re, fmt);
		return GMReturnString.c_str();
	}
	simplecatch("regex_replace", str);
}

expReal regex_split(GMString pattern, GMString str)
{
	try
	{
		srell::u8cregex re(pattern);
		std::string s(str);

		int id = gm::ds_list_create();
		size_t last = 0;

		for (srell::u8csregex_iterator it(s.begin(), s.end(), re), end; it != end; ++it)
		{
			size_t pos = (size_t)it->position(0);
			gm::ds_list_add(id, std::string(s.begin() + last, s.begin() + pos));
			last = pos + (size_t)it->length(0);
		}

		gm::ds_list_add(id, std::string(s.begin() + last, s.end()));
		return (GMReal)id;
	}
	simplecatch("regex_split", -1);
}
