#include "models/ModelManager.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "huggingface/HFAuthenticator.h"
#include "huggingface/HuggingFaceClient.h"
#include "utils/Hash.h"
#include "utils/Logger.h"

namespace studio {
namespace {

static size_t skipSpaces(const std::string& json, size_t p) {
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t' || json[p] == '\n' ||
                               json[p] == '\r')) {
        ++p;
    }
    return p;
}

// Locate `"key" :` tolerating whitespace; returns pos just past ':'.
bool findKey(const std::string& json, const std::string& key, size_t& valuePos,
             size_t start = 0) {
    const std::string pat = "\"" + key + "\"";
    size_t p = json.find(pat, start);
    while (p != std::string::npos) {
        size_t q = skipSpaces(json, p + pat.size());
        if (q < json.size() && json[q] == ':') {
            valuePos = skipSpaces(json, q + 1);
            return true;
        }
        p = json.find(pat, p + 1);
    }
    return false;
}

bool findString(const std::string& json, const std::string& key, std::string& out,
                size_t start = 0) {
    size_t v = 0;
    if (!findKey(json, key, v, start) || v >= json.size() || json[v] != '"') {
        return false;
    }
    const size_t e = json.find('"', v + 1);
    if (e == std::string::npos) {
        return false;
    }
    out = json.substr(v + 1, e - v - 1);
    return true;
}

bool findNumber(const std::string& json, const std::string& key, double& out,
                size_t start = 0) {
    size_t v = 0;
    if (!findKey(json, key, v, start)) {
        return false;
    }
    try {
        out = std::stod(json.substr(v));
        return true;
    } catch (...) {
        return false;
    }
}

std::filesystem::path appDataDir() {
#if defined(_WIN32)
    if (const char* appdata = std::getenv("LOCALAPPDATA")) {
        return std::filesystem::path(appdata) / "KimodoStudio";
    }
#endif
    return std::filesystem::path(".");
}

} // namespace

bool ModelManager::init(const std::string& registryPath, const std::string& modelDir,
                        const std::string& textBundle) {
    registryPath_ = registryPath;
    modelDir_ = modelDir;
    textBundle_ = textBundle;

    std::ifstream in(registryPath_);
    if (in) {
        const std::string json{std::istreambuf_iterator<char>(in), {}};
        // Flat object per model: {"models":[{...},{...}]}. Scan "id" occurrences.
        size_t pos = 0;
        std::string id;
        while (findString(json, "id", id, pos)) {
            ModelEntry e;
            e.id = id;
            const size_t anchor = json.find(id, pos);
            findString(json, "name", e.name, pos);
            findString(json, "skeleton", e.skeleton, pos);
            findString(json, "version", e.version, pos);
            findString(json, "license", e.license, pos);
            findString(json, "source", e.source, pos);
            findString(json, "motionFile", e.motionFile, pos);
            findString(json, "repo", e.repo, pos);
            findString(json, "remotePath", e.remotePath, pos);
            findString(json, "sha256", e.sha256, pos);
            double num = 0;
            if (findNumber(json, "sizeBytes", num, pos)) {
                e.sizeBytes = static_cast<uint64_t>(num);
            }
            entries_.push_back(std::move(e));
            pos = anchor + id.size();
        }
    }
    if (entries_.empty()) {
        Logger::instance().warning("ModelManager: registry empty or missing: " +
                                   registryPath_);
    }

    // Restore selection.
    std::ifstream sel(appDataDir() / "settings" / "active_model.txt");
    if (sel) {
        std::getline(sel, activeId_);
    }
    rescan();
    return true;
}

namespace {
const ModelEntry* findIn(const std::vector<ModelEntry>& entries, const std::string& id) {
    for (const ModelEntry& e : entries) {
        if (e.id == id) {
            return &e;
        }
    }
    return nullptr;
}
} // namespace

void ModelManager::rescan() {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::vector<std::string> dirs = {
        modelDir_,
        (appDataDir() / "models").string(),
    };
    for (ModelEntry& e : entries_) {
        e.installed = false;
        e.localPath.clear();
        e.localBytes = 0;
        if (e.motionFile.empty()) {
            continue;
        }
        for (const std::string& dir : dirs) {
            std::error_code ec;
            const auto cand = std::filesystem::path(dir) / e.motionFile;
            const auto bytes = std::filesystem::file_size(cand, ec);
            if (!ec) {
                e.installed = true;
                e.localPath = cand.string();
                e.localBytes = static_cast<uint64_t>(bytes);
                break;
            }
        }
    }
    const ModelEntry* active = findIn(entries_, activeId_);
    if (!active || !active->installed) {
        activeId_.clear();
        for (const ModelEntry& e : entries_) {
            if (e.installed) {
                activeId_ = e.id;
                break;
            }
        }
    }
}

