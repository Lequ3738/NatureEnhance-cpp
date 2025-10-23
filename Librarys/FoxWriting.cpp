#include "FoxWriting.h"

HMODULE FoxWritingDLL = nullptr;

namespace fw
{
	r_s string_width;
}

#define load(var, name, type) \
	var = (type)GetProcAddress(FoxWritingDLL, name); \
	if (var == nullptr) {\
		std::wstring wname(name, name + strlen(name)); \
		std::wstring err = L"加载 FoxWriting.dll 时获取函数 (" + wname + L") 失败。"; \
		throw err.c_str(); \
	}

expReal ImportFoxWritingModule(GMString name)
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

		load(fw::string_width, "FWStringWidth", r_s);

		finish;
	}
	simplecatch(L"ImportFoxWritingModule", 0)
}