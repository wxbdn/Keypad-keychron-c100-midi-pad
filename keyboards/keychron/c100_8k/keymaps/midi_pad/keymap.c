// Copyright 2026 roguefort-dev
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Notes + Drum Rack MIDI pad for the Keychron C100 8K.
//
// Derived from roguefort-dev/Keypad-keychron-c100-midi-pad (GPL-2.0-or-later).
// The Scale, Chord and Arpeggiator modes were removed; the USB MIDI transport,
// the one-command DFU flashing hook and the RGB canvas effect are unchanged.
//
// Layout (matrix row 0 = top, col 0 = left, 10 x 10 keys):
//
//   Outer ring            Centre 8 x 8 (rows 1-8, cols 1-8) = 64 pads
//   row 0: Oct-  Oct+  --  Notes  Drums  ...
//   row 9: Stop  Play  Rec  ...
//
// Notes mode: chromatic, left to right then upward, 8 notes per row.
//             Bottom-left pad = C1 (MIDI 36) until shifted.
// Drum mode:  four 4x4 blocks matching Ableton Live's Drum Rack pages
//             (bottom-left 36-51, bottom-right 52-67, top-left 68-83,
//             top-right 84-99), 4 pads per row, rows ascending upward.
//
// Octave -/+ shifts Notes mode by 12 semitones and Drum mode by one 16-pad page.
// Play / Stop / Record / Tap Tempo send CC messages on MIDI channel 16
// (CC 44, 45, 46, 47) for Live's MIDI map.
// Bright - / + (top-right) change LED brightness in five steps; the level is
// saved and restored after unplugging. Every pad sends velocity 127.

#include QMK_KEYBOARD_H

#include "midi.h"
#include "qmk_midi.h"

enum layers {
  NOTES_LAYER,
  DRUM_LAYER,
};

enum custom_keycodes {
  MD_PAD = SAFE_RANGE,
  MD_OCTAVE_DOWN,
  MD_OCTAVE_UP,
  MD_STOP,
  MD_PLAY,
  MD_REC,
  MD_TAP,
  MD_BRIGHT_DOWN,
  MD_BRIGHT_UP,
};

#define MD_N MD_PAD
#define MD_NOTES TO(NOTES_LAYER)
#define MD_DRUMS TO(DRUM_LAYER)
#define XXXXXXX KC_NO

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [NOTES_LAYER] = LAYOUT_tkl_ansi(
        MD_OCTAVE_DOWN, MD_OCTAVE_UP, XXXXXXX, MD_NOTES, MD_DRUMS, XXXXXXX, XXXXXXX, XXXXXXX, MD_BRIGHT_DOWN, MD_BRIGHT_UP,
        XXXXXXX, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, XXXXXXX,
        XXXXXXX, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, XXXXXXX,
        XXXXXXX, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, XXXXXXX,
        XXXXXXX, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, XXXXXXX,
        XXXXXXX, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, XXXXXXX,
        XXXXXXX, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, XXXXXXX,
        XXXXXXX, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, XXXXXXX,
        XXXXXXX, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, XXXXXXX,
        MD_STOP, MD_PLAY, MD_REC, MD_TAP, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX),

    [DRUM_LAYER] = LAYOUT_tkl_ansi(
        MD_OCTAVE_DOWN, MD_OCTAVE_UP, XXXXXXX, MD_NOTES, MD_DRUMS, XXXXXXX, XXXXXXX, XXXXXXX, MD_BRIGHT_DOWN, MD_BRIGHT_UP,
        XXXXXXX, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, XXXXXXX,
        XXXXXXX, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, XXXXXXX,
        XXXXXXX, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, XXXXXXX,
        XXXXXXX, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, XXXXXXX,
        XXXXXXX, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, XXXXXXX,
        XXXXXXX, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, XXXXXXX,
        XXXXXXX, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, XXXXXXX,
        XXXXXXX, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, MD_N, XXXXXXX,
        MD_STOP, MD_PLAY, MD_REC, MD_TAP, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX),
};
// clang-format on

#undef MD_N
#undef MD_NOTES
#undef MD_DRUMS
#undef XXXXXXX

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

#define NO_ACTIVE_NOTE 0xFF
#define MIDI_MAX_NOTE 127
#define MIDI_PAD_CHANNEL 0 // MIDI channel 1 for notes
#define PAD_VELOCITY 127

