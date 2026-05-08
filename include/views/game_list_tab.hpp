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

class GameData : public brls::RecyclerDataSource
{
  public:
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
    ~GameListTab() { if (loadThread.joinable()) loadThread.join(); }

    static brls::View* create();
private:
    BRLS_BIND(brls::RecyclerFrame, recycler, "recycler");
    BRLS_BIND(brls::Box, loading_box, "loading_box");
    BRLS_BIND(brls::ProgressSpinner, loading_spinner, "loading_spinner");

    GameData* gameData = nullptr;
    std::thread loadThread;
};