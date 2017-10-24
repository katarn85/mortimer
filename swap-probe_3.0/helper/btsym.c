/* Return list with names for address in backtrace.
   Copyright (C) 1998,1999,2000,2001,2003,2009 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Ulrich Drepper <drepper@cygnus.com>, 1998.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */


/*
2011-12-15 Jaewon Lim <jaewon81.lim@samsung.com> add hashing for symbols

2011-12-08 Jaewon Lim <jaewon81.lim@samsung.com> get symbol data from binary's symtab when dladdr cannot resolve symbol
*/

#include <assert.h>		// for assert
#include <stdio.h>		// for printf, sprintf
#include <stdlib.h>		// for malloc
#include <string.h>		// for strlen
#include <stddef.h>		// for ptrdiff_t
#include <errno.h>		// for program_invocation_name

#include <sys/types.h>	// for open, fstat
#include <sys/stat.h>	// for open, fstat
#include <fcntl.h>		// for open
#include <unistd.h>		// for fstat
#include <sys/mman.h>	// for mmap, munmap

#include "private_link.h"		// for link_map, ElfW
#include "dahelper.h"
#include "dacollection.h"

#if __ELF_NATIVE_CLASS == 32
# define WORD_WIDTH 8
#else
/* We assyme 64bits.  */
# define WORD_WIDTH 16
#endif

#define FILEPATH_MAX 1024

/* We use this macro to refer to ELF types independent of the native wordsize.
   'ElfW(TYPE)' is used in place of 'Elf32_TYPE' or 'Elf64_TYPE'.  */
#define ELFW(type)  _ElfW (ELF, __ELF_NATIVE_CLASS, type)

/* Result of the lookup functions and how to retrieve the base address.  */
typedef struct link_map *lookup_t;
#define LOOKUP_VALUE(map) map
#define LOOKUP_VALUE_ADDRESS(map) ((map) ? (map)->l_addr : 0)

/* On some architectures a pointer to a function is not just a pointer
   to the actual code of the function but rather an architecture
   specific descriptor. */
#ifndef ELF_FUNCTION_PTR_IS_SPECIAL
# define DL_SYMBOL_ADDRESS(map, ref) \
	(void *) (LOOKUP_VALUE_ADDRESS (map) + ref->st_value)
# define DL_LOOKUP_ADDRESS(addr) ((ElfW(Addr)) (addr))
# define DL_DT_INIT_ADDRESS(map, start) (start)
# define DL_DT_FINI_ADDRESS(map, start) (start)
#endif

/* On some architectures dladdr can't use st_size of all symbols this way.  */
#define DL_ADDR_SYM_MATCH(L, SYM, ADDR) \
	(((ADDR) >= (L)->l_addr + (SYM)->st_value)              \
		&& ((((SYM)->st_shndx == SHN_UNDEF || (SYM)->st_size == 0)       \
			&& ((ADDR) == (L)->l_addr + (SYM)->st_value))         \
			|| ((ADDR) < (L)->l_addr + (SYM)->st_value + (SYM)->st_size)))

// start of implementation by Jaewon Lim
struct _symdata
{
	ElfW(Shdr)	symhdr;
	ElfW(Shdr)	strhdr;
	ElfW(Sym)*	symtab;
	char*		strtab;
};

typedef struct _symdata symdata_t;

