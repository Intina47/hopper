#include "SitemapGenerator.h"

int main() {
    SitemapGenerator generator("https://www.fatface.com", "sitemap.xml");
    generator.generate();
    return 0;
}