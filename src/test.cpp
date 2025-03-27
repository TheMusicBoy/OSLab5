#include <ipc/db_client.h>
#include <common/logging.h>
#include <common/getopts.h>

#include <chrono>

class TTestDB {
private:
    NIpc::TDbClientPtr DbClient_;
    const std::string TableName_ = "temperatures";
    
public:
    TTestDB(NIpc::TDataBaseConfigPtr dbConfig)
        : DbClient_(NCommon::New<NIpc::TDbClient>(dbConfig))
    {
        DbClient_->Connect();
        ClearTable();
        CreateTable();
    }

    void ClearTable() {
        try {
            DbClient_->ExecuteQuery("DELETE FROM " + TableName_);
        } catch (const std::exception& ex) {
            LOG_INFO("Table clear wasn't needed: {}", ex.what());
        }
    }

    void CreateTable() {
        try {
            DbClient_->ExecuteQuery(
                "CREATE TABLE IF NOT EXISTS " + TableName_ + " ("
                "    id SERIAL PRIMARY KEY,"
                "    timestamp TIMESTAMPTZ NOT NULL,"
                "    temperature DOUBLE PRECISION NOT NULL"
                ")"
            );
            LOG_INFO("Checked/Created temperatures table");
        } catch(const std::exception& ex) {
            LOG_ERROR("CreateTable failed: {}", ex.what());
            throw;
        }
    }

    void TestInsertSelect() {
        try {
            // Insert test data
            NIpc::TDbClient::TParamMap values{
                {"timestamp", "NOW()"},
                {"temperature", "21.5"}
            };
            
            DbClient_->InsertRow(TableName_, values);
            
            // Test selection and validate
            auto result = DbClient_->SelectRows(TableName_, "temperature > 20");
            LOG_INFO("Got {} rows from test table", result.size());
            ASSERT(!result.empty(), "No rows returned from test table");
            
            // Delete test data
            DbClient_->DeleteRow(TableName_, "temperature = 21.5");
            
            // Verify deletion
            auto resultAfterDelete = DbClient_->SelectRows(TableName_, "temperature = 21.5");
            ASSERT(resultAfterDelete.empty(), "Data wasn't deleted");
            
        } catch(const std::exception& ex) {
            LOG_ERROR("Insert/Delete test failed: {}", ex.what());
            throw;
        }
    }

    void TestTransaction() {
        try {
            auto tx = DbClient_->BeginTransaction();
            
            // Insert within transaction
            NIpc::TDbClient::TParamMap values{
                {"timestamp", "NOW()"},
                {"temperature", "19.5"}
            };
            tx.InsertRow(TableName_, values);
            tx.Commit();
            
            LOG_INFO("Transaction committed successfully");
        } catch(const std::exception& ex) {
            LOG_ERROR("Transaction test failed: {}", ex.what());
            throw;
        }
    }

    void TestSelectAfterTransaction() {
        try {
            auto result = DbClient_->SelectRows(TableName_, "temperature = 19.5");
            
            LOG_INFO("After transaction: got {} matching rows", result.size());
            ASSERT(result.size() == 1, 
                   "Should find exactly 1 row after transaction commit");

            // After validation
            DbClient_->DeleteRow(TableName_, "temperature = 19.5");
            
            // Verify cleanup
            auto postDeleteResult = DbClient_->SelectRows(TableName_, "temperature = 19.5");
            ASSERT(postDeleteResult.empty(), "Transaction cleanup failed");
            
        } catch(const std::exception& ex) {
            LOG_ERROR("Post-transaction test failed: {}", ex.what());
            throw;
        }
    }
};

int main(int argc, const char* argv[]) {
    NCommon::GetOpts opts;
    opts.AddOption('h', "help", "Show help message");
    opts.AddOption('c', "config", "Path to config file", true);

    try {
        opts.Parse(argc, argv);
        
        if (opts.Has('h')) {
            std::cerr << opts.Help();
            return 0;
        }

        ASSERT(opts.Has('c'), "Config file is required");
        
        auto config = NCommon::New<NIpc::TDataBaseConfig>();
        config->LoadFromFile(opts.Get("config"));

        TTestDB tester(config);
        tester.TestInsertSelect();
        tester.TestTransaction();
        tester.TestSelectAfterTransaction();
        
        LOG_INFO("All database tests completed successfully");
    } catch(std::exception& ex) {
        LOG_ERROR("Got an error: {}", ex);
        return 1;
    }
    
    return 0;
}
