#include "supabase.hpp"
#include <windows.h>
#include <winhttp.h>
#include <vector>
#include <utility>

using json = nlohmann::json;

static std::wstring W(const std::string& s) {
    if (s.empty()) return {};

    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

SupabaseClient::SupabaseClient(std::string url, std::string key)
    : url_(std::move(url)), key_(std::move(key)) {}

json SupabaseClient::Request(
    const std::string& method,
    const std::string& path,
    const std::string& body,
    const std::string& accessToken,
    long& status,
    std::string& error,
    const std::string& extraHeaders
) {
    status = 0;
    error.clear();

    URL_COMPONENTSW parts{};
    parts.dwStructSize = sizeof(parts);

    wchar_t host[256]{};
    wchar_t object[4096]{};

    parts.lpszHostName = host;
    parts.dwHostNameLength = _countof(host);
    parts.lpszUrlPath = object;
    parts.dwUrlPathLength = _countof(object);

    const std::wstring full = W(url_ + path);
    if (!WinHttpCrackUrl(full.c_str(), 0, 0, &parts)) {
        error = "Invalid Supabase URL.";
        return {};
    }

    HINTERNET session = WinHttpOpen(
        L"ZuzifyPremium/2.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        nullptr,
        nullptr,
        0
    );

    if (!session) {
        error = "WinHttpOpen failed.";
        return {};
    }

    HINTERNET connect = WinHttpConnect(session, parts.lpszHostName, parts.nPort, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        error = "WinHttpConnect failed.";
        return {};
    }

    HINTERNET request = WinHttpOpenRequest(
        connect,
        W(method).c_str(),
        parts.lpszUrlPath,
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0
    );

    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        error = "WinHttpOpenRequest failed.";
        return {};
    }

    std::wstring headers =
        L"apikey: " + W(key_) +
        L"\r\nContent-Type: application/json\r\nAccept: application/json\r\n";

    if (!accessToken.empty()) {
        headers += L"Authorization: Bearer " + W(accessToken) + L"\r\n";
    }

    if (!extraHeaders.empty()) {
        headers += W(extraHeaders);
        headers += L"\r\n";
    }

    const std::string payload = body;

    const BOOL sent = WinHttpSendRequest(
        request,
        headers.c_str(),
        static_cast<DWORD>(-1),
        payload.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(payload.data()),
        static_cast<DWORD>(payload.size()),
        static_cast<DWORD>(payload.size()),
        0
    );

    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        error = "Network request failed.";
        return {};
    }

    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(
        request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX
    );

    std::string response;
    DWORD available = 0;

    do {
        if (!WinHttpQueryDataAvailable(request, &available) || !available) break;

        std::vector<char> buffer(available);
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data(), available, &read) || !read) break;
        response.append(buffer.data(), read);
    } while (available > 0);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    if (response.empty()) return json::object();

    try {
        json parsed = json::parse(response);

        if (status < 200 || status >= 300) {
            error = parsed.value(
                "msg",
                parsed.value(
                    "message",
                    parsed.value("error_description", "Supabase request failed.")
                )
            );
        }

        return parsed;
    } catch (...) {
        error = "Supabase returned invalid JSON.";
        return {};
    }
}

static AuthSession ParseAuth(json& r, const std::string& fallbackEmail) {
    AuthSession out;
    out.ok = true;
    out.accessToken = r.value("access_token", "");
    out.refreshToken = r.value("refresh_token", "");

    if (r.contains("user")) {
        out.userId = r["user"].value("id", "");
        out.email = r["user"].value("email", fallbackEmail);
    }

    return out;
}

AuthSession SupabaseClient::SignIn(const std::string& email, const std::string& password) {
    long status = 0;
    std::string error;

    json body = {{"email", email}, {"password", password}};
    auto r = Request("POST", "/auth/v1/token?grant_type=password", body.dump(), "", status, error);

    if (status < 200 || status >= 300) {
        AuthSession out;
        out.error = error;
        return out;
    }

    return ParseAuth(r, email);
}

AuthSession SupabaseClient::SignUp(const std::string& email, const std::string& password) {
    long status = 0;
    std::string error;

    json body = {{"email", email}, {"password", password}};
    auto r = Request("POST", "/auth/v1/signup", body.dump(), "", status, error);

    if (status < 200 || status >= 300) {
        AuthSession out;
        out.error = error;
        return out;
    }

    return ParseAuth(r, email);
}

