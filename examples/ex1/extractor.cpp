#include <iostream>
#include "../../stegelf.h"

int main(int argc, char **argv)
{
	const char *imgPath = argv[1];
	cimg_library::CImg<unsigned char> image(imgPath);

	// Opening the objFile with this constructor
	//		does everything for you. Assuming
	//		all goes well, just look for your
	//		function and execute it.
	stegelf::ObjFile objFile(&image);

	int(*fib)(int) = (int(*)(int))objFile.lookupFunction("fib"); // Gross, I know
	std::cout << "The 12th fibonacci number is " << (*fib)(12) << std::endl;
	return 0;
}
