#include "Main.h"
#include "base64.hpp"

expString base64_encode(GMString data)
{
	try
	{
		return string_to_cstr(base64::to_base64(data));
	}
	simplecatch("base64_encode", "");
}

expString base64_decode(GMString base64Text)
{
	try
	{
		return string_to_cstr(base64::from_base64(base64Text));
	}
	simplecatch("base64_decode", "");
}