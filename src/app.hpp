#pragma once
#include "supabase.hpp"
#include <string>

class ZuzifyApp {
public:
    ZuzifyApp();
    void Render();

private:
    SupabaseClient supabase_;
    AuthSession session_;
    PremiumState premium_;
    PremiumSettings settings_;

    int page_ = 0;
    bool signingUp_ = false;
    bool rememberMe_ = true;
    bool initialized_ = false;
    bool isAdmin_ = false;

    char email_[256]{};
    char password_[256]{};
    char targetUser_[64]{};
    char statusMessage_[512]{};

    void RenderLogin();
    void RenderDashboard();
    void RenderThemes();
    void RenderAccount();
    void RenderAdmin();
    void RefreshAccount();
    void SignOut();
    void SetStatus(const std::string& text);
};
