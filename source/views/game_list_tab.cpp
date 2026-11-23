#include "views/game_list_tab.hpp"
#include "utils/utils.hpp"
#include "utils/config.hpp"
#include "views/mods_list.hpp"
#include "activity/main_activity.hpp"
#include "views/hint_bar.hpp"
#include "views/oqb_ui.hpp"
#include "views/panel_ajustes.hpp"

#include <borealis.hpp>
#include <chrono>
#include "api/game.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace brls::literals;

std::vector<utils::GameInfo> GameListTab::s_cachedGames;
bool GameListTab::s_cacheLoaded = false;

GameCell::GameCell()
{
    this->inflateFromXMLRes("xml/cells/cell.xml");
}

GameCell* GameCell::create()
{
    return new GameCell();
}

std::string gameSubtitle(const utils::GameInfo& game)
{
    // OQB: lo util en vez del TitleID: cuantos mods tiene ya y cuando se jugo.
    std::string sub;
    if (game.installedMods == 1) sub = "menu/games/installed_one"_i18n;
    else if (game.installedMods > 1) sub = fmt::format(fmt::runtime("menu/games/installed_many"_i18n), game.installedMods);
    std::string played;
    if (game.playedKnown) {
        const uint32_t m = game.minutesAgo, d = m / (60 * 24);
        if (m < 60) played = "menu/games/played_now"_i18n;
        else if (m < 60 * 24) played = fmt::format(fmt::runtime("menu/games/played_hours"_i18n), m / 60);
        else if (d == 1) played = "menu/games/played_yesterday"_i18n;
        else if (d < 60) played = fmt::format(fmt::runtime("menu/games/played_days"_i18n), d);
        else played = fmt::format(fmt::runtime("menu/games/played_months"_i18n), d / 30);
    }
    if (!played.empty()) sub += (sub.empty() ? "" : "  ·  ") + played;
    if (sub.empty()) sub = fmt::format("TitleID : {}", game.tid);
    return sub;
}

void openGame(brls::View* from, const utils::GameInfo& info)
{
    Game game(info.name, info.tid);
    if(game.getGamebananaID() == 0) {
        auto dialog = new brls::Dialog("menu/notify/no_games_gamebanana"_i18n);
        dialog->addButton("hints/ok"_i18n, []() {});
        dialog->open();
        return;
    }
    if(game.getGamebananaID() == -1) {
        auto dialog = new brls::Dialog("menu/notify/request_error"_i18n);
        dialog->addButton("hints/ok"_i18n, []() {});
        dialog->open();
        return;
    }
    auto modListTab = new ModListTab(game);
    from->present(modListTab);
}

brls::RecyclerCell* GameData::cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath indexPath)
{
    auto cell = (GameCell*)recycler->dequeueReusableCell("Cell");
    const auto& game = games[indexPath.row];
    cell->label->setText(game.name);
    cell->subtitle->setText(gameSubtitle(game));
    if (!game.icon.empty())
        cell->image->setImageFromMem(game.icon.data(), game.icon.size());
    return cell;
}

// OQB: Bacán Shop deja aquí el juego que abrir («Mods» en su ficha) y luego
// pasa el control a esta app. Se lee una sola vez y se borra.
static constexpr const char* kPedidoBacan = "sdmc:/config/SimpleModDownloader/abrir-juego.json";

static bool leerPedidoBacan(std::string& tid, std::string& nombre) {
    std::ifstream f(kPedidoBacan);
    if (!f) return false;
    nlohmann::json j = nlohmann::json::parse(f, nullptr, false);
    f.close();
    std::remove(kPedidoBacan);
    if (!j.is_object()) return false;
    // Un pedido viejo (la app no se abrió en su momento) no se atiende.
    const long cuando = j.value("cuando", 0L);
    if (cuando <= 0 || std::time(nullptr) - cuando > 15 * 60) return false;
    tid = j.value("tid", "");
    nombre = j.value("nombre", "");
    for (auto& c : tid) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return tid.size() == 16;
}

void GameListTab::abrirPedidoBacan() {
    std::string tid, nombre;
    if (!leerPedidoBacan(tid, nombre)) return;
    brls::Logger::info("Pedido de Bacan Shop: abrir {} ({})", nombre, tid);
    utils::GameInfo info;
    bool hallado = false;
    for (const auto& g : s_cachedGames) {
        std::string t = g.tid;
        for (auto& c : t) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (t == tid) { info = g; hallado = true; break; }
    }
    if (!hallado) {
        if (nombre.empty()) return;
        info.name = nombre;
        info.tid = tid;
    }
    brls::sync([this, info]() { openGame(this, info); });
}

