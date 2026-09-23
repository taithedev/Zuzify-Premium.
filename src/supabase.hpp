#pragma once
#include <string>
#include <nlohmann/json.hpp>

struct AuthSession {
    bool ok = false;
    std::string accessToken;
    std::string refreshToken;
    std::string userId;
    std::string email;
    std::string error;
};

struct PremiumState {
    bool active = false;
    std::string status = "inactive";
    std::string plan = "premium";
    std::string expiresAt;
};

struct PremiumSettings {
    std::string accent = "#8b5cf6";
    int glass = 72;
};

struct CreditState {
    int balance = 0;
    int lifetimeEarned = 0;
};

class SupabaseClient {
public:
    SupabaseClient(std::string url, std::string key);

    AuthSession SignIn(const std::string& email, const std::string& password);
    AuthSession SignUp(const std::string& email, const std::string& password);
    AuthSession RefreshSession(const std::string& refreshToken);
    PremiumState GetPremium(const std::string& accessToken, const std::string& userId);
    PremiumSettings GetSettings(const std::string& accessToken, const std::string& userId);
    CreditState GetCredits(const std::string& accessToken, const std::string& userId);
    bool SaveSettings(const std::string& accessToken, const std::string& userId, const PremiumSettings& settings, std::string& error);
    bool AdminAction(const std::string& accessToken, const std::string& userId, const std::string& action, std::string& error);
    bool AdminCreditAction(const std::string& accessToken, const std::string& userId, int amount, const std::string& reason, std::string& error);
    bool IsAdmin(const std::string& accessToken, const std::string& userId);

private:
    std::string url_;
    std::string key_;

    nlohmann::json Request(
        const std::string& method,
        const std::string& path,
        const std::string& body,
        const std::string& accessToken,
        long& status,
        std::string& error,
        const std::string& extraHeaders = ""
    );
};
