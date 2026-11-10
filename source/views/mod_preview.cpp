#include "views/mod_preview.hpp"
#include "views/download_view.hpp"
#include "views/spinner_image_view.hpp"
#include "utils/utils.hpp"
#include "utils/progress_event.hpp"
#include "utils/config.hpp"

#include <future>
#include <filesystem>
#include <regex>
#include <algorithm>

using namespace brls::literals;

FileBox::FileBox(File& file) {
    this->inflateFromXMLRes("xml/cells/file_cell.xml");
    this->setFocusable(false);

    #ifndef NDEBUG
    cfg::Config config;
    if (config.getWireframe()) {
        this->setWireframeEnabled(true);
        for(auto& view : this->getChildren()) {
            view->setWireframeEnabled(true);
        }
    }
    #endif

    title->setText(file.getName());
    date->setText(fmt::format("{} : {}", "menu/mods/date"_i18n, utils::timestamp_to_date(static_cast<time_t>(file.getDate()))));
    size->setText(fmt::format("{} : {}", "menu/mods/size"_i18n, utils::file_size_to_string(file.getSize())));
    download->setText("menu/mods/download"_i18n);
    download->setStyle(&brls::BUTTONSTYLE_PRIMARY);
    download->setFocusable(true);
}

ModPreview::ModPreview(Mod& mod, std::vector<unsigned char>& bannerBuffer): mod(mod) {
    this->mod.loadMod();

    this->inflateFromXMLRes("xml/tabs/mod_preview.xml");

    this->setFocusable(false);
    scrolling->setFocusable(true);
    
    title->setText(this->mod.getName());
    /* OQB: detalle como el mockup. Arriba nombre, autor y si ya esta
       instalado; a la derecha me gusta, ultima actualizacion, descripcion,
       archivos (cada uno con su boton) y la carpeta donde se instala. */
    author->setText(fmt::format("{} {}", "menu/mods/author"_i18n, this->mod.getAuthor()));
    author->setTextColor(nvgRGB(140, 140, 140));
    {
        static const std::regex badChars("[:/\\<>|*]");
        std::string folder = utils::modFolderFor(const_cast<Game&>(this->mod.getGame()).getTitle());
        std::error_code ec;
        const bool installed = std::filesystem::is_directory(folder + "/" + std::regex_replace(this->mod.getName(), badChars, "-"), ec);
        installed_label->setText(installed ? "menu/games/installed"_i18n : "");
        installed_label->setTextColor(nvgRGB(46, 170, 110));
        if (folder.rfind("sdmc:", 0) == 0) folder = folder.substr(5);
        install_path->setText(fmt::format("{} {}/{}", "menu/games/install_to"_i18n, folder, std::regex_replace(this->mod.getName(), badChars, "-")));
        install_path->setTextColor(nvgRGB(140, 140, 140));
    }
    files_title->setText("menu/mods/files"_i18n);
    files_title->setTextColor(nvgRGB(140, 140, 140));
    {
        auto tileFor = [](const std::string& name, const std::string& value) {
            auto* tile = new brls::Box(brls::Axis::COLUMN);
            tile->setCornerRadius(10);
            tile->setBackgroundColor(nvgRGBA(128, 128, 128, 30));
            tile->setPadding(8, 14, 10, 14);
            tile->setMarginRight(10);
            auto* n = new brls::Label();
            n->setText(name);
            n->setFontSize(13);
            n->setTextColor(nvgRGB(140, 140, 140));
            tile->addView(n);
            auto* v = new brls::Label();
            v->setText(value);
            v->setFontSize(20);
            v->setMarginTop(2);
            tile->addView(v);
            return tile;
        };
        if (this->mod.getLikes() >= 0) stats_box->addView(tileFor("menu/games/likes_title"_i18n, std::to_string(this->mod.getLikes())));
        int newest = 0;
        for (auto& f : this->mod.getFiles()) newest = std::max(newest, f.getDate());
        if (newest > 0) stats_box->addView(tileFor("menu/games/updated_title"_i18n, utils::timestamp_to_date(static_cast<time_t>(newest))));
    }
    description->setText(this->mod.getDescription());

    // OQB: sin imagenes los botones de descarga no aparecian nunca (se ponian al llegar la primera).
    if (this->mod.getImagesUrl().empty()) brls::sync([this] { loadButtons(); });
    secondThread = std::thread(&ModPreview::loadImages, this);

    brls::sync([this] {
        getAppletFrame()->setHeaderVisibility(brls::Visibility::GONE);
    });

    this->registerAction("back", brls::ControllerButton::BUTTON_B, [this](brls::View* view) {
        brls::sync([this]{ getAppletFrame()->setHeaderVisibility(brls::Visibility::GONE); });
        this->dismiss();
        return true;
    }, true);
}

