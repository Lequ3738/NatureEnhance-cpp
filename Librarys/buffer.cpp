#include "buffer.h"

HMODULE BufferDLL = nullptr;

namespace gm
{
	r_v buffer_create;
	r_r buffer_destroy, buffer_exists, buffer_get_pos, buffer_get_size;
	r_r buffer_at_end, buffer_get_error, buffer_clear_error, buffer_clear;
	r_r buffer_zlib_compress, buffer_zlib_uncompress, buffer_read_int8;
	r_r buffer_read_uint8, buffer_read_int16, buffer_read_uint16;
	r_r buffer_read_int32, buffer_read_uint32, buffer_read_int64, buffer_read_uint64;
	r_r buffer_read_intv, buffer_read_uintv, buffer_read_float32, buffer_read_float64;
	s_r buffer_to_string, buffer_read_string;
	r_rr buffer_set_pos, buffer_rc4_crypt_buffer, buffer_write_int8, buffer_write_uint8;
	r_rr buffer_write_int16, buffer_write_uint16, buffer_write_int32;
	r_rr buffer_write_uint32, buffer_write_int64, buffer_write_uint64;
	r_rr buffer_write_intv, buffer_write_uintv, buffer_write_float32, buffer_write_float64;
	r_rr buffer_write_buffer, buffer_get_address, buffer_set_size;
	r_rs buffer_read_from_file, buffer_write_to_file, buffer_append_to_file;
	r_rs buffer_rc4_crypt, buffer_write_string, buffer_write_data, buffer_write_hex;
	r_rsrr buffer_read_from_file_part;
	s_rr buffer_read_data, buffer_read_hex;
	r_rrrr buffer_write_buffer_part;
}

#define load(var, name)											\
	var = (decltype(var))GetProcAddress(BufferDLL, name);		\
	if (var == nullptr) {										\
		throw std::runtime_error("加载 Http.dll 时获取函数 ( " +	\
			std::string(name) + ") 失败。");						\
	}

expReal ImportBufferModule(GMString name)
{
	try
	{
		std::wstring wname(name, name + strlen(name));
		BufferDLL = GetModuleHandle(wname.c_str());

		if (BufferDLL == nullptr)
			throw std::runtime_error("加载 " + std::string(name) + " 失败。");

		load(gm::buffer_create, "buffer_create");
		load(gm::buffer_destroy, "buffer_destroy");
		load(gm::buffer_exists, "buffer_exists");
		load(gm::buffer_to_string, "buffer_to_string");
		load(gm::buffer_get_pos, "buffer_get_pos");
		load(gm::buffer_get_size, "buffer_get_length");
		load(gm::buffer_at_end, "buffer_at_end");
		load(gm::buffer_get_error, "buffer_get_error");
		load(gm::buffer_clear_error, "buffer_clear_error");
		load(gm::buffer_clear, "buffer_clear");
		load(gm::buffer_set_pos, "buffer_set_pos");
		load(gm::buffer_read_from_file, "buffer_read_from_file");
		load(gm::buffer_read_from_file_part, "buffer_read_from_file_part");
		load(gm::buffer_write_to_file, "buffer_write_to_file");
		load(gm::buffer_append_to_file, "buffer_append_to_file");
		load(gm::buffer_rc4_crypt, "buffer_rc4_crypt");
		load(gm::buffer_rc4_crypt_buffer, "buffer_rc4_crypt_buffer");
		load(gm::buffer_zlib_compress, "buffer_zlib_compress");
		load(gm::buffer_zlib_uncompress, "buffer_zlib_uncompress");
		load(gm::buffer_read_int8, "buffer_read_int8");
		load(gm::buffer_read_uint8, "buffer_read_uint8");
		load(gm::buffer_read_int16, "buffer_read_int16");
		load(gm::buffer_read_uint16, "buffer_read_uint16");
		load(gm::buffer_read_int32, "buffer_read_int32");
		load(gm::buffer_read_uint32, "buffer_read_uint32");
		load(gm::buffer_read_int64, "buffer_read_int64");
		load(gm::buffer_read_uint64, "buffer_read_uint64");
		load(gm::buffer_read_intv, "buffer_read_intv");
		load(gm::buffer_read_uintv, "buffer_read_uintv");
		load(gm::buffer_read_float32, "buffer_read_float32");
		load(gm::buffer_read_float64, "buffer_read_float64");
		load(gm::buffer_write_int8, "buffer_write_int8");
		load(gm::buffer_write_uint8, "buffer_write_uint8");
		load(gm::buffer_write_int16, "buffer_write_int16");
		load(gm::buffer_write_uint16, "buffer_write_uint16");
		load(gm::buffer_write_int32, "buffer_write_int32");
		load(gm::buffer_write_uint32, "buffer_write_uint32");
		load(gm::buffer_write_int64, "buffer_write_int64");
		load(gm::buffer_write_uint64, "buffer_write_uint64");
		load(gm::buffer_write_intv, "buffer_write_intv");
		load(gm::buffer_write_uintv, "buffer_write_uintv");
		load(gm::buffer_write_float32, "buffer_write_float32");
		load(gm::buffer_write_float64, "buffer_write_float64");
		load(gm::buffer_write_string, "buffer_write_string");
		load(gm::buffer_read_string, "buffer_read_string");
		load(gm::buffer_write_data, "buffer_write_data");
		load(gm::buffer_read_data, "buffer_read_data");
		load(gm::buffer_write_hex, "buffer_write_hex");
		load(gm::buffer_read_hex, "buffer_read_hex");
		load(gm::buffer_write_buffer, "buffer_write_buffer");
		load(gm::buffer_write_buffer_part, "buffer_write_buffer_part");
		load(gm::buffer_get_address, "buffer_get_address");
		load(gm::buffer_set_size, "buffer_set_size");

		finish;
	}
	simplecatch("ImportBufferModule", 0);
}

