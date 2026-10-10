# Code Style

These conventions apply to PER-owned code in the monorepo.
Shared principles apply across languages.
The firmware C and Rust sections define their respective language rules.

These rules are often specific examples rather than an exhaustive list.
Their underlying principles should be applied broadly, with the overarching goal of writing consistent, safe, clean, simple, understandable, and maintainable code.


## Shared Principles

- Use descriptive names and keep functions focused on one responsibility. Short names such as `i` are appropriate for small loop counters.
- Prefer early returns and guard clauses to deeply nested control flow. Keep nesting to at most four levels; extract a helper when necessary. Release locks and other resources on every exit path.
- Include units in names where they would otherwise be ambiguous, such as `last_rx_time_ms`. Document units and conversions at API and protocol boundaries. Where possible, enforce proper usage via types.
- Replace magic numbers with named constants or enum variants. Explain constants whose meaning comes from hardware, a protocol, or a physical limit.
- Handle errors deliberately. Propagate them or report them where recovery is possible; do not silently discard faults or substitute misleading values.
- Update affected in-repository documentation in the same PR as behavior changes. Comments should explain intent, assumptions, and constraints.
- Let the language formatter control whitespace and wrapping.

## Firmware C

These rules apply to embedded firmware under `firmware/`.
Its STM32 targets use `arm-none-eabi-gcc` and C23, including `bool`, `nullptr`, `constexpr`, and `static_assert`.

Firmware allocation and naming rules do not apply to Rust or Python host tools.

### Memory and Types

- Dynamic memory allocation is forbidden in embedded firmware. Use compile-time storage or bounded stack allocations. Allocate FreeRTOS tasks, queues, and synchronization objects statically.
- Use `float`, `char`, `bool`, and fixed-width integer types from `<stdint.h>` for application values. Use `int32_t` or `uint16_t` over architecture-dependent `int` or `short`.
- Initialize variables explicitly, including globals that would otherwise be zero-initialized. Use `{0}` for zero-initialized aggregate values.
- Size buffers and task stacks for their worst-case use. Account for the 32-bit target architecture when reasoning about pointers and memory layout.

### Naming and Constants

- Use `snake_case` for variables and functions. Prefix globals with `g_` and put
  units at the end of variable names when applicable.
- Prefix/"namespace" shared-library functions with the library name in uppercase, for example `NXT_set_command`. Node-local functions use names such as `poll_shock_pots`.
- Use `SCREAMING_SNAKE_CASE` for macros, with a library prefix when applicable.
- Prefer typed `static constexpr` constants to macros. Use C23 digit separators to make large numbers readable.

```c
static constexpr uint32_t target_core_clock_rate_hz = 16'000'000;
static uint32_t g_last_rx_time_ms = 0;
```

### Control Flow and Macros

Always use braces for control statements, including single-statement bodies.

Prefer inline functions over function-like macros for type checking and single evaluation of arguments.

If a macro is necessary, parenthesize each argument and the complete expression:

```c
#define MAX_VALUE(a, b) (((a) > (b)) ? (a) : (b))
```

Parentheses protect expression precedence. Never pass side effects such as `i++` or a hardware read to it.

An inline function avoids the problems:

```c
static inline uint32_t max_u32(uint32_t a, uint32_t b) {
    return a > b ? a : b;
}
```

### Structs and Enums

Use `typedef` and the `_t` suffix for structs and enums. Group struct members by descending size/alignment when it reduces padding, with exceptions for logical grouping or a required layout. Exact sizes depend on the target ABI; do not assume reordering alone guarantees a wire-compatible layout.

```c
typedef struct {
    uint16_t small_num;
    uint32_t medium_num;
    uint64_t big_num;
    uint16_t small_num2;
} mixed_layout_t;

// Grouping members can reduce padding compared with mixed_layout_t.
typedef struct {
    uint64_t big_num;
    uint32_t medium_num;
    uint16_t small_num;
    uint16_t small_num2;
} grouped_layout_t;
```

Give enums an explicit underlying type, especially in structs or protocol data.
Prefix enumerators with the enum name and prefer explicit values:

