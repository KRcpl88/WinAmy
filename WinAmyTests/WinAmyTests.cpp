#include "CppUnitTest.h"

extern "C" {
#include "bitboard.h"
#include "dbase.h"
#include "hashtable.h"
#include "init.h"
#include "inline.h"
#include "magic.h"
#include "movedata.h"
}

#include <cstdint>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace WinAmyTests {
class PositionGuard {
  public:
    explicit PositionGuard(Position *position) : p(position) {}
    ~PositionGuard() { FreePosition(p); }
    Position *get() const { return p; }

  private:
    Position *p;
};

static uint64_t reference_rook_attacks(int sq, uint64_t occupied) {
    uint64_t attacks = 0;
    const int file = sq & 7;
    const int rank = sq >> 3;

    for (int r = rank + 1; r < 8; r++) {
        const int target = r * 8 + file;
        attacks |= SetMask(target);
        if (TstBit(occupied, target))
            break;
    }

    for (int r = rank - 1; r >= 0; r--) {
        const int target = r * 8 + file;
        attacks |= SetMask(target);
        if (TstBit(occupied, target))
            break;
    }

    for (int f = file + 1; f < 8; f++) {
        const int target = rank * 8 + f;
        attacks |= SetMask(target);
        if (TstBit(occupied, target))
            break;
    }

    for (int f = file - 1; f >= 0; f--) {
        const int target = rank * 8 + f;
        attacks |= SetMask(target);
        if (TstBit(occupied, target))
            break;
    }

    return attacks;
}

static uint64_t reference_bishop_attacks(int sq, uint64_t occupied) {
    uint64_t attacks = 0;
    int file = sq & 7;
    int rank = sq >> 3;

    for (int f = file + 1, r = rank + 1; f < 8 && r < 8; f++, r++) {
        const int target = r * 8 + f;
        attacks |= SetMask(target);
        if (TstBit(occupied, target))
            break;
    }

    for (int f = file - 1, r = rank + 1; f >= 0 && r < 8; f--, r++) {
        const int target = r * 8 + f;
        attacks |= SetMask(target);
        if (TstBit(occupied, target))
            break;
    }

    for (int f = file + 1, r = rank - 1; f < 8 && r >= 0; f++, r--) {
        const int target = r * 8 + f;
        attacks |= SetMask(target);
        if (TstBit(occupied, target))
            break;
    }

    for (int f = file - 1, r = rank - 1; f >= 0 && r >= 0; f--, r--) {
        const int target = r * 8 + f;
        attacks |= SetMask(target);
        if (TstBit(occupied, target))
            break;
    }

    return attacks;
}

static void assert_positions_equal(const Position *lhs, const Position *rhs) {
    for (int i = 0; i < 64; i++) {
        Assert::AreEqual((unsigned long long)lhs->atkTo[i],
                         (unsigned long long)rhs->atkTo[i]);
        Assert::AreEqual((unsigned long long)lhs->atkFr[i],
                         (unsigned long long)rhs->atkFr[i]);
        Assert::AreEqual((int)lhs->piece[i], (int)rhs->piece[i]);
    }

    for (int c = 0; c < 2; c++) {
        for (int p = 0; p < 7; p++) {
            Assert::AreEqual((unsigned long long)lhs->mask[c][p],
                             (unsigned long long)rhs->mask[c][p]);
        }

        Assert::AreEqual(lhs->material[c], rhs->material[c]);
        Assert::AreEqual(lhs->nonPawn[c], rhs->nonPawn[c]);
        Assert::AreEqual((int)lhs->kingSq[c], (int)rhs->kingSq[c]);
        Assert::AreEqual((int)lhs->material_signature[c],
                         (int)rhs->material_signature[c]);
    }

    Assert::AreEqual((unsigned long long)lhs->slidingPieces,
                     (unsigned long long)rhs->slidingPieces);
    Assert::AreEqual((unsigned long long)lhs->hkey, (unsigned long long)rhs->hkey);
    Assert::AreEqual((unsigned long long)lhs->pkey, (unsigned long long)rhs->pkey);
    Assert::AreEqual((int)lhs->castle, (int)rhs->castle);
    Assert::AreEqual((int)lhs->enPassant, (int)rhs->enPassant);
    Assert::AreEqual((int)lhs->turn, (int)rhs->turn);
    Assert::AreEqual((int)lhs->ply, (int)rhs->ply);
}

