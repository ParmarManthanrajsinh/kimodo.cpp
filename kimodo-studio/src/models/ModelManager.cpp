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

bool FModelManager::Init(const std::string& inRegistryPath, const std::string& inModelDir,
                         const std::string& inTextBundle) {
    registryPath = inRegistryPath;
    modelDir = inModelDir;
    textBundle = inTextBundle;

    std::ifstream in(registryPath);
    if (in) {
        const std::string json{std::istreambuf_iterator<char>(in), {}};
        // Flat object per model: {"models":[{...},{...}]}. Scan "id" occurrences.
        size_t pos = 0;
        std::string id;
        while (findString(json, "id", id, pos)) {
            FModelEntry e;
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
            entries.push_back(std::move(e));
            pos = anchor + id.size();
        }
    }
    if (entries.empty()) {
        FLogger::GetInstance().warning("ModelManager: registry empty or missing: " +
                                   registryPath);
    }

    // Restore selection.
    std::ifstream sel(appDataDir() / "settings" / "active_model.txt");
    if (sel) {
        std::getline(sel, ActiveId);
    }
    Rescan();
    return true;
}

namespace {
const FModelEntry* findIn(const std::vector<FModelEntry>& entries, const std::string& id) {
    for (const FModelEntry& e : entries) {
        if (e.id == id) {
            return &e;
        }
    }
    return nullptr;
}
} // namespace

void FModelManager::Rescan() {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<std::string> dirs = {
        modelDir,
        (appDataDir() / "models").string(),
        (std::filesystem::current_path() / "models").string(),
    };

#if defined(KIMODO_ROOT_DIR)
    dirs.push_back((std::filesystem::path(KIMODO_ROOT_DIR) / "models").string());
#endif
#if defined(KIMODO_STUDIO_SOURCE_DIR)
    dirs.push_back((std::filesystem::path(KIMODO_STUDIO_SOURCE_DIR) / "../models").string());
    dirs.push_back((std::filesystem::path(KIMODO_STUDIO_SOURCE_DIR) / "models").string());
#endif

    // Walk up searching for models/
    std::filesystem::path cur = std::filesystem::current_path();
    for (int i = 0; i < 5; ++i) {
        if (!cur.empty()) {
            dirs.push_back((cur / "models").string());
            cur = cur.parent_path();
        }
    }

    for (FModelEntry& e : entries) {
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
            if (!ec && bytes > 0) {
                e.installed = true;
                e.localPath = cand.string();
                e.localBytes = static_cast<uint64_t>(bytes);
                break;
            }
        }
    }
    const FModelEntry* active = findIn(entries, ActiveId);
    if (!active || !active->installed) {
        ActiveId.clear();
        for (const FModelEntry& e : entries) {
            if (e.installed) {
                ActiveId = e.id;
                break;
            }
        }
    }
}

void FModelManager::Shutdown() {
    if (worker.joinable()) {
        worker.join();
    }
    task.store(EModelTask::None);
}

std::vector<FModelEntry> FModelManager::GetEntries() const {
    std::lock_guard<std::mutex> lock(mutex);
    return entries;
}

std::string FModelManager::GetActiveId() const {
    std::lock_guard<std::mutex> lock(mutex);
    return ActiveId;
}

bool FModelManager::findCopy(const std::string& id, FModelEntry& out) const {
    std::lock_guard<std::mutex> lock(mutex);
    const FModelEntry* e = findIn(entries, id);
    if (!e) {
        return false;
    }
    out = *e;
    return true;
}

bool FModelManager::select(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex);
    const FModelEntry* e = findIn(entries, id);
    if (!e || !e->installed) {
        return false;
    }
    ActiveId = id;
    std::error_code ec;
    std::filesystem::create_directories(appDataDir() / "settings", ec);
    std::ofstream sel(appDataDir() / "settings" / "active_model.txt", std::ios::trunc);
    if (sel) {
        sel << id;
    }
    return true;
}

bool FModelManager::IsBusy() const {
    return task.load() != EModelTask::None;
}

float FModelManager::GetTaskProgress() const {
    const uint64_t total = taskTotal.load();
    if (total == 0) {
        return 0.0f;
    }
    float p = static_cast<float>(taskDone.load()) / static_cast<float>(total);
    return p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
}

std::string FModelManager::GetTaskLabel() const {
    std::lock_guard<std::mutex> lock(mutex);
    return TaskLabel;
}

void FModelManager::cancelTask() {
    bCancelRequested.store(true);
}

void FModelManager::startTask(EModelTask t, const std::string& label) {
    if (worker.joinable()) {
        worker.join();
    }
    taskDone.store(0);
    taskTotal.store(0);
    bCancelRequested.store(false);
    {
        std::lock_guard<std::mutex> lock(mutex);
        TaskLabel = label;
    }
    task.store(t);
}

void FModelManager::verifyAsync(const std::string& id) {
    if (IsBusy()) {
        return;
    }
    startTask(EModelTask::Verify, "Verifying " + id + "...");
    worker = std::thread(&FModelManager::runVerify, this, id);
}

