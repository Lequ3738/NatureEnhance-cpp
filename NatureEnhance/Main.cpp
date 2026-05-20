#include "buffer.h"
#include "DataStruct.h"
#include "iconv.h"
#include "Main.h"
#include <filesystem>
#include <fstream>
#include <psapi.h>
#include <regex>
#include <vector>

std::string GMReturnString;

gm::CGMVariable GetResource(GMString res)
{
    return gm::execute_string("return " + std::string(res));
}

gm::CGMVariable GetResource(std::string res)
{
    return gm::execute_string("return " + res);
}

void ShowMessage(std::string&& str, std::string&& caption, UINT type)
{
	int str_size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
	int caption_size = MultiByteToWideChar(CP_UTF8, 0, caption.c_str(), (int)caption.size(), nullptr, 0);

	std::wstring wstr(str_size, L'\0');
	std::wstring wcaption(caption_size, L'\0');

	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), wstr.data(), str_size);
	MultiByteToWideChar(CP_UTF8, 0, caption.c_str(), (int)caption.size(), wcaption.data(), caption_size);

	MessageBox(GMWindowsHandle, wstr.c_str(), wcaption.c_str(), type);
}

std::wstring ToWstring(GMString str)
{
	int len = strlen(str);
	int str_size = MultiByteToWideChar(CP_ACP, 0, str, len, nullptr, 0);

	std::wstring wstr(str_size, L'\0');
	MultiByteToWideChar(CP_ACP, 0, str, len, wstr.data(), str_size);

	return wstr;
}

#pragma region Debug

void DEBUG(std::string str)
{
    MessageBoxA(GMWindowsHandle, str.c_str(), "DEBUG", MB_OK);
}

void console_write(const std::string& info, int mode)
{
	auto inst = gmapi->GetCurrentInstancePtr();

	std::string name;
	if (gm::instance_exists(inst->id))
		name = gm::object_get_name(inst->object_index).c_str();
	else
		name = "<unknown>";
	name += " Main.dll";

	std::string filename(std::to_string(TimerGet()) + " " + name);
	if (mode == msb_error)
		filename += " Error";
	else if (mode == msb_warning)
		filename += " Warning";
	filename += ".txt";

	auto path(std::filesystem::current_path() / "Debug\\Log");
	path /= filename;

	auto now = std::chrono::system_clock::now();
	std::string text("[" + std::format("{:%H:%M:%S}", now) + "] " +
		name + ": " + info
	);

	UINT buffer = (UINT)gm::buffer_create();
	gm::buffer_write_string(buffer, text.c_str());
	gm::buffer_write_to_file(buffer, path.string().c_str());
	gm::buffer_destroy(buffer);
}

#pragma endregion

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
                throw std::runtime_error("不合法的二进制字符串字面量。");

            value = (value << 1) | (*bin - '0');
            ++bin;
        }

        return value;
    }
    simplecatch("BinToDec", -1)
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

#include <ShellScalingApi.h>
typedef HRESULT (WINAPI *GetDpi81)(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);
typedef UINT(WINAPI *GetDpi10)(HWND);

