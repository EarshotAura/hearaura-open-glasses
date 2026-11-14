// Where a voice is, from four microphones and from a face in a camera frame.
// Plain C with no hardware in it, so the same file runs in the firmware and
// in test_doa.c on any computer. Angles are degrees: 0 straight ahead,
// positive to the wearer's right, positive elevation upward.
#ifndef HEARAURA_DOA_H
#define HEARAURA_DOA_H

#include <math.h>
#include <stdint.h>

#define DOA_SPEED_OF_SOUND 343.0f

// How many samples later `b` hears the sound than `a` does (negative when
// earlier), searched over +/- maxlag and refined between samples with a
// parabola through the peak. `peak` gets the match quality, 0 to 1.
static float doa_delay(const int16_t *a, const int16_t *b, int n, int maxlag, float *peak) {
  double ea = 1e-9, eb = 1e-9;
  for (int i = maxlag; i < n - maxlag; i++) { ea += (double)a[i] * a[i]; eb += (double)b[i] * b[i]; }
  double norm = sqrt(ea * eb);
  double best = -2, at[3] = {0, 0, 0};
  int bestLag = 0;
  double prev = 0, cur = 0;
  for (int lag = -maxlag - 1; lag <= maxlag + 1; lag++) {
    double sum = 0;
    if (lag >= -maxlag && lag <= maxlag) {
      for (int i = maxlag; i < n - maxlag; i++) sum += (double)a[i] * b[i + lag];
      sum /= norm;
    } else {
      sum = -2;
    }
    // Track the value on each side of the best lag for the parabola.
    if (lag > -maxlag - 1 && cur > best) { best = cur; bestLag = lag - 1; at[0] = prev; at[1] = cur; at[2] = sum; }
    prev = cur;
    cur = sum;
  }
  if (peak) *peak = best < 0 ? 0 : (float)best;
  double shift = 0;
  if (at[0] > -1.5 && at[2] > -1.5) {
    double curve = at[0] - 2 * at[1] + at[2];
    if (fabs(curve) > 1e-12) shift = 0.5 * (at[0] - at[2]) / curve;
    if (shift > 0.5) shift = 0.5;
    if (shift < -0.5) shift = -0.5;
  }
  return (float)(bestLag + shift);
}

// Bearing of the loudest sound from four microphones at the corners of a
// rectangle: front-left, front-right, rear-left, rear-right. `width_m` is
// the left-to-right spacing, `depth_m` front-to-rear. Returns NAN when the
// microphones do not agree well enough to trust (`min_quality`, 0 to 1).
static float doa_bearing(const int16_t *fl, const int16_t *fr, const int16_t *rl, const int16_t *rr,
                         int n, float sample_rate, float width_m, float depth_m, float min_quality,
                         float *quality) {
  int lagW = (int)ceilf(width_m / DOA_SPEED_OF_SOUND * sample_rate) + 1;
  int lagD = (int)ceilf(depth_m / DOA_SPEED_OF_SOUND * sample_rate) + 1;
  float q1, q2, q3, q4;
  // Sound from the right reaches the right microphones first, so the left
  // ones hear it later: a positive left-minus-right delay means "right".
  float lr = 0.5f * (doa_delay(fr, fl, n, lagW, &q1) + doa_delay(rr, rl, n, lagW, &q2));
  // Sound from ahead reaches the front first: positive rear-minus-front
  // delay means "ahead".
  float fb = 0.5f * (doa_delay(fl, rl, n, lagD, &q3) + doa_delay(fr, rr, n, lagD, &q4));
  float q = 0.25f * (q1 + q2 + q3 + q4);
  if (quality) *quality = q;
  if (q < min_quality) return NAN;
  float x = lr / sample_rate * DOA_SPEED_OF_SOUND / width_m;   // sine part
  float y = fb / sample_rate * DOA_SPEED_OF_SOUND / depth_m;   // cosine part
  return atan2f(x, y) * 57.29578f;
}

// True when a block is loud enough over the running noise floor to be
// speech worth locating. `floor_rms` is updated slowly on quiet blocks.
static int doa_is_voice(const int16_t *x, int n, float *floor_rms) {
  double e = 0;
  for (int i = 0; i < n; i++) e += (double)x[i] * x[i];
  float rms = (float)sqrt(e / n);
  if (*floor_rms <= 0) *floor_rms = rms;
  int voice = rms > *floor_rms * 3.0f && rms > 150.0f;
  if (!voice) *floor_rms += (rms - *floor_rms) * 0.05f;
  return voice;
}

// A face box from the camera as a direction and a distance. `hfov_deg` is
// the camera's horizontal field of view. Distance assumes an adult face
// about 0.15 m wide, so it is a rough figure: good for near or far, not
// for centimetres.
static void doa_face(float center_x, float center_y, float box_width, int image_w, int image_h, float hfov_deg,
                     float *bearing, float *elevation, float *distance_m) {
  float focal = (image_w * 0.5f) / tanf(hfov_deg * 0.5f / 57.29578f);
  *bearing = atanf((center_x - image_w * 0.5f) / focal) * 57.29578f;
  *elevation = atanf((image_h * 0.5f - center_y) / focal) * 57.29578f;
  *distance_m = box_width > 1 ? focal * 0.15f / box_width : 0;
}

// The face nearest a voice's bearing, if one is within `tolerance_deg`;
// else -1. A voice with a face in view gets the face's exact direction,
// height and distance; a voice without one keeps its sound-only bearing.
static int doa_match_face(float voice_bearing, const float *face_bearings, int faces, float tolerance_deg) {
  int best = -1;
  float bestGap = tolerance_deg;
  for (int i = 0; i < faces; i++) {
    float gap = fabsf(face_bearings[i] - voice_bearing);
    if (gap <= bestGap) { bestGap = gap; best = i; }
  }
  return best;
}

#endif
