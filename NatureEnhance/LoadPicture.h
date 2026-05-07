#pragma once

#include "Main.h"
#include "lodepng.h"
#include "buffer.h"
#include <future>

typedef std::future<std::tuple<std::vector<UCHAR>, UINT, UINT>> PNGDecodeFuture;
typedef UINT uint;

PNGDecodeFuture AsyncDecodePNG(GMString file);
PNGDecodeFuture AsyncDecodeGMBCK(GMString file);