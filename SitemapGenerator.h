#include <iostream>
#include <regex>
#include <vector>
#include <string>
#include <fstream>
#include <curl/curl.h>
#include <gumbo.h>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable> 
#include "db.h"

class SitemapGenerator {
public:
    SitemapGenerator(const std::vector<std::string>& urls, const std::string& filename)
        : urls(urls), filename(filename) {
            // log all the urls to be processed in the console
            for(const auto& url : urls) {
                std::cout << "\n------URLS--------\n" << url << std::endl;
            }
            // connect to cassandra
            db.connect();
            session = db.getSession();
        }

    ~SitemapGenerator() {
        db.close();
    }

    void generate() {
        std::queue<std::string> urlQueue;
        for(const auto& url : urls) {
            urlQueue.push(url);
        }

        const int numThreads = 4;
        std::vector<std::thread> threads;

        // thread-safe access to urlQueue
        std::mutex urlQueueMutex;
        std::condition_variable urlQueueCondition;
        bool stopThreads = false;
        for(int i=0; i<numThreads; ++i){
            threads.emplace_back([this, &urlQueue, &urlQueueMutex, &urlQueueCondition, &stopThreads ](){
                while(true){
                    std::string currentUrl;
                    {
                        std::unique_lock<std::mutex> lock(urlQueueMutex);
                        urlQueueCondition.wait(lock, [&]{ 
                            std::cout << "Queue size: " << urlQueue.size() << std::endl;
                            return !urlQueue.empty() || stopThreads; 
                            });
                        if(urlQueue.empty()){
                            std::cout << "Queue is empty" << std::endl;
                            break;
                        }
                        currentUrl = urlQueue.front();
                        urlQueue.pop();
                    }
                    generateSitemap(currentUrl);
                }
            });
        }
        // notify threads that queue is empty
        urlQueueCondition.notify_all();
        // wait for threads to finish
        for(auto& thread : threads){
            thread.join();
        }
        std::cout << "All threads finished" << std::endl;
    }

private:
    std::vector<std::string> urls;
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

    std::string fetchWebpage(const std::string& url) {
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
// Extract site name from the site URL
    std::string extractSiteName(const std::string& siteUrl) {
        std::regex regex("https://www\\.(\\w+)\\.com");
        std::smatch match;
        std::string returnStatement;
        if (std::regex_search(siteUrl, match, regex)) {
            if (match.size() > 1) {
                return match[1].str();
            } else {
                returnStatement = "unknown";
            }
        }
        if (returnStatement == "unknown")
        {
         std::string filename = siteUrl.substr(siteUrl.find("www.")+4);
         filename = filename.substr(0, filename.find("."));
         return filename;
        }
        
        return "Noname";
    }
    
    void generateSitemap(const std::string& url) {
        std::cout << "Generating sitemap for " << url << std::endl;
        siteName = extractSiteName(url);

        std::string webpage = fetchWebpage(url);
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

        saveSitemapToCassandra(session, siteName, url, links);

    }

    void saveSitemapToCassandra(CassSession* session, const std::string& siteName, const std::string& siteUrl, const std::vector<std::string>& sitemapUrls) {

    std::string query = "SELECT site_name, site_url, sitemap_urls FROM sitemaps_keyspace.sitemaps_table WHERE site_name = ? AND site_url = ?";
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

    if (row == NULL) {
        std::cout << "Site doesn't exist in the database" << std::endl;
        insertSitemapToCassandra(session, siteName, siteUrl, sitemapUrls);
    } else {
        std::cout << "Site exists in the database" << std::endl;
        const CassValue* value = cass_row_get_column(row, 2);
        CassIterator* iterator = cass_iterator_from_collection(value);

        std::vector<std::string> existingSitemapUrls;
        while (cass_iterator_next(iterator)) {
            const char* sitemapUrl;
            size_t sitemapUrlLength;
            cass_value_get_string(cass_iterator_get_value(iterator), &sitemapUrl, &sitemapUrlLength);
            existingSitemapUrls.push_back(sitemapUrl);
        }

        cass_iterator_free(iterator);

        if (existingSitemapUrls.empty()) {
            std::cout << "Sitemap urls column is empty" << std::endl;
            insertSitemapToCassandra(session, siteName, siteUrl, sitemapUrls);
        } else {
            std::cout << "Sitemap urls column is not empty" << std::endl;
            std::vector<std::string> newSitemapUrls;
            for (const std::string& sitemapUrl : sitemapUrls) {
                if (std::find(existingSitemapUrls.begin(), existingSitemapUrls.end(), sitemapUrl) == existingSitemapUrls.end()) {
                    newSitemapUrls.push_back(sitemapUrl);
                }
            }

            if (newSitemapUrls.empty()) {
                std::cout << "No new sitemap urls to insert" << std::endl;
            } else {
                std::cout << "New sitemap urls to insert" << std::endl;
                insertSitemapToCassandra(session, siteName, siteUrl, newSitemapUrls);
            }
        }
    }

    cass_statement_free(statement);
    cass_result_free(result);
}

 void insertSitemapToCassandra(CassSession* session, const std::string& siteName, const std::string& siteUrl, const std::vector<std::string>& sitemapUrls) {
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