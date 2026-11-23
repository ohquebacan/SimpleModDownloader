#include "views/mods_list.hpp"
#include "views/mod_preview.hpp"
#include "views/hint_bar.hpp"
#include "views/oqb_ui.hpp"
#include "utils/utils.hpp"
#include "api/net.hpp"
#include "utils/config.hpp"


#include <borealis.hpp>
#include <filesystem>
#include <fstream>
#include <regex>

using namespace brls::literals;

ModCell::ModCell()
{
    this->inflateFromXMLRes("xml/cells/cell_no_img.xml");
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

ModCell* ModCell::create()
{
    return new ModCell();
}

CategorieCell::CategorieCell()
{
    this->inflateFromXMLRes("xml/cells/categorie_cell.xml");
}

CategorieCell* CategorieCell::create()
{
    return new CategorieCell();
}

brls::RecyclerCell* ModData::cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath indexPath)
{
    auto cell = (ModCell*)recycler->dequeueReusableCell("Cell");
    brls::Logger::debug("Mod name : {}", modList->getMods()[indexPath.row].getName());
    cell->label->setText(modList->getMods()[indexPath.row].getName());
    cell->subtitle->setText(fmt::format("{} : {}", "menu/mods/author"_i18n, modList->getMods()[indexPath.row].getAuthor()));
    return cell;
}

void ModData::didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath indexPath)
{ 
    brls::Logger::debug("Mod name : {}", modList->getMods()[indexPath.row].getName());
    recycler->present(new ModPreview(modList->getMods()[indexPath.row], bannerBuffer));
    
    //recycler->present(new ModPreview(modList->getMods()[indexPath.row]));
}

ModData::ModData(Game game): game(game)
{
    //modList = new ModList(game);
    modList = std::make_unique<ModList>(game);
    // OQB: la portada ya no se usa en la pagina del mod; no se descarga.
    brls::Logger::debug("{} mods found", modList->getMods().size());


}

int ModData::numberOfSections(brls::RecyclerFrame* recycler)
{
    return 1;
}

int ModData::numberOfRows(brls::RecyclerFrame* recycler, int section)
{
    return modList->getMods().size();
}

std::string ModData::titleForHeader(brls::RecyclerFrame* recycler, int section)
{
    return "";
}

ModListTab::ModListTab(Game& game) : gameCopy(game) {
    this->inflateFromXMLRes("xml/tabs/mods.xml");

    modData = std::make_unique<ModData>(game);
    getAppletFrameItem()->title = fmt::format("{} {}", "menu/mods/mods_for"_i18n, game.getTitle());

    /* OQB: L y R cambian el orden (las pestanas de arriba, como en la maqueta);
       ZL y ZR cambian de pagina. */
    this->registerAction("<", brls::ControllerButton::BUTTON_LB, [this](brls::View* view){
        const int sort = this->modData->getModList()->getSort();
        if (sort > 0) this->setSort(sort - 1);
        return true;
    }, true);
    this->registerAction(">", brls::ControllerButton::BUTTON_RB, [this](brls::View* view){
        const int sort = this->modData->getModList()->getSort();
        if (sort < 3) this->setSort(sort + 1);
        return true;
    }, true);
    this->registerAction("<<", brls::ControllerButton::BUTTON_LT, [this](brls::View* view){
        if (this->page <= 1) return true;
        this->modData->getModList()->previousPage();
        this->page--;
        this->buildGrid(true);
        return true;
    }, true);
    this->registerAction(">>", brls::ControllerButton::BUTTON_RT, [this](brls::View* view){
        this->modData->getModList()->nextPage();
        // Pagina vacia (no hay mas): se vuelve a la anterior en vez de quedarse en blanco.
        if (this->modData->getModList()->getMods().empty() && this->page >= 1) {
            this->modData->getModList()->previousPage();
            brls::Application::notify("menu/games/no_more_pages"_i18n);
            return true;
        }
        this->page++;
        this->buildGrid(true);
        return true;
    }, true);

    this->registerAction("menu/mods/search"_i18n, brls::ControllerButton::BUTTON_Y, [this](brls::View* view) {
        this->doSearch();
        return true;
    }, true);

    this->registerAction("menu/mods/categories"_i18n, brls::ControllerButton::BUTTON_X, [this](brls::View* view) {
        int selected = this->modData->getModList()->getCategory().getIndex();
        std::vector<std::string> categoriesNames = {};
        for (auto i : gameCopy.getCategories()) categoriesNames.push_back(i.getName());
        if (categoriesNames.empty()) return true;
        auto selector = new brls::Dropdown("menu/mods/categories"_i18n, categoriesNames, [this](int _selected) {
            this->modData->getModList()->setCategory(gameCopy.getCategories()[_selected]);
            this->page = 1;
            this->buildGrid(true);
            return true;
        }, selected);
        brls::Application::pushActivity(new brls::Activity(selector));
        return true;
    }, true);

    this->registerAction("menu/games/h_install"_i18n, brls::ControllerButton::BUTTON_START, [](brls::View*) {
        openModManagerDialog();
        return true;
    }, true);
    this->buildHead();
    this->buildGrid(false);
}

