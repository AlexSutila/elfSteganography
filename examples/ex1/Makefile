CIMG_PATH=~/Documents/GitHub/CImg# You might need to change this
LIBS=-L/usr/X11Rd/lib -lm -lpthread -lX11

all: inserter.cpp extractor.cpp
	g++ inserter.cpp $(LIBS) -I $(CIMG_PATH) -o inserter
	g++ extractor.cpp $(LIBS) -I $(CIMG_PATH) -o extractor
