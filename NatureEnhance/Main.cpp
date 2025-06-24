#include "Main.h"
#include "buffer.h"
#include "iconv.h"
#include <filesystem>
#include <vector>
#include <fstream>
#include <regex>
#include <psapi.h>

gm::CGMVariable GetResource(GMString res)
{
    return gm::execute_string("return " + std::string(res));
}

gm::CGMVariable GetResource(std::string res)
{
    return gm::execute_string("return " + res);
}

#pragma region GameMaker
bool show_error = true;
expReal ShowErrorMessage(GMReal mode)
{
	show_error = static_cast<bool>(mode);
	finish;
}

HWND GMWindowsHandle = nullptr;
IDirect3DDevice8* Device = nullptr;

expReal GetGMWindowsHandle(GMReal handle)
{
    GMWindowsHandle = (HWND)(DWORD)handle;
    Device = gmapi->GetDirect3DDevice();

    finish;
}

GMReal PropertyMap, NameMap;
expReal GetDSController(GMReal propertyMap, GMReal nameMap)
{
    PropertyMap = propertyMap;
    NameMap = nameMap;
    finish;
}

expReal BinToDec(GMString bin)
{
    try
    {
        int value = 0;
        while (*bin)
        {
            if (*bin != '0' && *bin != '1')
                throw L"不合法的二进制字符串字面量。";

            value = (value << 1) | (*bin - '0');
            ++bin;
        }

        return value;
    }
    simplecatch(L"BinToDec", -1)
}

typedef void (WINAPI* GetVersionPtr)(LPDWORD, LPDWORD, LPDWORD);

expReal OSGetVersion()
{
    HMODULE ntdllModule = GetModuleHandleW(L"ntdll.dll");
    if (!ntdllModule)
        return -1;

    auto GetVersion = (GetVersionPtr)GetProcAddress(ntdllModule, "RtlGetNtVersionNumbers");
    if (!GetVersion)
        return -1;

    DWORD major, minor, build;
    GetVersion(&major, &minor, &build);

    if (major == 10)
    {
        if (build >= 22000) return 11;
        else return 10;
    }
    else if (major == 6)
    {
        if (minor == 3) return 8.1;
        if (minor == 2) return 8;
        if (minor == 1) return 7;
        if (minor == 0) return 6;
    }

    return (double)major;
}

expReal ShowMessageBox(GMString text, GMString caption, GMReal icon)
{
    UINT realIcon = 0;
    if (icon == 1)
        realIcon = MB_ICONERROR;
    else if (icon == 2)
        realIcon = MB_ICONWARNING;
    else if (icon == 3)
        realIcon = MB_ICONINFORMATION;

    MessageBoxA(GMWindowsHandle, text, caption, MB_OK | realIcon);
    finish;
}

expReal window_set_dpiaware()
{
    SetProcessDPIAware();
    finish;
}

expReal get_ram_usage()
{
    DWORD dwProcessId;
    HANDLE Process;
    PROCESS_MEMORY_COUNTERS_EX pmc;

    dwProcessId = GetCurrentProcessId();
    Process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwProcessId);
    GetProcessMemoryInfo(Process, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    CloseHandle(Process);

    return pmc.PrivateUsage;
}

expReal WindowIsFocused()
{
    return GetForegroundWindow() == GMWindowsHandle;
}

expReal WindowSetFocus()
{
    if (!SetForegroundWindow(GMWindowsHandle))
    {
        // 附加输入线程处理（绕过系统限制）
        DWORD threadID = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
        DWORD currentThreadID = GetCurrentThreadId();

        if (threadID != currentThreadID)
        {
            AttachThreadInput(currentThreadID, threadID, TRUE);
            SetForegroundWindow(GMWindowsHandle);
            AttachThreadInput(currentThreadID, threadID, FALSE);
        }
    }

    // 确保窗口激活
    SetActiveWindow(GMWindowsHandle);
    SetFocus(GMWindowsHandle);

    finish;
}

GMString ChangeCoding(GMString str, GMString inputCoding, GMString outputCoding)
{
    iconv_t cd = iconv_open(outputCoding, inputCoding);
    if (cd == (iconv_t)-1)
        return nullptr;

    size_t in_len = strlen(str);
    size_t out_len = in_len * 4;  // UTF-8 最多是原始大小的 4 倍
    char* output = new char[out_len + 1]; // +1 存放终止符
    memset(output, 0, out_len + 1);

    // 设置输入/输出缓冲区指针
    char* in_ptr = const_cast<char*>(str);
    char* out_ptr = output;
    size_t in_bytes_left = in_len;
    size_t out_bytes_left = out_len;

    // 执行转换
    if (iconv(cd, (GMString*)&in_ptr, &in_bytes_left, &out_ptr, &out_bytes_left) == (size_t)-1)
    {
        iconv_close(cd);
        delete[] output;
        return nullptr;
    }

    // 添加终止符并清理
    *out_ptr = '\0';
    iconv_close(cd);
    return output;
}