void GameData::didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath indexPath)
{
    openGame(recycler, games[indexPath.row]);
}

GameData::GameData(std::vector<utils::GameInfo> g) : games(std::move(g)) {
    brls::Logger::debug("{} games found", games.size());
}

int GameData::numberOfSections(brls::RecyclerFrame* recycler) {
    return 1;
}

int GameData::numberOfRows(brls::RecyclerFrame* recycler, int section) {
    return games.size();
}

std::string GameData::titleForHeader(brls::RecyclerFrame* recycler, int section) {
    return "";
}

GameListTab::GameListTab() {
    this->inflateFromXMLRes("xml/tabs/game_list_tab.xml");

    gameData = new GameData();

    this->registerAction("menu/games/game_search"_i18n, brls::ControllerButton::BUTTON_Y, [this](brls::View*) {
        if (isLoading) return true;
        this->openSearch();
        return true;
    }, true);
    this->registerAction("menu/games/h_sort"_i18n, brls::ControllerButton::BUTTON_X, [this](brls::View*) {
        if (isLoading) return true;
        /* En «Con mods en GameBanana», mantener X medio segundo vuelve a revisar
           todos los juegos; una pulsacion corta sigue cambiando el orden. */
        if (this->sortMode == 3) {
            if (this->xEsperando) return true;
            this->xEsperando = true;
            std::weak_ptr<int> vivo = tileGen;
            brls::delay(500, [this, vivo]() {
                if (vivo.expired()) return;
                this->xEsperando = false;
                brls::ControllerState st{};
                brls::Application::getPlatform()->getInputManager()->updateUnifiedControllerState(&st);
                if (st.buttons[brls::BUTTON_X]) {
                    this->refrescarGameBanana();
                    return;
                }
                this->sortMode = 0;
                this->applyView(true);
            });
            return true;
        }
        this->sortMode = (this->sortMode + 1) % 4;
        if (this->sortMode == 3) this->startCheck();
        this->applyView(true);
        return true;
    }, true);
    this->registerAction("menu/games/clear_search"_i18n, brls::ControllerButton::BUTTON_BACK, [this](brls::View*) {
        if (this->filter.empty()) return true;
        this->filter.clear();
        this->applyView(true);
        return true;
    }, true);
    this->registerAction("menu/games/h_install"_i18n, brls::ControllerButton::BUTTON_START, [](brls::View*) {
        openModManagerDialog();
        return true;
    }, true);
    // OQB: R abre los ajustes en un panel a la derecha (como SimpleModManager).
    this->registerAction("menu/games/more"_i18n, brls::ControllerButton::BUTTON_RB, [this](brls::View*) {
        if (!this->panel) {
            this->panel = new PanelAjustes([this] { this->refrescarTema(); });
            this->addView(this->panel);
        }
        this->panel->mostrar();
        return true;
    }, true);

    /* OQB: arriba el titulo en Upheaval, la busqueda (si hay) y la hora. El
       orden se cambia con X y la busqueda con Y (ya no hay chips que enfocar). */
    head->addView(oqb::titulo("menu/games/title"_i18n));
    headSearch = new brls::Box(brls::Axis::ROW);
    headSearch->setMarginLeft(16);
    headSearch->setVisibility(brls::Visibility::GONE);
    head->addView(headSearch);
    auto* spacer = new brls::Box();
    spacer->setGrow(1.0f);
    head->addView(spacer);
    head->addView(new oqb::Estado());
    this->updateHints();

    if (s_cacheLoaded) {
        loading_box->setVisibility(brls::Visibility::GONE);
        this->applyView();
        this->abrirPedidoBacan();
        return;
    }

    loading_label->setText("Loading games...");
    loading_spinner->animate(true);

    isLoading = true;
    brls::Application::blockInputs();

    loadThread = std::thread([this]() {
        auto games = utils::getInstalledGames();
        s_cachedGames = games;
        auto* newData = new GameData(std::move(games));
        brls::sync([this, newData]() {
            isLoading = false;
            brls::Application::unblockInputs();
            s_cacheLoaded = true;
            delete newData;
            loading_spinner->animate(false);
            loading_box->setVisibility(brls::Visibility::GONE);
            this->applyView();
            this->abrirPedidoBacan();
        });
    });

    #ifndef NDEBUG
    cfg::Config config;
    if (config.getWireframe()) {
        this->setWireframeEnabled(true);
        for(auto& view : this->getChildren()) {
            view->setWireframeEnabled(true);
        }
    }
    #endif
}

