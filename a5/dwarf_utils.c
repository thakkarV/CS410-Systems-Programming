#include "dwarf_utils.h"
#include "datastructures.h"

// c utils
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

// sys utils
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/ptrace.h>

// dwarf
#include <dwarf.h>
#include <libdwarf.h>

Dwarf_Die cu_die = NULL;

void * get_dwarf_line_addr_from_line(int input_line_num)
{
	Dwarf_Unsigned cu_header_length, abbrev_offset, next_cu_header;
	Dwarf_Half version_stamp, address_size;
	Dwarf_Error err;
	Dwarf_Die no_die = 0;
	if (cu_die == NULL)
	{
		/* Find compilation unit header */
		if(dwarf_next_cu_header(dwarf_dbg,
								&cu_header_length,
								&version_stamp,
								&abbrev_offset,
								&address_size,
								&next_cu_header,
								&err) == DW_DLV_ERROR)
		{
			die("Error reading DWARF cu header\n");
		}

		/* Expect the CU to have a single sibling - a DIE */
		if(dwarf_siblingof(dwarf_dbg, no_die, &cu_die, &err) == DW_DLV_ERROR)
		{
			die("Error getting sibling of CU\n");
		}
	}
	
	int i;
	Dwarf_Error error;
	Dwarf_Signed cnt;
	Dwarf_Line *linebuf;
	bool found = false;
	void * ret_addr = NULL;
	if(dwarf_srclines(cu_die, &linebuf, &cnt, &error) == DW_DLV_OK)
	{
		for(i = 0; i < cnt; ++i)
		{
			Dwarf_Unsigned line_num;
			Dwarf_Addr line_addr;

			if(dwarf_lineno(linebuf[i], &line_num, &error) != DW_DLV_OK)
			{
				die("Error dwarf_lineno");
			}

			if(dwarf_lineaddr(linebuf[i], &line_addr, &error) != DW_DLV_OK)
			{
				die("Error dwarf_lineno");
			}

			if (input_line_num == (unsigned int) line_num)
			{
				bool found = true;
				dwarf_dealloc(dwarf_dbg, linebuf[i], DW_DLA_LINE);
				dwarf_dealloc(dwarf_dbg, linebuf, DW_DLA_LIST);
				ret_addr = (void *) line_addr;
				return ret_addr;
			}

			dwarf_dealloc(dwarf_dbg, linebuf[i], DW_DLA_LINE);
		}
		dwarf_dealloc(dwarf_dbg, linebuf, DW_DLA_LIST);
	}
	return ret_addr;
}

// load ELF. Called by "file" command
void do_load_elf(char * path)
{
	// save path
	elf_path = calloc(1, sizeof(char) * (strlen(path) + 1));
	strcpy(elf_path, path);

	// open elf
	if((elf_fd = open(elf_path, O_RDONLY)) < 0)
	{
		if (errno == ENOENT)
		{
			printf("%s: No such file or directory.\n");
			return;
		}
		else
		{
			perror("open");
			exit(1);
		}
	}

	printf("Reading symbols from %s...", elf_path);

	// init DWARF libs
	if(dwarf_init(elf_fd, DW_DLC_READ, 0, 0, &dwarf_dbg, &dwarf_err) != DW_DLV_OK)
	{
		fprintf(stderr, "Failed DWARF initialization\n");
		exit(1);
	}

	printf("done.\n");
}


// unload ELF. Called by loading a new file in.
void do_unload_elf(void)
{
	// free up the path
	free(elf_path);

	// close the open elf
	close(elf_fd);
	elf_fd = -1;

	// finalize DWARF lib
	if(dwarf_finish(dwarf_dbg, &dwarf_err) != DW_DLV_OK) {
		fprintf(stderr, "Failed DWARF finalization\n");
		exit(1);
	}
	dwarf_dbg = 0;

	// reset breakpoints
	bp_counter = 0;
	breakpoint * bp = bp_list_head;
	while (bp)
	{
		breakpoint * nbp = bp-> next;
		free(bp);
		bp = nbp;
	}
	bp_list_head = NULL;
}


// resgister methods
unsigned long get_register(unsigned reg_num)
{
	struct user_regs_struct regs;
	ptrace(PTRACE_GETREGS, child_pid, NULL, &regs);

	switch (reg_num)
	{
		case rip:
		{
			return regs.rip;
		}
		default:
		{
			return 0;
		}
	}
}


void set_register(unsigned reg_num, unsigned long value)
{
	struct user_regs_struct regs;
	ptrace(PTRACE_GETREGS, child_pid, NULL, &regs);
	switch (reg_num)
	{
		case rip:
		{
			regs.rip = value;
		}
		default:
		{

		}
	}

	ptrace(PTRACE_SETREGS, child_pid, NULL, &regs);
}


void die(char * fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(1);
}
