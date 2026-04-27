// Unity tests for the GAME2048 model layer
// (src/app/game_2048/game2048_contorller.cpp). The class already
// exposes getBoard() returning a non-const int* into its 4x4 grid, so
// tests can deterministically set up a board state, fire a move, and
// assert the post-state — no firmware change needed.
//
// Movement rules verified here mirror standard 2048: tiles slide in
// the move direction, then a single pass merges adjacent equal pairs
// (so [2,2,2,0] -> [4,2,0,0], not [6,0,0,0]), then tiles slide
// again to close gaps.

#include <unity.h>
#include "Arduino.h"  // game2048_contorller.h pulls Arduino.h transitively
#include "app/game_2048/game2048_contorller.h"

void setUp() {}
void tearDown() {}

// Helper: read board cell at (row, col). getBoard() returns a flat
// int* into board[4][4], laid out row-major.
static int cell(GAME2048 &g, int row, int col) {
    return g.getBoard()[row * 4 + col];
}
static void set_cell(GAME2048 &g, int row, int col, int v) {
    g.getBoard()[row * 4 + col] = v;
}

// --- init / judge ---

void test_init_zeros_board() {
    GAME2048 game;
    game.init();
    int *b = game.getBoard();
    for (int i = 0; i < 16; ++i) {
        TEST_ASSERT_EQUAL(0, b[i]);
    }
}

void test_judge_returns_0_when_empty_cells_remain() {
    GAME2048 game;
    game.init();
    TEST_ASSERT_EQUAL(0, game.judge());  // all zeros -> game continues
}

void test_judge_returns_1_when_2048_tile_present() {
    GAME2048 game;
    game.init();
    set_cell(game, 1, 1, 2048);
    TEST_ASSERT_EQUAL(1, game.judge());
}

void test_judge_returns_2_when_full_board_no_merges() {
    GAME2048 game;
    game.init();
    // No two adjacent (horizontal or vertical) cells are equal.
    int pattern[16] = {
        2, 4, 2, 4,
        4, 2, 4, 2,
        2, 4, 2, 4,
        4, 2, 4, 2,
    };
    int *b = game.getBoard();
    for (int i = 0; i < 16; ++i) b[i] = pattern[i];
    TEST_ASSERT_EQUAL(2, game.judge());
}

// --- moveLeft ---

void test_moveLeft_combines_adjacent_pair() {
    // [2,2,0,0] -> [4,0,0,0]
    GAME2048 game; game.init();
    set_cell(game, 0, 0, 2); set_cell(game, 0, 1, 2);
    game.moveLeft();
    TEST_ASSERT_EQUAL(4, cell(game, 0, 0));
    TEST_ASSERT_EQUAL(0, cell(game, 0, 1));
    TEST_ASSERT_EQUAL(0, cell(game, 0, 2));
    TEST_ASSERT_EQUAL(0, cell(game, 0, 3));
}

void test_moveLeft_three_in_row_combines_only_first_pair() {
    // [2,2,2,0] -> [4,2,0,0]  (the leftmost pair merges; trailing 2 stays)
    GAME2048 game; game.init();
    set_cell(game, 0, 0, 2);
    set_cell(game, 0, 1, 2);
    set_cell(game, 0, 2, 2);
    game.moveLeft();
    TEST_ASSERT_EQUAL(4, cell(game, 0, 0));
    TEST_ASSERT_EQUAL(2, cell(game, 0, 1));
    TEST_ASSERT_EQUAL(0, cell(game, 0, 2));
    TEST_ASSERT_EQUAL(0, cell(game, 0, 3));
}

void test_moveLeft_full_row_combines_into_two_pairs() {
    // [2,2,2,2] -> [4,4,0,0]
    GAME2048 game; game.init();
    for (int j = 0; j < 4; ++j) set_cell(game, 0, j, 2);
    game.moveLeft();
    TEST_ASSERT_EQUAL(4, cell(game, 0, 0));
    TEST_ASSERT_EQUAL(4, cell(game, 0, 1));
    TEST_ASSERT_EQUAL(0, cell(game, 0, 2));
    TEST_ASSERT_EQUAL(0, cell(game, 0, 3));
}

void test_moveLeft_slides_through_zeros() {
    // [0,0,0,4] -> [4,0,0,0]
    GAME2048 game; game.init();
    set_cell(game, 0, 3, 4);
    game.moveLeft();
    TEST_ASSERT_EQUAL(4, cell(game, 0, 0));
    TEST_ASSERT_EQUAL(0, cell(game, 0, 3));
}

// --- moveRight (mirror of moveLeft) ---

void test_moveRight_combines_to_right_edge() {
    // [2,2,0,0] -> [0,0,0,4]
    GAME2048 game; game.init();
    set_cell(game, 0, 0, 2); set_cell(game, 0, 1, 2);
    game.moveRight();
    TEST_ASSERT_EQUAL(0, cell(game, 0, 0));
    TEST_ASSERT_EQUAL(0, cell(game, 0, 1));
    TEST_ASSERT_EQUAL(0, cell(game, 0, 2));
    TEST_ASSERT_EQUAL(4, cell(game, 0, 3));
}

// --- moveUp / moveDown (column direction) ---

void test_moveDown_combines_column_to_bottom() {
    // column 0: [2,2,0,0]^T -> [0,0,0,4]^T
    GAME2048 game; game.init();
    set_cell(game, 0, 0, 2);
    set_cell(game, 1, 0, 2);
    game.moveDown();
    TEST_ASSERT_EQUAL(0, cell(game, 0, 0));
    TEST_ASSERT_EQUAL(0, cell(game, 1, 0));
    TEST_ASSERT_EQUAL(0, cell(game, 2, 0));
    TEST_ASSERT_EQUAL(4, cell(game, 3, 0));
}

void test_moveUp_combines_column_to_top() {
    // column 1: [0,0,8,8]^T -> [16,0,0,0]^T
    GAME2048 game; game.init();
    set_cell(game, 2, 1, 8);
    set_cell(game, 3, 1, 8);
    game.moveUp();
    TEST_ASSERT_EQUAL(16, cell(game, 0, 1));
    TEST_ASSERT_EQUAL(0,  cell(game, 1, 1));
    TEST_ASSERT_EQUAL(0,  cell(game, 2, 1));
    TEST_ASSERT_EQUAL(0,  cell(game, 3, 1));
}

int main(int /*argc*/, char ** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_init_zeros_board);
    RUN_TEST(test_judge_returns_0_when_empty_cells_remain);
    RUN_TEST(test_judge_returns_1_when_2048_tile_present);
    RUN_TEST(test_judge_returns_2_when_full_board_no_merges);
    RUN_TEST(test_moveLeft_combines_adjacent_pair);
    RUN_TEST(test_moveLeft_three_in_row_combines_only_first_pair);
    RUN_TEST(test_moveLeft_full_row_combines_into_two_pairs);
    RUN_TEST(test_moveLeft_slides_through_zeros);
    RUN_TEST(test_moveRight_combines_to_right_edge);
    RUN_TEST(test_moveDown_combines_column_to_bottom);
    RUN_TEST(test_moveUp_combines_column_to_top);
    return UNITY_END();
}
