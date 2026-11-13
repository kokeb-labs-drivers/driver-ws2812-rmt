# Testing and CI

How the driver is checked: host tests against a model of the pixels written from the datasheets, a fuzzer that controls
the configuration, the tick rate and the port's failures, compiled documentation, an install test, ESP32 and Cortex-M
builds, static analysis and measured quality gates, on every pull request.

## Host tests

```bash
cmake -S . -B build -DWS2812_RMT_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

| File | Covers |
| --- | --- |
| [`tests/test_waveform.cpp`](../tests/test_waveform.cpp) | `ResolveWaveform()` at known rates, the edges of every window, the period, reset and gap-limit rules, `ResolveRmtRate()` (the divider arithmetic of the ESP32 port), a sweep of hostile rates |
| [`tests/test_timing_models.cpp`](../tests/test_timing_models.cpp) | Every profile equals the intersection of its documents; the nominals lie inside every document; two WS2812B generations share no waveform; default orders |
| [`tests/test_model.cpp`](../tests/test_model.cpp) | The model itself: its decoding, its windows, its latch, with hand-made symbol streams |
| [`tests/test_encode.cpp`](../tests/test_encode.cpp), [`test_colour.cpp`](../tests/test_colour.cpp) | Byte packing for every colour order, `Scale8` over all 65 536 pairs, gamma, white, wheel |
| [`tests/test_frame.cpp`](../tests/test_frame.cpp) | Frame time, refresh rate, pixels per frame, the input-level function |
| [`tests/test_chain.cpp`](../tests/test_chain.cpp) | End to end: every variant, every document, every order, strips of 1 to 1024 pixels, a tick-rate error of 1000 ppm |
| [`tests/test_lifecycle.cpp`](../tests/test_lifecycle.cpp), [`test_errors.cpp`](../tests/test_errors.cpp), [`test_bus_faults.cpp`](../tests/test_bus_faults.cpp) | Start-up, every error value, failing and underrunning ports |
| [`tests/test_edges.cpp`](../tests/test_edges.cpp) | The boundaries a one-edit change would move: the reset wait at a rate that is not whole megahertz, the rounded-up tick time, the longer bit code in the frame time, the exclusive in-frame cap, the recovery after a failed `PowerOff()` |
| [`tests/esp32_stub/`](../tests/esp32_stub/esp32_basic_test.cpp) | The ESP32 port and both examples over stand-ins of the ESP-IDF headers: each example's `Start()` with the default pins reaches a shown frame, the channel settings, encoder, waits and recovery, the ESP32-H2 default rate |
| [`tests/test_kconfig.cpp`](../tests/test_kconfig.cpp) | The menuconfig values and their defaults |
| [`tests/mock_ws2812_rmt.hpp`](../tests/mock_ws2812_rmt.hpp) | The model: the documents' windows, a pixel chain that latches, and the fake ports |
| [`tests/fuzz/`](../tests/fuzz/fuzz_ws2812_rmt.cpp) | The fuzz target; CTest replays its corpus (`ws2812_rmt_fuzz_ws2812_rmt_corpus`), see below |
| [`tests/compile_fail/`](../tests/compile_fail/not_a_port.cpp) | Ports and configurations that must not compile |
| [`tests/docs/`](../tests/docs/quickstart.cpp) | Every code block of the site, compiled |
| [`tests/check_install.cmake`](../tests/check_install.cmake) | `find_package(ws2812_rmt)` from an installed package |

The tests build with `-fno-exceptions -fno-rtti` and warnings as errors, on GCC and Clang.

### What a test cannot show on the host

Every behaviour fix of the review rounds has a host test that fails without it, except these, which a host cannot
exercise. Documentation, label and figure fixes, the typed constants (checked by the compiler) and the reset-wait
busy loop are reviewed, not tested:

- The ESP32 port on the chip (the real clock, the memory size, the interrupt priority, the line level after a
  timeout) needs ESP-IDF and a chip. On the host, `tests/esp32_stub/` builds the port and both examples against
  stand-ins of the ESP-IDF headers and runs each example's start-up, the channel settings, the encoder, the waits and
  the recovery; its divider arithmetic is the pure `ResolveRmtRate()`, which is tested on its own as well.
- Whether the RMT driver reports an underrun is a property of ESP-IDF: the docs state that this port cannot see one,
  and the model test of an underrun shows the silent outcome a port that cannot see it produces.
- The reference circuit figures (`docs/hardware.yml`) are checked by `kokeb-docs hardware --check` and
  `kokeb-docs figures --check` for validity and routing, not for electrical correctness.

### The model stands alone

`tests/mock_ws2812_rmt.hpp` carries its own table of the windows of each datasheet document (`kDocuments`), typed in
from the PDFs and never from the driver's profile table. A test cross-checks that every named variant is the
intersection of its documents. The pixel chain decodes the half-symbols the port was given into bits, judges every
duration against the window, latches a frame when a low exceeds the reset, and reports pixels that received too few or
too many bits. So a transcription error in either table, a waveform outside a window, or a missing reset fails a test,
and green tests cannot come from one author's single reading of a datasheet.

## Fuzz tests

[`tests/fuzz/fuzz_ws2812_rmt.cpp`](../tests/fuzz/fuzz_ws2812_rmt.cpp) lets libFuzzer choose a configuration (any
variant, `Custom` with arbitrary windows, any order, brightness, gamma, reset), the port (any tick rate including 0 and
2^32 - 1, failing hooks, with or without the optional hooks) and up to 48 driver calls. After every call it checks the
driver's promises and aborts, naming the line, when one breaks:

- a failed call returns an `Error` this driver returns (never `Timeout`, `Crc` or `Busy`) and `T{}`;
- before `EnsureInitialized()` succeeded the port saw no `Transmit()`, and `InvalidArgument`, `NotInitialized` and
  `TimingUnreachable` mean nothing was transmitted;
- every transmitted waveform lies inside the windows of the declared profile and its reset is above the minimum:
  a hostile tick rate can never produce an illegal waveform;
- the wire has exactly `Length() x channels` bytes at every `Transmit()`, equal to a second encoding written in the
  target;
- after a failed `Transmit()` the pixels are intact, and the port's waits before the next `Transmit()` add up to
  strictly more than the reset (the port records every `DelayUs()` and every `Transmit()` in order);
- the supply pin write fails when the input says so: a failed write is `Bus`, `PowerOn()` then leaves the strip as it
  was, and after any `PowerOff()` (also one whose write failed) `Show()` and `TraceWire()` transmit nothing and return
  `NotPowered`;
- `TraceWire()` sends exactly the bytes it was given, and an empty span or one above four bytes per pixel of capacity
  is `InvalidArgument` with nothing sent;
- every log line is a non-empty plain-text message.

```bash
# libFuzzer needs Clang (on macOS, Homebrew's LLVM)
cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ -DWS2812_RMT_BUILD_TESTS=ON -DWS2812_RMT_BUILD_FUZZERS=ON
cmake --build build-fuzz --target fuzz_ws2812_rmt
./build-fuzz/tests/fuzz/fuzz_ws2812_rmt -max_total_time=120 tests/fuzz/corpus/fuzz_ws2812_rmt
```

The same file also builds without libFuzzer as `fuzz_replay_fuzz_ws2812_rmt`, and CTest replays every input in
[`tests/fuzz/corpus/fuzz_ws2812_rmt/`](../tests/fuzz/corpus/fuzz_ws2812_rmt) on GCC and Clang
(`ws2812_rmt_fuzz_ws2812_rmt_corpus`): the seeds, and every input that ever broke a promise (name it `regress-*`), so a
fixed bug stays fixed. CI fuzzes for 120 s on every pull request, from the seeds plus a corpus it keeps between runs
(job `Host quality / fuzz`).

## Quality gates

The `Host quality` job measures the library code, `inc/` and `src/` only (tests and examples are left out), and
fails when a value is over the limits of this repository's assurance level, `high` (set in
[`.kokeb/handbook.yml`](../.kokeb/handbook.yml), defined by the handbook's ADR-0020):

| Gate | Tool and how it is measured | Limit at `high` |
| --- | --- | --- |
| Complexity | lizard 1.24.1, modified cyclomatic complexity (`-m`: a `switch` counts once) per function | CCN at most 15 |
| Function length | lizard, lines of code per function | at most 60 |
| Parameters | lizard, per function | at most 6 |
| Line coverage | GCC 14 `--coverage -O0`, every CTest, gcovr 8.7 `--merge-lines` (a template's lines count once across its instances), `--exclude-unreachable-branches --exclude-throw-branches` | at least 85 % |
| Branch coverage | same run | at least 75 % |
| MC/DC | GCC `-fcondition-coverage`, same run | measured, not gated at `high` (100 % at `safety`) |

Measured on the last run: lines 97.2 % (522 of 537), functions 93.9 %, branches 89.1 % (344 of 386), conditions 88.9 %
(304 of 342), decisions 84.2 % (133 of 158).
| Sanitizers | Clang, `-fsanitize=address,undefined -fno-sanitize-recover=all`, every CTest | no report |
| Fuzzing | libFuzzer with ASan and UBSan, 120 s per target | no broken promise |

The reports are in the run's `host-quality-reports` artifact and its step summary. To run the gates locally:

```bash
# Coverage (GCC 14 or later), then the complexity limits
cmake -S . -B build-cov -DCMAKE_CXX_COMPILER=g++-14 -DWS2812_RMT_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="--coverage -O0 -fcondition-coverage"
cmake --build build-cov && ctest --test-dir build-cov
gcovr --root . --filter inc/ --filter src/ --merge-lines --exclude-unreachable-branches --txt build-cov
lizard -m -C 15 -L 60 -a 6 inc src

