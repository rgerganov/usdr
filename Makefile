PROGNAME = usdr
CFLAGS = -Wall -I/opt/src/portaudio/include
LDFLAGS = -L/opt/src/portaudio/lib/.libs -lportaudio

all: $(PROGNAME)

$(PROGNAME): main.cpp
	$(CXX) main.cpp -o $(PROGNAME) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(PROGNAME) *.o