void ModelManager::shutdown() {
    if (worker_.joinable()) {
        worker_.join();
    }
    task_.store(ModelTask::None);
}

std::vector<ModelEntry> ModelManager::entries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_;
}

std::string ModelManager::activeId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return activeId_;
}

bool ModelManager::findCopy(const std::string& id, ModelEntry& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const ModelEntry* e = findIn(entries_, id);
    if (!e) {
        return false;
    }
    out = *e;
    return true;
}

bool ModelManager::select(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const ModelEntry* e = findIn(entries_, id);
    if (!e || !e->installed) {
        return false;
    }
    activeId_ = id;
    std::error_code ec;
    std::filesystem::create_directories(appDataDir() / "settings", ec);
    std::ofstream sel(appDataDir() / "settings" / "active_model.txt", std::ios::trunc);
    if (sel) {
        sel << id;
    }
    return true;
}

bool ModelManager::busy() const {
    return task_.load() != ModelTask::None;
}

float ModelManager::taskProgress() const {
    const uint64_t total = taskTotal_.load();
    if (total == 0) {
        return 0.0f;
    }
    float p = static_cast<float>(taskDone_.load()) / static_cast<float>(total);
    return p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
}

std::string ModelManager::taskLabel() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return taskLabel_;
}

void ModelManager::cancelTask() {
    cancelRequested_.store(true);
}

void ModelManager::startTask(ModelTask t, const std::string& label) {
    if (worker_.joinable()) {
        worker_.join();
    }
    taskDone_.store(0);
    taskTotal_.store(0);
    cancelRequested_.store(false);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        taskLabel_ = label;
    }
    task_.store(t);
}

void ModelManager::verifyAsync(const std::string& id) {
    if (busy()) {
        return;
    }
    startTask(ModelTask::Verify, "Verifying " + id + "...");
    worker_ = std::thread(&ModelManager::runVerify, this, id);
}

void ModelManager::importAsync(const std::string& sourcePath, const std::string& id) {
    if (busy()) {
        return;
    }
    startTask(ModelTask::Import, "Importing " + id + "...");
    worker_ = std::thread(&ModelManager::runImport, this, sourcePath, id);
}

void ModelManager::deleteAsync(const std::string& id) {
    if (busy()) {
        return;
    }
    startTask(ModelTask::Delete, "Deleting " + id + "...");
    worker_ = std::thread(&ModelManager::runDelete, this, id);
}

void ModelManager::downloadAsync(const std::string& id) {
    if (busy()) {
        return;
    }
    startTask(ModelTask::Download, "Downloading " + id + "...");
    worker_ = std::thread(&ModelManager::runDownload, this, id);
}

void ModelManager::runDownload(std::string id) {
    std::string fail;
    ModelEntry e;
    if (!findCopy(id, e)) {
        fail = "unknown model id";
    } else if (e.repo.empty() || e.remotePath.empty()) {
        fail = "no Hugging Face source in registry";
    } else {
        // Downloads land in the user model dir, never in the repo checkout.
        const auto userDir = appDataDir() / "models";
        std::error_code ec;
        std::filesystem::create_directories(userDir, ec);
        const auto dest = userDir / e.motionFile;
        const std::string tmp = dest.string() + ".download";
        std::string token;
        HFAuthenticator::loadToken(token); // may be empty for public repos
        const std::string url = HuggingFaceClient::resolveUrl(e.repo, e.remotePath);
        Logger::instance().info("Model " + id + " download started");
        const bool ok = HuggingFaceClient::download(
            url, tmp, token, [this](uint64_t done, uint64_t total) {
                taskDone_.store(done);
                taskTotal_.store(total);
                return !cancelRequested_.load();
            }, fail);
        if (!ok) {
            if (fail.empty()) {
                fail = "download failed";
            } else if (fail == "cancelled") {
                fail = "Download cancelled (resume on next download)";
            }
        } else {
            std::string error;
            const std::string digest = FileHash::sha256(tmp, error);
            if (!e.sha256.empty() && digest != e.sha256) {
                std::filesystem::remove(tmp, ec);
                fail = "CHECKSUM MISMATCH; download discarded";
                Logger::instance().error("Model " + id + " download checksum mismatch");
            } else {
                std::filesystem::rename(tmp, dest, ec);
                if (ec) {
                    fail = "atomic install failed: " + ec.message();
                } else {
                    Logger::instance().info("Model " + id + " downloaded + verified");
                }
            }
        }
    }
    rescan();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        taskLabel_ = fail.empty() ? ("Installed " + id) : fail;
    }
    task_.store(ModelTask::None);
}

