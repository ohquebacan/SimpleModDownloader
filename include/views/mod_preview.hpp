#pragma once

#include <borealis.hpp>

#include "api/mod.hpp"
#include "views/spinner_image_view.hpp"

class FileBox : public brls::Box {
    public:
        FileBox(File& file);
        brls::Button* getDownloadButton() { return download.getView(); }
        void refrescar();  // «Descargado» y el texto del boton, segun el registro
        void marcarVersion();  // «Tu versión»: el nombre del archivo nombra la version instalada
    private:
        std::string fileID;
        brls::Box* marca = nullptr;
    public:
    private:
        BRLS_BIND(brls::Label, title, "title");
        BRLS_BIND(brls::Label, date, "date");
        BRLS_BIND(brls::Label, size, "size");
        BRLS_BIND(brls::Button, download, "download");
        BRLS_BIND(brls::Box, extBox, "ext_box");
};

class ModPreview : public brls::Box {
public:
    ModPreview(Mod& mod, std::vector<unsigned char>& bannerBuffer);
    ~ModPreview();
    void willAppear(bool resetState = false) override;
private:
    Mod mod;

    BRLS_BIND(brls::Label, title, "mod_preview_title");
    BRLS_BIND(brls::Label, installed_label, "installed_label");
    BRLS_BIND(brls::Box, stats_box, "stats_box");
    BRLS_BIND(brls::Box, version_box, "version_box");
    void mostrarVersion();
    void guardarMiniatura();
    std::string versionInstalada;  // la del juego en la consola ("4.0.1"), vacia si no se sabe
    BRLS_BIND(brls::Label, files_title, "files_title");
    BRLS_BIND(brls::Label, install_path, "install_path");
    BRLS_BIND(brls::Box, hint_bar, "prev_hint_bar");
    BRLS_BIND(brls::Box, head, "pv_head");
    BRLS_BIND(brls::Box, panel, "pv_panel");
    BRLS_BIND(brls::Box, filesPanel, "pv_files");
    BRLS_BIND(brls::Rectangle, filesBar, "files_bar");
    BRLS_BIND(brls::Label, author, "author");
    BRLS_BIND(brls::Label, description, "description");
    BRLS_BIND(brls::ScrollingFrame, scrolling, "mod_preview_scrolling");

    BRLS_BIND(brls::Box, files_box, "files_box");
    BRLS_BIND(brls::Box, big_image_box, "big_image_box");
    BRLS_BIND(brls::Box, screenshot_box, "screenshots_box");

    std::vector<brls::Box*> smallScreenshotsBoxs;

    void loadImages();
    bool shouldStopThread();
    void processImageOnMainThread(size_t index, std::vector<unsigned char>& buffer);

    void loadButtons();
    void stopThread();
    std::thread secondThread;
    std::mutex threadMutex;
    std::condition_variable threadCondition;
    
    std::mutex bigImageMutex;

    const int bigImageWidth = 640;

    bool stopThreadFlag = false;
    bool isExiting = false;
    bool firstImageDownloaded = false;

    SpinnerImageView* bigSpinImg = new SpinnerImageView(bigImageWidth, bigImageWidth * 9/16);
};