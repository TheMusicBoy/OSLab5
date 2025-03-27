#include <assets.h>

#include <common/exception.h>
#include <common/logging.h>

#include <jinja2cpp/template.h>

#include <fstream>
#include <sstream>
#include <filesystem>

namespace NAssets {

namespace {

////////////////////////////////////////////////////////////////////////////////

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    ASSERT(file.is_open(), "Failed to open file: {}", path.string());
    
    const auto file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::string content;
    content.resize(static_cast<size_t>(file_size));
    
    ASSERT(file.read(&content[0], file_size), "Error reading contents from: {}", path.string());
    return content;
}

////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////

bool TAsset::Format(const jinja2::ValuesMap& values) {
    jinja2::Template tmpl;
    tmpl.Load(Data);
    
    if (auto result = tmpl.RenderAsString(values)) {
        Data = result.value();
        return true;
    } else {
        return false;
    }
}

TAssetsManager::TAssetsManager(std::filesystem::path assetsRoot)
    : AssetsRoot_(std::move(assetsRoot))
{
    // Initialize common MIME types
    MimeTypes_ = {
        {".html", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".svg", "image/svg+xml"}
    };
}

void TAssetsManager::PreloadAssets() {
    std::lock_guard<std::mutex> lock(CacheMutex_);
    for (const auto& entry : std::filesystem::recursive_directory_iterator(AssetsRoot_)) {
        if (entry.is_regular_file()) {
            const auto path = entry.path().lexically_relative(AssetsRoot_).string();
            
            std::ifstream file(entry.path(), std::ios::binary);
            std::stringstream buffer;
            buffer << file.rdbuf();
            
            ContentCache_[path] = buffer.str();
        }
    }
}

TAsset TAssetsManager::LoadAsset(const std::filesystem::path& path) const {
    std::lock_guard<std::mutex> lock(CacheMutex_);
    
    if (path.string().find("..") != std::string::npos) {
        throw TForbidden();
    }

    auto it = ContentCache_.find(path);
    if (it == ContentCache_.end()) {
        throw TNotFound();
    }

    const auto extension = std::filesystem::path(path).extension().string();
    return TAsset{it->second, GetMimeType(extension)};
}

NRpc::TResponse TAssetsManager::HandleRequest(const std::filesystem::path& path) const {
    try {
        TAsset asset = LoadAsset(path);
        return NRpc::TResponse()
            .SetStatus(NRpc::EHttpCode::Ok)
            .SetAsset(asset);
    } catch (TForbidden) {
        return NRpc::TResponse().SetStatus(NRpc::EHttpCode::Forbidden);
    } catch (TNotFound) {
        return NRpc::TResponse().SetStatus(NRpc::EHttpCode::NotFound);
    } catch (std::exception&) {
        return NRpc::TResponse().SetStatus(NRpc::EHttpCode::InternalError);
    }
}

std::string TAssetsManager::GetMimeType(const std::string& extension) const {
    auto it = MimeTypes_.find(extension);
    return it != MimeTypes_.end() ? it->second : "application/octet-stream";
}

} // namespace NAssets
