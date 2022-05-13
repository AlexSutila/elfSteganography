#include <iostream>
#include "../../stegelf.h"

int main(int argc, char **argv)
{
	const char *imgPath = argv[1];
	cimg_library::CImg<unsigned char> image(imgPath);
	stegelf::ObjFile objFile(&image);

	int arr[] = {1,5,8,3,2,7};

	void(*mergeSort)(int*,int,int) = (void(*)(int*,int,int))objFile.lookupFunction("mergeSort");
	(*mergeSort)(arr, 0, 5);

	std::cout << "Sorted array: ";
	for (int i = 0; i < 6; i++)
		std::cout << arr[i] << ", ";
	std::cout << std::endl;

	return 0;
}
