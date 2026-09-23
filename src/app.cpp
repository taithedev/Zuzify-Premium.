#include "app.hpp"
#include "theme.hpp"
#include "imgui.h"
#include <cstring>

static ImVec4 Purple() { return ImVec4(0.65f, 0.38f, 0.98f, 1.0f); }

ZuzifyApp::ZuzifyApp()
    : supabase_("https://zqvntzvugfbvdwkoxfbm.supabase.co", "sb_publishable_hXc8J0cDIXTKJn5eQVJQrw_jsyYJjYy") {
    std::strncpy(statusMessage_, "Welcome to Zuzify Premium.", sizeof(statusMessage_) - 1);
}

void ZuzifyApp::SetStatus(const std::string& text) {
    std::strncpy(statusMessage_, text.c_str(), sizeof(statusMessage_) - 1);
    statusMessage_[sizeof(statusMessage_) - 1] = 0;
}

void ZuzifyApp::RefreshAccount() {
    if (!session_.ok) return;
    premium_ = supabase_.GetPremium(session_.accessToken, session_.userId);
    settings_ = supabase_.GetSettings(session_.accessToken, session_.userId);
    unsigned int r = 0x8b, g = 0x5c, b = 0xf6;
    if (settings_.accent.size() == 7 && settings_.accent[0] == '#')
        std::sscanf(settings_.accent.c_str() + 1, "%02x%02x%02x", &r, &g, &b);
    accentColor_ = ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
    isAdmin_ = supabase_.IsAdmin(session_.accessToken, session_.userId);
}

void ZuzifyApp::SignOut() {
    session_ = {};
    premium_ = {};
    isAdmin_ = false;
    page_ = 0;
    SetStatus("Signed out.");
}

void ZuzifyApp::RenderLogin() {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(470, 540), ImGuiCond_Always);

    ImGui::Begin("##login", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetWindowPos();
    const ImVec2 s = ImGui::GetWindowSize();
    draw->AddRectFilled(p, ImVec2(p.x+s.x,p.y+s.y), IM_COL32(12,12,18,248), 22.0f);
    draw->AddCircleFilled(ImVec2(p.x+s.x-35,p.y+35), 120.0f, IM_COL32(105,55,180,18));

    ImGui::Dummy(ImVec2(0, 18));
    ImGui::TextColored(Purple(), "ZUZIFY");
    ImGui::SameLine();
    ImGui::TextUnformatted("PREMIUM");
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(.55f,.55f,.64f,1), signingUp_ ? "Create your Premium account" : "Welcome back");
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
    if (ImGui::Button(signingUp_ ? "Already have an account" : "Create a new account", ImVec2(-1, 38)))
        signingUp_ = !signingUp_;

    ImGui::Dummy(ImVec2(0, 18));
    ImGui::TextWrapped("%s", statusMessage_);
    ImGui::End();
}

void ZuzifyApp::RenderDashboard() {
    ImGui::TextColored(Purple(), "Overview");
    ImGui::SameLine();
    ImGui::TextDisabled(" / Zuzify Premium");
    ImGui::Spacing();

    ImGui::BeginChild("hero", ImVec2(0, 170), false);
    ImGui::TextColored(Purple(), "Zuzify Premium");
    ImGui::Text("Your account, your workspace.");
    ImGui::Spacing();
    if (premium_.active) {
        ImGui::TextColored(ImVec4(.45f,1,.70f,1), "Premium is active");
        ImGui::TextWrapped("Your Premium controls and customization settings are available.");
    } else {
        ImGui::TextColored(ImVec4(1,.72f,.35f,1), "Premium is not active");
        ImGui::TextWrapped("The account is connected, but Premium access has not been enabled.");
    }
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::BeginChild("cards", ImVec2(0, 0), false);
    ImGui::BeginGroup();
    ImGui::TextColored(Purple(), "CUSTOMIZATION");
    ImGui::Text("Themes, glass intensity and accent controls.");
    ImGui::EndGroup();
    ImGui::SameLine(0, 80);
    ImGui::BeginGroup();
    ImGui::TextColored(Purple(), "ACCOUNT");
    ImGui::Text("Secure account and Premium state.");
    ImGui::EndGroup();
    ImGui::Spacing();
    ImGui::TextDisabled("No AI features. No badge system. Separate from Zuzify Socials.");
    ImGui::EndChild();
}

