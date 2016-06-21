#pragma once
#include "pa_ringbuffer.h"

bool start_capture_fake(const char *fname, PaUtilRingBuffer *buff);

void stop_capture_fake();
