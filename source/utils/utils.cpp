#include "utils/utils.hpp"

#include <switch.h>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <regex>
#include <borealis.hpp>
#include <SimpleIniParser.hpp>
#include <cstring>
#include <algorithm>
#include <memory>
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

        /* OQB: cache de nombres e iconos. Pedir el control de cada juego a la
           consola es lo lento; con la cache solo se pide el de los juegos nuevos
           y se olvidan los que ya no estan. */
        const std::string cacheDir = "sdmc:/config/SimpleModDownloader/";
        const std::string cachePath = cacheDir + "juegos.json";
        const std::string iconDir = cacheDir + "iconos/";
        nlohmann::json cache = nlohmann::json::object();
        {
            std::ifstream f(cachePath);
            if (f) {
                cache = nlohmann::json::parse(f, nullptr, false);
                if (!cache.is_object()) cache = nlohmann::json::object();
            }
        }
        nlohmann::json nuevo = nlohmann::json::object();
        std::error_code ec;
        std::filesystem::create_directories(iconDir, ec);
        bool cambio = false;

        rc = nsListApplicationRecord(records, 64000, 0, &recordCount);
        for (auto i = 0; i < recordCount; ++i) {
            uint64_t tid = records[i].application_id;
            /* Antes se saltaban los que no piden usuario al abrir, pero hay juegos
               que no lo piden (Saints Row, por ejemplo) y no salian. Ahora solo se
               saltan los forwarders de homebrew: sus ids no empiezan con 01. */
            if ((tid >> 56) != 0x01)
                continue;

            GameInfo info;
            info.tid = formatApplicationId(tid);
            const std::string iconPath = iconDir + info.tid + ".jpg";

            if (cache.contains(info.tid) && cache[info.tid].is_string() && std::filesystem::exists(iconPath, ec)) {
                info.name = cache[info.tid].get<std::string>();
                std::ifstream ic(iconPath, std::ios::binary);
                info.icon.assign(std::istreambuf_iterator<char>(ic), std::istreambuf_iterator<char>());
            }
            else {
                rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, tid, controlData, sizeof(*controlData), &controlSize);
                if (R_FAILED(rc) || controlSize < sizeof(controlData->nacp))
                    continue;
                decompressNacpNames(&controlData->nacp);
                rc = nacpGetLanguageEntrySpecialLanguage(&controlData->nacp, &langEntry, SetLanguage_ENUS);
                if (R_FAILED(rc) || !langEntry || !langEntry->name[0])
                    continue;
                info.name = std::string(langEntry->name, strnlen(langEntry->name, sizeof(langEntry->name)));
                const size_t iconSize = controlSize - sizeof(controlData->nacp);
                info.icon.assign(controlData->icon, controlData->icon + iconSize);
                std::ofstream oc(iconPath, std::ios::binary | std::ios::trunc);
                oc.write(reinterpret_cast<const char*>(info.icon.data()), info.icon.size());
                cambio = true;
            }
            nuevo[info.tid] = info.name;

            // Cuantos mods tiene ya en la carpeta de SimpleModManager.
            for (const auto& e : std::filesystem::directory_iterator(modFolderFor(info.name), ec))
                if (e.is_directory(ec)) info.installedMods++;

            games.push_back(std::move(info));
        }
        if (cambio || nuevo.size() != cache.size()) {
            for (auto it = cache.begin(); it != cache.end(); ++it)
                if (!nuevo.contains(it.key())) std::filesystem::remove(iconDir + it.key() + ".jpg", ec);
            std::ofstream f(cachePath, std::ios::trunc);
            f << nuevo.dump();
        }

        // Cuando se jugo cada uno por ultima vez, en una sola consulta (antes una por juego).
        if (!games.empty() && R_SUCCEEDED(pdmqryInitialize())) {
            std::vector<u64> ids;
            for (auto& g : games) ids.push_back(std::stoull(g.tid, nullptr, 16));
            std::vector<PdmLastPlayTime> last(ids.size());
            s32 total = 0;
            if (R_SUCCEEDED(pdmqryQueryLastPlayTime(false, last.data(), ids.data(), ids.size(), &total))) {
                for (s32 k = 0; k < total; ++k) {
                    if (!last[k].flag) continue;
                    for (auto& g : games) {
                        if (std::stoull(g.tid, nullptr, 16) != last[k].application_id) continue;
                        g.lastPlayed = last[k].timestamp_user;
                        g.minutesAgo = last[k].last_played_minutes;
                        g.playedKnown = true;
                        break;
                    }
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

namespace utils {

std::string installedVersion(const std::string& tid) {
    if (tid.size() != 16) return "";
    auto control = std::make_unique<NsApplicationControlData>();
    u64 size = 0;
    if (R_FAILED(nsGetApplicationControlData(NsApplicationControlSource_Storage, std::stoull(tid, nullptr, 16), control.get(), sizeof(NsApplicationControlData), &size)))
        return "";
    std::string v(control->nacp.display_version, strnlen(control->nacp.display_version, sizeof(control->nacp.display_version)));
    if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) v.erase(0, 1);
    return v;
}

std::vector<std::string> mentionedVersions(const std::string& text) {
    static const std::regex tres(R"((?:^|[^\d.])v?(\d{1,2}\.\d{1,2}\.\d{1,3})(?![\d.]))", std::regex::icase);
    static const std::regex dos(R"((?:version|ver\.?|update|v)\s*(\d{1,2}\.\d{1,2})(?![\d.]))", std::regex::icase);
    std::vector<std::pair<size_t, std::string>> found;
    for (const auto* re : {&tres, &dos})
        for (auto it = std::sregex_iterator(text.begin(), text.end(), *re); it != std::sregex_iterator(); ++it)
            found.emplace_back((*it).position(1), (*it)[1].str());
    std::sort(found.begin(), found.end());
    std::vector<std::string> out;
    for (auto& [pos, v] : found) {
        if (std::find(out.begin(), out.end(), v) != out.end()) continue;
        out.push_back(v);
        if (out.size() == 4) break;
    }
    return out;
}

bool nameMatchesVersion(const std::string& name, const std::string& version) {
    if (version.empty()) return false;
    std::vector<std::string> partes;
    std::string p;
    for (char c : version) {
        if (c == '.') { partes.push_back(p); p.clear(); }
        else if (std::isdigit(static_cast<unsigned char>(c))) p += c;
        else return false;
    }
    partes.push_back(p);
    if (partes.size() < 2) return false;
    std::vector<std::string> formas;
    for (const char* sep : {".", "_", "-", ""}) {
        std::string f;
        for (size_t i = 0; i < partes.size(); ++i) f += (i ? sep : "") + partes[i];
        if (std::string(sep).empty() && f.size() < 3) continue;  // "12" seria demasiado comun
        formas.push_back(f);
    }
    for (const auto& f : formas) {
        for (size_t pos = name.find(f); pos != std::string::npos; pos = name.find(f, pos + 1)) {
            const bool antes = pos == 0 || !std::isdigit(static_cast<unsigned char>(name[pos - 1]));
            const size_t fin = pos + f.size();
            const bool despues = fin >= name.size() || !std::isdigit(static_cast<unsigned char>(name[fin]));
            if (antes && despues) return true;
        }
    }
    return false;
}

}  // namespace utils
