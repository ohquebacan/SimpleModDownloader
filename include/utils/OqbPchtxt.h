// SPDX-License-Identifier: GPL-3.0-or-later
// OH! QUÉ BACÁN: convierte parches .pchtxt (formato de texto de IPSwitch) a .ips
// (IPS32), que es lo único que lee Atmosphère en atmosphere/exefs_patches/.
// Escrito desde cero a partir del formato público de IPSwitch.
//
// Lo que entiende:
//   @nsobid-<id>            el ejecutable (build id); da el nombre del .ips
//   @flag offset_shift N    se suma a cada posición (casi siempre 0x100: la
//                           cabecera del NSO; Atmosphère cuenta desde ahí)
//   @little-endian / @big-endian   orden de los bytes de los valores en hex
//   @enabled / @disabled    los parches que siguen se aplican o no
//   @stop                   fin del archivo
//   OFFSET HEX...           ej. 0012AB34 1F2003D5
//   OFFSET "texto"          con \n \t \r \0 \\ \" \x41
//   // o #                  comentarios
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace oqb::pchtxt {

struct Ips {
    std::string build_id;      // en mayúsculas, sin ceros finales sobrantes
    std::vector<uint8_t> data; // archivo .ips (IPS32) completo
    size_t parches{};          // cuántos parches activos lleva
};

// Convierte el texto de un .pchtxt. Devuelve un .ips por cada @nsobid con al
// menos un parche activo. Si algo no se entiende, `error` dice la línea.
bool Convertir(const std::string& texto, std::vector<Ips>& out, std::string& error);

// Nombre del archivo para exefs_patches: "<BUILDID>.ips".
std::string NombreIps(const Ips& ips);

} // namespace oqb::pchtxt
