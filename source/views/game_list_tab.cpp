#include "views/game_list_tab.hpp"
#include "utils/utils.hpp"
#include "utils/config.hpp"
#include "views/mods_list.hpp"
#include "activity/main_activity.hpp"

#include <borealis.hpp>
#include <algorithm>
#include <cctype>

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
        brls::Application::getImeManager()->openForText([this](std::string text) {
            this->filter = text;
        }, "menu/games/game_search"_i18n, "", 40, this->filter);
        this->applyView(true);
        return true;
    });
    this->registerAction("", brls::ControllerButton::BUTTON_X, [this](brls::View*) {
        if (isLoading) return true;
        this->sortMode = (this->sortMode + 1) % 3;
        this->applyView(true);
        return true;
    });
    this->registerAction("menu/games/clear_search"_i18n, brls::ControllerButton::BUTTON_BACK, [this](brls::View*) {
        if (this->filter.empty()) return true;
        this->filter.clear();
        this->applyView(true);
        return true;
    });
    this->registerAction("menu/games/more"_i18n, brls::ControllerButton::BUTTON_RB, [](brls::View*) {
        brls::Application::pushActivity(new MoreActivity());
        return true;
    });
    this->updateHints();

    if (s_cacheLoaded) {
        loading_box->setVisibility(brls::Visibility::GONE);
        this->applyView();
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
    for (const auto& g : s_cachedGames)
        if (f.empty() || lower(g.name).find(f) != std::string::npos) list.push_back(g);
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
    constexpr int PER_ROW = 6;
    grid->clearViews();
    brls::Box* row = nullptr;
    brls::View* first = nullptr;
    for (size_t i = 0; i < list.size(); ++i) {
        if (i % PER_ROW == 0) {
            row = new brls::Box(brls::Axis::ROW);
            row->setMarginBottom(18);
            grid->addView(row);
        }
        auto* tile = this->makeTile(list[i]);
        row->addView(tile);
        if (!first) first = tile;
    }
    this->shown = std::move(list);
    gridScroll->setContentOffsetY(0, false);
    if (focus && first) brls::Application::giveFocus(first);
    this->updateHints();
}

brls::View* GameListTab::makeTile(const utils::GameInfo& game) {
    constexpr float W = 184.0f;
    auto* tile = new brls::Box(brls::Axis::COLUMN);
    tile->setWidth(W);
    tile->setMarginRight(16);
    tile->setFocusable(true);
    tile->setHighlightCornerRadius(12);
    tile->setPadding(6, 6, 8, 6);

    auto* image = new brls::Image();
    image->setWidth(W - 12);
    image->setHeight(W - 12);
    image->setCornerRadius(10);
    image->setScalingType(brls::ImageScalingType::FILL);
    if (!game.icon.empty()) image->setImageFromMem(game.icon.data(), game.icon.size());
    tile->addView(image);

    auto* name = new brls::Label();
    name->setText(game.name);
    name->setFontSize(16);
    name->setSingleLine(true);
    name->setMarginTop(8);
    name->setWidth(W - 12);
    tile->addView(name);

    const std::string sub = gameSubtitle(game);
    auto* subLabel = new brls::Label();
    subLabel->setText(game.installedMods > 0 ? sub.substr(0, sub.find("  ·  ")) : (game.playedKnown ? sub : ""));
    subLabel->setFontSize(13);
    subLabel->setSingleLine(true);
    subLabel->setWidth(W - 12);
    subLabel->setMarginTop(2);
    subLabel->setTextColor(game.installedMods > 0 ? nvgRGB(46, 170, 110) : nvgRGB(140, 140, 140));
    tile->addView(subLabel);

    utils::GameInfo copy = game;
    tile->registerClickAction([tile, copy](brls::View*) {
        openGame(tile, copy);
        return true;
    });
    tile->addGestureRecognizer(new brls::TapGestureRecognizer(tile));
    return tile;
}

// Chip de la barra superior (no se elige: el orden se cambia con X).
static brls::Box* chipLabel(const std::string& text, bool on) {
    auto* chip = new brls::Box(brls::Axis::ROW);
    chip->setCornerRadius(16);
    chip->setPadding(6, 14, 6, 14);
    chip->setMarginLeft(8);
    chip->setBackgroundColor(on ? nvgRGBA(0, 160, 230, 70) : nvgRGBA(128, 128, 128, 30));
    auto* l = new brls::Label();
    l->setText(text);
    l->setFontSize(15);
    l->setSingleLine(true);
    chip->addView(l);
    return chip;
}

void GameListTab::updateHints() {
    // OQB: barra superior como el mockup (titulo, buscador y orden).
    headTitle->setText("menu/games/header"_i18n);
    headSearchBox->setCornerRadius(10);
    headSearchBox->setBackgroundColor(nvgRGBA(128, 128, 128, 30));
    headSearch->setText(this->filter.empty() ? "menu/games/game_search"_i18n + " (Y)"
                                             : fmt::format("{}: {}  ·  {}", "menu/games/game_search"_i18n, this->filter, this->shown.size()));
    headSearch->setTextColor(this->filter.empty() ? nvgRGB(140, 140, 140) : nvgRGB(230, 230, 230));
    headChips->clearViews();
    const char* chipKeys[] = {"menu/games/sort_recent", "menu/games/sort_az", "menu/games/sort_installed"};
    for (int i = 0; i < 3; ++i) headChips->addView(chipLabel(brls::getStr(chipKeys[i]), i == this->sortMode));

    const char* keys[] = {"menu/games/sort_recent", "menu/games/sort_az", "menu/games/sort_installed"};
    this->updateActionHint(brls::ControllerButton::BUTTON_X, fmt::format(fmt::runtime("menu/games/game_sort"_i18n), brls::getStr(keys[this->sortMode])));
    this->updateActionHint(brls::ControllerButton::BUTTON_Y, this->filter.empty() ? "menu/games/game_search"_i18n : "menu/games/game_search"_i18n + ": " + this->filter);
    this->updateActionHint(brls::ControllerButton::BUTTON_BACK, this->filter.empty() ? "" : "menu/games/clear_search"_i18n);
}

brls::View* GameListTab::create() {
    return new GameListTab();
}