std::wstring ToWstring(GMString str)
{
    GMString utf8_str = ChangeCoding(str, "GB2312", "UTF-8");
    std::string stdstr(utf8_str);
    return std::wstring(stdstr.begin(), stdstr.end());
}
#pragma endregion

#pragma region Camera
GMReal CameraX, CameraY, ViewX, ViewY;
GMReal RoomWidth = 400, RoomHeight = 225, ViewWidth = 400, ViewHeight = 225;
GMReal Mode = 1, SnapDiv = 12, OffsetX = 24, OffsetY = -24, Factor = 0.16, MoveMode = 0;
GMReal LimitLeft = 0, LimitTop = 0, OldCameraX = 0, OldCameraY = 0;

expReal CameraInit(GMReal mode, GMReal playerX, GMReal playerY, GMReal playerScale, GMReal limitLeft,
	GMReal limitTop, GMReal roomWidth, GMReal roomHeight, GMReal viewWidth, GMReal viewHeight)
{
	try
	{
        LimitLeft = limitLeft;
        LimitTop = limitTop;
        RoomWidth = roomWidth;
        RoomHeight = roomHeight;
        ViewWidth = viewWidth;
        ViewHeight = viewHeight;
        Mode = mode;

        if (mode == 1)
        {
            ViewX = clamp(floor(playerX / viewWidth) * viewWidth, limitLeft, roomWidth - 1);
            ViewY = clamp(floor(playerY / viewHeight) * viewHeight, limitTop, roomHeight - 1);

            CameraX = ViewX + (viewWidth / 2);
            CameraY = ViewY + (viewHeight / 2);
        }
        else if (mode == 2 || mode == 3)
        {
            GMReal shackPlayerX = clamp(playerX, LimitLeft + (ViewWidth / 2) - (playerScale * OffsetX),
                RoomWidth - (ViewWidth / 2) - (playerScale * OffsetX));
            GMReal shackPlayerY = clamp(playerY, LimitTop + (ViewHeight / 2) + OffsetY,
                RoomHeight - (ViewHeight / 2) + OffsetY);

            CameraX = shackPlayerX + (playerScale * OffsetX);
            CameraY = shackPlayerY + OffsetY;

            ViewX = CameraX - (viewWidth / 2);
            ViewY = CameraY - (viewHeight / 2);
        }
        else if (mode == 0)
        {
            ViewX = CameraX - (viewWidth / 2);
            ViewY = CameraY - (viewHeight / 2);
        }
        else
            throw L"未定义此运动模式";
        
        finish;
	}
    catch (const wchar_t* e)
    {
        if (show_error)
        {
            std::wstring err = L"在执行函数 CameraInit 时抛出异常。\n" + std::wstring(e);
            MessageBox(GMWindowsHandle, err.c_str(), L"NatureEnhance Error", MB_OK | MB_ICONERROR);
        }
        
        fail;
    };
}

expReal CameraMove(GMReal playerX, GMReal playerY, GMReal playerScale)
{
    try
    {
        if (Mode == 1)  //传统的以单元格为单位移动相机
        {
            GMReal xFollow = (floor(playerX / ViewWidth) * ViewWidth) + (ViewWidth / 2);
            GMReal yFollow = (floor(playerY / ViewHeight) * ViewHeight) + (ViewHeight / 2);

            GMReal dir = point_direction(CameraX, CameraY, xFollow, yFollow);
            GMReal dis = point_distance(CameraX, CameraY, xFollow, yFollow);

            OldCameraX = CameraX;
            OldCameraY = CameraY;

            CameraX += lengthdir_x(dis, dir) / SnapDiv;
            CameraY += lengthdir_y(dis, dir) / SnapDiv;

            if (abs(CameraX - OldCameraX) < Factor)
            {
                if (round(CameraX) != round(xFollow))
                    CameraX += sign(xFollow - CameraX) / 2;
                else
                    CameraX = xFollow;
            }

            if (abs(CameraY - OldCameraY) < Factor)
            {
                if (round(CameraY) != round(yFollow))
                    CameraY += sign(yFollow - CameraY) / 2;
                else
                    CameraY = yFollow;
            }

            ViewX = clamp(CameraX - (ViewWidth / 2), LimitLeft, RoomWidth - ViewWidth);
            ViewY = clamp(CameraY - (ViewHeight / 2), LimitTop, RoomHeight - ViewHeight);
        }
        else if (Mode == 2)  //以位置偏差参数跟随玩家来平滑移动相机
        {
            GMReal shackPlayerX = clamp(playerX, LimitLeft + (ViewWidth / 2) - (playerScale * OffsetX),
                RoomWidth - (ViewWidth / 2) - (playerScale * OffsetX));
            GMReal shackPlayerY = clamp(playerY, LimitTop + (ViewHeight / 2) + OffsetY,
                RoomHeight - (ViewHeight / 2) + OffsetY);

            GMReal dir = point_direction(CameraX, CameraY, shackPlayerX + (playerScale * OffsetX),
                shackPlayerY + OffsetY);
            GMReal dis = point_distance(CameraX, CameraY, shackPlayerX + (playerScale * OffsetX),
                shackPlayerY + OffsetY);

            OldCameraX = CameraX;
            OldCameraY = CameraY;

            CameraX += lengthdir_x(dis, dir) / SnapDiv;
            CameraY += lengthdir_y(dis, dir) / SnapDiv;

            if (abs(CameraX - OldCameraX) < Factor)
                CameraX = OldCameraX;

            if (abs(CameraY - OldCameraY) < Factor)
                CameraY = OldCameraY;

            ViewX = CameraX - (ViewWidth / 2);
            ViewY = CameraY - (ViewHeight / 2);
        }
        else if (Mode == 3)  //以位置偏差参数跟随玩家来平滑移动相机
        {
            GMReal shackPlayerX = clamp(playerX, LimitLeft + (ViewWidth / 2) - (playerScale * OffsetX),
                RoomWidth - (ViewWidth / 2) - (playerScale * OffsetX));
            GMReal shackPlayerY = clamp(playerY, LimitTop + (ViewHeight / 2) + OffsetY,
                RoomHeight - (ViewHeight / 2) + OffsetY);

            OldCameraX = CameraX;
            OldCameraY = CameraY;

            CameraX += (shackPlayerX + (playerScale * OffsetX) - CameraX) / SnapDiv;
            CameraY += (shackPlayerY + OffsetY - CameraY) / SnapDiv;

            if (abs(CameraX - OldCameraX) < Factor)
                CameraX = OldCameraX;

            if (abs(CameraY - OldCameraY) < Factor)
                CameraY = OldCameraY;

            ViewX = CameraX - (ViewWidth / 2);
            ViewY = CameraY - (ViewHeight / 2);
        }
        else if (Mode == 0)
        {
            ViewX = CameraX - (ViewWidth / 2);
            ViewY = CameraY - (ViewHeight / 2);
        }
        else
            throw L"未定义此运动模式";

        finish;
    }
    catch (const wchar_t* e)
    {
        if (show_error)
        {
            std::wstring err = L"在执行函数 CameraInit 时抛出异常。\n" + std::wstring(e);
            MessageBox(GMWindowsHandle, err.c_str(), L"NatureEnhance Error", MB_OK | MB_ICONERROR);
        }

        fail;
    };
}