void ModPreview::loadButtons() {
    //Don't give the focus in the constructor of ModPreview() because it will break the focus on the scroller. If you have a better option please tell me
    for(auto file : this->mod.getFiles()) {
        auto fileBox = new FileBox(file);
        fileBox->getDownloadButton()->registerClickAction(brls::ActionListener([file = std::move(file), this](brls::View* view) mutable {
            brls::Logger::debug("File clicked : {}", file.getName());
            file.loadFile();

            auto startDownload = [this, file]() mutable {
                brls::sync([this] {
                    getAppletFrame()->setHeaderVisibility(brls::Visibility::GONE);
                });
                ProgressEvent::instance().setInterupt(true);
                this->present(new DownloadView(file));
                this->stopThreadFlag = true;
            };

            //Smash tid
            if(file.getRomfs() || file.getGame().getTid() == "01006A800016E000") {
                startDownload();
            }
            else if(file.getCheckFailed()) {
                /* OQB: la API de GameBanana no contesto, asi que no pudimos
                 * comprobarlo ANTES de descargar. Descargamos y DownloadView lo
                 * verifica sobre el archivo real con extract::hasSupportedContents.
                 * Antes esto caia en el mensaje de "no compatible" y bloqueaba
                 * toda descarga cuando su API fallaba. */
                brls::Logger::debug("La API no contesto; se verificara el archivo tras descargarlo");
                startDownload();
            }
            else {
                auto dialog = new brls::Dialog("menu/notify/mod_unsupported"_i18n);
                dialog->addButton("hints/ok"_i18n, []() {});
                dialog->open();
            }
            return true;
        }));
        files_box->addView(fileBox);
    }
}

