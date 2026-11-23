#pragma once

/* OQB: piezas del estilo nuevo (el mismo de SimpleModManager): colores
   Amatista, letras de OQB (Upheaval en titulos, Instrument Sans en el texto,
   JetBrains Mono en las cifras), pildoras arriba y abajo, y portadas con el
   nombre encima. */

#include <borealis.hpp>
#include <switch.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>

namespace oqb {

/* Colores del tema: las mismas gamas que OQB Updater, JKSV BACAN y
   SimpleModManager (fondo, panel, linea, acento); el resto se deriva.
   Cian neón por defecto. Se eligen al abrir la app (usarPaleta en main.cpp). */
namespace pal {
    inline NVGcolor fondo, panel, lateral, elegido, borde, bordeSuave, chip, tinta, acento;
    inline const NVGcolor texto  = nvgRGB(232, 237, 230);
    inline const NVGcolor cuerpo = nvgRGB(201, 207, 198);
    inline const NVGcolor tenue  = nvgRGB(154, 163, 150);
    inline const NVGcolor verde  = nvgRGB(157, 234, 154);
    inline const NVGcolor oro    = nvgRGB(230, 185, 92);
}

struct Tema { const char* nombre; uint32_t fondo, panel, linea, claro; };
inline constexpr Tema kTemas[] = {
    {"Cian neón", 0x0E1518, 0x121C20, 0x304C58, 0x00D2FF},
    {"Grafito", 0x0E0F11, 0x131417, 0x373A41, 0xA9B8C8},
    {"Amatista", 0x0F0D12, 0x141118, 0x3A3243, 0xC0A8EE},
    {"Verde OQB", 0x0B0F0A, 0x121711, 0x2A3327, 0x9DEA9A},
    {"Ámbar", 0x100E08, 0x16130B, 0x3D3522, 0xFFE27A},
    {"Bosque", 0x0C0F0D, 0x101411, 0x323B35, 0x8FC9A0},
    {"Rubí", 0x110C0D, 0x171012, 0x423034, 0xEE9A9A},
    {"Neón", 0x0A0F08, 0x11170E, 0x2C3624, 0xCEF781},
    // Mas vivos, con los acentos de Bacán Shop sobre gris casi negro.
    {"Esmeralda", 0x0E1515, 0x121C1D, 0x304E4D, 0x00E0A5},
    {"Azul Switch", 0x0E1317, 0x12191F, 0x304655, 0x009BE6},
    {"Violeta", 0x131118, 0x1A1720, 0x473E58, 0xBE5FFF},
    {"Naranja", 0x151411, 0x1C1A17, 0x4F473D, 0xFFA51E},
    {"Rojo Switch", 0x151113, 0x1C1619, 0x4F3C42, 0xFF4B4B},
};
inline constexpr int kNumTemas = int(sizeof(kTemas) / sizeof(kTemas[0]));
inline constexpr const char* kTemaPath = "sdmc:/config/SimpleModDownloader/tema.txt";

inline NVGcolor hex(uint32_t c) { return nvgRGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF); }
inline NVGcolor mezcla(NVGcolor a, NVGcolor b, float p) {
    return nvgRGBf(a.r + (b.r - a.r) * p, a.g + (b.g - a.g) * p, a.b + (b.b - a.b) * p);
}

inline int& temaActual() { static int t = 0; return t; }

inline void usarPaleta(int i) {
    if (i < 0 || i >= kNumTemas) i = 0;
    temaActual() = i;
    const Tema& t = kTemas[i];
    pal::fondo = hex(t.fondo);
    pal::lateral = hex(t.panel);
    pal::borde = hex(t.linea);
    pal::acento = hex(t.claro);
    pal::panel = mezcla(pal::lateral, pal::borde, 0.12f);
    pal::bordeSuave = mezcla(pal::lateral, pal::borde, 0.6f);
    pal::chip = mezcla(pal::lateral, pal::borde, 0.55f);
    pal::elegido = mezcla(pal::fondo, pal::acento, 0.10f);
    pal::tinta = pal::lateral;
}

/* El tema se guarda por nombre. Un archivo viejo trae el numero del orden
   anterior (Amatista, Verde OQB, Ambar, Bosque, Grafito, Rubi, Neon). */
