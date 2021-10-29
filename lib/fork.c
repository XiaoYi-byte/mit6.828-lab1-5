// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *)utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	envid_t envid = sys_getenvid();
	//cprintf("err: %d, uvpt[PGNUM(addr)]: %d utf->utf_fault_va: %d\n", err, uvpt[PGNUM(addr)],utf->utf_fault_va);
	if (!(err & FEC_WR)) 
		panic("pgfault error: not writing a page.");
	if (!(uvpt[PGNUM(addr)] & PTE_COW))
		panic("pgfault error: not a COW page.");
	if(!(err & FEC_WR) || !(uvpt[PGNUM(addr)] & PTE_COW))
	{
		cprintf("[%08x] user fault va %08x ip %08x\n", sys_getenvid(), addr, utf->utf_eip);
		panic("Page Fault!");
	}
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
        if ((r = sys_page_alloc(envid, PFTEMP, PTE_W | PTE_U | PTE_P)) != 0) 
        	panic("pgfault: %e", r);
    	memcpy(PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);
    	if ((r = sys_page_map(envid, PFTEMP, envid, ROUNDDOWN(addr, PGSIZE), PTE_W | PTE_U | PTE_P)) != 0) 
        	panic("pgfault: %e", r);
    	if ((r = sys_page_unmap(envid, PFTEMP)) != 0) 
        	panic("pgfault: %e", r);
	// panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	void* vaddr=(void*)(pn*PGSIZE);
	
	if((uvpt[pn] & PTE_SHARE) == PTE_SHARE){
		if ((r = sys_page_map(0, vaddr, envid, vaddr, uvpt[pn] & PTE_SYSCALL)) < 0)
            		return r;
	} else if((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)){
		if ((r = sys_page_map(0, vaddr, envid, vaddr, PTE_P | PTE_U | PTE_COW)) < 0)
            		return r;
        	if ((r = sys_page_map(0, vaddr, 0, vaddr, PTE_P | PTE_U | PTE_COW)) < 0)
            		return r;
	} else if((r = sys_page_map(0, vaddr, envid, vaddr, PTE_P | PTE_U)) < 0) {
		return r;
	}
	// panic("duppage not implemented");
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	envid_t envid;
    	uint32_t addr;
    	int r;

    	set_pgfault_handler(pgfault);
    	envid = sys_exofork();
    	if (envid < 0) 
        	panic("sys_exofork: %e", envid);
    	if (envid == 0) {
        	thisenv = &envs[ENVX(sys_getenvid())];
        	return 0;
    	}

    	for (addr = 0; addr < USTACKTOP; addr += PGSIZE) 
        	if ((uvpd[PDX(addr)] & PTE_P) == PTE_P && (uvpt[PGNUM(addr)] & PTE_P) == PTE_P) 
            		duppage(envid, PGNUM(addr));
	
    	void _pgfault_upcall();

    	if ((r = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_W | PTE_U | PTE_P)) != 0) 
        	panic("fork: %e", r);
    	if ((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) != 0) 
        	panic("fork: %e", r);
    	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) != 0)
        	panic("fork: %e", r);
    	return envid;
	//panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
