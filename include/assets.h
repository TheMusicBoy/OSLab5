#pragma once
#include <rpc/http_server.h>

#include <common/exception.h>
#include <common/intrusive_ptr.h>

#include <jinja2cpp/value.h>

#include <filesystem>
#include <unordered_map>
#include <mutex>
#include <string>

namespace NAssets {

////////////////////////////////////////////////////////////////////////////////

class TForbidden : public std::exception {};

class TNotFound : public std::exception {};

struct TAsset {
    std::string Data;
    std::string Mime;

    bool Format(const jinja2::ValuesMap& values);
};

////////////////////////////////////////////////////////////////////////////////

class TAssetsManager
    : public NRefCounted::TRefCountedBase {
public:
    explicit TAssetsManager(std::filesystem::path assetsRoot);
    
    void PreloadAssets();

    TAsset LoadAsset(const std::filesystem::path& path) const;
    NRpc::TResponse HandleRequest(const std::filesystem::path& path) const;

private:
    std::string GetMimeType(const std::string& extension) const;
    
    mutable std::mutex CacheMutex_;
    std::unordered_map<std::string, std::string> ContentCache_;
    std::unordered_map<std::string, std::string> MimeTypes_;
    const std::filesystem::path AssetsRoot_;
};

DECLARE_REFCOUNTED(TAssetsManager);

////////////////////////////////////////////////////////////////////////////////

} // namespace NAssets
