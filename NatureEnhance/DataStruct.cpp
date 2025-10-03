#include "buffer.h"
#include "DataStruct.h"

expReal ne_list_create(GMString info)
{
	int id = gm::ds_list_create();

	gm::ds_map_add((int)PropertyMap, id, ds_type_list);
	gm::ds_map_add((int)NameMap, id, info);

	return id;
}

expReal ne_list_destroy(GMReal id)
{
	try
	{
		if (!ds_exists(id, ds_type_list))
			throw L"试图销毁未从 ne_list_create() 创建的 ds_list。";

		int i = static_cast<int>(id);

		gm::ds_list_destroy(i);

		gm::ds_map_delete((int)PropertyMap, i);
		gm::ds_map_delete((int)NameMap, i);

		finish;
	}
	simplecatch(L"ne_list_destroy", 0)
}

expReal ne_map_create(GMString info)
{
	int id = gm::ds_map_create();

	gm::ds_map_add((int)PropertyMap, id + 100000000, ds_type_map);
	gm::ds_map_add((int)NameMap, id + 100000000, info);

	return id;
}

expReal ne_map_destroy(GMReal id)
{
	try
	{
		if (!ds_exists(id, ds_type_map))
			throw L"试图销毁未从 ne_map_create() 创建的 ds_map。";

		int i = static_cast<int>(id);

		gm::ds_map_destroy(i);

		gm::ds_map_delete((int)PropertyMap, i + 100000000);
		gm::ds_map_delete((int)NameMap, i + 100000000);

		finish;
	}
	simplecatch(L"ne_map_destroy", 0)
}

expReal ne_stack_create(GMString info)
{
	int id = gm::ds_stack_create();

	gm::ds_map_add((int)PropertyMap, id + 200000000, ds_type_stack);
	gm::ds_map_add((int)NameMap, id + 200000000, info);

	return id;
}

expReal ne_stack_destroy(GMReal id)
{
	try
	{
		if (!ds_exists(id, ds_type_stack))
			throw L"试图销毁未从 ne_stack_create() 创建的 ds_stack。";

		int i = static_cast<int>(id);

		gm::ds_stack_destroy(i);

		gm::ds_map_delete((int)PropertyMap, i + 200000000);
		gm::ds_map_delete((int)NameMap, i + 200000000);

		finish;
	}
	simplecatch(L"ne_stack_destroy", 0)
}

expReal ne_queue_create(GMString info)
{
	int id = gm::ds_queue_create();

	gm::ds_map_add((int)PropertyMap, id + 300000000, ds_type_queue);
	gm::ds_map_add((int)NameMap, id + 300000000, info);

	return id;
}

expReal ne_queue_destroy(GMReal id)
{
	try
	{
		if (!ds_exists(id, ds_type_queue))
			throw L"试图销毁未从 ne_queue_create() 创建的 ds_queue。";

		int i = static_cast<int>(id);

		gm::ds_queue_destroy(i);

		gm::ds_map_delete((int)PropertyMap, i + 300000000);
		gm::ds_map_delete((int)NameMap, i + 300000000);

		finish;
	}
	simplecatch(L"ne_queue_destroy", 0)
}

expReal ne_grid_create(GMReal w, GMReal h, GMString info)
{
	int id = gm::ds_grid_create(static_cast<int>(w), static_cast<int>(h));

	gm::ds_map_add((int)PropertyMap, id + 400000000, ds_type_grid);
	gm::ds_map_add((int)NameMap, id + 400000000, info);

	return id;
}

expReal ne_grid_destroy(GMReal id)
{
	try
	{
		if (!ds_exists(id, ds_type_grid))
			throw L"试图销毁未从 ne_grid_create() 创建的 ds_grid。";

		int i = static_cast<int>(id);

		gm::ds_grid_destroy(i);

		gm::ds_map_delete((int)PropertyMap, i + 400000000);
		gm::ds_map_delete((int)NameMap, i + 400000000);

		finish;
	}
	simplecatch(L"ne_queue_destroy", 0)
}

expReal ne_priority_create(GMString info)
{
	int id = gm::ds_priority_create();

	gm::ds_map_add((int)PropertyMap, id + 500000000, ds_type_priority);
	gm::ds_map_add((int)NameMap, id + 500000000, info);

	return id;
}

expReal ne_priority_destroy(GMReal id)
{
	try
	{
		if (!ds_exists(id, ds_type_priority))
			throw L"试图销毁未从 ne_priority_create() 创建的 ds_priority。";

		int i = static_cast<int>(id);

		gm::ds_priority_destroy(i);

		gm::ds_map_delete((int)PropertyMap, i + 500000000);
		gm::ds_map_delete((int)NameMap, i + 500000000);

		finish;
	}
	simplecatch(L"ne_priority_destroy", 0)
}

expReal ne_list_read_buffer(GMReal list, GMReal buffer, GMReal types)
{
	try
	{
		int listSize = static_cast<int>(gm::buffer_read_uint32(buffer));
		int typeSize = gm::ds_list_size((int)types);

		if (listSize % typeSize != 0)
			throw L"被 buffer 记载的 list 的大小不能被 types 的大小整除。";

		gm::ds_list_clear((int)list);
		for (int i = 0; i < listSize; ++i)
		{
			GMReal type = gm::ds_list_find_value((int)types, i % typeSize);
			dynamic value = gm::buffer_read((int)buffer, (int)type);

			if (std::holds_alternative<std::string>(value))
				gm::ds_list_add((int)list, std::get<std::string>(value));
			else
				gm::ds_list_add((int)list, std::get<GMReal>(value));
		}

		ne_list_destroy(types);
		finish;
	}
	simplecatch(L"ne_list_read_buffer", 0)
}

expReal ne_list_write_buffer(GMReal list, GMReal buffer, GMReal types)
{
	try
	{
		int listSize = gm::ds_list_size((int)list);
		int typeSize = gm::ds_list_size((int)types);

		if (listSize % typeSize != 0)
			throw L"传入的 list 的大小不能被 types 的大小整除。";

		gm::buffer_write_uint32(buffer, listSize);
		for (int i = 0; i < listSize; ++i)
		{
			GMReal type = gm::ds_list_find_value((int)types, i % typeSize);
			gm::CGMVariable value = gm::ds_list_find_value((int)list, i);

			if (value.IsString())
				gm::buffer_write((int)buffer, (int)type, value.c_str());
			else
				gm::buffer_write((int)buffer, (int)type, value.real());
		}

		ne_list_destroy(types);
		finish;
	}
	simplecatch(L"ne_list_write_buffer", 0)
}

expReal ds_exists(GMReal id, GMReal type)
{
	int i = static_cast<int>(id);

	if (type == ds_type_map)
		i += 100000000;
	else if (type == ds_type_stack)
		i += 200000000;
	else if (type == ds_type_queue)
		i += 300000000;
	else if (type == ds_type_grid)
		i += 400000000;
	else if (type == ds_type_priority)
		i += 500000000;

	if (!gm::ds_map_exists((int)PropertyMap, i))
		return 0;
	
	return gm::ds_map_find_value((int)PropertyMap, i) == type ? 1 : 0;
}

template <typename... Args>
int ne_add_list(Args... args)
{
	GMString info = "ne_add_list() Created list";
	int id = ne_list_create(info);
	(gm::ds_list_add(id, args), ...);
	return id;
}