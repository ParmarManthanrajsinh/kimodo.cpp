#include "kimodo/KimodoAdapter.h"

#ifdef KIMODO_HAVE_BACKEND
#include <kimodo/kimodo_capi.h>
#endif

#include <cstring>

namespace studio {

struct KimodoAdapter::Handle {
#ifdef KIMODO_HAVE_BACKEND
    kimodo_model* model = nullptr;
#endif
};

KimodoAdapter::~KimodoAdapter() {
    unload();
}

int KimodoAdapter::abiVersion() {
#ifdef KIMODO_HAVE_BACKEND
    return kimodo_abi_version();
#else
    return -1;
#endif
}

bool KimodoAdapter::load(const std::string& motionGguf, const std::string& textBundle,
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
        lastError_ = error;
        delete h;
        return false;
    }
    handle_ = h;
    loaded_ = true;
    return true;
#else
    error = "Kimodo backend not linked (rebuild with VS2022 preset on MSVC)";
    lastError_ = error;
    return false;
#endif
}

bool KimodoAdapter::generate(const std::string& prompt, const GenerationParams& params,
                              MotionResult& out, std::string& error) {
#ifdef KIMODO_HAVE_BACKEND
    if (!loaded_ || !handle_ || !handle_->model) {
        error = "model not loaded";
        lastError_ = error;
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
    kimodo_motion* motion =
        kimodo_generate(handle_->model, prompt.c_str(), &opts, err, sizeof(err));
    if (!motion) {
        error = err[0] ? err : "kimodo_generate failed";
        lastError_ = error;
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
    lastError_ = error;
    return false;
#endif
}

void KimodoAdapter::unload() {
#ifdef KIMODO_HAVE_BACKEND
    if (handle_) {
        if (handle_->model) {
            kimodo_model_free(handle_->model);
        }
        delete handle_;
        handle_ = nullptr;
    }
#endif
    loaded_ = false;
}

} // namespace studio
