#include "SitemapGenerator.h"

int main() {
    SitemapGenerator generator("https://www.boohoo.com/", "sitemap.xml");
    generator.generate();
    return 0;
}