namespace {
    std::string lower(std::string s) {
        for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }
}

// OQB: la lista que se ve, filtrada por el buscador y en el orden elegido.
void GameListTab::applyView(bool focus) {
    std::vector<utils::GameInfo> list;
    const std::string f = lower(this->filter);
    // «Con página»: lo que la cache sabe de cada juego, leida una sola vez.
    const auto estados = this->sortMode == 3 ? Game::estadosGameBanana() : std::map<std::string, int>{};
    for (const auto& g : s_cachedGames) {
        if (!f.empty() && lower(g.name).find(f) == std::string::npos) continue;
        if (this->sortMode == 3) {
            auto it = estados.find(g.tid);
            if (it == estados.end() || it->second != 1) continue;
        }
        list.push_back(g);
    }
    auto byName = [](const utils::GameInfo& a, const utils::GameInfo& b) { return lower(a.name) < lower(b.name); };
    if (this->sortMode == 1)
        std::stable_sort(list.begin(), list.end(), byName);
    else if (this->sortMode == 2)
        std::stable_sort(list.begin(), list.end(), [&](const utils::GameInfo& a, const utils::GameInfo& b) {
            if (a.installedMods != b.installedMods) return a.installedMods > b.installedMods;
            return byName(a, b);
        });
    else
        std::stable_sort(list.begin(), list.end(), [&](const utils::GameInfo& a, const utils::GameInfo& b) {
            if (a.lastPlayed != b.lastPlayed) return a.lastPlayed > b.lastPlayed;
            return byName(a, b);
        });

    if (list.empty() && !f.empty()) {
        auto dialog = new brls::Dialog(fmt::format(fmt::runtime("menu/games/no_match"_i18n), this->filter));
        dialog->addButton("hints/ok"_i18n, []() {});
        dialog->open();
        this->filter.clear();
        this->applyView(focus);
        return;
    }

    /* OQB: cuadricula de portadas, 5 por fila. Cada portada se puede enfocar
       y con A abre los mods del juego. */
    constexpr int PER_ROW = 5;
    grid->clearViews();
    brls::Box* row = nullptr;
    brls::View* first = nullptr;
    if (list.empty()) {
        // Sin portadas: algo enfocable donde dejar el foco (la portada que lo tenia se borro).
        auto* vacio = new brls::Box(brls::Axis::ROW);
        vacio->setFocusable(true);
        vacio->setHideHighlight(true);
        vacio->addView(oqb::texto(this->checking ? "menu/games/gb_checking_empty"_i18n : "menu/games/gb_none"_i18n, 18, oqb::pal::tenue));
        grid->addView(vacio);
        first = vacio;
        focus = true;
    }
    for (size_t i = 0; i < list.size(); ++i) {
        if (i % PER_ROW == 0) {
            row = new brls::Box(brls::Axis::ROW);
            row->setMarginBottom(16);
            grid->addView(row);
        }
        auto* tile = this->makeTile(list[i], i, list.size());
        if (i % PER_ROW != PER_ROW - 1) tile->setMarginRight(16);
        row->addView(tile);
        if (!first) first = tile;
    }
    this->shown = std::move(list);

    /* Los iconos se ponen por tandas, unos pocos por cuadro: decodificar los
       100 de golpe congelaba la pantalla al abrir. */
    std::vector<std::pair<brls::Image*, size_t>> pending;
    {
        size_t idx = 0;
        for (auto* r : grid->getChildren())
            for (auto* t : static_cast<brls::Box*>(r)->getChildren()) {
                auto* img = static_cast<brls::Image*>(static_cast<brls::Box*>(t)->getChildren()[0]);
                if (idx < this->shown.size() && !this->shown[idx].icon.empty()) pending.emplace_back(img, idx);
                idx++;
            }
    }
    const int gen = ++(*tileGen);
    this->loadTileImages(std::move(pending), 0, gen);
    gridScroll->setContentOffsetY(0, false);
    if (focus && first) brls::Application::giveFocus(first);
    this->updateHints();
}