inline int leerTema() {
    static const char* viejos[] = {"Amatista", "Verde OQB", "Ámbar", "Bosque", "Grafito", "Rubí", "Neón"};
    char linea[64] = {0};
    if (FILE* f = std::fopen(kTemaPath, "r")) {
        if (!std::fgets(linea, sizeof(linea), f)) linea[0] = 0;
        std::fclose(f);
    }
    std::string nombre(linea);
    while (!nombre.empty() && (nombre.back() == '\n' || nombre.back() == '\r' || nombre.back() == ' ')) nombre.pop_back();
    if (nombre.size() == 1 && nombre[0] >= '0' && nombre[0] <= '6') nombre = viejos[nombre[0] - '0'];
    for (int i = 0; i < kNumTemas; ++i)
        if (nombre == kTemas[i].nombre) return i;
    return 0;
}

inline void guardarTema(int i) {
    if (i < 0 || i >= kNumTemas) return;
    if (FILE* f = std::fopen(kTemaPath, "w")) {
        std::fprintf(f, "%s\n", kTemas[i].nombre);
        std::fclose(f);
    }
}

inline NVGcolor alfa(NVGcolor c, float a) { return nvgRGBAf(c.r, c.g, c.b, c.a * a); }

/* Los colores de borealis (fondo, resaltado, botones, listas) con el tema
   actual. Se llama al abrir la app y al cambiar de tema: borealis los lee en
   cada cuadro, asi que el cambio se ve al instante. */
inline void aplicarTemaBorealis() {
    auto& t = brls::Theme::getDarkTheme();
    t.addColor("brls/background", pal::fondo);
    t.addColor("brls/clear", pal::fondo);  // con esto borealis limpia cada cuadro (el gris de fondo)
    t.addColor("brls/text", pal::texto);
    t.addColor("brls/text_disabled", pal::tenue);
    t.addColor("brls/backdrop", nvgRGBA(0, 0, 0, 178));
    t.addColor("brls/accent", pal::acento);
    t.addColor("brls/click_pulse", alfa(pal::acento, 0.25f));
    t.addColor("brls/highlight/background", pal::elegido);
    t.addColor("brls/highlight/color1", pal::acento);
    t.addColor("brls/highlight/color2", pal::acento);
    t.addColor("brls/sidebar/background", pal::panel);
    t.addColor("brls/sidebar/active_item", pal::acento);
    t.addColor("brls/sidebar/separator", pal::bordeSuave);
    t.addColor("brls/header/border", pal::bordeSuave);
    t.addColor("brls/header/rectangle", pal::acento);
    t.addColor("brls/header/subtitle", pal::tenue);
    t.addColor("brls/applet_frame/separator", pal::bordeSuave);
    t.addColor("brls/list/listItem_value_color", pal::verde);
    t.addColor("brls/spinner/bar_color", nvgRGBA(154, 163, 150, 110));
    t.addColor("brls/slider/line_filled", pal::acento);
    t.addColor("brls/slider/line_empty", pal::chip);
    t.addColor("brls/slider/pointer_color", pal::texto);
    t.addColor("brls/slider/pointer_border_color", pal::borde);
    t.addColor("brls/button/primary_enabled_background", pal::acento);
    t.addColor("brls/button/primary_disabled_background", pal::chip);
    t.addColor("brls/button/primary_enabled_text", pal::tinta);
    t.addColor("brls/button/primary_disabled_text", pal::tenue);
    t.addColor("brls/button/default_enabled_background", pal::chip);
    t.addColor("brls/button/default_disabled_background", pal::panel);
    t.addColor("brls/button/default_enabled_text", pal::texto);
    t.addColor("brls/button/default_disabled_text", pal::tenue);
    t.addColor("brls/button/highlight_enabled_text", pal::texto);
    t.addColor("brls/button/highlight_disabled_text", pal::tenue);
    t.addColor("brls/button/enabled_border_color", pal::borde);
    t.addColor("brls/button/disabled_border_color", pal::bordeSuave);
    t.addColor("brls/dropdown/background", pal::panel);
}

// Nombres de las letras cargadas en main.cpp.
inline constexpr const char* F_TEXTO = "oqb-texto";
inline constexpr const char* F_NEGRITA = "oqb-negrita";
inline constexpr const char* F_MONO = "oqb-mono";
inline constexpr const char* F_MARCA = "oqb-marca";

