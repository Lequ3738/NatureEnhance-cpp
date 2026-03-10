#include "Main.h"
#include <locale>
#include <format>
#include <vector>
#include <charconv>
#include "utf8.h"

GMString string_to_cstr(const std::string& str)
{
	size_t len = str.size() + 1;
	char* nstr = new char[len];
	strcpy_s(nstr, len, str.c_str());

	return nstr;
}

GMString STRCPY(GMString str)
{
	if (*str == '\0')
		return "";
	size_t len = strlen(str) + 1;
	char* nstr = new char[len];
	strcpy_s(nstr, len, str);
	return nstr;
}

expString StringChangeCoding(GMString str, GMString in, GMString out)
{
	char* input = toUpperAscii(in);
	char* output = toUpperAscii(out);

	GMString result = ChangeCoding(str, input, output);
	if (result == nullptr)
	{
		if (show_error)
		{
			std::wstring errorMsg = L"函数 StringChangeCoding 出现编码转换错误：\n"
				L"请检查输入编码和输出编码是否正确。";
			MessageBox(GMWindowsHandle, errorMsg.c_str(), L"NatureEnhance Error",
				MB_OK | MB_ICONERROR);
		}
		return "";
	}

	GMReturnString = result;
	delete[] result;

	return GMReturnString.c_str();
}

#define utf8catch(funcname, returns) \
	catch (const utf8::exception& ex) \
	{ \
		if (show_error) \
		{ \
			std::string what(ex.what());\
			std::wstring err = L"在执行函数 " + std::wstring(funcname) + L" 时抛出异常。" + \
				L"输入不合法的 UTF-8 字符串。" + std::wstring(what.begin(), what.end()); \
			MessageBox(GMWindowsHandle, err.c_str(), L"NatureEnhance Error", MB_OK | MB_ICONERROR); \
		} \
		return returns; \
	} 

expReal StringLength(GMString str)
{
	try
	{
		std::string string(str);
		return utf8::distance(string.begin(), string.end());
	}
	utf8catch(L"StringLength", 0);
}

expReal StringPos(GMString substr, GMString str)
{
	if (*substr == '\0' || *str == '\0')
		return 0;

	std::string substring(substr);
	std::string string(str);

	size_t byte_pos = string.find(substring);
	if (byte_pos == std::string::npos)
		return 0;

	auto start_it = string.begin();
	auto target_it = start_it + byte_pos;

	try
	{
		return utf8::distance(start_it, target_it) + 1;
	}
	utf8catch(L"StringPos", 0);
}

expString StringCopy(GMString str, GMReal index, GMReal count)
{
	if (*str == '\0')
		return "";

	std::string string(str);

	auto begin_it = string.begin();
	auto end_it = string.end();

	auto start_it = begin_it;
	try
	{
		utf8::advance(start_it, max(0, (size_t)index - 1), end_it);
	}
	catch (const utf8::not_enough_room&)
	{
		return "";
	}
	utf8catch(L"StringCopy", "");

	auto fin_it = start_it;
	try
	{
		utf8::advance(fin_it, (size_t)count, end_it);
	}
	catch (const utf8::not_enough_room&)
	{
		fin_it = end_it;  // 长度超出范围，截取到字符串末尾
	}
	utf8catch(L"StringCopy", "");

	GMReturnString = std::string(start_it, fin_it);
	return GMReturnString.c_str();
}

expString StringCharAt(GMString str, GMReal index)
{
	if (*str == '\0')
		return "";

	std::string string(str);

	auto begin_it = string.begin();
	auto end_it = string.end();

	try
	{
		utf8::advance(begin_it, max(0, (size_t)index - 1), end_it);
		auto next_it = begin_it;
		utf8::next(next_it, end_it);

		GMReturnString = std::string(begin_it, next_it);
		return GMReturnString.c_str();
	}
	catch (const utf8::not_enough_room&)
	{
		return "";
	}
	utf8catch(L"StringCharAt", "");
}

expString StringDelete(GMString str, GMReal index, GMReal count)
{
	if (*str == '\0')
		return "";

	std::string string(str);

	auto begin_it = string.begin();
	auto end_it = string.end();

	auto start_it = begin_it;
	try
	{
		utf8::advance(start_it, max(0, (size_t)index - 1), end_it);
	}
	catch (const utf8::not_enough_room&)
	{
		return str;
	}
	utf8catch(L"StringDelete", "");

	auto fin_it = start_it;
	try
	{
		utf8::advance(fin_it, (size_t)count, end_it);
	}
	catch (const utf8::not_enough_room&)
	{
		fin_it = end_it;  // 长度超出范围，截取到字符串末尾
	}
	utf8catch(L"StringDelete", "");

	std::string result;
	result.reserve(string.size());
	result.append(begin_it, start_it);
	result.append(fin_it, end_it);

	GMReturnString = std::move(result);
	return GMReturnString.c_str();
}