// get symbol data from file binary
static symdata_t* _get_symboldata(char* filepath)
{
	int fd;
	struct stat st;
	char *contents;
	symdata_t* pdata;

	// first find in glist
	pdata = (symdata_t*)find_glist(filepath);
	if(pdata != NULL)
	{
		return pdata;
	}

	fd = open(filepath, O_RDONLY | O_CLOEXEC);
	if(fd == -1)
	{
		return pdata;
	}

	if(fstat(fd, &st) == -1)
	{
		close(fd);
		return pdata;
	}

	contents = (char*)mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if(likely(contents != NULL))
	{
		ElfW(Ehdr) * elf_hdr = (ElfW(Ehdr) *)(contents);
		ElfW(Shdr) * sec_hdr = (ElfW(Shdr) *)(contents + elf_hdr->e_shoff);
		int i, symtab_idx = -1;

		for (i = 0; i < elf_hdr->e_shnum; ++i)
		{
			if(unlikely(sec_hdr[i].sh_type == SHT_SYMTAB))
			{
				symtab_idx = i;
				break;
			}
		}

		if(symtab_idx != -1)	//there is symbol table
		{
			int strtab_idx = sec_hdr[symtab_idx].sh_link;
			if(likely((strtab_idx != 0) && (sec_hdr[strtab_idx].sh_type == SHT_STRTAB)))	// associated string table is valid
			{
				pdata = (symdata_t*)malloc(sizeof(symdata_t) +
						sec_hdr[symtab_idx].sh_size +
						sec_hdr[strtab_idx].sh_size);

				if(likely(pdata != NULL))
				{
					memcpy(&(pdata->symhdr), &(sec_hdr[symtab_idx]), sizeof(ElfW(Shdr)));
					memcpy(&(pdata->strhdr), &(sec_hdr[strtab_idx]), sizeof(ElfW(Shdr)));
					pdata->symtab = (ElfW(Sym) *)(pdata + 1);
					pdata->strtab = ((char*)pdata) + sizeof(symdata_t) + sec_hdr[symtab_idx].sh_size;
					memcpy((void*)(pdata->symtab), (void*)(contents + sec_hdr[symtab_idx].sh_offset), sec_hdr[symtab_idx].sh_size);
					memcpy((void*)(pdata->strtab), (void*)(contents + sec_hdr[strtab_idx].sh_offset), sec_hdr[strtab_idx].sh_size);

					if(add_to_glist(filepath, (void*)pdata) == 0)	// fail to add
					{
						free(pdata);
						pdata = NULL;
					}
				}
			}
		}

		munmap((void*)contents, st.st_size);
	}

	close(fd);

	return pdata;
}

int get_map_address(void* symbol, void** map_start, void** map_end)
{
	Dl_info info;
	int status, ret = 0;
	struct link_map* map = NULL;

	status = dladdr1(symbol, &info, (void**)&map, RTLD_DL_LINKMAP);
	if(status && map != NULL)
	{
		*map_start = (void*)(map->l_map_start);
		*map_end = (void*)(map->l_map_end);
		ret = 1;
	}
	else
		ret = 0;

	return ret;
}

// end of implementation by Jaewon Lim

