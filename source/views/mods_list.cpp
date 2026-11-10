#include "views/mods_list.hpp"
#include "views/mod_preview.hpp"
#include "utils/utils.hpp"
#include "api/net.hpp"
#include "utils/config.hpp"


#include <borealis.hpp>
#include <filesystem>
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
    net::downloadImage(game.getBannerUrl(), bannerBuffer);
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

    this->registerAction("<", brls::ControllerButton::BUTTON_LB, [this](brls::View* view){
        this->modData->getModList()->previousPage();
        this->buildGrid(true);
        return true;
    });

    this->registerAction(">", brls::ControllerButton::BUTTON_RB, [this](brls::View* view){
        this->modData->getModList()->nextPage();
        this->buildGrid(true);
        return true;
    });

    this->registerAction("menu/mods/search"_i18n, brls::ControllerButton::BUTTON_Y, [this](brls::View* view) {
        std::string search;
        brls::Application::getImeManager()->openForText([&](std::string text){
            search = text;
        });
        if(search.size() < 3) {
            auto dialog = new brls::Dialog("menu/notify/string_to_short"_i18n);
            dialog->addButton("hints/ok"_i18n, []() {});
            dialog->open();
            return true;
        }
        this->modData->getModList()->search(search);
        this->buildGrid(true);
        return true;
    });

    this->registerAction("menu/mods/categories"_i18n, brls::ControllerButton::BUTTON_X, [this](brls::View* view) {
        int selected = this->modData->getModList()->getCategory().getIndex();
        std::vector<std::string> categoriesNames = {};
        for (auto i : gameCopy.getCategories()) categoriesNames.push_back(i.getName());
        if (categoriesNames.empty()) return true;
        auto selector = new brls::Dropdown("menu/mods/categories"_i18n, categoriesNames, [this](int _selected) {
            this->modData->getModList()->setCategory(gameCopy.getCategories()[_selected]);
            this->buildChips();
            this->buildGrid(true);
            return true;
        }, selected);
        brls::Application::pushActivity(new brls::Activity(selector));
        return true;
    });

    this->buildChips();
    this->buildGrid(false);
}

ModListTab::~ModListTab() {
    this->stopThumbs();
}

void ModListTab::stopThumbs() {
    if (alive) *alive = false;
    if (thumbs.joinable()) thumbs.join();
}

// Las categorias del juego como chips (las primeras 6; X abre la lista completa).
void ModListTab::buildChips() {
    chips->clearViews();
    auto cats = gameCopy.getCategories();
    const int current = this->modData->getModList()->getCategory().getIndex();
    const int shown = std::min<int>(cats.size(), 6);
    for (int i = 0; i < shown; ++i) {
        auto* chip = new brls::Box(brls::Axis::ROW);
        chip->setFocusable(true);
        chip->setHighlightCornerRadius(16);
        chip->setCornerRadius(16);
        chip->setPadding(6, 14, 6, 14);
        chip->setMarginRight(8);
        if (i == current) chip->setBackgroundColor(nvgRGBA(0, 160, 230, 60));
        auto* label = new brls::Label();
        label->setText(cats[i].getName());
        label->setFontSize(15);
        label->setSingleLine(true);
        chip->addView(label);
        chip->registerClickAction([this, i](brls::View*) {
            this->modData->getModList()->setCategory(gameCopy.getCategories()[i]);
            this->buildChips();
            this->buildGrid(true);
            return true;
        });
        chip->addGestureRecognizer(new brls::TapGestureRecognizer(chip));
        chips->addView(chip);
    }
}

void ModListTab::buildGrid(bool focus) {
    this->stopThumbs();
    alive = std::make_shared<std::atomic<bool>>(true);

    auto mods = this->modData->getModList()->getMods();
    grid->clearViews();

    if (mods.empty()) {
        auto* empty = new brls::Label();
        empty->setText("menu/games/no_mods"_i18n);
        empty->setFontSize(18);
        grid->addView(empty);
        return;
    }

    static const std::regex badChars("[:/\\\\<>|*]");
    const std::string gameFolder = utils::modFolderFor(gameCopy.getTitle());

    constexpr int PER_ROW = 3;
    constexpr float W = 250.0f, IMG_H = 140.0f;
    brls::Box* row = nullptr;
    brls::View* first = nullptr;
    std::vector<std::pair<brls::Image*, std::string>> pending;

    for (size_t i = 0; i < mods.size(); ++i) {
        if (i % PER_ROW == 0) {
            row = new brls::Box(brls::Axis::ROW);
            row->setMarginBottom(18);
            grid->addView(row);
        }
        Mod mod = mods[i];
        auto* card = new brls::Box(brls::Axis::COLUMN);
        card->setWidth(W);
        card->setMarginRight(18);
        card->setFocusable(true);
        card->setHighlightCornerRadius(12);
        card->setCornerRadius(12);
        card->setBackgroundColor(nvgRGBA(128, 128, 128, 28));

        auto* image = new brls::Image();
        image->setWidth(W);
        image->setHeight(IMG_H);
        image->setCornerRadius(12);
        image->setScalingType(brls::ImageScalingType::FILL);
        card->addView(image);
        if (!mod.getThumbUrl().empty()) pending.emplace_back(image, mod.getThumbUrl());

        auto* info = new brls::Box(brls::Axis::COLUMN);
        info->setPadding(8, 10, 10, 10);
        auto* name = new brls::Label();
        name->setText(mod.getName());
        name->setFontSize(16);
        name->setSingleLine(true);
        name->setWidth(W - 20);
        info->addView(name);

        std::error_code ec;
        const bool installed = std::filesystem::is_directory(gameFolder + "/" + std::regex_replace(mod.getName(), badChars, "-"), ec);
        std::string sub = mod.getAuthor();
        if (mod.getLikes() >= 0) sub += "  ·  " + fmt::format(fmt::runtime("menu/games/likes"_i18n), mod.getLikes());
        auto* subLabel = new brls::Label();
        subLabel->setText(installed ? "menu/games/installed_one"_i18n + "  ·  " + mod.getAuthor() : sub);
        subLabel->setFontSize(13);
        subLabel->setSingleLine(true);
        subLabel->setWidth(W - 20);
        subLabel->setMarginTop(3);
        subLabel->setTextColor(installed ? nvgRGB(46, 170, 110) : nvgRGB(140, 140, 140));
        info->addView(subLabel);
        card->addView(info);

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
        for (const auto& [image, url] : pending) {
            if (!*token) return;
            std::vector<unsigned char> buffer;
            net::downloadImage(url, buffer);
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