void ModListTab::doSearch() {
    std::string search;
    brls::Application::getImeManager()->openForText([&](std::string text){
        search = text;
    });
    if (search.empty()) return;
    if (search.size() < 3) {
        auto dialog = new brls::Dialog("menu/notify/string_to_short"_i18n);
        dialog->addButton("hints/ok"_i18n, []() {});
        dialog->open();
        return;
    }
    this->modData->getModList()->search(search);
    this->lastSearch = search;
    this->page = 1;
    this->buildGrid(true);
}

ModListTab::~ModListTab() {
    this->stopThumbs();
}

void ModListTab::stopThumbs() {
    if (alive) *alive = false;
    if (thumbs.joinable()) thumbs.join();
}

/* OQB: arriba, el orden en una pildora (se elige subiendo con la cruceta), el
   juego con su categoria, pagina y busqueda, y la hora. Los chips se crean una
   vez; despues solo cambian sus colores (borrar el chip con foco deja a
   borealis sin foco). */
void ModListTab::buildHead() {
    auto* sortPill = new brls::Box(brls::Axis::ROW);
    oqb::comoPildora(sortPill, 56);
    sortPill->setPadding(0, 10, 0, 10);
    sortPill->addView(oqb::texto(oqb::glifo("L"), 24, oqb::pal::tenue));
    const char* keys[] = {"menu/games/s_popular", "menu/games/s_liked", "menu/games/s_downloaded", "menu/games/s_new"};
    for (int i = 0; i < 4; ++i) {
        auto* chip = new brls::Box(brls::Axis::ROW);
        chip->setHeight(40);
        chip->setCornerRadius(20);
        chip->setHighlightCornerRadius(20);
        chip->setPadding(0, 14, 0, 14);
        chip->setAlignItems(brls::AlignItems::CENTER);
        chip->setFocusable(false);  // L y R los cambian; asi el foco empieza en la primera tarjeta
        auto* l = oqb::texto(brls::getStr(keys[i]), 16, oqb::pal::texto, oqb::F_TEXTO);
        chip->addView(l);
        chip->setHideHighlightBackground(true);
        chip->registerClickAction([this, i](brls::View*) {
            this->setSort(i);
            return true;
        });
        chip->addGestureRecognizer(new brls::TapGestureRecognizer(chip));
        sortPill->addView(chip);
        sortChips.push_back(chip);
        sortLabels.push_back(l);
    }
    auto* r = oqb::texto(oqb::glifo("R"), 24, oqb::pal::tenue);
    r->setMarginLeft(4);
    sortPill->addView(r);
    head->addView(sortPill);

    auto* gamePill = new brls::Box(brls::Axis::ROW);
    oqb::comoPildora(gamePill, 56);
    gamePill->setPadding(0, 18, 0, 20);
    gamePill->setMarginLeft(14);
    gamePill->setShrink(1.0f);
    auto* name = oqb::texto(gameCopy.getTitle(), 19, oqb::pal::texto, oqb::F_NEGRITA);
    name->setShrink(1.0f);
    name->setMaxWidth(300);
    gamePill->addView(name);
    gameInfo = new brls::Box(brls::Axis::ROW);
    gameInfo->setAlignItems(brls::AlignItems::CENTER);
    gamePill->addView(gameInfo);
    head->addView(gamePill);

    auto* spacer = new brls::Box();
    spacer->setGrow(1.0f);
    head->addView(spacer);
    head->addView(new oqb::Estado());
}

