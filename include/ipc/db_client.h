#pragma once

#include <common/intrusive_ptr.h>
#include <common/logging.h>
#include <common/exception.h>
#include <common/config.h>

#include <pqxx/pqxx>
#include <string>
#include <vector>
#include <unordered_map>

namespace NIpc {

////////////////////////////////////////////////////////////////////////////////

struct TDataBaseConfig
    : public NCommon::TConfigBase
{
    std::string HostAddr;
    uint32_t Port;
    bool RequireSsl;

    std::string DbName;

    std::string UserName;
    std::string Password;

    void Load(const nlohmann::json& data) override;
};

DECLARE_REFCOUNTED(TDataBaseConfig);

////////////////////////////////////////////////////////////////////////////////

class TTransaction;

class TDbClient : public NRefCounted::TRefCountedBase {
public:

    using TParamMap = std::unordered_map<std::string, std::string>;
    using TQueryParams = std::vector<std::string>;

    TDbClient(TDataBaseConfigPtr config);
    
    void Connect();

    template <typename Container>
    requires (std::is_same_v<typename Container::value_type, std::string>)
    inline pqxx::result ExecuteQueryR(const std::string& query, const Container& params) {
        pqxx::params queryParams;
        for (auto&& param : params) {
            queryParams.append(std::forward<decltype(param)>(param));
        }
        return ExecuteQuery(query, std::move(queryParams));
    }

    template <typename... Args>
    inline pqxx::result ExecuteQuery(const std::string& query, Args&&... args) {
        return ExecuteQuery(query, pqxx::params(std::forward<Args>(args)...));
    }

    inline pqxx::result ExecuteQuery(const std::string& query, pqxx::params&& params) {
        try {
            auto guard = std::lock_guard(Mutex_);
            if (Txn_) {
                return Txn_->exec(query, params);
            } else {
                pqxx::work txn(*Conn_);
                auto res = txn.exec(query, params);
                txn.commit();
                return res;
            }
        } catch (const std::exception& ex) {
            LOG_ERROR("Parameterized query failed: {}", ex.what());
            throw;
        }
    }

    void InsertRow(const std::string& table, const TParamMap& columns);
    void DeleteRow(const std::string& table, const std::string& conditions = "");

    pqxx::result SelectRows(const std::string& table, 
                           const std::string& conditions = "",
                           const TQueryParams& orderBy = {},
                           int limit = -1);

    TTransaction BeginTransaction();
    TTransaction BeginTransactionWithTimeout(std::chrono::milliseconds timeout);

    void Commit();
    void Rollback();

private:
    std::unique_ptr<pqxx::connection> Conn_;
    std::shared_ptr<pqxx::transaction<>> Txn_;
    std::mutex Mutex_;
    TDataBaseConfigPtr Config_;

    friend TTransaction;

    inline static const std::string LoggingSource = "Client";
};

DECLARE_REFCOUNTED(TDbClient);

////////////////////////////////////////////////////////////////////////////////

class TTransaction {
public:
    using TParamMap = std::unordered_map<std::string, std::string>;
    using TQueryParams = std::vector<std::string>;

    TTransaction(TDbClientPtr client, std::shared_ptr<pqxx::transaction<>> Txn);
    ~TTransaction();

    void Commit();
    void Rollback();

private:
    TDbClientPtr Client_;
    std::shared_ptr<pqxx::transaction<>> Txn_;

    inline static const std::string LoggingSource = "Client";
};


////////////////////////////////////////////////////////////////////////////////

} // namespace NIpc
