#include "MaizeMusic.h"

HMODULE MaizeMusicDLL = nullptr;

namespace mm
{
	r_sr load_music;
	r_rr set_tempo;
	r_r free_music, play, pause, resume, stop, get_active, get_pos, get_length;
}

#define load(var, name, type) \
	var = (type)GetProcAddress(MaizeMusicDLL, name); \
	if (var == nullptr) {\
		std::wstring wname(name, name + strlen(name)); \
		std::wstring err = L"加载 MaizeMusic.dll 时获取函数 (" + wname + L") 失败。"; \
		throw err.c_str(); \
	}

expReal ImportMaizeMusicModule(GMString name)
{
	try
	{
		std::wstring wname(name, name + strlen(name));
		MaizeMusicDLL = GetModuleHandle(wname.c_str());

		if (MaizeMusicDLL == nullptr)
		{
			std::wstring err = L"加载 " + wname + L" 失败。";
			throw err.c_str();
		}

		load(mm::load_music, "mmLoadMusic", r_sr);
		load(mm::free_music, "mmFreeMusic", r_r);
		load(mm::play, "mmMusicPlay", r_r);
		load(mm::pause, "mmMusicPause", r_r);
		load(mm::resume, "mmMusicResume", r_r);
		load(mm::stop, "mmMusicStop", r_r);
		load(mm::set_tempo, "mmMusicSetTempo", r_rr);
		load(mm::get_active, "mmMusicIsActive", r_r);
		load(mm::get_pos, "mmMusicGetPosition", r_r);
		load(mm::get_length, "mmMusicGetLength", r_r);

		finish;
	}
	simplecatch(L"ImportMaizeMusicModule", 0)
}

expReal FreeMaizeMusicModule()
{
	MaizeMusicDLL = nullptr;

	mm::load_music = nullptr;
	mm::free_music = nullptr;
	mm::play = nullptr;
	mm::pause = nullptr;
	mm::resume = nullptr;
	mm::stop = nullptr;
	mm::set_tempo = nullptr;
	mm::get_active = nullptr;
	mm::get_pos = nullptr;
	mm::get_length = nullptr;

	finish;
}