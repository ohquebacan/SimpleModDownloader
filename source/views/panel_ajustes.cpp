#include "views/panel_ajustes.hpp"
#include "views/oqb_ui.hpp"
#include "views/hint_bar.hpp"
#include "activity/main_activity.hpp"
#include "utils/config.hpp"

#include <filesystem>

using namespace brls::literals;

namespace {
    // Texto con una letra de OQB; devuelve el ancho.
    float escribir(NVGcontext* vg, const char* letra, float size, NVGcolor c, float x, float y, const std::string& t,
                   int align = NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE) {
        nvgFontFaceId(vg, oqb::fuente(letra));
        nvgFontSize(vg, size);
        nvgTextAlign(vg, align);
        nvgFillColor(vg, c);
        nvgBeginPath(vg);
        nvgText(vg, x, y, t.c_str(), nullptr);
        return nvgTextBounds(vg, 0, 0, t.c_str(), nullptr, nullptr);
    }

    void caja(NVGcontext* vg, float x, float y, float w, float h, float r, NVGcolor fondo, const NVGcolor* borde = nullptr, float grosor = 1) {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y, w, h, r);
        nvgFillColor(vg, fondo);
        nvgFill(vg);
        if (borde) {
            nvgBeginPath(vg);
            nvgRoundedRect(vg, x + grosor / 2, y + grosor / 2, w - grosor, h - grosor, std::max(0.f, r - grosor / 2));
            nvgStrokeColor(vg, *borde);
            nvgStrokeWidth(vg, grosor);
            nvgStroke(vg);
        }
    }

    // Boton en pildora con el icono de la consola; devuelve su ancho.
    float boton(NVGcontext* vg, float x, float y, const std::string& tecla, const std::string& texto, bool principal) {
        const float h = 44;
        nvgFontFaceId(vg, oqb::fuente(principal ? oqb::F_NEGRITA : oqb::F_TEXTO));
        nvgFontSize(vg, 18);
        const float w = 16 + 24 + 8 + nvgTextBounds(vg, 0, 0, texto.c_str(), nullptr, nullptr) + 18;
        const NVGcolor borde = principal ? oqb::pal::acento : oqb::pal::borde;
        caja(vg, x, y, w, h, h / 2, principal ? oqb::pal::chip : oqb::pal::panel, &borde);
        const char* g = oqb::glifo(tecla);
        escribir(vg, oqb::F_TEXTO, 24, principal ? oqb::pal::acento : oqb::pal::texto, x + 16 + 12, y + h / 2 + 2, g ? g : tecla,
                 NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        escribir(vg, principal ? oqb::F_NEGRITA : oqb::F_TEXTO, 18, oqb::pal::texto, x + 16 + 24 + 8, y + h / 2 + 1, texto);
        return w;
    }

    void iconoAjustes(NVGcontext* vg, float cx, float cy, NVGcolor c) {
        nvgStrokeColor(vg, c);
        nvgStrokeWidth(vg, 1.8f);
        nvgLineCap(vg, NVG_ROUND);
        const float l = cx - 9, r = cx + 9;
        nvgBeginPath(vg); nvgMoveTo(vg, l, cy - 5); nvgLineTo(vg, cx + 1, cy - 5); nvgMoveTo(vg, cx + 7, cy - 5); nvgLineTo(vg, r, cy - 5); nvgStroke(vg);
        nvgBeginPath(vg); nvgMoveTo(vg, l, cy + 5); nvgLineTo(vg, cx - 5, cy + 5); nvgMoveTo(vg, cx + 1, cy + 5); nvgLineTo(vg, r, cy + 5); nvgStroke(vg);
        nvgBeginPath(vg); nvgCircle(vg, cx + 4, cy - 5, 2.5f); nvgStroke(vg);
        nvgBeginPath(vg); nvgCircle(vg, cx - 2, cy + 5, 2.5f); nvgStroke(vg);
    }
}

