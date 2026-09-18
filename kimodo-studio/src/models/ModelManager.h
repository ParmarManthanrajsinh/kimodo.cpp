#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace studio {

struct FModelEntry {
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

enum class EModelTask { None, Verify, Import, Delete, Download };

// Local model registry + detection + maintenance worker.
// UI polls taskLabel()/taskProgress(); worker never touches UI.
class FModelManager {
public:
    ~FModelManager() { Shutdown(); }
    FModelManager(const FModelManager&) = delete;
    FModelManager& operator=(const FModelManager&) = delete;
    FModelManager() = default;

    bool Init(const std::string& registryPath, const std::string& modelDir,
              const std::string& textBundle);
    void Rescan();
    void Shutdown();

    std::vector<FModelEntry> GetEntries() const;
    const std::string& GetModelDir() const { return modelDir; }
    const std::string& GetTextBundle() const { return textBundle; }
    std::string GetActiveId() const;

    bool findCopy(const std::string& id, FModelEntry& out) const;
    bool select(const std::string& id); // persists selection, unloads nothing

    // Async maintenance (no-op while busy).
    bool IsBusy() const;
    EModelTask GetTask() const { return task.load(); }
    float GetTaskProgress() const;
    std::string GetTaskLabel() const;
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
    void startTask(EModelTask t, const std::string& label);

    std::vector<FModelEntry> entries;
    std::string registryPath;
    std::string modelDir;
    std::string textBundle;
    std::string ActiveId;

    std::thread worker;
    std::atomic<EModelTask> task{EModelTask::None};
    std::atomic<uint64_t> taskDone{0};
    std::atomic<uint64_t> taskTotal{0};
    std::atomic<bool> bCancelRequested{false};
    mutable std::mutex mutex;
    std::string TaskLabel = "idle";
};

} // namespace studio