expString StringInsert(GMString substr, GMString str, GMReal index)
{
	if (*substr == '\0')
		return str;

	std::string substring(substr);
	std::string string(str);

	if (index <= 1)
		return string_to_cstr(substring + string);

	auto begin_it = string.begin();
	auto end_it = string.end();

	auto insert_it = begin_it;
	try
	{
		utf8::advance(insert_it, (size_t)index - 1, end_it);
	}
	catch (const utf8::not_enough_room&)
	{
		return string_to_cstr(string + substring);
	}
	utf8catch(L"StringInsert", "");

	std::string result;
	result.reserve(string.size() + substring.size());
	result.append(begin_it, insert_it);
	result.append(substring);
	result.append(insert_it, end_it);

	GMReturnString = std::move(result);
	return GMReturnString.c_str();
}

expString TimeString(GMReal time, GMReal bit)
{
	std::string timeString;

	if (bit >= 3)
	{
		timeString = std::to_string((int)floor(time / 3600)) + ":";
		time = fmod(time, 3600);
	}
	if (bit >= 2)
	{
		timeString += std::to_string((int)floor(time / 600));
		time = fmod(time, 600);
		timeString += std::to_string((int)floor(time / 60)) + ":";
		time = fmod(time, 60);
	}
	if (bit >= 1)
	{
		timeString += std::to_string((int)floor(time / 10));
		time = fmod(time, 10);
		timeString += std::to_string((int)floor(time));
	}

	GMReturnString = std::move(timeString);
	return GMReturnString.c_str();
}

expString GetString(GMReal num, GMString format, GMString country)
{
	try
	{
		if (*format == '\0' && *country == '\0')
		{
			GMReturnString = std::format("{}", num);
		}
		else if (*country == '\0')
		{
			GMReturnString = std::vformat("{:" + std::string(format) + "}",
				std::make_format_args(num));
		}
		else
		{
			GMReturnString = std::vformat(std::locale(country),
				"{0:" + std::string(format) + "}", std::make_format_args(num));
		}

		return GMReturnString.c_str();
	}
	simplecatch("GetString", "")
}

std::vector<std::string> StringTokenResult;

expReal StringToken(GMString text, GMString sep, GMReal dontRemoveEmpty)
{
	StringTokenResult.clear();

	if (*text == '\0')
	{
		if (dontRemoveEmpty > 0.5)
		{
			StringTokenResult.push_back("");
			return 1;
		}
		else
			return 0;
	}

	std::string stringText(text);

	auto start = stringText.begin();
	auto end = stringText.end();
	auto cur_it = start;

	try
	{
		if (*sep == '\0')  // 空分隔符：按每个字符分割
		{
			auto prev_it = start;

			while (cur_it != end)
			{
				utf8::next(cur_it, end);  // 获取下一个字符

				std::string character(prev_it, cur_it);
				if (!character.empty())
					StringTokenResult.push_back(character);

				prev_it = cur_it;
			}

			return StringTokenResult.size();
		}
	}
	utf8catch(L"StringToken", -1);

	std::string stringSep(sep);
	size_t pos = 0, found = 0;
	while ((found = stringText.find(stringSep, pos)) != std::string::npos)
	{
		std::string token = stringText.substr(pos, found - pos);
		pos = found + stringSep.length();

		if (!token.empty() || dontRemoveEmpty > 0.5)
			StringTokenResult.push_back(token);
	}

	if (pos <= stringText.length())
	{
		std::string token = stringText.substr(pos);
		if (!token.empty() || dontRemoveEmpty > 0.5)
			StringTokenResult.push_back(token);
	}

	return StringTokenResult.size();
}

expString StringGetToken(GMReal num)
{
	if (num < 0 || num > StringTokenResult.size() - 1)
		return "";

	return StringTokenResult[(int)num].c_str();
}

expReal StringTryParse(GMString str)
{
	if (*str == '\0')
		return 0;

	std::string sstr(str);

	size_t start = sstr.find_first_not_of(' ');
	if (start == std::string::npos)
		return 0;
	size_t end = sstr.find_last_not_of(' ');
	auto s = sstr.substr(start, end - start + 1);

	GMReal value;
	auto result = std::from_chars(s.data(), s.data() + s.size(), value);

	return result.ec == std::errc() && result.ptr == s.data() + s.size();
}

void StringReplaceAll(std::string& str, const std::string& from, const std::string& to)
{
	if (from.empty()) return; // 避免空子串导致死循环

	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos)
	{
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}
}

expString string_replace_all(GMString str, GMString from, GMString to)
{
	std::string result(str);
	StringReplaceAll(result, from, to);

	GMReturnString = std::move(result);
	return GMReturnString.c_str();
}