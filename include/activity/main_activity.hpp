#pragma once 

#include <borealis.hpp>

class MainActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/main.xml")
};
// OQB: Instalacion, Ajustes y Acerca de, fuera de la pantalla principal (R).
class MoreActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/more.xml")
};

// OQB: «Acerca de» solo (imagen y texto), desde el panel de ajustes.
class AboutActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/about.xml")
    void onContentAvailable() override;
};
