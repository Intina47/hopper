// #ifdef SITEMAPGENERATOR_H
// #define SITEMAPGENERATOR_H

#include <iostream>
#include <regex>
#include <vector>
#include <string>
#include <fstream>
#include <curl/curl.h>
#include <gumbo.h>
#include <cassandra.h>
#include "db.h"

class SitemapGenerator {
public:
    SitemapGenerator(const std::string& url, const std::string& filename)
        : url(url), filename(filename) {
            // connect to cassandra
            db.connect();
            session = db.getSession();
        }

    ~SitemapGenerator() {
        db.close();
    }

    void generate() {
        std::cout << "Generating sitemap for " << url << std::endl;
        generateSitemap();
        std::cout << "Generating sitemap from DB" << std::endl;
        getSitemapFromCassandra(session, siteName, url);
        std::cout << "Sitemap generated successfully From DB" << std::endl;
    }

private:
    std::string url;
    std::string filename;
    CassSession* session;
    std::string siteName;
    DB db;

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
        size_t newLength = size*nmemb;
        try {
            s->append((char*)contents, newLength);
        } catch(std::bad_alloc &e) {
            std::cout << "Not enough memory" << std::endl;
            return 0;
        }
        return newLength;
    }

    std::string fetchWebpage() {
        CURL* curl;
        CURLcode res;
        std::string readBuffer;

        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl = curl_easy_init();
        if(curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            res = curl_easy_perform(curl);
            if(res != CURLE_OK) {
                std::cout << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            } else {
                std::cout << "Webpage fetched successfully" << std::endl;
            }
            curl_easy_cleanup(curl);
        }
        curl_global_cleanup();
        return readBuffer;
    }

    void extractLinks(GumboNode* node, std::vector<std::string>& links) {
        if (node->type != GUMBO_NODE_ELEMENT) {
            return;
        }
        GumboAttribute* href;
        if (node->v.element.tag == GUMBO_TAG_A &&
            (href = gumbo_get_attribute(&node->v.element.attributes, "href"))) {
            links.push_back(href->value);
        }
        GumboVector* children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i) {
            extractLinks(static_cast<GumboNode*>(children->data[i]), links);
        }
    }

    std::string extractSiteName(const std::string& siteUrl) {
        // Extract site name from the site URL (modify as needed)
        std::regex regex("https://www\\.(\\w+)\\.com");
        std::smatch match;
        if (std::regex_search(siteUrl, match, regex)) {
            if (match.size() > 1) {
                return match[1].str();
            }
        }
        return "unknown";
    }
    
    void generateSitemap() {
        std::string webpage = fetchWebpage();
        GumboOutput* output = gumbo_parse(webpage.c_str());
        std::vector<std::string> links;
        extractLinks(output->root, links);
        gumbo_destroy_output(&kGumboDefaultOptions, output);

        std::ofstream file(filename);
        file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        file << "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n";
        for (const std::string& link : links) {
            file << "  <url>\n";
            file << "    <loc>" << link << "</loc>\n";
            file << "  </url>\n";
        }
        file << "</urlset>\n";

        siteName = extractSiteName(url);
        saveSitemapToCassandra(session, siteName, url, links);

    }

    void saveSitemapToCassandra(CassSession* session, const std::string& siteName, const std::string& siteUrl, const std::vector<std::string>& sitemapUrls) {
    // Create and execute a query to insert data
    std::string query = "INSERT INTO sitemaps_keyspace.sitemaps_table (site_name, site_url, sitemap_urls) VALUES (?, ?, ?)";
    CassStatement* statement = cass_statement_new(query.c_str(), 3);

    cass_statement_bind_string(statement, 0, siteName.c_str());
    cass_statement_bind_string(statement, 1, siteUrl.c_str());

    CassCollection* sitemapCollection = cass_collection_new(CASS_COLLECTION_TYPE_SET, sitemapUrls.size());
    for (const std::string& sitemapUrl : sitemapUrls) {
        cass_collection_append_string(sitemapCollection, sitemapUrl.c_str());
    }

    cass_statement_bind_collection(statement, 2, sitemapCollection);

    CassFuture* result_future = cass_session_execute(session, statement);
    // log error into a variable

    CassError rc = cass_future_error_code(result_future);

    if (cass_future_error_code(result_future) != CASS_OK) {
        std::cout << "Error inserting data: " << rc << std::endl;
    } else {
        std::cout << "Data inserted successfully" << std::endl;
    }

    cass_collection_free(sitemapCollection);
    cass_statement_free(statement);
}

// get data from cassandra and save it to an xml file
void getSitemapFromCassandra(CassSession* session, const std::string& siteName, const std::string& siteUrl) {
    std::string query = "SELECT sitemap_urls FROM sitemaps_keyspace.sitemaps_table WHERE site_name = ? AND site_url = ?";
    CassStatement* statement = cass_statement_new(query.c_str(), 2);

    cass_statement_bind_string(statement, 0, siteName.c_str());
    cass_statement_bind_string(statement, 1, siteUrl.c_str());

    CassFuture* result_future = cass_session_execute(session, statement);

    if (cass_future_error_code(result_future) != CASS_OK) {
        std::cout << "Error executing Fetch query" << std::endl;
    } else {
        std::cout << "Fetch Query executed successfully" << std::endl;
    }

    const CassResult* result = cass_future_get_result(result_future);
    const CassRow* row = cass_result_first_row(result);

    // CassValue* value = cass_row_get_column_by_name(row, "sitemap_urls");
    const CassValue* value = cass_row_get_column(row, 0);
    CassIterator* iterator = cass_iterator_from_collection(value);

    std::ofstream file("sitemapDB.xml");
    file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    file << "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n";

    while (cass_iterator_next(iterator)) {
        const char* sitemapUrl;
        size_t sitemapUrlLength;
        cass_value_get_string(cass_iterator_get_value(iterator), &sitemapUrl, &sitemapUrlLength);
        file << "  <url>\n";
        file << "    <loc>" << sitemapUrl << "</loc>\n";
        file << "  </url>\n";
    }

    file << "</urlset>\n";

    cass_iterator_free(iterator);
    cass_statement_free(statement);
    cass_result_free(result);
}

};

// #endif // SITEMAPGENERATOR_H