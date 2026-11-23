#include "views/mod_preview.hpp"
#include "views/hint_bar.hpp"
#include "views/oqb_ui.hpp"
#include "views/download_view.hpp"
#include "views/spinner_image_view.hpp"
#include "utils/utils.hpp"
#include "utils/progress_event.hpp"
#include "utils/config.hpp"
#include "utils/descargas.hpp"
#include "api/net.hpp"
#include <fstream>

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
    date->setText(utils::timestamp_to_date(static_cast<time_t>(file.getDate())));
    date->setTextColor(oqb::pal::tenue);
    size->setText(utils::file_size_to_string(file.getSize()));
    size->setTextColor(oqb::pal::tenue);
    // OQB: colores del tema (el XML trae los de Amatista).
    this->setBackgroundColor(oqb::pal::lateral);
    this->setBorderColor(oqb::pal::bordeSuave);
    extBox->setBackgroundColor(oqb::pal::chip);
    // OQB: la extension del archivo en un cuadro, como la maqueta.
    {
        std::string ext;
        const std::string n = file.getName();
        const auto dot = n.rfind('.');
        if (dot != std::string::npos && n.size() - dot <= 6) ext = n.substr(dot);
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        extBox->addView(oqb::texto(ext.empty() ? "—" : ext, 12, oqb::pal::acento, oqb::F_MONO));
    }
    download->setFocusable(true);
    this->fileID = file.getFileID();
    this->refrescar();
}

void FileBox::refrescar() {
    const bool hecho = descargas::hecho(this->fileID);
    download->setText(hecho ? "menu/games/redownload"_i18n : "menu/mods/download"_i18n);
    // Ya bajado: boton secundario (color de panel del tema con borde), para distinguirlo.
    download->setStyle(hecho ? &brls::BUTTONSTYLE_DEFAULT : &brls::BUTTONSTYLE_PRIMARY);
    if (hecho && !marca) {
        marca = oqb::chip("menu/games/downloaded"_i18n, oqb::alfa(oqb::pal::verde, 0.14f), oqb::pal::verde, false, 12);
        marca->setMarginLeft(10);
        date->getParent()->addView(marca);
    }
}

void FileBox::marcarVersion() {
    auto* c = oqb::chip("menu/games/your_version"_i18n, oqb::alfa(oqb::pal::verde, 0.14f), oqb::pal::verde, false, 12);
    c->setMarginLeft(10);
    date->getParent()->addView(c);
}

/* OQB: la miniatura del mod para SimpleModManager, que la muestra en la
   tarjeta del mod: /config/SimpleModManager/miniaturas/<juego>/<mod>.jpg, con
   los mismos nombres de carpeta que usa la descarga. Sale de la cache de la
   lista; si no esta, se baja (es chica). */
