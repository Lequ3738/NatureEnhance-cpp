#include "Main.h"
#include "json.hpp"
#include <stack>

#pragma region Error
fnReal ShowErrorMessage(GMReal mode)
{
	show_error = static_cast<bool>(mode);
	finish;
}
#pragma endregion

#pragma region Camera
fnReal CameraInit(GMReal mode, GMReal playerX, GMReal playerY, GMReal playerScale, GMReal limitLeft, 
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
    simplecatch("CameraInit");
}

fnReal CameraMove(GMReal playerX, GMReal playerY, GMReal playerScale)
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
    simplecatch("CameraMove");
}

fnReal CameraSetOffset(GMReal offsetX, GMReal offsetY)
{
    OffsetX = offsetX;
    OffsetY = offsetY;

    finish;
}

fnReal CameraSetMode(GMReal mode)
{
    Mode = mode;
    finish;
}

fnReal CameraSetSnapLevel(GMReal level)
{
    SnapDiv = level;
    finish;
}

fnReal CameraExport(GMReal type)
{
    if (type == 0)
        return ViewX;
    else
        return ViewY;
}

fnReal CameraSetView(GMReal cameraX, GMReal cameraY)
{
    OldCameraX = CameraX;
    OldCameraY = CameraY;

    CameraX = cameraX;
    CameraY = cameraY;

    finish;
}

fnReal CameraSetRelativeView(GMReal cameraX, GMReal cameraY)
{
    OldCameraX = CameraX;
    OldCameraY = CameraY;

    CameraX += cameraX;
    CameraY += cameraY;

    finish;
}

fnReal CameraSetStopFactor(GMReal factor)
{
    Factor = factor;
    finish;
}

fnReal CameraSetRoomSize(GMReal roomWidth, GMReal roomHeight)
{
    RoomWidth = roomWidth;
    RoomHeight = roomHeight;

    finish;
}

fnReal CameraSetLimit(GMReal limitLeft, GMReal limitTop)
{
    LimitLeft = limitLeft;
    LimitTop = limitTop;

    finish;
}

fnReal CameraGetSpeedDirection()
{
    return point_direction(CameraX, CameraY, OldCameraX, OldCameraY);
}

fnReal CameraGetSpeed()
{
    return point_distance(CameraX, CameraY, OldCameraX, OldCameraY);
}

fnReal CameraGetXSpeed()
{
    return CameraX - OldCameraX;
}

fnReal CameraGetYSpeed()
{
    return CameraY - OldCameraY;
}

fnReal CameraSetViewSize(GMReal viewWidth, GMReal viewHeight)
{
    ViewWidth = viewWidth;
    ViewHeight = viewHeight;

    return 1;
}
#pragma endregion

#pragma region Rope Calculate
static GMReal RopeArclength(GMReal a, GMReal b, GMReal x)
{
    auto temp1 = 2 * a * (x - b);
    auto temp2 = sqrt(temp1 * temp1 + 1);

    return temp2 * (x - b) / 2 - log(temp2 - temp1) / a / 4;
}

fnReal RopeSetAccuracy(GMReal iterations, GMReal steps)
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

fnReal DrawRope(GMReal x1, GMReal y1, GMReal x2, GMReal y2, GMReal length, GMReal back)
{
    GMReal width = 1, height = 1;
    if (back != gm::noone)
    {
        width = gm::background_get_width(back);
		height = gm::background_get_height(back);
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
            gm::draw_primitive_begin_texture(gm::pr_trianglestrip, gm::background_get_texture(back));

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
                gm::draw_primitive_begin_texture(gm::pr_trianglestrip, gm::background_get_texture(back));
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
            gm::draw_primitive_begin_texture(gm::pr_trianglestrip, gm::background_get_texture(back));
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

#pragma region Json
using json = nlohmann::json;
GMReal jsonMapDictionary, jsonListDictionary;

struct JsonStruct
{
    bool isMap;
    GMReal id;
};

fnReal JsonInit()
{
    jsonMapDictionary = gm::ds_map_create();
	jsonListDictionary = gm::ds_list_create();
}

fnReal JsonDecode(GMString jsonstr)
{
    json data = json::parse(std::string(jsonstr));

    std::stack<std::pair<json, JsonStruct>> stack;
    JsonStruct jsonStruct = { true, gm::ds_map_create() };
    stack.push({ data, jsonStruct });

    while (!stack.empty())
    {
        auto [current, _struct] = stack.top();
		stack.pop();

		if (current.is_object())
		{
			GMReal mapId = gm::ds_map_create();
			gm::ds_map_add(jsonMapDictionary, _struct.id, mapId);
			for (auto& [key, value] : current.items())
			{
				GMString keyStr = gm::string(key);
				GMReal valueId;
				if (value.is_object() || value.is_array())
				{
					valueId = gm::ds_map_create();
					stack.push({ value, { value.is_object(), valueId } });
				}
				else
				{
					valueId = gm::execute_string("return " + gm::string(value));
				}
				gm::ds_map_add(mapId, keyStr, valueId);
			}
		}
		else if (current.is_array())
		{
			GMReal listId = gm::ds_list_create();
			gm::ds_list_add(jsonListDictionary, listId);
			for (size_t i = 0; i < current.size(); ++i)
			{
				GMReal itemId;
				if (current[i].is_object() || current[i].is_array())
				{
					itemId = gm::ds_map_create();
					stack.push({ current[i], { current[i].is_object(), itemId } });
				}
				else
				{
					itemId = gm::execute_string("return " + gm::string(current[i]));
				}
				gm::ds_list_add(listId, itemId);
			}
		})
    }
}

#pragma endregion