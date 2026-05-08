#include "views/game_list_tab.hpp"
#include "utils/utils.hpp"
#include "utils/config.hpp"
#include "views/mods_list.hpp"

#include <borealis.hpp>

using namespace brls::literals;

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

GameListTab::GameListTab() {
    this->inflateFromXMLRes("xml/tabs/game_list_tab.xml");

    recycler->estimatedRowHeight = 100;
    recycler->registerCell("Cell", []() { return GameCell::create(); });

    recycler->setVisibility(brls::Visibility::GONE);
    loading_spinner->animate(true);

    loadThread = std::thread([this]() {
        auto* data = new GameData(utils::getInstalledGames());
        brls::sync([this, data]() {
            gameData = data;
            loading_spinner->animate(false);
            loading_box->setVisibility(brls::Visibility::GONE);
            recycler->setVisibility(brls::Visibility::VISIBLE);
            recycler->setDataSource(gameData, false);
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

brls::View* GameListTab::create() {
    return new GameListTab();
}