#include "updater.hpp"
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>

using json = nlohmann::json;

static std::wstring Widen(const std::string& value) {
    if (value.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), size);
    return out;
}

static bool ParseUrl(const std::string& url, URL_COMPONENTSW& parts, wchar_t* host, DWORD hostCount, wchar_t* path, DWORD pathCount) {
    parts = {};
    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = host;
    parts.dwHostNameLength = hostCount;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = pathCount;
    const std::wstring full = Widen(url);
    return WinHttpCrackUrl(full.c_str(), 0, 0, &parts) == TRUE;
}

static bool ReadResponse(HINTERNET request, std::string& output) {
    DWORD available = 0;
    do {
        if (!WinHttpQueryDataAvailable(request, &available) || !available) break;
        std::vector<char> buffer(available);
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data(), available, &read) || !read) break;
        output.append(buffer.data(), read);
    } while (available > 0);
    return true;
}

static bool OpenRequestForUrl(const std::string& url, HINTERNET& session, HINTERNET& connect, HINTERNET& request, std::wstring& headers, std::string& error) {
    wchar_t host[512]{};
    wchar_t path[8192]{};
    URL_COMPONENTSW parts{};

    if (!ParseUrl(url, parts, host, _countof(host), path, _countof(path))) {
        error = "Invalid update URL.";
        return false;
    }

    session = WinHttpOpen(L"ZuzifyPremiumUpdater/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    if (!session) {
        error = "Could not start Windows HTTP.";
        return false;
    }

    connect = WinHttpConnect(session, parts.lpszHostName, parts.nPort, 0);
    if (!connect) {
        error = "Could not connect to update server.";
        WinHttpCloseHandle(session);
        session = nullptr;
        return false;
    }

    request = WinHttpOpenRequest(
        connect,
        L"GET",
        parts.lpszUrlPath,
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0
    );

    if (!request) {
        error = "Could not create update request.";
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        connect = nullptr;
        session = nullptr;
        return false;
    }

    headers = L"User-Agent: ZuzifyPremiumUpdater/1.0\r\nAccept: application/vnd.github+json\r\n";
    return true;
}

bool AppUpdater::HttpGet(const std::string& url, std::string& response, std::string& error) {
    HINTERNET session = nullptr;
    HINTERNET connect = nullptr;
    HINTERNET request = nullptr;
    std::wstring headers;

    if (!OpenRequestForUrl(url, session, connect, request, headers, error)) return false;

    BOOL sent = WinHttpSendRequest(request, headers.c_str(), static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        error = "Update request failed.";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);

    ReadResponse(request, response);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    if (status < 200 || status >= 300) {
        error = "GitHub returned HTTP " + std::to_string(status) + ".";
        return false;
    }

    return true;
}

bool AppUpdater::HttpDownload(const std::string& url, const std::string& path, std::string& error) {
    HINTERNET session = nullptr;
    HINTERNET connect = nullptr;
    HINTERNET request = nullptr;
    std::wstring headers;

    if (!OpenRequestForUrl(url, session, connect, request, headers, error)) return false;

    BOOL sent = WinHttpSendRequest(request, headers.c_str(), static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        error = "Update download failed.";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);

    if (status < 200 || status >= 300) {
        error = "GitHub returned HTTP " + std::to_string(status) + " for the update.";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        error = "Could not create the update file.";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD available = 0;
    do {
        if (!WinHttpQueryDataAvailable(request, &available) || !available) break;
        std::vector<char> buffer(available);
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data(), available, &read) || !read) break;
        file.write(buffer.data(), static_cast<std::streamsize>(read));
    } while (available > 0);

    file.close();
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return true;
}

AppUpdater::AppUpdater(std::string currentVersion)
    : currentVersion_(std::move(currentVersion)) {}

bool AppUpdater::IsNewerVersion(const std::string& current, const std::string& latest) {
    auto parse = [](std::string value) {
        if (!value.empty() && (value[0] == 'v' || value[0] == 'V')) value.erase(value.begin());
        std::vector<int> parts;
        std::stringstream ss(value);
        std::string part;
        while (std::getline(ss, part, '.')) {
            int number = 0;
            for (char c : part) {
                if (!std::isdigit(static_cast<unsigned char>(c))) break;
                number = number * 10 + (c - '0');
            }
            parts.push_back(number);
        }
        while (parts.size() < 3) parts.push_back(0);
        return parts;
    };

    const auto a = parse(current);
    const auto b = parse(latest);
    return b > a;
}

UpdateInfo AppUpdater::CheckLatest() {
    UpdateInfo info;
    info.currentVersion = currentVersion_;

    std::string body;
    std::string error;
    if (!HttpGet("https://api.github.com/repos/taithedev/Zuzify-Premium./releases/latest", body, error)) {
        info.checked = true;
        info.error = error;
        return info;
    }

    try {
        const auto release = json::parse(body);
        info.latestVersion = release.value("tag_name", "");
        info.releaseUrl = release.value("html_url", "");

        if (release.contains("assets") && release["assets"].is_array()) {
            for (const auto& asset : release["assets"]) {
                const std::string name = asset.value("name", "");
                if (name == "ZuzifyPremium.exe") {
                    info.downloadUrl = asset.value("browser_download_url", "");
                    break;
                }
            }
        }

        info.available = !info.latestVersion.empty() && IsNewerVersion(currentVersion_, info.latestVersion);
        if (info.available && info.downloadUrl.empty()) {
            info.error = "A newer release exists, but no ZuzifyPremium.exe asset was found.";
        }
    } catch (...) {
        info.error = "GitHub returned invalid release data.";
    }

    info.checked = true;
    return info;
}

bool AppUpdater::DownloadAndInstall(const UpdateInfo& info, std::string& error) {
    if (!info.available || info.downloadUrl.empty()) {
        error = "No installable update is available.";
        return false;
    }

    wchar_t modulePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, modulePath, _countof(modulePath));
    if (!length || length >= _countof(modulePath)) {
        error = "Could not locate the current application.";
        return false;
    }

    wchar_t tempPath[MAX_PATH]{};
    if (!GetTempPathW(_countof(tempPath), tempPath)) {
        error = "Could not locate the Windows temp folder.";
        return false;
    }

    const std::wstring tempExe = std::wstring(tempPath) + L"ZuzifyPremiumUpdate.exe";
    if (!HttpDownload(info.downloadUrl, std::string(tempExe.begin(), tempExe.end()), error)) {
        return false;
    }

    std::wstring scriptPath = std::wstring(tempPath) + L"ZuzifyPremiumUpdater.cmd";
    std::wofstream script(scriptPath);
    if (!script) {
        error = "Could not create the updater script.";
        return false;
    }

    script << L"@echo off\n";
    script << L"timeout /t 2 /nobreak >nul\n";
    script << L"for /l %%A in (1,1,10) do (\n";
    script << L"  move /Y \"" << tempExe << L"\" \"" << modulePath << L"\" >nul 2>&1\n";
    script << L"  if not errorlevel 1 goto startapp\n";
    script << L"  timeout /t 1 /nobreak >nul\n";
    script << L")\n";
    script << L"exit /b 1\n";
    script << L":startapp\n";
    script << L"start \"\" \"" << modulePath << L"\"\n";
    script << L"del \"%~f0\"\n";
    script.close();

    HINSTANCE result = ShellExecuteW(nullptr, L"open", scriptPath.c_str(), nullptr, nullptr, SW_HIDE);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        error = "Could not start the updater.";
        return false;
    }

    return true;
}