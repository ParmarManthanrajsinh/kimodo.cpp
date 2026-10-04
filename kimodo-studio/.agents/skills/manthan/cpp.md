# Modern C++ Guidelines

Preferred standard: C++20 / C++23

## Core Modern Practices

### Views & Zero-Copy Passing
- Prefer `std::string_view` for read-only string arguments (lookups, parsing, logging, hashing).
  - Keeps out-parameters as `std::string&` (or `std::expected` / result structs) for output buffers.
  - Sinks convert views via `std::string(sv)` or accept `std::string` by value + `std::move`.
- Use `std::span` for contiguous array/buffer views (`std::span<const T>`) without heap allocations or container coupling.
- Pass paths using `const std::filesystem::path&` or `std::string_view` at system boundaries.

### Attributes & Intent
- Use `[[nodiscard]]` on getters, query methods, validators, and pure functions where discarding the return value indicates a logic flaw.
- Mark non-throwing helpers, math conversions, trivial lookups, and move constructors `noexcept`.
- Use `constexpr` and `consteval` aggressively for compile-time constants and lookup tables.

### Types & Ownership (RAII)
- Explicit ownership: single clear owner per resource.
- Use `std::unique_ptr` for unique ownership; never expose raw owning pointers.
- Use `std::shared_ptr` only when ownership is genuinely shared.
- Prefer standard vocabulary types: `std::optional`, `std::variant`, `std::array`, `std::vector`.
- In-class member initialization: default member initializers (`= {}`, `= false`, `= 0.0f`, `= nullptr`) on all members.
- Single-argument constructors marked `explicit`.

### Clean, Fast & Extensible Code
- One class, one responsibility. No god objects.
- Keep headers lightweight: forward declarations over heavy `#include`s to ensure fast build times.
- Range-based loops with `const auto&` to eliminate accidental copies.
- Structured bindings (`auto [key, val] = ...`) for clear container unpacks.
- Avoid heavy template metaprogramming tricks; prefer clean, idiomatic, and readable designs that compile fast and debug easily.
