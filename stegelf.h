#include <iterator>
#include <string>
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
		enum x86_64_relocationTypes
		{
			x86_64_NONE      = 0,	/* No reloc */
			x86_64_64        = 1,	/* Direct 64 bit  */
			x86_64_PC32      = 2,	/* PC relative 32 bit signed */
			x86_64_GOT32     = 3,	/* 32 bit GOT entry */
			x86_64_PLT32     = 4,	/* 32 bit PLT address */
			x86_64_COPY      = 5,	/* Copy symbol at runtime */
			x86_64_GLOB_DAT  = 6,	/* Create GOT entry */
			x86_64_JUMP_SLOT = 7,	/* Create PLT entry */
			x86_64_RELATIVE  = 8,	/* Adjust by program base */
			x86_64_GOTPCREL  = 9,	/* 32 bit signed pc relative offset to GOT */
			x86_64_32        = 10,	/* Direct 32 bit zero extended */
			x86_64_32S       = 11,	/* Direct 32 bit sign extended */
			x86_64_16        = 12,	/* Direct 16 bit zero extended */
			x86_64_PC16      = 13,	/* 16 bit sign extended pc relative */
			x86_64_8         = 14,	/* Direct 8 bit sign extended  */
			x86_64_PC8       = 15,	/* 8 bit sign extended pc relative */
			x86_64_PC64      = 24	/* Place relative 64-bit signed */
		};
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
		uint8_t *dataBase;          // Will point to the base address of the data section
		uint8_t *rodataBase;        // Will point to the base address of the rodata section

	private:
		// Page aligns n, rounding up
		inline uint64_t pageAlign(uint64_t n)
		{
			return (n + (pageSize - 1)) & ~(pageSize - 1);
		}
		// Returns a pointer to the base address of a section given the section name
		const Elf64_Shdr *lookupSection(std::string name)
		{
			for (Elf64_Half i = 0; i < obj.hdr->e_shnum; i++)
			{	// Find sh_name in string table that points to string name
				std::string sectionName(shstrtab + sections[i].sh_name);
				if (name == sectionName && sections[i].sh_size != 0) return sections + i;
			}
			return nullptr;
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

			// Find symbol table entry in the sections table
			const Elf64_Shdr *symtab_hdr = lookupSection(".symtab");
			symbols = (const Elf64_Sym*)(obj.base + symtab_hdr->sh_offset);
			numSymbols = symtab_hdr->sh_size / symtab_hdr->sh_entsize;

			// Find string table entry in the sections table
			const Elf64_Shdr *strtab_hdr = lookupSection(".strtab");
			strtab = (const char*)(obj.base + strtab_hdr->sh_offset);

			// Find the text entry in the sections table
			const Elf64_Shdr *text_hdr = lookupSection(".text");
			if (text_hdr != nullptr)
			{	// Allocate memory for text if it exists
				textBase = (uint8_t*)mmap(nullptr, pageAlign(text_hdr->sh_size), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
				// Create duplicate of text section with read and execute permissions
				memcpy(textBase, obj.base + text_hdr->sh_offset, text_hdr->sh_size);
				mprotect(textBase, pageAlign(text_hdr->sh_size), PROT_READ | PROT_EXEC);
			}

			// Find the data entry in the sections table
			const Elf64_Shdr *data_hdr = lookupSection(".data");
			if (data_hdr != nullptr)
			{	// Allocate memory for the data if it exists
				dataBase = (uint8_t*)mmap(nullptr, pageAlign(data_hdr->sh_size), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
				// Create a duplicate of the data section, no need to change perms
				memcpy(dataBase, obj.base + data_hdr->sh_offset, data_hdr->sh_size);
			}

			// Find the rodata entry in the sections table
			const Elf64_Shdr *rodata_hdr = lookupSection(".rodata");
			if (rodata_hdr != nullptr)
			{	// Allocate memory for the rodata if it exists
				rodataBase = (uint8_t*)mmap(nullptr, pageAlign(rodata_hdr->sh_size), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
				// Create a duplicate of the data section, no need to change perms
				memcpy(rodataBase, obj.base + rodata_hdr->sh_offset, rodata_hdr->sh_size);
			}
		}

	public:
		/*
			I have decided to make the following two template functions public, as they are template functions
				for reading and writing different data types to and from the given image, .o files don't have
				to necessarily be the only thing that is embedded in what ever image you are manipulating :)
		*/

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
			obj.base = (uint8_t*)mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
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
		void *lookupFunction(std::string name)
		{
			// Loop through symbols
			for (int i = 0; i < numSymbols; i++)
			{
				// If symbols is a function symbol
				if (ELF64_ST_TYPE(symbols[i].st_info) == STT_FUNC)
				{
					// Find st_name in string table to find pointer to the string of the function name
					std::string function_name((const char*)(strtab + symbols[i].st_name));
					if (name == function_name) return textBase + symbols[i].st_value;
				}
			}
			return nullptr;
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