TEST_CLASS(CoreMoveAndBitboardTests) {
  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
    }

    TEST_METHOD(FindSetBitReturnsLeastSignificantSetBit) {
        Assert::AreEqual(3, FindSetBit(0xA8ULL));
        Assert::AreEqual(0, FindSetBit(1ULL));
        Assert::AreEqual(63, FindSetBit(0x8000000000000000ULL));
    }

    TEST_METHOD(CountBitsReturnsNumberOfSetBits) {
        Assert::AreEqual(0, CountBits(0ULL));
        Assert::AreEqual(64, CountBits(~0ULL));
        Assert::AreEqual(3, CountBits(SetMask(a1) | SetMask(h8) | SetMask(e4)));
    }

    TEST_METHOD(PawnAndKnightAttackTablesMatchExpectedSquares) {
        const uint64_t whitePawnExpected = SetMask(d3) | SetMask(f3);
        const uint64_t blackPawnExpected = SetMask(d6) | SetMask(f6);
        const uint64_t knightExpected = SetMask(e2) | SetMask(f3) | SetMask(h3);

        Assert::AreEqual((unsigned long long)whitePawnExpected,
                         (unsigned long long)PawnEPM[White][e2]);
        Assert::AreEqual((unsigned long long)blackPawnExpected,
                         (unsigned long long)PawnEPM[Black][e7]);
        Assert::AreEqual((unsigned long long)knightExpected,
                         (unsigned long long)KnightEPM[g1]);
    }

    TEST_METHOD(RookAndBishopMagicAttacksMatchNaiveAttacks) {
        const uint64_t occupied = SetMask(d4) | SetMask(d6) | SetMask(f4) |
                                  SetMask(d2) | SetMask(b4) | SetMask(f6) |
                                  SetMask(b6) | SetMask(f2) | SetMask(b2);

        Assert::AreEqual((unsigned long long)reference_rook_attacks(d4, occupied),
                         (unsigned long long)rook_attacks(d4, occupied));

        Assert::AreEqual((unsigned long long)reference_bishop_attacks(d4, occupied),
                         (unsigned long long)bishop_attacks(d4, occupied));
    }

    TEST_METHOD(DoMoveAndUndoMoveRestorePosition) {
        PositionGuard position(InitialPosition());
        PositionGuard snapshot(ClonePosition(position.get()));

        const move_t move = make_move(e2, e4, M_PAWND);
        DoMove(position.get(), move);

        Assert::AreEqual((int)Pawn, (int)position.get()->piece[e4]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[e2]);
        Assert::AreEqual((int)Black, (int)position.get()->turn);
        Assert::AreEqual(1, (int)position.get()->ply);

        UndoMove(position.get(), move);
        assert_positions_equal(position.get(), snapshot.get());
    }

    TEST_METHOD(DoNullAndUndoNullRestorePositionFields) {
        // Position with en passant available on d6.
        PositionGuard position(
            CreatePositionFromEPD((char *)"4k3/8/8/3pP3/8/8/8/4K3 w - d6"));
        PositionGuard snapshot(ClonePosition(position.get()));

        DoNull(position.get());
        Assert::AreEqual((int)Black, (int)position.get()->turn);
        Assert::AreEqual(0, (int)position.get()->enPassant);

        UndoNull(position.get());
        assert_positions_equal(position.get(), snapshot.get());
    }

    TEST_METHOD(RecalcAttacksRebuildsAtkSetDerivedData) {
        // Position with a white bishop on d5.
        PositionGuard position(
            CreatePositionFromEPD((char *)"4k3/8/8/3B4/8/8/8/4K3 w - -"));

        for (int i = 0; i < 64; i++) {
            position.get()->atkTo[i] = 0;
            position.get()->atkFr[i] = 0;
        }

        RecalcAttacks(position.get());

        const uint64_t occupied = position.get()->mask[White][0] | position.get()->mask[Black][0];
        const uint64_t expectedBishopAttacks = bishop_attacks(d5, occupied);

        Assert::AreEqual((unsigned long long)expectedBishopAttacks,
                         (unsigned long long)position.get()->atkTo[d5]);
        Assert::IsTrue((position.get()->atkFr[e6] & SetMask(d5)) != 0);
        Assert::IsTrue((position.get()->atkFr[c4] & SetMask(d5)) != 0);
    }
};
} // namespace WinAmyTests
