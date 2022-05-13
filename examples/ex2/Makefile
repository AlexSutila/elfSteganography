CIMG_PATH=~/Documents/GitHub/CImg# You might need to change this
LIBS=-L/usr/X11Rd/lib -lm -lpthread -lX11

all: source.c inserter.cpp extractor.cpp
	gcc -c source.c
	g++ inserter.cpp $(LIBS) -I $(CIMG_PATH) -o inserter
	g++ extractor.cpp $(LIBS) -I $(CIMG_PATH) -o extractor
