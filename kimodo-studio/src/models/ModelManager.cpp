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

static size_t SkipSpaces(const std::string& json, size_t p) {
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t' || json[p] == '\n' || json[p] == '\r')) {
        ++p;
    }
    return p;
}

// Locate `"key" :` tolerating whitespace; returns pos just past ':'.
bool find_key(const std::string& json, const std::string& key, size_t& value_pos, size_t start = 0) {
    const std::string pat = "\"" + key + "\"";
    size_t p = json.find(pat, start);
    while (p != std::string::npos) {
        size_t q = SkipSpaces(json, p + pat.size());
        if (q < json.size() && json[q] == ':') {
            value_pos = SkipSpaces(json, q + 1);
            return true;
        }
        p = json.find(pat, p + 1);
    }
    return false;
}

bool find_string(const std::string& json, const std::string& key, std::string& out, size_t start = 0) {
    size_t v = 0;
    if (!find_key(json, key, v, start) || v >= json.size() || json[v] != '"') {
        return false;
    }
    const size_t e = json.find('"', v + 1);
    if (e == std::string::npos) {
        return false;
    }
    out = json.substr(v + 1, e - v - 1);
    return true;
}

bool find_number(const std::string& json, const std::string& key, double& out, size_t start = 0) {
    size_t v = 0;
    if (!find_key(json, key, v, start)) {
        return false;
    }
    try {
        out = std::stod(json.substr(v));
        return true;
    } catch (...) {
        return false;
    }
}

std::filesystem::path AppDataDir() {
#if defined(_WIN32)
    if (const char* appdata = std::getenv("LOCALAPPDATA")) {
        return std::filesystem::path(appdata) / "KimodoStudio";
    }
#endif
    return std::filesystem::path(".");
}

} // namespace

bool ModelManager::Init(const std::string& in_registry_path, const std::string& in_model_dir,
                        const std::string& in_text_bundle) {
    registry_path = in_registry_path;
    model_dir = in_model_dir;
    text_bundle = in_text_bundle;

    std::ifstream in(registry_path);
    if (in) {
        const std::string json{std::istreambuf_iterator<char>(in), {}};
        // Flat object per model: {"models":[{...},{...}]}. Scan "id" occurrences.
        size_t pos = 0;
        std::string id;
        while (find_string(json, "id", id, pos)) {
            ModelEntry e;
            e.id = id;
            const size_t anchor = json.find(id, pos);
            find_string(json, "name", e.name, pos);
            find_string(json, "skeleton", e.skeleton, pos);
            find_string(json, "version", e.version, pos);
            find_string(json, "license", e.license, pos);
            find_string(json, "source", e.source, pos);
            find_string(json, "motionFile", e.motion_file, pos);
            find_string(json, "repo", e.repo, pos);
            find_string(json, "remote_path", e.remote_path, pos);
            find_string(json, "sha256", e.Sha256, pos);
            double num = 0;
            if (find_number(json, "size_bytes", num, pos)) {
                e.size_bytes = static_cast<uint64_t>(num);
            }
            entries.push_back(std::move(e));
            pos = anchor + id.size();
        }
    }
    if (entries.empty()) {
        Logger::GetInstance().Warning("ModelManager: registry empty or missing: " + registry_path);
    }

    // Restore selection.
    std::ifstream sel(AppDataDir() / "settings" / "active_model.txt");
    if (sel) {
        std::getline(sel, active_id);
    }
    Rescan();
    return true;
}

namespace {
const ModelEntry* find_in(const std::vector<ModelEntry>& entries, const std::string& id) {
    for (const ModelEntry& e : entries) {
        if (e.id == id) {
            return &e;
        }
    }
    return nullptr;
}
} // namespace

