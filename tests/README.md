# Extrapolation regression tests

Run from the repository root on Linux with GCC and Clang installed:

```sh
for test in extrapolation snapshot; do
  g++ -std=c++17 -Wall -Wextra -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer "tests/${test}_tests.cpp" -o "/tmp/${test}-debug"
  "/tmp/${test}-debug"
  g++ -std=c++17 -O3 -ffast-math "tests/${test}_tests.cpp" -o "/tmp/${test}-gcc"
  "/tmp/${test}-gcc"
  clang++ -std=c++17 -O3 -ffast-math "tests/${test}_tests.cpp" -o "/tmp/${test}-clang"
  "/tmp/${test}-clang"
done
```

Each configuration passes:

```text
CHECKS 1289 FAILURES 0
CHECKS 27 FAILURES 0
```

Coverage:

- Simulation-time age plus outgoing latency, without rounding into packet batches.
- Maximum 32 ticks / 500 ms, including 21 choked ticks plus latency.
- Duplicate/stale records, clock rewinds, teleports, entity reuse, death and dormancy.
- Consistent turning across yaw wrap, acceleration, reversals and ground/air transitions.
- Inferred turning/acceleration limited to 125 ms; native movement still handles collisions and gravity.
- NaN/infinity rejection under fast-math and integer tick overflow protection.
- Network-subobject offsets, pending snapshot capture and suppression during extrapolated animation.
- Same-simulation-time origin corrections and frame-time restoration.

The snapshot tests use mocked engine callbacks. They do not validate signatures, entity offsets against a live binary, native movement, hit registration or gameplay accuracy. `Get_Desired_Move` expects validated records/trends, finite elapsed time and a writable three-float output.

The full translation unit also cross-compiles to a Windows x64 COFF object with Clang/MinGW and a compatibility preinclude. This is not a production ClangCL link or an in-game test. Existing compiled artifacts are unchanged; rebuild `SegaGmodX64/Segregation.vcxproj` in Release/x64 with ClangCL before testing in Garry's Mod.
