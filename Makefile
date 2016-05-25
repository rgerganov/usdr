PROGNAME = usdr
CFLAGS = -Wall -I/opt/src/portaudio/include -I/opt/src/csdr -DUSE_FFTW
LDFLAGS = -L/opt/src/portaudio/lib/.libs -L/opt/src/csdr -lportaudio -lcsdr -lfftw3f

all: $(PROGNAME)

$(PROGNAME): main.cpp dsp.cpp
	$(CXX) main.cpp dsp.cpp -o $(PROGNAME) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(PROGNAME) *.o
