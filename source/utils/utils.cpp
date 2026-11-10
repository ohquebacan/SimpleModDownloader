#include "utils/utils.hpp"

#include <switch.h>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <regex>
#include <borealis.hpp>
#include <SimpleIniParser.hpp>
#include <cstring>
#include <zlib.h>


using namespace simpleIniParser;

namespace utils {

    Result nacpGetLanguageEntrySpecialLanguage(NacpStruct* nacp, NacpLanguageEntry** langentry, const SetLanguage LanguageChoosen) {
        Result rc=0;
        SetLanguage Language = LanguageChoosen;
        NacpLanguageEntry *entry = NULL;
        u32 i=0;

        if (nacp==NULL || langentry==NULL)
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);

        *langentry = NULL;

        entry = &nacp->lang[SetLanguage_ENUS];

        if (entry->name[0]==0 && entry->author[0]==0) {
            for(i=0; i<16; i++) {
                entry = &nacp->lang[i];
                if (entry->name[0] || entry->author[0]) break;
            }
        }

        if (entry->name[0]==0 && entry->author[0]==0)
            return rc;

        *langentry = entry;

        return rc;
    }


    /* OQB: desde el firmware 21, las actualizaciones de los juegos traen los
       nombres comprimidos (zlib, para que entren 32 idiomas): el byte 0x3215 de
       la NACP vale 1 y libnx los da tal cual, asi que salian "?" y simbolos raros
       (en la busqueda de GameBanana y en la carpeta de /mods que lee
       SimpleModManager). Esto los descomprime en su lugar. Ver switchbrew/libnx#714. */
    bool decompressNacpNames(NacpStruct* nacp)
    {
        auto* raw = reinterpret_cast<u8*>(nacp);
        if (raw[0x3215] != 1) return true;
        u16 size = 0;
        std::memcpy(&size, raw, sizeof(size));
        if (size == 0 || size > 0x3000 - 2) return false;
        std::vector<u8> out(0x6000);
        z_stream z{};
        z.next_in = raw + 2;
        z.avail_in = size;
        z.next_out = out.data();
        z.avail_out = out.size();
        if (inflateInit2(&z, -15) != Z_OK) return false;
        const int r = inflate(&z, Z_FINISH);
        inflateEnd(&z);
        if (r != Z_STREAM_END || z.total_out < 0x3000) return false;
        std::memcpy(raw, out.data(), 0x3000);  // los 16 primeros idiomas
        raw[0x3215] = 0;
        return true;
    }

    std::string formatApplicationId(u64 ApplicationId)
    {
        std::stringstream strm;
        strm << std::uppercase << std::setfill('0') << std::setw(16) << std::hex << ApplicationId;
        return strm.str();
    }

    std::vector<GameInfo> getInstalledGames() {
        std::vector<GameInfo> games;

        NsApplicationRecord* records = new NsApplicationRecord[64000]();
        NsApplicationControlData* controlData = new NsApplicationControlData();
        NacpLanguageEntry* langEntry = nullptr;

        Result rc;
        int recordCount = 0;
        size_t controlSize = 0;

        rc = nsListApplicationRecord(records, 64000, 0, &recordCount);
        for (auto i = 0; i < recordCount; ++i) {
            uint64_t tid = records[i].application_id;
            rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, tid, controlData, sizeof(*controlData), &controlSize);
            if (R_FAILED(rc))
                continue;

            // Skip system apps, media apps and NRO forwarders — games always require a user account
            if (controlData->nacp.startup_user_account == 0)
                continue;

            decompressNacpNames(&controlData->nacp);
            rc = nacpGetLanguageEntrySpecialLanguage(&controlData->nacp, &langEntry, SetLanguage_ENUS);
            if (R_FAILED(rc))
                continue;

            if (!langEntry || !langEntry->name[0])
                continue;

            GameInfo info;
            info.name = langEntry->name;
            info.tid = formatApplicationId(tid);
            info.icon.assign(controlData->icon, controlData->icon + 0x20000);

            // OQB: cuantos mods tiene ya en la carpeta de SimpleModManager.
            std::error_code ec;
            for (const auto& e : std::filesystem::directory_iterator(modFolderFor(info.name), ec))
                if (e.is_directory(ec)) info.installedMods++;

            games.push_back(std::move(info));
        }

        // OQB: cuando se jugo cada uno por ultima vez (para ordenar por recientes).
        if (R_SUCCEEDED(pdmqryInitialize())) {
            for (auto& g : games) {
                const u64 id = std::stoull(g.tid, nullptr, 16);
                PdmLastPlayTime last{};
                s32 total = 0;
                if (R_SUCCEEDED(pdmqryQueryLastPlayTime(false, &last, &id, 1, &total)) && total > 0 && last.flag) {
                    g.lastPlayed = last.timestamp_user;
                    g.minutesAgo = last.last_played_minutes;
                    g.playedKnown = true;
                }
            }
            pdmqryExit();
        }

        delete[] records;
        delete controlData;
        return games;
    }

    std::string modFolderFor(const std::string& gameName) {
        static const std::regex badChars("[:/\\<>|*]");
        std::string base = getModInstallPath();
        while (!base.empty() && base.front() == '/') base.erase(base.begin());
        static const std::regex accent("é");
        return "sdmc:/" + base + "/" + std::regex_replace(std::regex_replace(gameName, badChars, "-"), accent, "e");
    }

    std::string removeHtmlTags(const std::string& input) {
        //Replace <br> by \n
        std::regex brRegex("<br>");
        std::string output = std::regex_replace(input, brRegex, "\n");
        std::regex nbspRegex("&nbsp;");
        output = std::regex_replace(output, nbspRegex, " ");
        std::regex tagsRegex("<.*?>");
        return std::regex_replace(output, tagsRegex, "");
    }

    bool ends_with(const std::string& str, const std::string& suffix) {
        if (suffix.length() > str.length()) {
            return false;
        }
        return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    }

    bool starts_with(const std::string& str, const std::string& prefix) {
        if (prefix.length() > str.length()) {
            return false;
        }
        return str.compare(0, prefix.length(), prefix) == 0;
    }

    std::string getModInstallPath() {
        std::filesystem::path path(std::string("sdmc:/config/SimpleModManager/parameters.ini"));
        if(!std::filesystem::exists(path)) {
            return "mods";
        }
        Ini* config = Ini::parseFile(path.string());
        IniOption* path_mods = config->findFirstOption("stored-mods-base-folder");
        std::string pathString = path_mods->value;
        brls::Logger::debug("Mod install path: {}", pathString);
        if(ends_with(pathString, "/"))
            pathString.pop_back();
        if(starts_with(pathString, "/"))
            pathString.erase(0, 1);
        return pathString;
    }

    std::string timestamp_to_date(time_t timestamp) {
        std::tm* timeinfo = std::gmtime(&timestamp);
        std::ostringstream os;
        os << std::setfill('0') << std::setw(2) << timeinfo->tm_mday << "/"
        << std::setfill('0') << std::setw(2) << (timeinfo->tm_mon + 1) << "/"
        << (timeinfo->tm_year + 1900);
        return os.str();
    }

    std::string file_size_to_string(int file_size) {
        const double kb = 1024.0;
        const double mb = std::pow(kb, 2);
        const double gb = std::pow(kb, 3);

        std::ostringstream os;
        if (file_size >= gb) {
            os << std::fixed << std::setprecision(2) << file_size / gb << " GB";
        } else if (file_size >= mb) {
            os << std::fixed << std::setprecision(2) << file_size / mb << " MB";
        } else if (file_size >= kb) {
            os << std::fixed << std::setprecision(2) << file_size / kb << " KB";
        } else {
            os << file_size << " B";
        }

        return os.str();
    }
}