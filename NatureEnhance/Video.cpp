#include "Main.h"
#include "buffer.h"
#include "MaizeMusic.h"
#include <filesystem>

namespace fs = std::filesystem;
typedef unsigned int uint;
typedef unsigned short ushort;

GMReal Buffer = gm::noone, FrameBuffer = gm::noone;
GMReal FirstFrame, LastFrame, FrameTime, Soundtrack;
bool VideoUseInterframe, VideoLoop, VideoUseSoundtrack;
GMString GMTempPath;

float VideoFPS;
uint VideoTotal, VideoCurrent;
ushort VideoWidth, VideoHeight;

bool VideoPlaying;
GMReal VideoSpeed = 1, FrameOffset, SoundtrackLength;
int VideoExportSurface = gm::noone, VideoScratchSurface = gm::noone;
int VideoTempSurface1 = gm::noone, VideoTempSurface2 = gm::noone;

expReal VideoInit(GMString tempPath)
{
	GMTempPath = tempPath;
	finish;
}

expReal VideoFree()
{
	if (gm::buffer_exists(Buffer))
		gm::buffer_destroy(Buffer);
	if (gm::buffer_exists(FrameBuffer))
		gm::buffer_destroy(FrameBuffer);

	if (VideoUseSoundtrack && Soundtrack >= 0x80000000)
		mm::free_music(Soundtrack);

	if (gm::surface_exists(VideoExportSurface))
		gm::surface_free(VideoExportSurface);

	if (VideoUseInterframe)
	{
		if (gm::surface_exists(VideoScratchSurface))
			gm::surface_free(VideoScratchSurface);
		if (gm::surface_exists(VideoTempSurface1))
			gm::surface_free(VideoTempSurface1);
		if (gm::surface_exists(VideoTempSurface2))
			gm::surface_free(VideoTempSurface2);
	}

	finish;
}

expReal VideoPlay(GMString path, GMReal loop, GMReal interframe)
{
	try
	{
		VideoFree();

		if (!fs::exists(path))
			throw (L"尝试打开不存在的文件：" + std::wstring(path, path + strlen(path))).c_str();

		if (GMTempPath == nullptr)
			throw L"请先调用 VideoInit()。";

		Buffer = gm::buffer_create();
		FrameBuffer = gm::buffer_create();

		gm::buffer_read_from_file(Buffer, path);

		GMString sig = gm::buffer_read_string(Buffer);
		if (strcmp(sig, "renex audiovideo v3") != 0)
		{
			gm::buffer_destroy(Buffer);
			gm::buffer_destroy(FrameBuffer);
			throw L"该文件似乎不是Rav编解码器数据块。";
		}

		// 加载音频数据块
		GMReal len = gm::buffer_read_uint32(Buffer);
		if (len == 0)
			VideoUseSoundtrack = false;
		else
		{
			VideoUseSoundtrack = true;

			GMReal pos = gm::buffer_get_pos(Buffer);
			gm::buffer_write_buffer_part(FrameBuffer, Buffer, pos, len);
			gm::buffer_set_pos(Buffer, pos + len);

			std::string tempFile = std::string(GMTempPath) + "\\VideoSoundtrack.mp3";
			gm::buffer_write_to_file(FrameBuffer, tempFile.c_str());
			Soundtrack = mm::load_music(tempFile.c_str(), 0);
			if (Soundtrack < 0x80000000)
				throw L"加载音频数据块失败。";

			SoundtrackLength = mm::get_length(Soundtrack);

			gm::buffer_clear(FrameBuffer);
		}

		// 读取视频文件头
		VideoFPS = (float)gm::buffer_read_float32(Buffer);
		VideoTotal = (uint)gm::buffer_read_uint32(Buffer);
		VideoWidth = (ushort)gm::buffer_read_uint16(Buffer);
		VideoHeight = (ushort)gm::buffer_read_uint16(Buffer);

		// 播放音频
		if (VideoUseSoundtrack)
			mm::play(Soundtrack);
		else
			LastFrame = TimerGet();

		// 初始化视频变量
		FirstFrame = gm::buffer_get_pos(Buffer);
		FrameTime = 1 / VideoFPS;
		VideoPlaying = true;
		VideoSpeed = 1;
		VideoLoop = false;
		VideoCurrent = -1;
		FrameOffset = 0;
		
		VideoExportSurface = gm::surface_create(VideoWidth, VideoHeight);

		if (VideoUseInterframe)
		{
			VideoScratchSurface = gm::surface_create(VideoWidth, VideoHeight);
			VideoTempSurface1 = gm::surface_create(VideoWidth, VideoHeight);
			VideoTempSurface2 = gm::surface_create(VideoWidth, VideoHeight);
		}

		finish;
	}
	simplecatch(L"VideoOpen", 0)
}

expReal VideoReset()
{
	gm::buffer_set_pos(Buffer, FirstFrame);
	VideoCurrent = -1;
	if (VideoUseSoundtrack)
	{
		mm::stop(Soundtrack);
		mm::play(Soundtrack);
		mm::set_tempo(Soundtrack, VideoSpeed);
	}
	VideoPlaying = true;

	finish;
}

