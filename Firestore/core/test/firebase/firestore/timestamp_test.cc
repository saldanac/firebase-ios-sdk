/*
 * Copyright 2018 Google
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Firestore/core/include/firebase/firestore/timestamp.h"

#include <limits>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

namespace firebase {

namespace {

using TimePoint = std::chrono::time_point<std::chrono::system_clock>;
using Sec = std::chrono::seconds;
using Ms = std::chrono::milliseconds;

const auto kUpperBound = 253402300800L - 1;
const auto kLowerBound = -62135596800L;

}  // namespace

TEST(Timestamp, Constructors) {
  const Timestamp zero;
  EXPECT_EQ(0, zero.seconds());
  EXPECT_EQ(0, zero.nanoseconds());

  const Timestamp positive(100, 200);
  EXPECT_EQ(100, positive.seconds());
  EXPECT_EQ(200, positive.nanoseconds());

  const Timestamp negative(-100, 200);
  EXPECT_EQ(-100, negative.seconds());
  EXPECT_EQ(200, negative.nanoseconds());

  const Timestamp now = Timestamp::Now();
  EXPECT_LT(0, now.seconds());
  EXPECT_LE(0, now.nanoseconds());

  Timestamp copy_now = now;
  EXPECT_EQ(now, copy_now);
  EXPECT_EQ(now.seconds(), copy_now.seconds());
  EXPECT_EQ(now.nanoseconds(), copy_now.nanoseconds());
  const Timestamp move_now = std::move(copy_now);
  EXPECT_EQ(now, move_now);
}

TEST(Timestamp, Bounds) {
  const Timestamp max_timestamp{kUpperBound, 999999999};
  EXPECT_EQ(kUpperBound, max_timestamp.seconds());
  EXPECT_EQ(999999999, max_timestamp.nanoseconds());

  const Timestamp min_timestamp{kLowerBound, 0};
  EXPECT_EQ(kLowerBound, min_timestamp.seconds());
  EXPECT_EQ(0, min_timestamp.nanoseconds());
}

TEST(Timestamp, FromTimeT) {
  const Timestamp zero = Timestamp::FromTimeT(std::time_t{});
  EXPECT_EQ(0, zero.seconds());
  EXPECT_EQ(0, zero.nanoseconds());

  const Timestamp positive = Timestamp::FromTimeT(std::time_t{123456});
  EXPECT_EQ(123456, positive.seconds());
  EXPECT_EQ(0, positive.nanoseconds());

  const Timestamp negative = Timestamp::FromTimeT(std::time_t{-123456});
  EXPECT_EQ(-123456, negative.seconds());
  EXPECT_EQ(0, negative.nanoseconds());
}

TEST(Timestamp, FromChrono) {
  const auto zero = Timestamp::FromTimePoint(TimePoint{});
  EXPECT_EQ(0, zero.seconds());
  EXPECT_EQ(0, zero.nanoseconds());

  const auto sec = Timestamp::FromTimePoint(TimePoint{Sec(123)});
  EXPECT_EQ(123, sec.seconds());
  EXPECT_EQ(0, sec.nanoseconds());

  const auto ms = Timestamp::FromTimePoint(TimePoint{Sec(123) + Ms(456)});
  EXPECT_EQ(123, ms.seconds());
  EXPECT_EQ(456000000, ms.nanoseconds());
}

TEST(Timestamp, FromChronoNegativeTime) {
  const auto no_fraction = Timestamp::FromTimePoint(TimePoint{Sec(-123)});
  EXPECT_EQ(-123, no_fraction.seconds());
  EXPECT_EQ(0, no_fraction.nanoseconds());

  const auto with_positive_fraction =
      Timestamp::FromTimePoint(TimePoint{Sec(-123) + Ms(456)});
  EXPECT_EQ(-123, with_positive_fraction.seconds());
  EXPECT_EQ(456000000, with_positive_fraction.nanoseconds());

  const auto with_negative_fraction =
      Timestamp::FromTimePoint(TimePoint{Sec(-122) + Ms(-544)});
  EXPECT_EQ(-123, with_negative_fraction.seconds());
  EXPECT_EQ(456000000, with_negative_fraction.nanoseconds());

  const auto with_large_negative_fraction =
      Timestamp::FromTimePoint(TimePoint{Sec(-122) + Ms(-100544)});
  EXPECT_EQ(-223, with_large_negative_fraction.seconds());
  EXPECT_EQ(456000000, with_large_negative_fraction.nanoseconds());

  const auto only_negative_fraction =
      Timestamp::FromTimePoint(TimePoint{Ms(-544)});
  EXPECT_EQ(-1, only_negative_fraction.seconds());
  EXPECT_EQ(456000000, only_negative_fraction.nanoseconds());

  const auto positive_time_negative_fraction =
      Timestamp::FromTimePoint(TimePoint{Sec(1) + Ms(-544)});
  EXPECT_EQ(0, positive_time_negative_fraction.seconds());
  EXPECT_EQ(456000000, positive_time_negative_fraction.nanoseconds());

  const auto near_bounds =
      Timestamp::FromTimePoint(TimePoint{Sec(kUpperBound + 1) + Ms(-544)});
  EXPECT_EQ(kUpperBound, near_bounds.seconds());
  EXPECT_EQ(456000000, near_bounds.nanoseconds());
}

TEST(Timestamp, ToChrono) {
  namespace chr = std::chrono;

  {
    const Timestamp positive{123, 456789000};

    const auto micros = positive.ToTimePoint().time_since_epoch();
    EXPECT_EQ(123456789, chr::duration_cast<chr::microseconds>(micros).count());

    const auto millis =
        positive.ToTimePoint<chr::system_clock, chr::milliseconds>()
            .time_since_epoch();
    EXPECT_EQ(123456000, chr::duration_cast<chr::microseconds>(millis).count());

    const auto nanos =
        positive.ToTimePoint<chr::system_clock, chr::nanoseconds>()
            .time_since_epoch();
    EXPECT_EQ(123456789000,
              chr::duration_cast<chr::nanoseconds>(nanos).count());
  }  // namespace chr=std::chrono;

  {
    const Timestamp negative{-123, 456000000};

    const auto millis =
        negative.ToTimePoint<chr::system_clock, chr::milliseconds>()
            .time_since_epoch();
    const auto seconds = chr::duration_cast<chr::seconds>(millis);
    EXPECT_EQ(-122, seconds.count());
    EXPECT_EQ(-544,
              chr::duration_cast<chr::milliseconds>(millis - seconds).count());
  }

  // Bounds
  {
    const Timestamp max{kUpperBound, 999999999};
    const auto max_micros = max.ToTimePoint().time_since_epoch();
    EXPECT_EQ(kUpperBound * 1000 * 1000 + 999999,
              chr::duration_cast<chr::microseconds>(max_micros).count());

    const Timestamp min{kLowerBound, 0};
    const auto min_micros = min.ToTimePoint().time_since_epoch();
    EXPECT_EQ(kLowerBound * 1000 * 1000,
              chr::duration_cast<chr::microseconds>(min_micros).count());
  }

  // Overflow
  {
    const Timestamp max{kUpperBound, 999999999};

    const auto max_nanos =
        max.ToTimePoint<chr::system_clock, chr::nanoseconds>()
            .time_since_epoch();
    EXPECT_EQ(std::numeric_limits<chr::nanoseconds::rep>::max(),
              chr::duration_cast<chr::nanoseconds>(max_nanos).count());

    const Timestamp min{kLowerBound, 0};
    const auto min_nanos =
        min.ToTimePoint<chr::system_clock, chr::nanoseconds>()
            .time_since_epoch();
    EXPECT_EQ(std::numeric_limits<chr::nanoseconds::rep>::min(),
              chr::duration_cast<chr::nanoseconds>(min_nanos).count());
  }

  // Unsigned duration
  {
    // using UnsignedNanos = chr::duration<unsigned long long, std::nano>;

    // const Timestamp max{kUpperBound, 999999999};
    // const auto max_nanos =
    //     max.ToTimePoint<chr::system_clock, UnsignedNanos>().time_since_epoch();
    // EXPECT_EQ(kUpperBound * 1000 * 1000 * 1000 + 999999999, max_nanos.count());

    // const Timestamp small_negative{-123, 0};
    // const auto negative_nanos =
    //     small_negative.ToTimePoint<chr::system_clock, UnsignedNanos>()
    //         .time_since_epoch();
    // EXPECT_EQ(0, negative_nanos.count());
  }
}

TEST(Timestamp, Comparison) {
  EXPECT_LT(Timestamp(), Timestamp(1, 2));
  EXPECT_LT(Timestamp(1, 2), Timestamp(2, 1));
  EXPECT_LT(Timestamp(2, 1), Timestamp(2, 2));

  EXPECT_GT(Timestamp(1, 1), Timestamp());
  EXPECT_GT(Timestamp(2, 1), Timestamp(1, 2));
  EXPECT_GT(Timestamp(2, 2), Timestamp(2, 1));

  EXPECT_LE(Timestamp(), Timestamp());
  EXPECT_LE(Timestamp(), Timestamp(1, 2));
  EXPECT_LE(Timestamp(1, 2), Timestamp(2, 1));
  EXPECT_LE(Timestamp(2, 1), Timestamp(2, 1));
  EXPECT_LE(Timestamp(2, 1), Timestamp(2, 2));

  EXPECT_GE(Timestamp(), Timestamp());
  EXPECT_GE(Timestamp(1, 1), Timestamp());
  EXPECT_GE(Timestamp(1, 1), Timestamp(1, 1));
  EXPECT_GE(Timestamp(2, 1), Timestamp(1, 2));
  EXPECT_GE(Timestamp(2, 1), Timestamp(2, 1));
  EXPECT_GE(Timestamp(2, 2), Timestamp(2, 1));

  EXPECT_EQ(Timestamp(), Timestamp());
  EXPECT_EQ(Timestamp(), Timestamp(0, 0));
  EXPECT_EQ(Timestamp(123, 123456789), Timestamp(123, 123456789));

  EXPECT_NE(Timestamp(), Timestamp(0, 1));
  EXPECT_NE(Timestamp(), Timestamp(1, 0));
  EXPECT_NE(Timestamp(123, 123456789), Timestamp(123, 123456780));
}

TEST(Timestamp, Hash) {
  const Timestamp foo1{123, 456000000};
  const Timestamp foo2 = foo1;
  const Timestamp foo3 =
      Timestamp::FromTimePoint(TimePoint{Sec(123) + Ms(456)});
  EXPECT_EQ(std::hash<Timestamp>()(foo1), std::hash<Timestamp>()(foo2));
  EXPECT_EQ(std::hash<Timestamp>()(foo2), std::hash<Timestamp>()(foo3));

  const Timestamp bar{123, 456};
  EXPECT_NE(std::hash<Timestamp>()(foo1), std::hash<Timestamp>()(bar));
}

TEST(Timestamp, InvalidArguments) {
  // Negative nanoseconds.
  ASSERT_ANY_THROW(Timestamp(0, -1));
  ASSERT_ANY_THROW(Timestamp(100, -1));
  ASSERT_ANY_THROW(Timestamp(100, -12346789));

  // Nanoseconds that are more than one second.
  ASSERT_ANY_THROW(Timestamp(0, 999999999 + 1));

  // Seconds beyond supported range.
  ASSERT_ANY_THROW(Timestamp(kLowerBound - 1, 0));
  ASSERT_ANY_THROW(Timestamp(kUpperBound + 1, 0));

  // Using chrono.
  ASSERT_ANY_THROW(Timestamp::FromTimePoint(TimePoint{Sec(kLowerBound - 1)}));
  ASSERT_ANY_THROW(Timestamp::FromTimePoint(TimePoint{Sec(kUpperBound + 1)}));
}

}  // namespace firebase