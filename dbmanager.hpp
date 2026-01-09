#pragma once
#include <libpq-fe.h>
#include <string>
#include <stdexcept>

class DBManager {
    PGconn* conn;
public:
    DBManager(const std::string& conninfo) {
        conn = PQconnectdb(conninfo.c_str());
        if (PQstatus(conn) != CONNECTION_OK) {
            throw std::runtime_error(PQerrorMessage(conn));
        }
    }
    ~DBManager() { PQfinish(conn); }

    PGconn* get_conn() { return conn; }

    // Вспомогательная функция для выполнения запросов
    PGresult* query(const std::string& sql, const std::vector<const char*>& params) {
        return PQexecParams(conn, sql.c_str(), params.size(), NULL, params.data(), NULL, NULL, 0);
    }
};