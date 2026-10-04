#include "models/ModelManager.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "huggingface/HFAuthenticator.h"
#include "huggingface/HuggingFaceClient.h"
#include "utils/AppPaths.h"
#include "utils/Hash.h"
#include "utils/Logger.h"

namespace studio
{
namespace
{

static size_t SkipSpaces(std::string_view json, size_t p)
{
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t' || json[p] == '\n' || json[p] == '\r'))
    {
        ++p;
    }
    return p;
}

// Locate `"key" :` tolerating whitespace; returns pos just past ':'.
bool find_key(std::string_view json, std::string_view key, size_t& value_pos, size_t start = 0)
{
    const std::string pat = "\"" + std::string(key) + "\"";
    size_t p = json.find(pat, start);
    while (p != std::string_view::npos)
    {
        size_t q = SkipSpaces(json, p + pat.size());
        if (q < json.size() && json[q] == ':')
        {
            value_pos = SkipSpaces(json, q + 1);
            return true;
        }
        p = json.find(pat, p + 1);
    }
    return false;
}

bool find_string(std::string_view json, std::string_view key, std::string& out, size_t start = 0)
{
    size_t v = 0;
    if (!find_key(json, key, v, start) || v >= json.size() || json[v] != '"')
    {
        return false;
    }
    const size_t e = json.find('"', v + 1);
    if (e == std::string_view::npos)
    {
        return false;
    }
    out = std::string(json.substr(v + 1, e - v - 1));
    return true;
}

bool find_number(std::string_view json, std::string_view key, double& out, size_t start = 0)
{
    size_t v = 0;
    if (!find_key(json, key, v, start))
    {
        return false;
    }
    try
    {
        out = std::stod(std::string(json.substr(v)));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool find_bool(std::string_view json, std::string_view key, bool& out, size_t start = 0)
{
    size_t v = 0;
    if (!find_key(json, key, v, start))
    {
        return false;
    }
    if (json.substr(v, 4) == "true")
    {
        out = true;
        return true;
    }
    if (json.substr(v, 5) == "false")
    {
        out = false;
        return true;
    }
    return false;
}

std::filesystem::path AppDataDir()
{
#if defined(_WIN32)
    if (const char* appdata = std::getenv("LOCALAPPDATA"))
    {
        return std::filesystem::path(appdata) / "KimodoStudio";
    }
#endif
    return std::filesystem::path(".");
}

std::vector<BundleFile> GetCanonicalTextBundleFiles()
{
    std::vector<BundleFile> list = {
        {"generated/llm2vec-text-bundle/tokenizer.gguf", "tokenizer.gguf", 7295054ULL, "81614aca62a98846c02b72cc2e5378e5bdce8b4d507b88f96eb1faa90dae607e"},
        {"generated/llm2vec-text-bundle/embedding.gguf", "embedding.gguf", 1050674240ULL, "8d2c16c7996d2d2a6b5071d623fbff413432c881a998a0b4d442786b47c63560"},
        {"generated/llm2vec-text-bundle/final-norm.gguf", "final-norm.gguf", 9248ULL, "3d705ae86b5ac9c49634f8517abfdedb2266a03758750bc707dd0cc67927c95c"}
    };
    static const char* layer_hashes[32] = {
        "b4fbf81fa84d08c4c657dc7956fccdbd9d50c83e11fe291d9f3a58dddb4ba4cc",
        "6ac62e2961540327478b6a7487392f404ee98da4e9206b49b129d9b28fa54ab2",
        "c47cef118e2a8e48a479f4823e330bd2e861d8b31b3239a02e78fc31ca97dd04",
        "8539ec5b283159d38aad68115b99a12b7a54b38e65ca205d7297873a7bdfa509",
        "204f009bb401aaaab7a4a14adc57ce8b94896cdc4ae4e2e80b8df2f97954cad0",
        "b20a74d09ba0abb5135fdb900067cca02e6d5cc34d05a37147cb78c63b388c04",
        "c00d620a46a111786b340bce68f47ae89a62f3d05d31acde830c0a71d9060200",
        "578064ffe7a38b9c4a014780ac5ca99de2497ac93544c04ba849bb262470a563",
        "4a6319773a5e19b6e17bb55ee2629ddaa97f6a8fb22d37c3ea6be9e58e3c3087",
        "49a13e69f524144afed1c52fa819ec48f40014c3561f4e62ccf5eac1808e940e",
        "122fac4b17d800f84fbd2e9c0d5510ad773818e604d9e396328bd9cafcf72008",
        "aaddc4180b44bac7a0be466d7f4812fb338bbdefae188b4195580cf4cef64266",
        "889d22f1f7b01e5cbb74f3e8d48a3af0c1d03d075c997c660a1ec00ab9014b32",
        "4fbf6d5ef2e4001a8fd5e356702294ebe60799f79ae39a7495290ccd616f9ba4",
        "683b753e7778f774f819141d745fb8aee27b59186f18f0b6e7e4b2501c9aa779",
        "ebc5ec00c746ad1c4bf5b83a381f76de7354e756a514abfa741aa25a42cecd98",
        "195fb02a1828a5cdf10a1babca4346539c8e79c986051f246def49691cb0928b",
        "89010f85d6d65f6565d87ea973ae3478c0c50318bec65092677a0629037e4b29",
        "c792d2a68d5694c4b3c5f6ecb34ccdf58a9b70eecb0f2b4e088a0e000a2ce003",
        "8eea4627aa980a6af4c398fab8bd2722afbbddcf06eae20ebba8eabe1b9b4cff",
        "e78a1a63c518163180fcad8834ad2aafded81724a89b39a2b08c14868387e532",
        "5f299eab47553e622c0a8a21592ff54d9c5117ae612efb482f200186e588f718",
        "70a85e4fbcb4420d56f2ae919293a21a311c32ef2dd19ffead2392350af8d772",
        "1f91267d870d917fda9e2a3e7a9e99ba4c90746f24bc0c0a26a8263847def38c",
        "705fb89b13fe8aeb4207d95a9c83f54078983062719148859269238723015b49",
        "883f02a1d6d960f54ad109b7c0d546babc774a68857efbb7e7043c4916d948af",
        "7c7dcf9ce30093dd5bcb74e69215d7685bc451af7760e7c07f43ac0792aefc4f",
        "59babbcac45cce55247ecf582e30d6ef7987126517c504bf929c32fb925acc4a",
        "ac4575ab811d1e2b844133bf1d4076c8938de7de7801493701a45e2a7fb7ddaa",
        "c5a4c71d0ee6570cd8e6964500c2acd65a263cff3c61aaa45a01a911dd34695b",
        "2af5c0c42fea1b91bfe2f6a52918fc11724c09a9faf0beb08177c22d5b56ffb0",
        "a2e6086de679aa0e07c5fd8f7768c5eda0e00b5863ac2c1773622c90d619c16f"
    };
    for (int i = 0; i < 32; ++i)
    {
        char fname[32];
        std::snprintf(fname, sizeof(fname), "layer-%02d.gguf", i);
        std::string rem = "generated/llm2vec-text-bundle/" + std::string(fname);
        list.push_back({rem, std::string(fname), 441469344ULL, layer_hashes[i]});
    }
    return list;
}

} // namespace

bool ModelManager::Init(std::string_view in_registry_path, std::string_view in_model_dir,
                        std::string_view in_text_bundle)
{
    registry_path = std::string(in_registry_path);
    model_dir = std::string(in_model_dir);
    text_bundle = std::string(in_text_bundle);

    std::ifstream in(registry_path);
    if (in)
    {
        const std::string json{std::istreambuf_iterator<char>(in), {}};
        // Flat object per model: {"models":[{...},{...}]}. Scan "id" occurrences.
        size_t pos = 0;
        std::string id;
        while (find_string(json, "id", id, pos))
        {
            ModelEntry e;
            e.id = id;
            const size_t anchor = json.find(id, pos);
            size_t after_id = anchor + id.size();
            size_t next_id = json.find("\"id\"", after_id);
            size_t block_end = (next_id != std::string::npos) ? next_id : json.size();
            std::string entry_chunk = json.substr(anchor, block_end - anchor);

            find_string(entry_chunk, "name", e.name);
            find_string(entry_chunk, "skeleton", e.skeleton);
            find_string(entry_chunk, "version", e.version);
            find_string(entry_chunk, "license", e.license);
            find_string(entry_chunk, "source", e.source);
            if (!find_string(entry_chunk, "motionFile", e.motion_file))
            {
                if (!find_string(entry_chunk, "motion_file", e.motion_file))
                {
                    find_string(entry_chunk, "localFilename", e.motion_file);
                }
            }
            find_string(entry_chunk, "repo", e.repo);
            if (!find_string(entry_chunk, "remotePath", e.remote_path))
            {
                find_string(entry_chunk, "remote_path", e.remote_path);
            }
            find_string(entry_chunk, "sha256", e.Sha256);
            find_string(entry_chunk, "assetType", e.asset_type);
            bool req = true;
            if (find_bool(entry_chunk, "required", req))
            {
                e.required = req;
            }
            bool gtd = false;
            if (find_bool(entry_chunk, "gated", gtd))
            {
                e.gated = gtd;
            }
            double num = 0;
            if (find_number(entry_chunk, "sizeBytes", num) || find_number(entry_chunk, "size_bytes", num))
            {
                e.size_bytes = static_cast<uint64_t>(num);
            }

            // Parse optional "files" array for multi-file bundles
            size_t files_key = entry_chunk.find("\"files\"");
            if (files_key != std::string::npos)
            {
                size_t arr_start = entry_chunk.find('[', files_key);
                size_t arr_end = entry_chunk.find(']', arr_start);
                if (arr_start != std::string::npos && arr_end != std::string::npos)
                {
                    size_t fpos = arr_start;
                    while (fpos < arr_end)
                    {
                        size_t o_start = entry_chunk.find('{', fpos);
                        if (o_start == std::string::npos || o_start >= arr_end) break;
                        size_t o_end = entry_chunk.find('}', o_start);
                        if (o_end == std::string::npos || o_end > arr_end) break;
                        std::string chunk = entry_chunk.substr(o_start, o_end - o_start + 1);
                        BundleFile bf;
                        find_string(chunk, "remotePath", bf.remote_path);
                        find_string(chunk, "localFilename", bf.local_rel_path);
                        find_string(chunk, "sha256", bf.sha256);
                        double bsz = 0;
                        if (find_number(chunk, "sizeBytes", bsz))
                        {
                            bf.size_bytes = static_cast<uint64_t>(bsz);
                        }
                        if (!bf.local_rel_path.empty())
                        {
                            e.files.push_back(std::move(bf));
                        }
                        fpos = o_end + 1;
                    }
                }
            }

            // Fallback for llm2vec-text-bundle if files array was omitted
            if (e.id == "llm2vec-text-bundle" && e.files.empty())
            {
                e.files = GetCanonicalTextBundleFiles();
                if (e.motion_file.empty()) e.motion_file = "llm2vec-text-bundle";
                if (e.size_bytes == 0) e.size_bytes = 16228007890ULL;
            }
            // For single-file asset, ensure files has 1 entry
            else if (e.files.empty() && !e.remote_path.empty())
            {
                e.files.push_back({e.remote_path, e.motion_file, e.size_bytes, e.Sha256});
            }

            entries.push_back(std::move(e));
            pos = block_end;
        }
    }
    if (entries.empty())
    {
        Logger::GetInstance().Warning("ModelManager: registry empty or missing: " + registry_path);
    }

    // Restore selection.
    std::ifstream sel(AppDataDir() / "settings" / "active_model.txt");
    if (sel)
    {
        std::getline(sel, active_id);
    }
    Rescan();
    return true;
}

namespace
{
const ModelEntry* find_in(const std::vector<ModelEntry>& entries, std::string_view id)
{
    for (const auto& e : entries)
    {
        if (e.id == id)
        {
            return &e;
        }
    }
    return nullptr;
}
} // namespace

void ModelManager::Rescan()
{
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<std::string> dirs = {
        model_dir,
        (AppDataDir() / "models").string(),
        (std::filesystem::current_path() / "models").string(),
    };

#if defined(KIMODO_ROOT_DIR)
    dirs.push_back((std::filesystem::path(KIMODO_ROOT_DIR) / "models").string());
    dirs.push_back((std::filesystem::path(KIMODO_ROOT_DIR) / "generated").string());
#endif
#if defined(KIMODO_STUDIO_SOURCE_DIR)
    dirs.push_back((std::filesystem::path(KIMODO_STUDIO_SOURCE_DIR) / "../models").string());
    dirs.push_back((std::filesystem::path(KIMODO_STUDIO_SOURCE_DIR) / "models").string());
    dirs.push_back((std::filesystem::path(KIMODO_STUDIO_SOURCE_DIR) / "../generated").string());
#endif

    // Walk up searching for models/ and generated/
    std::filesystem::path cur = std::filesystem::current_path();
    for (int i = 0; i < 5; ++i)
    {
        if (!cur.empty())
        {
            dirs.push_back((cur / "models").string());
            dirs.push_back((cur / "generated").string());
            cur = cur.parent_path();
        }
    }

    for (ModelEntry& e : entries)
    {
        e.installed = false;
        e.local_path.clear();
        e.local_bytes = 0;

        // Special handling for bundled character and runtime dependencies
        if (e.asset_type == "character" || e.asset_type == "runtime")
        {
            std::filesystem::path cand = AppPaths::ResolveAsset(e.motion_file);
            std::error_code ec;
            if (std::filesystem::exists(cand, ec) && !ec)
            {
                e.installed = true;
                e.local_path = cand.string();
                e.local_bytes = std::filesystem::file_size(cand, ec);
            }
            continue;
        }

        if (e.motion_file.empty())
        {
            continue;
        }

        for (const std::string& dir : dirs)
        {
            std::error_code ec;

            // 1. Multi-file bundle: verify that EVERY file exists and is complete
            if (e.files.size() > 1)
            {
                const auto bundle_dir = std::filesystem::path(dir) / e.motion_file;
                if (!std::filesystem::is_directory(bundle_dir, ec) || ec)
                {
                    continue;
                }

                bool all_files_valid = true;
                uint64_t total_bundle_bytes = 0;

                for (const auto& bf : e.files)
                {
                    const auto fp = bundle_dir / bf.local_rel_path;
                    const auto part_p = bundle_dir / (bf.local_rel_path + ".part");
                    if (!std::filesystem::is_regular_file(fp, ec) || ec)
                    {
                        all_files_valid = false;
                        break;
                    }
                    if (std::filesystem::exists(part_p, ec))
                    {
                        // Incomplete / partially downloaded file exists
                        all_files_valid = false;
                        break;
                    }
                    auto sz = std::filesystem::file_size(fp, ec);
                    if (ec || sz == 0)
                    {
                        all_files_valid = false;
                        break;
                    }
                    if (bf.size_bytes > 0 && sz != bf.size_bytes)
                    {
                        all_files_valid = false;
                        break;
                    }
                    total_bundle_bytes += sz;
                }

                if (all_files_valid)
                {
                    e.installed = true;
                    e.local_path = bundle_dir.string();
                    e.local_bytes = total_bundle_bytes;
                    break;
                }
            }
            // 2. Single-file asset: same contract as bundles — the file must
            // exist at exactly the registry size (size-complete). A short
            // file is truncated, never installed.
            else
            {
                std::string fname = (!e.files.empty()) ? e.files.front().local_rel_path : e.motion_file;
                uint64_t expected =
                    (!e.files.empty() && e.files.front().size_bytes > 0)
                        ? e.files.front().size_bytes
                        : e.size_bytes;
                const auto cand = std::filesystem::path(dir) / fname;
                const auto part_cand = std::filesystem::path(dir) / (fname + ".part");
                if (std::filesystem::is_regular_file(cand, ec) && !ec && !std::filesystem::exists(part_cand, ec))
                {
                    auto bytes = std::filesystem::file_size(cand, ec);
                    if (!ec && bytes > 0 && (expected == 0 || bytes == expected))
                    {
                        e.installed = true;
                        e.local_path = cand.string();
                        e.local_bytes = static_cast<uint64_t>(bytes);
                        break;
                    }
                }
            }
        }

        // External bundle search (e.g. text_bundle passed into Init)
        if (e.asset_type == "text_encoder" && !e.installed && !text_bundle.empty())
        {
            std::error_code ec;
            if (std::filesystem::is_directory(text_bundle, ec) && !ec)
            {
                bool all_valid = true;
                uint64_t total_sz = 0;
                for (const auto& bf : e.files)
                {
                    const auto fp = std::filesystem::path(text_bundle) / bf.local_rel_path;
                    if (!std::filesystem::is_regular_file(fp, ec) || ec)
                    {
                        all_valid = false;
                        break;
                    }
                    auto sz = std::filesystem::file_size(fp, ec);
                    if (ec || sz == 0 || (bf.size_bytes > 0 && sz != bf.size_bytes))
                    {
                        all_valid = false;
                        break;
                    }
                    total_sz += sz;
                }
                if (all_valid)
                {
                    e.installed = true;
                    e.local_path = text_bundle;
                    e.local_bytes = total_sz;
                }
            }
        }
    }
    const ModelEntry* active = find_in(entries, active_id);
    if (!active || !active->installed)
    {
        active_id.clear();
        for (const ModelEntry& e : entries)
        {
            if (e.installed)
            {
                active_id = e.id;
                break;
            }
        }
    }
}

void ModelManager::Shutdown()
{
    if (worker.joinable())
    {
        worker.join();
    }
    task.store(ModelTask::None);
}

std::vector<ModelEntry> ModelManager::GetEntries() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return entries;
}

std::string ModelManager::GetActiveId() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return active_id;
}

bool ModelManager::find_copy(std::string_view id, ModelEntry& out) const
{
    std::lock_guard<std::mutex> lock(mutex);
    const ModelEntry* e = find_in(entries, id);
    if (!e)
    {
        return false;
    }
    out = *e;
    return true;
}

bool ModelManager::Select(std::string_view id)
{
    std::lock_guard<std::mutex> lock(mutex);
    const ModelEntry* e = find_in(entries, id);
    if (!e || !e->installed)
    {
        return false;
    }
    active_id = std::string(id);
    std::error_code ec;
    std::filesystem::create_directories(AppDataDir() / "settings", ec);
    std::ofstream sel(AppDataDir() / "settings" / "active_model.txt", std::ios::trunc);
    if (sel)
    {
        sel << id;
    }
    return true;
}

bool ModelManager::AreAllRequiredInstalled() const
{
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto& e : entries)
    {
        if (e.required && !e.installed)
        {
            return false;
        }
    }
    return true;
}

