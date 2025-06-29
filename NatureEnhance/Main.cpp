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

void DEBUG(std::string str)
{
    MessageBoxA(GMWindowsHandle, str.c_str(), "DEBUG", MB_OK);
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

std::wstring ToWstring(GMString str)
{
    GMString utf8_str = ChangeCoding(str, "GB2312", "UTF-8");
    std::string stdstr(utf8_str);
    return std::wstring(stdstr.begin(), stdstr.end());
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
