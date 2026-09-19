#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Thin RAII wrapper over kimodo C API (include/kimodo/kimodo_capi.h).
// All calls are synchronous and thread-safe at object level: caller
// runs them on a worker thread, never on UI thread.
namespace studio {

struct GenerationParams {
    uint64_t seed = 42;
    uint32_t frames = 120;
    uint32_t steps = 50;
    float text_cfg = 2.0f;
    float constraint_cfg = 2.0f;
};

struct MotionResult {
    int frames = 0;
    int joints = 0;
    std::vector<float> local_rotations_xyzw; // [frames, joints, 4]
    std::vector<float> root_positions;       // [frames, 3]
};

class KimodoAdapter {
public:
    KimodoAdapter() = default;
    ~KimodoAdapter();

    KimodoAdapter(const KimodoAdapter&) = delete;
    KimodoAdapter& operator=(const KimodoAdapter&) = delete;

    static int AbiVersion();

    bool Load(const std::string& motion_gguf, const std::string& text_bundle, std::string& error);
    bool IsLoaded() const { return loaded; }

    // progress(done, total) runs on the worker thread; return true to cancel.
    using ProgressFn = std::function<bool(unsigned done, unsigned total)>;

    bool Generate(const std::string& prompt, const GenerationParams& params, MotionResult& out, std::string& error,
                  ProgressFn progress = {});
    void Unload();

    const std::string& GetLastError() const { return last_error; }

private:
    struct handle;
    handle* HandlePtr = nullptr;
    bool loaded = false;
    std::string last_error;
};

} // namespace studio