inline int fuente(const char* nombre) {
    int f = brls::Application::getFont(nombre);
    if (f == brls::FONT_INVALID) f = brls::Application::getFont(brls::FONT_REGULAR);
    return f;
}

// Label con otra letra (borealis no deja elegirla desde fuera).
class Etiqueta : public brls::Label {
  public:
    void usarFuente(const char* nombre) { this->font = fuente(nombre); this->invalidate(); }
};

inline brls::Label* texto(const std::string& t, float size, NVGcolor color, const char* letra = F_TEXTO, bool unaLinea = true) {
    auto* l = new Etiqueta();
    l->usarFuente(letra);
    l->setFontSize(size);
    l->setTextColor(color);
    l->setSingleLine(unaLinea);
    l->setText(t);
    return l;
}

/* Los botones con los iconos de la propia consola: nitidos y bien centrados
   (una letra dentro de un circulo dibujado se veia borrosa y corrida). */
inline const char* glifo(const std::string& t) {
    if (t == "A") return "\uE0E0";
    if (t == "B") return "\uE0E1";
    if (t == "X") return "\uE0E2";
    if (t == "Y") return "\uE0E3";
    if (t == "L") return "\uE0E4";
    if (t == "R") return "\uE0E5";
    if (t == "ZL") return "\uE0E6";
    if (t == "ZR") return "\uE0E7";
    if (t == "+") return "\uE0EF";
    if (t == "−" || t == "-") return "\uE0F0";
    return nullptr;
}

// Titulo en Upheaval (multiplos de 20 px).
inline brls::Label* titulo(const std::string& t) { return texto(t, 40, pal::texto, F_MARCA); }

inline void comoPildora(brls::Box* b, float alto, NVGcolor fondo = pal::panel, NVGcolor borde = pal::borde) {
    b->setHeight(alto);
    b->setCornerRadius(alto / 2);
    b->setBackgroundColor(fondo);
    b->setBorderColor(borde);
    b->setBorderThickness(1);
    b->setAlignItems(brls::AlignItems::CENTER);
}

// Etiqueta chica en una caja redondeada.
inline brls::Box* chip(const std::string& t, NVGcolor fondo, NVGcolor color, bool mono = true, float size = 13) {
    auto* b = new brls::Box(brls::Axis::ROW);
    b->setCornerRadius(9);
    b->setBackgroundColor(fondo);
    b->setPadding(4, 9, 4, 9);
    b->setAlignItems(brls::AlignItems::CENTER);
    b->addView(texto(t, size, color, mono ? F_MONO : F_TEXTO));
    return b;
}

/* Pildora de hora, Wi-Fi y bateria. Se dibuja entera; su ancho se ajusta solo
   la primera vez que sabe cuanto mide. */
class Estado : public brls::View {
  public:
    Estado() {
        this->setHeight(56);
        this->setWidth(230);
        this->setFocusable(false);
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override {
        const time_t ahora = time(nullptr);
        if (ahora != visto) {
            visto = ahora;
            char b[8];
            strftime(b, sizeof(b), "%H:%M", localtime(&ahora));
            hora = b;
            hayBateria = R_SUCCEEDED(psmGetBatteryChargePercentage(&bateria));
            NifmInternetConnectionType tipo{};
            u32 senal = 0;
            NifmInternetConnectionStatus st{};
            conectado = R_SUCCEEDED(nifmGetInternetConnectionStatus(&tipo, &senal, &st)) && st == NifmInternetConnectionStatus_Connected;
        }
        const float a = this->getAlpha();
        const int f = fuente(F_TEXTO);
        nvgFontFaceId(vg, f);
        nvgFontSize(vg, 20);
        const std::string pct = hayBateria ? std::to_string(bateria) + "%" : "";
        const float wHora = nvgTextBounds(vg, 0, 0, hora.c_str(), nullptr, nullptr);
        const float wPct = pct.empty() ? 0 : nvgTextBounds(vg, 0, 0, pct.c_str(), nullptr, nullptr);
        const float w = 22 + wHora + 18 + 22 + (pct.empty() ? 0 : 18 + wPct + 8 + 30) + 22;
        if (std::abs(w - width) > 1) {
            this->setWidth(std::ceil(w));
            return;
        }
        const float cy = y + height / 2;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y, width, height, height / 2);
        nvgFillColor(vg, alfa(pal::panel, a));
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x + 0.5f, y + 0.5f, width - 1, height - 1, height / 2);
        nvgStrokeColor(vg, alfa(pal::borde, a));
        nvgStrokeWidth(vg, 1);
        nvgStroke(vg);