expReal CameraSetOffset(GMReal offsetX, GMReal offsetY)
{
    OffsetX = offsetX;
    OffsetY = offsetY;

    finish;
}

expReal CameraSetMode(GMReal mode)
{
    Mode = mode;
    finish;
}

expReal CameraSetSnapLevel(GMReal level)
{
    SnapDiv = level;
    finish;
}

expReal CameraExport(GMReal type)
{
    if (type == 0)
        return ViewX;
    else
        return ViewY;
}

expReal CameraSetView(GMReal cameraX, GMReal cameraY)
{
    OldCameraX = CameraX;
    OldCameraY = CameraY;

    CameraX = cameraX;
    CameraY = cameraY;

    finish;
}

expReal CameraSetRelativeView(GMReal cameraX, GMReal cameraY)
{
    OldCameraX = CameraX;
    OldCameraY = CameraY;

    CameraX += cameraX;
    CameraY += cameraY;

    finish;
}

expReal CameraSetStopFactor(GMReal factor)
{
    Factor = factor;
    finish;
}

expReal CameraSetRoomSize(GMReal roomWidth, GMReal roomHeight)
{
    RoomWidth = roomWidth;
    RoomHeight = roomHeight;

    finish;
}

expReal CameraSetLimit(GMReal limitLeft, GMReal limitTop)
{
    LimitLeft = limitLeft;
    LimitTop = limitTop;

    finish;
}

expReal CameraGetSpeedDirection()
{
    return point_direction(CameraX, CameraY, OldCameraX, OldCameraY);
}

expReal CameraGetSpeed()
{
    return point_distance(CameraX, CameraY, OldCameraX, OldCameraY);
}

expReal CameraGetXSpeed()
{
    return CameraX - OldCameraX;
}

expReal CameraGetYSpeed()
{
    return CameraY - OldCameraY;
}

expReal CameraSetViewSize(GMReal viewWidth, GMReal viewHeight)
{
    ViewWidth = viewWidth;
    ViewHeight = viewHeight;

    return 1;
}
#pragma endregion

#pragma region Rope Calculate
double RopeA, RopeB, RopeC;
int Iterations = 15, RopeSteps = 16;

static GMReal RopeArclength(GMReal a, GMReal b, GMReal x)
{
    auto temp1 = 2 * a * (x - b);
    auto temp2 = sqrt(temp1 * temp1 + 1);

    return temp2 * (x - b) / 2 - log(temp2 - temp1) / a / 4;
}

expReal RopeSetAccuracy(GMReal iterations, GMReal steps)
{
    Iterations = static_cast<int>(iterations);
	RopeSteps = static_cast<int>(steps);
    return 1;
}

