CIMG=$CIMG_PATH

all:
	g++ inserter.cpp -L/usr/X11Rd/lib -lm -lpthread -lX11 -I $(CIMG) -o inserter
	g++ extractor.cpp -L/usr/X11Rd/lib -lm -lpthread -lX11 -I $(CIMG) -o extractor
