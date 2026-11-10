#include "views/download_view.hpp"
#include "api/net.hpp"
#include "api/extract.hpp"
#include "utils/progress_event.hpp"
#include "utils/utils.hpp"
#include "utils/config.hpp"

#include <regex>
#include <filesystem>

using namespace brls::literals;

DownloadView::DownloadView(File& file): file(file) {
    this->inflateFromXMLRes("xml/tabs/download_view.xml");

    ProgressEvent::instance().reset();

    extract_progressBar->hidePointer();
    extract_progressBar->getPointer()->setFocusable(false);
    extract_progressBar->setProgress(0);
    download_progressBar->hidePointer();
    download_progressBar->getPointer()->setFocusable(false);
    download_progressBar->setProgress(0);
    download_percent->setText(fmt::format("{}%", 0));
    extract_percent->setText(fmt::format("{}%", 0));

    /* OQB: misma linea visual que el resto: titulo, el archivo y dos tarjetas
       (descarga e instalacion) con su progreso. */
    dl_title->setText("menu/games/dl_title"_i18n);
    download_text->setText(file.getName());
    download_text->setTextColor(nvgRGB(140, 140, 140));
    dl_step1->setText("menu/games/dl_step1"_i18n);
    dl_step2->setText("menu/games/dl_step2"_i18n);
    for (auto* card : {dl_card1.getView(), dl_card2.getView()}) {
        card->setCornerRadius(12);
        card->setBackgroundColor(nvgRGBA(128, 128, 128, 30));
    }
    dl_note->setText("menu/games/dl_note"_i18n);
    dl_note->setTextColor(nvgRGB(140, 140, 140));

    std::regex badChars("[:/\\<>|*]");
    std::string destino;
    if (file.getGame().getTid() != "01006A800016E000") {
        std::string folder = utils::modFolderFor(this->file.getGame().getTitle());
        if (folder.rfind("sdmc:", 0) == 0) folder = folder.substr(5);
        destino = fmt::format("{}/{}", folder, std::regex_replace(this->file.getModName(), badChars, "-"));
    }
    else
        destino = fmt::format("/ultimate/mods/{}", file.getModName());
    extract_text->setText(fmt::format("{} {}", "menu/games/install_to"_i18n, destino));
    extract_text->setTextColor(nvgRGB(140, 140, 140));

    this->setActionAvailable(brls::ControllerButton::BUTTON_B, false);

    #ifndef NDEBUG
    cfg::Config config;
    if (config.getWireframe()) {
        this->setWireframeEnabled(true);
        for(auto& view : this->getChildren()) {
            view->setWireframeEnabled(true);
        }
    }
    #endif

    this->setFocusable(true);
    this->setHideHighlightBackground(true);
    this->setHideHighlightBorder(true);

    downloadThread = std::thread(&DownloadView::downloadFile, this);
    updateThread = std::thread(&DownloadView::updateProgress, this);

    brls::sync([this]() {
        getAppletFrame()->setActionAvailable(brls::ControllerButton::BUTTON_B, false);
        getAppletFrame()->setTitle(fmt::format("{}/{}", "menu/title/extracting"_i18n, "menu/title/downloading"_i18n));
    });
    
}

void DownloadView::downloadFile() {
    {
        std::unique_lock<std::mutex> lock(threadMutex);
    }
    net::downloadFile(this->file.getUrl(), this->file.getPath());
    this->downloadFinished = true;

    ProgressEvent::instance().reset();

    /* OQB: comprobamos el archivo real en vez de fiarnos del listado de
     * GameBanana. Si no trae contenido instalable lo decimos y lo borramos, en
     * lugar de "extraer" cero ficheros y dar la instalacion por buena. */
    if (!extract::hasSupportedContents(this->file.getPath(), this->file.getGame().getTid())) {
        this->notSupported = true;
        std::filesystem::remove(this->file.getPath());
        this->extractFinished = true;
        return;
    }

    //Prevent incorrect chars in the path
    std::regex badChars("[:/\\<>|*]");
    std::regex accent("é");
    extract::extractEntry(this->file.getPath(), fmt::format("sdmc:/{}/{}/{}", utils::getModInstallPath(), std::regex_replace(std::regex_replace(this->file.getGame().getTitle(), badChars, "-"), accent, "e"), std::regex_replace(this->file.getModName(), badChars, "-")), this->file.getGame().getTid());
    
    this->extractFinished = true;
}

void DownloadView::updateProgress() {
    {
        std::unique_lock<std::mutex> lock(threadMutex);
    }
    //DOWNLOAD
    {
        while(ProgressEvent::instance().getTotal() == 0) {
            if(downloadFinished)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        while(ProgressEvent::instance().getNow() < ProgressEvent::instance().getTotal() && !downloadFinished) {
            ASYNC_RETAIN
            brls::sync([ASYNC_TOKEN](){
                ASYNC_RELEASE
                this->download_percent->setText(fmt::format("{}%", (int)(ProgressEvent::instance().getNow() / ProgressEvent::instance().getTotal() * 100)));
                this->download_progressBar->setProgress((float)ProgressEvent::instance().getNow() / ProgressEvent::instance().getTotal());
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        ASYNC_RETAIN
        brls::sync([ASYNC_TOKEN](){
            ASYNC_RELEASE
            this->download_percent->setText("100%");
            this->download_progressBar->setProgress(1);
            this->download_spinner->animate(false);
        });
    }
    //EXTRACT
    {
        while(ProgressEvent::instance().getMax() == 0) {
            if(extractFinished)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        while(ProgressEvent::instance().getStep() < ProgressEvent::instance().getMax() && !extractFinished) {
            ASYNC_RETAIN
            brls::sync([ASYNC_TOKEN](){
                ASYNC_RELEASE
                this->extract_percent->setText(fmt::format("{}%", (int)((ProgressEvent::instance().getStep() * 100 / ProgressEvent::instance().getMax()))));
                this->extract_progressBar->setProgress((float)ProgressEvent::instance().getStep() / ProgressEvent::instance().getMax());
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        ASYNC_RETAIN
        brls::sync([ASYNC_TOKEN](){
            ASYNC_RELEASE
            this->extract_percent->setText("100%");
            this->extract_progressBar->setProgress(1);
            this->extract_spinner->animate(false);
        });
    }

    //Add a button to go back after the end of the download
    ASYNC_RETAIN
    brls::sync([ASYNC_TOKEN]() {
        ASYNC_RELEASE
        /* OQB: leemos la bandera antes del dismiss(), que puede destruir la vista. */
        const bool unsupported = this->notSupported;
        getAppletFrame()->setHeaderVisibility(brls::Visibility::GONE);
        getAppletFrame()->setActionAvailable(brls::ControllerButton::BUTTON_B, true);
        this->dismiss();
        if (!unsupported) brls::Application::notify("menu/games/dl_done"_i18n);
        if (unsupported) {
            auto dialog = new brls::Dialog("menu/notify/mod_unsupported"_i18n);
            dialog->addButton("hints/ok"_i18n, []() {});
            dialog->open();
        }
        /*auto button = new brls::Button();
        button->setText("hints/back"_i18n);
        button->setFocusable(true);
        button->registerClickAction(brls::ActionListener([this](brls::View* view) {
            
            return true;
        }));
        this->addView(button);
        brls::Application::giveFocus(button);
        getAppletFrame()->setActionAvailable(brls::ControllerButton::BUTTON_B, true);*/
    });
}

