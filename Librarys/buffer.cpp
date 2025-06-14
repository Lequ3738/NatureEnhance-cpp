#include "buffer.h"

HMODULE BufferDLL = nullptr;

namespace gm
{
	r_v buffer_create;
	r_r buffer_destroy, buffer_exists, buffer_get_pos, buffer_get_length;
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
	r_rr buffer_write_buffer;
	r_rs buffer_read_from_file, buffer_write_to_file, buffer_append_to_file;
	r_rs buffer_rc4_crypt, buffer_write_string, buffer_write_data, buffer_write_hex;
	r_rsrr buffer_read_from_file_part;
	s_rr buffer_read_data, buffer_read_hex;
	r_rrrr buffer_write_buffer_part;
}

#define load(var, name, type) \
	var = (type)GetProcAddress(BufferDLL, name); \
	if (var == nullptr) {\
		std::wstring wname(name, name + strlen(name)); \
		std::wstring err = L"加载 Http.dll 时获取函数 (" + wname + L") 失败。"; \
		throw err.c_str(); \
	}

expReal ImportBufferModule(GMString path)
{
	try
	{
		std::wstring wpath(path, path + strlen(path));
		BufferDLL = LoadLibrary(wpath.c_str());

		if (BufferDLL == nullptr)
		{
			std::wstring err = L"加载 " + wpath + L" 失败。";
			throw err.c_str();
		}

		load(gm::buffer_create, "buffer_create", r_v);
		load(gm::buffer_destroy, "buffer_destroy", r_r);
		load(gm::buffer_exists, "buffer_exists", r_r);
		load(gm::buffer_to_string, "buffer_to_string", s_r);
		load(gm::buffer_get_pos, "buffer_get_pos", r_r);
		load(gm::buffer_get_length, "buffer_get_length", r_r);
		load(gm::buffer_at_end, "buffer_at_end", r_r);
		load(gm::buffer_get_error, "buffer_get_error", r_r);
		load(gm::buffer_clear_error, "buffer_clear_error", r_r);
		load(gm::buffer_clear, "buffer_clear", r_r);
		load(gm::buffer_set_pos, "buffer_set_pos", r_rr);
		load(gm::buffer_read_from_file, "buffer_read_from_file", r_rs);
		load(gm::buffer_read_from_file_part, "buffer_read_from_file_part", r_rsrr);
		load(gm::buffer_write_to_file, "buffer_write_to_file", r_rs);
		load(gm::buffer_append_to_file, "buffer_append_to_file", r_rs);
		load(gm::buffer_rc4_crypt, "buffer_rc4_crypt", r_rs);
		load(gm::buffer_rc4_crypt_buffer, "buffer_rc4_crypt_buffer", r_rr);
		load(gm::buffer_zlib_compress, "buffer_zlib_compress", r_r);
		load(gm::buffer_zlib_uncompress, "buffer_zlib_uncompress", r_r);
		load(gm::buffer_read_int8, "buffer_read_int8", r_r);
		load(gm::buffer_read_uint8, "buffer_read_uint8", r_r);
		load(gm::buffer_read_int16, "buffer_read_int16", r_r);
		load(gm::buffer_read_uint16, "buffer_read_uint16", r_r);
		load(gm::buffer_read_int32, "buffer_read_int32", r_r);
		load(gm::buffer_read_uint32, "buffer_read_uint32", r_r);
		load(gm::buffer_read_int64, "buffer_read_int64", r_r);
		load(gm::buffer_read_uint64, "buffer_read_uint64", r_r);
		load(gm::buffer_read_intv, "buffer_read_intv", r_r);
		load(gm::buffer_read_uintv, "buffer_read_uintv", r_r);
		load(gm::buffer_read_float32, "buffer_read_float32", r_r);
		load(gm::buffer_read_float64, "buffer_read_float64", r_r);
		load(gm::buffer_write_int8, "buffer_write_int8", r_rr);
		load(gm::buffer_write_uint8, "buffer_write_uint8", r_rr);
		load(gm::buffer_write_int16, "buffer_write_int16", r_rr);
		load(gm::buffer_write_uint16, "buffer_write_uint16", r_rr);
		load(gm::buffer_write_int32, "buffer_write_int32", r_rr);
		load(gm::buffer_write_uint32, "buffer_write_uint32", r_rr);
		load(gm::buffer_write_int64, "buffer_write_int64", r_rr);
		load(gm::buffer_write_uint64, "buffer_write_uint64", r_rr);
		load(gm::buffer_write_intv, "buffer_write_intv", r_rr);
		load(gm::buffer_write_uintv, "buffer_write_uintv", r_rr);
		load(gm::buffer_write_float32, "buffer_write_float32", r_rr);
		load(gm::buffer_write_float64, "buffer_write_float64", r_rr);
		load(gm::buffer_write_string, "buffer_write_string", r_rs);
		load(gm::buffer_read_string, "buffer_read_string", s_r);
		load(gm::buffer_write_data, "buffer_write_data", r_rs);
		load(gm::buffer_read_data, "buffer_read_data", s_rr);
		load(gm::buffer_write_hex, "buffer_write_hex", r_rs);
		load(gm::buffer_read_hex, "buffer_read_hex", s_rr);
		load(gm::buffer_write_buffer, "buffer_write_buffer", r_rr);
		load(gm::buffer_write_buffer_part, "buffer_write_buffer_part", r_rrrr);

		finish;
	}
	simplecatch(L"ImportBufferModule", 0);
}

