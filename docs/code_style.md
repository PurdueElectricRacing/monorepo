# Codestyle


## About

This document defines the coding style and rules for PER-owned code in the monorepo. The rules are organized in three parts:

1. **Shared** principles that apply to all languages
2. **Firmware/C** rules for the embedded firmware under `firmware/`
3. **Rust** rules for the DAQ workspace under `daq/`

These rules are often specific examples rather than an exhaustive list. Their underlying principles should be applied broadly, with the overarching goal of writing consistent, safe, clean, simple, understandable, and maintainable code.

The goal is to maintain a consistent and easily maintainable codebase. Following these rules reduces opportunities for hidden bugs and makes the code more readable and reviewable for others.

## Shared

These rules apply to all PER-owned code in the repository, regardless of language.

1. Avoid nesting where possible
    1. Try using early returns and condition inversion
        - Remember to free resources (like semaphore/mutex) before returning
    2. No more than 4 indentations deep
        - Extract a helper function if nesting is still too deep

    ```c
    void nesting_example_bad() {
    	if (condition1) {
    		if (condition2) {
    			// code here
    		}
    	}
    }
    
    void nesting_example_better() {
    	if (!condition1) {
    		return;
    	}
    	
    	if (!condition2) {
    		return;
    	}
    	// code here
    }
    ```

2. Use descriptive names
    1. Try to use full words wherever possible
    2. Keep it to a reasonable length
    3. The exception to this rule is temporary variables in for loops like `i` or `j`
    4. Variables with units should have their unit at the end of the name when it would otherwise be ambiguous
        - Document units and conversions at API and protocol boundaries, and where possible enforce proper usage via types

    ```c
    g_last_rx_time_ms = 0;
    ```

3. Avoid magic numbers
    1. Define a named constant, `enum`, or macro
    2. Explain constants whose meaning comes from hardware, a protocol, or a physical limit

4. Handle errors deliberately
    1. Propagate errors or report them where recovery is possible
    2. Do not silently discard faults or substitute misleading values

5. Keep documentation in sync
    1. Update affected in-repository documentation in the same PR as behavior changes
    2. Comments should explain intent, assumptions, and constraints, not restate the code

6. Generally follow the principle of least surprise
    1. Avoid unexpected side effects, hidden state, and non-obvious behavior
    2. Avoid unnecessary complexity, indirection, and abstraction

7. As a rule of thumb, it is best to follow the style of existing code.
    1. Match naming, formatting, and structure.

## Firmware C

The compiler standard for `firmware` is set to C23, enabling several important modern features:
- `bool`, `nullptr` as standard keywords
- `constexpr` for compile-time constants
- `static_assert` for compile-time assertions

Targets are STM32 MCUs (G4, F4) using the `arm-none-eabi-gcc` toolchain.
On this platform, 32 bit writes are atomic.

Firmware allocation and naming rules do not apply to Rust or Python host tools.

1. Use of dynamic memory allocation is forbidden
    1. Absolutely no `malloc()`, this will open the risk for memory fragmentation
    2. Instead, you must either allocate memory at compile time, or use stack-based local variables
    3. FreeRTOS tasks, queues, and synchronization objects must be created statically, not from the heap

2. Global variables
    1. Global variables should be prefixed with `g_`

    ```c
    g_global_var = 0;
    ```

3. Function names
    1. Library functions should be prefixed with the library name in caps

    ```c
    // Library: NXT
    void NXT_set_command();
    // Non-library function:
    void poll_shock_pots();
    ```

4. Macros
    1. Use `SCREAMING_SNAKE_CASE`
    2. Same rules as function names
    3. Wrap values with parenthesis to prevent expansion edge cases
    4. Never pass side effects like `i++` or hardware register reads to a macro, parenthesization does not prevent double evaluation

        ```c
        // bad
        #define ABS(x) ((x) < 0 ? (-1 * x) : x)
        // good
        #define ABS(x) ((x) < 0 ? (-1 * (x)) : (x))
        ```

5. Inline functions
    1. Use inline functions over macros wherever possible, they offer type checking and single evaluation of arguments
    2. Prefer typed `static constexpr` constants to macros for inherent type safety

    ```c
    static inline uint32_t max_u32(uint32_t a, uint32_t b) {
        return a > b ? a : b;
    }
    ```

6. Don't use brace-less control statements

    ```c
    // bad
    if (condition)
    	code();
    	
    // good
    if (condition) {
    	code();
    }
    ```

7. Allowed data types
    1. `float`
    2. `char`
    3. `bool` (standard keyword in C23)
    4. From `<stdint.h>`

    ```c
    // bad, size depends on architecture
    int num;
    short num2;
    
    // good, explicit sizes
    int32_t num;
    int16_t num2;
    ```

8. Always initialize variables with a value
    1. global variables should explicitly be zero'd out (even though the compiler does this for you)

    ```c
    type_t g_global_struct = {0};
    uint8_t g_global_val = 0;
    ```

9. `struct` declarations
    1. Always use `typedef` and suffix your new type with `_t`
    2. Order your variables in descending size (this saves space from padding)
    3. However exceptions can be made for logical/clarity purposes
    4. Remember that we target 32-bit architecture for firmware (pointers are 4 bytes)
    5. Exact sizes depend on the target ABI, so do not assume reordering alone guarantees a wire-compatible layout

    ```c
    // bad, 24 bytes of padding
    typedef struct {
    	uint16_t small_num;
    	uint32_t medium_num;
    	uint64_t big_num;
    	uint16_t small_num2;
    } bad_struct_t;
    
    // good, 0 bytes of padding
    typedef struct {
    	uint64_t big_num;
    	uint32_t medium_num;
    	uint16_t small_num;
    	uint16_t small_num2;
    } good_struct_t;
    ```