PanelAjustes::PanelAjustes(std::function<void()> alCambiarTema) : alCambiarTema(std::move(alCambiarTema)) {
    {
        cfg::Config config;
        idioma = config.getAppLanguage();
        estricta = config.getStrictSearch();
    }
    this->setFocusable(true);
    this->setHideHighlight(true);
    // Una capa encima de la pantalla de juegos: asi lo de atras se sigue dibujando.
    this->setPositionType(brls::PositionType::ABSOLUTE);
    this->setPositionTop(0);
    this->setPositionLeft(0);
    this->setWidth(brls::Application::contentWidth);
    this->setHeight(brls::Application::contentHeight);
    this->setVisibility(brls::Visibility::GONE);
    this->registerAction("", brls::ControllerButton::BUTTON_NAV_UP, [this](brls::View*) {
        sel = std::max(0, sel - 1);
        return true;
    }, true, true);
    this->registerAction("", brls::ControllerButton::BUTTON_NAV_DOWN, [this](brls::View*) {
        sel = std::min(int(filas().size()) - 1, sel + 1);
        return true;
    }, true, true);
    // Izquierda y derecha tambien cambian el tema, sin pulsar A.
    auto mover = [this](int d) {
        if (sel != 0) return;
        const int t = (oqb::temaActual() + d + oqb::kNumTemas) % oqb::kNumTemas;
        oqb::usarPaleta(t);
        oqb::guardarTema(t);
        oqb::aplicarTemaBorealis();
        if (this->alCambiarTema) this->alCambiarTema();
    };
    this->registerAction("", brls::ControllerButton::BUTTON_NAV_LEFT, [mover](brls::View*) { mover(-1); return true; }, true, true);
    this->registerAction("", brls::ControllerButton::BUTTON_NAV_RIGHT, [mover](brls::View*) { mover(1); return true; }, true, true);
    this->registerAction("", brls::ControllerButton::BUTTON_A, [this](brls::View*) { this->elegir(); return true; }, true);
    this->registerAction("", brls::ControllerButton::BUTTON_B, [this](brls::View*) { this->cerrar(); return true; }, true);
    this->registerAction("", brls::ControllerButton::BUTTON_LB, [this](brls::View*) { this->cerrar(); return true; }, true);
    // Los atajos de la pantalla de juegos no actuan mientras el panel esta abierto.
    for (auto b : {brls::ControllerButton::BUTTON_RB, brls::ControllerButton::BUTTON_X, brls::ControllerButton::BUTTON_Y,
                   brls::ControllerButton::BUTTON_START, brls::ControllerButton::BUTTON_BACK})
        this->registerAction("", b, [](brls::View*) { return true; }, true);
}

void PanelAjustes::mostrar() {
    anterior = brls::Application::getCurrentFocus();
    sel = 0;
    this->setVisibility(brls::Visibility::VISIBLE);
    brls::Application::giveFocus(this);
}

void PanelAjustes::cerrar() {
    this->setVisibility(brls::Visibility::GONE);
    if (anterior) brls::Application::giveFocus(anterior);
}

std::vector<PanelAjustes::Fila> PanelAjustes::filas() const {
    const std::string idiomaTxt = (idioma.empty() || idioma == "auto") ? "menu/panel/auto"_i18n : idioma;
    return {
        {"menu/settings_tab/theme"_i18n, oqb::kTemas[oqb::temaActual()].nombre, "menu/panel/theme_help"_i18n},
        {"menu/panel/language"_i18n, idiomaTxt, "menu/panel/language_help"_i18n},
        {"menu/panel/strict"_i18n, estricta ? "menu/panel/on"_i18n : "menu/panel/off"_i18n, "menu/panel/strict_help"_i18n},
        {"menu/games/h_install"_i18n, "", "menu/panel/smm_help"_i18n, true},
        {"menu/panel/more"_i18n, "", "menu/panel/more_help"_i18n, true},
    };
}

void PanelAjustes::elegir() {
    switch (sel) {
        case 0: {
            const int t = (oqb::temaActual() + 1) % oqb::kNumTemas;
            oqb::usarPaleta(t);
            oqb::guardarTema(t);
            oqb::aplicarTemaBorealis();
            if (this->alCambiarTema) this->alCambiarTema();
            break;
        }
        case 1: {
            std::vector<std::string> idiomas = {"auto"};
            std::error_code ec;
            for (const auto& e : std::filesystem::directory_iterator("romfs:/i18n", ec))
                if (e.is_directory()) idiomas.push_back(e.path().filename().string());
            int actual = 0;
            for (size_t i = 0; i < idiomas.size(); ++i)
                if (idiomas[i] == idioma) actual = int(i);
            auto* d = new brls::Dropdown("menu/panel/language"_i18n, idiomas, [this, idiomas](int elegido) {
                if (elegido < 0) return;
                cfg::Config c;
                c.setAppLanguage(idiomas[elegido]);
                this->idioma = idiomas[elegido];
                brls::Application::notify("menu/panel/language_later"_i18n);
            }, actual);
            brls::Application::pushActivity(new brls::Activity(d));
            break;
        }
        case 2: {
            estricta = !estricta;
            cfg::Config config;
            config.setStringSearch(estricta);
            break;
        }
        case 3:
            openModManagerDialog();
            break;
        case 4:
            this->cerrar();  // al volver de «Acerca de» se queda en la cuadricula, sin el panel a medias
            brls::Application::pushActivity(new AboutActivity());
            break;
    }
}

