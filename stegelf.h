#include <iterator>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <elf.h>
#include <errno.h>
#include "CImg.h" // If this causes problems for you, clone the repo and
                  //	set the include directory in the makefile or how
                  //	ever you are compiling

#define BITS_PER_BYTE 8
namespace stegelf
{
	struct ObjFile
	{
	private:
		int objsize; // Will contain the full size of the object file in bytes and be used
		             //		to determine how much space to allocate when extracting binary
		union
		{	// This union contains:
			//		1) hdr, a pointer to the elf header to simplify accessing specific
			//		   header entries
			//		2) base, a pointer to the base address of the entire object file
			const Elf64_Ehdr *hdr;
			const uint8_t *base;
		} obj;
		const Elf64_Shdr *sections; // Will point to the base address of the sections table
		const char *shstrtab;       // Will point to the base address of the section header
		                            //		... string table
		const Elf64_Sym *symbols;   // Will point to the base address of the symbol table
		int numSymbols;             // Will be populated with the symbol count
		const char *strtab;         // Will point to the base address of the string table
		uint64_t pageSize;          // Will be populated with the page size of the system
		uint8_t *textBase;          // Will point to the base address of the text section

	private:
		// Page aligns n, rounding up
		inline uint64_t pageAlign(uint64_t n)
		{
			return (n + (pageSize - 1)) & ~(pageSize - 1);
		}
		// Return a pointer to the base address of a section given the section name
		const Elf64_Shdr *lookupSection(const char *name)
		{
			size_t nameLen = strlen(name);
			for (Elf64_Half i = 0; i < obj.hdr->e_shnum; i++)
			{	// Find sh_name in string table that points to string name
				const char *sectionName = shstrtab + sections[i].sh_name;
				size_t sectionNameLen = strlen(sectionName);
				if (nameLen == sectionNameLen && !strcmp(name, sectionName) && sections[i].sh_size)
						return sections + i;
			}
			return NULL;
		}
		// Initializes the addresses for various tables and sections, important thing is that it finds the
		//		text section and creates a copy of it that is executable (and readable).
		//		...
		// After this is called, you should be able to look for functions in the binary using lookupFunction,
		//		the constructors call this for you.
		void parse()
		{
			// Find sections and section header string tables
			sections = (const Elf64_Shdr*)(obj.base + obj.hdr->e_shoff);
			shstrtab = (const char*)(obj.base + sections[obj.hdr->e_shstrndx].sh_offset);
			// Find symbol table in the sections table
			const Elf64_Shdr *symtab_hdr = lookupSection(".symtab");
			symbols = (const Elf64_Sym*)(obj.base + symtab_hdr->sh_offset);
			numSymbols = symtab_hdr->sh_size / symtab_hdr->sh_entsize;
			// Find string table in the sections table
			const Elf64_Shdr *strtab_hdr = lookupSection(".strtab");
			strtab = (const char*)(obj.base + strtab_hdr->sh_offset);
			// Find section, create executable + readable copy
			const Elf64_Shdr *text_hdr = lookupSection(".text");
			textBase = (uint8_t*)mmap(NULL, pageAlign(text_hdr->sh_size), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			memcpy(textBase, obj.base + text_hdr->sh_offset, text_hdr->sh_size);
			mprotect(textBase, pageAlign(text_hdr->sh_size), PROT_READ | PROT_EXEC);
		}

		// Template function for splitting data types into individual bits and writing each individual
		//		bit over the least significant bit in an image byte. This takes a pointer to a CImg
		//		iterator, and will move that iterator along as it inserts data.
		template<typename T>
		void write(cimg_library::CImg<unsigned char>::iterator *it, T n)
		{
			// Best to work this out on paper to see how the bit manipulation works
			for (int i = 0; i < sizeof(T) * BITS_PER_BYTE; i++)
			{
				(**it) &= ~0x1;
				(**it) |= n & 0x1;
				n >>= 1;
				(*it) += 1;
			}
		}
		// Template function for splitting data types into individual bits and reading each individual
		//		bit from the least significant of all the image bytes containing this piece of data. The
		//		iterator behaves the same was as in the 'write' template function
		template<typename T>
		T read(cimg_library::CImg<unsigned char>::iterator *it)
		{
			T n = 0;
			// Best to work this out on paper as well to see how the bit manipulation works
			for (int i = 0; i < sizeof(T) * BITS_PER_BYTE; i++)
			{
				n |= ((**it) & 0x1) << i;
				(*it) += 1;
			}
			return n;
		}

	public:
		// Use this constructor when you intend insert binary into an image. This will read the object
		//		file off the disk and create a mapping for it in the processes address range.
		ObjFile(const char* oPath)
		{
			struct stat sb; // This exists only to get the file size
			// Get the system page size
			pageSize = sysconf(_SC_PAGESIZE);
			// Opens file, loads file into process address space, then parse
			int fd = open(oPath, O_RDONLY);
			fstat(fd, &sb);
			obj.base = (uint8_t*)mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
			objsize = sb.st_size;
			close(fd);
			parse();
		}
		// Use this constructor when you intend to extract binary from an image. This leaves this objFile
		//		object pretty much blank so that everything can be populated when insert(...) is called.
		ObjFile(cimg_library::CImg<unsigned char> *imgPtr)
		{
			// Does a lot of the same things the other constructor does, it just copies it into memory
			//		(it kinda has to) and then sets the obj base to the obj extracted from the image
			cimg_library::CImg<unsigned char>::iterator it = imgPtr->begin();
			objsize = read<int>(&it);

			// Extract the object from the image
			uint8_t *temp = new uint8_t[objsize];
			for (int i = 0; i < objsize; i++)
				temp[i] = read<unsigned char>(&it);
			// Set the base pointer for the object file and parse
			obj.base = temp;
			parse();
		}
		// Returns a void pointer to the base address of a function with a given name, cast this
		//		to the actual function type and jump to it
		void *lookupFunction(const char *name)
		{
			size_t name_len = strlen(name);
			// Loop through symbols
			for (int i = 0; i < numSymbols; i++)
			{
				if (ELF64_ST_TYPE(symbols[i].st_info) == STT_FUNC)
				{
					// Find st_name in string table to find pointer to the string of the function name
					const char *function_name = strtab + symbols[i].st_name;
					size_t function_name_len = strlen(function_name);

					if (name_len == function_name_len && !strcmp(name, function_name))
						return textBase + symbols[i].st_value;
				}
			}
			return NULL;
		}

		// Call this function to insert the contents of this object file into the given image pointed
		//		to by imgPtr
		void insert(cimg_library::CImg<unsigned char> *imgPtr, const char* path)
		{
			cimg_library::CImg<unsigned char>::iterator it = imgPtr->begin();
			write<int>(&it, objsize);

			// Write entire object to image
			for (int i = 0; i < objsize; i++)
				write<unsigned char>(&it, obj.base[i]);

			// Save updated image
			imgPtr->save(path);
		}
	};
}
#undef BITS_PER_BYTE

