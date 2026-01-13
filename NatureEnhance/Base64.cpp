#include "Main.h"
#include "base64.hpp"

expString base64_encode(GMString data)
{
	try
	{
		GMReturnString = base64::to_base64(data);
		return GMReturnString.c_str();
	}
	simplecatch("base64_encode", "");
}

expString base64_decode(GMString base64Text)
{
	try
	{
		GMReturnString = base64::from_base64(base64Text);
		return GMReturnString.c_str();
	}
	simplecatch("base64_decode", "");
}