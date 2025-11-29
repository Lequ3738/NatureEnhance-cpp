#pragma once

#include "Main.h"

namespace gui
{
	enum class surface : char;

	namespace size
	{
		extern const GMReal Normal;
		extern const GMReal Double;
		extern const GMReal Max;
	}

	extern GMReal DrawAppSurScale, FontQualityScale;
	extern int DrawAppSurX, DrawAppSurY;
	extern surface DrawingSurface;

	void draw_text(GMReal x, GMReal y, GMString str);
	void draw_text_ext(GMReal x, GMReal y, GMString str, GMReal w);
	void draw_sprite(int spr, int sub, GMReal x, GMReal y, GMReal screenScale,
		GMReal alpha, bool noInterPolation);
	void draw_sprite_ext(int spr, int sub, GMReal x, GMReal y, GMReal screenScale,
		GMReal xscale, GMReal yscale, GMReal rot, int blend, GMReal alpha,
		bool noInterPolation);
}