expReal FreeBufferModule()
{
	if (BufferDLL != nullptr)
	{
		FreeLibrary(BufferDLL);
		BufferDLL = nullptr;
	}

	gm::buffer_create = nullptr;
	gm::buffer_destroy = nullptr;
	gm::buffer_exists = nullptr;
	gm::buffer_get_pos = nullptr;
	gm::buffer_get_length = nullptr;
	gm::buffer_at_end = nullptr;
	gm::buffer_get_error = nullptr;
	gm::buffer_clear_error = nullptr;
	gm::buffer_clear = nullptr;
	gm::buffer_zlib_compress = nullptr;
	gm::buffer_zlib_uncompress = nullptr;
	gm::buffer_read_int8 = nullptr;
	gm::buffer_read_uint8 = nullptr;
	gm::buffer_read_int16 = nullptr;
	gm::buffer_read_uint16 = nullptr;
	gm::buffer_read_int32 = nullptr;
	gm::buffer_read_uint32 = nullptr;
	gm::buffer_read_int64 = nullptr;
	gm::buffer_read_uint64 = nullptr;
	gm::buffer_read_intv = nullptr;
	gm::buffer_read_uintv = nullptr;
	gm::buffer_read_float32 = nullptr;
	gm::buffer_read_float64 = nullptr;
	gm::buffer_to_string = nullptr;
	gm::buffer_read_string = nullptr;
	gm::buffer_set_pos = nullptr;
	gm::buffer_rc4_crypt_buffer = nullptr;
	gm::buffer_write_int8 = nullptr;
	gm::buffer_write_uint8 = nullptr;
	gm::buffer_write_int16 = nullptr;
	gm::buffer_write_uint16 = nullptr;
	gm::buffer_write_int32 = nullptr;
	gm::buffer_write_uint32 = nullptr;
	gm::buffer_write_int64 = nullptr;
	gm::buffer_write_uint64 = nullptr;
	gm::buffer_write_intv = nullptr;
	gm::buffer_write_uintv = nullptr;
	gm::buffer_write_float32 = nullptr;
	gm::buffer_write_float64 = nullptr;
	gm::buffer_write_buffer = nullptr;
	gm::buffer_read_from_file = nullptr;
	gm::buffer_write_to_file = nullptr;
	gm::buffer_append_to_file = nullptr;
	gm::buffer_rc4_crypt = nullptr;
	gm::buffer_write_string = nullptr;
	gm::buffer_write_data = nullptr;
	gm::buffer_write_hex = nullptr;
	gm::buffer_read_from_file_part = nullptr;
	gm::buffer_read_data = nullptr;
	gm::buffer_read_hex = nullptr;
	gm::buffer_write_buffer_part = nullptr;

	finish;
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
