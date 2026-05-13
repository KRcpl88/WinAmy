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

TEST_CLASS(AttackTests) {
  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
    }

    TEST_METHOD(AtkSetPawnWhiteAttacksCorrectSquares) {
        // White pawn on e4 should attack d5 and f5
        char epd[] = "4k3/8/8/8/4P3/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        uint64_t expected = SetMask(d5) | SetMask(f5);
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)position.get()->atkTo[e4]);
    }

    TEST_METHOD(AtkSetPawnBlackAttacksCorrectSquares) {
        // Black pawn on e5 should attack d4 and f4
        char epd[] = "4k3/8/8/4p3/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        uint64_t expected = SetMask(d4) | SetMask(f4);
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)position.get()->atkTo[e5]);
    }

    TEST_METHOD(AtkSetKnightAttacksAllEightSquares) {
        // Knight on d4 attacks 8 squares
        char epd[] = "4k3/8/8/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        uint64_t expected = SetMask(c2) | SetMask(e2) | SetMask(b3) |
                            SetMask(f3) | SetMask(b5) | SetMask(f5) |
                            SetMask(c6) | SetMask(e6);
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)position.get()->atkTo[d4]);
    }

    TEST_METHOD(AtkSetKingAttacksAllEightSquares) {
        // King on e4 attacks 8 surrounding squares
        char epd[] = "4k3/8/8/8/4K3/8/8/8 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        uint64_t expected = SetMask(d3) | SetMask(e3) | SetMask(f3) |
                            SetMask(d4) | SetMask(f4) | SetMask(d5) |
                            SetMask(e5) | SetMask(f5);
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)position.get()->atkTo[e4]);
    }

    TEST_METHOD(AtkSetRookAttacksStopAtBlockers) {
        // Rook on d4 with blockers
        char epd[] = "4k3/8/8/8/1P1R1p2/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        uint64_t occupied = position.get()->mask[White][0] | position.get()->mask[Black][0];
        uint64_t expected = reference_rook_attacks(d4, occupied);
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)position.get()->atkTo[d4]);
    }

    TEST_METHOD(AtkSetBishopAttacksStopAtBlockers) {
        // Bishop on d4 with a blocker on f6
        char epd[] = "4k3/8/5p2/8/3B4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        uint64_t occupied = position.get()->mask[White][0] | position.get()->mask[Black][0];
        uint64_t expected = reference_bishop_attacks(d4, occupied);
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)position.get()->atkTo[d4]);
    }

    TEST_METHOD(AtkSetQueenCombinesRookAndBishopAttacks) {
        // Queen on d4
        char epd[] = "4k3/8/8/8/3Q4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        uint64_t occupied = position.get()->mask[White][0] | position.get()->mask[Black][0];
        uint64_t expectedRook = reference_rook_attacks(d4, occupied);
        uint64_t expectedBishop = reference_bishop_attacks(d4, occupied);
        uint64_t expected = expectedRook | expectedBishop;
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)position.get()->atkTo[d4]);
    }

    TEST_METHOD(AtkFrReflectsAtkTo) {
        // Verify atkFr is consistent with atkTo for all squares
        char epd[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -";
        PositionGuard position(CreatePositionFromEPD(epd));
        for (int sq = 0; sq < 64; sq++) {
            BitBoard atkTo = position.get()->atkTo[sq];
            while (atkTo) {
                int target = FindSetBit(atkTo);
                atkTo &= atkTo - 1;
                Assert::IsTrue(TstBit(position.get()->atkFr[target], sq) != 0,
                    L"atkFr must reflect atkTo");
            }
        }
    }

    TEST_METHOD(AtkClrViaDoMoveClearsOldSquareAttacks) {
        // After moving a knight, its old square should have no attacks
        char epd[] = "4k3/8/8/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        Assert::IsTrue(position.get()->atkTo[d4] != 0);

        move_t move = make_move(d4, e6, 0);
        DoMove(position.get(), move);

        // d4 is now empty, so atkTo[d4] should be 0
        Assert::AreEqual(0ULL, (uint64_t)position.get()->atkTo[d4]);
        // e6 should have knight attacks
        Assert::IsTrue(position.get()->atkTo[e6] != 0);
    }

    TEST_METHOD(AtkClrViaUndoMoveRestoresAttacks) {
        char epd[] = "4k3/8/8/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        uint64_t originalAtkTo = position.get()->atkTo[d4];

        move_t move = make_move(d4, e6, 0);
        DoMove(position.get(), move);
        UndoMove(position.get(), move);

        Assert::AreEqual((unsigned long long)originalAtkTo,
                         (unsigned long long)position.get()->atkTo[d4]);
    }

    TEST_METHOD(RecalcAttacksMatchesIncrementalAttacks) {
        // Set up a complex position, compare RecalcAttacks result with
        // the incrementally maintained data
        char epd[] = "r1bqkb1r/pppppppp/2n2n2/8/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq -";
        PositionGuard position(CreatePositionFromEPD(epd));
        PositionGuard recalced(ClonePosition(position.get()));

        // Clear and recalculate
        for (int i = 0; i < 64; i++) {
            recalced.get()->atkTo[i] = 0;
            recalced.get()->atkFr[i] = 0;
        }
        RecalcAttacks(recalced.get());

        for (int i = 0; i < 64; i++) {
            Assert::AreEqual((unsigned long long)position.get()->atkTo[i],
                             (unsigned long long)recalced.get()->atkTo[i]);
            Assert::AreEqual((unsigned long long)position.get()->atkFr[i],
                             (unsigned long long)recalced.get()->atkFr[i]);
        }
    }

    TEST_METHOD(CaptureUpdatesAttacksCorrectly) {
        // White knight on d4 captures black pawn on e6
        char epd[] = "4k3/8/4p3/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));

        move_t move = make_move(d4, e6, M_CAPTURE);
        DoMove(position.get(), move);

        // Knight now on e6, no piece on d4
        Assert::AreEqual((int)Knight, (int)position.get()->piece[e6]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[d4]);
        Assert::AreEqual(0ULL, (uint64_t)position.get()->atkTo[d4]);
        Assert::IsTrue(position.get()->atkTo[e6] != 0);
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

    TEST_METHOD(RookMagicAttacksOnEdgeSquares) {
        // Rook on a1 with no blockers besides edges
        uint64_t occupied = SetMask(a1);
        uint64_t expected = reference_rook_attacks(a1, occupied);
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)rook_attacks(a1, occupied));

        // Rook on h8
        occupied = SetMask(h8);
        expected = reference_rook_attacks(h8, occupied);
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)rook_attacks(h8, occupied));
    }

    TEST_METHOD(BishopMagicAttacksOnCornerSquares) {
        uint64_t occupied = SetMask(a1);
        uint64_t expected = reference_bishop_attacks(a1, occupied);
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)bishop_attacks(a1, occupied));

        occupied = SetMask(h8);
        expected = reference_bishop_attacks(h8, occupied);
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)bishop_attacks(h8, occupied));
    }
};

