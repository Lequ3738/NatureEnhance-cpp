#pragma once
#include "Main.h"

extern HMODULE FoxWritingDLL;

typedef GMReal (*r_s)(GMString);
typedef GMReal (*r_rrs)(GMReal, GMReal, GMString);
typedef GMReal (*r_srr)(GMString, GMReal, GMReal);

namespace fw
{
	extern r_s string_width, string_height;
	extern r_srr string_width_ext, string_height_ext;
	extern r_rrs draw_text;
	extern r_rrs draw_text_transformed;
	extern r_rrs draw_text_color;
	extern r_rrs draw_text_transformed_color;

	extern int argument_list;

	std::string& get_text(GMString str, GMReal w);
	void draw_text_ext(GMReal x, GMReal y, GMString str, GMReal w);
	void draw_text_ext_transformed(GMReal x, GMReal y, GMString str, GMReal w,
		GMReal xscale, GMReal yscale, GMReal angle);
	void draw_text_ext_color(GMReal x, GMReal y, GMString str, GMReal w,
		GMReal c1, GMReal c2, GMReal alpha);
	void draw_text_ext_transformed_color(GMReal x, GMReal y, GMString str, GMReal w,
		GMReal xscale, GMReal yscale, GMReal angle, GMReal c1, GMReal c2, GMReal alpha);
	void cleanup();
}