// Transport controls: CC messages on MIDI channel 16 (zero-based 15), so they
// never collide with note traffic.
#define TRANSPORT_CHANNEL 15
#define TRANSPORT_CC_PLAY 44   // Start
#define TRANSPORT_CC_STOP 45
#define TRANSPORT_CC_RECORD 46 // Global record
#define TRANSPORT_CC_TAP 47    // Tap tempo
// Play / Stop / Record send 127 on press only. Set true to also send 0 on
// release.
#define TRANSPORT_SEND_RELEASE false

// The 64 playable pads are the centre 8 x 8 block of the 10 x 10 matrix.
#define PAD_FIRST_ROW 1
#define PAD_FIRST_COL 1
#define PAD_SIZE 8
#define PAD_LAST_ROW (PAD_FIRST_ROW + PAD_SIZE - 1)

// Notes mode: C1 (36) bottom-left, shifted in octaves.
#define NOTES_ROW_STEP 8
#define NOTES_SHIFT_MIN (-3) // bottom-left = note 0
#define NOTES_SHIFT_MAX 7    // bottom-left = note 120; pads past 127 stay dark

// Drum mode: C1 (36) bottom-left, shifted in 16-pad Drum Rack pages.
#define DRUM_PAGE_SIZE 16
#define DRUM_BANK_MIN (-3)
#define DRUM_BANK_MAX 5

// Brightness levels
#define PAD_BRIGHTNESS 180
#define ROOT_BRIGHTNESS 225
#define PRESSED_BRIGHTNESS 255
#define RING_BRIGHTNESS 180
#define RING_DIM_BRIGHTNESS 60

// Global brightness: five steps applied to every LED except the press flash.
#define BRIGHTNESS_LEVEL_COUNT 5
#define BRIGHTNESS_LEVEL_DEFAULT 4 // full brightness
#define PAD_CONFIG_MAGIC 0xB1

static const uint8_t brightness_levels[BRIGHTNESS_LEVEL_COUNT] = {40, 90, 140,
                                                                   200, 255};

typedef union {
  uint32_t raw;
  struct {
    uint8_t magic;
    uint8_t brightness_level;
  } values;
} pad_config_t;

typedef struct {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} pad_color_t;

#define PAD_COLOR_WHITE ((pad_color_t){0xFF, 0xFF, 0xFF})
#define PAD_COLOR_CYAN ((pad_color_t){0x00, 0xB3, 0xFF})
#define PAD_COLOR_ORANGE ((pad_color_t){0xFF, 0xA2, 0x00})
#define PAD_COLOR_GREEN ((pad_color_t){0x1F, 0xFF, 0x62})
#define PAD_COLOR_MAGENTA ((pad_color_t){0xFF, 0x00, 0xEA})
#define PAD_COLOR_PURPLE ((pad_color_t){0xAE, 0x00, 0xFF})
#define PAD_COLOR_RED ((pad_color_t){0xFF, 0x00, 0x20})
#define PAD_COLOR_YELLOW ((pad_color_t){0xFF, 0xD5, 0x00})

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

static uint8_t active_notes[MATRIX_ROWS][MATRIX_COLS];
static uint8_t active_note_counts[MIDI_MAX_NOTE + 1];
static int8_t notes_shift = 0; // octaves
static int8_t drum_bank = 0;   // 16-pad pages
static uint8_t brightness_level = BRIGHTNESS_LEVEL_DEFAULT;
static bool dfu_sysex_matches = false;
static bool dfu_reboot_requested = false;
static pad_color_t led_frame[RGB_MATRIX_LED_COUNT];

static bool drum_mode_active(void) {
  return get_highest_layer(layer_state) == DRUM_LAYER;
}

// ---------------------------------------------------------------------------
// Note mapping
// ---------------------------------------------------------------------------