# Sanitizers (Clang)
cmake -S . -B build-san -DCMAKE_CXX_COMPILER=clang++ -DWS2812_RMT_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -O1 -g"
cmake --build build-san && ctest --test-dir build-san --output-on-failure
```

The fuzz gate runs locally as shown in [Fuzz tests](#fuzz-tests).

clang-tidy 20 ([`_config/.clang-tidy`](../_config/.clang-tidy): bugprone, CERT, clang-analyzer,
cppcoreguidelines, modernize, performance, portability and readability, each disabled check with its reason) and
cppcheck (strict) run in the `C++ lint` and `Repository checks` jobs and fail on any finding.

The `MISRA` job ([`ci-misra.yml`](../.github/workflows/ci-misra.yml)) runs the shared `ru-misra` workflow of
ci-tools at level `high`: it traces a host build and the Cortex-M build (`ws2812_rmt_optimised_build`) of every
translation unit, checks them against MISRA C++:2023 and the handbook's C++20 profile (the CodeQL coding-standards
and profile packs, plus the validated clang-tidy and cppcheck checks), and reports each finding against the
handbook's compliance matrix and deviation records. A finding is fixed in the code; a deliberate exception names
its deviation on the line before it (`// codeql::misra_deviation_next_line(MD-CPP-009)` above the `.ipp` include
of `inc/ws2812_rmt.hpp`). Until the repository is public the job runs with `open_source_prerelease: true`, the
declaration that the code is released under the licence in `LICENSE` and is to be published.