std::vector<ModelEntry> ModelManager::GetMissingRequired() const
{
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<ModelEntry> missing;
    for (const auto& e : entries)
    {
        if (e.required && !e.installed)
        {
            missing.push_back(e);
        }
    }
    return missing;
}

void ModelManager::ClearLastFailure()
{
    std::lock_guard<std::mutex> lock(mutex);
    last_failed_file.clear();
    last_expected_sha.clear();
    last_received_sha.clear();
    last_expected_bytes = 0;
    last_received_bytes = 0;
    last_received_known = false;
    last_fail_message.clear();
}

void ModelManager::SetLastFailure(std::string_view file, std::string_view expected_sha,
                                  std::string_view received_sha, uint64_t expected_bytes,
                                  uint64_t received_bytes, bool received_known,
                                  std::string_view message)
{
    std::lock_guard<std::mutex> lock(mutex);
    last_failed_file = std::string(file);
    last_expected_sha = std::string(expected_sha);
    last_received_sha = std::string(received_sha);
    last_expected_bytes = expected_bytes;
    last_received_bytes = received_bytes;
    last_received_known = received_known;
    last_fail_message = std::string(message);
}

bool ModelManager::GetLastFailure(std::string& failed_file, std::string& expected_sha256,
                                  std::string& received_sha256, uint64_t& expected_bytes,
                                  uint64_t& received_bytes, bool& received_known,
                                  std::string& message) const
{
    std::lock_guard<std::mutex> lock(mutex);
    if (last_failed_file.empty())
    {
        return false;
    }
    failed_file = last_failed_file;
    expected_sha256 = last_expected_sha;
    received_sha256 = last_received_sha;
    expected_bytes = last_expected_bytes;
    received_bytes = last_received_bytes;
    received_known = last_received_known;
    message = last_fail_message;
    return true;
}

