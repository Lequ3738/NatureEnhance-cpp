#include "FoxWriting.h"
#include "xxhash.hpp"
#include <unordered_map>

HMODULE FoxWritingDLL = nullptr;

namespace fw
{
	r_s string_width, string_height;
	r_srr string_width_ext, string_height_ext;
	r_rrs draw_text, draw_text_transformed, draw_text_color, draw_text_transformed_color;

	int argument_list;
}

#define load(var, name, type) \
	var = (type)GetProcAddress(FoxWritingDLL, name); \
	if (var == nullptr) {\
		std::wstring wname(name, name + strlen(name)); \
		std::wstring err = L"加载 FoxWriting.dll 时获取函数 (" + wname + L") 失败。"; \
		throw err.c_str(); \
	}

expReal ImportFoxWritingModule(GMString name, GMReal argList)
{
	try
	{
		std::wstring wname(name, name + strlen(name));
		FoxWritingDLL = GetModuleHandle(wname.c_str());
		if (FoxWritingDLL == nullptr)
		{
			std::wstring err = L"加载 " + wname + L" 失败。";
			throw err.c_str();
		}

		fw::argument_list = (int)argList;

		load(fw::string_width, "FWStringWidth", r_s);
		load(fw::string_height, "FWStringHeight", r_s);
		load(fw::string_width_ext, "FWStringWidthEx", r_srr);
		load(fw::string_height_ext, "FWStringHeightEx", r_srr);
		load(fw::draw_text, "FWDrawText", r_rrs);
		load(fw::draw_text_transformed, "FWDrawTextTransformed", r_rrs);
		load(fw::draw_text_color, "FWDrawTextColor", r_rrs);
		load(fw::draw_text_transformed_color, "FWDrawTextTransformedColor", r_rrs);

		finish;
	}
	simplecatch(L"ImportFoxWritingModule", 0)
}

namespace fw
{
	struct Data
	{
		std::string text;
		GMReal w = 0;
	};

	using HashMap = std::unordered_map<xxh::hash64_t, Data>;
	HashMap ProcessedText;

	std::string& get_text(GMString str, GMReal w)
	{
		std::string string(str);
		xxh::hash64_t hash = xxh::xxhash3<64>(string);
		
		if (!ProcessedText.contains(hash))
			ProcessedText[hash] = { .text = string_get_ext(str, w), .w = w };
		else if (ProcessedText[hash].w != w)
			ProcessedText[hash] = { .text = string_get_ext(str, w), .w = w };
		
		return ProcessedText[hash].text;
	}

	void draw_text_ext(GMReal x, GMReal y, GMString str, GMReal w)
	{
		draw_text(x, y, get_text(str, w).c_str());
	}

	void draw_text_ext_transformed(GMReal x, GMReal y, GMString str, GMReal w,
		GMReal xscale, GMReal yscale, GMReal angle)
	{
		gm::ds_list_clear(argument_list);
		gm::ds_list_add(argument_list, xscale);
		gm::ds_list_add(argument_list, yscale);
		gm::ds_list_add(argument_list, angle);

		draw_text_transformed(x, y, get_text(str, w).c_str());
	}

	void draw_text_ext_color(GMReal x, GMReal y, GMString str, GMReal w,
		GMReal c1, GMReal c2, GMReal alpha)
	{
		gm::ds_list_clear(argument_list);
		gm::ds_list_add(argument_list, c1);
		gm::ds_list_add(argument_list, c2);
		gm::ds_list_add(argument_list, alpha);

		draw_text_color(x, y, get_text(str, w).c_str());
	}

	void draw_text_ext_transformed_color(GMReal x, GMReal y, GMString str, GMReal w,
		GMReal xscale, GMReal yscale, GMReal angle, GMReal c1, GMReal c2, GMReal alpha)
	{
		gm::ds_list_clear(argument_list);
		gm::ds_list_add(argument_list, xscale);
		gm::ds_list_add(argument_list, yscale);
		gm::ds_list_add(argument_list, angle);
		gm::ds_list_add(argument_list, c1);
		gm::ds_list_add(argument_list, c2);
		gm::ds_list_add(argument_list, alpha);

		draw_text_transformed_color(x, y, get_text(str, w).c_str());
	}

	void cleanup() { ProcessedText.clear(); }
}

expReal FWDrawTextExt(GMReal x, GMReal y, GMString str)
{
	try
	{
		GMReal w = gm::ds_list_find_value(fw::argument_list, 0);

		fw::draw_text(x, y, fw::get_text(str, w).c_str());
		finish;
	}
	simplecatch(L"FWDrawTextExt", 0)
}

expReal FWDrawTextExtTransformed(GMReal x, GMReal y, GMString str)
{
	try
	{
		GMReal w = gm::ds_list_find_value(fw::argument_list, 0);
		gm::ds_list_delete(fw::argument_list, 0);
		
		fw::draw_text_transformed(x, y, fw::get_text(str, w).c_str());
		finish;
	}
	simplecatch(L"FWDrawTextExtTransformed", 0)
}

expReal FWDrawTextExtColor(GMReal x, GMReal y, GMString str)
{
	try
	{
		GMReal w = gm::ds_list_find_value(fw::argument_list, 0);
		gm::ds_list_delete(fw::argument_list, 0);

		fw::draw_text_color(x, y, fw::get_text(str, w).c_str());
		finish;
	}
	simplecatch(L"FWDrawTextExtColor", 0)
}

expReal FWDrawTextExtTransformedColor(GMReal x, GMReal y, GMString str)
{
	try
	{
		GMReal w = gm::ds_list_find_value(fw::argument_list, 0);
		gm::ds_list_delete(fw::argument_list, 0);

		fw::draw_text_transformed_color(x, y, fw::get_text(str, w).c_str());
		finish;
	}
	simplecatch(L"FWDrawTextExtTransformedColor", 0)
}

expReal FWStringWidthExt(GMString str, GMReal sep, GMReal w)
{
	return fw::string_width_ext(fw::get_text(str, w).c_str(), sep, 0);
}

expReal FWStringHeightExt(GMString str, GMReal sep, GMReal w)
{
	return fw::string_height_ext(fw::get_text(str, w).c_str(), sep, 0);
}

expReal FWTextCleanup()
{
	fw::cleanup();
	finish;
}