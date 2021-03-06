//////////////////////////////////////////////////////
//most relevant information can be found in chapter //
//4 of volume 3A of the intel developers' manual,   //
//which starts at pg 2910                           //
//////////////////////////////////////////////////////

#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/sysent.h>
#include <sys/kernel.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
/*struct pmap_statistics {
	int x; };
#include <machine/pmap.h>
#include <machine/vmparam.h>*/

struct vtp_args {
	unsigned long vaddr;
	unsigned long *to_fill; };

/////////////////////////////////////////////////////
//virtual address masks
//pg 2910 of intel developers' manual
#define PML5_MASK(x)	((x)&0x01ff000000000000)	//bits 56 to 48
#define PML4_MASK(x)	((x)&0x0000ff8000000000)	//bits 47 to 39
#define PDPT_MASK(x)	((x)&0x0000007fc0000000)	//bits 38 to 30
#define PD_MASK(x)	((x)&0x000000003fe00000)	//bits 29 to 21
#define PT_MASK(x)	((x)&0x00000000001ff000)	//bits 20 to 12
/////////////////////////////////////////////////////

/////////////////////////////////////////////////////
//page structure entry masks
#define PE_ADDR_MASK(x)	((x)&0x000ffffffffff000)	//bits 51 to 12
#define PE_PS_FLAG(x)	( (x) & ((long)1<<7) )		//page size flag
#define PE_P_FLAG(x) 	((x)&1)				//present flag
/////////////////////////////////////////////////////

/////////////////////////////////////////////////////
#define	DMAP_MIN_ADDRESS	(0xfffff80000000000)
#define PHYS_TO_VIRT(x) 	((x)|DMAP_MIN_ADDRESS)
/////////////////////////////////////////////////////

static int
vtp(struct thread *td, void *args) {
	//uprintf("[debug]: DMAP_MIN_ADDRESS: 0x%lx\n", DMAP_MIN_ADDRESS);
	struct vtp_args *uap=args;
	unsigned long vaddr=uap->vaddr;
	unsigned long *to_fill=uap->to_fill;
	////////////////////////////////////////////////////////////////////
	//asm block checks to see if 4 or 5-level paging is enabled
	//if so, moves the cr3 register into the cr3 variable
	//and sets la57_flag to assert whether 4-level or 5-level
	int la57_flag=0;
	unsigned long cr3=0;
	__asm__ __volatile__ (
		"mov %%cr0, %%rax;"		//check bit 31 of cr0 (PG flag)
		"test $0x80000000, %%eax;"	//deny request if 0
		"jz fail;"			//(ie if paging is not enabled)

		"mov $0xc0000080, %%ecx;"	//check bit 8 of ia32_efer (LME flag)
		"rdmsr;"			//deny request if 0
		"test $0x100, %%eax;"		//(module currently can't handle pae paging)
		"jz fail;"
		
	"success:\n"
		"mov %%cr3, %0;"
		"mov %%cr4, %%rax;"
		"shr $20, %%rax;"
		"and $1, %%rax;"
		"mov %%eax, %1;"
		"jmp break;"
	"fail:\n"
		"mov $0, %0;"
	"break:\n"
	
		: "=r"(cr3), "=r"(la57_flag)
		::"rax", "ecx", "memory");
	if(!cr3) {
		return EOPNOTSUPP; }
	////////////////////////////////////////////////////////////////////
	unsigned long psentry=0, paddr=0;
	////////////////////////////////////////////////////////////////////
	//pml5e (if applicable)
	if(la57_flag) {			//5-level paging
		printf("[debug]: &pml5e:\t0x%lx\n", PHYS_TO_VIRT( PE_ADDR_MASK(cr3)|(PML5_MASK(vaddr)>>45) ));
		psentry=*(unsigned long *)\
			PHYS_TO_VIRT( PE_ADDR_MASK(cr3)|(PML5_MASK(vaddr)>>45) );
		printf("[debug]: pml5e:\t\t0x%lx\n", psentry);
		if(!PE_P_FLAG(psentry)) {
			return EFAULT; }}
	else {
		psentry=cr3; }
	////////////////////////////////////////////////////////////////////
	//pml4e
	uprintf("[debug]: &pml4e:\t0x%lx\n", PHYS_TO_VIRT( PE_ADDR_MASK(psentry)|(PML4_MASK(vaddr)>>36) ));
	psentry=*(unsigned long *)\
		PHYS_TO_VIRT( PE_ADDR_MASK(psentry)|(PML4_MASK(vaddr)>>36) );
	uprintf("[debug]: pml4e:\t\t0x%lx\n", psentry);
	if(!PE_P_FLAG(psentry)) {
		return EFAULT; }
	////////////////////////////////////////////////////////////////////
	//pdpte
	uprintf("[debug]: &pdpte:\t0x%lx\n", PHYS_TO_VIRT( PE_ADDR_MASK(psentry)|(PDPT_MASK(vaddr)>>27) ));
	psentry=*(unsigned long *)\
		PHYS_TO_VIRT( PE_ADDR_MASK(psentry)|(PDPT_MASK(vaddr)>>27) );
	uprintf("[debug]: pdpte:\t\t0x%lx\n", psentry);
	if(PE_PS_FLAG(psentry)) {	//1GB page
		//bits (51 to 30) | bits (29 to 0)
		paddr=(psentry&0x0ffffc00000000)|(vaddr&0x3fffffff);
		return copyout(&paddr, to_fill, sizeof(unsigned long)); }
	if(!PE_P_FLAG(psentry)) {
		return EFAULT; }
	////////////////////////////////////////////////////////////////////
	//pde
	uprintf("[debug]: &pde:\t\t0x%lx\n", PHYS_TO_VIRT( PE_ADDR_MASK(psentry)|(PD_MASK(vaddr)>>18) ));
	psentry=*(unsigned long *)\
		PHYS_TO_VIRT( PE_ADDR_MASK(psentry)|(PD_MASK(vaddr)>>18) );
	uprintf("[debug]: pde:\t\t0x%lx\n", psentry);
	if(PE_PS_FLAG(psentry)) {	//2MB page
		//bits (51 to 21) | bits (20 to 0)
		paddr=(psentry&0x0ffffffffe0000)|(vaddr&0x1ffff);
		return copyout(&paddr, to_fill, sizeof(unsigned long)); }
	if(!PE_P_FLAG(psentry)) {
		return EFAULT; }
	////////////////////////////////////////////////////////////////////
	//pte
	uprintf("[debug]: &pte:\t\t0x%lx\n", PHYS_TO_VIRT( PE_ADDR_MASK(psentry)|(PT_MASK(vaddr)>>9) ));
	psentry=*(unsigned long *)\
		PHYS_TO_VIRT( PE_ADDR_MASK(psentry)|(PT_MASK(vaddr)>>9) );
	uprintf("[debug]: pte:\t\t0x%lx\n", psentry);
	paddr=(psentry&0x0ffffffffff000)|(vaddr&0xfff);
	return copyout(&paddr, to_fill, sizeof(unsigned long)); }
	////////////////////////////////////////////////////////////////////

static
struct sysent vtp_sysent = {
	2,
	vtp };

static int offset=NO_SYSCALL;

static int
load(struct module *module, int cmd, void *arg) {
	int error=0;
	switch(cmd) {
		case MOD_LOAD:
			uprintf("loading syscall at offset %d\n", offset);
			break;
		case MOD_UNLOAD:
			uprintf("unloading syscall from offset %d\n", offset);
			break;
		default:
			error=EOPNOTSUPP;
			break; }
	return error; }

SYSCALL_MODULE(vtp, &offset, &vtp_sysent, load, NULL);
