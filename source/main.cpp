#include <borealis.hpp>
#include <switch.h>
#include <curl/curl.h>
#include <filesystem>
#include <iostream>

#include "views/game_list_tab.hpp"
#include "views/installation_view.hpp"
#include "views/settings_tab.hpp"
#include "activity/main_activity.hpp"
#include "utils/config.hpp" 
#include "views/oqb_ui.hpp"

void init();
void exit();

/* OQB: estilo nuevo (el mismo de SimpleModManager). Instrument Sans pasa a ser
   la letra normal; Upheaval, la Semibold y JetBrains Mono quedan por nombre.
   Los iconos de botones, el cirilico y el CJK siguen saliendo de las letras de
   la consola. Colores Amatista sobre el tema oscuro, siempre. */
static void oqbEstilo() {
    NVGcontext* vg = brls::Application::getNVGContext();
    const int sistema = brls::Application::getFont(brls::FONT_REGULAR);
    const std::string respaldos[] = {
        brls::FONT_CHINESE_SIMPLIFIED, brls::FONT_CHINESE_SIMPLIFIED_EXT, brls::FONT_CHINESE_TRADITIONAL,
        brls::FONT_KOREAN_REGULAR, brls::FONT_SWITCH_ICONS, brls::FONT_MATERIAL_ICONS,
    };
    const std::pair<const char*, const char*> fuentes[] = {
        {"oqb-texto", "font/instrument_sans_medium.ttf"},
        {"oqb-negrita", "font/instrument_sans_semibold.ttf"},
        {"oqb-mono", "font/jetbrains_mono_medium.ttf"},
        {"oqb-marca", "font/upheaval.ttf"},
        {"regular", "font/instrument_sans_medium.ttf"},  // al final: reemplaza la letra normal
    };
    for (const auto& [nombre, archivo] : fuentes) {
        if (!brls::Application::loadFontFromFile(nombre, std::string(BRLS_RESOURCES) + archivo)) {
            brls::Logger::error("No se pudo cargar la letra {}", archivo);
            continue;
        }
        const int id = brls::Application::getFont(nombre);
        if (!vg || id == brls::FONT_INVALID) continue;
        if (sistema != brls::FONT_INVALID && sistema != id) nvgAddFallbackFontId(vg, id, sistema);
        for (const auto& r : respaldos) {
            const int fid = brls::Application::getFont(r);
            if (fid != brls::FONT_INVALID && fid != id) nvgAddFallbackFontId(vg, id, fid);
        }
    }

    oqb::usarPaleta(oqb::leerTema());
    brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);
    oqb::aplicarTemaBorealis();
}

int main(int argc, char* argv[]) {     
    init();
    
    brls::Logger::setLogLevel(brls::LogLevel::LOG_DEBUG);

    std::filesystem::create_directories("sdmc:/config/SimpleModDownloader");

    #ifdef NDEBUG //release
        // Using FILE* because brls::Logger::setLogOutput only takes FILE*, not std::ofstream
        FILE* logFile = fopen("sdmc:/config/SimpleModDownloader/log.log", "w");
        if (logFile) {
            /* OQB: sin buffer, para que un cierre inesperado no se lleve las
             * ultimas lineas del log. Es lo que permitio localizar el crash. */
            setvbuf(logFile, nullptr, _IONBF, 0);
            brls::Logger::setLogOutput(logFile);
        }
    #endif


    {
        cfg::Config config;
        if(config.getAppLanguage() != "auto") {
            brls::Platform::APP_LOCALE_DEFAULT = config.getAppLanguage();
            brls::Logger::debug("Loaded translations for language {}", config.getAppLanguage());
        }
    }

    if(!brls::Application::init()) {
        brls::Logger::error("Unable to init Borealis application");
    }

    brls::loadTranslations();

    brls::Application::setGlobalQuit(false);

    brls::Application::createWindow(fmt::format("UpcomingSwitchGames"));
    brls::Application::setGlobalQuit(false);
    oqbEstilo();  // despues de crear la ventana: ahi existen NanoVG y las letras de la consola

    //XML View
    brls::Application::registerXMLView("GameListTab", GameListTab::create);
    brls::Application::registerXMLView("InstallationTab", InstallationView::create);
    brls::Application::registerXMLView("SettingsTab", SettingsTab::create);

    // Add custom values to the theme
    brls::Theme::getLightTheme().addColor("captioned_image/caption", nvgRGB(2, 176, 183));
    brls::Theme::getDarkTheme().addColor("captioned_image/caption", nvgRGB(51, 186, 227));

    // Add custom values to the style
    brls::getStyle().addMetric("about/padding_top_bottom", 50);
    brls::getStyle().addMetric("about/padding_sides", 75);
    brls::getStyle().addMetric("about/description_margin", 50);

    brls::Application::pushActivity(new MainActivity());

    while(brls::Application::mainLoop()) ;

    exit();

    return -1;
}

void init() {
    setsysInitialize();
    socketInitializeDefault();
    nxlinkStdio();
    plInitialize(PlServiceType_User);
    nsInitialize();
    pmdmntInitialize();
    pminfoInitialize();
    splInitialize();
    fsInitialize();
    romfsInit();
    setInitialize();
    psmInitialize();
    nifmInitialize(NifmServiceType_User);
    lblInitialize();
    curl_global_init(CURL_GLOBAL_ALL);
}

void exit() {
    lblExit();
    nifmExit();
    psmExit();
    setExit();
    romfsExit();
    splExit();
    pminfoExit();
    pmdmntExit();
    nsExit();
    setsysExit();
    fsExit();
    plExit();
    socketExit();
    curl_global_cleanup();
}