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
/*#include <sys/lock.h>
#include <sys/sx.h>
#include <sys/mutex.h>*/
#include <sys/malloc.h>

#define ZD_INT 0x00
#define IDT_ENTRY_SIZE 8
struct idte_t {
	unsigned short offset_0_15;
	unsigned short segment_selector;
	unsigned char ist;	//look up interrupt stack table; if 0, field is unused
	unsigned char type:4;
	unsigned char zero_12:1;
	unsigned char dpl:2;
	unsigned char p:1;
	unsigned short offset_16_31;
	unsigned int offset_32_63;
	unsigned int rsv; }
	__attribute__((packed));
struct idte_t *old_idt;
struct idte_t *new_idt;
//void (*idte_offset)(void);
unsigned long idte_offset;
struct idtr_t {
	unsigned short lim_val;
	unsigned long addr; }
	__attribute__((packed))
	old_idtr, new_idtr;

__asm__(
	".global asm_hook;"
"asm_hook:;"
	"jmp *idte_offset;");
	//"push idte_offset;"
	//"push $0xffffffff81080f90;"
	//"ret;");
extern void asm_hook(void);

static int
load(struct module *module, int cmd, void *arg) {
	int error=0;
	switch(cmd) {
		case MOD_LOAD:
			__asm__ __volatile__ ("sidt old_idtr;");
			uprintf("[*]  idtr dump\n"
				"[**] address:\t%p\n"
				"[**] lim val:\t0x%x\n"
				"[*]  end dump\n\n",
				(void *)old_idtr.addr, old_idtr.lim_val);
			old_idt=(struct idte_t *)old_idtr.addr;
			
			uprintf("[*]  intializing new idt\n"
				"[**] allocating %d bytes\n",
				old_idtr.lim_val+1);
			new_idt=malloc(old_idtr.lim_val+1, M_TEMP, M_NOWAIT);
			if(new_idt==NULL) {
				uprintf("[*] failed to allocate, returning enomem\n");
				error=ENOMEM;
				break; }
			uprintf("[**] new idt:\t%p\n", new_idt);
			memcpy(new_idt, old_idt, old_idtr.lim_val+1);
			uprintf("[**] copied:\t%p\n", old_idt);
			uprintf("[**] filling new idtr\n");
			new_idtr.addr=(unsigned long)new_idt;
			new_idtr.lim_val=old_idtr.lim_val;
			uprintf("[*]  initialization complete\n\n");

			uprintf("[*]  old idt entry %d:\n"
				"[**] addr:\t%p\n"
				"[**] ist:\t%d\n"
				"[**] type:\t%d\n"
				"[**] dpl:\t%d\n"
				"[**] p:\t\t%d\n"
				"[*]  end dump\n\n",
				ZD_INT, (void *)(\
				(long)old_idt[ZD_INT].offset_0_15|((long)old_idt[ZD_INT].offset_16_31<<16)|((long)old_idt[ZD_INT].offset_32_63<<32)),
				old_idt[ZD_INT].ist, old_idt[ZD_INT].type, old_idt[ZD_INT].dpl, old_idt[ZD_INT].p);
			//check for present flag

			/*new_idt[ZD_INT].offset_0_15=((unsigned long)(&asm_hook))&0xffff;
			new_idt[ZD_INT].offset_16_31=((unsigned long)(&asm_hook)>>16)&0xffff;
			new_idt[ZD_INT].offset_32_63=((unsigned long)(&asm_hook)>>32)&0xffffffff;*/
			uprintf("[*]  new idt entry %d:\n"
				"[**] addr:\t%p\n"
				"[**] ist:\t%d\n"
				"[**] type:\t%d\n"
				"[**] dpl:\t%d\n"
				"[**] p:\t\t%d\n"
				"[*]  end dump\n\n",
				ZD_INT, (void *)(\
				(long)new_idt[ZD_INT].offset_0_15|((long)new_idt[ZD_INT].offset_16_31<<16)|((long)new_idt[ZD_INT].offset_32_63<<32)),
				new_idt[ZD_INT].ist, new_idt[ZD_INT].type, new_idt[ZD_INT].dpl, new_idt[ZD_INT].p);
				
			uprintf("[*]  loading new idt\n"
				"[**] address:\t%p\n",
				new_idt);
			__asm__ __volatile__("lidt new_idtr");
			/*int j;
			for(int i=0; i<64; i++) {
				uprintf("\n\t");
				for(j=0; j<8; j++) {
					uprintf("%02x ", *((unsigned char *)(old_idt)+i*8+j)); }
				uprintf("   ");
				for(j=0; j<8; j++) {
					uprintf("%02x ", *((unsigned char *)(new_idt)+i*8+j)); }}*/
			uprintf("[*]  done\n\n");
			break;
		case MOD_UNLOAD:
			uprintf("[*]  loading old idt\n"
				"[**] address:\t%p\n",
				old_idt);
			__asm__ __volatile__("lidt old_idtr");
			uprintf("[*]  done\n\n");
		
			uprintf("[*]  freeing new idt\n"
				"[**] address:\t%p\n",
				new_idt);
			free(new_idt, M_TEMP);
			uprintf("[*]  done\n\n");
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
