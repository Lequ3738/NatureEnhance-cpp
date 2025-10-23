#pragma once
#include "Main.h"

extern HMODULE FoxWritingDLL;

typedef GMReal (*r_s)(GMString);

namespace fw
{
	extern r_s string_width;
}