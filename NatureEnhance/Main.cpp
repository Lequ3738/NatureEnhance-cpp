#include "Main.h"
#include "buffer.h"
#include "DataStruct.h"
#include <filesystem>
#include <vector>

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
expReal GetGMWindowsHandle(GMReal handle)
{
    GMWindowsHandle = (HWND)(DWORD)handle;
    finish;
}

GMReal PropertyMap, NameMap;
expReal GetDSController(GMReal propertyMap, GMReal nameMap)
{
    PropertyMap = propertyMap;
    NameMap = nameMap;
    finish;
}
#pragma endregion

#pragma region Camera
GMReal CameraX, CameraY, ViewX, ViewY;
GMReal RoomWidth = 400, RoomHeight = 225, ViewWidth = 400, ViewHeight = 225;
GMReal Mode = 1, SnapDiv = 12, OffsetX = 24, OffsetY = -24, Factor = 0.16, MoveMode = 0;
GMReal LimitLeft = 0, LimitTop = 0, OldCameraX = 0, OldCameraY = 0, RegistryRoot = 0;

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
        GMReal num = gm::buffer_read_uint32(buffer);
        for (int i = 0; i < num; ++i)
        {
            gm::buffer_read_int32(buffer);
            gm::buffer_read_string(buffer);
        }

        std::vector<int> resList;
        std::vector<bool> resExistsList;
        std::string err = "";

        // Tiles
        num = gm::buffer_read_uint32(buffer);
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

        num = gm::buffer_read_uint32(buffer);
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
        num = gm::buffer_read_uint32(buffer);
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

        num = gm::buffer_read_uint32(buffer);
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