bool ModelManager::IsModelOrBundleInstalled(std::string_view id) const
{
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto& e : entries)
    {
        if (e.id == id && e.installed)
        {
            return true;
        }
    }
    return false;
}

bool ModelManager::IsBusy() const
{
    return task.load() != ModelTask::None;
}

float ModelManager::GetTaskProgress() const
{
    const uint64_t total = task_total.load();
    if (total == 0)
    {
        return 0.0f;
    }
    float p = static_cast<float>(task_done.load()) / static_cast<float>(total);
    return p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
}

std::string ModelManager::GetTaskLabel() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return task_label;
}

void ModelManager::CancelTask()
{
    cancel_requested.store(true);
}

void ModelManager::StartTask(ModelTask t, std::string_view label)
{
    if (worker.joinable())
    {
        worker.join();
    }
    task_done.store(0);
    task_total.store(0);
    cancel_requested.store(false);
    {
        std::lock_guard<std::mutex> lock(mutex);
        task_label = std::string(label);
    }
    ClearLastFailure();
    SetTaskResult(TaskResult::Unknown);
    task.store(t);
}

void ModelManager::VerifyAsync(std::string_view id)
{
    if (IsBusy())
    {
        return;
    }
    StartTask(ModelTask::Verify, "Verifying " + std::string(id) + "...");
    worker = std::thread(&ModelManager::RunVerify, this, std::string(id));
}

