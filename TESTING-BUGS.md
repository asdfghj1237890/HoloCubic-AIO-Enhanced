# Bugs Surfaced by the Test Framework

Real bugs that were latent in the firmware (or in the test stubs) and
got flushed out as the regression suite was built up. Each entry has
the fix commit, the symptom, the root cause, and which test mechanism
would have caught it on subsequent regressions.

---

## 1. stockmarket — `lv_obj_del(stockmarket_gui)` deletes the active screen

**Severity:** crash on app exit
**Fix:** commit `ba4fb82` — switched
`lv_obj_del` to `lv_obj_clean` in `stockmarket_gui_del`
**File:** `AIO_Firmware_PIO/src/app/stockmarket/stockmarket_gui.c:230`

### Symptom

stockmarket's smoke scenario reliably segfaulted partway through the
RETURN action. With nothing but `Serial.println` available it was
silent — process exited 0 in CI even though no scenario steps ran past
the crash, and the suite quietly flagged "completed with 0 failure(s)"
because the SIGSEGV handler had `_exit(0)`-style semantics that
bypassed the harness exit code.

### Root cause

`stockmarket_gui_del` was the only `_gui_del` in the codebase calling
`lv_obj_del` on its top-level screen object — every other app
(bilibili, anniversary, example, game_2048, game_snake, heartbeat)
uses `lv_obj_clean`. The order matters because
`AppController::app_exit` invokes the app's `exit_callback` *before*
`app_control_display_scr` loads the next screen. When stockmarket
deleted its active screen inside that callback, LVGL set
`disp->act_scr = NULL` (and printed `"the active screen was deleted"`
that we eventually saw in the log), and the next refresh tick
segfaulted in `lv_obj_update_layout` dereferencing the NULL act_scr.

### How a regression would be caught

- **Track A** segfault in the stockmarket smoke scenario — the SIGSEGV
  handler installed in `test/harness/main.cpp` prints a backtrace to
  stderr; the CI workflow runs addr2line over the `+0xN` offsets so
  the trace shows file:line. The crash propagates as exit code 139
  which the workflow now correctly captures (this incident also
  uncovered a workflow-side `if ! cmd; then rc=$?` bug that was
  swallowing the crash; fixed in the same series).

---

## 2. game_2048 — `judge()` off-by-one made defeat unreachable

**Severity:** logic — Game state can never report defeat; would have
shipped to users as "game continues forever once board fills"
**Fix:** commit `13c88b2` — `<= SCALE_SIZE *
SCALE_SIZE` → `< SCALE_SIZE * SCALE_SIZE` in both win-check and
empty-check loops
**File:** `AIO_Firmware_PIO/src/app/game_2048/game2048_contorller.cpp::GAME2048::judge()`

### Symptom

A unit-test setup that filled the 4×4 board with a non-merging
2/4/2/4 alternating pattern (no zeros, no adjacent equal) expected
`judge()` to return 2 (defeat). It returned 0 (continue).

### Root cause

```cpp
for (int i = 0; i <= SCALE_SIZE * SCALE_SIZE; i++) {  // <= 16, off by one
    if (board[i / 4][i % 4] == 0) return 0;
}
```

`i <= 16` reads `board[16/4][16%4] = board[4][0]` — one past the 4×4
array. The class layout puts `previous[0][0]` immediately after, which
sits at 0 post-init, so the empty-check loop always saw a zero in that
17th read and returned 0. The win-check loop has the same `<=` but is
harmless: `previous[0][0]` isn't ≥ 2048, so no false-positive win.

### How a regression would be caught

- **Track B** `test_judge_returns_2_when_full_board_no_merges` now
  asserts the expected return-2 path. Same test caught the original.

---

## 3. media_player — `calloc` of a struct containing `fs::File` is UB

**Severity:** crash on the first SD-card-resolved fetch
**Fix:** commit `e31e70f` — placement-new the
`File` member after `calloc`
**File:** `AIO_Firmware_PIO/src/app/media_player/media_player.cpp::media_player_init`

### Symptom

After the SD fixture work made `tf.listDir("/movie")` actually return
a non-empty list, media_player's first process tick segfaulted in
`fs::File::operator=(fs::File&&)` → `String::operator=(String&&)`.

### Root cause

```cpp
struct MediaAppRunData {
    ...
    File file;       // contains String fname / std::string s
};
run_data = (MediaAppRunData *)calloc(1, sizeof(MediaAppRunData));
...
run_data->file = tf.open(file_name);
```

`calloc` zero-initialises the raw memory but **never runs constructors**.
The `String` inside `File` ends up with a `std::string` whose internal
SSO buffer pointers are zeroed instead of properly set up. The
`run_data->file = ...` move-assign then dereferences those pointers and
crashes. Worked on ESP32 because Arduino's `String` is more permissive
about zero-initialised state than `std::string`.

### How a regression would be caught

- **Track A** SIGSEGV during the media smoke scenario, with the
  addr2line trace pointing at line 110 of media_player.cpp. The fix
  is a two-line `new (&run_data->file) File()` placement-new with a
  comment cross-referencing this failure mode.

---

## 4. FlashFS — `mkdir` parent dir doesn't exist, all writes silently fail

**Severity:** infrastructure — `read_config` / `write_config` across
every app were no-ops on the host harness, hidden by the firmware's
graceful-fallback-to-defaults behaviour
**Fix:** commit `ae1058e` — repointed
`FLASH_FIXTURE_DIR` from `"test/fixtures/flash"` to
`"../test/fixtures/flash"`
**File:** `test/stubs/stubs_runtime.cpp`

### Symptom

The new `flash_seed` scenario directive (added for the Sina
stockmarket test) wrote `/stockmarket.cfg` before app_init ran but
`read_config` still picked up the default `AAPL/US` config. The seed
was ignored — `parse_yahoo_data` ran instead of `parse_sina_data`.

### Root cause

```cpp
static const char *FLASH_FIXTURE_DIR = "test/fixtures/flash";  // wrong
...
mkdir(FLASH_FIXTURE_DIR, 0755);
FILE *f = fopen(full.c_str(), "wb");
if (!f) return;
```

The native_test binary runs from `lv_simulater_platformio/`, so the
relative path resolved to `lv_simulater_platformio/test/fixtures/flash`
— a directory whose parent (`lv_simulater_platformio/test/`) does not
exist. `mkdir` returns `ENOENT` (we don't check), `fopen` returns NULL
(silently bailed), and every `writeFile` was a no-op. Subsequent
`readFile` returned 0 bytes, which firmware `read_config` paths
universally interpret as "first boot — write defaults".

This had been latent since the FlashFS stub was first added. No
regression noticed because every scenario that writes a config also
re-derives its data on each boot, so the missing persistence wasn't
visible from the outside.

### How a regression would be caught

- **Track A** the Sina stockmarket scenario asserts (via screenshot
  diff) that the parsed data shows `海得控制 / 11.65` rather than
  `AAPL / 175.50`. If the seeded config doesn't reach `read_config`
  the wrong stock would render.
- More generally: any scenario that uses `flash_seed` is now an
  end-to-end test of the FlashFS read/write pipeline.

---

## Notes on detection

The four bugs span four distinct mechanisms — and the framework's value
is that adding *any one of these mechanisms first* would have surfaced
the bug as soon as the corresponding code path got exercised:

| Bug | Detection mechanism |
|---|---|
| stockmarket active-screen del | Track A scenario + SIGSEGV+addr2line |
| game_2048 judge() | Track B Unity assertion |
| media_player calloc/String UB | Track A scenario + SD fixture path coverage |
| FlashFS mkdir | flash_seed end-to-end through screenshot diff |
