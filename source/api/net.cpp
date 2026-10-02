#include "api/net.hpp"
#include "utils/progress_event.hpp"

#include <curl/curl.h>
#include <borealis.hpp>
#include <fstream>
#include <mutex>

namespace net {
    std::chrono::_V2::steady_clock::time_point time_old;
    double dlold;

    /* OQB: el libcurl de devkitPro (7.69.1) va enlazado contra un mbedTLS
     * compilado SIN MBEDTLS_THREADING_C, asi que no es thread-safe. Al pulsar
     * descargar, loadFile() corre en el hilo de la UI mientras loadImages()
     * sigue bajando capturas en secondThread: dos curl_easy_perform() a la vez
     * corrompen memoria y la app se cierra (o crashea con PC=0 dentro del
     * vector del callback de imagenes). Serializamos todo el acceso a la red. */
    static std::mutex curlMutex;

    size_t WriteCallback(void* content, size_t size, size_t nmemb, std::string* buffer) {
        buffer->append((char*)content, size * nmemb);
        return size * nmemb;
    }

    nlohmann::json downloadRequest(std::string url) {
        std::lock_guard<std::mutex> lock(curlMutex);
        auto curl = curl_easy_init();

        brls::Logger::debug("Requesting: " + url);
        if(!curl) {
            brls::Logger::error("Failed to initialize curl");
            return nlohmann::json();
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        std::string response;

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "SimpleModDownloader");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        auto res = curl_easy_perform(curl);

        nlohmann::json json;

        if(res != CURLE_OK) {
            brls::Logger::error("Failed to perform request: " + std::string(curl_easy_strerror(res)));
        } else {
            /* OQB: si la respuesta no es JSON valido (error HTML, 406, corte de
             * conexion) parse() lanza y la excepcion sube por un hilo sin
             * try/catch -> std::terminate. Devolvemos un json vacio. */
            try {
                json = nlohmann::json::parse(response);
            } catch (const std::exception& e) {
                brls::Logger::error("Failed to parse JSON from {}: {}", url, e.what());
            }
        }

        curl_easy_cleanup(curl);

        return json;
    }

    size_t WriteCallbackImage(char* ptr, size_t size, size_t nmemb, void* userdata) {
        if (ProgressEvent::instance().getInterupt()) {
            return 0;
        }
        std::vector<unsigned char>* buffer = static_cast<std::vector<unsigned char>*>(userdata);
        size_t total_size = size * nmemb;
        buffer->insert(buffer->end(), ptr, ptr + total_size);
        return total_size;
    }

    void downloadImage(const std::string& url, std::vector<unsigned char>& buffer) {
        std::lock_guard<std::mutex> lock(curlMutex);
        auto curl = curl_easy_init();

        brls::Logger::debug("Downloading image: {}", url);
        if(!curl) {
            brls::Logger::error("Failed to initialize curl");
            return;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallbackImage);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 120000L);

        auto res = curl_easy_perform(curl);
        if(res != CURLE_OK)
            brls::Logger::error(curl_easy_strerror(res));
        curl_easy_cleanup(curl);
    }

    size_t WriteCallbackFile(void* ptr, size_t size, size_t nmemb, std::ofstream *stream) {
        stream->write(static_cast<char *>(ptr), size * nmemb);
        return size * nmemb;
    }
    
    int downloadFileProgress(void* p, double dltotal, double dlnow, double ultotal, double ulnow) {
        //brls::Logger::debug("Downloaded: {} / {}", dlnow, dltotal);
        if(dltotal < 0.0) return 0;

        double progress = dlnow / dltotal;
        int counter = (int)(progress * ProgressEvent::instance().getMax());
        ProgressEvent::instance().setStep(std::min(ProgressEvent::instance().getMax() - 1, counter));
        ProgressEvent::instance().setNow(dlnow);
        ProgressEvent::instance().setTotalCount(dltotal);
        auto time_now = std::chrono::steady_clock::now();
        double elapsed_time = ((std::chrono::duration<double>)(time_now - time_old)).count();
        if(elapsed_time > 1.2f) {
            ProgressEvent::instance().setSpeed((dlnow - dlold) / elapsed_time);
            dlold = dlnow;
            time_old = time_now;
        }

        return 0;
    }

    void downloadFile(const std::string& url, const std::string& path) {
        brls::Logger::debug("Downloading file: {}, in the location : {}", url, path);

        std::lock_guard<std::mutex> lock(curlMutex);
        auto curl = curl_easy_init();

        if(!curl) {
            brls::Logger::error("Failed to initialize curl");
            return;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, downloadFileProgress);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        std::ofstream ofs(path, std::ios::binary); 

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallbackFile);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ofs);

        auto res = curl_easy_perform(curl);

        ofs.close();

        curl_easy_cleanup(curl);

        if(res != CURLE_OK) {
            brls::Logger::error(curl_easy_strerror(res));
            std::filesystem::remove(path);
        }
    }

}