expReal os_get_dpiscale()
{
    // windows 10+ API
    if (HMODULE user32 = GetModuleHandle(L"user32.dll"))
    {
        auto GetDpi = (GetDpi10)GetProcAddress(user32, "GetDpiForWindow");
        if (GetDpi)
        {
			UINT dpi = GetDpi(GMWindowsHandle);
			return (double)dpi / 96.0; // 96 DPI 是标准 DPI
        }
    }

	// windows 8.1 API
    if (HMODULE shcore = LoadLibrary(L"shcore.dll"))
    {
        auto GetDpi = (GetDpi81)GetProcAddress(shcore, "GetDpiForMonitor");
        if (GetDpi)
        {
            UINT dpiX, dpiY;
            HMONITOR monitor = MonitorFromWindow(GMWindowsHandle, MONITOR_DEFAULTTONEAREST);
            HRESULT hr = GetDpi(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
            
            if (SUCCEEDED(hr))
            {
                FreeLibrary(shcore);
                return (double)dpiX / 96.0;
            }
        }
        FreeLibrary(shcore);
    }
	
    // windows 7 API
    HDC hdc = GetDC(GMWindowsHandle);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX); // 水平DPI
    ReleaseDC(GMWindowsHandle, hdc);
	return (double)dpi / 96.0;
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

static PROCESS_INFORMATION pi;
static int process_running = 0;

expReal execute_program_async(GMString command)
{
	if (process_running) return 75;

	STARTUPINFOW si = { sizeof(si) };

	std::wstring wcommand = ToWstring(command);

	bool proc = static_cast<bool>(CreateProcessW(0, wcommand.data(), nullptr, nullptr, 
		true, 0x08000000, nullptr, nullptr, &si, &pi));
	process_running = 1;

	return proc;
}

expReal execute_program_async_result()
{
	if (!process_running) return 75;
	DWORD ret;

	GetExitCodeProcess(pi.hProcess, &ret);

	if (ret == 259) return 259;

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	process_running = 0;

	return (double)ret;
}

expReal GetFunctionAddress(GMString name)
{
    return (double)(int)gm::CGMAPI::GetGMFunctionAddress(name);
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

void instance_set_scale(int id, GMReal xscale, GMReal yscale)
{
	gm::PGMINSTANCE inst = gmapi->GetInstancePtr(id);
	if (!inst) return;

	inst->image_xscale = xscale;
	inst->image_yscale = yscale;
	
	int mask = inst->mask_index;
	if (mask < 0)
		mask = inst->sprite_index;
	if (mask < 0)
		return;

	GMReal origin_x = static_cast<GMReal>(gm::sprite_get_xoffset(mask));
	GMReal origin_y = static_cast<GMReal>(gm::sprite_get_yoffset(mask));

	GMReal raw_left = static_cast<double>(gm::sprite_get_bbox_left(mask)) - origin_x;
	GMReal raw_right = static_cast<double>(gm::sprite_get_bbox_right(mask)) + 1.0 - origin_x;
	GMReal raw_top = static_cast<double>(gm::sprite_get_bbox_top(mask)) - origin_y;
	GMReal raw_bottom = static_cast<double>(gm::sprite_get_bbox_bottom(mask)) + 1.0 - origin_y;

	if (xscale >= 0)
	{
		inst->bbox_left = static_cast<int>(std::floor(inst->x + raw_left * xscale));
		inst->bbox_right = static_cast<int>(std::ceil(inst->x + raw_right * xscale)) - 1;
	}
	else
	{
		inst->bbox_left = static_cast<int>(std::floor(inst->x + raw_right * xscale));
		inst->bbox_right = static_cast<int>(std::floor(inst->x + raw_left * xscale)) - 1;
	}

	if (yscale >= 0)
	{
		inst->bbox_top = static_cast<int>(std::floor(inst->y + raw_top * yscale));
		inst->bbox_bottom = static_cast<int>(std::ceil(inst->y + raw_bottom * yscale)) - 1;
	}
	else
	{
		inst->bbox_top = static_cast<int>(std::floor(inst->y + raw_bottom * yscale));
		inst->bbox_bottom = static_cast<int>(std::floor(inst->y + raw_top * yscale)) - 1;
	}
}

#pragma endregion

#pragma region Load Text Resources

expReal InitTexts(GMString path)
{
    using namespace std;
    namespace fs = filesystem;

    try
    {
        if (!fs::exists(path))
            throw std::runtime_error("文件夹路径 (" + string(path) + ") 不存在。");

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
            if (!filestream.is_open())
                throw std::runtime_error("文件 (" + file.string() + ") 打开失败。");

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
    simplecatch("InitTexts", 0)
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
    gm::registry_set_root((int)root);

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
                throw std::runtime_error("注册表路径不存在: " + std::string(name));

            throw std::runtime_error("打开注册表失败 (错误代码: " + std::to_string(result) + ")");
        }

        result = RegDeleteValue(hkey, valueName.c_str());
        if (result != ERROR_SUCCESS)
        {
            if (result == ERROR_FILE_NOT_FOUND)
                throw std::runtime_error("值不存在: " + std::string(key));

            throw std::runtime_error("删除失败 (错误代码: " + std::to_string(result) + ")");
        }

        RegCloseKey(hkey);
        finish;
    }
    simplecatch("RegistryDeleteKey", 0)
}

std::vector<std::string> MatchedFiles;

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
                    MatchedFiles.push_back(entry.path().string());
            }
        }

        return MatchedFiles.size();
    }
    simplecatch("GetAllFilesInSubfolders", -1)
}