void ModListTab::setSort(int sort) {
    this->modData->getModList()->setSort(sort);
    this->page = 1;
    this->buildGrid(true);
}

void ModListTab::updateHeader() {
    const int sort = this->modData->getModList()->getSort();
    for (int i = 0; i < (int)sortChips.size(); ++i) {
        sortChips[i]->setBackgroundColor(i == sort ? oqb::pal::acento : nvgRGBA(0, 0, 0, 0));
        sortLabels[i]->setTextColor(i == sort ? oqb::pal::tinta : oqb::pal::texto);
    }

    gameInfo->clearViews();
    auto addChip = [this](brls::Box* c) { c->setMarginLeft(10); gameInfo->addView(c); };
    const std::string category = this->modData->getModList()->getCategory().getName();
    if (!category.empty()) addChip(oqb::chip(category, oqb::alfa(oqb::pal::acento, 0.14f), oqb::pal::acento, false, 13));
    addChip(oqb::chip(fmt::format(fmt::runtime("menu/games/page_chip"_i18n), this->page), oqb::pal::chip, oqb::pal::tenue));
    if (!this->lastSearch.empty()) addChip(oqb::chip("«" + this->lastSearch + "»", oqb::pal::chip, oqb::pal::cuerpo, false, 13));

    const size_t n = this->modData->getModList()->getMods().size();
    this->counter = fillHintBar(hintBar, {{"A", "menu/games/h_view"_i18n},
                          {"Y", "menu/games/h_search"_i18n},
                          {"X", "menu/games/h_categories"_i18n},
                          {"ZL ZR", fmt::format(fmt::runtime("menu/games/h_page_n"_i18n), this->page)},
                          {"+", "menu/games/h_install"_i18n},
                          {"B", "menu/games/h_back"_i18n}}, n ? fmt::format("1 / {}", n) : "");
}

