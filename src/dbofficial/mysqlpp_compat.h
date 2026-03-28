/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU Affero General Public License as            *
 * published by the Free Software Foundation, either version 3 of the        *
 * License, or (at your option) any later version.                           *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU Affero General Public License for more details.                       *
 *                                                                           *
 * You should have received a copy of the GNU Affero General Public License  *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *                                                                           *
 *                                                                           *
 * Additional permission under GNU AGPL version 3 section 7                  *
 *                                                                           *
 * If you modify this program, or any covered work, by linking or            *
 * combining it with the OpenSSL project's OpenSSL library (or a             *
 * modified version of that library), containing parts covered by the        *
 * terms of the OpenSSL or SSLeay licenses, the authors of PokerTH           *
 * (Felix Hammer, Florian Thauer, Lothar May) grant you additional           *
 * permission to convey the resulting work.                                  *
 * Corresponding Source for a non-source form of such a combination          *
 * shall include the source code for the parts of OpenSSL used as well       *
 * as that of the covered work.                                              *
 *****************************************************************************/
// Minimal compatibility layer to replace mysql++ usage with Qt6::Sql
// This header provides a tiny subset of the mysqlpp API used in src/dbofficial
// and implements it on top of Qt6::Sql. It is intentionally minimal and
// only supports the operations used by the existing code.

#pragma once

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtCore/QVariant>
#include <QtCore/QDateTime>
#include <QtSql/QSqlRecord>
#include <string>
#include <vector>
#include <sstream>
#include <mutex>
#include <atomic>

namespace mysqlpp {

// Dummy option used in original code. We accept it but ignore specifics.
class SetCharsetNameOption {
public:
    explicit SetCharsetNameOption(const char* charset) : m_charset(charset) {}
    std::string m_charset;
};

// Simple string wrapper to imitate mysqlpp::String
struct String {
    String() : m_isNull(true) {}
    explicit String(const std::string &s) : s(s), m_isNull(false) {}
    explicit String(const char* c) : s(c ? c : ""), m_isNull(!c) {}
    operator std::string() const { return s; }
    std::string s;
    bool is_null() const { return m_isNull; }
    void to_string(std::string &out) const { out = s; }
    void setNull(bool n) { m_isNull = n; }
    operator int() const { try { return s.empty() ? 0 : std::stoi(s); } catch(const std::exception&) { return 0; } }
    operator unsigned int() const { try { return s.empty() ? 0u : static_cast<unsigned int>(std::stoul(s)); } catch(const std::exception&) { return 0u; } }
private:
    bool m_isNull = false;
};

// Simple DateTime wrapper that converts to a string acceptable by SQL
struct DateTime {
    explicit DateTime(time_t t = 0) { setTime(t); }
    void setTime(time_t t) { dt = QDateTime::fromSecsSinceEpoch((qint64)t); }
    operator std::string() const { return dt.toString(Qt::ISODate).toStdString(); }
private:
    QDateTime dt;
};

// Forward declarations
class Connection;

// A small result set wrapper providing result[row][col] access
class StoreQueryResult {
public:
    struct Row {
        Row(std::vector<std::string> &r, std::vector<bool> &n) : row(r), nullFlags(n) {}
        String operator[](size_t i) const {
            if (i < row.size()) {
                String s(row[i]);
                if (i < nullFlags.size() && nullFlags[i])
                    s.setNull(true);
                return s;
            }
            String s;
            s.setNull(true);
            return s;
        }
        std::vector<std::string> &row;
        std::vector<bool> &nullFlags;
        bool empty() const { return row.empty(); }
    };

    StoreQueryResult() : m_valid(false) {}
    void setValid(bool v) { m_valid = v; }
    bool operator()() const { return m_valid; }
    explicit operator bool() const { return m_valid; }

    void addRow(const std::vector<std::string> &r) { m_rows.emplace_back(r); }
    void addNullFlags(const std::vector<bool> &n) { m_nullFlags.emplace_back(n); }
    Row operator[](size_t i) {
        if (i >= m_rows.size())
            throw std::out_of_range("StoreQueryResult index out of range");
        return Row(m_rows[i], m_nullFlags[i]);
    }
    size_t size() const { return m_rows.size(); }
    size_t num_rows() const { return m_rows.size(); }

private:
    bool m_valid;
    std::vector<std::vector<std::string>> m_rows;
    std::vector<std::vector<bool>> m_nullFlags;
};

// Simple quote manipulator used in original code
struct Quote {};
static Quote quote;

// Query builder / executor
class Query {
public:
    explicit Query(Connection *c = nullptr);

    // stream-like appenders
    Query &operator<<(const char *s) { append(std::string(s)); return *this; }
    Query &operator<<(const std::string &s) { append(s); return *this; }
    Query &operator<<(const Quote &) { m_quoteNext = true; return *this; }
    template<typename T>
    Query &operator<<(const T &v) { std::ostringstream tmp; tmp << v; append(tmp.str()); return *this; }

