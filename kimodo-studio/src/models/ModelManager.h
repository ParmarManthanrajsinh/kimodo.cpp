#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace studio {

struct ModelEntry {
    std::string id;            // "soma-rp-v1.1"
    std::string name;          // "SOMA RP v1.1"
    std::string skeleton;      // "soma30"
    std::string version;
    std::string license;
    std::string source;        // hugging face repo
    std::string motionFile;    // expected filename
    std::string repo;          // hugging face repo id, may be empty
    std::string remotePath;    // path inside repo, may be empty
    std::string sha256;        // expected hex, may be empty
    uint64_t sizeBytes = 0;

    // Detected state (rescan fills these).
    bool installed = false;
    std::string localPath;
    uint64_t localBytes = 0;
};

enum class ModelTask { None, Verify, Import, Delete, Download };

// Local model registry + detection + maintenance worker.
// UI polls taskLabel()/taskProgress(); worker never touches UI.
class ModelManager {
public:
    ~ModelManager() { shutdown(); }
    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;
    ModelManager() = default;

    bool init(const std::string& registryPath, const std::string& modelDir,
              const std::string& textBundle);
    void rescan();
    void shutdown();

    std::vector<ModelEntry> entries() const;
    const std::string& modelDir() const { return modelDir_; }
    const std::string& textBundle() const { return textBundle_; }
    std::string activeId() const;

    bool findCopy(const std::string& id, ModelEntry& out) const;
    bool select(const std::string& id); // persists selection, unloads nothing

    // Async maintenance (no-op while busy).
    bool busy() const;
    ModelTask task() const { return task_.load(); }
    float taskProgress() const;
    std::string taskLabel() const;
    void verifyAsync(const std::string& id);
    void importAsync(const std::string& sourcePath, const std::string& id);
    void deleteAsync(const std::string& id);
    void downloadAsync(const std::string& id);
    void cancelTask();

private:
    void runVerify(std::string id);
    void runImport(std::string sourcePath, std::string id);
    void runDelete(std::string id);
    void runDownload(std::string id);
    void startTask(ModelTask t, const std::string& label);

    std::vector<ModelEntry> entries_;
    std::string registryPath_;
    std::string modelDir_;
    std::string textBundle_;
    std::string activeId_;

    std::thread worker_;
    std::atomic<ModelTask> task_{ModelTask::None};
    std::atomic<uint64_t> taskDone_{0};
    std::atomic<uint64_t> taskTotal_{0};
    std::atomic<bool> cancelRequested_{false};
    mutable std::mutex mutex_;
    std::string taskLabel_ = "idle";
};

} // namespace studio
