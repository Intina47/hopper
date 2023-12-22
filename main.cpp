#include "SitemapGenerator.h"
#include <vector>

int main() {
    std::vector<std::string> urls = {"https://www.fatface.com", "https://www.boohoo.com/", "https://www.asos.com/", "https://www.missguided.co.uk/", "https://www.prettylittlething.com/", "https://www.newlook.com/uk", "https://www.topshop.com/", "https://www.riverisland.com/", "https://www.next.co.uk/", "https://www.marksandspencer.com/", "https://www.zara.com/uk/", "https://www.hm.com/gb", "https://www.johnlewis.com/", "https://www.debenhams.com/", "https://www.houseoffraser.co.uk/", "https://www.selfridges.com/GB/en/", "https://www.harveynichols.com/", "https://www.tkmaxx.com/uk/en/", "https://www.very.co.uk/", "https://www.littlewoods.com/", "https://www.very.co.uk/", "https://www.littlewoods.com/", "https://www.very.co.uk/", "https://www.littlewoods.com/", "https://www.very.co.uk/", "https://www.littlewoods.com/", "https://www.very.co.uk/", "https://www.very.co.uk/"};
    std::string filename;
    for(auto& url : urls) {
        std::string filename = url.substr(url.find("www.")+4);
        filename = filename.substr(0, filename.find("."));
        filename += ".xml";
    }
    SitemapGenerator generator(urls, filename);
    generator.generate();
    return 0;
}