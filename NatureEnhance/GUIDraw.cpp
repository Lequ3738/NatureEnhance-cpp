#include "GUIDraw.h"
#include "FoxWriting.h"

namespace gui
{
	enum class surface : char
	{
		application,
		gui,
		expo,
		user,
		prev
	};

	namespace size
	{
		constexpr GMReal Normal = 1;
		constexpr GMReal Double = 2;
		constexpr GMReal Max = 4.8;
	}

	GMReal DrawAppSurScale = size::Max, FontQualityScale = 1;
	int DrawAppSurX = 0, DrawAppSurY = 0;
	surface DrawingSurface = surface::expo;

	void draw_text(GMReal x, GMReal y, GMString str)
	{
		GMReal guiX = x * DrawAppSurScale;
		GMReal guiY = y * DrawAppSurScale;

		if (DrawingSurface == surface::gui)
		{
			guiX += DrawAppSurX;
			guiY += DrawAppSurY;
		}

		gm::ds_list_clear(fw::argument_list);
		gm::ds_list_add(fw::argument_list, FontQualityScale);
		gm::ds_list_add(fw::argument_list, FontQualityScale);
		gm::ds_list_add(fw::argument_list, 0);

		fw::draw_text_transformed(guiX, guiY, str);
	}

	void draw_text_ext(GMReal x, GMReal y, GMString str, GMReal w)
	{
		GMReal guiX = x * DrawAppSurScale;
		GMReal guiY = y * DrawAppSurScale;
		GMReal guiW = w * DrawAppSurScale / FontQualityScale;
		
		if (DrawingSurface == surface::gui)
		{
			guiX += DrawAppSurX;
			guiY += DrawAppSurY;
		}
		
		fw::draw_text_ext_transformed(guiX, guiY, str, guiW, FontQualityScale, 
			FontQualityScale, 0);
	}

	void draw_sprite(int spr, int sub, GMReal x, GMReal y, GMReal screenScale,
		GMReal alpha, bool noInterPolation = false)
	{
		GMReal guiX = x * DrawAppSurScale;
		GMReal guiY = y * DrawAppSurScale;
		GMReal scnScale = DrawAppSurScale / screenScale;

		if (DrawingSurface == surface::gui)
		{
			guiX += DrawAppSurX;
			guiY += DrawAppSurY;
		}

		if (noInterPolation)
			gm::texture_set_interpolation(false);

		gm::draw_sprite_ext(spr, sub, guiX, guiY, scnScale, scnScale, 0, gm::c_white, alpha);

		if (noInterPolation)
			gm::texture_set_interpolation(true);
	}

	void draw_sprite_ext(int spr, int sub, GMReal x, GMReal y, GMReal screenScale,
		GMReal xscale, GMReal yscale, GMReal rot, int blend, GMReal alpha,
		bool noInterPolation = false)
	{
		GMReal guiX = x * DrawAppSurScale;
		GMReal guiY = y * DrawAppSurScale;
		GMReal scnScale = DrawAppSurScale / screenScale;

		if (DrawingSurface == surface::gui)
		{
			guiX += DrawAppSurX;
			guiY += DrawAppSurY;
		}

		if (noInterPolation)
			gm::texture_set_interpolation(false);

		gm::draw_sprite_ext(spr, sub, guiX, guiY, scnScale * xscale, scnScale * yscale, 
			rot, blend, alpha);

		if (noInterPolation)
			gm::texture_set_interpolation(true);
	}

	void draw_background(int back, GMReal x, GMReal y, GMReal screenScale, GMReal alpha, 
		bool noInterPolation = false)
	{
		GMReal guiX = x * DrawAppSurScale;
		GMReal guiY = y * DrawAppSurScale;
		GMReal scnScale = DrawAppSurScale / screenScale;

		if (DrawingSurface == surface::gui)
		{
			guiX += DrawAppSurX;
			guiY += DrawAppSurY;
		}

		if (noInterPolation)
			gm::texture_set_interpolation(false);

		gm::draw_background_ext(back, guiX, guiY, scnScale, scnScale, 0, gm::c_white, alpha);

		if (noInterPolation)
			gm::texture_set_interpolation(true);
	}

	void draw_background_ext(int back, GMReal x, GMReal y, GMReal screenScale, GMReal xscale, 
		GMReal yscale, GMReal rot, int blend, GMReal alpha, bool noInterPolation = false)
	{
		GMReal guiX = x * DrawAppSurScale;
		GMReal guiY = y * DrawAppSurScale;
		GMReal scnScale = DrawAppSurScale / screenScale;

		if (DrawingSurface == surface::gui)
		{
			guiX += DrawAppSurX;
			guiY += DrawAppSurY;
		}

		if (noInterPolation)
			gm::texture_set_interpolation(false);

		gm::draw_background_ext(back, guiX, guiY, scnScale * xscale, scnScale * yscale,
			rot, blend, alpha);

		if (noInterPolation)
			gm::texture_set_interpolation(true);
	}
}

expReal SetGuiVariable(GMString name, GMReal value)
{
	if (std::strcmp(name, "SurScale") == 0)
		gui::DrawAppSurScale = value;
	else if (std::strcmp(name, "FontScale") == 0)
		gui::FontQualityScale = value;
	else if (std::strcmp(name, "AppSurX") == 0)
		gui::DrawAppSurX = (int)value;
	else if (std::strcmp(name, "AppSurY") == 0)
		gui::DrawAppSurY = (int)value;
	else if (std::strcmp(name, "DrawingSurface") == 0)
		gui::DrawingSurface = (gui::surface)(int)value;

	finish;
}

expReal GuiDrawText(GMReal x, GMReal y, GMString str)
{
	gui::draw_text(x, y, str);
	finish;
}

expReal GuiDrawSprite(GMReal spr, GMReal sub, GMReal x, GMReal y,
	GMReal screenScale, GMReal alpha, GMReal noInterPolation)
{
	gui::draw_sprite(static_cast<int>(spr), static_cast<int>(sub), x, y, 
		screenScale, alpha, static_cast<bool>(noInterPolation));

	finish;
}

expReal GuiDrawSpriteExt(GMReal spr, GMReal sub, GMReal x, GMReal y,
	GMReal screenScale, GMReal xscale, GMReal yscale, GMReal rot,
	GMReal blend, GMReal alpha, GMReal noInterPolation)
{
	gui::draw_sprite_ext(static_cast<int>(spr), static_cast<int>(sub), x, y,
		screenScale, xscale, yscale, rot, static_cast<int>(blend), alpha,
		static_cast<bool>(noInterPolation));
	finish;
}

expReal GuiDrawBackground(GMReal back, GMReal x, GMReal y,
	GMReal screenScale, GMReal alpha, GMReal noInterPolation)
{
	gui::draw_background(static_cast<int>(back), x, y,
		screenScale,  alpha, static_cast<bool>(noInterPolation));
	finish;
}

expReal GuiDrawBackgroundExt(GMReal back, GMReal x, GMReal y,
	GMReal screenScale, GMReal xscale, GMReal yscale, GMReal rot,
	GMReal blend, GMReal alpha, GMReal noInterPolation)
{
	gui::draw_background_ext(static_cast<int>(back), x, y,
		screenScale, xscale, yscale, rot, static_cast<int>(blend), alpha,
		static_cast<bool>(noInterPolation));
	finish;
}