TEST_CLASS(MoveTests) {
  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
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
        char epd[] = "4k3/8/8/3pP3/8/8/8/4K3 w - d6";
        PositionGuard position(CreatePositionFromEPD(epd));
        PositionGuard snapshot(ClonePosition(position.get()));

        DoNull(position.get());
        Assert::AreEqual((int)Black, (int)position.get()->turn);
        Assert::AreEqual(0, (int)position.get()->enPassant);

        UndoNull(position.get());
        assert_positions_equal(position.get(), snapshot.get());
    }

    TEST_METHOD(RecalcAttacksRebuildsAtkSetDerivedData) {
        // Position with a white bishop on d5.
        char epd[] = "4k3/8/8/3B4/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));

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

    TEST_METHOD(DoMoveCaptureRemovesCapturedPiece) {
        // White knight captures black pawn
        char epd[] = "4k3/8/4p3/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        PositionGuard snapshot(ClonePosition(position.get()));

        move_t move = make_move(d4, e6, M_CAPTURE);
        DoMove(position.get(), move);

        Assert::AreEqual((int)Knight, (int)position.get()->piece[e6]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[d4]);

        UndoMove(position.get(), move);
        assert_positions_equal(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMoveEnPassantCapturesCorrectly) {
        // White pawn on e5, black pawn just moved d7-d5, en passant on d6
        char epd[] = "4k3/8/8/3pP3/8/8/8/4K3 w - d6";
        PositionGuard position(CreatePositionFromEPD(epd));
        PositionGuard snapshot(ClonePosition(position.get()));

        move_t move = make_move(e5, d6, M_ENPASSANT);
        DoMove(position.get(), move);

        Assert::AreEqual((int)Pawn, (int)position.get()->piece[d6]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[e5]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[d5]);

        UndoMove(position.get(), move);
        assert_positions_equal(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMoveShortCastleMovesKingAndRook) {
        char epd[] = "4k3/8/8/8/8/8/8/4K2R w K -";
        PositionGuard position(CreatePositionFromEPD(epd));
        PositionGuard snapshot(ClonePosition(position.get()));

        move_t move = make_move(e1, g1, M_SCASTLE);
        DoMove(position.get(), move);

        Assert::AreEqual((int)King, (int)position.get()->piece[g1]);
        Assert::AreEqual((int)Rook, (int)position.get()->piece[f1]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[e1]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[h1]);

        UndoMove(position.get(), move);
        assert_positions_equal(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMoveLongCastleMovesKingAndRook) {
        char epd[] = "4k3/8/8/8/8/8/8/R3K3 w Q -";
        PositionGuard position(CreatePositionFromEPD(epd));
        PositionGuard snapshot(ClonePosition(position.get()));

        move_t move = make_move(e1, c1, M_LCASTLE);
        DoMove(position.get(), move);

        Assert::AreEqual((int)King, (int)position.get()->piece[c1]);
        Assert::AreEqual((int)Rook, (int)position.get()->piece[d1]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[e1]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[a1]);

        UndoMove(position.get(), move);
        assert_positions_equal(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMovePromotionChangesType) {
        char epd[] = "7k/4P3/8/8/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        PositionGuard snapshot(ClonePosition(position.get()));

        move_t move = make_move(e7, e8, (Queen << M_PROMOTION_OFFSET));
        DoMove(position.get(), move);

        Assert::AreEqual((int)Queen, (int)position.get()->piece[e8]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[e7]);

        UndoMove(position.get(), move);
        assert_positions_equal(position.get(), snapshot.get());
    }
};
} // namespace WinAmyTests