void ZuzifyApp::RenderThemes() {
    ImGui::TextColored(Purple(), "Appearance");
    ImGui::Spacing();

    ImGui::BeginChild("appearance", ImVec2(0, 0), false);
    ImGui::TextUnformatted("Glass intensity");
    ImGui::SliderInt("##glass", &settings_.glass, 0, 100, "%d%%");
    ImGui::Spacing();
    ImGui::TextUnformatted("Accent");
    ImGui::ColorEdit4("##accent", reinterpret_cast<float*>(&ImVec4{0.65f,0.38f,0.98f,1.0f}),
        ImGuiColorEditFlags_NoInputs);
    ImGui::Spacing();
    ImGui::TextDisabled("Theme changes are stored per account once connected to the settings endpoint.");
    ImGui::EndChild();
}

void ZuzifyApp::RenderAccount() {
    ImGui::TextColored(Purple(), "Account");
    ImGui::Spacing();
    ImGui::BeginChild("account", ImVec2(0, 0), false);
    ImGui::Text("Email");
    ImGui::TextColored(ImVec4(.65f,.65f,.72f,1), "%s", session_.email.c_str());
    ImGui::Spacing();
    ImGui::Text("User ID");
    ImGui::TextColored(ImVec4(.65f,.65f,.72f,1), "%s", session_.userId.c_str());
    ImGui::Spacing();
    ImGui::Text("Plan");
    ImGui::TextColored(Purple(), "%s", premium_.active ? premium_.plan.c_str() : "Free");
    ImGui::Spacing();
    if (ImGui::Button("Refresh account", ImVec2(170, 40))) {
        RefreshAccount();
        SetStatus("Account refreshed.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Sign out", ImVec2(120, 40))) SignOut();
    ImGui::Spacing();
    ImGui::TextWrapped("%s", statusMessage_);
    ImGui::EndChild();
}

void ZuzifyApp::RenderAdmin() {
    ImGui::TextColored(Purple(), "Premium Management");
    ImGui::Spacing();
    ImGui::BeginChild("admin", ImVec2(0, 0), false);
    ImGui::TextDisabled("Admin actions are authorized by the Supabase Edge Function.");
    ImGui::Spacing();
    ImGui::TextUnformatted("Target user ID");
    ImGui::InputText("##target", targetUser_, sizeof(targetUser_));
    ImGui::Spacing();

    if (ImGui::Button("Grant Premium", ImVec2(180, 42))) {
        std::string error;
        if (supabase_.AdminAction(session_.accessToken, targetUser_, "grant", error))
            SetStatus("Premium granted.");
        else
            SetStatus(error.empty() ? "Grant failed." : error);
    }
    ImGui::SameLine();
    if (ImGui::Button("Revoke Premium", ImVec2(180, 42))) {
        std::string error;
        if (supabase_.AdminAction(session_.accessToken, targetUser_, "revoke", error))
            SetStatus("Premium revoked.");
        else
            SetStatus(error.empty() ? "Revoke failed." : error);
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

    ImGui::SetNextWindowSize(ImVec2(1120, 720), ImGuiCond_FirstUseEver);
    ImGui::Begin("Zuzify Premium", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::BeginChild("sidebar", ImVec2(190, 0), true);
    ImGui::TextColored(Purple(), "ZUZIFY");
    ImGui::TextDisabled("PREMIUM");
    ImGui::Spacing();

    const char* pages[] = {"Dashboard", "Themes", "Account"};
    for (int i = 0; i < 3; ++i) {
        if (ImGui::Selectable(pages[i], page_ == i, 0, ImVec2(0, 42))) page_ = i;
    }

    if (isAdmin_) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Selectable("Admin", page_ == 3, 0, ImVec2(0, 42))) page_ = 3;
    }

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 62);
    ImGui::TextDisabled("Zuzify Premium");
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("content", ImVec2(0, 0), false);
    if (page_ == 0) RenderDashboard();
    else if (page_ == 1) RenderThemes();
    else if (page_ == 2) RenderAccount();
    else RenderAdmin();
    ImGui::EndChild();

    ImGui::End();
}