// Returns the MIDI note for a pad, or NO_ACTIVE_NOTE when the pad falls
// outside 0..127 (only possible near the ends of the octave/bank range).
static uint8_t note_for_pad(uint8_t row, uint8_t col) {
  const uint8_t r = PAD_LAST_ROW - row; // 0 = bottom row of pads
  const uint8_t c = col - PAD_FIRST_COL;
  int16_t note;

  if (drum_mode_active()) {
    // Four 4x4 blocks, Push-style: bottom-left, bottom-right, top-left,
    // top-right. Inside each block, 4 pads per row, rows ascending upward.
    note = MIDI_PAD_BASE_C_NOTE + (drum_bank * DRUM_PAGE_SIZE) +
           ((r / 4) * 32) + ((c / 4) * 16) + ((r % 4) * 4) + (c % 4);
  } else {
    note = MIDI_PAD_BASE_C_NOTE + (notes_shift * 12) +
           (r * NOTES_ROW_STEP) + c;
  }

  if (note < 0 || note > MIDI_MAX_NOTE) {
    return NO_ACTIVE_NOTE;
  }
  return (uint8_t)note;
}

// ---------------------------------------------------------------------------
// MIDI output
// ---------------------------------------------------------------------------

static void tracked_note_on(uint8_t note) {
  if (active_note_counts[note]++ == 0) {
    midi_send_noteon(&midi_device, MIDI_PAD_CHANNEL, note, PAD_VELOCITY);
  }
}

static void tracked_note_off(uint8_t note) {
  if (active_note_counts[note] > 0 && --active_note_counts[note] == 0) {
    midi_send_noteoff(&midi_device, MIDI_PAD_CHANNEL, note, 0);
  }
}

static void press_pad(uint8_t row, uint8_t col) {
  if (active_notes[row][col] != NO_ACTIVE_NOTE) {
    return;
  }
  const uint8_t note = note_for_pad(row, col);
  if (note == NO_ACTIVE_NOTE) {
    return;
  }
  active_notes[row][col] = note;
  tracked_note_on(note);
}

static void release_pad(uint8_t row, uint8_t col) {
  const uint8_t note = active_notes[row][col];
  if (note == NO_ACTIVE_NOTE) {
    return;
  }
  active_notes[row][col] = NO_ACTIVE_NOTE;
  tracked_note_off(note);
}

// Releases every sounding note and forgets which key started it, so a later
// key release after an octave/mode change cannot leave a stuck note.
static void release_tracked_notes(void) {
  for (uint16_t note = 0; note <= MIDI_MAX_NOTE; ++note) {
    if (active_note_counts[note] > 0) {
      midi_send_noteoff(&midi_device, MIDI_PAD_CHANNEL, note, 0);
      active_note_counts[note] = 0;
    }
  }
  for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
    for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
      active_notes[row][col] = NO_ACTIVE_NOTE;
    }
  }
}

static void shift_range(int8_t direction) {
  if (drum_mode_active()) {
    const int8_t next = drum_bank + direction;
    if (next >= DRUM_BANK_MIN && next <= DRUM_BANK_MAX) {
      release_tracked_notes();
      drum_bank = next;
    }
  } else {
    const int8_t next = notes_shift + direction;
    if (next >= NOTES_SHIFT_MIN && next <= NOTES_SHIFT_MAX) {
      release_tracked_notes();
      notes_shift = next;
    }
  }
}

static void save_brightness(void) {
  pad_config_t config = {.raw = 0};
  config.values.magic = PAD_CONFIG_MAGIC;
  config.values.brightness_level = brightness_level;
  eeconfig_update_user(config.raw);
}

void eeconfig_init_user(void) {
  brightness_level = BRIGHTNESS_LEVEL_DEFAULT;
  save_brightness();
}

static void change_brightness(int8_t direction) {
  const int8_t next = (int8_t)brightness_level + direction;
  if (next >= 0 && next < BRIGHTNESS_LEVEL_COUNT) {
    brightness_level = next;
    save_brightness();
  }
}

static void send_cc_button(uint8_t cc, bool pressed, bool send_release) {
  if (pressed) {
    midi_send_cc(&midi_device, TRANSPORT_CHANNEL, cc, 127);
  } else if (send_release) {
    midi_send_cc(&midi_device, TRANSPORT_CHANNEL, cc, 0);
  }
}

// ---------------------------------------------------------------------------
// One-command flashing: the repo's `make flash` sends this SysEx to reboot the
// running pad into its DFU bootloader.
// ---------------------------------------------------------------------------

