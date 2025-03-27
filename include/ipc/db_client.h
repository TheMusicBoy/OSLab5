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

    TTransaction BeginTransaction();

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
            pqxx::work txn(*Conn_);
            auto res = txn.exec(query, params);
            txn.commit();
            return res;
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

private:
    std::unique_ptr<pqxx::connection> Conn_;
    TDataBaseConfigPtr Config_;
};

DECLARE_REFCOUNTED(TDbClient);

////////////////////////////////////////////////////////////////////////////////

class TTransaction {
public:
    using TParamMap = std::unordered_map<std::string, std::string>;
    using TQueryParams = std::vector<std::string>;

    TTransaction(pqxx::connection* connection);

    void Commit();
    void Rollback();

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
            return Txn_->exec(query, params);
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

private:
    std::unique_ptr<pqxx::transaction<>> Txn_;
    bool Committed_ = false;
    bool Rollbacked_ = false;
};


////////////////////////////////////////////////////////////////////////////////

} // namespace NIpc
