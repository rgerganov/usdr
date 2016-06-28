#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <limits.h>
#include "fake.h"
#include "pa_ringbuffer.h"

#define VOICE_FILE "data/speech48k-float.raw"
#define TX_FILE "output.cfile"

static pthread_t capture_thread;
static volatile bool capture_running;

static pthread_t tx_thread;
static volatile bool tx_running;

static void *capture(void *arg)
{
    float buff[512];
    PaUtilRingBuffer *ring_buff = (PaUtilRingBuffer*) arg;
    FILE *f = fopen(VOICE_FILE, "rb");
    if (f == NULL) {
        fprintf(stderr, "cannot open %s\n", VOICE_FILE);
        return NULL;
    }
    while (capture_running) {
        int read = fread(buff, sizeof(float), 512, f);
        PaUtil_WriteRingBuffer(ring_buff, buff, read);
        if (read != 512) {
            break;
        }
        usleep(10 * 1000); //sleep for 10ms
    }
    fclose(f);
    fprintf(stderr, "capture thread exit\n");
    return NULL;
}

bool start_capture_fake(PaUtilRingBuffer *buff)
{
    if (pthread_create(&capture_thread, NULL, capture, buff)) {
        fprintf(stderr, "cannot start capture thread\n");
        return false;
    }
    capture_running = true;
    return true;
}

void stop_capture_fake()
{
    capture_running = false;
    if (pthread_join(capture_thread, NULL)) {
        fprintf(stderr, "error joining capture thread\n");
    }
}

static void *tx(void *arg)
{
    float buff[512];
    int8_t buff2[512];
    PaUtilRingBuffer *ring_buff = (PaUtilRingBuffer*) arg;

    FILE *f = fopen(TX_FILE, "wb");
    if (f == NULL) {
        fprintf(stderr, "cannot open %s\n", TX_FILE);
        return NULL;
    }
    while (tx_running) {
        int32_t read = PaUtil_ReadRingBuffer(ring_buff, &buff, 512);
        if (read > 0) {
            for (int i = 0; i < read; i++) {
                buff2[i] = buff[i] * SCHAR_MAX;
            }
            fwrite(buff2, 512, sizeof(int8_t), f);
        } else {
            // fprintf(stderr, "U");
        }
        usleep(40);
    }
    fclose(f);
    fprintf(stderr, "tx thread exit\n");
    return NULL;
}

bool start_tx_fake(PaUtilRingBuffer *buff)
{
    if (pthread_create(&tx_thread, NULL, tx, buff)) {
        fprintf(stderr, "cannot start TX thread\n");
        return false;
    }
    tx_running = true;
    return true;
}

void stop_tx_fake()
{
    tx_running = false;
    if (pthread_join(tx_thread, NULL)) {
        fprintf(stderr, "error joining tx thread\n");
    }
}
