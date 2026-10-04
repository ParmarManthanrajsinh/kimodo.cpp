#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace studio
{

struct BundleFile
{
    std::string remote_path;
    std::string local_rel_path;
    uint64_t size_bytes = 0;
    std::string sha256;
};

struct ModelEntry
{
    std::string id;       // "soma-rp-v1.1"
    std::string name;     // "SOMA RP v1.1"
    std::string skeleton; // "soma30"
    std::string version;
    std::string license;
    std::string source;      // hugging face repo
    std::string motion_file; // expected filename or directory
    std::string repo;        // hugging face repo id, may be empty
    std::string remote_path; // path inside repo, may be empty
    std::string Sha256;      // expected hex, may be empty
    uint64_t size_bytes = 0;
    bool required = true;
    bool gated = false;
    std::string asset_type = "motion"; // "motion", "text_encoder", "character", "runtime"

    // Multi-file asset support
    std::vector<BundleFile> files;

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

// Explicit worker outcome. SetupManager must branch on this, never by
// substring-matching the human-readable task label: an unrecognized label
// previously meant "success", silently auto-restarting failed downloads.
enum class TaskResult
{
    Unknown, // task started (or never ran): no verdict yet
    Ok,
    Failed
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

    bool Init(std::string_view registry_path, std::string_view model_dir, std::string_view text_bundle);
    void Rescan();
    void Shutdown();

    [[nodiscard]] std::vector<ModelEntry> GetEntries() const;
    [[nodiscard]] const std::string& GetModelDir() const noexcept
    {
        return model_dir;
    }
    [[nodiscard]] const std::string& GetTextBundle() const noexcept
    {
        return text_bundle;
    }
    [[nodiscard]] std::string GetActiveId() const;

    [[nodiscard]] bool find_copy(std::string_view id, ModelEntry& out) const;
    bool Select(std::string_view id); // persists selection, unloads nothing

    [[nodiscard]] bool AreAllRequiredInstalled() const;
    [[nodiscard]] std::vector<ModelEntry> GetMissingRequired() const;
    [[nodiscard]] bool IsModelOrBundleInstalled(std::string_view id) const;
    // NOTE: size-complete only (all files present at exact registry sizes,
    // no .part files). SHA-256 verification happens in VerifyAsync/VerifyAll.
    // A bare existing directory is never considered complete.
    [[nodiscard]] bool IsBundleComplete(std::string_view id) const
    {
        return IsModelOrBundleInstalled(id);
    }

    // Async maintenance (no-op while busy).
    [[nodiscard]] bool IsBusy() const;
    [[nodiscard]] ModelTask GetTask() const noexcept
    {
        return task.load();
    }
    // Explicit outcome of the last finished worker. Unknown while a task is
    // running or when no task has run since Init/StartTask.
    [[nodiscard]] TaskResult GetTaskResult() const noexcept
    {
        return task_result.load();
    }
    [[nodiscard]] float GetTaskProgress() const;
    [[nodiscard]] uint64_t GetTaskDone() const noexcept
    {
        return task_done.load();
    }
    [[nodiscard]] uint64_t GetTaskTotal() const noexcept
    {
        return task_total.load();
    }
    [[nodiscard]] std::string GetTaskLabel() const;
    void VerifyAsync(std::string_view id);
    // Last verification/download failure details for the Setup error UX.
    // Empty `failed_file` means no failure recorded since the last task start.
    bool GetLastFailure(std::string& failed_file, std::string& expected_sha256,
                        std::string& received_sha256, uint64_t& expected_bytes,
                        uint64_t& received_bytes, bool& received_known,
                        std::string& message) const;
    void ImportAsync(std::string_view source_path, std::string_view id);
    void DeleteAsync(std::string_view id);
    void DownloadAsync(std::string_view id);
    void CancelTask();

private:
    void RunVerify(std::string id);
    void RunImport(std::string source_path, std::string id);
    void RunDelete(std::string id);
    void RunDownload(std::string id);
    void StartTask(ModelTask t, std::string_view label);

    std::vector<ModelEntry> entries;
    std::string registry_path;
    std::string model_dir;
    std::string text_bundle;
    std::string active_id;

    std::thread worker;
    std::atomic<ModelTask> task{ModelTask::None};
    std::atomic<TaskResult> task_result{TaskResult::Unknown};
    void SetTaskResult(TaskResult r)
    {
        task_result.store(r);
    }
    std::atomic<uint64_t> task_done{0};
    std::atomic<uint64_t> task_total{0};
    std::atomic<bool> cancel_requested{false};
    mutable std::mutex mutex;
    std::string task_label = "idle";
    // Last failure details (cleared at task start, set on verify failures).
    std::string last_failed_file;
    std::string last_expected_sha;
    std::string last_received_sha;
    uint64_t last_expected_bytes = 0;
    uint64_t last_received_bytes = 0;
    bool last_received_known = false;
    std::string last_fail_message;
    void ClearLastFailure();
    void SetLastFailure(std::string_view file, std::string_view expected_sha,
                        std::string_view received_sha, uint64_t expected_bytes,
                        uint64_t received_bytes, bool received_known, std::string_view message);
};

} // namespace studio