void ModListTab::buildGrid(bool focus) {
    this->stopThumbs();
    this->updateHeader();
    alive = std::make_shared<std::atomic<bool>>(true);

    auto mods = this->modData->getModList()->getMods();
    grid->clearViews();

    if (mods.empty()) {
        /* Algo enfocable donde dejar el foco: la tarjeta que lo tenia se acaba de
           borrar, y con el foco en una vista borrada B dejaba de responder. */
        auto* vacio = new brls::Box(brls::Axis::ROW);
        vacio->setFocusable(true);
        vacio->setHideHighlight(true);
        vacio->addView(oqb::texto("menu/games/no_mods"_i18n, 18, oqb::pal::tenue));
        grid->addView(vacio);
        brls::Application::giveFocus(vacio);
        return;
    }

    static const std::regex badChars("[:/\\\\<>|*]");
    const std::string gameFolder = utils::modFolderFor(gameCopy.getTitle());

    // OQB: dos columnas, como la maqueta: imagen a la izquierda y datos al lado.
    constexpr int PER_ROW = 2;
    constexpr float W = 601.0f, H = 166.0f, IMG_W = 236.0f, IMG_H = 140.0f;
    brls::Box* row = nullptr;
    brls::View* first = nullptr;
    std::vector<std::pair<brls::Image*, std::string>> pending;

    for (size_t i = 0; i < mods.size(); ++i) {
        if (i % PER_ROW == 0) {
            row = new brls::Box(brls::Axis::ROW);
            row->setMarginBottom(14);
            grid->addView(row);
        }
        Mod mod = mods[i];
        auto* card = new brls::Box(brls::Axis::ROW);
        card->setWidth(W);
        card->setHeight(H);
        if (i % PER_ROW != PER_ROW - 1) card->setMarginRight(14);
        card->setPadding(12, 12, 12, 12);
        card->setFocusable(true);
        card->setHighlightCornerRadius(14);
        card->setCornerRadius(14);
        card->setBackgroundColor(oqb::pal::panel);
        card->setBorderColor(oqb::pal::bordeSuave);
        card->setBorderThickness(1);

        auto* image = new brls::Image();
        image->setWidth(IMG_W);
        image->setHeight(IMG_H);
        image->setCornerRadius(10);
        image->setScalingType(brls::ImageScalingType::FILL);
        image->setBackgroundColor(oqb::colorDe(mod.getName()));
        card->addView(image);
        if (!mod.getThumbUrl().empty()) pending.emplace_back(image, mod.getThumbUrl());

        auto* info = new brls::Box(brls::Axis::COLUMN);
        info->setMarginLeft(16);
        info->setWidth(W - 24 - IMG_W - 16);
        auto* name = oqb::texto(mod.getName(), 19, oqb::pal::texto, oqb::F_NEGRITA);
        name->setWidth(W - 24 - IMG_W - 16);
        info->addView(name);
        auto* by = oqb::texto(fmt::format(fmt::runtime("menu/games/by"_i18n), mod.getAuthor()), 15, oqb::pal::tenue);
        by->setWidth(W - 24 - IMG_W - 16);
        by->setMarginTop(6);
        info->addView(by);

        std::error_code ec;
        const bool installed = std::filesystem::is_directory(gameFolder + "/" + std::regex_replace(mod.getName(), badChars, "-"), ec);
        auto* chips = new brls::Box(brls::Axis::ROW);
        chips->setMarginTop(10);
        if (installed) {
            auto* c = oqb::chip("menu/games/installed"_i18n, nvgRGBA(157, 234, 154, 30), oqb::pal::verde, false, 13);
            c->setMarginRight(6);
            chips->addView(c);
        }
        if (mod.getLikes() >= 0)
            chips->addView(oqb::chip(fmt::format(fmt::runtime("menu/games/likes"_i18n), mod.getLikes()), oqb::pal::chip, oqb::pal::cuerpo));
        info->addView(chips);
        card->addView(info);

        card->getFocusEvent()->subscribe([this, i, n = mods.size()](brls::View*) {
            if (this->counter) this->counter->setText(fmt::format("{} / {}", i + 1, n));
        });
        card->registerClickAction([this, mod](brls::View* view) mutable {
            view->present(new ModPreview(mod, this->modData->getBanner()));
            return true;
        });
        card->addGestureRecognizer(new brls::TapGestureRecognizer(card));
        row->addView(card);
        if (!first) first = card;
    }
    scroll->setContentOffsetY(0, false);
    if (focus && first) brls::Application::giveFocus(first);

    // Miniaturas en segundo plano; se ponen en el hilo de la interfaz si la cuadricula sigue viva.
    auto token = alive;
    thumbs = std::thread([pending, token]() {
        /* OQB: las miniaturas se guardan en la SD; la proxima vez salen al
           instante sin descargarlas. Si la carpeta pasa de 600, se vacia. */
        const std::string dir = "sdmc:/config/SimpleModDownloader/miniaturas/";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        {
            size_t n = 0;
            for (auto it = std::filesystem::directory_iterator(dir, ec); it != std::filesystem::directory_iterator(); it.increment(ec)) ++n;
            if (n > 600) {
                std::filesystem::remove_all(dir, ec);
                std::filesystem::create_directories(dir, ec);
            }
        }
        for (const auto& [image, url] : pending) {
            if (!*token) return;
            const std::string path = dir + std::to_string(std::hash<std::string>{}(url)) + ".jpg";
            std::vector<unsigned char> buffer;
            if (std::filesystem::exists(path, ec)) {
                std::ifstream f(path, std::ios::binary);
                buffer.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
            }
            if (buffer.empty()) {
                net::downloadImage(url, buffer);
                if (!buffer.empty()) {
                    std::ofstream f(path, std::ios::binary | std::ios::trunc);
                    f.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
                }
            }
            if (buffer.empty() || !*token) continue;
            brls::sync([image = image, buffer = std::move(buffer), token]() {
                if (*token) image->setImageFromMem(buffer.data(), buffer.size());
            });
        }
    });
}

/*brls::View* ModListTab::create() {
    return new ModListTab();
}*/