void ModelManager::Rescan() {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<std::string> dirs = {
        model_dir,
        (AppDataDir() / "models").string(),
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

    for (ModelEntry& e : entries) {
        e.installed = false;
        e.local_path.clear();
        e.local_bytes = 0;
        if (e.motion_file.empty()) {
            continue;
        }
        for (const std::string& dir : dirs) {
            std::error_code ec;
            const auto cand = std::filesystem::path(dir) / e.motion_file;
            const auto bytes = std::filesystem::file_size(cand, ec);
            if (!ec && bytes > 0) {
                e.installed = true;
                e.local_path = cand.string();
                e.local_bytes = static_cast<uint64_t>(bytes);
                break;
            }
        }
    }
    const ModelEntry* active = find_in(entries, active_id);
    if (!active || !active->installed) {
        active_id.clear();
        for (const ModelEntry& e : entries) {
            if (e.installed) {
                active_id = e.id;
                break;
            }
        }
    }
}

void ModelManager::Shutdown() {
    if (worker.joinable()) {
        worker.join();
    }
    task.store(ModelTask::None);
}

std::vector<ModelEntry> ModelManager::GetEntries() const {
    std::lock_guard<std::mutex> lock(mutex);
    return entries;
}

std::string ModelManager::GetActiveId() const {
    std::lock_guard<std::mutex> lock(mutex);
    return active_id;
}

bool ModelManager::find_copy(const std::string& id, ModelEntry& out) const {
    std::lock_guard<std::mutex> lock(mutex);
    const ModelEntry* e = find_in(entries, id);
    if (!e) {
        return false;
    }
    out = *e;
    return true;
}

bool ModelManager::Select(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex);
    const ModelEntry* e = find_in(entries, id);
    if (!e || !e->installed) {
        return false;
    }
    active_id = id;
    std::error_code ec;
    std::filesystem::create_directories(AppDataDir() / "settings", ec);
    std::ofstream sel(AppDataDir() / "settings" / "active_model.txt", std::ios::trunc);
    if (sel) {
        sel << id;
    }
    return true;
}

bool ModelManager::IsBusy() const { return task.load() != ModelTask::None; }

float ModelManager::GetTaskProgress() const {
    const uint64_t total = task_total.load();
    if (total == 0) {
        return 0.0f;
    }
    float p = static_cast<float>(task_done.load()) / static_cast<float>(total);
    return p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
}

std::string ModelManager::GetTaskLabel() const {
    std::lock_guard<std::mutex> lock(mutex);
    return task_label;
}

void ModelManager::CancelTask() { cancel_requested.store(true); }

void ModelManager::StartTask(ModelTask t, const std::string& label) {
    if (worker.joinable()) {
        worker.join();
    }
    task_done.store(0);
    task_total.store(0);
    cancel_requested.store(false);
    {
        std::lock_guard<std::mutex> lock(mutex);
        task_label = label;
    }
    task.store(t);
}

void ModelManager::VerifyAsync(const std::string& id) {
    if (IsBusy()) {
        return;
    }
    StartTask(ModelTask::Verify, "Verifying " + id + "...");
    worker = std::thread(&ModelManager::RunVerify, this, id);
}

void ModelManager::ImportAsync(const std::string& source_path, const std::string& id) {
    if (IsBusy()) {
        return;
    }
    StartTask(ModelTask::Import, "Importing " + id + "...");
    worker = std::thread(&ModelManager::RunImport, this, source_path, id);
}

void ModelManager::DeleteAsync(const std::string& id) {
    if (IsBusy()) {
        return;
    }
    StartTask(ModelTask::Delete, "Deleting " + id + "...");
    worker = std::thread(&ModelManager::RunDelete, this, id);
}

void ModelManager::DownloadAsync(const std::string& id) {
    if (IsBusy()) {
        return;
    }
    StartTask(ModelTask::Download, "Downloading " + id + "...");
    worker = std::thread(&ModelManager::RunDownload, this, id);
}

void ModelManager::RunDownload(std::string id) {
    std::string fail;
    ModelEntry e;
    if (!find_copy(id, e)) {
        fail = "unknown model id";
    } else if (e.repo.empty() || e.remote_path.empty()) {
        fail = "no Hugging Face source in registry";
    } else {
        // Downloads land in the user model dir, never in the repo checkout.
        const auto user_dir = AppDataDir() / "models";
        std::error_code ec;
        std::filesystem::create_directories(user_dir, ec);
        const auto dest = user_dir / e.motion_file;
        const std::string tmp = dest.string() + ".download";
        std::string token;
        HFAuthenticator::LoadToken(token); // may be empty for public repos
        const std::string url = HuggingFaceClient::ResolveUrl(e.repo, e.remote_path);
        Logger::GetInstance().Info("Model " + id + " download started");
        const bool ok = HuggingFaceClient::download(
            url, tmp, token,
            [this](uint64_t done, uint64_t total) {
                task_done.store(done);
                task_total.store(total);
                return !cancel_requested.load();
            },
            fail);
        if (!ok) {
            if (fail.empty()) {
                fail = "download failed";
            } else if (fail == "cancelled") {
                fail = "Download cancelled (resume on next download)";
            }
        } else {
            std::string error;
            const std::string digest = FileHash::Sha256(tmp, error);
            if (!e.Sha256.empty() && digest != e.Sha256) {
                std::filesystem::remove(tmp, ec);
                fail = "CHECKSUM MISMATCH; download discarded";
                Logger::GetInstance().Error("Model " + id + " download checksum mismatch");
            } else {
                std::filesystem::rename(tmp, dest, ec);
                if (ec) {
                    fail = "atomic install failed: " + ec.message();
                } else {
                    Logger::GetInstance().Info("Model " + id + " downloaded + verified");
                }
            }
        }
    }
    Rescan();
    {
        std::lock_guard<std::mutex> lock(mutex);
        task_label = fail.empty() ? ("Installed " + id) : fail;
    }
    task.store(ModelTask::None);
}

