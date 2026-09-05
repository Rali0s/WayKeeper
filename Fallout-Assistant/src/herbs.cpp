#include "offgrid/herbs.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

#ifdef OFFGRID_HAVE_SQLITE3
#include <sqlite3.h>
#endif

namespace offgrid {
namespace {

#ifdef OFFGRID_HAVE_SQLITE3
class Database {
public:
    explicit Database(const std::filesystem::path& path) {
        if (sqlite3_open_v2(path.string().c_str(), &handle_, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
            error_ = handle_ ? sqlite3_errmsg(handle_) : "SQLite could not allocate a database handle.";
        }
    }
    ~Database() { if (handle_) sqlite3_close(handle_); }
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    sqlite3* get() const { return error_.empty() ? handle_ : nullptr; }
    const std::string& error() const { return error_; }

private:
    sqlite3* handle_{};
    std::string error_;
};

class Statement {
public:
    Statement(sqlite3* database, const char* sql) {
        if (sqlite3_prepare_v2(database, sql, -1, &handle_, nullptr) != SQLITE_OK) {
            error_ = sqlite3_errmsg(database);
        }
    }
    ~Statement() { if (handle_) sqlite3_finalize(handle_); }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    sqlite3_stmt* get() const { return error_.empty() ? handle_ : nullptr; }
    const std::string& error() const { return error_; }

private:
    sqlite3_stmt* handle_{};
    std::string error_;
};

std::string column_text(sqlite3_stmt* statement, const int column) {
    const auto* text = sqlite3_column_text(statement, column);
    return text ? reinterpret_cast<const char*>(text) : std::string{};
}

std::string safe_fts_query(const std::string& query) {
    std::string result;
    std::string token;
    const auto flush = [&]() {
        if (token.empty()) return;
        if (!result.empty()) result += " AND ";
        result += token;
        token.clear();
    };
    for (const unsigned char character : query) {
        if (std::isalnum(character)) token += static_cast<char>(std::tolower(character));
        else flush();
    }
    flush();
    return result;
}
#endif

}  // namespace

bool herb_database_support_available() {
#ifdef OFFGRID_HAVE_SQLITE3
    return true;
#else
    return false;
#endif
}

std::filesystem::path herb_database_path(const std::filesystem::path& resource_root) {
    return resource_root / "library" / "plants-herbs" / "plants-herbs.sqlite3";
}

bool inspect_herb_database(
    const std::filesystem::path& database, HerbDatabaseStats& stats, std::string& error) {
#ifndef OFFGRID_HAVE_SQLITE3
    (void)database;
    (void)stats;
    error = "Plant database support was built without SQLite3.";
    return false;
#else
    if (!std::filesystem::is_regular_file(database)) {
        error = "Plant database is missing: " + database.string();
        return false;
    }
    Database connection(database);
    if (!connection.get()) {
        error = "Could not open plant database: " + connection.error();
        return false;
    }
    Statement statement(connection.get(),
        "SELECT (SELECT COUNT(*) FROM source_document),"
        "       (SELECT COALESCE(SUM(page_count), 0) FROM source_document),"
        "       (SELECT COUNT(*) FROM plant),"
        "       (SELECT COUNT(*) FROM evidence_statement)");
    if (!statement.get() || sqlite3_step(statement.get()) != SQLITE_ROW) {
        error = "Could not read plant database statistics: " +
            (statement.error().empty() ? std::string(sqlite3_errmsg(connection.get())) : statement.error());
        return false;
    }
    stats.documents = static_cast<std::size_t>(sqlite3_column_int64(statement.get(), 0));
    stats.pages = static_cast<std::size_t>(sqlite3_column_int64(statement.get(), 1));
    stats.structured_plants = static_cast<std::size_t>(sqlite3_column_int64(statement.get(), 2));
    stats.reviewed_statements = static_cast<std::size_t>(sqlite3_column_int64(statement.get(), 3));
    return true;
#endif
}

std::vector<HerbSearchHit> search_herb_database(
    const std::filesystem::path& database,
    const std::filesystem::path& resource_root,
    const std::string& query,
    const std::size_t limit,
    std::string& error) {
    std::vector<HerbSearchHit> hits;
#ifndef OFFGRID_HAVE_SQLITE3
    (void)database; (void)resource_root; (void)query; (void)limit;
    error = "Plant database support was built without SQLite3.";
#else
    const auto fts_query = safe_fts_query(query);
    if (fts_query.empty()) {
        error = "Enter at least one letter or number to search.";
        return hits;
    }
    Database connection(database);
    if (!connection.get()) {
        error = "Could not open plant database: " + connection.error();
        return hits;
    }
    Statement statement(connection.get(),
        "SELECT f.document_id, d.title, f.page_number, d.page_count,"
        "       snippet(document_page_fts, 2, '[', ']', ' ... ', 22), d.local_pdf "
        "FROM document_page_fts AS f "
        "JOIN source_document AS d ON d.id = f.document_id "
        "WHERE document_page_fts MATCH ?1 "
        "ORDER BY bm25(document_page_fts) LIMIT ?2");
    if (!statement.get()) {
        error = "Could not prepare plant search: " + statement.error();
        return hits;
    }
    sqlite3_bind_text(statement.get(), 1, fts_query.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement.get(), 2, static_cast<sqlite3_int64>(limit));
    int status = SQLITE_ROW;
    while ((status = sqlite3_step(statement.get())) == SQLITE_ROW) {
        HerbSearchHit hit;
        hit.document_id = column_text(statement.get(), 0);
        hit.title = column_text(statement.get(), 1);
        hit.page = static_cast<std::size_t>(sqlite3_column_int64(statement.get(), 2));
        hit.page_count = static_cast<std::size_t>(sqlite3_column_int64(statement.get(), 3));
        hit.excerpt = column_text(statement.get(), 4);
        hit.pdf_path = resource_root / column_text(statement.get(), 5);
        hits.push_back(std::move(hit));
    }
    if (status != SQLITE_DONE) error = "Plant search failed: " + std::string(sqlite3_errmsg(connection.get()));
#endif
    return hits;
}

std::optional<HerbPage> read_herb_page(
    const std::filesystem::path& database,
    const std::filesystem::path& resource_root,
    const std::string& document_id,
    const std::size_t page,
    std::string& error) {
#ifndef OFFGRID_HAVE_SQLITE3
    (void)database; (void)resource_root; (void)document_id; (void)page;
    error = "Plant database support was built without SQLite3.";
    return std::nullopt;
#else
    Database connection(database);
    if (!connection.get()) {
        error = "Could not open plant database: " + connection.error();
        return std::nullopt;
    }
    Statement statement(connection.get(),
        "SELECT d.title, d.page_count, d.local_pdf, p.page_text "
        "FROM document_page AS p JOIN source_document AS d ON d.id = p.document_id "
        "WHERE p.document_id = ?1 AND p.page_number = ?2");
    if (!statement.get()) {
        error = "Could not prepare plant page reader: " + statement.error();
        return std::nullopt;
    }
    sqlite3_bind_text(statement.get(), 1, document_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement.get(), 2, static_cast<sqlite3_int64>(page));
    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
        error = "Plant source page was not found.";
        return std::nullopt;
    }
    HerbPage result;
    result.document_id = document_id;
    result.title = column_text(statement.get(), 0);
    result.page_count = static_cast<std::size_t>(sqlite3_column_int64(statement.get(), 1));
    result.pdf_path = resource_root / column_text(statement.get(), 2);
    result.text = column_text(statement.get(), 3);
    result.page = page;
    return result;
#endif
}

}  // namespace offgrid
