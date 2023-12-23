#include "SitemapGenerator.h"
#include <vector>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

int main() {
    std::ifstream jsonFile("urls.json");
    
    if (!jsonFile.is_open()) {
        std::cerr << "Error opening JSON file" << std::endl;
        return 1;
    }

    nlohmann::json jsonUrls;
    jsonFile >> jsonUrls;

    std::vector<std::string> urls = jsonUrls;

    std::string filename = "sitemap.xml";
    // for(auto& url : urls) {
    //     std::string filename = url.substr(url.find("www.")+4);
    //     filename = filename.substr(0, filename.find("."));
    //     filename += ".xml";
    // }
    SitemapGenerator generator(urls, filename);
    generator.generate();
    return 0;
}