#include "TestHelpers.h"

namespace WinAmyTests {

TEST_CLASS(BitboardTests) {
  public:
    TEST_METHOD(SetMaskSetsSingleBit) {
        Assert::AreEqual(1ULL, SetMask(0));
        Assert::AreEqual(2ULL, SetMask(1));
        Assert::AreEqual(4ULL, SetMask(2));
        Assert::AreEqual(0x8000000000000000ULL, SetMask(63));
        Assert::AreEqual(0x100ULL, SetMask(8));
    }

    TEST_METHOD(ClrMaskClearsSingleBit) {
        Assert::AreEqual(~1ULL, ClrMask(0));
        Assert::AreEqual(~2ULL, ClrMask(1));
        Assert::AreEqual(~0x8000000000000000ULL, ClrMask(63));
    }

    TEST_METHOD(SetBitSetsSpecifiedBit) {
        BitBoard b = 0;
        SetBit(b, 0);
        Assert::AreEqual(1ULL, b);
        SetBit(b, 63);
        Assert::AreEqual(0x8000000000000001ULL, b);
        SetBit(b, 0); // setting already-set bit is idempotent
        Assert::AreEqual(0x8000000000000001ULL, b);
    }

    TEST_METHOD(ClrBitClearsSpecifiedBit) {
        BitBoard b = 0xFFFFFFFFFFFFFFFFULL;
        ClrBit(b, 0);
        Assert::AreEqual(0xFFFFFFFFFFFFFFFEULL, b);
        ClrBit(b, 63);
        Assert::AreEqual(0x7FFFFFFFFFFFFFFEULL, b);
        ClrBit(b, 0); // clearing already-clear bit is idempotent
        Assert::AreEqual(0x7FFFFFFFFFFFFFFEULL, b);
    }

    TEST_METHOD(TstBitReturnsTrueForSetBits) {
        BitBoard b = SetMask(e4) | SetMask(a1) | SetMask(h8);
        Assert::IsTrue(TstBit(b, e4) != 0);
        Assert::IsTrue(TstBit(b, a1) != 0);
        Assert::IsTrue(TstBit(b, h8) != 0);
        Assert::IsTrue(TstBit(b, d4) == 0);
        Assert::IsTrue(TstBit(b, b2) == 0);
    }

    TEST_METHOD(FindSetBitReturnsLeastSignificantSetBit) {
        Assert::AreEqual(3, FindSetBit(0xA8ULL));
        Assert::AreEqual(0, FindSetBit(1ULL));
        Assert::AreEqual(63, FindSetBit(0x8000000000000000ULL));
        Assert::AreEqual(5, FindSetBit(0x20ULL));
        Assert::AreEqual(0, FindSetBit(0xFFFFFFFFFFFFFFFFULL));
    }

    TEST_METHOD(FindSetBitForEachSquare) {
        for (int i = 0; i < 64; i++) {
            Assert::AreEqual(i, FindSetBit(SetMask(i)));
        }
    }

    TEST_METHOD(CountBitsReturnsNumberOfSetBits) {
        Assert::AreEqual(0, CountBits(0ULL));
        Assert::AreEqual(64, CountBits(~0ULL));
        Assert::AreEqual(1, CountBits(1ULL));
        Assert::AreEqual(1, CountBits(0x8000000000000000ULL));
        Assert::AreEqual(3, CountBits(SetMask(a1) | SetMask(h8) | SetMask(e4)));
    }

    TEST_METHOD(CountBitsForPowersOfTwo) {
        for (int i = 0; i < 64; i++) {
            Assert::AreEqual(1, CountBits(SetMask(i)));
        }
    }

    TEST_METHOD(CountBitsForContiguousBits) {
        BitBoard b = 0;
        for (int i = 0; i < 64; i++) {
            SetBit(b, i);
            Assert::AreEqual(i + 1, CountBits(b));
        }
    }

    TEST_METHOD(SetBitAndClrBitAreInverses) {
        BitBoard b = 0;
        for (int i = 0; i < 64; i++) {
            SetBit(b, i);
        }
        Assert::AreEqual(~0ULL, b);
        for (int i = 0; i < 64; i++) {
            ClrBit(b, i);
        }
        Assert::AreEqual(0ULL, b);
    }

    TEST_METHOD(SetMaskAndClrMaskAreComplements) {
        for (int i = 0; i < 64; i++) {
            Assert::AreEqual(~0ULL, SetMask(i) | ClrMask(i));
            Assert::AreEqual(0ULL, SetMask(i) & ClrMask(i));
        }
    }
};

} // namespace WinAmyTests