AuthSession SupabaseClient::RefreshSession(const std::string& refreshToken) {
    long status = 0;
    std::string error;

    json body = {{"refresh_token", refreshToken}};
    auto r = Request("POST", "/auth/v1/token?grant_type=refresh_token", body.dump(), "", status, error);

    if (status < 200 || status >= 300) {
        AuthSession out;
        out.error = error;
        return out;
    }

    return ParseAuth(r, "");
}

PremiumState SupabaseClient::GetPremium(const std::string& accessToken, const std::string& userId) {
    PremiumState out;

    long status = 0;
    std::string error;
    const std::string path =
        "/rest/v1/premium_subscriptions?select=status,plan,expires_at&user_id=eq." +
        userId + "&limit=1";

    auto r = Request("GET", path, "", accessToken, status, error);
    if (status < 200 || status >= 300 || !r.is_array() || r.empty()) return out;

    const auto& row = r[0];
    out.status = row.value("status", "inactive");
    out.plan = row.value("plan", "premium");
    out.expiresAt = row.value("expires_at", "");
    out.active = out.status == "active";
    return out;
}

PremiumSettings SupabaseClient::GetSettings(const std::string& accessToken, const std::string& userId) {
    PremiumSettings out;

    long status = 0;
    std::string error;
    const std::string path =
        "/rest/v1/premium_settings?select=accent_color,glass_intensity&user_id=eq." +
        userId + "&limit=1";

    auto r = Request("GET", path, "", accessToken, status, error);
    if (status >= 200 && status < 300 && r.is_array() && !r.empty()) {
        out.accent = r[0].value("accent_color", out.accent);
        out.glass = r[0].value("glass_intensity", out.glass);
    }

    return out;
}

CreditState SupabaseClient::GetCredits(const std::string& accessToken, const std::string& userId) {
    CreditState out;

    long status = 0;
    std::string error;
    const std::string path =
        "/rest/v1/premium_credits?select=balance,lifetime_earned&user_id=eq." +
        userId + "&limit=1";

    auto r = Request("GET", path, "", accessToken, status, error);
    if (status >= 200 && status < 300 && r.is_array() && !r.empty()) {
        out.balance = r[0].value("balance", 0);
        out.lifetimeEarned = r[0].value("lifetime_earned", 0);
    }

    return out;
}

bool SupabaseClient::SaveSettings(
    const std::string& accessToken,
    const std::string& userId,
    const PremiumSettings& settings,
    std::string& error
) {
    long status = 0;

    json body = {
        {"user_id", userId},
        {"accent_color", settings.accent},
        {"glass_intensity", settings.glass}
    };

    Request(
        "POST",
        "/rest/v1/premium_settings",
        body.dump(),
        accessToken,
        status,
        error,
        "Prefer: resolution=merge-duplicates,return=minimal"
    );

    return status >= 200 && status < 300;
}

bool SupabaseClient::IsAdmin(const std::string& accessToken, const std::string& userId) {
    long status = 0;
    std::string error;

    auto r = Request(
        "GET",
        "/rest/v1/premium_admins?select=role&user_id=eq." + userId + "&limit=1",
        "",
        accessToken,
        status,
        error
    );

    return status >= 200 && status < 300 && r.is_array() && !r.empty();
}

bool SupabaseClient::AdminAction(
    const std::string& accessToken,
    const std::string& userId,
    const std::string& action,
    std::string& error
) {
    long status = 0;

    json body = {
        {"action", action},
        {"user_id", userId}
    };

    auto r = Request(
        "POST",
        "/functions/v1/premium-admin",
        body.dump(),
        accessToken,
        status,
        error
    );

    return status >= 200 && status < 300 && r.value("success", false);
}

bool SupabaseClient::AdminCreditAction(
    const std::string& accessToken,
    const std::string& userId,
    int amount,
    const std::string& reason,
    std::string& error
) {
    long status = 0;

    json body = {
        {"action", "credit"},
        {"user_id", userId},
        {"amount", amount},
        {"reason", reason}
    };

    auto r = Request(
        "POST",
        "/functions/v1/premium-admin",
        body.dump(),
        accessToken,
        status,
        error
    );

    return status >= 200 && status < 300 && r.value("success", false);
}
