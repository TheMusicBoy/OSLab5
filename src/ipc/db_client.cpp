#include <ipc/db_client.h>

namespace NIpc {

namespace {

////////////////////////////////////////////////////////////////////////////////

std::string CreatePlaceHolders(size_t size) {
    std::ostringstream ss;
    for (size_t i = 1; i <= size; i++) {
        ss << (i == 1 ? "$" : ",$") << i;
    }
    return ss.str();
}

template <typename Container>
std::pair<std::vector<std::string>, std::vector<std::string>> SplitMap(const Container& data) {
    std::pair<std::vector<std::string>, std::vector<std::string>> result;
    result.first.reserve(data.size());
    result.second.reserve(data.size());

    for (const auto& [x, y] : data) {
        result.first.emplace_back(x);
        result.second.emplace_back(y);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////

void TDataBaseConfig::Load(const nlohmann::json& data) {
    HostAddr = TConfigBase::LoadRequired<std::string>(data, "host_address");
    Port = TConfigBase::Load<uint32_t>(data, "port", 5432);
    RequireSsl = TConfigBase::Load<bool>(data, "require_ssl", false);

    DbName = TConfigBase::LoadRequired<std::string>(data, "db_name");

    UserName = TConfigBase::LoadRequired<std::string>(data, "user_name");
    Password = TConfigBase::Load<std::string>(data, "password", "");
    if (Password.empty()) {
        auto env = TConfigBase::Load<std::string>(data, "password_env", "");
        if (!env.empty()) {
            Password = getenv(env.c_str());
        }
    }
    ASSERT(!Password.empty(), "No password provided for user '{}'", UserName);
}

////////////////////////////////////////////////////////////////////////////////

TTransaction::TTransaction(pqxx::connection* connection) {
    ASSERT(connection, "Database not connected");
    Txn_ = std::make_unique<pqxx::transaction<>>(*connection);
}

void TTransaction::Commit() {
    if (!Txn_) {
        THROW("No active transaction to commit");
    }
    Txn_->commit();
    Committed_ = true;
    Txn_.reset();
}

void TTransaction::Rollback() {
    if (!Txn_) {
        THROW("No active transaction to rollback");
    }
    Txn_->abort();
    Rollbacked_ = true;
    Txn_.reset();
}

void TTransaction::InsertRow(const std::string& table, const TParamMap& columns) {
    auto splited = SplitMap(columns);

    ExecuteQueryR(
        NCommon::Format(
            "INSERT INTO {} ({}) VALUES ({})",
            table, NCommon::Join(splited.first, ", "), CreatePlaceHolders(columns.size())),
        splited.second
    );
}

void TTransaction::DeleteRow(const std::string& table, const std::string& conditions) {
    std::string query = "DELETE FROM " + table;
    if (!conditions.empty()) {
        query += " WHERE " + conditions;
    }
    ExecuteQuery(query);
}

pqxx::result TTransaction::SelectRows(const std::string& table,
                                  const std::string& conditions,
                                  const TQueryParams& orderBy,
                                  int limit) 
{
    std::string query = "SELECT * FROM " + table;
    
    if (!conditions.empty()) {
        query += " WHERE " + conditions;
    }
    
    if (!orderBy.empty()) {
        query += " ORDER BY ";
        for (const auto& field : orderBy) {
            query += (field == orderBy.front() ? "" : ", ") + field;
        }
    }
    
    if (limit > 0) {
        query += " LIMIT " + std::to_string(limit);
    }

    return ExecuteQuery(query);
}

////////////////////////////////////////////////////////////////////////////////

TDbClient::TDbClient(TDataBaseConfigPtr config)
    : Config_(config) {}

void TDbClient::Connect() {
    try {
        Conn_ = std::make_unique<pqxx::connection>(
            NCommon::Format("hostaddr={} port={} dbname={} user={} password={} requiressl={}",
                Config_->HostAddr, Config_->Port, Config_->DbName, Config_->UserName, Config_->Password, Config_->RequireSsl));
        LOG_INFO("Connected to PostgreSQL database: {}", Config_->DbName);
    } catch (const std::exception& ex) {
        RETHROW(ex, "Database connection failed");
    }
}

TTransaction TDbClient::BeginTransaction() {
    return TTransaction(Conn_.get());
}

void TDbClient::InsertRow(const std::string& table, const TParamMap& columns) {
    auto splited = SplitMap(columns);

    ExecuteQueryR(
        NCommon::Format(
            "INSERT INTO {} ({}) VALUES ({})",
            table, NCommon::Join(splited.first, ", "), CreatePlaceHolders(columns.size())),
        splited.second
    );
}

void TDbClient::DeleteRow(const std::string& table, const std::string& conditions) {
    std::string query = "DELETE FROM " + table;
    if (!conditions.empty()) {
        query += " WHERE " + conditions;
    }
    ExecuteQuery(query);
}

pqxx::result TDbClient::SelectRows(const std::string& table,
                                  const std::string& conditions,
                                  const TQueryParams& orderBy,
                                  int limit) 
{
    std::string query = "SELECT * FROM " + table;
    
    if (!conditions.empty()) {
        query += " WHERE " + conditions;
    }
    
    if (!orderBy.empty()) {
        query += " ORDER BY ";
        for (const auto& field : orderBy) {
            query += (field == orderBy.front() ? "" : ", ") + field;
        }
    }
    
    if (limit > 0) {
        query += " LIMIT " + std::to_string(limit);
    }

    return ExecuteQuery(query);
}


////////////////////////////////////////////////////////////////////////////////

} // namespace NIpc
