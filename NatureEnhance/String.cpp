#include "Main.h"
#include <locale>
#include <format>
#include <vector>
#include <charconv>

inline bool is_gb2312_leader(unsigned char c)
{
	return c > 0x7F;
}

std::wstring gb2312_to_wstring(GMString str)
{
	std::wstring wstr;
	const size_t len = strlen(str);
	size_t i = 0;

	while (i < len)
	{
		const unsigned char c = static_cast<unsigned char>(str[i]);

		if (c <= 0x7F)
		{
			wstr.push_back(static_cast<wchar_t>(c));
			i += 1;
		}
		else
		{
			if (i + 1 >= len)
				break;

			const unsigned char c2 = static_cast<unsigned char>(str[i + 1]);
			wstr.push_back(static_cast<wchar_t>((c << 8) | c2));
			i += 2;
		}
	}
	return wstr;
}

GMString wstring_to_gb2312(const std::wstring& wstr)
{
	std::string gbstr;

	for (wchar_t wc : wstr)
	{
		const unsigned char low = static_cast<unsigned char>(wc & 0xFF);

		if ((wc & 0xFF00) == 0)
			gbstr.push_back(static_cast<char>(low));
		else
		{
			const unsigned char high = static_cast<unsigned char>((wc >> 8) & 0xFF);
			gbstr.push_back(static_cast<char>(high));
			gbstr.push_back(static_cast<char>(low));
		}
	}

	return string_to_char(gbstr);
}

GMString string_to_char(const std::string& str)
{
	size_t len = str.size() + 1;
	char* nstr = new char[len];
	strcpy_s(nstr, len, str.c_str());

	return nstr;
}

expReal StringLength(GMString str)
{
	size_t length = 0;
	while (*str)
	{
		str += is_gb2312_leader(*str) ? 2 : 1;
		++length;
	}

	return length;
}

expReal StringPos(GMString substr, GMString str)
{
	if (*substr == '\0')
		return 0;

	std::wstring w_substr = gb2312_to_wstring(substr);
	std::wstring w_str = gb2312_to_wstring(str);

	size_t pos = w_str.find(w_substr);

	return (pos == std::wstring::npos) ? 0 : pos + 1;
}

expString StringCopy(GMString str, GMReal index, GMReal count)
{
	std::wstring w_str = gb2312_to_wstring(str);

	index = clamp(index, 1, w_str.size());
	std::wstring result = w_str.substr((size_t)index - 1, (size_t)count);

	return wstring_to_gb2312(result);
}

expString StringCharAt(GMString str, GMReal index)
{
	return StringCopy(str, index, 1);
}

expString StringDelete(GMString str, GMReal index, GMReal count)
{
	std::wstring w_str = gb2312_to_wstring(str);

	index = clamp(index, 1, w_str.size());
	w_str.erase((size_t)index - 1, (size_t)count);

	return wstring_to_gb2312(w_str);
}

expString StringInsert(GMString substr, GMString str, GMReal index)
{
	if (*substr == '\0')
		return str;

	std::wstring w_substr = gb2312_to_wstring(substr);
	std::wstring w_str = gb2312_to_wstring(str);

	index = clamp(index, 1, w_str.size());
	w_str.insert((size_t)index - 1, w_substr);

	return wstring_to_gb2312(w_str);
}

expString TimeString(GMReal time)
{
	std::string timeString;

	timeString = std::to_string((int)floor(time / 3600)) + ":";
	time = fmod(time, 3600);
	timeString += std::to_string((int)floor(time / 600));
	time = fmod(time, 600);
	timeString += std::to_string((int)floor(time / 60)) + ":";
	time = fmod(time, 60);
	timeString += std::to_string((int)floor(time / 10));
	time = fmod(time, 10);
	timeString += std::to_string((int)floor(time));

	return string_to_char(timeString);
}

expString GetString(GMReal num, GMString format, GMString country)
{
	try
	{
		if (*format == '\0' && *country == '\0')
		{
			return string_to_char(std::format("{}", num));
		}
		else if (*country == '\0')
		{
			return string_to_char(std::vformat("{:" + std::string(format) + "}", 
				std::make_format_args(num)));
		}
		else
		{
			return string_to_char(std::vformat(std::locale(country), 
				"{0:" + std::string(format) + "}", std::make_format_args(num)));
		}
	}
	simplecatch(L"GetString", "")
}

std::vector<std::wstring> StringTokenResult;

expReal StringToken(GMString text, GMString sep, GMReal dontRemoveEmpty)
{
	StringTokenResult.clear();

	std::wstring w_text = gb2312_to_wstring(text);
	std::wstring w_sep = gb2312_to_wstring(sep);
	
	if (*sep == '\0')
	{
		StringTokenResult.push_back(w_text);
		return 1;
	}
	
	size_t pos = 0, found = 0;
	while ((found = w_text.find(w_sep, pos)) != std::wstring::npos)
	{
		std::wstring token = w_text.substr(pos, found - pos);
		pos = found + w_sep.length();
		
		if (!token.empty() || dontRemoveEmpty > 0.5)
			StringTokenResult.push_back(token);
	}

	if (pos <= w_text.length())
	{
		std::wstring token = w_text.substr(pos);
		if (!token.empty() || dontRemoveEmpty > 0.5)
			StringTokenResult.push_back(token);
	}

	return StringTokenResult.size();
}

expString StringGetToken(GMReal num)
{
	if (num < 0 || num > StringTokenResult.size() - 1)
		return "";

	return wstring_to_gb2312(StringTokenResult[(int)num]);
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