void ModPreview::guardarMiniatura() {
    const std::string url = this->mod.getThumbUrl();
    if (url.empty()) return;
    static const std::regex badChars("[:/\\<>|*]");
    std::string juego = utils::modFolderFor(const_cast<Game&>(this->mod.getGame()).getTitle());
    juego = juego.substr(juego.rfind('/') + 1);
    const std::string dir = "sdmc:/config/SimpleModManager/miniaturas/" + juego;
    const std::string destino = dir + "/" + std::regex_replace(this->mod.getName(), badChars, "-") + ".jpg";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const std::string cache = "sdmc:/config/SimpleModDownloader/miniaturas/" + std::to_string(std::hash<std::string>{}(url)) + ".jpg";
    if (std::filesystem::exists(cache, ec)) {
        std::filesystem::copy_file(cache, destino, std::filesystem::copy_options::overwrite_existing, ec);
        return;
    }
    std::vector<unsigned char> buffer;
    net::downloadImage(url, buffer);
    if (buffer.empty()) return;
    std::ofstream f(destino, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
}

// Al volver de una descarga, las marcas de los archivos se ponen al dia.
void ModPreview::willAppear(bool resetState) {
    brls::Box::willAppear(resetState);
    for (auto* v : files_box->getChildren())
        if (auto* fb = dynamic_cast<FileBox*>(v)) fb->refrescar();
}

ModPreview::ModPreview(Mod& mod, std::vector<unsigned char>& bannerBuffer): mod(mod) {
    this->mod.loadMod();

    this->inflateFromXMLRes("xml/tabs/mod_preview.xml");

    this->setFocusable(false);
    scrolling->setFocusable(true);
    
    title->setText(this->mod.getName());

    // OQB: arriba "FICHA" y el juego en una pildora, y la hora a la derecha.
    {
        auto* pill = new brls::Box(brls::Axis::ROW);
        oqb::comoPildora(pill, 56);
        pill->setPadding(0, 22, 0, 22);
        pill->setShrink(1.0f);
        pill->addView(oqb::titulo("menu/games/sheet"_i18n));
        auto* game = oqb::texto(const_cast<Game&>(this->mod.getGame()).getTitle(), 17, oqb::pal::tenue);
        game->setMarginTop(5);  // Upheaval dibuja mas abajo: asi quedan centrados juntos
        game->setMarginLeft(12);
        game->setShrink(1.0f);
        game->setMaxWidth(600);
        pill->addView(game);
        // La version instalada, para no tener que salir de la app a mirarla.
        this->versionInstalada = utils::installedVersion(const_cast<Game&>(this->mod.getGame()).getTid());
        if (!this->versionInstalada.empty()) {
            auto* v = oqb::chip("v" + this->versionInstalada, oqb::alfa(oqb::pal::acento, 0.14f), oqb::pal::acento);
            v->setMarginLeft(10);
            v->setMarginTop(3);
            pill->addView(v);
        }
        head->addView(pill);
        auto* spacer = new brls::Box();
        spacer->setGrow(1.0f);
        head->addView(spacer);
        head->addView(new oqb::Estado());
        panel->setCornerRadius(16);
        panel->setBackgroundColor(oqb::pal::panel);
        panel->setBorderColor(oqb::pal::borde);
        panel->setBorderThickness(1);
        filesPanel->setCornerRadius(16);
        filesPanel->setBackgroundColor(oqb::pal::panel);
        filesPanel->setBorderColor(oqb::pal::borde);
        filesPanel->setBorderThickness(1);
        filesBar->setColor(oqb::pal::acento);
    }
    /* OQB: detalle como el mockup. Arriba nombre, autor y si ya esta
       instalado; a la derecha me gusta, ultima actualizacion, descripcion,
       archivos (cada uno con su boton) y la carpeta donde se instala. */
    author->setText(fmt::format(fmt::runtime("menu/games/by"_i18n), this->mod.getAuthor()));
    author->setTextColor(oqb::pal::tenue);
    {
        static const std::regex badChars("[:/\\<>|*]");
        std::string folder = utils::modFolderFor(const_cast<Game&>(this->mod.getGame()).getTitle());
        std::error_code ec;
        const bool installed = std::filesystem::is_directory(folder + "/" + std::regex_replace(this->mod.getName(), badChars, "-"), ec);
        installed_label->setText(installed ? "menu/games/installed"_i18n : "");
        installed_label->setTextColor(oqb::pal::verde);
        if (!installed) installed_label->setVisibility(brls::Visibility::GONE);
        if (folder.rfind("sdmc:", 0) == 0) folder = folder.substr(5);
        install_path->setText(fmt::format("{} {}/{}", "menu/games/install_to"_i18n, folder, std::regex_replace(this->mod.getName(), badChars, "-")));
        install_path->setTextColor(oqb::pal::tenue);
    }
    fillHintBar(hint_bar, {{"A", "menu/games/h_choose"_i18n}, {"B", "menu/games/h_back"_i18n}});
    files_title->setText("menu/mods/files"_i18n);
    files_title->setTextColor(oqb::pal::texto);
    description->setTextColor(oqb::pal::cuerpo);
    {
        // OQB: los datos como etiquetas chicas (me gusta, ultima actualizacion).
        auto tileFor = [](const std::string& name, const std::string& value) {
            auto* c = oqb::chip(name + " " + value, oqb::pal::chip, oqb::pal::cuerpo, true, 13);
            c->setMarginRight(8);
            return c;
        };
        if (this->mod.getLikes() >= 0) stats_box->addView(tileFor("menu/games/likes_title"_i18n, std::to_string(this->mod.getLikes())));
        int newest = 0;
        for (auto& f : this->mod.getFiles()) newest = std::max(newest, f.getDate());
        if (newest > 0) stats_box->addView(tileFor("menu/games/updated_title"_i18n, utils::timestamp_to_date(static_cast<time_t>(newest))));
    }
    description->setText(this->mod.getDescription());
    this->mostrarVersion();

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
        if (utils::nameMatchesVersion(file.getName(), this->versionInstalada)) fileBox->marcarVersion();
        fileBox->getDownloadButton()->registerClickAction(brls::ActionListener([file = std::move(file), this](brls::View* view) mutable {
            brls::Logger::debug("File clicked : {}", file.getName());
            file.loadFile();

            auto startDownload = [this, file]() mutable {
                this->guardarMiniatura();
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
    /* OQB: navegacion fija entre la tarjeta de informacion y los archivos:
       desde el primer boton, arriba vuelve a la informacion (antes podia
       caer en las miniaturas segun donde estuviera el foco). */
    auto& hijos = files_box->getChildren();
    if (!hijos.empty())
        if (auto* primero = dynamic_cast<FileBox*>(hijos.front()))
            primero->getDownloadButton()->setCustomNavigationRoute(brls::FocusDirection::UP, scrolling.getView());
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

/* OQB: pista de version. GameBanana no dice para que version del juego es un
   mod, asi que se compara la version instalada con las que menciona el mod
   (descripcion y nombres de archivo). Si no menciona ninguna, no se muestra
   nada. Es una pista: muchos mods de romfs sirven en cualquier version. */
void ModPreview::mostrarVersion() {
    std::string texto = this->mod.getName() + "\n" + this->mod.getDescription();
    for (auto& f : this->mod.getFiles()) texto += "\n" + f.getName();
    const auto menciona = utils::mentionedVersions(texto);
    const std::string tengo = this->versionInstalada;
    bool archivo = false;  // algun archivo nombra la version instalada (chainchomp401.rar)
    for (auto& f : this->mod.getFiles()) archivo = archivo || utils::nameMatchesVersion(f.getName(), tengo);
    if (menciona.empty() && !archivo) {
        version_box->setVisibility(brls::Visibility::GONE);
        return;
    }
    std::string lista;
    for (size_t i = 0; i < menciona.size(); ++i) lista += (i ? ", " : "") + menciona[i];
    const bool coincide = archivo || (!tengo.empty() && std::find(menciona.begin(), menciona.end(), tengo) != menciona.end());
    std::string t;
    if (archivo) t = fmt::format(fmt::runtime("menu/games/ver_file"_i18n), tengo);
    else if (coincide) t = fmt::format(fmt::runtime("menu/games/ver_match"_i18n), tengo);
    else if (tengo.empty()) t = fmt::format(fmt::runtime("menu/games/ver_mentions"_i18n), lista);
    else t = fmt::format(fmt::runtime("menu/games/ver_other"_i18n), tengo, lista);
    const NVGcolor c = coincide ? oqb::pal::verde : oqb::pal::oro;
    auto* caja = new brls::Box(brls::Axis::COLUMN);
    caja->setCornerRadius(12);
    caja->setBackgroundColor(oqb::alfa(c, 0.10f));
    caja->setPadding(10, 14, 10, 14);
    caja->setMarginTop(12);
    caja->addView(oqb::texto(t, 15, c, oqb::F_NEGRITA, false));
    if (!coincide) {
        auto* nota = oqb::texto("menu/games/ver_note"_i18n, 13, oqb::pal::cuerpo, oqb::F_TEXTO, false);
        nota->setMarginTop(4);
        caja->addView(nota);
    }
    version_box->addView(caja);
}