static GMReal RopeCalculate(GMReal x1, GMReal y1, GMReal x2, GMReal y2, GMReal length)
{
    if (x1 > x2)
    {
        auto temp = x1; x1 = x2; x2 = temp;
        temp = y1; y1 = y2; y2 = temp;
    }

    if (x2 - x1 < 0.1)
        return 3;

    // left bound
    auto a = -0.000001;
    auto b = (a * (x1 * x1 - x2 * x2) + y2 - y1) / (2 * a * (x1 - x2));
    auto len = RopeArclength(a, b, x2) - RopeArclength(a, b, x1);
    if (len > length)
        return 2;

    auto a1 = a;

    // right bound
    a = -1;  // initial guess
    while (true)
    {
        b = (a * (x1 * x1 - x2 * x2) + y2 - y1) / (2 * a * (x1 - x2));
        len = RopeArclength(a, b, x2) - RopeArclength(a, b, x1);
        if (len > length)
            break;

        a1 = a;  // move left bound
        a *= 2;
        if (a < -1000000)
            return 3;
    }

    // bisection method
    auto a2 = a;
    for (int i = 0; i < Iterations; i++)
    {
        a = (a1 + a2) / 2;
        b = (a * (x1 * x1 - x2 * x2) + y2 - y1) / (2 * a * (x1 - x2));
        len = RopeArclength(a, b, x2) - RopeArclength(a, b, x1);

        if (len > length)
            a2 = a;
        else
            a1 = a;
    }
    a = (a1 + a2) / 2;

    // generate solution
    RopeA = a;
    RopeB = (a * (x1 * x1 - x2 * x2) + y2 - y1) / (2 * a * (x1 - x2));
    RopeC = (2 * a * sqr(x1 - x2) * (y1 + y2) - a * a * sqr(sqr(x1 - x2)) -
        sqr(y2 - y1)) / (a * sqr(x1 - x2) * 4);

    return 1;
}

expReal DrawRope(GMReal x1, GMReal y1, GMReal x2, GMReal y2, GMReal length, GMReal back)
{
    GMReal width = 1, height = 1;
    if (back != gm::noone)
    {
        width = gm::background_get_width(static_cast<int>(back));
		height = gm::background_get_height(static_cast<int>(back));
    }

	GMReal state = RopeCalculate(x1, y1, x2, y2, length);

    if (state == 1)
    {
        GMReal xcale = (x1 < x2) ? 1 : -1;
        
        // draw the rope
        if (back != gm::noone)
        {
            // calculate length
            GMReal tx1 = 0, ty1 = 0;
            GMReal len = 0;
            for (int i = 0; i <= RopeSteps; i++)
            {
                GMReal tx2 = x1 + (x2 - x1) * i / RopeSteps;
                GMReal ty2 = RopeA * sqr(tx2 - RopeB) + RopeC;

                if (i != 0)
                    len += point_distance(tx1, ty1, tx2, ty2);

                tx1 = tx2;
                ty1 = ty2;
            }

            gm::texture_set_repeat(true);
            gm::draw_primitive_begin_texture(gm::pr_trianglestrip, 
                gm::background_get_texture(static_cast<int>(back)));

            tx1 = x1;
            ty1 = RopeA * sqr(tx1 - RopeB) + RopeC;
            GMReal len2 = 0;

            for (int i = 0; i <= RopeSteps; i++)
            {
                GMReal tx2 = x1 + (x2 - x1) * i / RopeSteps;
                GMReal ty2 = RopeA * sqr(tx2 - RopeB) + RopeC;

                if (i != 0)
                    len2 += point_distance(tx1, ty1, tx2, ty2);

                GMReal rc = 2 * RopeA * (tx2 - RopeB);
                GMReal d = xcale * height / 2 / sqrt(sqr(rc) + 1);

                gm::draw_vertex_texture(tx2 - rc * d - 0.5, ty2 + d - 0.5, length * len2 / len / width, 1);
                gm::draw_vertex_texture(tx2 + rc * d - 0.5, ty2 - d - 0.5, length * len2 / len / width, 0);

				tx1 = tx2;
				ty1 = ty2;
            }
        }
        else
        {
			gm::draw_primitive_begin(gm::pr_linestrip);

            for (int i = 0; i <= RopeSteps; i++)
            {
                GMReal tx = x1 + (x2 - x1) * i / RopeSteps;
                GMReal ty = RopeA * sqr(tx - RopeB) + RopeC;

                gm::draw_vertex(tx - 0.5, ty - 0.5);
            }
        }

        gm::draw_primitive_end();
    }
    else if (state == 2)
    {
        if (back != gm::noone)
        {
            GMReal tx = x2 - x1, ty = y2 - y1;
            GMReal d = sqrt(sqr(tx) + sqr(ty));

            if (d > 0.001)
            {
                tx *= height / 2 / d;
                ty *= height / 2 / d;

                gm::texture_set_repeat(true);
                gm::draw_primitive_begin_texture(gm::pr_trianglestrip, 
                    gm::background_get_texture(static_cast<int>(back)));

                gm::draw_vertex_texture(x1 - ty - 0.5, y1 + tx - 0.5, 0, 1);
                gm::draw_vertex_texture(x1 + ty - 0.5, y1 - tx - 0.5, 0, 0);
                gm::draw_vertex_texture(x2 - ty - 0.5, y2 + tx - 0.5, length / width, 1);
                gm::draw_vertex_texture(x2 + ty - 0.5, y2 - tx - 0.5, length / width, 0);
            }
        }
        else
        {
            gm::draw_primitive_begin(gm::pr_linestrip);
            gm::draw_vertex(x1 - 0.5, y1 - 0.5);
            gm::draw_vertex(x2 - 0.5, y2 - 0.5);
        }

        gm::draw_primitive_end();
    }
    else
    {
        if (back != gm::noone)
        {
            GMReal tx = (x1 + x2) / 2, ty = (y1 + y2 + length) / 2;
            GMReal d = height / 2;
            GMReal e = (y2 - y1 + length) / 2 / width;

            gm::texture_set_repeat(true);
            gm::draw_primitive_begin_texture(gm::pr_trianglestrip, 
                gm::background_get_texture(static_cast<int>(back)));

            gm::draw_vertex_texture(x1 - d - 0.5, y1 - 0.5, 0, 1);
            gm::draw_vertex_texture(x1 + d - 0.5, y1 - 0.5, 0, 0);
            gm::draw_vertex_texture(tx - d, ty, e, 1);
            gm::draw_vertex_texture(tx + d, ty, e, 0);
            gm::draw_vertex_texture(tx + d, ty, e, 1);
            gm::draw_vertex_texture(tx - d, ty, e, 0);
            gm::draw_vertex_texture(x2 + d - 0.5, y2 + 0.5, length / width, 1);
            gm::draw_vertex_texture(x2 - d - 0.5, y2 - 0.5, length / width, 0);
        }
        else
        {
			gm::draw_primitive_begin(gm::pr_linestrip);
			gm::draw_vertex(x1 - 0.5, y1 - 0.5);
			gm::draw_vertex(x2 - 0.5, y2 - 0.5);
        }

		gm::draw_primitive_end();
    }

    return state;
}
#pragma endregion

