#include "Main.h"
#include <locale>
#include <format>
#include <vector>
#include <charconv>
#include "utf8.h"
#include "FoxWriting.h"
#include "linebreak.h"

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

	return result;
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

	return string_to_cstr(std::string(start_it, fin_it));
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
		return string_to_cstr(std::string(begin_it, next_it));
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

	return string_to_cstr(result);
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

	return string_to_cstr(result);
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

	return string_to_cstr(timeString);
}

expString GetString(GMReal num, GMString format, GMString country)
{
	try
	{
		if (*format == '\0' && *country == '\0')
		{
			return string_to_cstr(std::format("{}", num));
		}
		else if (*country == '\0')
		{
			return string_to_cstr(std::vformat("{:" + std::string(format) + "}", 
				std::make_format_args(num)));
		}
		else
		{
			return string_to_cstr(std::vformat(std::locale(country), 
				"{0:" + std::string(format) + "}", std::make_format_args(num)));
		}
	}
	simplecatch(L"GetString", "")
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

	return string_to_cstr(StringTokenResult[(int)num]);
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

std::string string_get_ext(GMString str, GMReal w, GMString lang)
{
	if (w <= 0 || *str == '\0')
		return "In function gui_get_string_ext(): The argument is valid.";

	GMString l = lang;
	if (lang != nullptr && *lang == '\0')
		l = nullptr;

	// 获取指定字符串的“可合法断点”列表
	size_t len = strlen(str) + 1;
	std::vector<char> brks(len);
	set_linebreaks_utf8((const utf8_t*)str, len, l, brks.data());

	// 按 utf-8 字符分隔字符串
	size_t num = (size_t)StringToken(str, "", false);

	std::string token,			// 从上一个合法断点到当前处理字符的字符串
		line,			// 从当前行开始到上一个合法断点的字符串
		result;			// 结果字符串

	// 计算要绘制的字符宽度并自动断行
	size_t brk_pos = 0;
	for (size_t i = 0; i < num; i++)
	{
		// 将基于字节的“可合法断点”列表由基于字符的模式读取
		size_t chr_len = StringTokenResult[i].length();
		if (chr_len == 0)
		{
			return "In function gui_get_string_ext():"
				"An Error has occurred in function StringToken().";
		}

		brk_pos += chr_len;
		char br = brks[brk_pos - 1];

		token += StringTokenResult[i];

		// 遇到库标记出错（断在字符内部）
		if (br == LINEBREAK_INSIDEACHAR)
		{
			return "In function gui_get_string_ext(): "
				"An Error has occurred in character position ("
				+ std::to_string(i) + " - " + std::to_string(i + 1) + ").";
		}

		// 当到达可断点、必须断点或字符串末尾时处理 token
		if (br == LINEBREAK_ALLOWBREAK || br == LINEBREAK_MUSTBREAK || i == num - 1)
		{
			if (br == LINEBREAK_MUSTBREAK)
			{
				result += line + token;

				// 若 token 没有显式的换行符，加上换行符
				if (!token.empty() && token.back() != '\n' && token.back() != '\r')
					result += "\n";

				line.clear();
				token.clear();
			}
			else  // ALLOWBREAK 或到达字符串末尾
			{
				// 计算合并后的宽度
				std::string candidate = line + token;
				double width = fw::string_width(candidate.c_str());

				if (width <= w)  // 若放得下，合并到当前行
					line = std::move(candidate);
				else  // 若放不下，需要把当前行写出并开始新行
				{
					if (!line.empty())
					{
						result += line + "\n";
						line = token;
					}
					else  // 当前行为空，即单个 token 超过宽度的情形
					{
						result += token;
						if (i != num - 1)
							result += "\n";

						line.clear();
					}
				}

				token.clear();
			}
		}
	}

	// 将残余内容加入结果（如果有）
	if (!line.empty())
		result += line;
	else if (!token.empty())
		result += token;

	return std::move(result);
}

expString StringGetExt(GMString str, GMReal w, GMString lang)
{
	std::string result = string_get_ext(str, w, lang);
	return string_to_cstr(result);
}