## CI workflows

Everything runs from [`ci.yml`](../.github/workflows/ci.yml), which calls one workflow file per check and ends in
the `CI gate` job, the single required check (`CI gate / gate`).

| Job | What it checks |
| --- | --- |
| Host tests | GCC, then Clang (one job), build and run every test above, then build and run `examples/host` on its own |
| Host quality | The gates above: complexity, coverage, sanitizers, fuzzing |
| Cross-compile | `arm-none-eabi-g++` for Cortex-M0+, M4F and M7F with the strict flags, warnings as errors (nothing linked) |
| C++ lint | clang-format and clang-tidy over the sources |
| MISRA | MISRA C++:2023 and the C++20 profile on traced host and Cortex-M builds, at level `high` (see [Quality gates](#quality-gates)) |
| Docs | Strict site build (`_config/mkdocs.yml`), generated-region drift, Doxygen (`_config/Doxyfile`), offline link check |
| Repository checks | One job: DCO on pull requests, the org standard, Markdown and YAML lint, the [Kokeb Labs handbook](https://github.com/kokeb-labs/handbook) rules for `.kokeb/handbook.yml` (by rule ID), template drift (warns only) and cppcheck over the driver sources |
| CI gate | Passes only if every job above passed: the one required check |

The **ESP32 examples** job builds every app in `examples/esp32/app_config.yml` with the repository root as the
driver's component. The gate needs it too.
