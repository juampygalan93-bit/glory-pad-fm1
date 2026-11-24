
/* GLORY PAD FM-1
 * Version 0.1 - HEAVEN
 * Ambient worship pad prototype
 */

#include "fm1.h"

#define OSC_COUNT 6
#define ACCENT RGB(110, 190, 235)
#define DIM RGB(100, 120, 145)

static const char *const NOTES[12] = {
  "C", "C#", "D", "D#", "E", "F",
  "F#", "G", "G#", "A", "A#", "B"
};

static const char *const LABELS[4] = {
  "LEVEL", "TONE", "MOTION", "FADE"
};

static const int offsets[OSC_COUNT] = {
  0, 0, 7, 12, 12, 19
};

static const int weights[OSC_COUNT] = {
  6500, 5000, 4000, 2800, 2200, 1200
};

typedef struct {
  uint32_t phase[OSC_COUNT];
  uint32_t inc[OSC_COUNT];
  int32_t amp;
  int root;
} Pad;

static Pad pads[2];

static volatile int requested_root = 0;
static volatile int playing = 0;

static volatile int knob[4] = {
  95, 45, 35, 75
};

static int current = 0;
static uint32_t motion_phase = 0;

static void tune_pad(Pad *p, int root) {
  p->root = root;

  for (int j = 0; j < OSC_COUNT; j++) {
    uint32_t inc = note_inc(36 + root + offsets[j]);

    if (j == 1) inc += inc / 350;
    if (j == 4) inc -= inc / 280;

    p->inc[j] = inc;
  }
}

void audio(int32_t *out, uint32_t frames) {

  int root = requested_root;
  int active = playing;

  int level = knob[0];
  int tone = knob[1];
  int motion = knob[2];
  int fade = knob[3];

  int target = active ? level * 258 : 0;
  int step = 1 + (127 - fade) / 12;

  if (pads[current].root != root) {
    int next = 1 - current;

    if (pads[next].root == root ||
        pads[next].amp == 0) {
      tune_pad(&pads[next], root);
      current = next;
    }
  }

  for (uint32_t i = 0; i < frames; i++) {

    motion_phase += 9000;

    int32_t movement =
      (sine(motion_phase) * motion) >> 10;

    int32_t left = 0;
    int32_t right = 0;

    for (int b = 0; b < 2; b++) {

      Pad *p = &pads[b];

      int goal = b == current ? target : 0;

      if (p->amp < goal) {
        p->amp += step;
        if (p->amp > goal) p->amp = goal;
      } else if (p->amp > goal) {
        p->amp -= step;
        if (p->amp < goal) p->amp = goal;
      }

      if (!p->amp) continue;

      int32_t l = 0;
      int32_t r = 0;

      for (int j = 0; j < OSC_COUNT; j++) {

        p->phase[j] += p->inc[j];

        int32_t fundamental = sine(p->phase[j]);

        int32_t harmonic =
          (sine(p->phase[j] * 2u) * tone) >> 9;

        int32_t s = fundamental + harmonic;

        int32_t weight = weights[j];

        if (j >= 3) {
          weight += (movement * 2);
          if (weight < 0) weight = 0;
        }

        s = (s * weight) >> 15;

        if (j == 1 || j == 4) {
          l += s / 2;
          r += s;
        } else if (j == 2 || j == 5) {
          l += s;
          r += s / 2;
        } else {
          l += s;
          r += s;
        }
      }

      left += (l * p->amp) >> 15;
      right += (r * p->amp) >> 15;
    }

    out[2 * i] += left;
    out[2 * i + 1] += right;
  }
}

static void centered(
  int y,
  const char *s,
  uint16_t color,
  int font
) {
  rect(0, y, SCREEN_W, text_height(font), BLACK);

  text(
    (SCREEN_W - text_width(s, font)) / 2,
    y, s, color, font
  );
}

static void draw_status(void) {

  centered(18, "GLORY PAD", ACCENT, FONT_BIG);

  centered(52, "HEAVEN", WHITE, FONT_SMALL);

  rect(0, 78, SCREEN_W, 52, BLACK);

  centered(88, NOTES[requested_root],
           WHITE, FONT_BIG);

  centered(
    145,
    playing ? "PAD ON" : "STANDBY",
    playing ? ACCENT : DIM,
    FONT_SMALL
  );
}

static void draw_knob(int k) {

  char value[12];
  int x = k * 60;

  format_int(value, knob[k]);

  rect(x, 180, 60, 52, BLACK);

  text(
    x + (60 - text_width(
      LABELS[k], FONT_SMALL)) / 2,
    180, LABELS[k], DIM, FONT_SMALL
  );

  text(
    x + (60 - text_width(
      value, FONT_BIG)) / 2,
    198, value, ACCENT, FONT_BIG
  );
}

void setup(void) {

  tune_pad(&pads[0], 0);
  tune_pad(&pads[1], 0);

  clear(BLACK);

  draw_status();

  for (int k = 0; k < 4; k++) {
    draw_knob(k);
  }
}

void loop(void) {

  int changed = 0;

  for (int k = 0; k < KEY_COUNT; k++) {

    if (key_pressed(k)) {

      requested_root =
        (KEY_FIRST_NOTE + k) % 12;

      playing = 1;
      changed = 1;
    }
  }

  uint8_t st, d1, d2;

  while (midi_read(&st, &d1, &d2)) {

    if ((st & 0xF0) == 0x90 && d2) {
      requested_root = d1 % 12;
      playing = 1;
      changed = 1;
    }
  }

  int delta = encoder(ENC_SELECT);

  if (delta) {

    int root = (requested_root + delta) % 12;

    if (root < 0) root += 12;

    requested_root = root;
    changed = 1;
  }

  if (button_pressed(BTN_PLAY)) {

    playing = !playing;
    changed = 1;
  }

  for (int k = 0; k < 4; k++) {

    int value =
      knob[k] + encoder(ENC_KNOB1 + k);

    if (value < 0) value = 0;
    if (value > 127) value = 127;

    if (value != knob[k]) {

      knob[k] = value;
      draw_knob(k);
    }
  }

  led_button(BTN_PLAY, playing);

  for (int k = 0; k < KEY_COUNT; k++) {

    led_key(
      k,
      playing &&
      (KEY_FIRST_NOTE + k) % 12 ==
      requested_root
    );
  }

  if (changed) draw_status();
}
