#include "api/game.hpp"
#include "api/net.hpp"
#include "utils/config.hpp"
#include <curl/curl.h>

#include <regex>
#include <ctime>
#include <fstream>
#include <mutex>
#include <algorithm>
#include <cctype>

/* OQB: lo que GameBanana dice de cada juego (su id, nombre, portada y
   categorias) se guarda por TitleID una semana. Asi abrir un juego ya visto no
   hace dos consultas antes de pedir la lista de mods. */
static const char* GB_CACHE = "sdmc:/config/SimpleModDownloader/gamebanana.json";

/* La revisión en segundo plano (filtro «Con página») y la pantalla pueden
   leer y escribir la caché a la vez: un candado para las dos cosas. */
static std::mutex gbMutex;

static nlohmann::json leerCacheGb() {
    std::lock_guard<std::mutex> lock(gbMutex);
    std::ifstream f(GB_CACHE);
    if (!f) return nlohmann::json::object();
    nlohmann::json j = nlohmann::json::parse(f, nullptr, false);
    return j.is_object() ? j : nlohmann::json::object();
}

// Guarda (o reemplaza) lo de un juego, releyendo la caché bajo el candado.
static void guardarEnCacheGb(const std::string& tid, const nlohmann::json& dato) {
    std::lock_guard<std::mutex> lock(gbMutex);
    nlohmann::json cache = nlohmann::json::object();
    {
        std::ifstream f(GB_CACHE);
        if (f) {
            cache = nlohmann::json::parse(f, nullptr, false);
            if (!cache.is_object()) cache = nlohmann::json::object();
        }
    }
    cache[tid] = dato;
    std::ofstream f(GB_CACHE, std::ios::trunc);
    f << cache.dump();
}

int Game::estadoGameBanana(const std::string& tid) {
    const nlohmann::json cache = leerCacheGb();
    if (!cache.contains(tid) || !cache[tid].is_object()) return -1;
    const auto& c = cache[tid];
    const long ahora = static_cast<long>(std::time(nullptr));
    if (c.value("v", 0) != 2 || ahora - c.value("cuando", 0L) >= 7 * 24 * 3600) return -1;
    if (c.value("id", 0) > 0 && !c.contains("mods")) return -1;
    return (c.value("id", 0) > 0 && c.value("mods", 0) > 0) ? 1 : 0;
}

void Game::olvidarGameBanana(const std::vector<std::string>& tids) {
    std::lock_guard<std::mutex> lock(gbMutex);
    nlohmann::json cache = nlohmann::json::object();
    {
        std::ifstream f(GB_CACHE);
        if (f) {
            cache = nlohmann::json::parse(f, nullptr, false);
            if (!cache.is_object()) cache = nlohmann::json::object();
        }
    }
    for (const auto& t : tids) cache.erase(t);
    std::ofstream f(GB_CACHE, std::ios::trunc);
    f << cache.dump();
}

std::map<std::string, int> Game::estadosGameBanana() {
    const nlohmann::json cache = leerCacheGb();
    const long ahora = static_cast<long>(std::time(nullptr));
    std::map<std::string, int> out;
    for (auto it = cache.begin(); it != cache.end(); ++it) {
        const auto& c = it.value();
        if (!c.is_object() || c.value("v", 0) != 2 || ahora - c.value("cuando", 0L) >= 7 * 24 * 3600) continue;
        if (c.value("id", 0) > 0 && !c.contains("mods")) continue;  // guardado antes de contar mods: se revisa
        out[it.key()] = (c.value("id", 0) > 0 && c.value("mods", 0) > 0) ? 1 : 0;
    }
    return out;
}

Game::Game(const std::string& m_title, const std::string& m_tid) {
    title = m_title;
    tid = m_tid;

    const long ahora = static_cast<long>(std::time(nullptr));
    nlohmann::json cache = leerCacheGb();
    if (!tid.empty() && cache.contains(tid) && cache[tid].is_object()) {
        const auto& c = cache[tid];
        if (c.value("v", 0) == 2 && c.value("id", 0) > 0 && c.contains("mods") && ahora - c.value("cuando", 0L) < 7 * 24 * 3600 && c.contains("categorias") && c["categorias"].is_array()) {
            modCount = c.value("mods", 0);
            title = c.value("titulo", title);
            gamebananaID = c.value("id", 0);
            bannerURL = c.value("portada", "");
            int index = 0;
            for (const auto& cat : c["categorias"])
                if (cat.is_array() && cat.size() == 2) categories.push_back(Category(cat[0].get<std::string>(), cat[1].get<int>(), index++));
            return;
        }
        // Ya se sabe que no tiene pagina (se revisa otra vez a la semana).
        if (c.value("v", 0) == 2 && c.value("id", -1) == 0 && ahora - c.value("cuando", 0L) < 7 * 24 * 3600) {
            gamebananaID = 0;
            return;
        }
    }

    searchGame();
    parseJson();
    if (gamebananaID > 0)
        loadCategories();

    if (gamebananaID > 0 && !tid.empty()) {
        nlohmann::json cats = nlohmann::json::array();
        for (auto& c : categories) cats.push_back({c.getName(), c.getID()});
        guardarEnCacheGb(tid, {{"v", 2}, {"id", gamebananaID}, {"titulo", title}, {"portada", bannerURL}, {"categorias", cats}, {"mods", modCount}, {"cuando", ahora}});
    }
    // La busqueda contesto y ningun resultado coincide: se recuerda que no tiene pagina.
    else if (gamebananaID == 0 && !tid.empty() && !json.empty()) {
        guardarEnCacheGb(tid, {{"v", 2}, {"id", 0}, {"cuando", ahora}});
    }
}

