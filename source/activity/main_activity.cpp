#include "activity/main_activity.hpp"
#include "views/hint_bar.hpp"

using namespace brls::literals;

void AboutActivity::onContentAvailable() {
    /* El foco tiene que quedar en esta pantalla: si no hay nada enfocable, se
       queda en la cuadricula de atras (se veia el recuadro de una portada
       flotando y la cruceta movia portadas que no se veian). */
    if (auto* root = this->getView("about_root")) {
        root->setFocusable(true);
        root->setHideHighlight(true);
        brls::sync([root] { brls::Application::giveFocus(root); });
    }
    if (auto* bar = dynamic_cast<brls::Box*>(this->getView("about_hint")))
        fillHintBar(bar, {{"B", "menu/games/h_back"_i18n}});
}
