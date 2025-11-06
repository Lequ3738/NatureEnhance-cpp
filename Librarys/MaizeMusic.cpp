#include "MaizeMusic.h"

HMODULE MaizeMusicDLL = nullptr;

namespace mm
{
	r_sr load_music;
	r_rr set_tempo;
	r_r free_music, play, pause, resume, stop, get_active, get_pos, get_length;
}

#define load(var, name)													\
	var = (decltype(var))GetProcAddress(MaizeMusicDLL, name);			\
	if (var == nullptr) {												\
		throw std::runtime_error("加载 MaizeMusic.dll 时获取函数 ( " +		\
			std::string(name) + ") 失败。");								\
	}

expReal ImportMaizeMusicModule(GMString name)
{
	try
	{
		std::wstring wname(name, name + strlen(name));
		MaizeMusicDLL = GetModuleHandle(wname.c_str());

		if (MaizeMusicDLL == nullptr)
			throw std::runtime_error("加载 " + std::string(name) + " 失败。");

		load(mm::load_music, "mmLoadMusic");
		load(mm::free_music, "mmFreeMusic");
		load(mm::play, "mmMusicPlay");
		load(mm::pause, "mmMusicPause");
		load(mm::resume, "mmMusicResume");
		load(mm::stop, "mmMusicStop");
		load(mm::set_tempo, "mmMusicSetTempo");
		load(mm::get_active, "mmMusicIsActive");
		load(mm::get_pos, "mmMusicGetPosition");
		load(mm::get_length, "mmMusicGetLength");

		finish;
	}
	simplecatch("ImportMaizeMusicModule", 0)
}