This repo provides a header that allows one to hide compiled C binary from an object file within the least 
significant bits of an image's pixel data. The changes in the image are not noticable to the naked eye and the binary can 
be extracted from the image and executed easily using the header. 

## Compiling

This repo relies on the CImg library for image manipulation. The example in examples/ex1 contains two c++ files 
that both include stegelf.h. There is a Makefile in this directory as well, but both files can be compiled by
doing the following:

````
g++ FILENAME.cpp -L/usr/X11Rd/lib -lm -lpthread -lX11 -I *CIMG_REPO_PATH*
````
The included bitmap comes with a binary already hidden inside it (containing a function computing the n'th fibonacci number),
the source code for that can be found in source.c. 

The inserter.cpp demonstrates how to insert the binary, once it is compiled, into an image. The binary in example/ex1
can be reinserted by doing the following:

````
./inserter frog.bmp source.o
````

The extractor.cpp demonstrates how to re-open a binary already inserted inside an image and look for a function
within that binary. It also calls the fib function in source.o, the parameter can be changed freely:

````
./extractor frog.bmp
````

Note, that I'm not quite done with this yet. Attempting to call other functions or manipulating global variables
(I would imagine) will not work the way you would expect it to. 

## Credit

I would like to give credit to those who have contributed to the CImg library, as CImg is what the image manipulation
is done with in this repository. Here is the link to the CImg repo.
https://github.com/dtschump/CImg

In addition, I would like to give credit to the author of the following article as it explains things thoroughly 
and contains commented C code that was helpful to reference while doing this. 
https://blog.cloudflare.com/how-to-execute-an-object-file-part-1/
