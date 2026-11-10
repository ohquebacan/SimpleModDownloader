#pragma once

#include <borealis.hpp>

#include "api/mod.hpp"

class DownloadView : public brls::Box {
public:
    DownloadView(File& file);
    ~DownloadView() {
        if(downloadThread.joinable())
            downloadThread.join();
        if(updateThread.joinable())
            updateThread.join();
    }
private:
    File file;

    BRLS_BIND(brls::Label, download_text, "download_text");
    BRLS_BIND(brls::Label, dl_title, "dl_title");
    BRLS_BIND(brls::Label, dl_step1, "dl_step1");
    BRLS_BIND(brls::Label, dl_step2, "dl_step2");
    BRLS_BIND(brls::Label, dl_note, "dl_note");
    BRLS_BIND(brls::Box, dl_card1, "dl_card1");
    BRLS_BIND(brls::Box, dl_card2, "dl_card2");
    BRLS_BIND(brls::ProgressSpinner, download_spinner, "download_spinner");
    BRLS_BIND(brls::Label, download_percent, "download_percent");
    BRLS_BIND(brls::Slider, download_progressBar, "download_progressBar");

    BRLS_BIND(brls::Label, extract_text, "extract_text");
    BRLS_BIND(brls::ProgressSpinner, extract_spinner, "extract_spinner");
    BRLS_BIND(brls::Label, extract_percent, "extract_percent");
    BRLS_BIND(brls::Slider, extract_progressBar, "extract_progressBar");

    void downloadFile();
    void updateProgress();

    std::thread updateThread;
    std::thread downloadThread;
    std::mutex threadMutex;
    std::condition_variable threadCondition;

    bool downloadFinished = false;
    bool extractFinished = false;
    /* OQB: el archivo descargado no traia contenido instalable. */
    bool notSupported = false;
};