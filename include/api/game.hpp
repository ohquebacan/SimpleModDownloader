#pragma once 

#include <string>
#include <map>
#include <borealis.hpp>
#include <nlohmann/json.hpp>

class Category {
public:
    Category(const std::string& name, const int& ID, const int& index) : name(name), ID(ID), index(index) {}
    Category() {}
    std::string getName() { return name; }
    int getID() { return ID; }
    int getIndex() { return index; }
private:
    std::string name = "";
    int ID = 0;
    int index = 0;
};

class Game {
    public:
        Game(const std::string& title, const std::string& tid);
        Game() {}
        // OQB: lo que dice la cache: 1 tiene pagina en GameBanana con mods, 0 no (o sin mods), -1 no se sabe.
        static int estadoGameBanana(const std::string& tid);
        // Lo mismo para todos los juegos de la cache, leyendola una sola vez (tid -> 1/0).
        static std::map<std::string, int> estadosGameBanana();
        // Borra de la cache estos juegos (para volver a revisarlos).
        static void olvidarGameBanana(const std::vector<std::string>& tids);
        
        std::string getTitle() { return title; }
        int getGamebananaID() { return gamebananaID; }
        std::string getTid() { return tid; }
        brls::Image* getIcon() { return icon; }
        std::string getBannerUrl() { return bannerURL; }
        std::vector<Category> getCategories() { return categories; }
    private:
        void loadCategories();
        void searchGame();
        void parseJson();

        nlohmann::json json;
        nlohmann::json json_categories;

        std::string title;
        std::string bannerURL;
        int gamebananaID = 0;
        int modCount = 0;  // OQB: mods en su pagina de GameBanana
        std::string tid;
        bool strictFallback = false;
        std::vector<Category> categories;
        brls::Image* icon;
};