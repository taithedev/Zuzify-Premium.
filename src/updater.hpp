#pragma once
#include <string>

struct UpdateInfo {
    bool checked = false;
    bool available = false;
    bool downloading = false;
    bool ready = false;
    std::string currentVersion;
    std::string latestVersion;
    std::string downloadUrl;
    std::string releaseUrl;
    std::string error;
};

class AppUpdater {
public:
    explicit AppUpdater(std::string currentVersion);

    UpdateInfo CheckLatest();
    bool DownloadAndInstall(const UpdateInfo& info, std::string& error);

private:
    std::string currentVersion_;

    static bool IsNewerVersion(const std::string& current, const std::string& latest);
    static bool HttpGet(const std::string& url, std::string& response, std::string& error);
    static bool HttpDownload(const std::string& url, const std::string& path, std::string& error);
};