void gm::buffer_write(int id, int type, dynamic value)
{
	switch (type)
	{
	case buffer_int8:
		gm::buffer_write_int8(id, std::get<GMReal>(value));
		break;
	case buffer_int16:
		gm::buffer_write_int16(id, std::get<GMReal>(value));
		break;
	case buffer_int32:
		gm::buffer_write_int32(id, std::get<GMReal>(value));
		break;
	case buffer_int64:
		gm::buffer_write_int64(id, std::get<GMReal>(value));
		break;
	case buffer_uint8:
		gm::buffer_write_uint8(id, std::get<GMReal>(value));
		break;
	case buffer_uint16:
		gm::buffer_write_uint16(id, std::get<GMReal>(value));
		break;
	case buffer_uint32:
		gm::buffer_write_uint32(id, std::get<GMReal>(value));
		break;
	case buffer_uint64:
		gm::buffer_write_uint64(id, std::get<GMReal>(value));
		break;

	case buffer_intv:
		gm::buffer_write_intv(id, std::get<GMReal>(value));
		break;
	case buffer_uintv:
		gm::buffer_write_uintv(id, std::get<GMReal>(value));
		break;

	case buffer_float32:
		gm::buffer_write_float32(id, std::get<GMReal>(value));
		break;
	case buffer_float64:
		gm::buffer_write_float64(id, std::get<GMReal>(value));
		break;

	case buffer_string:
		gm::buffer_write_string(id, std::get<std::string>(value).c_str());
		break;
	case buffer_string_part:
		gm::buffer_write_data(id, std::get<std::string>(value).c_str());
		break;
	case buffer_hex:
		gm::buffer_write_hex(id, std::get<std::string>(value).c_str());
		break;
	}
}

dynamic gm::buffer_read(int id, int type, int len)
{
	switch (type)
	{
		case buffer_int8:			return gm::buffer_read_int8(id);
		case buffer_int16:			return gm::buffer_read_int16(id);
		case buffer_int32:			return gm::buffer_read_int32(id);
		case buffer_int64:			return gm::buffer_read_int64(id);
		case buffer_uint8:			return gm::buffer_read_uint8(id);
		case buffer_uint16:			return gm::buffer_read_uint16(id);
		case buffer_uint32:			return gm::buffer_read_uint32(id);
		case buffer_uint64:			return gm::buffer_read_uint64(id);

		case buffer_intv:			return gm::buffer_read_intv(id);
		case buffer_uintv:			return gm::buffer_read_uintv(id);

		case buffer_float32:		return gm::buffer_read_float32(id);
		case buffer_float64:		return gm::buffer_read_float64(id);

		case buffer_string:			return gm::buffer_read_string(id);
		case buffer_string_part:	return gm::buffer_read_data(id, len);
		case buffer_hex:			return gm::buffer_read_hex(id, len);
	}

	return -4.0;
}

void gm::buffer_jump(GMReal id, int offset)
{
	gm::buffer_set_pos(id, gm::buffer_get_pos(id) + offset);
}