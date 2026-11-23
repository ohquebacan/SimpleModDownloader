#pragma once

/* OQB: registro de los archivos ya descargados (el id de GameBanana de cada
   archivo, uno por linea). Sirve para marcar «Descargado» en la ficha del mod. */

#include <fstream>
#include <string>

namespace descargas {

inline constexpr const char* kRuta = "sdmc:/config/SimpleModDownloader/descargados.txt";

inline bool hecho(const std::string& id) {
    if (id.empty()) return false;
    std::ifstream f(kRuta);
    std::string linea;
    while (std::getline(f, linea))
        if (linea == id) return true;
    return false;
}

inline void marcar(const std::string& id) {
    if (id.empty() || hecho(id)) return;
    std::ofstream f(kRuta, std::ios::app);
    f << id << '\n';
}

}  // namespace descargas
