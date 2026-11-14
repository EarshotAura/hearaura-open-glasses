// Runs the direction-finding maths on made-up sound: a source at a known
// angle, heard by four microphones with the right delays, plus room noise.
// cc -O2 -o test_doa tests/test_doa.c -lm && ./test_doa
#include <stdio.h>
#include <stdlib.h>
#include "../firmware/src/doa.h"

#define FS 48000.0f
#define N 2048
#define W 0.14f   // left-right spacing, metres
#define D 0.09f   // front-rear spacing

static float source[N + 256];

static float frand(void) { return (float)rand() / RAND_MAX * 2 - 1; }

// Speech-like: noise through a low-pass, so neighbouring samples relate.
static void make_source(void) {
  float y = 0, y2 = 0;
  for (int i = 0; i < N + 256; i++) { y += 0.25f * (frand() - y); y2 += 0.5f * (y - y2); source[i] = y2 * 9000; }
}

// The source as heard `delay` samples later (may be a fraction), plus noise.
static void heard(int16_t *out, float delay, float noise) {
  for (int i = 0; i < N; i++) {
    float t = i + 128 - delay;
    int k = (int)floorf(t);
    float f = t - k;
    float v = source[k] * (1 - f) + source[k + 1] * f;
    out[i] = (int16_t)(v + noise * frand());
  }
}

static float wrap(float d) { while (d > 180) d -= 360; while (d < -180) d += 360; return d; }

int main(void) {
  int failures = 0;
  float worst = 0;
  srand(7);
  static int16_t fl[N], fr[N], rl[N], rr[N];
  for (int angle = -180; angle < 180; angle += 15) {
    for (int trial = 0; trial < 4; trial++) {
      make_source();
      float th = angle / 57.29578f;
      // Each microphone's distance along the direction of the source.
      float sx = sinf(th), sy = cosf(th);
      float px[4] = {-W / 2, W / 2, -W / 2, W / 2}, py[4] = {D / 2, D / 2, -D / 2, -D / 2};
      int16_t *mic[4] = {fl, fr, rl, rr};
      for (int m = 0; m < 4; m++) heard(mic[m], -(px[m] * sx + py[m] * sy) / DOA_SPEED_OF_SOUND * FS, 300);
      float q;
      float got = doa_bearing(fl, fr, rl, rr, N, FS, W, D, 0.3f, &q);
      float err = isnan(got) ? 999 : fabsf(wrap(got - angle));
      if (err > worst) worst = err;
      if (err > 8) { failures++; printf("FAIL angle %d got %.1f quality %.2f\n", angle, got, q); }
    }
  }
  printf("bearing: worst error %.1f degrees over 96 trials\n", worst);

  // Unrelated noise on every microphone must not be given a direction.
  for (int i = 0; i < N; i++) { fl[i] = 3000 * frand(); fr[i] = 3000 * frand(); rl[i] = 3000 * frand(); rr[i] = 3000 * frand(); }
  if (!isnan(doa_bearing(fl, fr, rl, rr, N, FS, W, D, 0.3f, NULL))) { failures++; printf("FAIL noise got a bearing\n"); }

  // Voice detection: quiet, then loud.
  float floor_rms = 0;
  for (int i = 0; i < N; i++) fl[i] = 60 * frand();
  for (int k = 0; k < 20; k++) if (doa_is_voice(fl, N, &floor_rms)) { failures++; printf("FAIL quiet called voice\n"); break; }
  for (int i = 0; i < N; i++) fl[i] = 4000 * frand();
  if (!doa_is_voice(fl, N, &floor_rms)) { failures++; printf("FAIL loud not called voice\n"); }

  // A face 60 px wide, right of centre, in a 320x240 frame with a 66 degree lens.
  float b, e, d;
  doa_face(240, 90, 60, 320, 240, 66, &b, &e, &d);
  if (fabsf(b - 18.0f) > 1.5f || fabsf(e - 6.9f) > 1.5f || fabsf(d - 0.62f) > 0.08f) { failures++; printf("FAIL face %.1f %.1f %.2f\n", b, e, d); }
  float faces[3] = {-25, 4, 30};
  if (doa_match_face(8, faces, 3, 12) != 1 || doa_match_face(70, faces, 3, 12) != -1) { failures++; printf("FAIL match\n"); }

  printf(failures ? "%d FAILED\n" : "all passed\n", failures);
  return failures != 0;
}