        const NVGcolor tc = alfa(pal::texto, a);
        float cx = x + 22;
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, tc);
        nvgBeginPath(vg);
        nvgText(vg, cx, cy + 1, hora.c_str(), nullptr);
        cx += wHora + 18;
        {
            const NVGcolor wc = conectado ? tc : alfa(pal::tenue, 0.6f * a);
            const float ox = cx + 11, oy = cy + 7;
            nvgStrokeColor(vg, wc);
            nvgStrokeWidth(vg, 2);
            nvgLineCap(vg, NVG_ROUND);
            for (int i = 1; i <= 3; i++) {
                nvgBeginPath(vg);
                nvgArc(vg, ox, oy, 4.f * i, -NVG_PI * 0.78f, -NVG_PI * 0.22f, NVG_CW);
                nvgStroke(vg);
            }
            nvgBeginPath(vg);
            nvgCircle(vg, ox, oy, 1.6f);
            nvgFillColor(vg, wc);
            nvgFill(vg);
            cx += 22;
        }
        if (!pct.empty()) {
            cx += 18;
            nvgFillColor(vg, tc);
            nvgBeginPath(vg);
            nvgText(vg, cx, cy + 1, pct.c_str(), nullptr);
            cx += wPct + 8;
            nvgBeginPath(vg);
            nvgRoundedRect(vg, cx + 0.8f, cy - 7, 25, 14, 3);
            nvgStrokeColor(vg, tc);
            nvgStrokeWidth(vg, 1.6f);
            nvgStroke(vg);
            const float lleno = std::max(2.f, 19.f * std::min<u32>(bateria, 100) / 100.f);
            nvgBeginPath(vg);
            nvgRoundedRect(vg, cx + 3.8f, cy - 4, lleno, 8, 1.5f);
            nvgFillColor(vg, alfa(bateria <= 15 ? pal::oro : pal::verde, a));
            nvgFill(vg);
            nvgBeginPath(vg);
            nvgRoundedRect(vg, cx + 27, cy - 3, 2, 6, 1);
            nvgFillColor(vg, tc);
            nvgFill(vg);
        }
    }

  private:
    time_t visto = 0;
    std::string hora;
    u32 bateria = 0;
    bool hayBateria = false, conectado = false;
};

// Un color de relleno estable por nombre, para portadas sin imagen.
inline NVGcolor colorDe(const std::string& s) {
    static const NVGcolor colores[] = {nvgRGB(122, 43, 43), nvgRGB(69, 52, 92), nvgRGB(44, 74, 94), nvgRGB(92, 42, 34), nvgRGB(47, 82, 70),
                                       nvgRGB(45, 79, 110), nvgRGB(74, 45, 85), nvgRGB(62, 58, 99), nvgRGB(58, 77, 52), nvgRGB(90, 58, 36)};
    unsigned h = 2166136261u;
    for (unsigned char c : s) h = (h ^ c) * 16777619u;
    return colores[h % 10];
}

inline std::string siglas(const std::string& s) {
    std::string out;
    bool inicio = true;
    for (unsigned char c : s) {
        if (std::isalnum(c)) { if (inicio && out.size() < 4) out += char(std::toupper(c)); inicio = false; }
        else if (c == ' ' || c == '-' || c == ':' || c == '_') inicio = true;
    }
    return out.empty() ? "?" : out;
}

inline std::string ajustar(NVGcontext* vg, const std::string& t, float ancho) {
    if (nvgTextBounds(vg, 0, 0, t.c_str(), nullptr, nullptr) <= ancho) return t;
    std::string s = t;
    while (!s.empty()) {
        do s.pop_back(); while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80);
        if (!s.empty()) s.pop_back();
        const std::string p = s + "…";
        if (nvgTextBounds(vg, 0, 0, p.c_str(), nullptr, nullptr) <= ancho) return p;
    }
    return "…";
}

/* Portada de un juego: la imagen cubre la tarjeta y encima van el nombre, sus
   etiquetas y, arriba a la derecha, la marca verde (mods instalados). */
