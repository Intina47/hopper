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
// #include <cassandra/cassandra.h>
class SitemapGenerator {
public:
    SitemapGenerator(const std::string& url, const std::string& filename)
        : url(url), filename(filename) {
            // connect to cassandra
            std::cout << "Connecting to cassandra" << std::endl;
            cluster = cass_cluster_new();
            session = cass_session_new();
            cass_cluster_set_contact_points(cluster, "127.0.0.1");
            cass_cluster_set_port(cluster, 9042);
            connect_future = cass_session_connect(session, cluster);

            CassError rc = cass_future_error_code(connect_future);
            if (rc != CASS_OK) {
                std::cout << "Error connecting to cassandra" << std::endl;
            } else {
                std::cout << "Connected to cassandra" << std::endl;
                createTables(session);
            }
        }

    ~SitemapGenerator() {
        // re
        cass_future_free(connect_future);
        cass_session_free(session);
        cass_cluster_free(cluster);
    }

    void generate() {
        std::cout << "Generating sitemap for " << url << std::endl;
        generateSitemap();
    }

    void createTables(CassSession* session) {
        // Create keyspace if not exists
        std::string createKeyspaceQuery = "CREATE KEYSPACE IF NOT EXISTS sitemaps_keyspace WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}";
        executeCqlQuery(session, createKeyspaceQuery);

        // Create sitemaps_table
        std::string createTableQuery = "CREATE TABLE IF NOT EXISTS sitemaps_keyspace.sitemaps_table ("
                                       "site_name text,"
                                       "site_url text,"
                                       "sitemap_urls set<text>,"
                                       "PRIMARY KEY (site_name, site_url)"
                                       ")";
        executeCqlQuery(session, createTableQuery);
    }

private:
    std::string url;
    std::string filename;
    CassCluster* cluster;
    CassSession* session;
    CassFuture* connect_future;
    std::string siteName;

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

    void executeCqlQuery(CassSession* session, const std::string& query) {
        CassStatement* statement = cass_statement_new(query.c_str(), 0);
        CassFuture* result_future = cass_session_execute(session, statement);

        if (cass_future_error_code(result_future) != CASS_OK) {
            std::cout << "Error executing query" << std::endl;
        } else {
            std::cout << "Query executed successfully" << std::endl;
        }

        cass_statement_free(statement);
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

    if (cass_future_error_code(result_future) != CASS_OK) {
        // Handle insert error
        std::cout << "Error inserting data" << std::endl;
    } else {
        std::cout << "Data inserted successfully" << std::endl;
    }

    cass_collection_free(sitemapCollection);
    cass_statement_free(statement);
}

};

// #endif // SITEMAPGENERATOR_H