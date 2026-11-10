#include <cctype>
#include "api/extract.hpp"
#include "utils/progress_event.hpp"

#include <archive.h>
#include <archive_entry.h>
#include <filesystem>
#include <fstream>
#include <switch.h>
#include <borealis.hpp>

const std::string smash_tid = "01006A800016E000";

namespace extract {
    int getFileCount(const std::string& archivePath) {
        struct archive* archive;
        struct archive_entry* entry;
        int fileCount = 0;

        archive = archive_read_new();
        archive_read_support_format_all(archive);
        archive_read_support_filter_all(archive);

        if(archive_read_open_filename(archive, archivePath.c_str(), 10240) == ARCHIVE_OK) {
            while(archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
                fileCount++;
            }
            archive_read_close(archive);
        }

        archive_read_free(archive);
        return fileCount;
    }

    s64 getTotalArchiveSize(const std::string& archivePath) {
        struct archive* archive;
        struct archive_entry* entry;
        s64 totalSize = 0;

        archive = archive_read_new();
        archive_read_support_format_all(archive);
        archive_read_support_filter_all(archive);

        if(archive_read_open_filename(archive, archivePath.c_str(), 10240) == ARCHIVE_OK) {
            while(archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
                totalSize += archive_entry_size(entry);
            }
            archive_read_close(archive);
        }

        archive_read_free(archive);
        return totalSize;
    }

