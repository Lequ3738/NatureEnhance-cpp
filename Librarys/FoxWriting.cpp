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

template<typename T> void load(T& var, GMString name)
{
	var = (T)GetProcAddress(FoxWritingDLL, name);
	if (var == nullptr) {
		throw std::runtime_error("加载 Http.dll 时获取函数 ( " +
			std::string(name) + ") 失败。");
	}
}

expReal ImportFoxWritingModule(GMString name, GMReal argList)
{
	try
	{
		std::wstring wname(name, name + strlen(name));
		FoxWritingDLL = GetModuleHandle(wname.c_str());
		if (FoxWritingDLL == nullptr)
			throw std::runtime_error("加载 " + std::string(name) + " 失败。");

		fw::argument_list = (int)argList;

		load(fw::string_width, "FWStringWidth");
		load(fw::string_height, "FWStringHeight");
		load(fw::string_width_ext, "FWStringWidthEx");
		load(fw::string_height_ext, "FWStringHeightEx");
		load(fw::draw_text, "FWDrawText");
		load(fw::draw_text_transformed, "FWDrawTextTransformed");
		load(fw::draw_text_color, "FWDrawTextColor");
		load(fw::draw_text_transformed_color, "FWDrawTextTransformedColor");

		finish;
	}
	simplecatch("ImportFoxWritingModule", 0)
}

namespace fw
{
	struct Data
	{
		std::string text;
		std::string raw;
		GMReal w = 0;
		int font = -1;
	};

	using HashMap = std::unordered_map<xxh::hash64_t, std::vector<Data>>;
	HashMap ProcessedText;
	int CurrentFont = -1;

	std::string& get_text(GMString str, GMReal w)
	{
		std::string string(str);

		xxh::hash_state_t<64> hs;
		hs.update(string.data(), string.size());
		hs.update(&w, sizeof(GMReal));
		hs.update(&CurrentFont, sizeof(int));

		xxh::hash64_t hash = hs.digest();

		auto it_map = ProcessedText.find(hash);
		if (it_map != ProcessedText.end())
		{
			for (auto& entry : it_map->second)
			{
				if (entry.w == w && entry.font == CurrentFont && entry.raw == string)
					return entry.text;
			}
		}
		
		Data data = {
			.text = string_get_ext(str, w),
			.raw = std::move(string),
			.w = w,
			.font = CurrentFont
		};
		
		ProcessedText[hash].push_back(data);
		return ProcessedText[hash].back().text;
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
	simplecatch("FWDrawTextExt", 0)
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
	simplecatch("FWDrawTextExtTransformed", 0)
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
	simplecatch("FWDrawTextExtColor", 0)
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
	simplecatch("FWDrawTextExtTransformedColor", 0)
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

expReal FWSetCurrentFont(GMReal font)
{
	fw::CurrentFont = (int)font;
	finish;
}