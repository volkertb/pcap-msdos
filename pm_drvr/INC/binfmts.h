#ifndef _LINUX_BINFMTS_H
#define _LINUX_BINFMTS_H

#include <linux/ptrace.h>

/*
 * MAX_ARG_PAGES defines the number of pages allocated for arguments
 * and envelope for the new program. 32 should suffice, this gives
 * a maximum env+arg of 128kB w/4KB pages!
 */
#define MAX_ARG_PAGES 32

/*
 * This structure is used to hold the arguments that are used when loading binaries.
 */
struct linux_binprm{
	char buf[128];
	unsigned long page[MAX_ARG_PAGES];
	unsigned long p;
	int sh_bang;
	int java;		/* Java binary, prevent recursive invocation */
	struct inode * inode;
	int e_uid, e_gid;
	int argc, envc;
	char * filename;	/* Name of binary */
	unsigned long loader, exec;
	int dont_iput;		/* binfmt handler has put inode */
};

/*
 * This structure defines the functions that are used to load the binary formats that
 * linux accepts.
 */
struct linux_binfmt {
	struct linux_binfmt * next;
	struct module *module;
	int (*load_binary)(struct linux_binprm *, struct  pt_regs * regs);
	int (*load_shlib)(int fd);
	int (*core_dump)(long signr, struct pt_regs * regs);
};

extern int register_binfmt(struct linux_binfmt *);
extern int unregister_binfmt(struct linux_binfmt *);

extern int read_exec(struct inode *inode, unsigned long offset,
	char * addr, unsigned long count, int to_kmem);

extern int open_inode(struct inode * inode, int mode);

extern int init_elf_binfmt(void);
extern int init_aout_binfmt(void);
extern int init_script_binfmt(void);
extern int init_java_binfmt(void);

extern int prepare_binprm(struct linux_binprm *);
extern void remove_arg_zero(struct linux_binprm *);
extern int search_binary_handler(struct linux_binprm *,struct pt_regs *);
extern void flush_old_exec(struct linux_binprm * bprm);
extern unsigned long setup_arg_pages(unsigned long p, struct linux_binprm * bprm);
extern unsigned long copy_strings(int argc,char ** argv,unsigned long *page,
		unsigned long p, int from_kmem);

/* this eventually goes away */
#define change_ldt(a,b) setup_arg_pages(a,b)

#endif