10. `enum` declaration
    1. Always use `typedef` and suffix your new type with `_t`
    2. Enum members should be prefixed with the name of the enum (example below)
    3. Always explicitly declare the base type of the enum, ESPECIALLY when it is a member of a `struct`
    4. It is recommended to assign explicit values to each enum member

    ```c
    // example enum
    typedef enum : uint8_t {
        FAULT_STATE_CLEAR = 0,
        FAULT_STATE_PENDING = 1,
        FAULT_STATE_LATCHED = 2,
        FAULT_STATE_RECOVERING = 3
    } fault_state_t;
    ```

11. Check null pointers before dereferencing
    1. Document whether pointer arguments may be null
    2. A documented non-null precondition must be upheld by callers
    3. Silently returning from a `void` function is not a substitute for an error contract, return an error or report a fault when invalid input matters

    ```c
    // bad
    void pass_by_reference(uint16_t *ptr) {
    	*ptr = 10;
    }
    
    // good, returns false for an invalid output pointer and the caller must handle failure
    bool set_value(uint16_t *value) {
    	if (value == nullptr) {
    		return false;
    	}
    	*value = 10;
    	return true;
    }
    ```

12. Use apostrophes to format large numbers (C23)

    ```c
    // bad
    static constexpr uint32_t target_core_clock_rate_hz = 16000000;
    
    // good
    static constexpr uint32_t target_core_clock_rate_hz = 16'000'000;
    ```

## Rust

These rules apply to all repository-owned Rust, including tests and examples. Unlike the firmware C, host Rust may use heap allocation and standard collections.

1. Do not use `.unwrap()` or `.unwrap_err()`
    1. This includes tests and examples, a panic in an example is still a crash
    2. Use `Result`/`Option` with `?` or explicit matching instead

2. Keep production `.expect()` and `.expect_err()` exceptional
    1. An invariant established by the code must guarantee the required variant
    2. Put the justification beside the call and explain the invariant in its message
    3. Prefer an infallible constructor when one is available
    4. Tests may use descriptive `expect`/`expect_err` calls for setup and assertions, prefer a test returning `Result` when `?` makes the intent clearer

    ```rust
    fn loopback_address() -> std::net::Ipv4Addr {
        // This source-controlled literal is valid IPv4, independent of external input.
        "127.0.0.1"
            .parse()
            .expect("hard-coded loopback literal should be a valid IPv4 address")
    }
    ```

3. Handle recoverable errors with `Result` and `Option`
    1. `Result` for operations that can fail, `Option` for expected absence
    2. Propagate with `?` or explicit matching, preserve useful error context
    3. Better yet, use the type system to prevent the error from being possible in the first place
    4. External input, parsing user files, filesystem access, device operations, and channel communication require recoverable handling, they cannot justify an `expect` simply because failure is unlikely

    ```rust
    fn read_log(path: &std::path::Path) -> std::io::Result<String> {
        let contents = std::fs::read_to_string(path)?;
        Ok(contents)
    }
    ```

4. Only use defaults when the fallback is meaningful
    1. `unwrap_or`, `unwrap_or_else`, and `unwrap_or_default` are allowed when the fallback is meaningful
    2. Do not conceal failures with defaults, `.ok()`, or ignored results
    3. Explain intentional best-effort behavior, including when an error can safely be ignored during cleanup

    ```rust
    fn log_folder(configured: Option<std::path::PathBuf>) -> std::path::PathBuf {
        // No configured folder means use the application's default log directory.
        configured.unwrap_or_else(|| std::path::PathBuf::from("logs"))
    }
    ```

5. Do not replace error handling with panics
    1. No `panic!`, unchecked extraction, or unchecked indexing of external data
    2. Assertions should express internal invariants

6. Naming
    1. `snake_case` for functions, variables, modules, and fields
    2. `UpperCamelCase` for types and enum variants
    3. `SCREAMING_SNAKE_CASE` for constants and statics
    4. Do not carry C's library prefixes, `g_` prefix, or `_t` suffix into Rust naming

7. Prefer borrowing to unnecessary cloning
    1. Accept `&str`, slices, or `&Path` when ownership is not required
    2. Take ownership explicitly when it is

8. Use enums to express distinct states
    1. Instead of ambiguous combinations of flags
    2. Keep APIs and abstractions focused on the actual use case

9. Use explicit widths for wire data
    1. Use `usize` for collection sizes and indices
    2. Explain intentional lossy numeric conversions

10. Use the type system
    1. Prevent invalid states and values and guarantee invariants
    2. Make ownership clear
    3. Prevent misuse of APIs

11. Keep blocking work out of the UI
    1. Keep blocking I/O and expensive processing outside UI updates
    2. Use channels to communicate with workers and make state ownership clear

12. Handle the worker lifecycle deliberately
    1. Check channel disconnection, cancellation, and worker shutdown
    2. Avoid holding locks across blocking work, review lock ordering and exit paths

13. Only use safe Rust
    1. No `unsafe` blocks
