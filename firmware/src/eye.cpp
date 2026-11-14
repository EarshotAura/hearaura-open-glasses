// The camera: faces as angles and a distance, never as pictures.
#include <Arduino.h>
#include <list>
#include "esp_camera.h"
#include "human_face_detect_msr01.hpp"
#include "doa.h"
#include "eye.h"

// How wide the lens sees, in degrees. The stock lens is about 66; a
// wide-angle replacement is about 120.
static const float CAMERA_HFOV = 66.0f;

volatile float faceBearing[4], faceElevation[4], faceDistance[4];
volatile int faceCount = 0;
volatile unsigned long facesAt = 0;

bool startCamera() {
  if (!psramFound()) return false;             // a plain XIAO ESP32-S3 has no camera
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = 15; c.pin_d1 = 17; c.pin_d2 = 18; c.pin_d3 = 16;
  c.pin_d4 = 14; c.pin_d5 = 12; c.pin_d6 = 11; c.pin_d7 = 48;
  c.pin_xclk = 10; c.pin_pclk = 13; c.pin_vsync = 38; c.pin_href = 47;
  c.pin_sccb_sda = 40; c.pin_sccb_scl = 39;
  c.pin_pwdn = -1; c.pin_reset = -1;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_RGB565;
  c.frame_size = FRAMESIZE_QVGA;               // 320 x 240
  c.fb_count = 1;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;
  return esp_camera_init(&c) == ESP_OK;
}

void cameraTask(void *) {
  HumanFaceDetectMSR01 detector(0.3F, 0.5F, 4, 0.2F);
  for (;;) {
    camera_fb_t *frame = esp_camera_fb_get();
    if (frame) {
      std::list<dl::detect::result_t> &found =
          detector.infer((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3});
      int n = 0;
      for (auto &face : found) {
        if (n >= 4) break;
        float b, e, d;
        doa_face((face.box[0] + face.box[2]) * 0.5f, (face.box[1] + face.box[3]) * 0.5f,
                 (float)(face.box[2] - face.box[0]), frame->width, frame->height, CAMERA_HFOV, &b, &e, &d);
        faceBearing[n] = b; faceElevation[n] = e; faceDistance[n] = d;
        n++;
      }
      faceCount = n;
      facesAt = millis();
      esp_camera_fb_return(frame);             // the picture is dropped here
    }
    vTaskDelay(pdMS_TO_TICKS(300));
  }
}
