#include <stdio.h>
#include <iostream>
#include <cassandra.h>

class DB {
public:
  void connect(){
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
  void close(){
            cass_future_free(connect_future);
            cass_session_free(session);
            cass_cluster_free(cluster);
  }
      void createTables(CassSession* session) {
        // Create keyspace if not exists
        std::string createKeyspaceQuery = "CREATE KEYSPACE IF NOT EXISTS sitemaps_keyspace WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}";
        executeCqlQuery(session, createKeyspaceQuery);

        // std::string dropTableQuery = "DROP TABLE IF EXISTS sitemaps_keyspace.sitemaps_table";
        // executeCqlQuery(session, dropTableQuery);

        std::string createTableQuery = "CREATE TABLE IF NOT EXISTS sitemaps_keyspace.sitemaps_table ("
                                       "site_name text,"
                                       "site_url text,"
                                       "sitemap_urls set<text>,"
                                       "PRIMARY KEY (site_name, site_url)"
                                       ")";
        executeCqlQuery(session, createTableQuery);
    }

    void setSession(CassSession* session) {
        this->session = session;
    }

    CassSession* getSession() {
        return session;
    }
    
private:
    CassCluster *cluster;
    CassSession *session;
    CassFuture *connect_future;

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
};
