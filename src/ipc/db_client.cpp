#include <ipc/db_client.h>

#include <thread>

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

TTransaction TDbClient::BeginTransaction() {
    std::lock_guard<std::mutex> lock(Mutex_);
    if (Txn_) {
        THROW("Another transaction is already active");
    }
    
    try {
        Txn_ = std::make_shared<pqxx::transaction<>>(*Conn_);
        LOG_DEBUG("Transaction started");
        return TTransaction(NCommon::TIntrusivePtr<TDbClient>(this), Txn_);
    } catch (const std::exception& ex) {
        RETHROW(ex, "Failed to begin transaction");
    }
}

TTransaction TDbClient::BeginTransactionWithTimeout(std::chrono::milliseconds timeout) {
    auto endTime = std::chrono::steady_clock::now() + timeout;
    
    while (true) {
        {
            std::unique_lock<std::mutex> lock(Mutex_);
            if (!Txn_) {
                try {
                    Txn_ = std::make_shared<pqxx::transaction<>>(*Conn_);
                    LOG_DEBUG("Transaction started after waiting");
                    return TTransaction(NCommon::TIntrusivePtr<TDbClient>(this), Txn_);
                } catch (const std::exception& ex) {
                    RETHROW(ex, "Failed to begin transaction");
                }
            }
            
            if (std::chrono::steady_clock::now() >= endTime) {
                THROW("Timed out waiting for previous transaction to complete");
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void TDbClient::Commit() {
    std::lock_guard<std::mutex> lock(Mutex_);
    if (!Txn_) {
        THROW("No active transaction to commit");
    }
    
    try {
        Txn_->commit();
        LOG_DEBUG("Transaction committed");
        Txn_.reset();
    } catch (const std::exception& ex) {
        RETHROW(ex, "Failed to commit transaction");
    }
}

void TDbClient::Rollback() {
    std::lock_guard<std::mutex> lock(Mutex_);
    if (!Txn_) {
        THROW("No active transaction to rollback");
    }
    
    try {
        Txn_->abort();
        LOG_DEBUG("Transaction rolled back");
        Txn_.reset();
    } catch (const std::exception& ex) {
        RETHROW(ex, "Failed to rollback transaction");
    }
}

////////////////////////////////////////////////////////////////////////////////

TTransaction::TTransaction(TDbClientPtr client, std::shared_ptr<pqxx::transaction<>> Txn)
    : Client_(std::move(client)),
      Txn_(Txn)
{}

TTransaction::~TTransaction() {
    try {
        if (Client_ && Client_->Txn_ && Client_->Txn_ == Txn_) {
            LOG_WARNING("Transaction was not explicitly committed or rolled back, rolling back");
            Client_->Rollback();
        }
    } catch (const std::exception& ex) {
        LOG_ERROR("Failed to rollback transaction in destructor: {}", ex.what());
    }
}

void TTransaction::Commit() {
    Client_->Commit();
}

void TTransaction::Rollback() {
    Client_->Rollback();
}


////////////////////////////////////////////////////////////////////////////////

} // namespace NIpc