void ModPreview::loadImages() {
    if (shouldStopThread()) {
        brls::Logger::debug("Thread stopped before starting.");
        return;
    }
    size_t imageCount = std::min(int(this->mod.getImagesUrl().size()), 7);

    std::vector<SpinnerImageView*> spinnerViews;

    // add spinners to the boxes
    std::promise<void> spinnersAddedPromise;
    auto spinnersAddedFuture = spinnersAddedPromise.get_future();

    brls::sync([this, &spinnerViews, &spinnersAddedPromise, &imageCount]() {
        brls::Logger::debug("Adding spinners...");
        brls::Logger::debug("Number of image URLs: {}", imageCount);

        // create boxes for the images
        for (auto i = 0; i < imageCount; i += 8) {
            if (shouldStopThread()) {
                brls::Logger::debug("Thread stopped while creating boxes.");
                return;
            }

            brls::Logger::debug("Creating box for images {} to {}", i, imageCount);
            auto box = new brls::Box();
            box->setWidth(bigImageWidth);
            box->setHeight(bigImageWidth/8 * 9/16);
            box->setAxis(brls::Axis::ROW);
            box->setJustifyContent(brls::JustifyContent::FLEX_START);
            box->setAlignItems(brls::AlignItems::FLEX_START);
            //box->setWireframeEnabled(true);
            brls::Logger::debug("Box created with width: {}, height: {}", bigImageWidth, bigImageWidth/8 * 9/16);
            screenshot_box->addView(box);
            brls::Logger::debug("Box added to screenshot_box.");
            smallScreenshotsBoxs.push_back(box);
            brls::Logger::debug("Box added to smallScreenshotsBoxs.");
        }

        for (size_t i = 0; i < imageCount; i++) {
            if (shouldStopThread()) {
                brls::Logger::debug("Thread stopped while adding spinners.");
                return;
            }

            try {
                auto spinnerImageView = new SpinnerImageView(bigImageWidth/8, bigImageWidth/8 * 9/16, bigImageWidth/8/7/2);
                this->smallScreenshotsBoxs[abs(i / 7)]->addView(spinnerImageView);

                spinnerViews.push_back(spinnerImageView);
            } catch (const std::exception& e) {
                brls::Logger::error("Error while creating SpinnerImageView: {}", e.what());
                return;
            }
        }

        /*// set custom navidation route
        for (size_t i = 0; i < spinnerViews.size(); i++) {
            if (shouldStopThread()) {
                brls::Logger::debug("Thread stopped while setting navigation route.");
                return;
            }
            if (i < 7) {
                spinnerViews[i]->setCustomNavigationRoute(brls::FocusDirection::DOWN, spinnerViews[i + 7]);
            } else if (i >= 7 && i < spinnerViews.size() - 7) {
                spinnerViews[i]->setCustomNavigationRoute(brls::FocusDirection::UP, spinnerViews[i - 7]);
                spinnerViews[i]->setCustomNavigationRoute(brls::FocusDirection::DOWN, spinnerViews[i + 7]);
            } else {
                spinnerViews[i]->setCustomNavigationRoute(brls::FocusDirection::UP, spinnerViews[i - 7]);
            }
        }*/

        big_image_box->addView(bigSpinImg);
        brls::Logger::debug("Spinners added.");

        #ifndef NDEBUG
        cfg::Config config;
        if (config.getWireframe()) {
            this->setWireframeEnabled(true);
            for(auto& view : this->getChildren()) {
                view->setWireframeEnabled(true);
            }
            for(auto& view : this->smallScreenshotsBoxs) {
                view->setWireframeEnabled(true);
            }
            for(auto& view : this->scrolling->getChildren()) {
                view->setWireframeEnabled(true);
            }
        }
        #endif
        

        spinnersAddedPromise.set_value();
    });

    // wait for the spinners to be added before downloading images
    spinnersAddedFuture.wait();

    brls::Logger::debug("SpinnerViews size: {}", spinnerViews.size());

    for (size_t i = 0; i < spinnerViews.size(); i++) {
        if (shouldStopThread()) {
            brls::Logger::debug("Thread stopped during image download.");
            return;
        }

        std::vector<unsigned char> buffer;

        brls::Logger::debug("Downloading image {}", i);
        buffer = this->mod.downloadImage(i);
        brls::Logger::debug("Downloaded image {} with size {}", i, buffer.size());

        if (shouldStopThread()) {
            brls::Logger::debug("Thread stopped after downloading image {}.", i);
            return;
        }

        brls::sync([this, spinnerView = spinnerViews[i], buffer, i]() mutable {
            if (shouldStopThread()) {
                brls::Logger::debug("Thread stopped while updating UI for image {}.", i);
                return;
            }

            // if it's the first image, set it to the big image
            if (i == 0) {
                bigSpinImg->setImage(buffer);
                loadButtons();
            }

            brls::Logger::debug("Replacing spinner with image {}", i);
            spinnerView->setImage(buffer);
            spinnerView->registerClickAction(brls::ActionListener([this, buffer](brls::View* view) {
                this->bigSpinImg->setImage(buffer);
                return true;
            }));
        });
    }
}

bool ModPreview::shouldStopThread() {
    std::unique_lock<std::mutex> lock(threadMutex);
    return stopThreadFlag || isExiting;
}

void ModPreview::stopThread() {
    {
        std::unique_lock<std::mutex> lock(threadMutex);
        stopThreadFlag = true;
    }
    threadCondition.notify_one();
}

ModPreview::~ModPreview() {
    stopThread();
    {
        std::unique_lock<std::mutex> lock(threadMutex);
        isExiting = true;
        stopThreadFlag = true;
    }
    threadCondition.notify_one();
    if (secondThread.joinable())
        secondThread.join();
}