static void midi_pad_sysex_callback(MidiDevice *device, uint16_t start_byte,
                                    uint8_t data_length, uint8_t *data) {
  static const uint8_t command[] = {
      SYSEX_BEGIN, 0x7D, 'C', '1', '0', '0', 'D', 'F', 'U', SYSEX_END,
  };
  (void)device;

  if (start_byte == 0) {
    dfu_sysex_matches = true;
  }
  for (uint8_t index = 0; index < data_length; ++index) {
    const uint16_t position = start_byte + index;
    if (position >= sizeof(command) || data[index] != command[position]) {
      dfu_sysex_matches = false;
    }
    if (data[index] == SYSEX_END) {
      if (dfu_sysex_matches && position + 1 == sizeof(command)) {
        dfu_reboot_requested = true;
      }
      dfu_sysex_matches = false;
    }
  }
}

void matrix_scan_user(void) {
  if (dfu_reboot_requested) {
    dfu_reboot_requested = false;
    release_tracked_notes();
    reset_keyboard();
  }
}

void keyboard_post_init_user(void) {
  rgb_matrix_mode_noeeprom(RGB_MATRIX_CUSTOM_MIDI_PAD_CANVAS);
  pad_config_t config = {.raw = eeconfig_read_user()};
  if (config.values.magic == PAD_CONFIG_MAGIC &&
      config.values.brightness_level < BRIGHTNESS_LEVEL_COUNT) {
    brightness_level = config.values.brightness_level;
  } else {
    eeconfig_init_user(); // also replaces data left by the old firmware
  }
  for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
    for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
      active_notes[row][col] = NO_ACTIVE_NOTE;
    }
  }
  midi_register_sysex_callback(&midi_device, midi_pad_sysex_callback);
}

layer_state_t layer_state_set_user(layer_state_t state) {
  static uint8_t previous_layer = NOTES_LAYER;
  const uint8_t next_layer = get_highest_layer(state);
  if (next_layer != previous_layer) {
    release_tracked_notes();
    previous_layer = next_layer;
  }
  return state;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  const uint8_t row = record->event.key.row;
  const uint8_t col = record->event.key.col;
  const bool pressed = record->event.pressed;

  switch (keycode) {
  case MD_PAD:
    if (pressed) {
      press_pad(row, col);
    } else {
      release_pad(row, col);
    }
    return false;

  case MD_OCTAVE_DOWN:
    if (pressed) {
      shift_range(-1);
    }
    return false;

  case MD_OCTAVE_UP:
    if (pressed) {
      shift_range(1);
    }
    return false;

  case MD_STOP:
    send_cc_button(TRANSPORT_CC_STOP, pressed, TRANSPORT_SEND_RELEASE);
    return false;

  case MD_PLAY:
    send_cc_button(TRANSPORT_CC_PLAY, pressed, TRANSPORT_SEND_RELEASE);
    return false;

  case MD_REC:
    send_cc_button(TRANSPORT_CC_RECORD, pressed, TRANSPORT_SEND_RELEASE);
    return false;

  case MD_BRIGHT_DOWN:
    if (pressed) {
      change_brightness(-1);
    }
    return false;

  case MD_BRIGHT_UP:
    if (pressed) {
      change_brightness(1);
    }
    return false;

  case MD_TAP:
    // Tap tempo is a momentary tap: 127 on press, 0 on release, so the
    // controller never stays held on.
    send_cc_button(TRANSPORT_CC_TAP, pressed, true);
    return false;
  }

  return true;
}

// ---------------------------------------------------------------------------
// RGB
// ---------------------------------------------------------------------------

static uint8_t led_index(uint8_t row, uint8_t col) {
  return (row * MATRIX_COLS) + col;
}

static uint8_t scale_component(uint8_t component, uint8_t brightness) {
  return ((uint16_t)component * brightness) / 255;
}

// Writes a colour without the global brightness setting; used for press flashes
// so they stay visible at the lowest level.
static void set_key_color_raw(uint8_t row, uint8_t col, pad_color_t color,
                              uint8_t brightness) {
  const uint8_t led = led_index(row, col);
  if (led >= RGB_MATRIX_LED_COUNT) {
    return;
  }
  led_frame[led] = (pad_color_t){scale_component(color.red, brightness),
                                 scale_component(color.green, brightness),
                                 scale_component(color.blue, brightness)};
}

static void set_key_color(uint8_t row, uint8_t col, pad_color_t color,
                          uint8_t brightness) {
  set_key_color_raw(row, col, color,
                    ((uint16_t)brightness * brightness_levels[brightness_level]) /
                        255);
}

