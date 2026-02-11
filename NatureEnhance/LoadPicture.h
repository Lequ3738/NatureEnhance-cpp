#pragma once

#include "Main.h"
#include "lodepng.h"
#include "buffer.h"
#include <future>

typedef std::future<std::tuple<std::vector<UCHAR>, UINT, UINT>> PNGDecodeFuture;

PNGDecodeFuture AsyncDecodePNG(GMString file);
PNGDecodeFuture AsyncDecodeGMBCK(GMString file);