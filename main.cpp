#include "SitemapGenerator.h"
#include <vector>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

int main() {
    std::cout << "\nSitemap Generator Running....\n" << std::endl;
    std::ifstream jsonFile("urls.json");
    
    if (!jsonFile.is_open()) {
        std::cerr << "Error opening JSON file" << std::endl;
        return 1;
    }

    nlohmann::json jsonUrls;
    jsonFile >> jsonUrls;

    std::vector<std::string> urls = jsonUrls;

    std::string filename = "sitemap.xml";
    SitemapGenerator generator(urls, filename);
    generator.generate();
    return 0;
}