void ModelManager::RunVerify(std::string id) {
    ModelEntry e;
    std::string fail = "entry missing";
    if (find_copy(id, e) && e.installed) {
        if (e.Sha256.empty()) {
            fail.clear();
            std::lock_guard<std::mutex> lock(mutex);
            task_label = "No checksum in registry; skipped";
        } else {
            std::string error;
            const std::string digest = FileHash::Sha256(e.local_path, error, [this](uint64_t done, uint64_t total) {
                task_done.store(done);
                task_total.store(total);
            });
            if (digest.empty()) {
                fail = "hash failed: " + error;
            } else if (digest != e.Sha256) {
                fail = "CHECKSUM MISMATCH (file != registry)";
                Logger::GetInstance().Error("Model " + id + " checksum mismatch");
            } else {
                fail.clear();
                Logger::GetInstance().Info("Model " + id + " checksum OK");
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(mutex);
        task_label = fail.empty() ? ("Verified " + id + ": checksum OK") : fail;
    }
    if (worker.joinable()) {
        // Keep thread joinable for owner shutdown(); mark done via task reset below.
    }
    task.store(ModelTask::None);
}

void ModelManager::RunImport(std::string source_path, std::string id) {
    std::string fail;
    ModelEntry e;
    if (!find_copy(id, e)) {
        fail = "unknown model id";
    } else {
        // Imports land in the user model dir, never in the repo checkout.
        const auto user_dir = AppDataDir() / "models";
        std::error_code ec;
        std::filesystem::create_directories(user_dir, ec);
        const auto dest = user_dir / e.motion_file;
        const auto tmp = dest.string() + ".download";
        std::ifstream src(source_path, std::ios::binary);
        std::ofstream dst(tmp, std::ios::binary | std::ios::trunc);
        if (!src || !dst) {
            fail = "cannot open source or destination";
        } else {
            src.seekg(0, std::ios::end);
            const auto total = src.tellg();
            src.seekg(0, std::ios::beg);
            task_total.store(static_cast<uint64_t>(total));
            std::vector<char> buf(1 << 20);
            uint64_t done = 0;
            bool ok = true;
            while (src) {
                src.read(buf.data(), static_cast<std::streamsize>(buf.size()));
                const auto n = src.gcount();
                if (n > 0) {
                    dst.write(buf.data(), n);
                    done += static_cast<uint64_t>(n);
                    task_done.store(done);
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
                const std::string digest = FileHash::Sha256(tmp, error);
                if (!e.Sha256.empty() && digest != e.Sha256) {
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
        task_label = fail.empty() ? ("Installed " + id) : fail;
    }
    task.store(ModelTask::None);
}

void ModelManager::RunDelete(std::string id) {
    ModelEntry e;
    std::string fail = "entry missing";
    if (find_copy(id, e) && e.installed) {
        std::error_code ec;
        // Only delete files inside known model dirs (never arbitrary paths).
        const auto p = std::filesystem::path(e.local_path);
        const auto dir = p.parent_path().string();
        const bool known_dir = dir == model_dir || dir == (AppDataDir() / "models").string();
        if (!known_dir) {
            fail = "refusing to delete outside model dirs";
        } else if (!std::filesystem::remove(p, ec) || ec) {
            fail = "delete failed: " + ec.message();
        } else {
            fail.clear();
            if (active_id == id) {
                active_id.clear();
            }
        }
    }
    Rescan();
    {
        std::lock_guard<std::mutex> lock(mutex);
        task_label = fail.empty() ? ("Deleted " + id) : fail;
    }
    task.store(ModelTask::None);
}

} // namespace studio