char** cached_backtrace_symbols (void* const* array, int size)
{
	Dl_info info[MAX_STACK_DEPTH];
	int status[MAX_STACK_DEPTH];
	char* chararr[MAX_STACK_DEPTH];
	int cnt;
	size_t total = 0;
	char **result;
	char* foundsym;

	memset(chararr, 0, MAX_STACK_DEPTH * sizeof(char*));

	/* Fill in the information we can get from `dladdr'.  */
	for (cnt = 0; cnt < size; ++cnt)
	{
		struct link_map* map;

		if(find_symbol_hash(array[cnt], &foundsym) <= 0)	// not found or error
		{
			status[cnt] = dladdr1 (array[cnt], &info[cnt], (void**)&map, RTLD_DL_LINKMAP);
			if (status[cnt] && info[cnt].dli_fname && info[cnt].dli_fname[0] != '\0')
			{
				/* We have some info, compute the length of the string which will be
				"<file-name>(<sym-name>+offset) [address].  */
				total += (strlen (info[cnt].dli_fname ?: "")
						+ strlen (info[cnt].dli_sname ?: "")
						+ 3 + WORD_WIDTH + 3 + WORD_WIDTH + 5);

				/* The load bias is more useful to the user than the load
				address.  The use of these addresses is to calculate an
				address in the ELF file, so its prelinked bias is not
				something we want to subtract out.  */
				info[cnt].dli_fbase = (void *) map->l_addr;
			}
			else
				total += 5 + WORD_WIDTH;
		}
		else		// there is a entry for key
		{
			status[cnt] = 0;
			chararr[cnt] = foundsym;
			if(chararr[cnt] != NULL)
				total += (strlen(chararr[cnt]) + 1);
			else
			{
				// this never happened
				total += 100;
			}
		}
	}

	/* Allocate memory for the result.  */
	result = (char **) malloc (size * sizeof (char *) + total);
	if (result != NULL)
	{
		char *last = (char *) (result + size);

		for (cnt = 0; cnt < size; ++cnt)
		{
			result[cnt] = last;

			if(chararr[cnt] != NULL)		// there is a cache
			{
				last += (1 + sprintf(last, "%s", chararr[cnt]));
			}
			else		// there is no cache
			{
				int tstrlen;
				if (status[cnt] && info[cnt].dli_fname != NULL && info[cnt].dli_fname[0] != '\0')
				{
					// We found no symbol name to use, so describe it as relative to the file.
					if (info[cnt].dli_sname == NULL)
						info[cnt].dli_saddr = info[cnt].dli_fbase;

					if (info[cnt].dli_sname == NULL && info[cnt].dli_saddr == 0)
					{
						tstrlen = sprintf (last, "%s(%s) [%p]", info[cnt].dli_fname ?: "", info[cnt].dli_sname ?: "", array[cnt]);
					}
					else
					{
						char sign;
						ptrdiff_t offset;
						if (array[cnt] >= (void *) info[cnt].dli_saddr)
						{
							sign = '+';
							offset = array[cnt] - info[cnt].dli_saddr;
						}
						else
						{
							sign = '-';
							offset = info[cnt].dli_saddr - array[cnt];
						}

						tstrlen = sprintf (last, "%s(%s%c%#tx) [%p]",
								info[cnt].dli_fname ?: "",
								info[cnt].dli_sname ?: "",
								sign, offset, array[cnt]);
					}
				}
				else
				{
					tstrlen = sprintf (last, "[%p]", array[cnt]);
				}
				tstrlen++;

				add_symbol_hash(array[cnt], last, tstrlen);

				last += tstrlen;
			}
		}

		assert (last <= (char *) result + size * sizeof (char *) + total);
	}
	else		// fail to malloc
	{
		// do nothing
	}

	return result;
}

