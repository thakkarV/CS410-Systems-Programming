#include "executor.h"
#include "datastructures.h"

// c utils
#include <string.h>
#include <stdlib.h>

// sys utils
#include <signal.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/ptrace.h>

// dwarf
#include <dwarf.h>
#include <libdwarf.h>


void do_run(const char * path, char ** argv)
{
	if ((child_pid = fork()) == 0)
	{
		// ptrace traceme
		if (ptrace(PTRACE_TRACEME, NULL, NULL, NULL) < 0)
		{
			perror("ptrace");
			exit(1);
		}

		execvp(path, argv);
		perror("execvp");
		exit(1);
	}
	else if (child_pid > 0)
	{
		int ret_val;
		waitpid(child_pid, &ret_val, 0);

		// now enable all breakpoints
		breakpoint * bp = bp_list_head;
		while (bp != NULL)
		{
			if (!(bp-> is_enabled))
				enable_breakpoint(bp);

			bp = bp-> next;
		}

		do_continue();
	}
	else
	{
		perror("fork");
		exit(1);
	}
}


void do_set_breakpoint(unsigned int line_num)
{
	void * line_addr = get_dwarf_line_addr_from_line(line_num);

	if (line_addr == NULL)
	{
		printf("No line %d in the current file.\n", line_num);
	}
	else
	{
		breakpoint * bp = alloc_breakpoint(++bp_counter);
		bp-> bp_addr = line_addr;
		bp-> srcfile_line_num = line_num;

		if (is_running)
		{
			enable_breakpoint(bp);
			bp-> is_enabled = true;
		}

		if (bp_list_head == NULL)
		{
			bp_list_head = bp;
		}
		else
		{
			bp_list_head-> previous = bp;
			bp-> next = bp_list_head;
			bp-> previous = NULL;
			bp_list_head = bp;
		}
		printf("Breakpoint %d set at line %d.\n", bp_counter, (int) line_num);
	}
}


void do_unset_breakpoint(unsigned int line_num)
{
	// get
	breakpoint * bp = get_breakpoint_by_line(bp_list_head, line_num);

	if (bp == NULL)
	{
		printf("No breakponint at %d.\n", line_num);
	}
	else
	{
		// deactivate
		if (bp-> is_enabled)
			disable_breakpoint(bp);
		// splice out

		if (bp-> previous != NULL)
			bp-> previous-> next = bp-> next;

		if (bp-> next != NULL)
			bp-> next-> previous = bp-> previous;
		
		if (bp == bp_list_head)
			bp_list_head = bp-> next;
		// dealloc
		free(bp);
	}
}


void do_continue(void)
{
	// check if a breakpoint was hit
	step_over_breakpoint();
	// resume child
	ptrace(PTRACE_CONT, child_pid, NULL, NULL);

	// now wait on the child
	int status;
	waitpid(child_pid, &status, 0);

	// process status
	process_status(status);
}

void do_print(char * var_name)
{

}

void do_quit(void)
{
	// kill child then terminate
	kill(child_pid, SIGKILL);
	terminate = true;
}


void process_status(int status)
{
	if (WIFEXITED(status))
	{
		int ret_val = WEXITSTATUS(status);
		if (ret_val == 0)
			printf("Program exited normally.\n");
		else
			printf("Program exited with code %d.\n", ret_val);

		is_running = false;

		// mark all BPs as disabled after exit
		breakpoint * bp = bp_list_head;
		while (bp != NULL)
		{
			bp-> is_enabled = false;
			bp = bp-> next;
		}
	}
	else
	{
		siginfo_t info;
		ptrace(PTRACE_GETSIGINFO, child_pid, NULL, &info);
		if (info.si_signo == SIGTRAP)
		{
			breakpoint * bp = get_breakpoint_by_addr(bp_list_head, (void *) (get_register(rip) - 1));
			printf("Breakpoint %d at line %d.\n", bp-> bp_count, bp-> srcfile_line_num);
		}
		else
		{
			printf("Process received signal %d.\n", info.si_signo);
		}
	}
}


void step_over_breakpoint(void)
{
	unsigned long rip_val = get_register(rip);
	breakpoint * bp;

	if ((bp = get_breakpoint_by_addr(bp_list_head, (void *) (rip_val - 1))) != NULL)
	{
		if (bp-> is_enabled)
		{
			// first restore orginal instruction
			set_register(rip, rip_val - 1);
			disable_breakpoint(bp);
			// single step
			ptrace(PTRACE_SINGLESTEP, child_pid, NULL, NULL);
			int status;
			waitpid(child_pid, &status, 0);
			// re-enable breakponit
			enable_breakpoint(bp);
		}
	}
}