static bool is_black_key(uint8_t pitch_class) {
  switch (pitch_class % 12) {
  case 1:
  case 3:
  case 6:
  case 8:
  case 10:
    return true;
  default:
    return false;
  }
}

static pad_color_t drum_block_color(uint8_t row, uint8_t col) {
  const uint8_t r = PAD_LAST_ROW - row;
  const uint8_t c = col - PAD_FIRST_COL;
  switch ((c / 4) + (2 * (r / 4))) {
  case 0:
    return PAD_COLOR_CYAN; // bottom-left
  case 1:
    return PAD_COLOR_ORANGE; // bottom-right
  case 2:
    return PAD_COLOR_GREEN; // top-left
  default:
    return PAD_COLOR_MAGENTA; // top-right
  }
}

static void render_pads(void) {
  const bool drums = drum_mode_active();
  for (uint8_t row = PAD_FIRST_ROW; row <= PAD_LAST_ROW; ++row) {
    for (uint8_t col = PAD_FIRST_COL; col < PAD_FIRST_COL + PAD_SIZE; ++col) {
      const uint8_t note = note_for_pad(row, col);
      if (note == NO_ACTIVE_NOTE) {
        continue; // outside MIDI range: leave dark
      }
      if (active_note_counts[note] > 0) {
        set_key_color_raw(row, col, PAD_COLOR_WHITE, PRESSED_BRIGHTNESS);
      } else if (drums) {
        set_key_color(row, col, drum_block_color(row, col), PAD_BRIGHTNESS);
      } else if (note % 12 == 0) {
        set_key_color(row, col, PAD_COLOR_ORANGE, ROOT_BRIGHTNESS); // every C
      } else if (is_black_key(note)) {
        set_key_color(row, col, PAD_COLOR_PURPLE, PAD_BRIGHTNESS / 2);
      } else {
        set_key_color(row, col, PAD_COLOR_CYAN, PAD_BRIGHTNESS);
      }
    }
  }
}

static void render_ring(void) {
  const bool drums = drum_mode_active();
  set_key_color(0, 0, PAD_COLOR_ORANGE, RING_BRIGHTNESS); // Octave / bank down
  set_key_color(0, 1, PAD_COLOR_GREEN, RING_BRIGHTNESS);  // Octave / bank up
  set_key_color(0, 3, PAD_COLOR_CYAN,
                drums ? RING_DIM_BRIGHTNESS : PRESSED_BRIGHTNESS); // Notes
  set_key_color(0, 4, PAD_COLOR_ORANGE,
                drums ? PRESSED_BRIGHTNESS : RING_DIM_BRIGHTNESS); // Drums
  set_key_color(0, 8, PAD_COLOR_WHITE, RING_DIM_BRIGHTNESS);        // Bright -
  set_key_color(0, 9, PAD_COLOR_WHITE, RING_BRIGHTNESS);            // Bright +
  set_key_color(PAD_LAST_ROW + 1, 0, PAD_COLOR_WHITE, 100);        // Stop
  set_key_color(PAD_LAST_ROW + 1, 1, PAD_COLOR_GREEN, RING_BRIGHTNESS); // Play
  set_key_color(PAD_LAST_ROW + 1, 2, PAD_COLOR_RED, RING_BRIGHTNESS);   // Record
  set_key_color(PAD_LAST_ROW + 1, 3, PAD_COLOR_YELLOW, RING_BRIGHTNESS); // Tap tempo
}

// Ring keys flash white while held; pads already show their note state.
static void render_ring_presses(void) {
  for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
    for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
      const bool is_pad = row >= PAD_FIRST_ROW && row <= PAD_LAST_ROW &&
                          col >= PAD_FIRST_COL &&
                          col < PAD_FIRST_COL + PAD_SIZE;
      if (!is_pad && matrix_is_on(row, col)) {
        set_key_color_raw(row, col, PAD_COLOR_WHITE, PRESSED_BRIGHTNESS);
      }
    }
  }
}

bool rgb_matrix_indicators_user(void) {
  memset(led_frame, 0, sizeof(led_frame));
  render_ring();
  render_pads();
  render_ring_presses();
  for (uint8_t led = 0; led < RGB_MATRIX_LED_COUNT; ++led) {
    rgb_matrix_set_color(led, led_frame[led].red, led_frame[led].green,
                         led_frame[led].blue);
  }
  return false;
}