void ModelManager::ImportAsync(std::string_view source_path, std::string_view id)
{
    if (IsBusy())
    {
        return;
    }
    StartTask(ModelTask::Import, "Importing " + std::string(id) + "...");
    worker = std::thread(&ModelManager::RunImport, this, std::string(source_path), std::string(id));
}

void ModelManager::DeleteAsync(std::string_view id)
{
    if (IsBusy())
    {
        return;
    }
    StartTask(ModelTask::Delete, "Deleting " + std::string(id) + "...");
    worker = std::thread(&ModelManager::RunDelete, this, std::string(id));
}

void ModelManager::DownloadAsync(std::string_view id)
{
    if (IsBusy())
    {
        return;
    }
    StartTask(ModelTask::Download, "Downloading " + std::string(id) + "...");
    worker = std::thread(&ModelManager::RunDownload, this, std::string(id));
}

void ModelManager::RunDownload(std::string id)
{
    std::string fail;
    ModelEntry e;
    if (!find_copy(id, e))
    {
        fail = "unknown model id";
    }
    else if (e.repo.empty())
    {
        fail = "no Hugging Face repository in registry";
    }
    else
    {
        std::string token;
        HFAuthenticator::LoadToken(token); // Windows Credential Manager; may be empty for public repos

        const auto user_dir = AppDataDir() / "models";
        std::error_code ec;
        std::filesystem::create_directories(user_dir, ec);

        std::vector<BundleFile> target_files = e.files;
        if (target_files.empty() && !e.remote_path.empty())
        {
            target_files.push_back({e.remote_path, e.motion_file, e.size_bytes, e.Sha256});
        }

        if (target_files.empty())
        {
            fail = "no downloadable files specified in registry";
        }
        else
        {
            // Calculate total expected bytes across bundle
            uint64_t total_expected = 0;
            for (const auto& bf : target_files)
            {
                total_expected += bf.size_bytes;
            }
            if (total_expected == 0)
            {
                total_expected = e.size_bytes;
            }
            task_total.store(total_expected);

            uint64_t overall_done = 0;
            bool all_succeeded = true;

            for (size_t i = 0; i < target_files.size(); ++i)
            {
                if (cancel_requested.load())
                {
                    fail = "Download cancelled";
                    all_succeeded = false;
                    break;
                }

                const auto& bf = target_files[i];
                const std::filesystem::path target_path = (target_files.size() > 1)
                    ? (user_dir / e.motion_file / bf.local_rel_path)
                    : (user_dir / bf.local_rel_path);

                std::filesystem::create_directories(target_path.parent_path(), ec);
                const std::filesystem::path part_path = target_path.string() + ".part";

                // Verify any pre-existing final file before skipping: size AND SHA-256.
                // A same-size-corrupt file must be replaced, never trusted.
                if (std::filesystem::is_regular_file(target_path, ec) && !ec)
                {
                    auto cur_sz = std::filesystem::file_size(target_path, ec);
                    if (!ec && (bf.size_bytes == 0 || cur_sz == bf.size_bytes))
                    {
                        if (!bf.sha256.empty())
                        {
                            std::string verr;
                            const std::string vdigest = FileHash::Sha256(target_path.string(), verr);
                            if (!verr.empty() || vdigest != bf.sha256)
                            {
                                Logger::GetInstance().Warning(
                                    "ModelManager: discarding corrupt pre-existing file " +
                                    target_path.string() + " (expected sha256 " + bf.sha256 + ")");
                                std::filesystem::remove(target_path, ec);
                            }
                            else
                            {
                                overall_done += (bf.size_bytes > 0 ? bf.size_bytes : cur_sz);
                                task_done.store(overall_done);
                                continue;
                            }
                        }
                        else if (bf.size_bytes > 0 && cur_sz == bf.size_bytes)
                        {
                            overall_done += bf.size_bytes;
                            task_done.store(overall_done);
                            continue;
                        }
                    }
                    else
                    {
                        // Size mismatch: stale/incomplete final. Remove so the
                        // atomic rename below cannot fail on Windows.
                        Logger::GetInstance().Warning("ModelManager: discarding size-mismatched file " +
                                                      target_path.string());
                        std::filesystem::remove(target_path, ec);
                    }
                }

                std::string file_label = "Downloading " + e.name;
                if (target_files.size() > 1)
                {
                    file_label += " (" + std::to_string(i + 1) + "/" + std::to_string(target_files.size()) + ": " + bf.local_rel_path + ")";
                }
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    task_label = file_label;
                }

                const std::string url = HuggingFaceClient::ResolveUrl(e.repo, bf.remote_path);
                if (bf.remote_path.empty())
                {
                    fail = "Registry error: empty remotePath for " + bf.local_rel_path +
                           " (expected HF path, local destination " + target_path.string() + ")";
                    all_succeeded = false;
                    break;
                }
                Logger::GetInstance().Info("ModelManager: downloading " + e.repo + "/" + bf.remote_path +
                                           " -> " + target_path.string());
                uint64_t file_prev_done = 0;

                auto progress_cb = [this, &overall_done, &file_prev_done](uint64_t done, uint64_t total) {
                    (void)total;
                    uint64_t delta = (done > file_prev_done) ? (done - file_prev_done) : 0;
                    file_prev_done = done;
                    overall_done += delta;
                    task_done.store(overall_done);
                    return !cancel_requested.load();
                };

                auto run_once = [&](std::string& derr) {
                    return HuggingFaceClient::download(url, part_path.string(), token, progress_cb, derr, 3,
                                                       bf.size_bytes);
                };

                bool ok = run_once(fail);
                if (!ok && fail == "server ignored resume; retrying")
                {
                    // Client already removed the stale .part; retry once fresh.
                    file_prev_done = 0;
                    Logger::GetInstance().Warning("ModelManager: server ignored Range for " +
                                                  bf.local_rel_path + "; restarting without resume");
                    ok = run_once(fail);
                }
                if (!ok && fail.find("incomplete download") != std::string::npos && bf.size_bytes > 0 &&
                    !cancel_requested.load())
                {
                    // Resume-hostile path: repeated open-ended attempts keep dying
                    // early. Fall back to explicit bounded byte-range chunks.
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        task_label = "Retrying with chunked download: " + bf.local_rel_path + "...";
                    }
                    Logger::GetInstance().Warning("ModelManager: falling back to chunked download for " +
                                                  bf.local_rel_path + " (" + fail + ")");
                    std::error_code sec2;
                    file_prev_done = std::filesystem::file_size(part_path, sec2);
                    if (sec2)
                    {
                        file_prev_done = 0;
                    }
                    std::string chunk_err;
                    if (HuggingFaceClient::download_chunked(url, part_path.string(), token, progress_cb,
                                                            chunk_err, bf.size_bytes))
                    {
                        ok = true;
                        fail.clear();
                    }
                    else
                    {
                        fail = chunk_err + " [after standard download failed: " + fail + "]";
                    }
                }

                if (!ok)
                {
                    all_succeeded = false;
                    if (fail == "cancelled")
                    {
                        fail = "Download cancelled (resumable .part saved)";
                    }
                    else if (fail.empty())
                    {
                        fail = "Download failed for " + bf.local_rel_path;
                    }
                    break;
                }

                // Size gate before hashing: a short .part is an incomplete
                // download, not corruption. Keep it for resume and report byte
                // counts distinctly from a full-size checksum mismatch.
                {
                    std::error_code sec;
                    auto part_sz = std::filesystem::file_size(part_path, sec);
                    if (!sec && bf.size_bytes > 0 && part_sz != bf.size_bytes)
                    {
                        fail = "Incomplete download of " + bf.local_rel_path + " (" +
                               std::to_string(part_sz) + " of " + std::to_string(bf.size_bytes) +
                               " bytes); .part kept, resume on retry";
                        SetLastFailure(bf.local_rel_path, bf.sha256, {}, bf.size_bytes, part_sz,
                                       true, fail);
                        Logger::GetInstance().Warning("ModelManager: " + fail);
                        all_succeeded = false;
                        break;
                    }
                }

                // Verify checksum of .part file before atomic rename
                if (!bf.sha256.empty())
                {
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        task_label = "Verifying checksum: " + bf.local_rel_path + "...";
                    }
                    std::string err;
                    std::string digest = FileHash::Sha256(part_path.string(), err);
                    std::error_code sec;
                    auto part_sz = std::filesystem::file_size(part_path, sec);
                    if (!err.empty() || digest != bf.sha256)
                    {
                        std::string detail = "Checksum mismatch on " + bf.local_rel_path +
                                             " (expected sha256 " + bf.sha256 + ", received " +
                                             (digest.empty() ? "<hash failed: " + err + ">" : digest) +
                                             ", expected " + std::to_string(bf.size_bytes) + " bytes" +
                                             (sec ? "" : ", received " + std::to_string(part_sz) + " bytes") +
                                             "); file discarded";
                        SetLastFailure(bf.local_rel_path, bf.sha256, digest, bf.size_bytes,
                                       sec ? 0 : part_sz, !static_cast<bool>(sec), detail);
                        std::filesystem::remove(part_path, ec);
                        fail = detail;
                        all_succeeded = false;
                        break;
                    }
                }

                // Atomic install: remove any stale final first (Windows rename
                // fails when the destination exists), then rename .part.
                std::filesystem::remove(target_path, ec);
                ec.clear();
                std::filesystem::rename(part_path, target_path, ec);
                if (ec)
                {
                    std::filesystem::remove(part_path, ec);
                    fail = "Atomic install failed: " + ec.message();
                    all_succeeded = false;
                    break;
                }
            }

            if (all_succeeded)
            {
                fail.clear();
                Logger::GetInstance().Info("Model/Bundle " + id + " downloaded and verified successfully");
            }
        }
    }
    Rescan();
    {
        std::lock_guard<std::mutex> lock(mutex);
        task_label = fail.empty() ? ("Installed " + id) : fail;
    }
    // Explicit outcome first: SetupManager branches on this, never on labels.
    SetTaskResult(fail.empty() ? TaskResult::Ok : TaskResult::Failed);
    task.store(ModelTask::None);
}