#pragma region Load Tiles
std::vector<int> DrawSpritesList;

expReal LoadDrawSpritesList(GMReal a, GMReal b, GMReal c, GMReal d, GMReal e, GMReal f)
{
    DrawSpritesList.reserve(6);

    DrawSpritesList.clear();
    DrawSpritesList.push_back(static_cast<int>(a));
    DrawSpritesList.push_back(static_cast<int>(b));
    DrawSpritesList.push_back(static_cast<int>(c));
    DrawSpritesList.push_back(static_cast<int>(d));
    DrawSpritesList.push_back(static_cast<int>(e));
    DrawSpritesList.push_back(static_cast<int>(f));

    finish;
}

expReal LoadRoomTiles(GMString path)
{
    try
    {
        if (!std::filesystem::exists(path))
            return 0.0;

        GMReal buffer = gm::buffer_create();
        gm::buffer_read_from_file(buffer, path);

        GMReal version = gm::buffer_read_uint8(buffer);

        // Tile Layer - 在非编辑模式下无用
        int num = static_cast<int>(gm::buffer_read_uint32(buffer));
        for (int i = 0; i < num; ++i)
        {
            gm::buffer_read_int32(buffer);
            gm::buffer_read_string(buffer);
        }

        std::vector<int> resList;
        std::vector<bool> resExistsList;
        std::string err = "";

        // Tiles
        num = static_cast<int>(gm::buffer_read_uint32(buffer));
        resList.reserve(num);
        resExistsList.reserve(num);

        for (int i = 0; i < num; ++i)
        {
            std::string name = gm::buffer_read_string(buffer);
            int back = static_cast<int>(GetResource(name));
            resList.push_back(back);

            if (!gm::background_exists(back))
            {
                err += "在 scrLoadRoomTiles() 中，背景 (" + name + ") 不存在。\n";
                resExistsList.push_back(false);
            }
            else
                resExistsList.push_back(true);
        }

        num = static_cast<int>(gm::buffer_read_uint32(buffer));
        for (int i = 0; i < num; ++i)
        {
            int pos = static_cast<int>(gm::buffer_read_int32(buffer));
            if (!resExistsList[pos])
            {
                gm::buffer_set_pos(buffer, gm::buffer_get_pos(buffer) + 9 * 4 + 1);
                continue;
            }

            int left = static_cast<int>(gm::buffer_read_int32(buffer));
            int top = static_cast<int>(gm::buffer_read_int32(buffer));
            int width = static_cast<int>(gm::buffer_read_int32(buffer));
            int height = static_cast<int>(gm::buffer_read_int32(buffer));
            GMReal x = gm::buffer_read_int32(buffer);
            GMReal y = gm::buffer_read_int32(buffer);
            int depth = static_cast<int>(gm::buffer_read_int32(buffer));
            GMReal xscale = gm::buffer_read_float32(buffer);
            GMReal yscale = gm::buffer_read_float32(buffer);

            int tile = gm::tile_add(resList[pos], left, top, width, height, x, y, depth);
            gm::tile_set_scale(tile, xscale, yscale);
            gm::tile_set_alpha(tile, gm::buffer_read_uint8(buffer) / 255);
        }

        resList.clear();
        resExistsList.clear();

        // Sprites
        num = static_cast<int>(gm::buffer_read_uint32(buffer));
        resList.reserve(num);
        resExistsList.reserve(num);

        for (int i = 0; i < num; ++i)
        {
            std::string name = gm::buffer_read_string(buffer);
            int spr = static_cast<int>(GetResource(name));
            resList.push_back(spr);

            if (!gm::sprite_exists(spr))
            {
                err += "在 scrLoadRoomTiles() 中，Sprite (" + name + ") 不存在。\n";
                resExistsList.push_back(false);
            }
            else
                resExistsList.push_back(true);
        }

        num = static_cast<int>(gm::buffer_read_uint32(buffer));
        for (int i = 0; i < num; ++i)
        {
            int pos = static_cast<int>(gm::buffer_read_int32(buffer));
            if (!resExistsList[pos])
            {
                gm::buffer_set_pos(buffer, gm::buffer_get_pos(buffer) + 8 * 4 + 2);
                continue;
            }

            int map = gm::ds_map_create();
            gm::ds_map_add(map, "sprite", resList[pos]);
            gm::ds_map_add(map, "scrollX", gm::buffer_read_float32(buffer));
            gm::ds_map_add(map, "scrollY", gm::buffer_read_float32(buffer));
            gm::ds_map_add(map, "curIndex", gm::buffer_read_int32(buffer));
            gm::ds_map_add(map, "x", gm::buffer_read_int32(buffer));
            gm::ds_map_add(map, "y", gm::buffer_read_int32(buffer));
            gm::ds_map_add(map, "xscale", gm::buffer_read_float32(buffer));
            gm::ds_map_add(map, "yscale", gm::buffer_read_float32(buffer));
            gm::ds_map_add(map, "alpha", gm::buffer_read_uint8(buffer) / 255);
            gm::ds_map_add(map, "speed", gm::buffer_read_float32(buffer));

            if (gm::ds_map_find_value(map, "speed") != 0)
                gm::ds_map_replace(map, "curIndex", 0);

            int listPos = static_cast<int>(gm::buffer_read_uint8(buffer));
            if (listPos > 5)
            {
                err += "在 scrLoadRoomTiles() 中，层 " + std::to_string(listPos) + " 不存在。\n";
                continue;
            }

            gm::ds_list_add(DrawSpritesList[listPos], map);
        }

        gm::buffer_destroy(buffer);

        if (err != "")
            throw std::wstring(err.begin(), err.end()).c_str();

        finish;
    }
    simplecatch(L"scrLoadRoomTiles()", 0)
}

