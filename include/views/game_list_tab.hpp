#pragma once

#include <borealis.hpp>
#include <thread>
#include <memory>
#include <atomic>
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
    ~GameListTab() {
        if (isLoading) brls::Application::unblockInputs();
        if (loadThread.joinable()) loadThread.join();
        checkStop = true;
        if (checkThread.joinable()) checkThread.join();
    }

    static brls::View* create();
    void refrescarTema();
    class PanelAjustes* panel = nullptr;  // ajustes (R), una capa encima
private:
    BRLS_BIND(brls::ScrollingFrame, gridScroll, "grid_scroll");
    BRLS_BIND(brls::Box, head, "head");
    BRLS_BIND(brls::Box, hintBar, "hint_bar");
    brls::Box* headSearch = nullptr;  // pildora con la busqueda (se ve solo si hay una)
    std::shared_ptr<int> tileGen = std::make_shared<int>(0);
    void loadTileImages(std::vector<std::pair<brls::Image*, size_t>> pending, size_t from, int gen);
    void openSearch();
    void abrirPedidoBacan();
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
    int sortMode = 0;  // 0 recientes, 1 A-Z, 2 con mods, 3 solo con pagina en GameBanana
    /* Filtro «Con página»: los juegos que no se conocen se revisan en segundo
       plano, uno cada 2 s, y el resultado queda una semana en la cache. */
    std::thread checkThread;
    std::atomic<bool> checkStop{false};
    std::atomic<int> checkDone{0};
    int checkTotal = 0;
    bool checking = false;
    void startCheck();
    void agregarPortada(const std::string& tid);
    void refrescarGameBanana();
    bool xEsperando = false;  // X apretada: se mira a los 500 ms si sigue (mantener = actualizar)
    void updateHead();
    void applyView(bool focus = false);
    void updateHints();
    brls::View* makeTile(const utils::GameInfo& game, size_t index, size_t total);
    brls::Label* counter = nullptr;  // "3 / 38" en la barra de abajo
};
