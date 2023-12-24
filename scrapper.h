// gets sitemap from cassandra and using it to crawl and scrapes the data from the webpages, and returns the data in json format to be accessed later probably create an API, the kind of data is ecommerce data things like price, title, description, image, etc.

#ifndef SCRAPPER_H
#define SCRAPPER_H

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <nlohmann/json.hpp>
#include <gumbo.h>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include "db.h"
#include "WebPageFetcher.h"

using namespace std;
using json = nlohmann::json;

class Scrapper {
    private:
        // cassandra variables
        GumboOutput* output;
        json j;
        mutex mtx;
        condition_variable cv;
        CassSession* session;
        std::ifstream jsonFile;
        json jsonUrls;
        std::vector<std::string> urls;
        DB db;
        WebPageFetcher wpf;
        std::string webpage;
        std::vector<std::string> sitemap;
    private:
        void get_urls_from_json() {
            jsonFile.open("urls.json");
            if (!jsonFile.is_open()) {
                std::cerr << "Error opening JSON file" << std::endl;
                return;
            }
            jsonFile >> jsonUrls;
            urls = jsonUrls;
        }

    public:
        Scrapper() {
            get_urls_from_json();
        }

        ~Scrapper() {
            gumbo_destroy_output(&kGumboDefaultOptions, output);
            jsonFile.close();
        }

        void run(){
            get_sitemap();
            get_webpage();
            signal_python_script();
        }

        // get site map from cassandra
        void get_sitemap(){
            session = db.getSession();
            db.connect();
            for (auto url : urls) {
                sitemap = db.getSitemapFromCassandra(session, url);
            }
            db.close();
        }

        void get_webpage(){
            for (auto url : urls) {
                webpage = wpf.fetch(url);

                // shared memory object
                boost::interprocess::shared_memory_object shm(
                    boost::interprocess::open_or_create,
                    "webpage",
                    boost::interprocess::read_write
                );

                // set size
                shm.truncate(webpage.size() + 1);

                // map the whole shared memory in this process
                boost::interprocess::mapped_region region(
                    shm,
                    boost::interprocess::read_write
                );

                // copy data to shared memory
                std::memcpy(region.get_address(), webpage.c_str(), webpage.size() + 1);

                // remove shared memory
                boost::interprocess::shared_memory_object::remove("webpage");

                // signal python script to start processing the data using a named pipe and handle synchronization between the C++ and Python parts of your program to ensure that the Python script doesn't start reading the HTML content before the C++ program has finished writing it
                signal_python_script();
  
            }
        }

        // signal python script to start processing the data using a named pipe
        void signal_python_script() {
            std::ofstream pipe;
            pipe.open("pipe", std::ios::out);
            pipe << "start";
            pipe.close();
        }
};
#endif