// Cuando se jugo, corto para una etiqueta de la portada.
static std::string playedShort(const utils::GameInfo& game) {
    if (!game.playedKnown) return "";
    const uint32_t m = game.minutesAgo, d = m / (60 * 24);
    if (m < 60 * 24) return "menu/games/played_short_today"_i18n;
    if (d == 1) return "menu/games/played_short_yesterday"_i18n;
    if (d < 60) return fmt::format(fmt::runtime("menu/games/played_short_days"_i18n), d);
    return fmt::format(fmt::runtime("menu/games/played_short_months"_i18n), d / 30);
}

brls::View* GameListTab::makeTile(const utils::GameInfo& game, size_t index, size_t total) {
    // 5 portadas en 1216 px (1280 menos los margenes), como la maqueta.
    std::string marca;
    if (game.installedMods == 1) marca = "menu/games/installed_one"_i18n;
    else if (game.installedMods > 1) marca = fmt::format(fmt::runtime("menu/games/installed_many"_i18n), game.installedMods);
    auto* tile = new oqb::Portada(230, 226, game.name, playedShort(game), "", marca);

    // El contador de abajo dice en que portada se esta.
    tile->getFocusEvent()->subscribe([this, index, total](brls::View*) {
        if (this->counter) this->counter->setText(fmt::format("{} / {}", index + 1, this->shown.size()));
    });

    utils::GameInfo copy = game;
    tile->registerClickAction([tile, copy](brls::View*) {
        openGame(tile, copy);
        return true;
    });
    tile->addGestureRecognizer(new brls::TapGestureRecognizer(tile));
    return tile;
}

void GameListTab::loadTileImages(std::vector<std::pair<brls::Image*, size_t>> pending, size_t from, int gen) {
    if (gen != *tileGen) return;  // la cuadricula se rehizo: estas imagenes ya no existen
    const size_t hasta = std::min(pending.size(), from + 6);
    for (size_t i = from; i < hasta; ++i) {
        const auto& icon = this->shown[pending[i].second].icon;
        pending[i].first->setImageFromMem(icon.data(), icon.size());
    }
    if (hasta >= pending.size()) return;
    std::weak_ptr<int> vivo = tileGen;
    brls::sync([this, pending = std::move(pending), hasta, gen, vivo]() mutable {
        if (vivo.expired()) return;
        this->loadTileImages(std::move(pending), hasta, gen);
    });
}

void GameListTab::openSearch() {
    brls::Application::getImeManager()->openForText([this](std::string text) {
        this->filter = text;
    }, "menu/games/game_search"_i18n, "", 40, this->filter);
    this->applyView(true);
}

// La pildora de arriba: la busqueda y, en «Con página», cuanto lleva revisado.
void GameListTab::updateHead() {
    headSearch->clearViews();
    std::string t;
    if (!this->filter.empty())
        t = fmt::format(fmt::runtime("menu/games/search_chip"_i18n), this->filter, this->shown.size(), s_cachedGames.size());
    if (this->sortMode == 3 && this->checking)
        t += (t.empty() ? "" : "   ") + fmt::format(fmt::runtime("menu/games/gb_checking"_i18n), this->checkDone.load(), this->checkTotal);
    else if (this->sortMode == 3) {
        // Solo los juegos de esta consola (la cache puede tener otros ya desinstalados).
        const auto estados = Game::estadosGameBanana();
        int con = 0;
        for (const auto& g : s_cachedGames) {
            auto e = estados.find(g.tid);
            if (e != estados.end() && e->second == 1) con++;
        }
        t += (t.empty() ? "" : "   ") + fmt::format(fmt::runtime("menu/games/gb_summary"_i18n), con, s_cachedGames.size()) + "  ·  " + "menu/games/gb_hold"_i18n;
    }
    headSearch->setVisibility(t.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    if (!t.empty()) headSearch->addView(oqb::chip(t, oqb::pal::chip, oqb::pal::cuerpo, false, 14));
}

void GameListTab::updateHints() {
    this->updateHead();
    const char* keys[] = {"menu/games/sort_recent", "menu/games/sort_az", "menu/games/sort_installed", "menu/games/sort_gb"};
    std::string sortName = brls::getStr(keys[this->sortMode]);
    if (!sortName.empty()) sortName[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(sortName[0])));
    const std::string count = this->shown.empty() ? "" : fmt::format("1 / {}", this->shown.size());
    this->counter = fillHintBar(hintBar, {{"A", "menu/games/h_open"_i18n},
                          {"Y", "menu/games/h_search"_i18n},
                          {"X", fmt::format(fmt::runtime("menu/games/sort_label"_i18n), sortName)},
                          {"+", "menu/games/h_install"_i18n},
                          {"R", "menu/panel/title_hint"_i18n},
                          {"−", this->filter.empty() ? "" : "menu/games/h_clear"_i18n},
                          {"B", "menu/games/h_exit"_i18n}}, count);
}