void ModelManager::RunVerify(std::string id)
{
    ModelEntry e;
    std::string fail = "entry missing";
    if (find_copy(id, e) && e.installed)
    {
        fail.clear();
        std::vector<BundleFile> target_files = e.files;
        if (target_files.empty() && !e.Sha256.empty())
        {
            target_files.push_back({"", e.motion_file, e.size_bytes, e.Sha256});
        }

        uint64_t total_bytes = 0;
        for (const auto& bf : target_files) total_bytes += bf.size_bytes;
        task_total.store(total_bytes);
        uint64_t verified_done = 0;

        for (const auto& bf : target_files)
        {
            if (bf.sha256.empty()) continue;

            std::filesystem::path fp = (target_files.size() > 1)
                ? (std::filesystem::path(e.local_path) / bf.local_rel_path)
                : std::filesystem::path(e.local_path);

            std::string error;
            const std::string digest = FileHash::Sha256(fp.string(), error, [this, &verified_done](uint64_t done, uint64_t total) {
                (void)total;
                task_done.store(verified_done + done);
            });
            verified_done += bf.size_bytes;

            std::error_code sec;
            auto actual_sz = std::filesystem::file_size(fp, sec);
            if (digest.empty())
            {
                fail = "hash failed on " + bf.local_rel_path + ": " + error;
                SetLastFailure(bf.local_rel_path, bf.sha256, {}, bf.size_bytes,
                               sec ? 0 : actual_sz, !static_cast<bool>(sec), fail);
                break;
            }
            else if (digest != bf.sha256)
            {
                fail = "CHECKSUM MISMATCH on " + bf.local_rel_path + " (expected sha256 " +
                       bf.sha256 + ", received " + digest + ")";
                SetLastFailure(bf.local_rel_path, bf.sha256, digest, bf.size_bytes,
                               sec ? 0 : actual_sz, !static_cast<bool>(sec), fail);
                Logger::GetInstance().Error("Model " + id + " file " + bf.local_rel_path + " checksum mismatch");
                break;
            }
        }
        if (fail.empty())
        {
            Logger::GetInstance().Info("Model/Bundle " + id + " checksums OK");
        }
    }
    {
        std::lock_guard<std::mutex> lock(mutex);
        task_label = fail.empty() ? ("Verified " + id + ": checksum OK") : fail;
    }
    SetTaskResult(fail.empty() ? TaskResult::Ok : TaskResult::Failed);
    task.store(ModelTask::None);
}

