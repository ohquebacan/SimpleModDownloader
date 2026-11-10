#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <ctime>

namespace utils {
    struct GameInfo {
        std::string name;
        std::string tid;
        std::vector<uint8_t> icon;
        // OQB
        uint32_t lastPlayed = 0;     // 0 = nunca o no se sabe
        uint32_t minutesAgo = 0;
        bool playedKnown = false;
        int installedMods = 0;       // carpetas en /mods/<juego>/
    };

    // OQB: la carpeta de /mods de un juego (mismo nombre que usa la descarga).
    std::string modFolderFor(const std::string& gameName);

    std::vector<GameInfo> getInstalledGames();
    std::string removeHtmlTags(const std::string& str);
    std::string getModInstallPath();
    std::string timestamp_to_date(time_t timestamp);
    std::string file_size_to_string(int file_size);
}