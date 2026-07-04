#include <windows.h>
#include <wininet.h>
#include <shellapi.h>
#include <fstream>
#include <string>
#include <filesystem>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.h>
#include <shlobj.h>
#include <propvarutil.h>
#include <propsys.h>
#include <propkey.h>
#include <wincrypt.h>

using namespace winrt;
using namespace Windows::UI::Notifications;
using namespace Windows::Data::Xml::Dom;

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

namespace fs = std::filesystem;

const std::string URL_VERSION = "https://raw.githubusercontent.com/ssirnichek2/VotV/refs/heads/main/version.txt";
const std::string URL_RELEASE_BASE = "https://github.com/ssirnichek2/VotV/releases/download/";
const std::string URL_HASH = "https://raw.githubusercontent.com/ssirnichek2/VotV/refs/heads/main/hash.txt";
const std::string PAK_FILE_NAME = "Russian_by_Ssirnichek_p.pak";

const std::string FILE_LOCAL_VERSION = "version.txt";
const std::string PATH_LOCAL_PAK = "VotV\\Content\\Paks\\Russian_by_Ssirnichek_p.pak";
const std::string FILE_GAME_EXE = "VotV_Original.exe";


std::string Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string DownloadString(const std::string& url) {
    HINTERNET hInternet = InternetOpenA("VotVLauncher", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return "";

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string result;
    char buffer[128];
    DWORD bytesRead;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        result.append(buffer, bytesRead);
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    return Trim(result);
}

bool DownloadFile(const std::string& url, const fs::path& targetPath) {
    HINTERNET hInternet = InternetOpenA("VotVLauncher", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return false;

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return false;
    }

    std::ofstream outFile(targetPath.string(), std::ios::binary);
    if (!outFile.is_open()) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return false;
    }

    char buffer[4096];
    DWORD bytesRead;
    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        outFile.write(buffer, bytesRead);
    }

    outFile.close();
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    return true;
}

void ShowToast(const std::wstring& title, const std::wstring& message)
{
    std::wstring xml =
        L"<toast>"
        L"<visual>"
        L"<binding template='ToastGeneric'>"
        L"<text>" + title + L"</text>"
        L"<text>" + message + L"</text>"
        L"</binding>"
        L"</visual>"
        L"</toast>";

    XmlDocument doc;
    doc.LoadXml(xml);

    ToastNotification toast(doc);

    ToastNotifier notifier =
        ToastNotificationManager::CreateToastNotifier(
            L"VotV.Ssirnichek.Launcher"
        );

    notifier.Show(toast);
}

std::string GetFileHash(const fs::path& file)
{
    HANDLE hFile = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);

    if (hFile == INVALID_HANDLE_VALUE) return "";

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;

    CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT);
    CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash);

    BYTE buffer[4096];
    DWORD bytesRead = 0;

    while (ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead)
        CryptHashData(hHash, buffer, bytesRead, 0);

    BYTE hash[32];
    DWORD hashSize = 32;

    CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashSize, 0);

    CloseHandle(hFile);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    std::string result;
    char hex[3];

    for (DWORD i = 0; i < hashSize; i++)
    {
        sprintf_s(hex, "%02x", hash[i]);
        result += hex;
    }

    return result;
}

std::string HttpGet(const std::string& url)
{
    HINTERNET hInternet = InternetOpenA("VotVLauncher", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return "";

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hUrl)
    {
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string result;
    char buffer[1024];
    DWORD bytesRead;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead)
        result.append(buffer, bytesRead);

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    return result;
}

std::string ToLower(std::string s)
{
    for (char& c : s)
        c = (char)tolower((unsigned char)c);
    return s;
}