expString GetAllFilesDir(GMReal num)
{
    if (num < 0 || num > MatchedFiles.size() - 1)
        return "";

    return MatchedFiles[(int)num].c_str();
}

expString ReadAllText(GMString file)
{
    try
    {
        std::ifstream filestream(file, std::ios::binary);
        if (!filestream)
            throw std::runtime_error("文件 (" + std::string(file) + ") 打开失败。");

        std::string data = {
            std::istreambuf_iterator<char>(filestream),
            std::istreambuf_iterator<char>()
        };

		GMReturnString = std::move(data);
        return GMReturnString.c_str();
    }
    simplecatch("ReadAllText", "")
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

expReal ReadCBVFile(GMString filename)
{
    try
    {
        std::ifstream file(filename, std::ios::binary);
        if (!file)
            throw std::runtime_error("文件 (" + std::string(filename) + ") 打开失败。");

        // 检测文件字节序是否和系统默认字节序一致
        char endian_flag;
        file.read(&endian_flag, 1);
		bool reversed = (endian_flag != 1);  // 1 表示小端字节序，0 表示大端字节序

        // 计算文件中浮点数的数量
        file.seekg(0, std::ios::end);
        size_t num_floats = ((size_t)file.tellg() - 1) / 4;
        file.seekg(1, std::ios::beg);  // 跳过首字节

        std::string info("CBV File's List: " + std::string(filename));
        int list = (int)ne_list_create(info.c_str());
        for (size_t i = 0; i < num_floats; ++i)
        {
            char buffer[4];
            file.read(buffer, 4);

            if (reversed)  // 处理字节序转换
                std::reverse(buffer, buffer + 4);

            // 将字节转换为浮点数
            float value;
            std::memcpy(&value, buffer, sizeof(float));

            // 将浮点数添加到列表中
            gm::ds_list_add(list, value);
        }

        return list;
    }
    simplecatch("ReadCBVFile", -1)
}
#pragma endregion

#pragma region Array

expReal GetLocalFirstDimensionSize(GMString var)
{
    int varid = gmapi->GetSymbolID(var);
    auto gmvar = gmapi->GetLocalVariablePtr(gm::self, varid);
    if (gmvar == nullptr)
        return -1;

    return gmvar->GetFirstDimensionSize();
}

expReal GetLocalSecondDimensionSize(GMString var, GMReal index)
{
    int varid = gmapi->GetSymbolID(var);
    auto gmvar = gmapi->GetLocalVariablePtr(gm::self, varid);
    if (gmvar == nullptr)
        return -1;

    return gmvar->GetSecondDimensionSize((ULONG)index);
}

expReal GetGlobalFirstDimensionSize(GMString var)
{
    int varid = gmapi->GetSymbolID(var);
    auto gmvar = gmapi->GetGlobalVariablePtr(varid);
    if (gmvar == nullptr)
        return -1;

    return gmvar->GetFirstDimensionSize();
}

expReal GetGlobalSecondDimensionSize(GMString var, GMReal index)
{
    int varid = gmapi->GetSymbolID(var);
    auto gmvar = gmapi->GetGlobalVariablePtr(varid);
    if (gmvar == nullptr)
        return -1;

    return gmvar->GetSecondDimensionSize((ULONG)index);
}

#pragma endregion

#pragma region Graphic

expReal GetTextureMipmapCount(GMReal gmtex)
{
    IDirect3DTexture8* texture = gm::CGMAPI::GetTextureArray()[(int)gmtex].texture;
    if (texture == nullptr)
		return -1;

	return texture->GetLevelCount();
}

#pragma endregion