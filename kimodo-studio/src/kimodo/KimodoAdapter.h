#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Thin RAII wrapper over kimodo C API (include/kimodo/kimodo_capi.h).
// All calls are synchronous and thread-safe at object level: caller
// runs them on a worker thread, never on UI thread.
namespace studio {

struct FGenerationParams {
    uint64_t seed = 42;
    uint32_t frames = 120;
    uint32_t steps = 50;
    float textCfg = 2.0f;
    float constraintCfg = 2.0f;
};

struct FMotionResult {
    int frames = 0;
    int joints = 0;
    std::vector<float> localRotationsXyzw; // [frames, joints, 4]
    std::vector<float> rootPositions;      // [frames, 3]
};

class FKimodoAdapter {
public:
    FKimodoAdapter() = default;
    ~FKimodoAdapter();

    FKimodoAdapter(const FKimodoAdapter&) = delete;
    FKimodoAdapter& operator=(const FKimodoAdapter&) = delete;

    static int abiVersion();

    bool load(const std::string& motionGguf, const std::string& textBundle,
              std::string& error);
    bool IsLoaded() const { return bLoaded; }

    // progress(done, total) runs on the worker thread; return true to cancel.
    using ProgressFn = std::function<bool(unsigned done, unsigned total)>;

    bool generate(const std::string& prompt, const FGenerationParams& params,
                  FMotionResult& out, std::string& error, ProgressFn progress = {});
    void unload();

    const std::string& GetLastError() const { return lastError; }

private:
    struct Handle;
    Handle* HandlePtr = nullptr;
    bool bLoaded = false;
    std::string lastError;
};

} // namespace studio
