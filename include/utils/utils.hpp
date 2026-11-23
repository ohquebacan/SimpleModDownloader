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

    // OQB: la version instalada de un juego ("3.0.3"), segun su NACP; vacia si no se sabe.
    std::string installedVersion(const std::string& tid);
    /* OQB: versiones de juego que menciona un texto: numeros de tres partes
       (3.0.3) o de dos si van tras "version", "ver", "update", "v" (v1.2).
       Sin repetir, en el orden en que aparecen, como mucho 4. */
    std::vector<std::string> mentionedVersions(const std::string& text);
    /* OQB: si un nombre de archivo nombra esta version: "4.0.1", "4_0_1",
       "4-0-1" o "401" (sin puntos, como chainchomp401.rar), suelto entre
       caracteres que no son digitos. */
    bool nameMatchesVersion(const std::string& name, const std::string& version);
    std::string removeHtmlTags(const std::string& str);
    std::string getModInstallPath();
    std::string timestamp_to_date(time_t timestamp);
    std::string file_size_to_string(int file_size);
}