void CreateShortcut()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring exePath = path;

    wchar_t startMenu[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, startMenu);

    std::wstring linkPath = std::wstring(startMenu) + L"\\VotV Launcher.lnk";

    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) return;

    IShellLinkW* shellLink = nullptr;

    hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
        IID_IShellLinkW, (LPVOID*)&shellLink);

    if (FAILED(hr) || !shellLink) {
        CoUninitialize();
        return;
    }

    shellLink->SetPath(exePath.c_str());
    shellLink->SetDescription(L"VotV Launcher");

    IPropertyStore* propStore = nullptr;
    hr = shellLink->QueryInterface(IID_PPV_ARGS(&propStore));

    if (SUCCEEDED(hr) && propStore)
    {
        PROPVARIANT pv;
        InitPropVariantFromString(L"VotV.Ssirnichek.Launcher", &pv);

        propStore->SetValue(PKEY_AppUserModel_ID, pv);
        propStore->Commit();

        PropVariantClear(&pv);
        propStore->Release();
    }

    IPersistFile* persistFile = nullptr;
    hr = shellLink->QueryInterface(IID_PPV_ARGS(&persistFile));

    if (SUCCEEDED(hr) && persistFile)
    {
        persistFile->Save(linkPath.c_str(), TRUE);
        persistFile->Release();
    }

    shellLink->Release();
    CoUninitialize();
}

std::string GetLocalVersion(const fs::path& path) {
    if (!fs::exists(FILE_LOCAL_VERSION)) return "0.0.0";
    std::ifstream f(FILE_LOCAL_VERSION);
    if (!f.is_open()) return "0.0.0";
    std::string version;
    std::getline(f, version);
    return Trim(version);
}

void SaveLocalVersion(const fs::path& path, const std::string& version) {
    std::ofstream f(FILE_LOCAL_VERSION);
    if (f.is_open()) {
        f << version;
    }
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd) {

    SetCurrentProcessExplicitAppUserModelID(L"VotV.Ssirnichek.Launcher");

    winrt::init_apartment();

    CreateShortcut();

    wchar_t exePathBuf[MAX_PATH];
    GetModuleFileNameW(NULL, exePathBuf, MAX_PATH);
    fs::path appDir = fs::path(exePathBuf).parent_path();

    fs::path fileGameExe = appDir / "VotV_Original.exe";
    fs::path fileLocalVersion = appDir / "version.txt";
    fs::path pathLocalPak = appDir / "VotV" / "Content" / "Paks" / "Russian_by_Ssirnichek_p.pak";

    if (!fs::exists(fileGameExe)) {
        ShowToast(L"Ошибка запуска", L"Отсутствует файл VotV_Original.exe");
        return 1;
    }

    std::string localVersion = GetLocalVersion(fileLocalVersion);

    std::string serverVersion = DownloadString(URL_VERSION);
    if (serverVersion.empty()) {
        ShellExecuteW(NULL, L"open", fileGameExe.c_str(), (LPCWSTR)lpCmdLine, appDir.c_str(), SW_SHOW);
        return 1;
    }

    if (localVersion == serverVersion) {
        ShellExecuteW(NULL, L"open", fileGameExe.c_str(), (LPCWSTR)lpCmdLine, appDir.c_str(), SW_SHOW);
        return 0;
    }

    std::wstring updateMsg = L"Обнаружена новая версия: " + std::wstring(serverVersion.begin(), serverVersion.end());
    ShowToast(L"Обновление перевода", updateMsg);

    if (pathLocalPak.has_parent_path()) {
        fs::create_directories(pathLocalPak.parent_path());
    }

    std::string finalPakUrl = URL_RELEASE_BASE + serverVersion + "/" + PAK_FILE_NAME;

    if (DownloadFile(finalPakUrl, pathLocalPak)) {
        std::string localHash = GetFileHash(pathLocalPak);
        std::string serverHash = Trim(HttpGet(URL_HASH));

        if (ToLower(Trim(localHash)) != ToLower(Trim(serverHash)))
        {
            ShowToast(L"Ошибка", L"Файл повреждён");
            return 0;
        }
        SaveLocalVersion(fileLocalVersion, serverVersion);
        ShowToast(L"Обновление перевода", L"Перевод успешно обновлен!");
    }
    else {
        ShowToast(L"Ошибка", L"Произошла ошибка при скачивании перевода.");
    }

    ShellExecuteW(NULL, L"open", fileGameExe.c_str(), (LPCWSTR)lpCmdLine, appDir.c_str(), SW_SHOW);

    return 0;
}