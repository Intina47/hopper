include_directories(/usr/local/include)

# Your target definition
add_executable(sitemap sitemap.cpp)

find_library(CASSANDRA_LIB cassandra)

# Link against the DataStax C++ Driver
target_link_libraries(sitemap ${CASSANDRA_LIBRARIES})
