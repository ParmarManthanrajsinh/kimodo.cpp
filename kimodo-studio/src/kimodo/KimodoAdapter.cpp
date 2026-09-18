#include "kimodo/KimodoAdapter.h"

#ifdef KIMODO_HAVE_BACKEND
#include <kimodo/kimodo_capi.h>
#endif

#include <cstring>

namespace studio {

struct FKimodoAdapter::Handle {
#ifdef KIMODO_HAVE_BACKEND
    kimodo_model* model = nullptr;
#endif
};

FKimodoAdapter::~FKimodoAdapter() {
    unload();
}

int FKimodoAdapter::abiVersion() {
#ifdef KIMODO_HAVE_BACKEND
    return kimodo_abi_version();
#else
    return -1;
#endif
}

bool FKimodoAdapter::load(const std::string& motionGguf, const std::string& textBundle,
                          std::string& error) {
#ifdef KIMODO_HAVE_BACKEND
    unload();
    kimodo_runtime_options opts{};
    opts.size = sizeof(opts);
    opts.threads = 0; // runtime default
    opts.device = KIMODO_DEVICE_AUTO;
    opts.backend_dir = nullptr; // exe/library directory
    char err[1024] = {};
    Handle* h = new Handle();
    h->model = kimodo_model_load(motionGguf.c_str(), textBundle.c_str(), nullptr,
                                 &opts, err, sizeof(err));
    if (!h->model) {
        error = err[0] ? err : "kimodo_model_load failed";
        lastError = error;
        delete h;
        return false;
    }
    HandlePtr = h;
    bLoaded = true;
    return true;
#else
    error = "Kimodo backend not linked (rebuild with VS2022 preset on MSVC)";
    lastError = error;
    return false;
#endif
}

namespace {
#ifdef KIMODO_HAVE_BACKEND
int progressTrampoline(unsigned done, unsigned total, void* user) noexcept {
    auto* fn = static_cast<FKimodoAdapter::ProgressFn*>(user);
    try {
        return (*fn)(done, total) ? 1 : 0;
    } catch (...) {
        return 1; // never let exceptions cross the C boundary; treat as cancel
    }
}
#endif
} // namespace

bool FKimodoAdapter::generate(const std::string& prompt, const FGenerationParams& params,
                              FMotionResult& out, std::string& error,
                              ProgressFn progress) {
#ifdef KIMODO_HAVE_BACKEND
    if (!bLoaded || !HandlePtr || !HandlePtr->model) {
        error = "model not loaded";
        lastError = error;
        return false;
    }
    kimodo_generation_options opts{};
    opts.size = sizeof(opts);
    opts.seed = params.seed;
    opts.frames = params.frames;
    opts.diffusion_steps = params.steps;
    opts.text_cfg_weight = params.textCfg;
    opts.constraint_cfg_weight = params.constraintCfg;
    char err[1024] = {};
    kimodo_motion* motion = kimodo_generate_with_progress(
        HandlePtr->model, prompt.c_str(), &opts,
        progress ? &progressTrampoline : nullptr, progress ? &progress : nullptr, err,
        sizeof(err));
    if (!motion) {
        error = err[0] ? err : "kimodo_generate failed";
        lastError = error;
        return false;
    }
    out.frames = kimodo_motion_frames(motion);
    out.joints = kimodo_motion_joints(motion);
    if (const float* r = kimodo_motion_local_rotations_xyzw(motion)) {
        out.localRotationsXyzw.assign(
            r, r + static_cast<size_t>(out.frames) * out.joints * 4);
    }
    if (const float* p = kimodo_motion_root_positions(motion)) {
        out.rootPositions.assign(p, p + static_cast<size_t>(out.frames) * 3);
    }
    kimodo_motion_free(motion);
    return true;
#else
    error = "Kimodo backend not linked (rebuild with VS2022 preset on MSVC)";
    lastError = error;
    return false;
#endif
}

void FKimodoAdapter::unload() {
#ifdef KIMODO_HAVE_BACKEND
    if (HandlePtr) {
        if (HandlePtr->model) {
            kimodo_model_free(HandlePtr->model);
        }
        delete HandlePtr;
        HandlePtr = nullptr;
    }
#endif
    bLoaded = false;
}

} // namespace studio
