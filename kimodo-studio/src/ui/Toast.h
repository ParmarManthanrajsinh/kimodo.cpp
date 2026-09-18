#pragma once

#include <string>
#include <vector>

namespace studio {

enum class EToastKind { Info, Success, Warning, Error };

struct FToast {
    std::string text;
    EToastKind kind = EToastKind::Info;
    double expiresAt = 0.0;
};

// Transient notifications (plan section 38). UI thread only:
// push from event sites, draw once per frame.
class SToasts {
public:
    void Push(const std::string& text, EToastKind kind = EToastKind::Info);
    void Draw();

private:
    std::vector<FToast> items;
};

} // namespace studio
