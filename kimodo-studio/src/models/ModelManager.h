#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace studio
{

struct ModelEntry
{
    std::string id;       // "soma-rp-v1.1"
    std::string name;     // "SOMA RP v1.1"
    std::string skeleton; // "soma30"
    std::string version;
    std::string license;
    std::string source;      // hugging face repo
    std::string motion_file; // expected filename
    std::string repo;        // hugging face repo id, may be empty
    std::string remote_path; // path inside repo, may be empty
    std::string Sha256;      // expected hex, may be empty
    uint64_t size_bytes = 0;

    // Detected state (rescan fills these).
    bool installed = false;
    std::string local_path;
    uint64_t local_bytes = 0;
};

enum class ModelTask
{
    None,
    Verify,
    Import,
    Delete,
    Download
};

// Local model registry + detection + maintenance worker.
// UI polls taskLabel()/taskProgress(); worker never touches UI.
class ModelManager
{
public:
    ~ModelManager()
    {
        Shutdown();
    }
    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;
    ModelManager() = default;

    bool Init(const std::string& registry_path, const std::string& model_dir, const std::string& text_bundle);
    void Rescan();
    void Shutdown();

    std::vector<ModelEntry> GetEntries() const;
    const std::string& GetModelDir() const
    {
        return model_dir;
    }
    const std::string& GetTextBundle() const
    {
        return text_bundle;
    }
    std::string GetActiveId() const;

    bool find_copy(const std::string& id, ModelEntry& out) const;
    bool Select(const std::string& id); // persists selection, unloads nothing

    // Async maintenance (no-op while busy).
    bool IsBusy() const;
    ModelTask GetTask() const
    {
        return task.load();
    }
    float GetTaskProgress() const;
    std::string GetTaskLabel() const;
    void VerifyAsync(const std::string& id);
    void ImportAsync(const std::string& source_path, const std::string& id);
    void DeleteAsync(const std::string& id);
    void DownloadAsync(const std::string& id);
    void CancelTask();

private:
    void RunVerify(std::string id);
    void RunImport(std::string source_path, std::string id);
    void RunDelete(std::string id);
    void RunDownload(std::string id);
    void StartTask(ModelTask t, const std::string& label);

    std::vector<ModelEntry> entries;
    std::string registry_path;
    std::string model_dir;
    std::string text_bundle;
    std::string active_id;

    std::thread worker;
    std::atomic<ModelTask> task{ModelTask::None};
    std::atomic<uint64_t> task_done{0};
    std::atomic<uint64_t> task_total{0};
    std::atomic<bool> cancel_requested{false};
    mutable std::mutex mutex;
    std::string task_label = "idle";
};

} // namespace studio
