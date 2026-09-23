#pragma once
#include "supabase.hpp"
#include "system_stats.hpp"
#include "updater.hpp"
#include "imgui.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

class ZuzifyApp {
public:
    ZuzifyApp();
    ~ZuzifyApp();

    void Render();

private:
    SupabaseClient supabase_;
    AuthSession session_;
    PremiumState premium_;
    PremiumSettings settings_;
    CreditState credits_;
    SystemStatsProvider statsProvider_;
    SystemStats stats_;
    AppUpdater updater_;

    UpdateInfo updateInfo_;
    std::mutex updateMutex_;
    std::thread updateThread_;

    int page_ = 0;
    bool signingUp_ = false;
    bool rememberMe_ = true;
    bool isAdmin_ = false;
    bool updateInstallBusy_ = false;
    double nextStatsRefresh_ = 0.0;

    char email_[256]{};
    char password_[256]{};
    char targetUser_[64]{};
    char creditAmount_[32]{"100"};
    char creditReason_[128]{"Admin adjustment"};
    char statusMessage_[512]{};

    ImVec4 accentColor_{0.65f, 0.38f, 0.98f, 1.0f};

    void RenderLogin();
    void RenderDashboard();
    void RenderPerformance();
    void RenderThemes();
    void RenderAccount();
    void RenderUpdates();
    void RenderAdmin();

    void RefreshAccount();
    void SignOut();
    void SetStatus(const std::string& text);

    void StartUpdateCheck();
    void SaveSettings();
    void ApplyAccentFromSettings();
    void SetAccent(float r, float g, float b);
    void SaveRememberedToken();
    void LoadRememberedSession();
    void ClearRememberedToken();
};