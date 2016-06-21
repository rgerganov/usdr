#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "fake_capture.h"
#include "pa_ringbuffer.h"

static FILE *f;
static pthread_t th;
static volatile bool running;

static void *th_callback(void *arg)
{
    float buff[512];
    PaUtilRingBuffer *ring_buff = (PaUtilRingBuffer*) arg;
    while (running) {
        int read = fread(buff, sizeof(float), 512, f);
        PaUtil_WriteRingBuffer(ring_buff, buff, read);
        if (read != 512) {
            break;
        }
        usleep(10 * 1000); //sleep for 10ms
    }
    fprintf(stderr, "th_callback exit\n");
    return NULL;
}

bool start_capture_fake(const char *fname, PaUtilRingBuffer *buff)
{
    f = fopen(fname, "rb");
    if (f == NULL) {
        fprintf(stderr, "cannot open %s\n", fname);
        return false;
    }
    running = true;
    if (pthread_create(&th, NULL, th_callback, buff)) {
        fprintf(stderr, "cannot start thread\n");
        return false;
    }
    return true;
}


void stop_capture_fake()
{
    running = false;
    if (pthread_join(th, NULL)) {
        fprintf(stderr, "error joining thread\n");
    }
    fclose(f);
}