expReal VideoUpdate()
{
	try
	{
		if (!VideoPlaying)
			fail;

		uint pos;

		if (!VideoUseSoundtrack)
		{
			pos = VideoCurrent;
			auto timer = TimerGet();
			if (timer >= LastFrame + FrameTime / VideoSpeed || VideoCurrent == -1)
			{
				pos = VideoCurrent + 1;
				LastFrame = timer;
			}

			FrameOffset = clamp((timer - LastFrame) / (FrameTime / VideoSpeed), 0, 1);
		}
		else
		{
			int state = (int)mm::get_active(Soundtrack);
			if (state == 0)  // Stopped
			{
				pos = 0;
				FrameOffset = 0;
				VideoPlaying = false;
			}
			else
			{
				GMReal p = min(1, mm::get_pos(Soundtrack) / SoundtrackLength) * VideoTotal;
				FrameOffset = fmod(p, 1.0);  // 取 p 的小数部分
				pos = (uint)p;  // 取 p 的整数部分
			}
		}

		while (pos > VideoCurrent && VideoCurrent < VideoTotal - 1)
		{
			++VideoCurrent;

			// 提取一帧数据
			gm::buffer_clear(FrameBuffer);

			auto len = gm::buffer_read_uint32(Buffer);
			auto p = gm::buffer_get_pos(Buffer);

			gm::buffer_write_buffer_part(FrameBuffer, Buffer, p, len);
			gm::buffer_zlib_uncompress(FrameBuffer);
			gm::buffer_set_pos(Buffer, p + len);

			// 将数据写入表面
			GMReal size = VideoWidth * VideoHeight * 4;
			GMReal cursize = gm::buffer_get_size(FrameBuffer);
			if (cursize != size)
			{
				std::wstring err = L"视频数据块大小不正确。\n视频帧：" + std::to_wstring(VideoCurrent) + 
					L"/" + std::to_wstring(VideoTotal) + L"\n读取到的数据大小：" + 
					std::to_wstring(cursize) + L"\n应读取的数据大小：" + std::to_wstring(size);
				throw err.c_str();
			}

			if (!gm::surface_exists(VideoExportSurface))
				VideoExportSurface = gm::surface_create(VideoWidth, VideoHeight);

			if (VideoUseInterframe)
			{
				if (!gm::surface_exists(VideoScratchSurface))
					VideoScratchSurface = gm::surface_create(VideoWidth, VideoHeight);

				if (!gm::surface_exists(VideoTempSurface1))
					VideoTempSurface1 = gm::surface_create(VideoWidth, VideoHeight);

				if (!gm::surface_exists(VideoTempSurface2))
					VideoTempSurface2 = gm::surface_create(VideoWidth, VideoHeight);

				int gmtex = gm::surface_get_texture(VideoScratchSurface);
				BufferToTexture(FrameBuffer, gmtex, VideoWidth, VideoHeight);

				gm::surface_copy(VideoTempSurface1, 0, 0, VideoExportSurface);

				gm::surface_set_target(VideoExportSurface);
				gm::draw_surface(VideoScratchSurface, 0, 0);
				gm::surface_reset_target();
			}
			else
			{
				int gmtex = gm::surface_get_texture(VideoExportSurface);
				BufferToTexture(FrameBuffer, gmtex, VideoWidth, VideoHeight);
			}
		}

		if (VideoUseInterframe)
		{
			gm::surface_set_target(VideoTempSurface2);
			gm::draw_clear(gm::c_black);
			gm::draw_surface(VideoTempSurface1, 0, 0);
			
			Device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED |
				D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
			gm::draw_surface_ext(VideoExportSurface, 0, 0, 1, 1, 0, 0xffffff, 
				FrameOffset + (VideoCurrent == 0));
			Device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED |
				D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);

			gm::surface_reset_target();
		}

		if (VideoCurrent >= VideoTotal - 1)
		{
			if (VideoLoop)
				VideoReset();
			else
				VideoPlaying = false;
		}

		finish;
	}
	simplecatch(L"VideoUpdate", 0)
}

expReal VideoPause()
{
	VideoPlaying = false;
	if (VideoUseSoundtrack)
		mm::pause(Soundtrack);

	finish;
}

expReal VideoResume()
{
	VideoPlaying = true;
	if (VideoUseSoundtrack)
		mm::resume(Soundtrack);

	finish;
}

expReal VideoStop()
{
	gm::buffer_set_pos(Buffer, FirstFrame);
	VideoCurrent = -1;
	if (VideoUseSoundtrack)
		mm::stop(Soundtrack);
	
	VideoPlaying = false;

	finish;
}

expReal VideoGetWidth() { return VideoWidth; }
expReal VideoGetHeight() { return VideoHeight; }
expReal VideoGetFPS() { return VideoFPS; }
expReal VideoGetFrames() { return VideoTotal - 1; }
expReal VideoGetPosition() { return VideoCurrent; }
expReal VideoGetSpeed() { return VideoSpeed; }
expReal VideoGetLoop() { return VideoLoop ? 1.0 : 0.0; }

expReal VideoGetSurface()
{
	if (VideoUseInterframe)
		return VideoTempSurface2;
	else
		return VideoExportSurface;
}

expReal VideoGetSoundtrack()
{
	if (VideoUseSoundtrack)
		return Soundtrack;
	else
		return gm::noone;
}

expReal VideoSetSpeed(GMReal speed)
{
	if (speed <= 0)
		fail;
	
	VideoSpeed = speed;
	if (VideoUseSoundtrack)
		mm::set_tempo(Soundtrack, VideoSpeed);

	finish;
}

expReal VideoSetLoop(GMReal loop)
{
	VideoLoop = (loop > 0.5);
	finish;
}