    /* OQB: la carpeta del mod (romfs/, exefs/, exefs_patches/) se busca sin
       distinguir mayusculas: muchos mods traen "Romfs" o "RomFS" y antes salian
       como no compatibles. Devuelve la posicion en la ruta y la carpeta tal como
       debe quedar en la SD (en minusculas, que es como la lee Atmosphere). */
    static size_t findModFolder(const std::string& path, std::string& folder) {
        std::string low = path;
        for (auto& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        // exefs_patches antes que exefs; cada uno debe empezar una carpeta.
        for (const char* f : {"exefs_patches/", "romfs/", "exefs/"}) {
            size_t pos = 0;
            while ((pos = low.find(f, pos)) != std::string::npos) {
                if (pos == 0 || low[pos - 1] == '/') {
                    folder = f;
                    return pos;
                }
                pos++;
            }
        }
        return std::string::npos;
    }

    bool hasSupportedContents(const std::string& archivePath, const std::string& tid) {
        /* Los mods de Smash van tal cual a sdmc:/ultimate/mods, no se filtran. */
        if (tid == smash_tid)
            return true;

        struct archive* archive = archive_read_new();
        struct archive_entry* entry;

        archive_read_support_format_all(archive);
        archive_read_support_filter_all(archive);

        if (archive_read_open_filename(archive, archivePath.c_str(), 10240) != ARCHIVE_OK) {
            /* No podemos abrirlo: que lo reporte extractEntry con su propio
             * mensaje en vez de decir aqui que el mod no es compatible. */
            brls::Logger::error("No se pudo abrir {} para comprobar su contenido", archivePath);
            archive_read_free(archive);
            return true;
        }

        bool supported = false;
        while (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
            const std::string name = archive_entry_pathname(entry);
            std::string folder;
            if (findModFolder(name, folder) != std::string::npos) {
                brls::Logger::debug("Contenido instalable encontrado: {}", name);
                supported = true;
                break;
            }
        }

        archive_read_close(archive);
        archive_read_free(archive);

        if (!supported)
            brls::Logger::error("{} no trae romfs/, exefs/ ni exefs_patches/", archivePath);

        return supported;
    }

bool extractEntry(const std::string& archiveFile, const std::string& outputDir, const std::string& tid) {
        chdir("sdmc:/");
        struct archive* archive = archive_read_new();

        brls::Logger::debug("Extracting {} to {}", archiveFile, outputDir);

        archive_read_support_format_all(archive);
        int result = archive_read_open_filename(archive, archiveFile.c_str(), 10240);
        if (result != ARCHIVE_OK) {
            brls::Logger::error("Failed to open archive: {}", archiveFile);
            archive_read_free(archive);
            //std::filesystem::remove(archiveFile);
            ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
            return false;
        }
        struct archive_entry* entry;
        ProgressEvent::instance().setTotalSteps(getFileCount(archiveFile));
        ProgressEvent::instance().setStep(0);

        s64 freeStorage;
        if(R_SUCCEEDED(nsGetFreeSpaceSize(NcmStorageId_SdCard, &freeStorage)) && getTotalArchiveSize(archiveFile) * 1.1 > freeStorage) {
            brls::Logger::error("sd is full");
            archive_read_free(archive);
            std::filesystem::remove(archiveFile);
            ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
            brls::Application::crash("full");
            std::this_thread::sleep_for(std::chrono::microseconds(2000000));
            brls::Application::quit();
            return false;
        }

        while (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
            if (ProgressEvent::instance().getInterupt()) {
                archive_read_close(archive);
                archive_read_free(archive);
                std::filesystem::remove(archiveFile);
                ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
                return false;
            }
            const char* entryName = archive_entry_pathname(entry);
            
            if ((tid != smash_tid)) {
                std::string folder;
                const std::string entryStr = entryName;
                const size_t at = findModFolder(entryStr, folder);
                if (at != std::string::npos) {

                    // La carpeta del mod en minusculas, aunque el zip la traiga como "Romfs".
                    const std::string rest = folder + entryStr.substr(at + folder.size());
                    std::string outputFilePath;
                    if (folder == "exefs_patches/")
                        outputFilePath = fmt::format("{}/{}", outputDir, rest);
                    else
                        outputFilePath = fmt::format("{}/contents/{}/{}", outputDir, tid, rest);

                    if (std::string(entryName).find("|") != std::string::npos)
                        outputFilePath = outputFilePath.substr(0, outputFilePath.find("|"));
                  
                    brls::Logger::debug("Extracting file {} to {}", entryName,outputFilePath);
                    std::filesystem::path outputPath(outputFilePath);
                    std::filesystem::create_directories(outputPath.parent_path());

                    if (archive_entry_filetype(entry) == AE_IFDIR) {
                        ProgressEvent::instance().incrementStep(1);
                        // Skip directories
                        continue;
                    }


                    std::ofstream outputFile(outputFilePath, std::ios::binary);
                    if (!outputFile) {
                        brls::Logger::error("Failed to create output file: {}", outputFilePath);
                        archive_read_free(archive);
                        std::filesystem::remove(archiveFile);
                        ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
                        return false;
                    }

                    const size_t bufferSize = 100000;
                    char buffer[bufferSize];
                    ssize_t bytesRead;
                    while ((bytesRead = archive_read_data(archive, buffer, bufferSize)) > 0) {
                        outputFile.write(buffer, bytesRead);
                    }

                    outputFile.close();

                    ProgressEvent::instance().incrementStep(1);
                } else {
                    brls::Logger::debug("Skipping {}", entryName);
                }
            } else {
                // Smash bros mods
                std::string outputFilePath = fmt::format("sdmc:/ultimate/mods/{}",std::string(entryName));
                std::filesystem::path outputPath(outputFilePath);
                std::filesystem::create_directories(outputPath.parent_path());
                if (archive_entry_filetype(entry) == AE_IFDIR) {
                    // Create the directory
                    if (!std::filesystem::create_directory(outputPath)) {
                        brls::Logger::error("Failed to create directory: {}", outputFilePath);
                    }
                    ProgressEvent::instance().incrementStep(1);
                    continue;
                }

                std::ofstream outputFile(outputFilePath, std::ios::binary);
                if (!outputFile) {
                    brls::Logger::error("Failed to create output file: {}", outputFilePath);
                    archive_read_free(archive);
                    std::filesystem::remove(archiveFile);
                    ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
                    return false;
                }

                const size_t bufferSize = 100000;
                char buffer[bufferSize];
                ssize_t bytesRead;
                while ((bytesRead = archive_read_data(archive, buffer, bufferSize)) > 0) {
                    outputFile.write(buffer, bytesRead);
                }

                outputFile.close();

                brls::Logger::debug("Extracted file: {}", outputFilePath);
                ProgressEvent::instance().incrementStep(1);
            }
        }

        archive_read_close(archive);
        archive_read_free(archive);
        std::filesystem::remove(archiveFile);
        ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
        return true;
    }
}