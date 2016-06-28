#pragma once
#include "pa_ringbuffer.h"

bool start_capture_fake(PaUtilRingBuffer *buff);

void stop_capture_fake();

bool start_tx_fake(PaUtilRingBuffer *buff);

void stop_tx_fake();
