#include "scrapper.h"
#include <vector>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

int main() {
    std::cout << "\nScrapper Running....\n" << std::endl;
    Scrapper scrapper;
    scrapper.run();
    return 0;
}
