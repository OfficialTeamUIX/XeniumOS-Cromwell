/*
 * USB support for XBOX, based on Linux kernel source
 *
 * 2003-06-21 Georg Acher (georg@acher.org)
 *
*/
          
#include "../usb_wrapper.h"

void subsys_usb_init(void);
extern struct pci_device_id  *module_table_pci_ids;

// straigth call...
int usb_hcd_pci_probe (struct pci_dev *dev, const struct pci_device_id *id);
void usb_hcd_pci_remove (struct pci_dev *dev);

void XPADInit(void);
void XPADRemove(void);
void XRemoteInit(void);
void XRemoteRemove(void);

extern int (*thread_handler)(void*);
int (*hub_thread_handler)(void*);

extern int nousb;
extern int USB_init_ani;
extern int xpad_num;

struct pci_dev xx_ohci_dev={
        .vendor = 0,
        .device = 0,
        .bus = NULL,
        .irq = 1, // currently not used...
        .slot_name = "OHCI",
        .dev = {.name = "PCI",.dma_mask=1},
        .base = {0xfed00000}, 
        .flags = {}
};

/*------------------------------------------------------------------------*/ 

#undef printk
#define printk(x) BootVideoChunkedPrint(x,strlen(x))

void BootStartUSB(void)
{
	int n;

	nousb=0;

		printk("Init_Wrapper\n");
        init_wrapper();
        printk("Subsys_usb_init\n");
        subsys_usb_init();
        hub_thread_handler=thread_handler;
        printk("usb_hcd_pci_probe\n");
	usb_hcd_pci_probe(&xx_ohci_dev,
			  module_table_pci_ids);
	printk("XPADInit\n");	
	XPADInit();
	printk("XRemoteInit\n");
	XRemoteInit();
	printk("UsbKeyBoardInit\n");	
	UsbKeyBoardInit();
	
	printk("Spin loop\n");
	// Find a few connected devices first...
//	for(n=0;n<200;n++)
//	{
//		USBGetEvents();
//		//wait_ms(1);
//		if (xpad_num!=0) {  // Houston, we have a XPAD!
//			break;
//		}
//	}
	printk("Done!\n");
	USB_init_ani=0; // No more animation
}
/*------------------------------------------------------------------------*/ 
void USBGetEvents(void)
{	
        inc_jiffies(1);
        do_all_timers();
        hub_thread_handler(NULL);
        handle_irqs(-1);       
}
/*------------------------------------------------------------------------*/ 
void BootStopUSB(void)
{
	int n;
        
        XPADRemove();
	XRemoteRemove();
	UsbKeyBoardRemove();
	
	for(n=0;n<100;n++)
	{
		USBGetEvents();
		wait_ms(1);
	}	

	usb_hcd_pci_remove(&xx_ohci_dev);
	
	for(n=0;n<100;n++)
	{
		USBGetEvents();
		wait_ms(1);
	}

	//memset((void*)0x0,0x0,100);
	memset((void*)0xfed00006,0x0,40);
	memset((void*)0xfed08006,0x0,40);
}	
/*------------------------------------------------------------------------*/ 	
