// Purpose: Header file for WebPageFetcher.cpp
#ifndef WEBPAGEFETCHER_H
#define WEBPAGEFETCHER_H
#include <curl/curl.h>
#include <string>
#include <vector>
#include <iostream>
#include <ctime>
#include <thread>
#include <chrono>

class WebPageFetcher {
    private:
        std::string read_buffer;
        struct curl_slist *headers = nullptr;
    private:
        static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
            size_t newLength = size * nmemb;
            try {
                s->append(static_cast<char*>(contents), newLength);
            } catch (std::bad_alloc& e) {
                std::cerr << "Not enough memory" << std::endl;
                return 0;
            }
            return newLength;
    }

    void setDefaultOptions(CURL* curl) {
            // set default options
            headers = curl_slist_append(headers, "Accept: text/html");
            headers = curl_slist_append(headers, "Content-Type: text/html; charset=UTF-8");
            headers = curl_slist_append(headers, "charsets: utf-8");
            headers = curl_slist_append(headers, "Accept-Encoding: gzip, deflate, br");
            headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9");
            headers = curl_slist_append(headers, "Connection: keep-alive");
            headers = curl_slist_append(headers, "Upgrade-Insecure-Requests: 1");
            headers = curl_slist_append(headers, "Cache-Control: max-age=0");
            headers = curl_slist_append(headers, "TE: Trailers");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_ENCODING, "gzip, deflate, br");

            // Randomize User-Agent
            std::vector<std::string> user_agents = {
                    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537",
                    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/109.0.0.0 Safari/537.36",
                    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/109.0.0.0 Safari/537.36",
                    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36",
                    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36",
                    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/108.0.0.0 Safari/537.36",
                    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.1 Safari/605.1.15",
                    "Mozilla/5.0 (Macintosh; Intel Mac OS X 13_1) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.1 Safari/605.1.15"
            };
            std::srand(std::time(nullptr));
            std::string user_agent = user_agents[std::rand() % user_agents.size()];
            // delay between requests
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
            // handle cookies
            curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "cookies.txt");
            curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "cookies.txt");
            // handle redirects
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
            curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
            curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
            curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    }

    public:
        WebPageFetcher(){
            curl_global_init(CURL_GLOBAL_DEFAULT);
        }
        ~WebPageFetcher(){
            if(headers){
                curl_slist_free_all(headers);
            }
            curl_global_cleanup();
        }
        std::string fetch(const std::string& url){
            CURL* curl = curl_easy_init();
            if (curl) {
                setDefaultOptions(curl);
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
                CURLcode res = curl_easy_perform(curl);
                if (res != CURLE_OK) {
                    std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
                }
                curl_easy_cleanup(curl);
            } else {
                std::cerr << "Error initializing CURL" << std::endl;
            }
            curl_easy_cleanup(curl);
            return read_buffer;
        }
        void print(){
            std::cout << read_buffer << std::endl;
        }
};
#endif