```c
typedef enum : uint8_t {
    FAULT_STATE_CLEAR = 0,
    FAULT_STATE_PENDING = 1,
    FAULT_STATE_LATCHED = 2,
    FAULT_STATE_RECOVERING = 3,
} fault_state_t;
```

### Pointer Contracts and Errors

Document whether pointer arguments may be null. Validate nullable or untrusted arguments before dereferencing them, and return an error or report a fault when invalid input matters. A documented non-null precondition must be upheld by callers. Often silently returning from a void function is not a substitute for an error contract.

```c
// Returns false for an invalid output pointer; the caller must handle failure.
bool set_value(uint16_t *value) {
    if (value == nullptr) {
        return false;
    }
    *value = 10;
    return true;
}
```

## Rust

These conventions apply to all repository-owned Rust, including tests and examples. Host Rust may use heap allocation and standard collections.

### Error Handling and Panics

- Do not use `.unwrap()` or `.unwrap_err()`, including in tests and examples.
- Keep production `.expect()` and `.expect_err()` exceptional. An invariant established by the code must guarantee the required variant. Put the justification beside the call and explain the invariant in its message. Instead, where possible, handle recoverable errors with `Result` or `Option`, and propagate them with `?` or explicit matching. Or better, use the type system to prevent the error from being possible in the first place.
- External input, parsing user files, filesystem access, device operations, and channel communication require recoverable handling. They cannot justify an `expect` simply because failure is unlikely.
- Use `Result` for operations that can fail, `Option` for expected absence, and `?` or explicit matching to propagate or handle failures. Preserve useful error context.
- Tests may use descriptive `expect`/`expect_err` calls for setup and assertions. Prefer a test returning `Result` when `?` makes the intent clearer.
- Do not replace error handling with `panic!`, unchecked extraction, or unchecked indexing of external data. Assertions should express internal invariants.
- `unwrap_or`, `unwrap_or_else`, and `unwrap_or_default` are allowed when the fallback is meaningful. Do not conceal failures with defaults, `.ok()`, or ignored results. Explain intentional best-effort behavior, including when an error can safely be ignored during cleanup.

Propagate a recoverable file error instead of panicking:

```rust
fn read_log(path: &std::path::Path) -> std::io::Result<String> {
    let contents = std::fs::read_to_string(path)?;
    Ok(contents)
}
```

Use a default for expected absence, rather than for an unexplained failure:

```rust
fn log_folder(configured: Option<std::path::PathBuf>) -> std::path::PathBuf {
    // No configured folder means use the application's default log directory.
    configured.unwrap_or_else(|| std::path::PathBuf::from("logs"))
}
```

A justified `expect` explains why the value cannot fail. Prefer an infallible
constructor when one is available; this example illustrates the required invariant:

```rust
fn loopback_address() -> std::net::Ipv4Addr {
    // This source-controlled literal is valid IPv4, independent of external input.
    "127.0.0.1"
        .parse()
        .expect("hard-coded loopback literal should be a valid IPv4 address")
}
```

### Naming, Ownership, and APIs

- Use `snake_case` for functions, variables, modules, and fields; `UpperCamelCase` for types and enum variants; and `SCREAMING_SNAKE_CASE` for constants and statics. Do not carry C's library prefixes, `g_` prefix, or `_t` suffix into Rust naming.
- Prefer borrowing to unnecessary cloning. Accept `&str`, slices, or `&Path` when ownership is not required, and take ownership explicitly when it is.
- Use enums to express distinct states instead of ambiguous combinations of flags. Keep APIs and abstractions focused on the actual use case.
- Use explicit widths for wire data and `usize` for collection sizes and indices. Explain intentional lossy numeric conversions.
- Document public contracts, units, error cases, and meaningful panic conditions.
- Use the type system to prevent invalid states or values and to guarantee invariants. Additionally, use it to to make ownership clear and to prevent misuse of APIs.

### Concurrency and Safety

- Keep blocking I/O and expensive processing outside UI updates. Use channels to communicate with workers and make state ownership clear.
- Handle channel disconnection, cancellation, and worker shutdown deliberately. Avoid holding locks across blocking work; review lock ordering and exit paths.
- Only use safe Rust. No `unsafe` blocks.