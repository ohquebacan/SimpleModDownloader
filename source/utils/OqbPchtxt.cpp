// SPDX-License-Identifier: GPL-3.0-or-later
// OH! QUÉ BACÁN: .pchtxt -> .ips (IPS32). Ver OqbPchtxt.h.
#include "utils/OqbPchtxt.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <sstream>

namespace oqb::pchtxt {
namespace {

std::string Recortar(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string Minusculas(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

int Nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Quita un comentario "//" que no esté dentro de comillas.
std::string SinComentario(const std::string& linea) {
    bool comillas = false;
    for (size_t i = 0; i + 1 < linea.size(); ++i) {
        if (linea[i] == '\\' && comillas) { ++i; continue; }
        if (linea[i] == '"') comillas = !comillas;
        else if (!comillas && linea[i] == '/' && linea[i + 1] == '/') return linea.substr(0, i);
    }
    return linea;
}

bool Hex(const std::string& s, std::vector<uint8_t>& out) {
    std::string limpio;
    for (char c : s) if (!std::isspace(static_cast<unsigned char>(c))) limpio += c;
    if (limpio.empty() || limpio.size() % 2) return false;
    out.clear();
    for (size_t i = 0; i < limpio.size(); i += 2) {
        const int a = Nibble(limpio[i]), b = Nibble(limpio[i + 1]);
        if (a < 0 || b < 0) return false;
        out.push_back(static_cast<uint8_t>(a << 4 | b));
    }
    return true;
}

bool Texto(const std::string& s, std::vector<uint8_t>& out) {
    // s empieza y termina con comillas.
    if (s.size() < 2 || s.front() != '"' || s.back() != '"') return false;
    out.clear();
    for (size_t i = 1; i + 1 < s.size(); ++i) {
        char c = s[i];
        if (c != '\\') { out.push_back(static_cast<uint8_t>(c)); continue; }
        if (i + 2 >= s.size()) return false;
        c = s[++i];
        switch (c) {
            case 'n': out.push_back('\n'); break;
            case 't': out.push_back('\t'); break;
            case 'r': out.push_back('\r'); break;
            case '0': out.push_back('\0'); break;
            case '\\': out.push_back('\\'); break;
            case '"': out.push_back('"'); break;
            case '\'': out.push_back('\''); break;
            case 'x': {
                if (i + 3 >= s.size()) return false;
                const int a = Nibble(s[i + 1]), b = Nibble(s[i + 2]);
                if (a < 0 || b < 0) return false;
                out.push_back(static_cast<uint8_t>(a << 4 | b));
                i += 2;
                break;
            }
            default: return false;
        }
    }
    return true;
}

bool Numero(const std::string& s, uint64_t& out) {
    if (s.empty()) return false;
    char* fin = nullptr;
    out = std::strtoull(s.c_str(), &fin, 0);
    return fin && *fin == '\0';
}

void Be32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(static_cast<uint8_t>(x >> 24)); v.push_back(static_cast<uint8_t>(x >> 16));
    v.push_back(static_cast<uint8_t>(x >> 8));  v.push_back(static_cast<uint8_t>(x));
}

struct Seccion {
    std::string id;
    std::map<uint32_t, std::vector<uint8_t>> parches;  // posición -> bytes
    size_t n{};
};

// "@nsobid-..." -> id en hex, mayúsculas, como mucho 64 cifras, sin ceros
// finales de más (Atmosphère acepta el id recortado, en cantidad par).
bool NormalizarId(const std::string& crudo, std::string& id) {
    id.clear();
    for (char c : crudo) {
        if (Nibble(c) < 0) return false;
        id += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    if (id.size() > 64) id.resize(64);
    while (id.size() > 2 && id.size() % 2 == 0 && id.substr(id.size() - 2) == "00") id.resize(id.size() - 2);
    if (id.size() % 2) id += '0';
    return !id.empty();
}

std::vector<uint8_t> ArmarIps(const Seccion& s) {
    std::vector<uint8_t> out = {'I', 'P', 'S', '3', '2'};
    for (const auto& [pos, bytes] : s.parches) {
        // Un registro IPS32 lleva como mucho 0xFFFF bytes.
        for (size_t hecho = 0; hecho < bytes.size();) {
            const size_t n = std::min<size_t>(0xFFFF, bytes.size() - hecho);
            Be32(out, pos + static_cast<uint32_t>(hecho));
            out.push_back(static_cast<uint8_t>(n >> 8));
            out.push_back(static_cast<uint8_t>(n));
            out.insert(out.end(), bytes.begin() + static_cast<long>(hecho), bytes.begin() + static_cast<long>(hecho + n));
            hecho += n;
        }
    }
    out.insert(out.end(), {'E', 'E', 'O', 'F'});
    return out;
}

} // namespace

bool Convertir(const std::string& texto, std::vector<Ips>& out, std::string& error) {
    out.clear();
    error.clear();
    std::vector<Seccion> secciones;
    uint64_t desplazamiento = 0;
    bool activo = true, little = false;
    std::istringstream in(texto);
    std::string linea;
    size_t numero = 0;
    while (std::getline(in, linea)) {
        ++numero;
        if (numero == 1 && linea.size() >= 3 && static_cast<unsigned char>(linea[0]) == 0xEF) linea = linea.substr(3);  // BOM
        std::string l = Recortar(SinComentario(linea));
        if (l.empty() || l[0] == '#') continue;
        const std::string low = Minusculas(l);
        if (low[0] == '@') {
            if (low.rfind("@nsobid-", 0) == 0) {
                Seccion s;
                if (!NormalizarId(Recortar(l.substr(8)), s.id)) { error = "línea " + std::to_string(numero) + ": @nsobid no es hexadecimal"; return false; }
                secciones.push_back(std::move(s));
                activo = true;
            } else if (low.rfind("@flag", 0) == 0) {
                std::istringstream f(l.substr(5));
                std::string nombre, valor;
                f >> nombre >> valor;
                if (Minusculas(nombre) == "offset_shift") {
                    if (!Numero(valor, desplazamiento)) { error = "línea " + std::to_string(numero) + ": offset_shift no es un número"; return false; }
                }
            } else if (low == "@enabled") activo = true;
            else if (low == "@disabled") activo = false;
            else if (low == "@little-endian") little = true;
            else if (low == "@big-endian") little = false;
            else if (low == "@stop") break;
            continue;  // otras banderas de IPSwitch no cambian los bytes
        }
        // Un parche: posición y valor.
        const size_t esp = l.find_first_of(" \t");
        if (esp == std::string::npos) { error = "línea " + std::to_string(numero) + ": parche sin valor"; return false; }
        std::vector<uint8_t> pos_bytes;
        const std::string pos_txt = l.substr(0, esp);
        if (!Hex(pos_txt, pos_bytes) || pos_bytes.size() > 4) { error = "línea " + std::to_string(numero) + ": posición no válida"; return false; }
        uint64_t pos = 0;
        for (uint8_t b : pos_bytes) pos = pos << 8 | b;
        const std::string valor = Recortar(l.substr(esp));
        std::vector<uint8_t> bytes;
        if (!valor.empty() && valor[0] == '"') {
            if (!Texto(valor, bytes)) { error = "línea " + std::to_string(numero) + ": texto mal cerrado"; return false; }
        } else {
            if (!Hex(valor, bytes)) { error = "línea " + std::to_string(numero) + ": valor no es hexadecimal"; return false; }
            if (little) std::reverse(bytes.begin(), bytes.end());
        }
        if (!activo || bytes.empty()) continue;
        if (secciones.empty()) { error = "línea " + std::to_string(numero) + ": parche antes de @nsobid"; return false; }
        const uint64_t final_pos = pos + desplazamiento;
        if (final_pos + bytes.size() > 0xFFFFFFFFull) { error = "línea " + std::to_string(numero) + ": posición fuera de rango"; return false; }
        auto& s = secciones.back();
        // Si un parche posterior pisa al anterior en la misma posición, gana el último.
        s.parches[static_cast<uint32_t>(final_pos)] = bytes;
        ++s.n;
    }
    for (const auto& s : secciones) {
        if (!s.n) continue;
        Ips ips;
        ips.build_id = s.id;
        ips.parches = s.n;
        ips.data = ArmarIps(s);
        out.push_back(std::move(ips));
    }
    if (out.empty() && error.empty()) error = "no trae parches activos";
    return !out.empty();
}

std::string NombreIps(const Ips& ips) { return ips.build_id + ".ips"; }

} // namespace oqb::pchtxt
