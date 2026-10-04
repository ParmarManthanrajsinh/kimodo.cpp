# Coding Style & Architecture

## Formatting & Mechanics
- **Indentation**: Tabs for indentation.
- **Braces**: Allman brace style (opening brace on next line).
- **Line Length**: Keep lines readable and bounded (~100-120 columns).

## Naming Conventions

### Standard C++ / Native Engine (Kimodo, Raylib, Desktop)
- Variables, members, parameters: `snake_case` (`frame_count`, `file_path`).
- Types, Structs, Classes, Enums: `PascalCase` (`Animation`, `Skeleton`).
- Functions & Methods: `PascalCase` (`LoadModel()`, `UpdateKinematics()`).
- Avoid Hungarian notation and `m_` prefixes.

### Unreal Engine Projects
- Follow strict Unreal Engine standard: `PascalCase` everywhere.
- Prefixes: `A` (Actor), `U` (Object/Component), `F` (Struct), `E` (Enum), `I` (Interface), `b` (Boolean).

## Clean, Readable, Fast & Extensible Design
- **Readability**: Code should read like well-crafted prose. Self-documenting identifier names beat excessive comments.
- **Comments**: Explain **WHY**, never WHAT. Only comment non-obvious algorithms, architectural decisions, or hardware-specific quirks.
- **Const Correctness**:
  - Mark read-only member functions `const`.
  - Pass read-only views with `std::string_view`, `std::span`, or `const Type&`.
  - Range-for loops over containers always use `const auto&`.
- **Fast Build Times**:
  - Forward declare types in headers whenever possible.
  - Keep `#include` dependencies strictly minimal.
- **Extensibility**:
  - Favor composition over inheritance.
  - Design clean module boundaries and well-defined interfaces.
  - Do not over-abstract or introduce speculative flexibility before requirements demand it.
