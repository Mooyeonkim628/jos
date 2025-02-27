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
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if ((err & FEC_WR) == 0 || (uvpd[PDX(addr)] & PTE_P) == 0 || (~uvpt[PGNUM(addr)] & (PTE_COW | PTE_P)) != 0) 
		panic("failed pgdault");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	addr = (void *)ROUNDDOWN(addr, PGSIZE);
	if (sys_page_alloc(0, (void *)PFTEMP, PTE_P | PTE_W | PTE_U) < 0) 
		panic("sys_page_alloc failed");
	memcpy((void *)PFTEMP, addr, PGSIZE);
	if (sys_page_map(0, (void *)PFTEMP, 0, addr, PTE_P | PTE_W | PTE_U) < 0) 
		panic("sys_page_map failed");
	if (sys_page_unmap(0, (void *)PFTEMP) < 0) 
		panic("sys_page_unmap failed");
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
	void *addr = (void *)(pn * PGSIZE);

	if (uvpt[pn] & PTE_SHARE) {
		sys_page_map(0, addr, envid, addr, PTE_SYSCALL);
		return 0;
	}

	if ((uvpt[pn] & (PTE_W | PTE_COW)) != 0) {
		if (sys_page_map(0, addr, envid, addr, PTE_P | PTE_U | PTE_COW) < 0) 
			panic("duppage failed: sys_page_map envid");	
		if (sys_page_map(0, addr, 0, addr, PTE_P | PTE_U | PTE_COW) < 0) 
			panic("duppage failed: sys_page_map 0");	
	} 
	else if (sys_page_map(0, addr, envid, addr, PTE_P | PTE_U) < 0) 
			panic("duppage failed: sys_page_map envid");

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
	set_pgfault_handler(pgfault);

	envid_t envid = sys_exofork();
	if (envid < 0) 
		panic("sys_exofork failed");
	else if (envid == 0) {
		thisenv = envs + ENVX(sys_getenvid());
		return envid;
	}

	uint32_t addr;
	for (addr = UTEXT; addr < UTOP; addr += PGSIZE) {
		if (addr != UXSTACKTOP - PGSIZE) {
			if (((uvpd[PDX(addr)] & PTE_P) != 0) && ((~uvpt[PGNUM(addr)] & (PTE_P | PTE_U)) == 0)) 
				duppage(envid, addr / PGSIZE);
		}
	}

	if (sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W) < 0) 
		panic("fork: sys_page_alloc Failed");
	extern void _pgfault_upcall(void);
	if (sys_env_set_pgfault_upcall(envid, _pgfault_upcall) < 0) 
		panic("fork: sys_env_set_pgfault_upcall Failed");
	
	if (sys_env_set_status(envid, ENV_RUNNABLE) < 0) 
		panic("fork: sys_env_set_status Failed");

	return envid;
}

int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