    bool exec();
    StoreQueryResult store();
    const char *error() const { return m_lastError.c_str(); }
    void reset() { m_ss.str(""); m_ss.clear(); m_lastError.clear(); m_quoteNext = false; }

private:
    void append(const std::string &s) {
        if (m_quoteNext) {
            m_ss << '\'' << escape(s) << '\'';
            m_quoteNext = false;
        } else {
            m_ss << s;
        }
    }
    std::string escape(const std::string &in) const {
        std::string out;
        out.reserve(in.size()*2);
        for (char c : in) {
            switch (c) {
                case '\0': out += "\\0"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\x1a': out += "\\Z"; break;
                case '\'': out += "\\'"; break;
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                default: out.push_back(c); break;
            }
        }
        return out;
    }

    Connection *m_conn;
    std::ostringstream m_ss;
    bool m_quoteNext = false;
    std::string m_lastError;
};

// Lightweight connection wrapper using QSqlDatabase
class Connection {
public:
    Connection() : m_connected(false) {}
    // Some code constructs Connection(false) with mysql++ - accept and ignore
    explicit Connection(bool) : m_connected(false) {}

    bool connect(const char *dbName, const char *host, const char *user, const char *pwd) {
        std::lock_guard<std::mutex> l(m_mutex);
        if (m_db.isValid() && m_db.isOpen()) {
            m_db.close();
            QString oldName = m_connName;
            m_connName.clear();
            m_db = QSqlDatabase();
            QSqlDatabase::removeDatabase(oldName);
        }
        static std::atomic<int> instance{0};
        m_connName = QString("pokerth_dbofficial_%1").arg(++instance);
        m_db = QSqlDatabase::addDatabase("QMYSQL", m_connName);
        m_db.setHostName(host);
        m_db.setDatabaseName(dbName);
        m_db.setUserName(user);
        m_db.setPassword(pwd);
        if (!m_db.open()) {
            m_lastError = m_db.lastError().text().toStdString();
            m_connected = false;
            return false;
        }
        m_connected = true;
        return true;
    }

    void disconnect() {
        std::lock_guard<std::mutex> l(m_mutex);
        if (m_db.isValid() && m_db.isOpen()) m_db.close();
        QString name = m_connName;
        m_connName.clear();
        m_db = QSqlDatabase();
        if (!name.isEmpty())
            QSqlDatabase::removeDatabase(name);
        m_connected = false;
    }

    bool connected() const { return m_connected; }

    Query query() { return Query(this); }

    void set_option(SetCharsetNameOption *opt) {
        if (opt && m_db.isValid() && m_db.isOpen()) {
            static const std::vector<std::string> allowedCharsets = {
                "utf8", "utf8mb4", "latin1", "ascii", "binary", "utf16", "utf32"
            };
            const std::string& charset = opt->m_charset;
            bool valid = false;
            for (const auto& allowed : allowedCharsets) {
                if (charset == allowed) {
                    valid = true;
                    break;
                }
            }
            if (!valid) {
                m_lastError = "Invalid charset: " + charset;
                return;
            }
            QSqlQuery q(m_db);
            q.exec(QString::fromStdString("SET NAMES '" + charset + "'"));
        }
    }

    bool execSQL(const std::string &sql, std::string &errorOut, StoreQueryResult *out = nullptr) {
        std::lock_guard<std::mutex> l(m_mutex);
        if (!m_db.isValid() || !m_db.isOpen()) {
            errorOut = "No DB connection";
            return false;
        }
        QSqlQuery q(m_db);
        bool ok = q.exec(QString::fromStdString(sql));
        if (!ok) {
            errorOut = q.lastError().text().toStdString();
            return false;
        }
        if (out) {
            QSqlRecord rec = q.record();
            int cols = rec.count();
            while (q.next()) {
                std::vector<std::string> row;
                std::vector<bool> nullRow;
                row.reserve(cols);
                nullRow.reserve(cols);
                for (int i=0;i<cols;++i) {
                    nullRow.push_back(q.value(i).isNull());
                    if (q.value(i).isNull()) {
                        row.push_back("");
                    } else {
                        row.push_back(q.value(i).toString().toStdString());
                    }
                }
                out->addRow(row);
                out->addNullFlags(nullRow);
            }
            out->setValid(true);
        }
        return true;
    }

    std::string lastError() const { return m_lastError; }
    // mysql++ uses error() to return a C-string sometimes
    const char *error() const { return m_lastError.c_str(); }

private:
    mutable std::mutex m_mutex;
    QSqlDatabase m_db;
    QString m_connName;
    std::atomic<bool> m_connected;
    std::string m_lastError;
};

// Query implementation
inline Query::Query(Connection *c) : m_conn(c) {}

inline bool Query::exec() {
    std::string sql = m_ss.str();
    m_lastError.clear();
    if (!m_conn) { m_lastError = "No connection"; return false; }
    bool ok = m_conn->execSQL(sql, m_lastError, nullptr);
    return ok;
}

inline StoreQueryResult Query::store() {
    StoreQueryResult r;
    std::string sql = m_ss.str();
    m_lastError.clear();
    if (!m_conn) { m_lastError = "No connection"; return r; }
    if (!m_conn->execSQL(sql, m_lastError, &r)) {
        r.setValid(false);
    }
    return r;
}

} // namespace mysqlpp
