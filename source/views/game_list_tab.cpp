#include "views/game_list_tab.hpp"
#include "utils/utils.hpp"
#include "utils/config.hpp"
#include "views/mods_list.hpp"

#include <borealis.hpp>

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

brls::RecyclerCell* GameData::cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath indexPath)
{
    auto cell = (GameCell*)recycler->dequeueReusableCell("Cell");
    const auto& game = games[indexPath.row];
    cell->label->setText(game.name);
    cell->subtitle->setText(fmt::format("TitleID : {}", game.tid));
    if (!game.icon.empty())
        cell->image->setImageFromMem(game.icon.data(), game.icon.size());
    return cell;
}

void GameData::didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath indexPath)
{
    Game game(games[indexPath.row].name, games[indexPath.row].tid);
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
    recycler->present(modListTab);
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

GameListTab::~GameListTab() {
    if (isLoading) {
        cancelled->store(true);
        brls::Application::unblockInputs();
    }
}

GameListTab::GameListTab() {
    this->inflateFromXMLRes("xml/tabs/game_list_tab.xml");

    recycler->estimatedRowHeight = 100;
    recycler->registerCell("Cell", []() { return GameCell::create(); });

    if (s_cacheLoaded) {
        gameData = new GameData(s_cachedGames);
        recycler->setDataSource(gameData, false);
        loading_box->setVisibility(brls::Visibility::GONE);
        return;
    }

    gameData = new GameData();
    recycler->setDataSource(gameData, false);
    loading_label->setText("Loading games...");
    loading_spinner->animate(true);

    isLoading = true;
    cancelled = std::make_shared<std::atomic<bool>>(false);
    brls::Application::blockInputs();

    auto c = cancelled;
    loadThread = std::thread([this, c]() {
        auto games = utils::getInstalledGames();
        brls::sync([this, games = std::move(games), c]() mutable {
            if (c->load()) return;
            brls::Application::unblockInputs();
            isLoading = false;
            s_cachedGames = games;
            s_cacheLoaded = true;
            delete gameData;
            gameData = new GameData(std::move(games));
            recycler->setDataSource(gameData, false);
            loading_spinner->animate(false);
            loading_box->setVisibility(brls::Visibility::GONE);
        });
    });
    loadThread.detach();

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

brls::View* GameListTab::create() {
    return new GameListTab();
}