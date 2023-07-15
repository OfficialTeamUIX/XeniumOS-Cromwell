#include "boot.h"
#include "BootEEPROM.h"
#include "BootFlash.h"
#include "BootFATX.h"
#include "xbox.h"
#include "config.h"

#include <shared.h>
#include <filesys.h>
#include "cpu.h"

void HMAC_hdd_calculation(int version,unsigned char *HMAC_result, ... );

//This is the code used to call out to a secondary, dynamically linked binary.
//Note that this binary could be ANYTHING.

extern tsHarddiskInfo tsaHarddiskInfo[2];
extern int _start_xsel, _end_xsel;
extern int xpad_num;



/////Start internals crossover section/////
typedef int		(*fn_SetLed)			(char b);
typedef void	(*fn_USBGetEvents)		(void);
typedef int		(*fn_printk)			(const char *szFormat, ...);
typedef int		(*fn_xpad_button)		(unsigned char selected_Button);
typedef void	(*fn_wait_ms)			(DWORD ticks);
typedef void    (*fn_wait_us)			(DWORD ticks);
typedef int 	(*fn_I2CTransmitWord)	(BYTE bPicAddressI2cFormat, WORD wDataToWrite);
typedef DWORD 	(*fn_StringWidth)		(const char * szc);
typedef void*	(*fn_malloc)			(size_t size);
typedef void	(*fn_free)(void *ptr);
typedef int		(*fn_UnpackJpeg)		(BYTE *pbaJpegFileImage,int nFileLength,JPEG * pJpeg);
typedef void 	(*fn_ClearScreen)		(JPEG * pJpeg, int nStartLine, int nEndLine);
typedef int		(*fn_sprintf)			(char * buf, const char *fmt, ...);
typedef int		(*fn_I2CTransmitByteGetReturn)(BYTE bPicAddressI2cFormat, BYTE bDataToWrite);
typedef void*	(*fn_memcpy)			(void *dest, const void *src, unsigned int size);
typedef void 	(*fn_HMAC_hdd_calculation) (int version,unsigned char *HMAC_result, ... );
typedef int		(*fn_BootIdeReadSector)	(int nDriveIndex, void * pbBuffer, unsigned int block, int byte_offset, int n_bytes);
typedef int 	(*fn_BootIdeWriteSector) (int nDriveIndex, void * pbBuffer, unsigned int block);
typedef int		(*fn_BootIso9660GetFile) (const char *szcPath, BYTE *pbaFile, DWORD dwFileLengthMax, DWORD dwOffset);
typedef void	(*fn_IdeInit) (void);
typedef int     (*fn_UnlockHD) (void);
typedef int	(*fn_LockHD) (void);

// FATX Stuff
typedef FATXPartition* (*fn_OpenFATXPartition)(int nDriveIndex,unsigned int partitionOffset,u_int64_t partitionSize);
typedef void (*fn_LoadFATXCluster)(FATXPartition* partition, int clusterId, unsigned char* clusterData);
typedef u_int32_t (*fn_getNextClusterInChain)(FATXPartition* partition, int clusterId);
typedef void (*fn_CloseFATXPartition)(FATXPartition* partition);
typedef int (*fn_FATXLoadFromDisk)(FATXPartition* partition, FATXFILEINFO *fileinfo);


typedef struct _internals {
	//data
	int					*xpad_num;
	int					*cursorx;
	int					*cursory;
	int					*textcolor;
	DWORD				screenw;
	DWORD				screenh;
	int					framebuffer;
	unsigned char			quickflag;

	//functions
	fn_SetLed 			SetLed;
	fn_USBGetEvents 	USBGetEvents;
	fn_printk 			printk;
	fn_xpad_button		xpad_button;
	fn_wait_ms			wait_ms;
	fn_wait_us			wait_us;
	fn_I2CTransmitWord	I2CTransmitWord;
	fn_StringWidth		StringWidth;
	fn_malloc			malloc;
	fn_free			free;
	fn_UnpackJpeg		UnpackJpeg;
	fn_ClearScreen		ClearScreen;
	fn_sprintf			sprintf;
	fn_I2CTransmitByteGetReturn I2CTransmitByteGetReturn;
	fn_memcpy			memcpy;
	fn_HMAC_hdd_calculation HMAC_hdd_calculation;
	fn_BootIdeReadSector BootIdeReadSector;
	fn_BootIdeWriteSector BootIdeWriteSector;
	fn_BootIso9660GetFile BootIso9660GetFile;
	fn_IdeInit			IdeInit;
	fn_UnlockHD			UnlockHD;
	fn_LockHD			LockHD;
	tsHarddiskInfo			*HDInfo;
	tsHarddiskInfo			*DVDInfo;
	fn_OpenFATXPartition		OpenFATXPartition;
	fn_CloseFATXPartition		CloseFATXPartition;
	fn_LoadFATXCluster		LoadFATXCluster;
	fn_getNextClusterInChain	GetNextClusterInChain;
	fn_FATXLoadFromDisk		FATXLoadFromDisk;
//	fn_BootVideoJpegBlitBlend BlitBlend;
} internals;
/////End internals/////

extern void IdeInit(void);

