#pragma once
#include "Main.h"

extern HMODULE MaizeMusicDLL;

typedef double (*r_r)(double);
typedef double (*r_rr)(double, double);
typedef double (*r_sr)(const char*, double);

namespace mm
{
	extern r_sr load_music;
	extern r_rr set_tempo;
	extern r_r free_music, play, pause, resume, stop, get_active, get_pos, get_length;
}