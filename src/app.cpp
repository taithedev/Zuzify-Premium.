#include "app.hpp"
#include "theme.hpp"
#include "imgui.h"
#include <windows.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
#include <algorithm>

static ImVec4 Purple() { return ImVec4(0.65f, 0.38f, 0.98f, 1.0f); }

static std::wstring ToWide(const std::string& value) {
    if (value.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), size);
    return out;
}

static std::wstring SessionPath() {
    wchar_t base[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, base))) return {};
    const std::wstring folder = std::wstring(base) + L"\\ZuzifyPremium";
    CreateDirectoryW(folder.c_str(), nullptr);
    return folder + L"\\session.bin";
}

ZuzifyApp::ZuzifyApp()
    : supabase_("https://zqvntzvugfbvdwkoxfbm.supabase.co", "sb_publishable_hXc8J0cDIXTKJn5eQVJQrw_jsyYJjYy"),
      updater_(ZUZIFY_VERSION) {
    std::strncpy(statusMessage_, "Welcome to Zuzify Premium.", sizeof(statusMessage_) - 1);
    LoadRememberedSession();
    StartUpdateCheck();
}

ZuzifyApp::~ZuzifyApp() {
    if (updateThread_.joinable()) updateThread_.join();
}

void ZuzifyApp::SetStatus(const std::string& text) {
    std::strncpy(statusMessage_, text.c_str(), sizeof(statusMessage_) - 1);
    statusMessage_[sizeof(statusMessage_) - 1] = 0;
}

void ZuzifyApp::SetAccent(float r, float g, float b) {
    accentColor_ = ImVec4(r, g, b, 1.0f);

    char hex[8]{};
    std::snprintf(
        hex,
        sizeof(hex),
        "#%02X%02X%02X",
        static_cast<unsigned int>(r * 255.0f),
        static_cast<unsigned int>(g * 255.0f),
        static_cast<unsigned int>(b * 255.0f)
    );

    settings_.accent = hex;
    SaveSettings();
}

void ZuzifyApp::ApplyAccentFromSettings() {
    unsigned int r = 0x8b;
    unsigned int g = 0x5c;
    unsigned int b = 0xf6;

    if (settings_.accent.size() == 7 && settings_.accent[0] == '#') {
        std::sscanf(settings_.accent.c_str() + 1, "%02x%02x%02x", &r, &g, &b);
    }

    accentColor_ = ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
}

void ZuzifyApp::SaveSettings() {
    if (!session_.ok) return;

    std::string error;
    if (!supabase_.SaveSettings(session_.accessToken, session_.userId, settings_, error) && !error.empty()) {
        SetStatus(error);
    }
}

void ZuzifyApp::RefreshAccount() {
    if (!session_.ok) return;

    premium_ = supabase_.GetPremium(session_.accessToken, session_.userId);
    settings_ = supabase_.GetSettings(session_.accessToken, session_.userId);
    credits_ = supabase_.GetCredits(session_.accessToken, session_.userId);
    isAdmin_ = supabase_.IsAdmin(session_.accessToken, session_.userId);
    ApplyAccentFromSettings();
}

void ZuzifyApp::SaveRememberedToken() {
    if (!rememberMe_ || session_.refreshToken.empty()) return;

    const std::wstring path = SessionPath();
    if (path.empty()) return;

    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(session_.refreshToken.data()));
    input.cbData = static_cast<DWORD>(session_.refreshToken.size());

    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"Zuzify Premium", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) return;

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (file) {
        file.write(reinterpret_cast<const char*>(output.pbData), output.cbData);
    }

    LocalFree(output.pbData);
}

