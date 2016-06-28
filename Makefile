PROGNAME = usdr
CFLAGS = -Wall -I/opt/src/portaudio/include -I/opt/src/csdr -I/opt/src/hackrf/host/libhackrf/src -DUSE_FFTW
LDFLAGS = -L/opt/src/portaudio/lib/.libs -L/opt/src/csdr -L/opt/src/hackrf/host/build/libhackrf/src -lpthread -lportaudio -lcsdr -lfftw3f -lSDL2 -lhackrf

all: $(PROGNAME)

$(PROGNAME): main.cpp dsp.cpp dsp.h pa_ringbuffer.cpp fake.cpp pa_ringbuffer.h pa_memorybarrier.h
	$(CXX) main.cpp dsp.cpp pa_ringbuffer.cpp fake.cpp -o $(PROGNAME) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(PROGNAME) *.o