class Portada : public brls::Box {
  public:
    Portada(float w, float h, std::string nombre, std::string etiqueta1, std::string etiqueta2, std::string marca)
        : brls::Box(brls::Axis::COLUMN), nombre(std::move(nombre)), e1(std::move(etiqueta1)), e2(std::move(etiqueta2)), marca(std::move(marca)) {
        this->setWidth(w);
        this->setHeight(h);
        this->setCornerRadius(14);
        this->setHighlightCornerRadius(14);
        this->setFocusable(true);
        this->setBackgroundColor(colorDe(this->nombre));
        this->setBorderColor(pal::bordeSuave);
        this->setBorderThickness(1);
        imagen = new brls::Image();
        imagen->setWidth(w);
        imagen->setHeight(h);
        imagen->setCornerRadius(14);
        imagen->setScalingType(brls::ImageScalingType::FILL);
        this->addView(imagen);
    }

    brls::Image* getImage() { return imagen; }

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override {
        brls::Box::draw(vg, x, y, width, height, style, ctx);
        const float a = this->getAlpha();
        if (imagen->getTexture() <= 0) {
            nvgFontFaceId(vg, fuente(F_MARCA));
            nvgFontSize(vg, 40);
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
            nvgFillColor(vg, nvgRGBAf(1, 1, 1, 0.55f * a));
            nvgBeginPath(vg);
            nvgText(vg, x + 16, y + 14, siglas(nombre).c_str(), nullptr);
        }
        // Velo abajo para que se lea el nombre.
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y, width, height, 14);
        nvgFillPaint(vg, nvgLinearGradient(vg, 0, y + height - 104, 0, y + height - 50,
                                           nvgRGBAf(pal::fondo.r, pal::fondo.g, pal::fondo.b, 0), nvgRGBAf(pal::fondo.r, pal::fondo.g, pal::fondo.b, 0.92f * a)));
        nvgFill(vg);

        nvgFontFaceId(vg, fuente(F_NEGRITA));
        nvgFontSize(vg, 16);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, alfa(pal::texto, a));
        nvgBeginPath(vg);
        nvgText(vg, x + 12, y + height - 44, ajustar(vg, nombre, width - 24).c_str(), nullptr);

        float ex = x + 12;
        auto etiqueta = [&](const std::string& t, NVGcolor c) {
            if (t.empty()) return;
            nvgFontFaceId(vg, fuente(F_MONO));
            nvgFontSize(vg, 12);
            const float tw = nvgTextBounds(vg, 0, 0, t.c_str(), nullptr, nullptr) + 16;
            if (ex + tw > x + width - 8) return;
            nvgBeginPath(vg);
            nvgRoundedRect(vg, ex, y + height - 30, tw, 22, 8);
            nvgFillColor(vg, alfa(pal::chip, a));
            nvgFill(vg);
            nvgFillColor(vg, alfa(c, a));
            nvgBeginPath(vg);
            nvgText(vg, ex + 8, y + height - 18, t.c_str(), nullptr);
            ex += tw + 6;
        };
        etiqueta(e1, pal::texto);
        etiqueta(e2, pal::tenue);

        if (!marca.empty()) {
            nvgFontFaceId(vg, fuente(F_TEXTO));
            nvgFontSize(vg, 14);
            const float tw = nvgTextBounds(vg, 0, 0, marca.c_str(), nullptr, nullptr);
            const float bw = 10 + 8 + 6 + tw + 10, bx = x + width - 10 - bw, by = y + 10;
            nvgBeginPath(vg);
            nvgRoundedRect(vg, bx, by, bw, 26, 13);
            nvgFillColor(vg, nvgRGBAf(pal::fondo.r, pal::fondo.g, pal::fondo.b, 0.78f * a));
            nvgFill(vg);
            nvgBeginPath(vg);
            nvgCircle(vg, bx + 14, by + 13, 4);
            nvgFillColor(vg, alfa(pal::verde, a));
            nvgFill(vg);
            nvgFillColor(vg, alfa(pal::verde, a));
            nvgBeginPath(vg);
            nvgText(vg, bx + 24, by + 14, marca.c_str(), nullptr);
        }
    }

  private:
    brls::Image* imagen = nullptr;
    std::string nombre, e1, e2, marca;
};

}  // namespace oqb
