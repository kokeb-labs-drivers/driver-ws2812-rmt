/**
 * @file test_errors.cpp
 * @brief The `Error` enum follows the standard: the chip-specific values are
 * returned by real calls and the values this write-only driver never returns
 * are never returned.
 */
#include <cstring>

#include "mock_ws2812_rmt.hpp"
#include <array>

#include "test_harness.hpp"
#include "ws2812_rmt.hpp"

using ws2812_rmt::Error;
using ws2812_rmt_test::FullPort;
using ws2812_rmt_test::MinimalPort;

static_assert((static_cast<int32_t>(Error::Ok) == 0) && (static_cast<int32_t>(Error::Bus) == 3) &&
                  (static_cast<int32_t>(Error::DeviceIdMismatch) == 5) &&
                  (static_cast<int32_t>(Error::Unsupported) == 9) &&
                  (static_cast<int32_t>(Error::TimingUnreachable) == 16),
              "the Error values are fixed (standards/driver.md)");

namespace {

constexpr int32_t kFirstChipSpecific = 16;
using Seen = std::array<bool, 256>;

void Record(Seen& seen, Error e) {
  seen[static_cast<uint8_t>(e)] = true;
}

void RunScenarios(Seen& seen) {
  {
    MinimalPort port;
    ws2812_rmt::Config bad{};
    bad.reset_us = 1;
    ws2812_rmt::Ws2812Rmt<MinimalPort, 4> invalid(port, bad);
    Record(seen, invalid.EnsureInitialized().error); // InvalidArgument
    ws2812_rmt::Ws2812Rmt<MinimalPort, 4> s(port);
    Record(seen, s.Show().error);                            // NotInitialized
    Record(seen, s.SetPixel(9, ws2812_rmt::Colour{}).error); // InvalidArgument
    Record(seen, s.EnsureInitialized().error);               // Ok
    Record(seen, s.PowerOn().error);                         // Unsupported
    port.fail_transmit_at = 1;
    Record(seen, s.Show().error); // Bus
  }
  {
    FullPort port;
    ws2812_rmt::Ws2812Rmt<FullPort, 4> s(port);
    Record(seen, s.EnsureInitialized().error); // Ok
    Record(seen, s.PowerOff().error);          // Ok
    Record(seen, s.Show().error);              // NotPowered
  }
  {
    MinimalPort port;
    port.resolution_hz = 1'000'000;
    ws2812_rmt::Ws2812Rmt<MinimalPort, 4> s(port);
    Record(seen, s.EnsureInitialized().error); // TimingUnreachable
  }
  {
    MinimalPort port;
    port.fail_init = true;
    ws2812_rmt::Ws2812Rmt<MinimalPort, 4> s(port);
    Record(seen, s.EnsureInitialized().error); // Bus
  }
}

} // namespace

TEST_CASE("Every chip-specific Error value is returned by some call, and the "
          "unused base values "
          "never are") {
  Seen seen{};
  RunScenarios(seen);
  for (int32_t v = 0; v < 256; ++v) {
    const auto e = static_cast<Error>(v);
    if (std::strcmp(ws2812_rmt::ToString(e), "Unknown") == 0) {
      CHECK(!seen[static_cast<size_t>(v)]);
      continue;
    }
    const bool never_returned = (v == 4) || (v == 5) || (v == 6) || (v == 7) || (v == 8);
    if (v >= kFirstChipSpecific) {
      CHECK(seen[static_cast<size_t>(v)]);
    }
    if (never_returned) {
      CHECK(!seen[static_cast<size_t>(v)]);
    }
    CHECK((v < 10) || (v >= kFirstChipSpecific));
  }
  CHECK(seen[static_cast<size_t>(Error::Ok)]);
  CHECK(seen[static_cast<size_t>(Error::NotInitialized)]);
  CHECK(seen[static_cast<size_t>(Error::InvalidArgument)]);
  CHECK(seen[static_cast<size_t>(Error::Bus)]);
  CHECK(seen[static_cast<size_t>(Error::Unsupported)]);
}

TEST_CASE("ToString names every enumerator and falls back for anything else") {
  CHECK(std::strcmp(ws2812_rmt::ToString(Error::TimingUnreachable), "TimingUnreachable") == 0);
  CHECK(std::strcmp(ws2812_rmt::ToString(Error::Timeout), "Timeout") == 0);
  CHECK(std::strcmp(ws2812_rmt::ToString(Error::Crc), "Crc") == 0);
  CHECK(std::strcmp(ws2812_rmt::ToString(Error::Busy), "Busy") == 0);
  CHECK(std::strcmp(ws2812_rmt::ToString(Error::DeviceFault), "DeviceFault") == 0);
  CHECK(std::strcmp(ws2812_rmt::ToString(Error::DeviceIdMismatch), "DeviceIdMismatch") == 0);
  CHECK(std::strcmp(ws2812_rmt::ToString(ws2812_rmt_test::RawEnum<Error>(77)), "Unknown") == 0);
  const auto ok = ws2812_rmt::Result<int32_t>::Success(5);
  const auto failed = ws2812_rmt::Result<int32_t>::Failure(Error::Bus);
  CHECK(ok.ok() && static_cast<bool>(ok) && (ok.value == 5));
  CHECK(!failed.ok() && !static_cast<bool>(failed) && (failed.value == 0));
}