int SetupCallbacks(internals *i, int quickcheck)
{
	i->quickflag = quickcheck;
	if (!quickcheck)
	{
		i->xpad_num = &xpad_num;
		i->cursorx = (int*)&VIDEO_CURSOR_POSX;
		i->cursory = (int*)&VIDEO_CURSOR_POSY;
		i->textcolor = (int*)&VIDEO_ATTR;
		i->screenw = currentvideomodedetails.m_dwWidthInPixels;
		i->screenh = currentvideomodedetails.m_dwHeightInLines;
		i->framebuffer = FRAMEBUFFER_START;
	}


	i->SetLed = (fn_SetLed)I2cSetFrontpanelLed;
	i->USBGetEvents = (fn_USBGetEvents)USBGetEvents;
	i->printk = (fn_printk)printk;
	i->xpad_button = (fn_xpad_button)risefall_xpad_BUTTON;
	i->wait_ms = (fn_wait_ms)wait_ms;
	i->wait_us = (fn_wait_us)wait_us;
	i->I2CTransmitWord = (fn_I2CTransmitWord)I2CTransmitWord;
	i->StringWidth = (fn_StringWidth)BootVideoGetStringTotalWidth;
	i->malloc = (fn_malloc)malloc;
	i->free = (fn_free)free;
	i->UnpackJpeg = (fn_UnpackJpeg)BootVideoJpegUnpackAsRgb;
	i->ClearScreen = (fn_ClearScreen)BootVideoClearScreen;
	i->sprintf = (fn_sprintf)sprintf;
	i->I2CTransmitByteGetReturn = (fn_I2CTransmitByteGetReturn)I2CTransmitByteGetReturn;
	i->memcpy = (fn_memcpy)memcpy;
	i->HMAC_hdd_calculation = (fn_HMAC_hdd_calculation)HMAC_hdd_calculation;
//	i->BlitBlend = (fn_BootVideoJpegBlitBlend)BootVideoJpegBlitBlend;
	i->BootIdeReadSector = BootIdeReadSector;
	i->BootIdeWriteSector = BootIdeWriteSector;
	i->BootIso9660GetFile = BootIso9660GetFile;
	i->IdeInit = IdeInit;
	i->UnlockHD = (fn_UnlockHD)BootIdeUnlockHD;
	i->LockHD = (fn_LockHD)BootIdeLockHD;
	i->HDInfo = &(tsaHarddiskInfo[0]);
	i->DVDInfo = &(tsaHarddiskInfo[1]);
	i->OpenFATXPartition = OpenFATXPartition;
	i->CloseFATXPartition = CloseFATXPartition;
	i->LoadFATXCluster = LoadFATXCluster;
	i->GetNextClusterInChain = getNextClusterInChain;
	i->FATXLoadFromDisk = FATXLoadFromDisk;
	return 1;
}




typedef void (*ds)(internals);

#define psecondary 0x00100000

ds DoSecondary=(ds)psecondary;


int FlashLoadImage(void)
{
	unsigned char *d = (unsigned char*)psecondary;
	unsigned char *s = (unsigned char*)0xFF000000;
	int i;

	/////REMOVED 1 LINE/////

	for(i=0;i<1024*512;i++)
	{
		*d = *s;
		s++;
		d++;
	}
	DoSecondary=(ds)psecondary;
	return 1;
}

int SelfLoadImage(void)
{
	char *d = (char*)psecondary;
	char *s = (char*)&_start_xsel;

	//read image from self to proper location
	while(s < (char*)&_end_xsel && s < (char*)(&_start_xsel + (1024*2)))
	{
		*d = *s;
		s++;
		d++;
	}
	DoSecondary=(ds)psecondary;
	return 1;
}

int FatXLoadImage(void)
{
	FATXPartition *partition = NULL;
	FATXFILEINFO fileinfo;

	partition = OpenFATXPartition(0,SECTOR_STORE,STORE_SIZE);

	if(partition != NULL) {

		if(LoadFATXFile(partition,"/xsel.bin",&fileinfo)) {
			memcpy((void*)psecondary,fileinfo.buffer,fileinfo.fileSize);
			free(fileinfo.buffer);
		} else {
		 CloseFATXPartition(partition);
		 return 0;
		}
	} else {
		 CloseFATXPartition(partition);
		 return 0;
	}
	return 1;
}


void SetRGB(BYTE b)
{
        /////REMOVED 1 LINE/////
}



void xCallout(int quickcheck)
{
	internals b;
//	printk("Attempting to load secondary binary\n");



//	if(FatXLoadImage())
//	{
//		printk("image loaded from drive\n");
//		if(SetupCallbacks(&b))
//		{
//			DoSecondary(b);
//		}
//		else
//		{
//			//setting up callbacks failed
//		}
//	}
//	else
//	{

		if(FlashLoadImage())
		{
//			printk("image loaded from flash\n");

			//check md5 hash
			//
			if(SetupCallbacks(&b, quickcheck))
			{
				DoSecondary(b);
			}
			else
			{
				//setting up callbacks failed
			}
		}
		//loading image failed
//	}
//	printk("Image load failed!\n");
}
