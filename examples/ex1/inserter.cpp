#include "../../stegelf.h"

int main(int argc, char **argv)
{
	const char *imgPath = argv[1], *oFilePath = argv[2];
	cimg_library::CImg<unsigned char> image(imgPath);
	stegelf::ObjFile objFile(oFilePath);

	// This inserts the binary into the image
	objFile.insert(&image, "frog.bmp");
	return 0;
}