char** da_backtrace_symbols (void* const* array, int size)
{
	Dl_info info[MAX_STACK_DEPTH];
	int status[MAX_STACK_DEPTH];
	char* chararr[MAX_STACK_DEPTH];
	int cnt;
	size_t total = 0;
	char **result;
	char* foundsym;

	memset(chararr, 0, MAX_STACK_DEPTH * sizeof(char*));

	/* Fill in the information we can get from `dladdr'.  */
	for (cnt = 0; cnt < size; ++cnt)
	{
		struct link_map* map;

		if(find_symbol_hash(array[cnt], &foundsym) <= 0)	// not found or error
		{
			status[cnt] = dladdr1 (array[cnt], &info[cnt], (void**)&map, RTLD_DL_LINKMAP);
			if(info[cnt].dli_sname == NULL)
			{
				char filepath[FILEPATH_MAX];	// for file path

				/* If this is the main program the information is incomplete.  */
				if (map->l_name[0] == '\0' && map->l_type == lt_executable)
				{
					strncpy(filepath, program_invocation_name, FILEPATH_MAX - 1);
				}
				else
				{
					int len;
					if(map->l_origin)
					{
						strcpy(filepath, map->l_origin);
						len = strlen(filepath);
						if(len > 0 && filepath[len-1] != '/')
						{
							filepath[len] = '/';
							filepath[len+1] = '\0';
						}
					}
					else
						filepath[0] = '\0';
					strcat(filepath, map->l_name);
				}

				symdata_t* pdata = _get_symboldata(filepath);
				if(pdata != NULL)
				{
					ElfW(Sym) * sym_ent = pdata->symtab;
					char* strtab = pdata->strtab;
					int i, num_syms = pdata->symhdr.sh_size / pdata->symhdr.sh_entsize;

					for(i = 0; i < num_syms; ++i)
					{
						if (ELFW(ST_TYPE) (sym_ent[i].st_info) != STT_TLS
								&& (sym_ent[i].st_shndx != SHN_UNDEF || sym_ent[i].st_value != 0)
								&& DL_ADDR_SYM_MATCH (map, &(sym_ent[i]), DL_LOOKUP_ADDRESS (array[cnt]))
								&& sym_ent[i].st_name < pdata->strhdr.sh_size)
						{
							// We found a symbol close by.  Fill in its name and exact address.
							info[cnt].dli_sname = strtab + ((ElfW(Sym) *)(sym_ent + i))->st_name;
							info[cnt].dli_saddr = DL_SYMBOL_ADDRESS (map, ((ElfW(Sym) *)(sym_ent + i)));
							break;
						}
					}
				}
			}

			if (status[cnt] && info[cnt].dli_fname && info[cnt].dli_fname[0] != '\0')
			{
				/* We have some info, compute the length of the string which will be
				"<file-name>(<sym-name>+offset) [address].  */
				total += (strlen (info[cnt].dli_fname ?: "")
						+ strlen (info[cnt].dli_sname ?: "")
						+ 3 + WORD_WIDTH + 3 + WORD_WIDTH + 5);

				/* The load bias is more useful to the user than the load
				address.  The use of these addresses is to calculate an
				address in the ELF file, so its prelinked bias is not
				something we want to subtract out.  */
//				info[cnt].dli_fbase = (void *) map->l_addr;
			}
			else
				total += 5 + WORD_WIDTH;
		}
		else		// there is a entry for key
		{
			chararr[cnt] = foundsym;
			if(chararr[cnt] != NULL)
				total += (strlen(chararr[cnt]) + 1);
			else
			{
				assert(false);
				total += 100;
			}
		}
	}

	/* Allocate memory for the result.  */
	result = (char **) malloc (size * sizeof (char *) + total);
	if (result != NULL)
	{
		char *last = (char *) (result + size);

		for (cnt = 0; cnt < size; ++cnt)
		{
			result[cnt] = last;

			if(chararr[cnt] != NULL)		// there is a cache
			{
				last += (1 + sprintf(last, "%s", chararr[cnt]));
			}
			else		// there is no cache
			{
				int tstrlen;
				if (status[cnt] && info[cnt].dli_fname != NULL && info[cnt].dli_fname[0] != '\0')
				{
					// We found no symbol name to use, so describe it as relative to the file.
					if (info[cnt].dli_sname == NULL)
						info[cnt].dli_saddr = info[cnt].dli_fbase;

					if (info[cnt].dli_sname == NULL && info[cnt].dli_saddr == 0)
					{
						tstrlen = sprintf (last, "%s(%s) [%p]", info[cnt].dli_fname ?: "", info[cnt].dli_sname ?: "", array[cnt]);
					}
					else
					{
						char sign;
						ptrdiff_t offset;
						if (array[cnt] >= (void *) info[cnt].dli_saddr)
						{
							sign = '+';
							offset = array[cnt] - info[cnt].dli_saddr;
						}
						else
						{
							sign = '-';
							offset = info[cnt].dli_saddr - array[cnt];
						}

						tstrlen = sprintf (last, "%s(%s%c%#tx) [%p]",
								info[cnt].dli_fname ?: "",
								info[cnt].dli_sname ?: "",
								sign, offset, array[cnt]);
					}
				}
				else
				{
					tstrlen = sprintf (last, "[%p]", array[cnt]);
				}
				tstrlen++;

				add_symbol_hash(array[cnt], last, tstrlen);

				last += tstrlen;
			}
		}

		assert (last <= (char *) result + size * sizeof (char *) + total);
	}
	else		// fail to malloc
	{
		// do nothing
	}

	return result;
}