void ModelManager::RunImport(std::string source_path, std::string id)
{
    std::string fail;
    ModelEntry e;
    if (!find_copy(id, e))
    {
        fail = "unknown model id";
    }
    else if (!e.files.empty() && e.files.size() > 1)
    {
        // Multi-file bundle import: source_path must be a directory holding
        // every required component file. Each file is verified (size + SHA-256)
        // and installed atomically; the bundle is marked installed only when
        // ALL files verify.
        const auto user_dir = AppDataDir() / "models";
        const auto dest_dir = user_dir / e.motion_file;
        std::error_code ec;
        std::filesystem::create_directories(dest_dir, ec);
        task_total.store(e.size_bytes);
        uint64_t done_total = 0;
        for (const auto& bf : e.files)
        {
            if (cancel_requested.load())
            {
                fail = "Import cancelled";
                break;
            }
            const auto src_file = std::filesystem::path(source_path) / bf.local_rel_path;
            const auto dest_file = dest_dir / bf.local_rel_path;
            const auto tmp = dest_file.string() + ".download";
            std::filesystem::create_directories(dest_file.parent_path(), ec);
            std::ifstream src(src_file, std::ios::binary);
            std::ofstream dst(tmp, std::ios::binary | std::ios::trunc);
            if (!src || !dst)
            {
                std::filesystem::remove(tmp, ec);
                fail = "cannot open bundle source file: " + bf.local_rel_path;
                break;
            }
            std::vector<char> buf(1 << 20);
            bool ok = true;
            while (src)
            {
                src.read(buf.data(), static_cast<std::streamsize>(buf.size()));
                const auto n = src.gcount();
                if (n > 0)
                {
                    dst.write(buf.data(), n);
                    done_total += static_cast<uint64_t>(n);
                    task_done.store(done_total);
                }
            }
            ok = static_cast<bool>(src.eof()) && static_cast<bool>(dst);
            dst.close();
            if (!ok)
            {
                std::filesystem::remove(tmp, ec);
                fail = "copy failed for " + bf.local_rel_path;
                break;
            }
            std::string error;
            const std::string digest = FileHash::Sha256(tmp, error);
            auto sz = std::filesystem::file_size(tmp, ec);
            if (!bf.sha256.empty() && (!error.empty() || digest != bf.sha256))
            {
                std::filesystem::remove(tmp, ec);
                fail = "CHECKSUM MISMATCH on " + bf.local_rel_path + "; file discarded";
                break;
            }
            if (bf.size_bytes > 0 && (ec || sz != bf.size_bytes))
            {
                std::filesystem::remove(tmp, ec);
                fail = "Size mismatch on " + bf.local_rel_path + "; file discarded";
                break;
            }
            std::filesystem::remove(dest_file, ec);
            ec.clear();
            std::filesystem::rename(tmp, dest_file, ec);
            if (ec)
            {
                std::filesystem::remove(tmp, ec);
                fail = "atomic install failed for " + bf.local_rel_path + ": " + ec.message();
                break;
            }
        }
    }
    else
    {
        // Imports land in the user model dir, never in the repo checkout.
        const auto user_dir = AppDataDir() / "models";
        const auto dest = user_dir / e.motion_file;
        std::error_code ec;
        std::filesystem::create_directories(dest.parent_path(), ec);
        const auto tmp = dest.string() + ".download";
        std::ifstream src(source_path, std::ios::binary);
        std::ofstream dst(tmp, std::ios::binary | std::ios::trunc);
        if (!src || !dst)
        {
            fail = "cannot open source or destination";
        }
        else
        {
            src.seekg(0, std::ios::end);
            const auto total = src.tellg();
            src.seekg(0, std::ios::beg);
            task_total.store(static_cast<uint64_t>(total));
            std::vector<char> buf(1 << 20);
            uint64_t done = 0;
            bool ok = true;
            while (src)
            {
                src.read(buf.data(), static_cast<std::streamsize>(buf.size()));
                const auto n = src.gcount();
                if (n > 0)
                {
                    dst.write(buf.data(), n);
                    done += static_cast<uint64_t>(n);
                    task_done.store(done);
                }
            }
            ok = static_cast<bool>(src.eof()) && static_cast<bool>(dst);
            dst.close();
            if (!ok)
            {
                std::filesystem::remove(tmp, ec);
                fail = "copy failed (interrupted?)";
            }
            else
            {
                // Verify before atomic install (plan section 41).
                std::string error;
                const std::string digest = FileHash::Sha256(tmp, error);
                if (!e.Sha256.empty() && digest != e.Sha256)
                {
                    std::filesystem::remove(tmp, ec);
                    fail = "CHECKSUM MISMATCH; incomplete file discarded";
                }
                else
                {
                    std::filesystem::remove(dest, ec);
                    ec.clear();
                    std::filesystem::rename(tmp, dest, ec);
                    if (ec)
                    {
                        std::filesystem::remove(tmp, ec);
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
    SetTaskResult(fail.empty() ? TaskResult::Ok : TaskResult::Failed);
    task.store(ModelTask::None);
}

void ModelManager::RunDelete(std::string id)
{
    ModelEntry e;
    std::string fail = "entry missing";
    if (find_copy(id, e) && e.installed)
    {
        std::error_code ec;
        // Only delete files inside known model dirs (never arbitrary paths).
        const auto p = std::filesystem::path(e.local_path);
        const auto user_models = (AppDataDir() / "models").string();
        const bool is_bundle = !e.files.empty() && e.files.size() > 1;
        const std::string scope_dir = is_bundle ? p.string() : p.parent_path().string();
        const bool known_dir = scope_dir == model_dir || scope_dir == user_models;
        if (!known_dir)
        {
            fail = "refusing to delete outside model dirs";
        }
        else if (is_bundle)
        {
            // Remove every bundle component + stray .part files, then the dir.
            for (const auto& bf : e.files)
            {
                std::filesystem::remove(p / bf.local_rel_path, ec);
                std::filesystem::remove(p / (bf.local_rel_path + ".part"), ec);
            }
            ec.clear();
            std::filesystem::remove(p, ec); // removes dir if now empty; ignore failure
            fail.clear();
            if (active_id == id)
            {
                active_id.clear();
            }
        }
        else if (!std::filesystem::remove(p, ec) || ec)
        {
            fail = "delete failed: " + ec.message();
        }
        else
        {
            fail.clear();
            if (active_id == id)
            {
                active_id.clear();
            }
        }
    }
    Rescan();
    {
        std::lock_guard<std::mutex> lock(mutex);
        task_label = fail.empty() ? ("Deleted " + id) : fail;
    }
    SetTaskResult(fail.empty() ? TaskResult::Ok : TaskResult::Failed);
    task.store(ModelTask::None);
}

} // namespace studio
