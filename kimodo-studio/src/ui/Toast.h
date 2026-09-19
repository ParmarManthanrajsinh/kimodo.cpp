#pragma once

#include <string>
#include <vector>

namespace studio {

enum class ToastKind { Info, Success, Warning, Error };

struct Toast {
    std::string text;
    ToastKind kind = ToastKind::Info;
    double expiresAt = 0.0;
};

// Transient notifications (plan section 38). UI thread only:
// push from event sites, draw once per frame.
class Toasts {
public:
    void Push(const std::string& text, ToastKind kind = ToastKind::Info);
    void Draw();

private:
    std::vector<Toast> items;
};

} // namespace studio
