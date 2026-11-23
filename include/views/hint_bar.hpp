#pragma once

#include <borealis.hpp>
#include <string>
#include <utility>
#include <vector>

#include "views/oqb_ui.hpp"

/* OQB: barra de abajo del estilo nuevo: a la izquierda un contador (si se
   pasa) y a la derecha cada atajo como un boton en pildora, con la tecla en un
   circulo. El primero es el principal (borde y circulo en acento). Los atajos
   sin texto no se muestran. */
inline brls::Label* fillHintBar(brls::Box* bar, const std::vector<std::pair<std::string, std::string>>& hints, const std::string& counter = "")
{
    bar->clearViews();
    brls::Label* counterLabel = nullptr;
    bar->setAxis(brls::Axis::ROW);
    bar->setAlignItems(brls::AlignItems::CENTER);
    // Aire arriba: lo que se desplaza (portadas, tarjetas) no llega a tocar los botones.
    bar->setPadding(16, 32, 14, 32);
    bar->setHeight(74);
    if (!counter.empty()) {
        auto* c = new brls::Box(brls::Axis::ROW);
        oqb::comoPildora(c, 44, oqb::pal::fondo);
        c->setPadding(0, 18, 0, 18);
        counterLabel = oqb::texto(counter, 17, oqb::pal::texto, oqb::F_MONO);
        c->addView(counterLabel);
        bar->addView(c);
    }
    auto* spacer = new brls::Box();
    spacer->setGrow(1.0f);
    bar->addView(spacer);
    bool first = true;
    for (const auto& [key, text] : hints) {
        if (text.empty()) continue;
        const bool main = first;
        first = false;
        auto* item = new brls::Box(brls::Axis::ROW);
        oqb::comoPildora(item, 44, main ? oqb::pal::chip : oqb::pal::panel, main ? oqb::pal::acento : oqb::pal::borde);
        item->setPadding(0, 16, 0, 12);
        item->setMarginLeft(10);
        // Una o dos teclas ("ZL ZR"), cada una con su icono de la consola.
        size_t from = 0;
        while (from <= key.size()) {
            const size_t sp = key.find(' ', from);
            const std::string k = key.substr(from, sp == std::string::npos ? std::string::npos : sp - from);
            from = sp == std::string::npos ? key.size() + 1 : sp + 1;
            if (k.empty()) continue;
            const char* g = oqb::glifo(k);
            auto* kl = oqb::texto(g ? g : k, g ? 24 : 16, main ? oqb::pal::acento : oqb::pal::texto, g ? oqb::F_TEXTO : oqb::F_NEGRITA);
            kl->setMarginRight(2);
            if (g) kl->setMarginTop(2);  // el icono de la consola queda un poco alto respecto del texto
            item->addView(kl);
        }
        auto* t = oqb::texto(text, 18, oqb::pal::texto, main ? oqb::F_NEGRITA : oqb::F_TEXTO);
        t->setMarginLeft(8);
        item->addView(t);
        bar->addView(item);
    }
    return counterLabel;  // para ir cambiando el contador sin rehacer la barra
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
