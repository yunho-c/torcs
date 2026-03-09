---
name: torcs-test-writing
description: Write TORCS unit tests consistently, including robust floating-point assertions for tests.
---

## When to load this skill

Load this skill whenever you add or modify TORCS unit tests (`test/libs/*_test`) and especially when testing physics/simulation code that uses floating-point arithmetic.

## Core rule for floating-point assertions

- Use `EXPECT_NEAR(actual, expected, eps)` for **computed** values.
- Do not default to `EXPECT_FLOAT_EQ` for formula results.
- `EXPECT_FLOAT_EQ` is acceptable only for values that are intentionally exact (e.g., direct constant assignment, boolean-like 0/1 values not produced by arithmetic chains).

## Epsilon policy

Define a local file epsilon near the top of each test file:

```cpp
static const float kEps = 1e-5f;
```

Use this default unless there is a reason to tighten or relax:

- `1e-6f`: simple one-step arithmetic
- `1e-5f`: default for simulation formulas and inertia/torque propagation
- `1e-4f`: only if cross-platform/compiler differences require it

Do not use different epsilons per assertion unless needed; prefer one file-local default for readability.

## Quick decision table

- `copied_value == configured_literal` -> `EXPECT_NEAR(..., kEps)` is still acceptable and preferred for consistency
- `clamped/normalized/aggregated/computed value` -> `EXPECT_NEAR(..., kEps)`
- `integer states, enums, indices, counts` -> `EXPECT_EQ`
- `pointer identity / null checks` -> `EXPECT_EQ`, `EXPECT_NE`

## TORCS-specific test practices

- Reuse existing constants/macros (`MAX_GEARS`, `TRANS_*`, `CLUTCH_*`, `SECT_*`, `PRM_*`, `VAL_TRANS_*`) from headers.
- Keep test projects isolated when stubs may collide at link time.
- For simulator module tests, validate both:
  - behavior/state changes (ratios, inertias, clutch state, gear)
  - call routing/ordering (stub call logs)

## Pre-merge checklist for test edits

1. Search for float equality in touched test files:
   - `EXPECT_FLOAT_EQ(` should be absent unless intentionally justified.
2. Build the test project.
3. Run the test executable and verify gtest summary contains `[  PASSED  ]`.

## Example patterns

Use this for computed values:

```cpp
EXPECT_NEAR(trans.driveI[2], expectedDriveI, kEps);
EXPECT_NEAR(trans.overallRatio[gear], expectedRatio, kEps);
```

Use this for discrete values:

```cpp
EXPECT_EQ(trans.gearbox.gear, 2);
EXPECT_EQ(trans.clutch.state, CLUTCH_RELEASING);
```
