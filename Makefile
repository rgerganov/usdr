PROGNAME = usdr
CFLAGS = -Wall -I/opt/src/portaudio/include -I/opt/src/csdr -DUSE_FFTW
LDFLAGS = -L/opt/src/portaudio/lib/.libs -L/opt/src/csdr -lportaudio -lcsdr -lfftw3f -lSDL2

all: $(PROGNAME)

$(PROGNAME): main.cpp dsp.cpp dsp.h pa_ringbuffer.cpp pa_ringbuffer.h pa_memorybarrier.h
	$(CXX) main.cpp dsp.cpp pa_ringbuffer.cpp -o $(PROGNAME) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(PROGNAME) *.o