#pragma endregion

#pragma region Load Text Resources

static void StringReplaceAll(std::string& str, const std::string& from, const std::string& to)
{
    if (from.empty()) return; // 避免空子串导致死循环

    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

expReal InitTexts(GMString path)
{
    using namespace std;
    namespace fs = filesystem;

    try
    {
        if (!fs::exists(path))
        {
            string errpath = path;
            wstring err = L"文件夹路径 (" + wstring(errpath.begin(), errpath.end()) + L") 不存在。";
            throw err.c_str();
        }

        vector<fs::path> textFilePath;

        if (fs::is_regular_file(path))  // 读取指定的文件
            textFilePath.push_back(path);
        else
        {
            // 读取文件夹（及其子文件夹）下所有的 .txt 文件
            for (const auto& entry : fs::recursive_directory_iterator(path))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".txt")
                    textFilePath.push_back(entry.path().lexically_normal());
            }
        }

        for (auto& file : textFilePath)
        {
            ifstream filestream(file);
            if (!filestream)
            {
                wstring err = L"文件 (" + wstring(file) + L") 打开失败。";
                throw err.c_str();
            }

            // 将整个文件都读取到字符串中，减少 I/O 调用带来的性能开销
            string data = {
                istreambuf_iterator<char>(filestream),
                istreambuf_iterator<char>()
            };

            istringstream strstream(data);
            string line, code = "";

            while (getline(strstream, line))
            {
                // 去掉前面的空格和制表符
                size_t whitePos = line.find_first_not_of(" \t");
                line = (whitePos == string::npos) ? "" : line.substr(whitePos);

                if (line == "")
                    continue;
                
                size_t commentPos = line.find("//");
                if (commentPos == 0)
                    continue;
                else if (commentPos != string::npos)
                    line = line.substr(0U, commentPos);
                
                size_t regionPos = line.find("#region");
                if (regionPos >= 0 && regionPos != string::npos)
                    continue;
                
                size_t regionEndPos = line.find("#end");
                if (regionEndPos >= 0 && regionEndPos != string::npos)
                    continue;
                
                size_t namedPos = line.find_first_of('[');
                if (namedPos == string::npos)
                {
                    namedPos = line.find_first_of('=');
                    if (namedPos == string::npos)
                        continue;
                }

                string name = line.substr(0U, namedPos);

                StringReplaceAll(line, "#", "\n");
                StringReplaceAll(line, "''", "\" + chr(34) + \"");

                code += "globalvar " + name + "; " + line + "\n";
            }

            gm::execute_string(code);
        }

        finish;
    }
    simplecatch(L"InitTexts", 0)
}

