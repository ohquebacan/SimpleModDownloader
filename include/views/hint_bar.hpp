#pragma once

#include <borealis.hpp>
#include <string>
#include <utility>
#include <vector>

/* OQB: barra de atajos de abajo, como el mockup: cada tecla en un recuadro
   redondeado algo mas grande y el texto al lado. Reemplaza la de borealis
   (que mostraba atajos vacios y no se podia ordenar a gusto). */
inline void fillHintBar(brls::Box* bar, const std::vector<std::pair<std::string, std::string>>& hints)
{
    bar->clearViews();
    for (const auto& [key, text] : hints) {
        if (text.empty()) continue;
        auto* item = new brls::Box(brls::Axis::ROW);
        item->setAlignItems(brls::AlignItems::CENTER);
        item->setMarginRight(26);
        auto* keyBox = new brls::Box(brls::Axis::ROW);
        keyBox->setWidth(30);
        keyBox->setHeight(30);
        keyBox->setCornerRadius(8);
        keyBox->setBackgroundColor(nvgRGBA(255, 255, 255, 46));
        keyBox->setJustifyContent(brls::JustifyContent::CENTER);
        keyBox->setAlignItems(brls::AlignItems::CENTER);
        auto* k = new brls::Label();
        k->setText(key);
        k->setFontSize(16);
        k->setSingleLine(true);
        keyBox->addView(k);
        item->addView(keyBox);
        auto* t = new brls::Label();
        t->setText(text);
        t->setFontSize(16);
        t->setSingleLine(true);
        t->setMarginLeft(10);
        item->addView(t);
        bar->addView(item);
    }
}

#include <filesystem>
#include <switch.h>

/* OQB: boton + "Instalar mods": los mods se activan en SimpleModManager. Pregunta
   y, si se acepta, lo abre (al salir de esta app, hbloader carga ese .nro). */
inline void openModManagerDialog()
{
    using namespace brls::literals;
    const char* paths[] = {"sdmc:/switch/SimpleModManager.nro", "sdmc:/switch/SimpleModManager/SimpleModManager.nro"};
    std::string found;
    std::error_code ec;
    for (const char* p : paths)
        if (std::filesystem::exists(p, ec)) { found = p; break; }
    if (found.empty()) {
        auto* d = new brls::Dialog("menu/games/smm_missing"_i18n);
        d->addButton("hints/ok"_i18n, []() {});
        d->open();
        return;
    }
    auto* d = new brls::Dialog("menu/games/smm_text"_i18n);
    d->addButton("menu/games/smm_cancel"_i18n, []() {});
    d->addButton("menu/games/smm_open"_i18n, [found]() {
        const std::string arg = "\"" + found + "\"";
        envSetNextLoad(found.c_str(), arg.c_str());
        brls::Application::quit();
    });
    d->open();
}