void ZuzifyApp::LoadRememberedSession() {
    const std::wstring path = SessionPath();
    if (path.empty()) return;

    std::ifstream file(path, std::ios::binary);
    if (!file) return;

    std::vector<char> encrypted((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (encrypted.empty()) return;

    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE*>(encrypted.data());
    input.cbData = static_cast<DWORD>(encrypted.size());

    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        DeleteFileW(path.c_str());
        return;
    }

    const std::string refreshToken(reinterpret_cast<const char*>(output.pbData), output.cbData);
    LocalFree(output.pbData);

    if (refreshToken.empty()) return;

    AuthSession result = supabase_.RefreshSession(refreshToken);
    if (result.ok && !result.accessToken.empty() && !result.userId.empty()) {
        session_ = result;
        rememberMe_ = true;
        RefreshAccount();
        SetStatus("Signed in with your saved session.");
        SaveRememberedToken();
    } else {
        DeleteFileW(path.c_str());
    }
}

void ZuzifyApp::ClearRememberedToken() {
    const std::wstring path = SessionPath();
    if (!path.empty()) DeleteFileW(path.c_str());
}

void ZuzifyApp::SignOut() {
    ClearRememberedToken();
    session_ = {};
    premium_ = {};
    settings_ = {};
    credits_ = {};
    accentColor_ = Purple();
    isAdmin_ = false;
    page_ = 0;
    SetStatus("Signed out.");
}

void ZuzifyApp::StartUpdateCheck() {
    if (updateThread_.joinable()) updateThread_.join();

    updateThread_ = std::thread([this] {
        UpdateInfo result = updater_.CheckLatest();
        std::scoped_lock lock(updateMutex_);
        updateInfo_ = std::move(result);
    });
}

void ZuzifyApp::RenderLogin() {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(470, 580), ImGuiCond_Always);
    ImGui::Begin("##login", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    const ImVec2 p = ImGui::GetWindowPos();
    const ImVec2 s = ImGui::GetWindowSize();
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(p, ImVec2(p.x + s.x, p.y + s.y), IM_COL32(12, 12, 18, 248), 22.0f);
    draw->AddCircleFilled(ImVec2(p.x + s.x - 35, p.y + 35), 120.0f, IM_COL32(105, 55, 180, 18));

    ImGui::Dummy(ImVec2(0, 18));
    ImGui::TextColored(Purple(), "ZUZIFY");
    ImGui::SameLine();
    ImGui::TextUnformatted("PREMIUM");

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(.55f, .55f, .64f, 1), signingUp_ ? "Create your Premium account" : "Welcome back");
    ImGui::Dummy(ImVec2(0, 20));

    ImGui::TextUnformatted("Email");
    ImGui::InputText("##email", email_, sizeof(email_), ImGuiInputTextFlags_CharsNoBlank);
    ImGui::TextUnformatted("Password");
    ImGui::InputText("##password", password_, sizeof(password_), ImGuiInputTextFlags_Password);

    ImGui::Dummy(ImVec2(0, 8));
    ImGui::Checkbox("Keep me signed in", &rememberMe_);
    ImGui::Dummy(ImVec2(0, 12));

    if (ImGui::Button(signingUp_ ? "Create account" : "Sign in", ImVec2(-1, 46))) {
        AuthSession result = signingUp_
            ? supabase_.SignUp(email_, password_)
            : supabase_.SignIn(email_, password_);

        if (result.ok && !result.accessToken.empty()) {
            session_ = result;
            SaveRememberedToken();
            RefreshAccount();
            page_ = 0;
            SetStatus("Signed in successfully.");
        } else if (result.ok) {
            SetStatus("Account created. Check your email if confirmation is enabled.");
        } else {
            SetStatus(result.error.empty() ? "Authentication failed." : result.error);
        }
    }

    ImGui::Dummy(ImVec2(0, 10));
    if (ImGui::Button(signingUp_ ? "Already have an account" : "Create a new account", ImVec2(-1, 38))) {
        signingUp_ = !signingUp_;
    }

    ImGui::Dummy(ImVec2(0, 18));
    ImGui::TextDisabled("Version %s", ZUZIFY_VERSION);
    ImGui::Spacing();
    ImGui::TextWrapped("%s", statusMessage_);
    ImGui::End();
}