#pragma endregion

#pragma region High Resolution Timer

ULONGLONG frequency = 1;

expReal TimerInit()
{
    if (QueryPerformanceFrequency((LARGE_INTEGER*)&frequency))
        finish;

    fail;
}

expReal TimerGet()
{
    ULONGLONG time = 0;
    if (QueryPerformanceCounter((LARGE_INTEGER*)&time))
        return (double)time / (double)frequency;
    
    return -1;
}

#pragma endregion

#pragma region IO
HKEY RegistryRoot;

expReal RegistrySetRoot(GMReal root)
{
    switch ((int)root)
    {
    case 0: RegistryRoot = HKEY_CURRENT_USER; finish;
    case 1: RegistryRoot = HKEY_LOCAL_MACHINE; finish;
    case 2: RegistryRoot = HKEY_CLASSES_ROOT; finish;
    case 3: RegistryRoot = HKEY_USERS; finish;
    }

    fail;
}

expReal RegistryDeleteKey(GMString name, GMString key)
{
    try
    {
        HKEY hkey;
        std::wstring subKey = ToWstring(name);
        std::wstring valueName = ToWstring(key);

        long result = RegOpenKeyEx(RegistryRoot, subKey.c_str(), 0, KEY_WRITE, &hkey);
        if (result != ERROR_SUCCESS)
        {
            if (result == ERROR_FILE_NOT_FOUND)
            {
                std::wstring err = L"注册表路径不存在: " + subKey;
                throw err.c_str();
            }

            std::wstring err = L"打开注册表失败 (错误代码: " + std::to_wstring(result) + L")";
            throw err.c_str();
        }

        result = RegDeleteValue(hkey, valueName.c_str());
        if (result != ERROR_SUCCESS)
        {
            if (result == ERROR_FILE_NOT_FOUND)
            {
                std::wstring err = L"值不存在: " + valueName;
                throw err.c_str();
            }

            std::wstring err = L"删除失败 (错误代码: " + std::to_wstring(result) + L")";
            throw err.c_str();
        }

        RegCloseKey(hkey);
        finish;
    }
    simplecatch(L"RegistryDeleteKey", 0)
}

std::vector<GMString> MatchedFiles;

expReal GetAllFilesInSubfolders(GMString dir, GMString starchPattern)
{
    namespace fs = std::filesystem;

    try
    {
        MatchedFiles.clear();

        fs::path directory(dir);
        std::string pattern(starchPattern);

        pattern = pattern.empty() ? "*" : pattern;

        // 转换通配符为正则表达式
        std::string regexPattern;
        regexPattern.reserve(pattern.size() * 2);

        for (char c : pattern)
        {
            switch (c)
            {
            case '*':   regexPattern += ".*";   break;
            case '?':   regexPattern += '.';    break;
            case '.':   regexPattern += "\\.";  break;
            case '\\':  regexPattern += "\\\\"; break;
            default:
                if (std::isalnum(static_cast<unsigned char>(c)))
                    regexPattern += c;
                else
                    regexPattern += '\\', regexPattern += c;
                break;
            }
        }

        std::regex regex = std::regex(regexPattern, std::regex_constants::icase);

        for (const auto& entry : fs::recursive_directory_iterator(
            directory, fs::directory_options::skip_permission_denied))
        {
            if (entry.is_regular_file())
            {
                std::string filename = entry.path().filename().string();

                if (std::regex_match(filename, regex))
                    MatchedFiles.push_back(string_to_char(entry.path().string()));
            }
        }

        return MatchedFiles.size();
    }
    catch (const std::regex_error&)
    {
        if (show_error)
        {
            MessageBox(GMWindowsHandle, L"在执行函数 GetAllFilesInSubfolders 时抛出异常。\n无效的通配符。", 
                L"NatureEnhance Error", MB_OK | MB_ICONERROR);
        }
        
        return -1;
    }
    catch (const fs::filesystem_error&)
    {
        if (show_error)
        {
            MessageBox(GMWindowsHandle, L"在执行函数 GetAllFilesInSubfolders 时抛出异常。\n文件系统错误。",
                L"NatureEnhance Error", MB_OK | MB_ICONERROR);
        }

        return -1;
    }
    simplecatch(L"GetAllFilesInSubfolders", -1)
}

expString GetAllFilesDir(GMReal num)
{
    if (num < 0 || num > MatchedFiles.size() - 1)
        return "";

    return MatchedFiles[(int)num];
}

expString ReadAllText(GMString file)
{
    try
    {
        std::ifstream filestream(file, std::ios::binary);
        if (!filestream)
        {
            std::wstring err = L"文件 (" + std::wstring(std::filesystem::path(file)) +
                L") 打开失败。";
            throw err.c_str();
        }

        std::string data = {
            std::istreambuf_iterator<char>(filestream),
            std::istreambuf_iterator<char>()
        };

        return string_to_char(data);
    }
    simplecatch(L"ReadAllText", "")
}

