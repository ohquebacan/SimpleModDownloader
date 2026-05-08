#pragma once

#include <vector>
#include <string>
#include <ctime>

namespace utils {
    struct GameInfo {
        std::string name;
        std::string tid;
        std::vector<uint8_t> icon;
    };

    std::vector<GameInfo> getInstalledGames();
    std::string removeHtmlTags(const std::string& str);
    std::string getModInstallPath();
    std::string timestamp_to_date(time_t timestamp);
    std::string file_size_to_string(int file_size);
}