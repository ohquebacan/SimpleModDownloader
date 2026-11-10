#pragma once

#include <borealis.hpp>
#include <thread>
#include "utils/utils.hpp"

class GameCell : public brls::RecyclerCell
{
  public:
    GameCell();

    BRLS_BIND(brls::Rectangle, accent, "brls/sidebar/item_accent");
    BRLS_BIND(brls::Label, label, "title");
    BRLS_BIND(brls::Label, subtitle, "subtitle");
    BRLS_BIND(brls::Image, image, "image");

    static GameCell* create();
};

// Abre la lista de mods de un juego (antes vivia en GameData::didSelectRowAt).
void openGame(brls::View* from, const utils::GameInfo& game);
std::string gameSubtitle(const utils::GameInfo& game);

class GameData : public brls::RecyclerDataSource
{
  public:
    GameData() = default;
    explicit GameData(std::vector<utils::GameInfo> games);
    int numberOfSections(brls::RecyclerFrame* recycler) override;
    int numberOfRows(brls::RecyclerFrame* recycler, int section) override;
    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override;
    void didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath indexPath) override;
    std::string titleForHeader(brls::RecyclerFrame* recycler, int section) override;
  private:
    std::vector<utils::GameInfo> games;
};

class GameListTab : public brls::Box {
public:
    GameListTab();
    ~GameListTab() { if (isLoading) brls::Application::unblockInputs(); if (loadThread.joinable()) loadThread.join(); }

    static brls::View* create();
private:
    BRLS_BIND(brls::ScrollingFrame, gridScroll, "grid_scroll");
    BRLS_BIND(brls::Label, headTitle, "head_title");
    BRLS_BIND(brls::Box, headSearchBox, "head_search_box");
    BRLS_BIND(brls::Label, headSearch, "head_search_label");
    BRLS_BIND(brls::Box, headChips, "head_chips_top");
    BRLS_BIND(brls::Box, grid, "grid");
    BRLS_BIND(brls::Box, loading_box, "loading_box");
    BRLS_BIND(brls::Label, loading_label, "loading_label");
    BRLS_BIND(brls::ProgressSpinner, loading_spinner, "loading_spinner");

    GameData* gameData = nullptr;
    std::thread loadThread;
    bool isLoading = false;

    static std::vector<utils::GameInfo> s_cachedGames;
    static bool s_cacheLoaded;

    // OQB: buscador y orden
    std::string filter;
    std::vector<utils::GameInfo> shown;
    int sortMode = 0;  // 0 recientes, 1 A-Z, 2 con mods
    void applyView(bool focus = false);
    void updateHints();
    brls::View* makeTile(const utils::GameInfo& game);
};