void ModelManager::runVerify(std::string id) {
    ModelEntry e;
    std::string fail = "entry missing";
    if (findCopy(id, e) && e.installed) {
        if (e.sha256.empty()) {
            fail.clear();
            std::lock_guard<std::mutex> lock(mutex_);
            taskLabel_ = "No checksum in registry; skipped";
        } else {
            std::string error;
            const std::string digest = FileHash::sha256(
                e.localPath, error, [this](uint64_t done, uint64_t total) {
                    taskDone_.store(done);
                    taskTotal_.store(total);
                });
            if (digest.empty()) {
                fail = "hash failed: " + error;
            } else if (digest != e.sha256) {
                fail = "CHECKSUM MISMATCH (file != registry)";
                Logger::instance().error("Model " + id + " checksum mismatch");
            } else {
                fail.clear();
                Logger::instance().info("Model " + id + " checksum OK");
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        taskLabel_ = fail.empty() ? ("Verified " + id + ": checksum OK") : fail;
    }
    if (worker_.joinable()) {
        // Keep thread joinable for owner shutdown(); mark done via task reset below.
    }
    task_.store(ModelTask::None);
}

void ModelManager::runImport(std::string sourcePath, std::string id) {
    std::string fail;
    ModelEntry e;
    if (!findCopy(id, e)) {
        fail = "unknown model id";
    } else {
        // Imports land in the user model dir, never in the repo checkout.
        const auto userDir = appDataDir() / "models";
        std::error_code ec;
        std::filesystem::create_directories(userDir, ec);
        const auto dest = userDir / e.motionFile;
        const auto tmp = dest.string() + ".download";
        std::ifstream src(sourcePath, std::ios::binary);
        std::ofstream dst(tmp, std::ios::binary | std::ios::trunc);
        if (!src || !dst) {
            fail = "cannot open source or destination";
        } else {
            src.seekg(0, std::ios::end);
            const auto total = src.tellg();
            src.seekg(0, std::ios::beg);
            taskTotal_.store(static_cast<uint64_t>(total));
            std::vector<char> buf(1 << 20);
            uint64_t done = 0;
            bool ok = true;
            while (src) {
                src.read(buf.data(), static_cast<std::streamsize>(buf.size()));
                const auto n = src.gcount();
                if (n > 0) {
                    dst.write(buf.data(), n);
                    done += static_cast<uint64_t>(n);
                    taskDone_.store(done);
                }
            }
            ok = static_cast<bool>(src.eof()) && static_cast<bool>(dst);
            dst.close();
            if (!ok) {
                std::filesystem::remove(tmp, ec);
                fail = "copy failed (interrupted?)";
            } else {
                // Verify before atomic install (plan section 41).
                std::string error;
                const std::string digest = FileHash::sha256(tmp, error);
                if (!e.sha256.empty() && digest != e.sha256) {
                    std::filesystem::remove(tmp, ec);
                    fail = "CHECKSUM MISMATCH; incomplete file discarded";
                } else {
                    std::filesystem::rename(tmp, dest, ec);
                    if (ec) {
                        fail = "atomic install failed: " + ec.message();
                    }
                }
            }
        }
    }
    rescan();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        taskLabel_ = fail.empty() ? ("Installed " + id) : fail;
    }
    task_.store(ModelTask::None);
}

void ModelManager::runDelete(std::string id) {
    ModelEntry e;
    std::string fail = "entry missing";
    if (findCopy(id, e) && e.installed) {
        std::error_code ec;
        // Only delete files inside known model dirs (never arbitrary paths).
        const auto p = std::filesystem::path(e.localPath);
        const auto dir = p.parent_path().string();
        const bool knownDir =
            dir == modelDir_ || dir == (appDataDir() / "models").string();
        if (!knownDir) {
            fail = "refusing to delete outside model dirs";
        } else if (!std::filesystem::remove(p, ec) || ec) {
            fail = "delete failed: " + ec.message();
        } else {
            fail.clear();
            if (activeId_ == id) {
                activeId_.clear();
            }
        }
    }
    rescan();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        taskLabel_ = fail.empty() ? ("Deleted " + id) : fail;
    }
    task_.store(ModelTask::None);
}

} // namespace studio
