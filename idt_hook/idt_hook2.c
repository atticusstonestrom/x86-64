//////////////////////////////////////////////////////
//                                                  //
//                                                  //
//                                                  //
//////////////////////////////////////////////////////

#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/sysent.h>
#include <sys/kernel.h>
#include <sys/syscall.h>
#include <sys/sysproto.h>
#include <sys/systm.h>


struct idte_t {
	unsigned short offset_0_15;
	unsigned short segment_selector;
	unsigned char ist;			//interrupt stack table
	unsigned char type:4;
	unsigned char zero_12:1;
	unsigned char dpl:2;			//descriptor privilege level
	unsigned char p:1;			//present flag
	unsigned short offset_16_31;
	unsigned int offset_32_63;
	unsigned int rsv; }
	__attribute__((packed))
	*zd_idte;

#define ZD_INT 0x00
unsigned long idte_offset;			//contains absolute address of original interrupt handler
struct idtr_t {
	unsigned short lim_val;
	struct idte_t *addr; }
	__attribute__((packed))
	idtr;

__asm__(
	".text;"
	".global asm_hook;"
"asm_hook:;"
	"jmp *(idte_offset);");
extern void asm_hook(void);


static int
init() {
	__asm__ __volatile__ (
		"cli;"
		"sidt %0;"
		"sti;"
		:: "m"(idtr));
	uprintf("[*]  idtr dump\n"
		"[**] address:\t%p\n"
		"[**] lim val:\t0x%x\n"
		"[*]  end dump\n\n",
		idtr.addr, idtr.lim_val);
	zd_idte=(idtr.addr)+ZD_INT;

	idte_offset=(long)(zd_idte->offset_0_15)|((long)(zd_idte->offset_16_31)<<16)|((long)(zd_idte->offset_32_63)<<32);
	uprintf("[*]  old idt entry %d:\n"
		"[**] addr:\t%p\n"
		"[**] segment:\t0x%x\n"
		"[**] ist:\t%d\n"
		"[**] type:\t%d\n"
		"[**] dpl:\t%d\n"
		"[**] p:\t\t%d\n"
		"[*]  end dump\n\n",
		ZD_INT, (void *)idte_offset, zd_idte->segment_selector, 
		zd_idte->ist, zd_idte->type, zd_idte->dpl, zd_idte->p);
	if(!zd_idte->p) {
		uprintf("[*] fatal: handler segment not present\n");
		return ENOSYS; }

	__asm__ __volatile__("cli");
	zd_idte->offset_0_15=((unsigned long)(&asm_hook))&0xffff;
	zd_idte->offset_16_31=((unsigned long)(&asm_hook)>>16)&0xffff;
	zd_idte->offset_32_63=((unsigned long)(&asm_hook)>>32)&0xffffffff;
	__asm__ __volatile__("sti");
	uprintf("[*]  new idt entry %d:\n"
		"[**] addr:\t%p\n"
		"[**] segment:\t0x%x\n"
		"[**] ist:\t%d\n"
		"[**] type:\t%d\n"
		"[**] dpl:\t%d\n"
		"[**] p:\t\t%d\n"
		"[*]  end dump\n\n",
		ZD_INT, (void *)(\
		(long)zd_idte->offset_0_15|((long)zd_idte->offset_16_31<<16)|((long)zd_idte->offset_32_63<<32)),
		zd_idte->segment_selector, zd_idte->ist, zd_idte->type, zd_idte->dpl, zd_idte->p);
	
	//uprintf("%p\n", &asm_hook);
	/*unsigned short cs;
	__asm__ __volatile__("mov %%cs, %0" : "=r"(cs));
	uprintf("cs: 0x%x\n", cs);
	for(int i=0; i<64; i++) {
		uprintf("idt entry %d:\t%p\n", i,
			(void *)((long)idtr.addr[i].offset_0_15|((long)idtr.addr[i].offset_16_31<<16)|((long)idtr.addr[i].offset_32_63<<32))); }*/

	return 0; }

static void
fini() {
	__asm__ __volatile__("cli");
	zd_idte->offset_0_15=idte_offset&0xffff;
	zd_idte->offset_16_31=(idte_offset>>16)&0xffff;
	zd_idte->offset_32_63=(idte_offset>>32)&0xffffffff;
	__asm__ __volatile__("sti"); }

static int
load(struct module *module, int cmd, void *arg) {
	int error=0;
	switch(cmd) {
		case MOD_LOAD:
			error=init();
			break;
		case MOD_UNLOAD:
			fini();
			break;
		default:
			error=EOPNOTSUPP;
			break; }
	return error; }

static moduledata_t idt_hook_mod = {
	"idt_hook",
	load,
	NULL };

DECLARE_MODULE(idt_hook, idt_hook_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);