void ZuzifyApp::RenderDashboard() {
    if (nextStatsRefresh_ <= ImGui::GetTime()) {
        stats_ = statsProvider_.Get();
        nextStatsRefresh_ = ImGui::GetTime() + 0.75;
    }

    ImGui::TextColored(accentColor_, "Overview");
    ImGui::SameLine();
    ImGui::TextDisabled(" / Zuzify Premium");
    ImGui::Spacing();

    ImGui::BeginChild("hero", ImVec2(0, 180), true);
    ImGui::TextColored(accentColor_, "Zuzify Premium");
    ImGui::SameLine();
    if (premium_.active) {
        ImGui::TextColored(ImVec4(.45f, 1, .70f, 1), "[ ACTIVE ]");
    } else {
        ImGui::TextColored(ImVec4(1, .72f, .35f, 1), "[ FREE ]");
    }

    ImGui::Text("Your account, workspace and Premium controls in one place.");
    ImGui::Spacing();

    ImGui::BeginGroup();
    ImGui::TextColored(accentColor_, "CREDITS");
    ImGui::Text("%d", credits_.balance);
    ImGui::EndGroup();

    ImGui::SameLine(0, 90);
    ImGui::BeginGroup();
    ImGui::TextColored(accentColor_, "CPU");
    ImGui::Text("%.0f%%", stats_.cpuUsage);
    ImGui::EndGroup();

    ImGui::SameLine(0, 90);
    ImGui::BeginGroup();
    ImGui::TextColored(accentColor_, "MEMORY");
    ImGui::Text("%.0f%%", stats_.memoryUsage);
    ImGui::EndGroup();

    ImGui::SameLine(0, 90);
    ImGui::BeginGroup();
    ImGui::TextColored(accentColor_, "FPS");
    ImGui::Text("%.0f", ImGui::GetIO().Framerate);
    ImGui::EndGroup();
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::BeginChild("quick", ImVec2(0, 0), false);

    ImGui::TextColored(accentColor_, "QUICK STATUS");
    ImGui::Separator();
    ImGui::Text("GPU: %s", stats_.gpuName.c_str());
    ImGui::Text("VRAM: %llu MB / %llu MB", static_cast<unsigned long long>(stats_.gpuMemoryUsedMB),
        static_cast<unsigned long long>(stats_.gpuMemoryTotalMB));
    ImGui::Text("RAM: %llu MB / %llu MB", static_cast<unsigned long long>(stats_.memoryUsedMB),
        static_cast<unsigned long long>(stats_.memoryTotalMB));

    if (updateInfo_.available) {
        ImGui::Spacing();
        ImGui::TextColored(accentColor_, "Update %s is available.", updateInfo_.latestVersion.c_str());
        if (ImGui::Button("Open Updates", ImVec2(150, 36))) page_ = 4;
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Version %s", ZUZIFY_VERSION);
    ImGui::EndChild();
}

void ZuzifyApp::RenderPerformance() {
    if (nextStatsRefresh_ <= ImGui::GetTime()) {
        stats_ = statsProvider_.Get();
        nextStatsRefresh_ = ImGui::GetTime() + 0.75;
    }

    ImGui::TextColored(accentColor_, "PC Performance");
    ImGui::SameLine();
    ImGui::TextDisabled(" / Live system information");
    ImGui::Spacing();

    ImGui::BeginChild("performance", ImVec2(0, 0), false);

    ImGui::TextColored(accentColor_, "PROCESSOR");
    ImGui::Text("%.1f%% CPU usage", stats_.cpuUsage);
    ImGui::ProgressBar(static_cast<float>(stats_.cpuUsage / 100.0), ImVec2(-1, 8));

    ImGui::Spacing();
    ImGui::TextColored(accentColor_, "MEMORY");
    ImGui::Text("%llu MB used of %llu MB", static_cast<unsigned long long>(stats_.memoryUsedMB),
        static_cast<unsigned long long>(stats_.memoryTotalMB));
    ImGui::ProgressBar(static_cast<float>(stats_.memoryUsage / 100.0), ImVec2(-1, 8));

    ImGui::Spacing();
    ImGui::TextColored(accentColor_, "GRAPHICS");
    ImGui::Text("%s", stats_.gpuName.c_str());
    ImGui::Text("%llu MB VRAM used / %llu MB budget",
        static_cast<unsigned long long>(stats_.gpuMemoryUsedMB),
        static_cast<unsigned long long>(stats_.gpuMemoryTotalMB));

    ImGui::Spacing();
    ImGui::TextColored(accentColor_, "APPLICATION");
    ImGui::Text("%.0f FPS", ImGui::GetIO().Framerate);
    ImGui::Text("%.2f ms frame time", ImGui::GetIO().DeltaTime * 1000.0f);
    ImGui::Text("Windows uptime: %llu seconds", static_cast<unsigned long long>(stats_.uptimeSeconds));

    ImGui::EndChild();
}

void ZuzifyApp::RenderThemes() {
    ImGui::TextColored(accentColor_, "Appearance");
    ImGui::SameLine();
    ImGui::TextDisabled(" / Personalize your workspace");
    ImGui::Spacing();

    ImGui::BeginChild("appearance", ImVec2(0, 0), false);

    ImGui::TextColored(accentColor_, "Presets");
    if (ImGui::Button("Purple", ImVec2(100, 36))) SetAccent(.65f, .38f, .98f);
    ImGui::SameLine();
    if (ImGui::Button("Blue", ImVec2(100, 36))) SetAccent(.25f, .55f, 1.0f);
    ImGui::SameLine();
    if (ImGui::Button("Pink", ImVec2(100, 36))) SetAccent(1.0f, .35f, .70f);
    ImGui::SameLine();
    if (ImGui::Button("Green", ImVec2(100, 36))) SetAccent(.25f, .85f, .55f);

    ImGui::Spacing();
    ImGui::TextUnformatted("Glass intensity");
    ImGui::SliderInt("##glass", &settings_.glass, 0, 100, "%d%%");
    if (ImGui::IsItemDeactivatedAfterEdit()) SaveSettings();

    ImGui::Spacing();
    ImGui::TextUnformatted("Custom accent");
    if (ImGui::ColorEdit4("##accent", &accentColor_.x, ImGuiColorEditFlags_NoInputs)) {
        settings_.accent = "#";
        char hex[8]{};
        std::snprintf(hex, sizeof(hex), "#%02X%02X%02X",
            static_cast<unsigned int>(accentColor_.x * 255.0f),
            static_cast<unsigned int>(accentColor_.y * 255.0f),
            static_cast<unsigned int>(accentColor_.z * 255.0f));
        settings_.accent = hex;

        if (ImGui::IsItemDeactivatedAfterEdit()) SaveSettings();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Changes are stored per account and loaded on the next sign-in.");
    ImGui::EndChild();
}

void ZuzifyApp::RenderAccount() {
    ImGui::TextColored(accentColor_, "Account");
    ImGui::SameLine();
    ImGui::TextDisabled(" / Profile and Premium");
    ImGui::Spacing();

    ImGui::BeginChild("account", ImVec2(0, 0), false);

    ImGui::TextColored(accentColor_, "ACCOUNT");
    ImGui::Text("Email: %s", session_.email.c_str());
    ImGui::Text("User ID: %s", session_.userId.c_str());

    ImGui::Spacing();
    ImGui::TextColored(accentColor_, "PREMIUM");
    ImGui::Text("Plan: %s", premium_.active ? premium_.plan.c_str() : "Free");
    if (!premium_.expiresAt.empty()) ImGui::Text("Expires: %s", premium_.expiresAt.c_str());

    ImGui::Spacing();
    ImGui::TextColored(accentColor_, "CREDITS");
    ImGui::Text("%d available", credits_.balance);
    ImGui::Text("%d lifetime earned", credits_.lifetimeEarned);

    ImGui::Spacing();
    ImGui::TextColored(accentColor_, "ACCOUNT STATUS");
    if (premium_.active) {
        ImGui::TextColored(ImVec4(.45f, 1, .70f, 1), "Premium Active");
    } else {
        ImGui::TextDisabled("Free Account");
    }

    if (isAdmin_) {
        ImGui::SameLine();
        ImGui::TextColored(accentColor_, "  ADMIN");
    }

    ImGui::Spacing();
    if (ImGui::Button("Refresh account", ImVec2(170, 40))) {
        RefreshAccount();
        SetStatus("Account refreshed.");
    }

    ImGui::SameLine();
    if (ImGui::Button("Sign out", ImVec2(120, 40))) {
        SignOut();
    }

    ImGui::Spacing();
    ImGui::TextWrapped("%s", statusMessage_);
    ImGui::EndChild();
}

void ZuzifyApp::RenderUpdates() {
    UpdateInfo info;
    {
        std::scoped_lock lock(updateMutex_);
        info = updateInfo_;
    }

    ImGui::TextColored(accentColor_, "Updates");
    ImGui::SameLine();
    ImGui::TextDisabled(" / GitHub Releases");
    ImGui::Spacing();

    ImGui::BeginChild("updates", ImVec2(0, 0), false);

    ImGui::TextColored(accentColor_, "Installed version");
    ImGui::Text("%s", ZUZIFY_VERSION);

    ImGui::Spacing();
    if (!info.checked) {
        ImGui::TextDisabled("Checking GitHub for the latest release...");
    } else if (!info.error.empty() && !info.available) {
        ImGui::TextColored(ImVec4(1, .65f, .35f, 1), "%s", info.error.c_str());
    } else if (info.available) {
        ImGui::TextColored(ImVec4(.45f, 1, .70f, 1), "Update available: %s", info.latestVersion.c_str());
        ImGui::TextWrapped("The update is downloaded from the official GitHub release and replaces the current executable after Zuzify Premium closes.");

        if (!updateInstallBusy_ && ImGui::Button("Install update", ImVec2(180, 42))) {
            updateInstallBusy_ = true;
            std::string error;
            if (updater_.DownloadAndInstall(info, error)) {
                SetStatus("Update downloaded. Restarting into the new version...");
                ExitProcess(0);
            } else {
                updateInstallBusy_ = false;
                SetStatus(error.empty() ? "Update failed." : error);
            }
        }

        if (!info.releaseUrl.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("Release: %s", info.releaseUrl.c_str());
        }
    } else {
        ImGui::TextColored(ImVec4(.45f, 1, .70f, 1), "You're up to date.");
        if (!info.latestVersion.empty()) ImGui::Text("Latest release: %s", info.latestVersion.c_str());
    }

    ImGui::Spacing();
    if (ImGui::Button("Check again", ImVec2(140, 38))) StartUpdateCheck();
    ImGui::EndChild();
}

void ZuzifyApp::RenderAdmin() {
    ImGui::TextColored(accentColor_, "Premium Management");
    ImGui::SameLine();
    ImGui::TextDisabled(" / Owner and admin controls");
    ImGui::Spacing();

    ImGui::BeginChild("admin", ImVec2(0, 0), false);
    ImGui::TextDisabled("Server-side authorization is enforced by the Supabase Edge Function.");
    ImGui::Spacing();

    ImGui::TextUnformatted("Target user ID");
    ImGui::InputText("##target", targetUser_, sizeof(targetUser_));

    ImGui::Spacing();
    if (ImGui::Button("Grant Premium", ImVec2(180, 42))) {
        std::string error;
        if (supabase_.AdminAction(session_.accessToken, targetUser_, "grant", error)) {
            SetStatus("Premium granted.");
        } else {
            SetStatus(error.empty() ? "Grant failed." : error);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Revoke Premium", ImVec2(180, 42))) {
        std::string error;
        if (supabase_.AdminAction(session_.accessToken, targetUser_, "revoke", error)) {
            SetStatus("Premium revoked.");
        } else {
            SetStatus(error.empty() ? "Revoke failed." : error);
        }
    }

    ImGui::Spacing();
    ImGui::TextColored(accentColor_, "Credit management");
    ImGui::InputText("Amount", creditAmount_, sizeof(creditAmount_));
    ImGui::InputText("Reason", creditReason_, sizeof(creditReason_));

    if (ImGui::Button("Apply credits", ImVec2(180, 42))) {
        int amount = 0;
        if (std::sscanf(creditAmount_, "%d", &amount) == 1 && amount != 0) {
            std::string error;
            if (supabase_.AdminCreditAction(session_.accessToken, targetUser_, amount, creditReason_, error)) {
                SetStatus("Credits updated.");
            } else {
                SetStatus(error.empty() ? "Credit update failed." : error);
            }
        } else {
            SetStatus("Enter a non-zero integer credit amount.");
        }
    }

    ImGui::Spacing();
    ImGui::TextWrapped("%s", statusMessage_);
    ImGui::EndChild();
}

void ZuzifyApp::Render() {
    ZuzifyTheme::Apply();

    if (!session_.ok) {
        RenderLogin();
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(1180, 760), ImGuiCond_FirstUseEver);
    ImGui::Begin("Zuzify Premium", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::BeginChild("sidebar", ImVec2(205, 0), true);
    ImGui::TextColored(accentColor_, "ZUZIFY");
    ImGui::TextDisabled("PREMIUM");
    ImGui::Spacing();

    const char* pages[] = {"Dashboard", "Performance", "Themes", "Account", "Updates"};
    for (int i = 0; i < 5; ++i) {
        if (ImGui::Selectable(pages[i], page_ == i, 0, ImVec2(0, 42))) page_ = i;
    }

    if (isAdmin_) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Selectable("Admin", page_ == 5, 0, ImVec2(0, 42))) page_ = 5;
    }

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 100);
    ImGui::TextColored(accentColor_, "%d credits", credits_.balance);
    ImGui::TextDisabled("v%s", ZUZIFY_VERSION);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("content", ImVec2(0, 0), false);

    if (page_ == 0) RenderDashboard();
    else if (page_ == 1) RenderPerformance();
    else if (page_ == 2) RenderThemes();
    else if (page_ == 3) RenderAccount();
    else if (page_ == 4) RenderUpdates();
    else RenderAdmin();

    ImGui::EndChild();
    ImGui::End();
}
