#pragma once

#include <string> 

namespace extract {
    bool extractEntry(const std::string& path, const std::string& outputDir, const std::string& tid);

    /* OQB: comprueba en el archivo YA DESCARGADO si trae contenido instalable,
     * con el mismo criterio que usa extractEntry (romfs/, exefs/,
     * exefs_patches/). Sustituye a la comprobacion que haciamos contra
     * _aArchiveFileTree de GameBanana, que no siempre esta disponible. */
    bool hasSupportedContents(const std::string& archivePath, const std::string& tid);
}