void PanelAjustes::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    using namespace oqb;
    const auto fs = filas();
    sel = std::clamp(sel, 0, int(fs.size()) - 1);

    // Lo de atras se apaga.
    caja(vg, x, y, width, height, 0, nvgRGBAf(pal::fondo.r, pal::fondo.g, pal::fondo.b, 0.72f));

    const float pw = 640, px = x + width - pw;
    caja(vg, px, y, pw, height, 0, pal::lateral);
    caja(vg, px, y, 1, height, 0, pal::borde);

    // Titulo en una pildora.
    {
        nvgFontFaceId(vg, fuente(F_MARCA));
        nvgFontSize(vg, 40);
        const std::string t = "menu/panel/title"_i18n;
        const float tw = nvgTextBounds(vg, 0, 0, t.c_str(), nullptr, nullptr);
        const float bw = 18 + 24 + 12 + tw + 22;
        caja(vg, px + 32, y + 24, bw, 56, 28, pal::panel, &pal::borde);
        iconoAjustes(vg, px + 32 + 18 + 12, y + 52, pal::texto);
        escribir(vg, F_MARCA, 40, pal::texto, px + 32 + 18 + 24 + 12, y + 54, t);
    }

    // Filas.
    float ry = y + 100;
    const float rx = px + 32, rw = pw - 64, rh = 62;
    for (int i = 0; i < int(fs.size()); ++i) {
        const auto& f = fs[i];
        if (i == sel) caja(vg, rx, ry, rw, rh, 12, pal::elegido, &pal::acento, 3);
        nvgBeginPath(vg);
        nvgCircle(vg, rx + 26, ry + rh / 2, 4);
        nvgFillColor(vg, pal::acento);
        nvgFill(vg);
        const float nw = escribir(vg, F_TEXTO, 21, pal::texto, rx + 50, ry + rh / 2 + 1, f.nombre);
        if (f.flecha) {
            const float ax = rx + rw - 28, ay = ry + rh / 2;
            nvgBeginPath(vg);
            nvgMoveTo(vg, ax - 3, ay - 7);
            nvgLineTo(vg, ax + 4, ay);
            nvgLineTo(vg, ax - 3, ay + 7);
            nvgStrokeColor(vg, pal::verde);
            nvgStrokeWidth(vg, 2.2f);
            nvgLineCap(vg, NVG_ROUND);
            nvgLineJoin(vg, NVG_ROUND);
            nvgStroke(vg);
        } else {
            nvgFontFaceId(vg, fuente(F_TEXTO));
            nvgFontSize(vg, 19);
            std::string v = ajustar(vg, f.valor, rw - 50 - nw - 40);
            // El tema se cambia con izquierda/derecha: flechas a los lados del valor.
            if (i == 0 && i == sel) v = "‹  " + v + "  ›";
            escribir(vg, F_TEXTO, 19, pal::verde, rx + rw - 20, ry + rh / 2 + 1, v, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        }
        ry += rh + 6;
    }

    // A la izquierda, la tarjeta que explica la opcion elegida.
    {
        const auto& f = fs[sel];
        const float cw = 320, cx = px - 24 - cw, cy = y + 96, pad = 24;
        nvgFontFaceId(vg, fuente(F_TEXTO));
        nvgFontSize(vg, 17);
        nvgTextLineHeight(vg, 1.45f);
        float b[4];
        nvgTextBoxBounds(vg, cx + pad, 0, cw - pad * 2, f.ayuda.c_str(), nullptr, b);
        const float ch = 22 + 24 + 10 + (b[3] - b[1]) + 22;
        caja(vg, cx, cy, cw, ch, 16, pal::panel, &pal::borde);
        escribir(vg, F_NEGRITA, 19, pal::texto, cx + pad, cy + 22 + 12, f.nombre);
        nvgFontFaceId(vg, fuente(F_TEXTO));
        nvgFontSize(vg, 17);
        nvgTextLineHeight(vg, 1.45f);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgFillColor(vg, pal::cuerpo);
        nvgBeginPath(vg);
        nvgTextBox(vg, cx + pad, cy + 22 + 24 + 10, cw - pad * 2, f.ayuda.c_str(), nullptr);
    }

    // Abajo: contador y botones.
    {
        const float by = y + height - 20 - 44;
        const std::string cnt = std::to_string(sel + 1) + " / " + std::to_string(fs.size());
        nvgFontFaceId(vg, fuente(F_MONO));
        nvgFontSize(vg, 17);
        const float cwid = nvgTextBounds(vg, 0, 0, cnt.c_str(), nullptr, nullptr) + 36;
        caja(vg, px + 32, by, cwid, 44, 22, pal::fondo, &pal::borde);
        escribir(vg, F_MONO, 17, pal::texto, px + 32 + 18, by + 23, cnt);
        const std::string a = sel == 0 || sel == 2 ? "menu/panel/change"_i18n : "menu/games/h_choose"_i18n;
        const std::string bb = "menu/panel/close"_i18n;
        nvgFontFaceId(vg, fuente(F_TEXTO));
        nvgFontSize(vg, 18);
        const float wb = 16 + 24 + 8 + nvgTextBounds(vg, 0, 0, bb.c_str(), nullptr, nullptr) + 18;
        nvgFontFaceId(vg, fuente(F_NEGRITA));
        const float wa = 16 + 24 + 8 + nvgTextBounds(vg, 0, 0, a.c_str(), nullptr, nullptr) + 18;
        const float right = px + pw - 32;
        boton(vg, right - wb - 10 - wa, by, "A", a, true);
        boton(vg, right - wb, by, "B", bb, false);
    }
}
