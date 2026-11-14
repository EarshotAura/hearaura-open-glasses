// The camera, kept in its own file: the camera library and the motion
// sensor library both define a type called sensor_t, so they cannot be
// included in the same file.
#pragma once

extern volatile float faceBearing[4], faceElevation[4], faceDistance[4];
extern volatile int faceCount;
extern volatile unsigned long facesAt;

// False on a board with no camera (a plain XIAO ESP32-S3).
bool startCamera();
// Runs for ever: finds faces about three times a second and keeps only
// their directions and distances.
void cameraTask(void *);
