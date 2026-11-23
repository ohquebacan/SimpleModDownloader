#pragma once

/* OQB: ajustes en un panel a la derecha, sobre la pantalla de juegos apagada,
   como en SimpleModManager. Se dibuja entero; la cruceta recorre las
   opciones, A elige y B o L cierran. El tema se aplica al instante. */

#include <borealis.hpp>
#include <functional>
#include <string>
#include <vector>

class PanelAjustes : public brls::Box {
  public:
    explicit PanelAjustes(std::function<void()> alCambiarTema);

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;
    brls::View* getDefaultFocus() override { return this; }

    void mostrar();  // se abre encima de la pantalla de juegos (que sigue viendose detras)

  private:
    struct Fila {
        std::string nombre, valor, ayuda;
        bool flecha = false;
    };
    std::vector<Fila> filas() const;
    void elegir();
    void cerrar();
    brls::View* anterior = nullptr;  // el foco al abrir, para devolverlo al cerrar

    int sel = 0;
    // Copia de la configuracion: leerla (y guardarla) en cada cuadro tocaria la SD 60 veces por segundo.
    std::string idioma;
    bool estricta = false;
    std::function<void()> alCambiarTema;
};