void Game::searchGame() {
    auto curl = curl_easy_init();
    std::string title_url;
    title_url = curl_easy_escape(curl, title.c_str(), title.length());
    curl_easy_cleanup(curl);

    cfg::Config config;
    std::string endpoint = config.getStrictSearch() ?
        "https://gamebanana.com/apiv11/Util/Game/NameMatch?_sName={}" :
        "https://gamebanana.com/apiv11/Util/Search/Results?_sModelName=Game&_sOrder=best_match&_sSearchString={}%20%28Switch%29";

    try {
        json = net::downloadRequest(fmt::format(endpoint, title_url));
    } catch (const std::exception& e) {
        brls::Logger::error("Failed to search for game: " + title + " - " + e.what());
        gamebananaID = -1;
        return;
    }
    
    
    // OQB: la búsqueda estricta pide el nombre exacto; si no da nada, se
    // prueba la normal (por ejemplo, nombres con «Edition» o subtítulos).
    if (config.getStrictSearch()) {
        bool vacio = json.empty();
        if (!vacio) {
            try { vacio = json.at("_aMetadata").at("_nRecordCount").get<int>() == 0; } catch (...) { vacio = true; }
        }
        if (vacio) {
            try {
                json = net::downloadRequest(fmt::format(
                    "https://gamebanana.com/apiv11/Util/Search/Results?_sModelName=Game&_sOrder=best_match&_sSearchString={}%20%28Switch%29",
                    title_url));
                strictFallback = true;
            } catch (const std::exception& e) {
                brls::Logger::error("Fallback search failed: " + title + " - " + e.what());
            }
        }
    }

    if(json.empty()) {
        brls::Logger::error("Failed to search for game: " + title);
    }
}

/* OQB: el nombre normalizado para comparar: minusculas, sin signos ni
   espacios, y sin "(Switch)", "Nintendo Switch" ni "Switch Edition" al final. */
static std::string normalizar(const std::string& n) {
    std::string out;
    for (unsigned char c : n) {
        if (std::isalnum(c)) out += static_cast<char>(std::tolower(c));
        else if (c >= 0x80) out += static_cast<char>(c);  // letras con acento (UTF-8) quedan
    }
    for (const char* cola : {"nintendoswitchedition", "switchedition", "nintendoswitch", "switch"}) {
        const std::string t(cola);
        if (out.size() > t.size() + 2 && out.compare(out.size() - t.size(), t.size(), t) == 0) {
            out.erase(out.size() - t.size());
            break;
        }
    }
    return out;
}

/* OQB: antes se tomaba el primer resultado de la busqueda. Con juegos que no
   tienen pagina en GameBanana (12 orbits, 51 Worldwide Games, A Good
   Snowman...), el primero era la pagina general «Nintendo Switch» y todos
   mostraban los mismos mods. Ahora el nombre tiene que coincidir: igual, o uno
   contiene al otro y comparten al menos la mitad del largo (Saints Row: The
   Third - The Full Package / Saints Row: The Third). Si ninguno coincide, el
   juego queda sin pagina (la app avisa que no esta en GameBanana). */
void Game::parseJson() {
    if(json.empty() || !json.contains("_aRecords") || json.at("_aMetadata").at("_nRecordCount").get<int>() == 0) {
        return;
    }
    const std::string buscado = normalizar(title);
    if (buscado.empty()) return;

    int mejor = -1;
    double puntaje = 0;
    const auto& records = json.at("_aRecords");
    for (size_t i = 0; i < records.size(); ++i) {
        const std::string nombre = records[i].value("_sName", "");
        const std::string n = normalizar(nombre);
        if (n.empty()) continue;
        double p = 0;
        if (n == buscado) p = 2.0;
        else if (n.find(buscado) != std::string::npos || buscado.find(n) != std::string::npos) {
            const double corto = std::min(n.size(), buscado.size()), largo = std::max(n.size(), buscado.size());
            if (corto / largo >= 0.5) p = corto / largo;
        }
        // La pagina general de la consola nunca es la de un juego.
        if (records[i].value("_idRow", 0) == 6384 && p < 2.0) p = 0;
        // Ante dos iguales, la que dice Switch (juegos multiplataforma).
        if (p > 0 && nombre.find("Switch") != std::string::npos) p += 0.01;
        if (p > puntaje) { puntaje = p; mejor = int(i); }
    }
    if (mejor < 0) {
        brls::Logger::info("GameBanana: ningun resultado coincide con {}", title);
        return;
    }
    const auto& record = records[mejor];
    title = record.at("_sName").get<std::string>();
    gamebananaID = record.at("_idRow").get<int>();
}

void Game::loadCategories() {
    json = net::downloadRequest(fmt::format("https://gamebanana.com/apiv11/Game/{}/ProfilePage", gamebananaID));

    if(json.empty() || json.find("_aModRootCategories") == json.end()) {
        brls::Logger::error("Failed to load tags for game: {}", title);
        return;
    }

    for (const auto& image : json.at("_aPreviewMedia").at("_aImages")) {
        if (image.at("_sType").get<std::string>() == "banner") {
            bannerURL = image.at("_sUrl").get<std::string>();
            break;
        }
    }

    int index = 0;
    modCount = 0;
    for(auto tag : json.at("_aModRootCategories")) {
        // OQB: cuantos mods tiene la pagina (hay paginas de juegos sin ningun mod).
        if (tag.contains("_nItemCount") && tag["_nItemCount"].is_number_integer()) modCount += tag["_nItemCount"].get<int>();
        categories.push_back(Category(tag.at("_sName").get<std::string>(), tag.at("_idRow").get<int>(), index));
        index++;
    }
}