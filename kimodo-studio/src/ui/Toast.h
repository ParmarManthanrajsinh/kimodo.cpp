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
    void push(const std::string& text, ToastKind kind = ToastKind::Info);
    void draw();

private:
    std::vector<Toast> items_;
};

} // namespace studio