void FModelManager::importAsync(const std::string& sourcePath, const std::string& id) {
    if (IsBusy()) {
        return;
    }
    startTask(EModelTask::Import, "Importing " + id + "...");
    worker = std::thread(&FModelManager::runImport, this, sourcePath, id);
}

void FModelManager::deleteAsync(const std::string& id) {
    if (IsBusy()) {
        return;
    }
    startTask(EModelTask::Delete, "Deleting " + id + "...");
    worker = std::thread(&FModelManager::runDelete, this, id);
}

void FModelManager::downloadAsync(const std::string& id) {
    if (IsBusy()) {
        return;
    }
    startTask(EModelTask::Download, "Downloading " + id + "...");
    worker = std::thread(&FModelManager::runDownload, this, id);
}

void FModelManager::runDownload(std::string id) {
    std::string fail;
    FModelEntry e;
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
        FHFAuthenticator::loadToken(token); // may be empty for public repos
        const std::string url = FHuggingFaceClient::resolveUrl(e.repo, e.remotePath);
        FLogger::GetInstance().info("Model " + id + " download started");
        const bool ok = FHuggingFaceClient::download(
            url, tmp, token, [this](uint64_t done, uint64_t total) {
                taskDone.store(done);
                taskTotal.store(total);
                return !bCancelRequested.load();
            }, fail);
        if (!ok) {
            if (fail.empty()) {
                fail = "download failed";
            } else if (fail == "cancelled") {
                fail = "Download cancelled (resume on next download)";
            }
        } else {
            std::string error;
            const std::string digest = FFileHash::sha256(tmp, error);
            if (!e.sha256.empty() && digest != e.sha256) {
                std::filesystem::remove(tmp, ec);
                fail = "CHECKSUM MISMATCH; download discarded";
                FLogger::GetInstance().error("Model " + id + " download checksum mismatch");
            } else {
                std::filesystem::rename(tmp, dest, ec);
                if (ec) {
                    fail = "atomic install failed: " + ec.message();
                } else {
                    FLogger::GetInstance().info("Model " + id + " downloaded + verified");
                }
            }
        }
    }
    Rescan();
    {
        std::lock_guard<std::mutex> lock(mutex);
        TaskLabel = fail.empty() ? ("Installed " + id) : fail;
    }
    task.store(EModelTask::None);
}

void FModelManager::runVerify(std::string id) {
    FModelEntry e;
    std::string fail = "entry missing";
    if (findCopy(id, e) && e.installed) {
        if (e.sha256.empty()) {
            fail.clear();
            std::lock_guard<std::mutex> lock(mutex);
            TaskLabel = "No checksum in registry; skipped";
        } else {
            std::string error;
            const std::string digest = FFileHash::sha256(
                e.localPath, error, [this](uint64_t done, uint64_t total) {
                    taskDone.store(done);
                    taskTotal.store(total);
                });
            if (digest.empty()) {
                fail = "hash failed: " + error;
            } else if (digest != e.sha256) {
                fail = "CHECKSUM MISMATCH (file != registry)";
                FLogger::GetInstance().error("Model " + id + " checksum mismatch");
            } else {
                fail.clear();
                FLogger::GetInstance().info("Model " + id + " checksum OK");
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(mutex);
        TaskLabel = fail.empty() ? ("Verified " + id + ": checksum OK") : fail;
    }
    if (worker.joinable()) {
        // Keep thread joinable for owner shutdown(); mark done via task reset below.
    }
    task.store(EModelTask::None);
}

void FModelManager::runImport(std::string sourcePath, std::string id) {
    std::string fail;
    FModelEntry e;
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
            taskTotal.store(static_cast<uint64_t>(total));
            std::vector<char> buf(1 << 20);
            uint64_t done = 0;
            bool ok = true;
            while (src) {
                src.read(buf.data(), static_cast<std::streamsize>(buf.size()));
                const auto n = src.gcount();
                if (n > 0) {
                    dst.write(buf.data(), n);
                    done += static_cast<uint64_t>(n);
                    taskDone.store(done);
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
                const std::string digest = FFileHash::sha256(tmp, error);
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
    Rescan();
    {
        std::lock_guard<std::mutex> lock(mutex);
        TaskLabel = fail.empty() ? ("Installed " + id) : fail;
    }
    task.store(EModelTask::None);
}

void FModelManager::runDelete(std::string id) {
    FModelEntry e;
    std::string fail = "entry missing";
    if (findCopy(id, e) && e.installed) {
        std::error_code ec;
        // Only delete files inside known model dirs (never arbitrary paths).
        const auto p = std::filesystem::path(e.localPath);
        const auto dir = p.parent_path().string();
        const bool knownDir =
            dir == modelDir || dir == (appDataDir() / "models").string();
        if (!knownDir) {
            fail = "refusing to delete outside model dirs";
        } else if (!std::filesystem::remove(p, ec) || ec) {
            fail = "delete failed: " + ec.message();
        } else {
            fail.clear();
            if (ActiveId == id) {
                ActiveId.clear();
            }
        }
    }
    Rescan();
    {
        std::lock_guard<std::mutex> lock(mutex);
        TaskLabel = fail.empty() ? ("Deleted " + id) : fail;
    }
    task.store(EModelTask::None);
}

} // namespace studio