expReal FileIsUsing(GMString file)
{
    HANDLE hFile = CreateFileA(
        file,
        GENERIC_READ | GENERIC_WRITE,
        0,  // 独占模式打开
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        // 共享冲突或拒绝访问表示文件被占用
        return (error == ERROR_SHARING_VIOLATION || error == ERROR_ACCESS_DENIED);
    }

    CloseHandle(hFile);
    return false;  // 文件未被占用
}
#pragma endregion

#pragma region Buffer & Surface
inline void D3DCheck(HRESULT result, int pos)
{
    if (SUCCEEDED(result))
        return;

    std::wstring err = L"位置" + std::to_wstring(pos) + L": " + DXGetErrorDescription8(result);
    throw err.c_str();
}

expReal TextureToBuffer(GMReal buffer, GMReal gmtex, GMReal w, GMReal h)
{
    try
    {
        UINT width = (UINT)w, height = (UINT)h;

        IDirect3DTexture8* texture = gm::CGMAPI::GetTextureArray()[(int)gmtex].texture;
        if (Device == nullptr)
            D3DCheck(texture->GetDevice(&Device), 1);

        IDirect3DSurface8* surf = nullptr;
        IDirect3DSurface8* surfTemp = nullptr;
        D3DCheck(texture->GetSurfaceLevel(0, &surf), 2);

        // 因为 GameMaker 的纹理被设置为 D3DPOOL_DEFAULT，不能直接读取数据信息
        // 所以要创建一个额外的 IDirect3DSurface8，将里面的数据复制过来
        D3DCheck(Device->CreateImageSurface(width, height, D3DFMT_A8R8G8B8, &surfTemp), 3);
        D3DCheck(D3DXLoadSurfaceFromSurface(surfTemp, nullptr, nullptr, surf, nullptr,
            nullptr, D3DX_FILTER_NONE, 0), 4);

        // 获取纹理数据信息
        D3DLOCKED_RECT lock;
        D3DCheck(surfTemp->LockRect(&lock, nullptr, 0), 5);
        char* src = (char*)lock.pBits;

        // 传入的 buffer 直接操作其指向的内存，提高效率
        if (!gm::buffer_exists(buffer))
            throw L"传入无效的 buffer 引用。";

        gm::buffer_set_size(buffer, width * height * 4);
        gm::buffer_set_pos(buffer, 0);
        char* dest = (char*)(int)gm::buffer_get_address(buffer, false);

        if (dest == nullptr)
            throw L"传入无效的 buffer 引用。";

        // 复制纹理信息到 buffer 中。因为该纹理是非压缩纹理，所以要按行复制
        UINT srcPos = 0, destPos = 0, bufferStride = width * 4;
        for (UINT i = 0; i < height; ++i)
        {
            memcpy(&dest[destPos], &src[srcPos], bufferStride);
            srcPos += lock.Pitch;
            destPos += bufferStride;
        }

        // 结束，释放内存
        D3DCheck(surfTemp->UnlockRect(), 6);
        surfTemp->Release();
        surf->Release();

        finish;
    }
    simplecatch(L"TextureToBuffer", 0.0)
}

expReal SurfaceToBuffer(GMReal buffer, GMReal surface)
{
    int gmtex = gm::surface_get_texture((int)surface);
    int width = gm::surface_get_width((int)surface);
    int height = gm::surface_get_height((int)surface);

    return TextureToBuffer(buffer, gmtex, width, height);
}

expReal BufferToTexture(GMReal buffer, GMReal gmtex, GMReal w, GMReal h)
{
    try
    {
        UINT width = (UINT)w, height = (UINT)h;
        char* src = (char*)(int)gm::buffer_get_address(buffer, false);
        if (src == nullptr)
            throw L"传入无效的 buffer 引用。";

        IDirect3DTexture8* texture = gm::CGMAPI::GetTextureArray()[(int)gmtex].texture;

        IDirect3DSurface8* surf = nullptr;
        D3DCheck(texture->GetSurfaceLevel(0, &surf), 1);

        RECT rect = { .left = 0, .top = 0, .right = (long)width, .bottom = (long)height };

        D3DCheck(D3DXLoadSurfaceFromMemory(surf, nullptr, &rect, src, D3DFMT_A8R8G8B8,
            width * 4, nullptr, &rect, D3DX_FILTER_NONE, 0), 2);

        D3DCheck(texture->AddDirtyRect(&rect), 3);
        surf->Release();

        finish;
    }
    simplecatch(L"BufferToTexture", 0.0)
}

expReal BufferToSurface(GMReal buffer, GMReal surface)
{
    char* src = (char*)(int)gm::buffer_get_address(buffer, false);

    int gmtex = gm::surface_get_texture((int)surface);
    int width = gm::surface_get_width((int)surface);
    int height = gm::surface_get_height((int)surface);

    return BufferToTexture(buffer, gmtex, width, height);
}

expReal ARGBGetColor(GMReal color) { return ((UINT)color) & 0x00ffffff; }
expReal ARGBGetAlpha(GMReal color)
{
    return ((double)((((UINT)color) & 0xff000000) >> 24)) / 0xff;
}

#pragma endregion
