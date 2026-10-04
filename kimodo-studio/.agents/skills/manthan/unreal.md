# Unreal Engine Coding Standard & Architecture

Approach: Hybrid C++ + Blueprint (production-grade, scalable, high-performance)

## Division of Responsibilities
- **C++**:
  - Core gameplay mechanics, math, tick logic, state machines.
  - Performance-critical algorithms, networking/replication.
  - Base classes, interfaces, component architectures, subsystems.
- **Blueprint**:
  - UI (UMG widgets) and layout composition.
  - Materials, VFX (Niagara) hooks, audio cue wiring.
  - Rapid prototyping, level-specific sequencing, and data asset definitions.

## Unreal Naming Conventions
- `A` for Actors (`ACharacter`, `AWeapon`).
- `U` for UObjects & Components (`UActorComponent`, `UInventoryComponent`).
- `F` for Structs and non-UObject classes (`FHitResult`, `FItemData`).
- `T` for Templates (`TArray`, `TMap`, `TSet`, `TSubclassOf`).
- `I` for Interfaces (`IDamageableInterface`).
- `E` for Enums (`enum class EMovementState : uint8`).
- `S` for Slate widgets (`SCompoundWidget`).
- `b` prefix for booleans (`bIsAlive`, `bCanJump`).
- Functions & Variables: `PascalCase` across both C++ and Blueprints.

## Memory & Object Safety
- Use `TObjectPtr<T>` for `UPROPERTY()` member object references (UE5 standard).
- Use `TWeakObjectPtr<T>` for non-owning references to avoid dangling pointers and cycle leaks.
- Ensure all UObject pointers held by classes are properly marked with `UPROPERTY()` to participate in Garbage Collection.
- For non-UObject types, use `TSharedPtr<T>` / `TUniquePtr<T>`.

## Extensible Architecture
- **Interfaces (`UInterface`)**: Prefer interfaces over deep class hierarchies to decouple components and systems.
- **Gameplay Tags (`FGameplayTag`)**: Use gameplay tags for state flags, abilities, status effects, and category queries rather than hardcoded enums or strings.
- **Subsystems (`USubsystem`)**: Use Engine/GameInstance/World subsystems for modular singleton-like managers without global state.
- **Extension Hooks**: Use `BlueprintNativeEvent` for C++ default implementations that Blueprints can override, and `BlueprintCallable` for public API exposure.

## Performance & Optimization
- Use `FName` for fast hashed comparisons/lookups; use `FStringView` for zero-allocation string parsing.
- Avoid dynamic allocations and heavy reflection queries (`FindComponentByClass`) in hot paths or `Tick()`; cache pointers in `BeginPlay()`.
- Minimize `Tick()` usage: prefer event-driven logic, timers (`FTimerManager`), and delegates.
- Pass complex structs by `const FStructType&`.
