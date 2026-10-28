#include "static_files.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>

namespace {

std::optional<std::filesystem::path> TargetToPath(const std::string& document_root,
                                                  const std::string& raw_target) {
    const std::size_t query_start = raw_target.find_first_of("?#");
    const std::string target = raw_target.substr(0, query_start);
    if (target.empty() || target.front() != '/') return std::nullopt;

    std::filesystem::path path(document_root);
    std::size_t start = 1;
    while (start < target.size()) {
        const std::size_t end = target.find('/', start);
        const std::string_view component(target.data() + start,
                                         (end == std::string::npos ? target.size() : end) - start);
        if (component == ".." || component.find('\\') != std::string_view::npos) return std::nullopt;
        if (!component.empty() && component != ".") path /= component;
        start = end == std::string::npos ? target.size() : end + 1;
    }
    if (target == "/") path /= "index.html";
    return path;
}

std::string ContentTypeForPath(const std::filesystem::path& path) {
    const std::string extension = path.extension().string();
    if (extension == ".html" || extension == ".htm") return "text/html; charset=utf-8";
    if (extension == ".css") return "text/css; charset=utf-8";
    if (extension == ".txt") return "text/plain; charset=utf-8";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".png") return "image/png";
    return "application/octet-stream";
}

bool IsInsideDocumentRoot(const std::filesystem::path& document_root, const std::filesystem::path& path) {
    const std::filesystem::path canonical_root = std::filesystem::weakly_canonical(document_root);
    const std::filesystem::path canonical_path = std::filesystem::weakly_canonical(path);
    auto root_part = canonical_root.begin();
    auto path_part = canonical_path.begin();
    while (root_part != canonical_root.end()) {
        if (path_part == canonical_path.end() || *root_part != *path_part) return false;
        ++root_part;
        ++path_part;
    }
    return true;
}

}  // namespace

std::optional<StaticFile> ReadStaticFile(const std::string& document_root, const std::string& target) {
    const std::optional<std::filesystem::path> path = TargetToPath(document_root, target);
    if (!path || !IsInsideDocumentRoot(document_root, *path) || !std::filesystem::is_regular_file(*path)) {
        return std::nullopt;
    }

    std::ifstream file(*path, std::ios::binary);
    if (!file) return std::nullopt;
    std::ostringstream contents;
    contents << file.rdbuf();
    if (!file.good() && !file.eof()) return std::nullopt;
    return StaticFile{ContentTypeForPath(*path), contents.str()};
}