/* Revisa en segundo plano los juegos que la cache no conoce, uno cada 2 s
   (pocas consultas a GameBanana: la regla de no hacer rafagas). Al terminar,
   si sigue elegido «Con página», la cuadricula se arma con todos. */
void GameListTab::startCheck() {
    if (this->checking) return;
    const auto estados = Game::estadosGameBanana();
    std::vector<std::pair<std::string, std::string>> faltan;
    for (const auto& g : s_cachedGames)
        if (!estados.count(g.tid)) faltan.emplace_back(g.name, g.tid);
    if (faltan.empty()) return;
    if (checkThread.joinable()) checkThread.join();
    this->checking = true;
    this->checkStop = false;
    this->checkDone = 0;
    this->checkTotal = int(faltan.size());
    std::weak_ptr<int> vivo = tileGen;
    checkThread = std::thread([this, faltan, vivo]() {
        for (const auto& [nombre, tid] : faltan) {
            if (this->checkStop) return;
            Game g(nombre, tid);  // busca y deja el resultado en la cache
            this->checkDone++;
            const bool conMods = Game::estadoGameBanana(tid) == 1;
            brls::sync([this, vivo, tid, conMods]() {
                if (vivo.expired()) return;
                if (conMods) this->agregarPortada(tid);  // aparece al momento, sin rearmar la cuadricula
                this->updateHead();
            });
            for (int i = 0; i < 20 && !this->checkStop; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        brls::sync([this, vivo]() {
            if (vivo.expired()) return;
            this->checking = false;
            this->updateHead();  // el resumen: «40 de 160 juegos tienen mods en GameBanana»
        });
    });
}

// Olvida lo guardado de los juegos de la consola y los revisa de nuevo.
void GameListTab::refrescarGameBanana() {
    if (this->checking) {
        brls::Application::notify("menu/games/gb_already"_i18n);
        return;
    }
    std::vector<std::string> tids;
    for (const auto& g : s_cachedGames) tids.push_back(g.tid);
    Game::olvidarGameBanana(tids);
    brls::Application::notify("menu/games/gb_refresh"_i18n);
    this->startCheck();
    this->applyView(true);  // vacia (con «Revisando…»); las portadas vuelven a aparecer de a una
}

/* Una portada mas al final de la cuadricula (la revision encontro otro juego
   con mods). Si la cuadricula estaba vacia (solo el mensaje), se arma entera. */
void GameListTab::agregarPortada(const std::string& tid) {
    if (this->sortMode != 3 || !this->filter.empty()) return;
    auto it = std::find_if(s_cachedGames.begin(), s_cachedGames.end(), [&](const utils::GameInfo& g) { return g.tid == tid; });
    if (it == s_cachedGames.end()) return;
    if (this->shown.empty()) {
        this->applyView(true);
        return;
    }
    constexpr int PER_ROW = 5;
    const size_t i = this->shown.size();
    this->shown.push_back(*it);
    brls::Box* row = nullptr;
    if (i % PER_ROW == 0) {
        row = new brls::Box(brls::Axis::ROW);
        row->setMarginBottom(16);
        grid->addView(row);
    } else {
        row = static_cast<brls::Box*>(grid->getChildren().back());
    }
    auto* tile = static_cast<oqb::Portada*>(this->makeTile(*it, i, this->shown.size()));
    if (i % PER_ROW != PER_ROW - 1) tile->setMarginRight(16);
    row->addView(tile);
    if (!it->icon.empty()) tile->getImage()->setImageFromMem(it->icon.data(), it->icon.size());
}

// Tema nuevo: lo que se armo con los colores viejos se repinta (las portadas
// se dibujan con el tema actual; su borde y la barra de abajo, no).
void GameListTab::refrescarTema() {
    for (auto* fila : grid->getChildren())
        if (auto* f = dynamic_cast<brls::Box*>(fila))
            for (auto* t : f->getChildren())
                if (auto* p = dynamic_cast<oqb::Portada*>(t)) p->setBorderColor(oqb::pal::bordeSuave);
    this->updateHints();
}

brls::View* GameListTab::create() {
    return new GameListTab();
}
