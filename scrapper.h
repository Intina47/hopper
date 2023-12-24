// gets sitemap from cassandra and using it to crawl and scrapes the data from the webpages, and returns the data in json format to be accessed later probably create an API, the kind of data is ecommerce data things like price, title, description, image, etc.

#ifndef SCRAPPER_H
#define SCRAPPER_H

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <condition_variable>
#include <nlohmann/json.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include "db.h"
#include "WebPageFetcher.h"

using namespace std;
using json = nlohmann::json;

class Scrapper {
    private:
        // cassandra variables
        mutex mtx;
        condition_variable cv;
        CassSession* session;
        std::ifstream jsonFile;
        json jsonUrls;
        std::vector<std::string> siteUrls;
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
            siteUrls = jsonUrls;
        }

        void process_url(std::string siteUrl){
            get_sitemap_for_url(siteUrl);
            fetch_and_signal_python_scripts();
        }

        void get_sitemap_for_url(std::string siteUrl) {
            std::vector<std::string> sitemapData;
            sitemapData = db.getSitemapFromCassandra(session, siteUrl);
            if (!sitemapData.empty()) {
            sitemap = sitemapData;
            }
        }

        void fetch_and_signal_python_scripts() {
            for (auto siteMapUrl : sitemap) {
                fetch_and_signal_python_script(siteMapUrl);
            }
        }

        void fetch_and_signal_python_script(std::string siteMapUrl) {
            webpage = wpf.fetch(siteMapUrl);
            try{
                //shared memory object
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

                //signal python script
                signal_python_script();
            } catch (boost::interprocess::interprocess_exception &ex) {
                std::cout << "BOOST::EXCEPTION: " << ex.what() << std::endl;
            }
        }

    public:
        Scrapper() {
            get_urls_from_json();
            db.connect();
            session = db.getSession();
        }

        ~Scrapper() {
            jsonFile.close();
            db.close();
        }

        void run(){
            std::cout << "Scrapper running..." << std::endl;
            std::vector<std::thread> threads;
            for (const auto& siteurl : siteUrls) {
                threads.push_back(std::thread(&Scrapper::process_url, this, siteurl));
            }
            for (auto& thread : threads) {
                thread.join();
            }
        }

        // get site map from cassandra
        void get_sitemap(){
            session = db.getSession();
            for (auto url : siteUrls) {
                sitemap = db.getSitemapFromCassandra(session, url);
            }
        }

        // signal python script to start processing the data using a named pipe
        void signal_python_script() {
            std::ofstream pipe;
            pipe.open("pipe", std::ios::out);
            pipe << "1";
            pipe.close();

            // Wait for the named pipe to be created
            const std::string pipePath = "pipe";
            const std::chrono::milliseconds waitTime(100);

            while (!std::ifstream(pipePath)) {
                std::this_thread::sleep_for(waitTime);
            }

            //run python script
            const std::string pythonScript = "html_parser.py";
            std::string command = "python3 " + pythonScript;
            int status = std::system(command.c_str());
            if (status == 0) {
                std::cout << "Python script ran successfully" << std::endl;
            } else {
                std::cout << "Python script failed to run" << std::endl;
            }

        }
};
#endif