include_directories(/usr/local/include)

add_executable(sitemap main.cpp)

find_library(CASSANDRA_LIB cassandra)

target_link_libraries(sitemap ${CASSANDRA_LIBRARIES})
