#pragma once

#include <borealis.hpp>
#include <atomic>
#include <memory>
#include <thread>

#include "api/mod.hpp"
#include "api/game.hpp"

class ModCell : public brls::RecyclerCell
{
  public:
    ModCell();

    BRLS_BIND(brls::Rectangle, accent, "brls/sidebar/item_accent");
    BRLS_BIND(brls::Label, label, "modtitle");
    BRLS_BIND(brls::Label, subtitle, "modsubtitle");
    
    //TODO : Multithreaded image loading
    //BRLS_BIND(brls::Image, image, "image");

    static ModCell* create();
};

class CategorieCell : public brls::RecyclerCell
{
  public:
    CategorieCell();

    BRLS_BIND(brls::Rectangle, accent, "brls/sidebar/item_accent");
    BRLS_BIND(brls::Label, label, "categorie");
    
    //TODO : Multithreaded image loading
    //BRLS_BIND(brls::Image, image, "image");

    static CategorieCell* create();
};

class ModData : public brls::RecyclerDataSource 
{
public:
    ModData(Game game);

    int numberOfSections(brls::RecyclerFrame* recycler) override;
    int numberOfRows(brls::RecyclerFrame* recycler, int section) override;
    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override;
    void didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath indexPath) override;
    std::string titleForHeader(brls::RecyclerFrame* recycler, int section) override;

    std::unique_ptr<ModList>& getModList() { return modList; }
    std::vector<unsigned char>& getBanner() { return bannerBuffer; }
private:
    Game& game;
    std::unique_ptr<ModList> modList;
    std::vector<unsigned char> bannerBuffer;
    //ModList* modList;
};

class ModListTab : public brls::Box {
public:
    ModListTab(Game& game);
    ModListTab();
    ~ModListTab() override;

private:
    // OQB: cuadricula de mods con categorias arriba (antes una lista)
    BRLS_BIND(brls::Box, chips, "mod_chips");
    BRLS_BIND(brls::Label, headTitle, "mhead_title");
    BRLS_BIND(brls::Box, headSearchBox, "mhead_search_box");
    BRLS_BIND(brls::Label, headSearch, "mhead_search_label");
    BRLS_BIND(brls::Box, headChips, "mhead_chips_top");
    std::string lastSearch;
    int page = 1;
    void updateHeader();
    BRLS_BIND(brls::ScrollingFrame, scroll, "mod_scroll");
    BRLS_BIND(brls::Box, grid, "mod_grid");

    std::unique_ptr<ModData> modData;
    Game gameCopy;
    std::shared_ptr<std::atomic<bool>> alive;
    std::thread thumbs;

    void buildChips();
    void buildGrid(bool focus);
    void stopThumbs();
};
