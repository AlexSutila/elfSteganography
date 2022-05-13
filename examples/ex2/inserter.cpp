#include "../../stegelf.h"

int main(int argc, char **argv)
{
	const char *imgPath = argv[1], *oFilePath = argv[2];
	cimg_library::CImg<unsigned char> image(imgPath);
	stegelf::ObjFile objFile(oFilePath);

	objFile.insert(&image, "elgato.bmp");
	return 0;
}
