/*
 *  This code was originally from:
 *   UBL, The Universal Talkware Boot Loader
 *    Copyright (C) 2000 Universal Talkware Inc.
 */

#include "boot.h"
#include "shared.h"
#include "BootEEPROM.h"

#undef sprintf

////////////////////////////////////
// IDE types and constants


#define IDE_SECTOR_SIZE 		0x200
#define IDE_BASE1             		(0x1F0u) /* primary controller */

#define IDE_REG_EXTENDED_OFFSET   	(0x200u)

#define IDE_REG_DATA(base)          	((base) + 0u) /* word register */
#define IDE_REG_ERROR(base)         	((base) + 1u)
#define IDE_REG_FEATURE(base)       	((base) + 1u)
#define IDE_REG_SECTOR_COUNT(base)  	((base) + 2u)
#define IDE_REG_SECTOR_NUMBER(base) 	((base) + 3u)
#define IDE_REG_CYLINDER_LSB(base)  	((base) + 4u)
#define IDE_REG_CYLINDER_MSB(base)  	((base) + 5u)
#define IDE_REG_DRIVEHEAD(base)     	((base) + 6u)
#define IDE_REG_STATUS(base)        	((base) + 7u)
#define IDE_REG_COMMAND(base)       	((base) + 7u)
#define IDE_REG_ALTSTATUS(base)     	((base) + IDE_REG_EXTENDED_OFFSET + 6u)
#define IDE_REG_CONTROL(base)       	((base) + IDE_REG_EXTENDED_OFFSET + 6u)
#define IDE_REG_ADDRESS(base)       	((base) + IDE_REG_EXTENDED_OFFSET + 7u)


// for hdd lock/unlock
#define IDE_SECURITY_SUPPORTED			0x0001
#define IDE_SECURITY_ENABLED			0x0002
#define IDE_SECURITY_PASSWORD_SET		0x0004
#define IDE_SECURITY_FROZEN			0x0008
#define IDE_SECURITY_COUNT_EXPIRED		0x0010
#define IDE_SECURITY_LEVEL_MAX			0x0100


typedef struct {
	unsigned char m_bPrecomp;
	unsigned char m_bCountSector;
	unsigned char m_bSector;
	unsigned short m_wCylinder;
	unsigned char m_bDrivehead;
	
	   /* 48-bit LBA */   
	unsigned char m_bCountSectorExt;   
	unsigned char m_bSectorExt;   
	unsigned short m_wCylinderExt; 
	
#       define IDE_DH_DEFAULT (0xA0)
#       define IDE_DH_HEAD(x) ((x) & 0x0F)
#       define IDE_DH_MASTER  (0x00)
#       define IDE_DH_SLAVE   (0x10)
#       define IDE_DH_DRIVE(x) ((((x) & 1) != 0)?IDE_DH_SLAVE:IDE_DH_MASTER)
#       define IDE_DH_LBA     (0x40)
#       define IDE_DH_CHS     (0x00)

} tsIdeCommandParams;

#define IDE_DEFAULT_COMMAND { 0xFFu, 0x01, 0x00, 0x0000, IDE_DH_DEFAULT | IDE_DH_SLAVE }

typedef enum {
	IDE_CMD_NOOP = 0,
	IDE_CMD_RECALIBRATE = 0x10,
	IDE_CMD_READ_MULTI_RETRY = 0x20,
	IDE_CMD_READ_MULTI = IDE_CMD_READ_MULTI_RETRY,
	IDE_CMD_READ_MULTI_NORETRY = 0x21,
	
	IDE_CMD_READ_EXT = 0x24, /* 48-bit LBA */
    	
    	IDE_CMD_WRITE_MULTI_RETRY = 0x30,
    	
	IDE_CMD_DRIVE_DIAG = 0x90,
	IDE_CMD_SET_PARAMS = 0x91,
	IDE_CMD_STANDBY_IMMEDIATE = 0x94, /* 2 byte command- also send
	                                     IDE_CMD_STANDBY_IMMEDIATE2 */
	IDE_CMD_SET_MULTIMODE = 0xC6,
	IDE_CMD_STANDBY_IMMEDIATE2 = 0xE0,
	IDE_CMD_GET_INFO = 0xEC,

	IDE_CMD_SET_FEATURES = 0xef,

	IDE_CMD_ATAPI_PACKET = 0xa0,

	IDE_CMD_SECURITY_SETPASSWORD = 0xf1,
	IDE_CMD_SECURITY_UNLOCK = 0xf2,
	IDE_CMD_SECURITY_FREEZE	= 0xf5,
	IDE_CMD_SECURITY_DISABLE = 0xf6 
	
} ide_command_t;


#define printk_debug bprintf


//////////////////////////////////
//  Statics

tsHarddiskInfo tsaHarddiskInfo[2];  // static struct stores data about attached drives

const char * const szaSenseKeys[] = {
	"No Sense", "Recovered Error", "Not Ready", "Medium Error",
	"Hardware Error", "Illegal request", "Unit Attention", "Data Protect",
	"Reserved 8", "Reserved 9", "Reserved 0xa", "Aborted Command",
	"Miscompare", "Reserved 0xf"
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//  Helper routines
//


/* -------------------------------------------------------------------------------- */

int BootIdeWaitNotBusy(unsigned uIoBase)
{
	BYTE b = 0x80;
//	int n=0;

//	I2cSetFrontpanelLed(0x66);
	while (b & 0x80) {
		b=IoInputByte(IDE_REG_ALTSTATUS(uIoBase));
//		printk("%02x %d\n", b, n++);
//		WATCHDOG;
	}

	return b&1;
}

/* -------------------------------------------------------------------------------- */

int BootIdeWaitDataReady(unsigned uIoBase)
{
	int i = 0x800000;
	wait_smalldelay();
	do {
		if ( ((IoInputByte(IDE_REG_ALTSTATUS(uIoBase)) & 0x88) == 0x08)	)	{
	    if(IoInputByte(IDE_REG_ALTSTATUS(uIoBase)) & 0x01) return 2;
			return 0;
		}
//		WATCHDOG();
		i--;
	} while (i != 0);

	if(IoInputByte(IDE_REG_ALTSTATUS(uIoBase)) & 0x01) return 2;
	return 1;
}

/* -------------------------------------------------------------------------------- */

int BootIdeIssueAtaCommand(
	unsigned uIoBase,
	ide_command_t command,
	tsIdeCommandParams * params)
{
	int n;

	IoInputByte(IDE_REG_STATUS(uIoBase));

	n=BootIdeWaitNotBusy(uIoBase);
//	if(n)	{// as our command may be being used to clear the error, not a good policy to check too closely!
//		printk("error on BootIdeIssueAtaCommand wait 1: ret=%d, error %02X\n", n, IoInputByte(IDE_REG_ERROR(uIoBase)));
//		return 1;
//	}  
 	
 	/* 48-bit LBA */   
        /* this won't hurt for non 48-bit LBA commands since we re-write these registers below */   
	
	IoOutputByte(IDE_REG_SECTOR_COUNT(uIoBase), params->m_bCountSectorExt);   
	IoOutputByte(IDE_REG_SECTOR_NUMBER(uIoBase), params->m_bSectorExt);   
	IoOutputByte(IDE_REG_CYLINDER_LSB(uIoBase), params->m_wCylinderExt & 0xFF);   
	IoOutputByte(IDE_REG_CYLINDER_MSB(uIoBase), (params->m_wCylinderExt >> 8) ); 
	/* End 48-bit LBA */   

	IoOutputByte(IDE_REG_SECTOR_COUNT(uIoBase), params->m_bCountSector);
	IoOutputByte(IDE_REG_SECTOR_NUMBER(uIoBase), params->m_bSector);
	IoOutputByte(IDE_REG_CYLINDER_LSB(uIoBase), params->m_wCylinder & 0xFF);
	IoOutputByte(IDE_REG_CYLINDER_MSB(uIoBase), (params->m_wCylinder >> 8) /* & 0x03 */);
	IoOutputByte(IDE_REG_DRIVEHEAD(uIoBase), params->m_bDrivehead);

	IoOutputByte(IDE_REG_COMMAND(uIoBase), command);
	wait_smalldelay();

	n=BootIdeWaitNotBusy(uIoBase);
	if(n)	{
//		printk("error on BootIdeIssueAtaCommand wait 3: ret=%d, error %02X\n", n, IoInputByte(IDE_REG_ERROR(uIoBase)));
		return 1;
	}

	return 0;
}

/* -------------------------------------------------------------------------------- */

int BootIdeReadData(unsigned uIoBase, void * buf, size_t size)
{
	WORD * ptr = (WORD *) buf;

	if (BootIdeWaitDataReady(uIoBase)) {
		printk("BootIdeReadData data not ready...\n");
		return 1;
	}

	while (size > 1) {
		*ptr++ = IoInputWord(IDE_REG_DATA(uIoBase));
		size -= 2;
	}

	IoInputByte(IDE_REG_STATUS(uIoBase));

	if(IoInputByte(IDE_REG_ALTSTATUS(uIoBase)) & 0x01) {
		printk("BootIdeReadData ended with an error\n");
		return 2;
	}

	return 0;
}

/* -------------------------------------------------------------------------------- */

// issues a block of data ATA-style

int BootIdeWriteData(unsigned uIoBase, void * buf, size_t size)
{
	register unsigned short * ptr = (unsigned short *) buf;
	int n;

	n=BootIdeWaitDataReady(uIoBase);
//	if(n) {
//		printk("BootIdeWriteData timeout or error from BootIdeWaitDataReady ret %d\n", n);
//		return 1;
//	}
	//wait_smalldelay();

	while (size > 1) {

		IoOutputWord(IDE_REG_DATA(uIoBase), *ptr);
		size -= 2;
		ptr++;
	}
	wait_smalldelay();
	
	n=BootIdeWaitNotBusy(uIoBase);
	//wait_smalldelay();
	if(n) {
		printk("BootIdeWriteData timeout or error from BootIdeWaitNotBusy ret %d\n", n);
		return 1;
	}

   	if(IoInputByte(IDE_REG_ALTSTATUS(uIoBase)) & 0x01) return 2;
	
	return 0;
}





/* -------------------------------------------------------------------------------- */

int BootIdeWriteAtapiData(unsigned uIoBase, void * buf, size_t size)
{
	WORD * ptr = (unsigned short *) buf;
	WORD w;
	int n;

	n=BootIdeWaitDataReady(uIoBase);
//	if(n) {
//		printk("BootIdeWriteAtapiData error before data ready ret %d\n", n);
//		return 1;
//	}
	wait_smalldelay();

	w=IoInputByte(IDE_REG_CYLINDER_LSB(uIoBase));
	w|=(IoInputByte(IDE_REG_CYLINDER_MSB(uIoBase)))<<8;

//	bprintf("(bytes count =%04X)\n", w);
	n=IoInputByte(IDE_REG_STATUS(uIoBase));
	if(n&1) { // error
		printk("BootIdeWriteAtapiData Error BEFORE writing data (0x%x bytes) err=0x%X\n", n, w);
		return 1;
	}

	while (size > 1) {

		IoOutputWord(IDE_REG_DATA(uIoBase), *ptr);
		size -= 2;
		ptr++;
	}
	n=IoInputByte(IDE_REG_STATUS(uIoBase));
	if(n&1) { // error
//		printk("BootIdeWriteAtapiData Error after writing data err=0x%X\n", n);
		return 1;
	}
	wait_smalldelay();
	n=BootIdeWaitNotBusy(uIoBase);
	if(n) {
//		printk("BootIdeWriteAtapiData timeout or error before not busy ret %d\n", n);
		return 1;
	}
	wait_smalldelay();

   	if(IoInputByte(IDE_REG_ALTSTATUS(uIoBase)) & 0x01) return 2;
	
	return 0;
}


/* -------------------------------------------------------------------------------- */

int BootIdeIssueAtapiPacketCommandAndPacket(int nDriveIndex, BYTE *pAtapiCommandPacket12Bytes)
{
	tsIdeCommandParams tsicp = IDE_DEFAULT_COMMAND;
	unsigned 	uIoBase = tsaHarddiskInfo[nDriveIndex].m_fwPortBase;

	tsicp.m_bDrivehead = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS | IDE_DH_DRIVE(nDriveIndex);
	IoOutputByte(IDE_REG_DRIVEHEAD(uIoBase), tsicp.m_bDrivehead);

	tsicp.m_wCylinder=2048;
	BootIdeWaitNotBusy(uIoBase);
	if(BootIdeIssueAtaCommand(uIoBase, IDE_CMD_ATAPI_PACKET, &tsicp)) 
	{
//			printk("  Drive %d: BootIdeIssueAtapiPacketCommandAndPacket 1 FAILED, error=%02X\n", nDriveIndex, IoInputByte(IDE_REG_ERROR(uIoBase)));
			return 1;
	}
	
	if(BootIdeWaitNotBusy(uIoBase)) 
	{
		printk("  Drive %d: BootIdeIssueAtapiPacketCommandAndPacket 2 FAILED, error=%02X\n", nDriveIndex, IoInputByte(IDE_REG_ERROR(uIoBase)));
		return 1;
	}


//	printk("  Drive %d:   status=0x%02X, error=0x%02X\n",
//	nDriveIndex, IoInputByte(IDE_REG_ALTSTATUS(uIoBase)), IoInputByte(IDE_REG_ERROR(uIoBase)));

	if(BootIdeWriteAtapiData(uIoBase, pAtapiCommandPacket12Bytes, 12)) 
	{
//		printk("  Drive %d:BootIdeIssueAtapiPacketCommandAndPacket 3 FAILED, error=0x%02X\n", nDriveIndex, IoInputByte(IDE_REG_ERROR(uIoBase)));
//		BootIdeAtapiPrintkFriendlyError(nDriveIndex);
		return 1;
	}

	if(pAtapiCommandPacket12Bytes[0]!=0x1e) 
	{
		if(BootIdeWaitDataReady(uIoBase)) 
		{
			printk("  Drive %d:  BootIdeIssueAtapiPacketCommandAndPacket Atapi Wait for data ready FAILED, status=0x%02X, error=0x%02X\n",
				nDriveIndex, IoInputByte(IDE_REG_ALTSTATUS(uIoBase)), IoInputByte(IDE_REG_ERROR(uIoBase)));
			return 1;
		}
	}
	return 0;
}


/* -------------------------------------------------------------------------------- */


/////////////////////////////////////////////////
//  BootIdeDriveInit
//
//  Called by BootIdeInit for each drive
//  detects and inits each drive, and the structs containing info about them

#define DRIVETYPE_HDD	0
#define DRIVETYPE_ATAPI	1

int BootIdeDriveInit(unsigned uIoBase, int nIndexDrive,int drivetype)
{
	tsIdeCommandParams tsicp = IDE_DEFAULT_COMMAND;
	unsigned short* drive_info;
	BYTE baBuffer[512];
     
	tsaHarddiskInfo[nIndexDrive].m_fwPortBase = uIoBase;
	tsaHarddiskInfo[nIndexDrive].m_wCountHeads = 0u;
	tsaHarddiskInfo[nIndexDrive].m_wCountCylinders = 0u;
	tsaHarddiskInfo[nIndexDrive].m_wCountSectorsPerTrack = 0u;
	tsaHarddiskInfo[nIndexDrive].m_dwCountSectorsTotal = 1ul;
	tsaHarddiskInfo[nIndexDrive].m_bLbaMode = IDE_DH_CHS;
	tsaHarddiskInfo[nIndexDrive].m_fDriveExists = 0;
	tsaHarddiskInfo[nIndexDrive].m_enumDriveType=EDT_UNKNOWN;
	tsaHarddiskInfo[nIndexDrive].m_fAtapi=false;
	tsaHarddiskInfo[nIndexDrive].m_wAtaRevisionSupported=0;
	tsaHarddiskInfo[nIndexDrive].m_fHasMbr=0;

	tsicp.m_bDrivehead = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS | IDE_DH_DRIVE(nIndexDrive);
	IoOutputByte(IDE_REG_DRIVEHEAD(uIoBase), tsicp.m_bDrivehead);

	IoOutputByte(IDE_REG_CONTROL(uIoBase), 0x0a); // kill interrupt,
	IoOutputByte(IDE_REG_FEATURE(uIoBase), 0x00); // kill DMA


	if(BootIdeWaitNotBusy(uIoBase)) 
	
	{
			printk_debug("  Drive %d: Not Ready\n", nIndexDrive);
			return 1;
	}


//	printk_debug("  Drive %d: happy\n", nIndexDrive);

        // Basically, if (nIndexdrive==0) .....(this is for the MAster drive)
        
        //Lets try sending ATAPI first:
        if(!BootIdeIssueAtaCommand(uIoBase, 0xa1, &tsicp)) 
        {
        	tsaHarddiskInfo[nIndexDrive].m_fAtapi=true;
        	drivetype = DRIVETYPE_ATAPI;
        }
        else
        {
        	drivetype = DRIVETYPE_HDD;
        }

        
	
	if (drivetype == DRIVETYPE_HDD)
	{ 
		// master... you have to send it IDE_CMD_GET_INFO
		int nReturn=0;
		tsaHarddiskInfo[nIndexDrive].m_fAtapi=false;
		if(BootIdeIssueAtaCommand(uIoBase, IDE_CMD_GET_INFO, &tsicp)) {
			nReturn=IoInputByte(IDE_REG_CYLINDER_MSB(uIoBase))<<8;
			nReturn |=IoInputByte(IDE_REG_CYLINDER_LSB(uIoBase));
			printk("nReturn = %x\n", nReturn);

			if(nReturn!=0xeb14) {
				printk("  Drive %d: detect FAILED, error=%02X\n", nIndexDrive, IoInputByte(IDE_REG_ERROR(uIoBase)));
				return 1;
			}
		}
	
	} 	
		
	BootIdeWaitDataReady(uIoBase);
	
	if(BootIdeReadData(uIoBase, baBuffer, IDE_SECTOR_SIZE)) 
	{
		printk("  %d: Drive not detected\n", nIndexDrive);
		return 1;
	}  
	

	drive_info = (unsigned short*)baBuffer;
	tsaHarddiskInfo[nIndexDrive].m_wCountHeads = drive_info[3];
	tsaHarddiskInfo[nIndexDrive].m_wCountCylinders = drive_info[1];
	tsaHarddiskInfo[nIndexDrive].m_wCountSectorsPerTrack = drive_info[6];
	tsaHarddiskInfo[nIndexDrive].m_dwCountSectorsTotal = *((unsigned int*)&(drive_info[60]));
	tsaHarddiskInfo[nIndexDrive].m_wAtaRevisionSupported = drive_info[88];
	tsaHarddiskInfo[nIndexDrive].m_wSecurityFlags = drive_info[128];


        /* 48-bit LBA */   
	if( drive_info[83] & 1ul<<10 ) 
	{   
	        unsigned long long maxLBA;   
	        /*   
	           Do a sanity check here since the T13 spec is not clear on byte/word ordering of this value   
	           The lower 28 bits of the Max 48 bit LBA should match the previously returned 28 bit max LBA.   
	           If it does match, then we are reading the 48-bit Max LBA correctly and we can go ahead and use it...   
	           Once this code is actually tested this check can be removed.   
	        */   
	        printk("ATA 48-bit LBA Supported\n");   
	        maxLBA = *((unsigned long long*)&(drive_info[100]));   
	        if( tsaHarddiskInfo[nIndexDrive].m_dwCountSectorsTotal == (unsigned int)(maxLBA & 0xFFFFFFF) ) {   
	                tsaHarddiskInfo[nIndexDrive].m_dwCountSectorsTotal = (unsigned int)maxLBA;   
	        } else {   
	                printk("28 LSBs of Max 48-bit LBA's don't match.  Using Max 28-bit LBA\n");   
	        }   
	} else {   
	        printk("ATA 28-bit LBA Support Only\n");   
	} 
	
	/* End 48-bit LBA */   
	
	{ 
		WORD * pw=(WORD *)&(drive_info[10]);
		tsaHarddiskInfo[nIndexDrive].s_length =
			copy_swap_trim(tsaHarddiskInfo[nIndexDrive].m_szSerial,(BYTE*)pw,20);
		pw=(WORD *)&(drive_info[27]);
		tsaHarddiskInfo[nIndexDrive].m_length =
			copy_swap_trim(tsaHarddiskInfo[nIndexDrive].m_szIdentityModelNumber,(BYTE *)pw,40);
		copy_swap_trim(tsaHarddiskInfo[nIndexDrive].m_szFirmware,(BYTE *)&(drive_info[23]),8);

	}

	tsaHarddiskInfo[nIndexDrive].m_fDriveExists = 1;

	if (drivetype == DRIVETYPE_ATAPI)
	{ // CDROM/DVD
                // We Detected a CD-DVD or so, as there are no Heads ...
		tsaHarddiskInfo[nIndexDrive].m_fAtapi=true;

		VIDEO_ATTR=0xffc8c8c8;
		printk("hd%c: ", nIndexDrive+'a');
		VIDEO_ATTR=0xffc8c800;

		printk("%s %s %s\n",tsaHarddiskInfo[nIndexDrive].m_szIdentityModelNumber,
			tsaHarddiskInfo[nIndexDrive].m_szIdentityModelNumber,
			tsaHarddiskInfo[nIndexDrive].m_szSerial,
			tsaHarddiskInfo[nIndexDrive].m_szFirmware);


		if (cromwell_config==CROMWELL) 
		{
		// IS working as Cromwell
		
		  // this is the only way to clear the ATAPI ''I have been reset'' error indication
			BYTE ba[128];
			ba[2]=0x06;

			while (ba[2]==0x06) 
			{  
				// while bitching that it 'needs attention', give it REQUEST SENSE
				int nPacketLength=BootIdeAtapiAdditionalSenseCode(nIndexDrive, &ba[0], sizeof(ba));
				if(nPacketLength<12) 
				{
					printk("Unable to get ASC from drive when clearing sticky DVD error, retcode=0x%x\n", nPacketLength);
	//				return 1;
	//				while(1);
					ba[2]=0;
				} else {
//					printk("ATAPI Drive reports ASC 0x%02X\n", ba[12]);  // normally 0x29 'reset' but clears the condition by reading
				}
		
          		}
/*

	// Xbox drives can't handle ALLOW REMOVAL??
			memset(&ba[0], 0, 12);
			ba[0]=0x1e;
			if(BootIdeIssueAtapiPacketCommandAndPacket(nIndexDrive, &ba[0])) {
				printk("Unable to issue allow media removal command\n");
				while(1) ;
			} else {
				BootIdeAtapiPrintkFriendlyError(nIndexDrive);
			}
*/
		}


	} 
        
        
        if (drivetype == DRIVETYPE_HDD)
        {

		unsigned long ulDriveCapacity1024=((tsaHarddiskInfo[nIndexDrive].m_dwCountSectorsTotal /1000)*512)/1000;
/*
		int nAta=0;
		if(tsaHarddiskInfo[nIndexDrive].m_wAtaRevisionSupported&2) nAta=1;
		if(tsaHarddiskInfo[nIndexDrive].m_wAtaRevisionSupported&4) nAta=2;
		if(tsaHarddiskInfo[nIndexDrive].m_wAtaRevisionSupported&8) nAta=3;
		if(tsaHarddiskInfo[nIndexDrive].m_wAtaRevisionSupported&16) nAta=4;
		if(tsaHarddiskInfo[nIndexDrive].m_wAtaRevisionSupported&32) nAta=5;
*/
		VIDEO_ATTR=0xffc8c8c8;
		printk("hd%c: ", nIndexDrive+'a');
		VIDEO_ATTR=0xffc8c800;

		printk("%s %s %u.%uGB ",
			tsaHarddiskInfo[nIndexDrive].m_szIdentityModelNumber,
			tsaHarddiskInfo[nIndexDrive].m_szFirmware,
//			tsaHarddiskInfo[nIndexDrive].m_szSerial,
//			nAta,
// 			tsaHarddiskInfo[nIndexDrive].m_wCountHeads,
//  		tsaHarddiskInfo[nIndexDrive].m_wCountCylinders,
//   		tsaHarddiskInfo[nIndexDrive].m_wCountSectorsPerTrack,
			ulDriveCapacity1024/1000, ulDriveCapacity1024%1000 //,
//			drive_info[128]
		);




		if (cromwell_config==CROMWELL) {
			// Cromwell Mode
			if((drive_info[128]&0x0004)==0x0004) 
			{ 
				// 'security' is in force, unlock the drive (was 0x104/0x104)
				BYTE baMagic[0x200], baKeyFromEEPROM[0x10], baEeprom[0x30];
				bool fUnlocked=false;
				int n,nVersionHashing;
				tsIdeCommandParams tsicp1 = IDE_DEFAULT_COMMAND;
				DWORD dwMagicFromEEPROM;
				DWORD BootHddKeyGenerateEepromKeyData(BYTE *eeprom_data,BYTE *HDKey);
				
				int nVersionSuccessfulDecrypt=0;

				printk(" Lck[%x]", drive_info[128]);

				// THE HDD_SANITY Code removed from the CODE

				dwMagicFromEEPROM=0;

	 			nVersionHashing = 0;
	
				memcpy(&baEeprom[0], &eeprom, 0x30); // first 0x30 bytes from EEPROM image we picked up earlier
	
				for(n=0;n<0x10;n++) baKeyFromEEPROM[n]=0;
				nVersionHashing = BootHddKeyGenerateEepromKeyData( &baEeprom[0], &baKeyFromEEPROM[0]);
	
				for(n=0;n<0x200;n++) baMagic[n]=0;
	
		#ifdef HDD_DEBUG
				printk("Model='%s', Serial='%s'\n", tsaHarddiskInfo[nIndexDrive].m_szIdentityModelNumber, tsaHarddiskInfo[nIndexDrive].m_szSerial);
				VideoDumpAddressAndData(0, &baKeyFromEEPROM[0], 0x10);
		#endif
	
				// Claculate the hdd pw from EErpom and Serial / Model Number
				HMAC_SHA1 (&baMagic[2], baKeyFromEEPROM, 0x10,
						 tsaHarddiskInfo[nIndexDrive].m_szIdentityModelNumber,
						 tsaHarddiskInfo[nIndexDrive].m_length,
						 tsaHarddiskInfo[nIndexDrive].m_szSerial,
						 tsaHarddiskInfo[nIndexDrive].s_length);
	                       
				if (nVersionHashing == 0)
				{
					printk("Got a 0 return from BootHddKeyGenerateEepromKeyData\n");
					// ERRORR -- Corrupt EEprom or Newer Version of EEprom - key
				}
	
				// clear down the unlock packet, except for b8 set in first word (high security unlock)
	
				{

					WORD * pword=(WORD *)&baMagic[0];
					*pword=0;  // try user
				}
	
				if(BootIdeWaitNotBusy(uIoBase)) 
				{
					printk("  %d:  Not Ready\n", nIndexDrive);
					return 1;
				}
				tsicp1.m_bDrivehead = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS | IDE_DH_DRIVE(nIndexDrive);
	
				if(BootIdeIssueAtaCommand(uIoBase, IDE_CMD_SECURITY_UNLOCK, &tsicp1)) 
				{
					printk("  %d:  when issuing unlock command FAILED, error=%02X\n", nIndexDrive, IoInputByte(IDE_REG_ERROR(uIoBase)));
					return 1;
				}
				BootIdeWaitDataReady(uIoBase);
				BootIdeWriteData(uIoBase, &baMagic[0], IDE_SECTOR_SIZE);
        	
				if (BootIdeWaitNotBusy(uIoBase))	
				{
					printk("..");
					{
						WORD * pword=(WORD *)&baMagic[0];
						*pword=1;  // try master
					}

					if(BootIdeIssueAtaCommand(uIoBase, IDE_CMD_SECURITY_UNLOCK, &tsicp1)) 
					{
						printk("  %d:  when issuing unlock command FAILED, error=%02X\n", nIndexDrive, IoInputByte(IDE_REG_ERROR(uIoBase)));
						return 1;
					}
					BootIdeWaitDataReady(uIoBase);
					BootIdeWriteData(uIoBase, &baMagic[0], IDE_SECTOR_SIZE);

					if (BootIdeWaitNotBusy(uIoBase))	
					{
//						printk("  - Both Master and User unlock attempts failed\n");
					}
				}

				// check that we are unlocked

				tsicp.m_bDrivehead = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS | IDE_DH_DRIVE(nIndexDrive);
				if(BootIdeIssueAtaCommand(uIoBase, IDE_CMD_GET_INFO, &tsicp)) 
				{
					printk("  %d:  on issuing get status after unlock detect FAILED, error=%02X\n", nIndexDrive, IoInputByte(IDE_REG_ERROR(uIoBase)));
					return 1;
				}
				BootIdeWaitDataReady(uIoBase);
				if(BootIdeReadData(uIoBase, baBuffer, IDE_SECTOR_SIZE)) 
				{
					printk("  %d:  BootIdeReadData FAILED, error=%02X\n", nIndexDrive, IoInputByte(IDE_REG_ERROR(uIoBase)));
					return 1;
				}

		//		printk("post-unlock sec status: 0x%x\n", drive_info[128]);
				if(drive_info[128]&0x0004) 
				{
					printk("  %d:  FAILED to unlock drive, security: %04x\n", nIndexDrive, drive_info[128]);
				} else {
	//				printk("  %d:  Drive unlocked, new sec=%04X\n", nIndexDrive, drive_info[128]);
					fUnlocked=true;
		//			printk("    Unlocked");
				}

//			nVersionHashing++;

			

				if(!fUnlocked)
				{
					printk("\n");
					printk("FAILED to unlock drive, sec: %04x; Version=%d, EEPROM:\n", drive_info[128], nVersionSuccessfulDecrypt);
					VideoDumpAddressAndData(0, &baEeprom[0], 0x30);
					printk("Computed key:\n");
					VideoDumpAddressAndData(0, &baMagic[0], 20);
					return 1;
				}
			}

		} else {
			if(nIndexDrive==0) printk("  Unlocked");
		}
	}  

// End the C/X romwell Selection from above


	if (drive_info[49] & 0x200) 
	{ 
		/* bit 9 of capability word is lba supported bit */
		tsaHarddiskInfo[nIndexDrive].m_bLbaMode = IDE_DH_LBA;
	} else {
		tsaHarddiskInfo[nIndexDrive].m_bLbaMode = IDE_DH_CHS;
	}

//			tsaHarddiskInfo[nIndexDrive].m_bLbaMode = IDE_DH_CHS;

/*
	if(tsaHarddiskInfo[nIndexDrive].m_bLbaMode == IDE_DH_CHS) {
			printk("      CHS Mode\n");
	} else {
			printk("      LBA Mode\n");
	}
*/
	if (drivetype == DRIVETYPE_HDD) 
	{ 

		unsigned char ba[512];
		int nError;

//		printk("Looking at capabilities\n");

			// report on the FATX-ness of the drive contents

		if((nError=BootIdeReadSector(nIndexDrive, &ba[0], 3, 0, 512))) 
		{
			printk("  -  Unable to read FATX sector");
		} else {
			if( (ba[0]=='B') && (ba[1]=='R') && (ba[2]=='F') && (ba[3]=='R') ) 
			{
				tsaHarddiskInfo[nIndexDrive].m_enumDriveType=EDT_UNKNOWN;
				printk(" - FATX", nIndexDrive);
			} else {
				printk(" - No FATX", nIndexDrive);
			}
		}

			// report on the MBR-ness of the drive contents

		if((nError=BootIdeReadSector(nIndexDrive, &ba[0], 0,  0, 512))) 
		{
			printk("     Unable to get first sector, returned %d\n", nError);
		} else {
			if( (ba[0x1fe]==0x55) && (ba[0x1ff]==0xaa) ) 
			{
				printk(" - MBR", nIndexDrive);
				if (drivetype == DRIVETYPE_HDD) 
				{
					tsaHarddiskInfo[nIndexDrive].m_fHasMbr=1;
				}
			
			} else {
				tsaHarddiskInfo[nIndexDrive].m_fHasMbr=0;
				printk(" - No MBR", nIndexDrive);
			}
		}

//		printk("BootIdeDriveInit() HDD completed ok\n");

		printk("\n");

	} 

	if (drivetype == DRIVETYPE_ATAPI) 
	{ 
		// cd/dvd
//		printk("BootIdeDriveInit() DVD completed ok\n");
	}

	return 0;
}




/* -------------------------------------------------------------------------------- */

/////////////////////////////////////////////////
//  BootIdeInit
//
//  Called at boot-time to init and detect connected devices

int BootIdeInit(void)
{
	memset(&tsaHarddiskInfo[0],0x00,sizeof(struct tsHarddiskInfo));
	memset(&tsaHarddiskInfo[1],0x00,sizeof(struct tsHarddiskInfo));
	
	tsaHarddiskInfo[0].m_bCableConductors=40;
	tsaHarddiskInfo[1].m_bCableConductors=40;

	IoOutputByte(0xff60+0, 0x00); // stop bus mastering
	IoOutputByte(0xff60+2, 0x62); // DMA possible for both drives

	BootIdeDriveInit(IDE_BASE1, 0, DRIVETYPE_HDD);		// Master is a HDD
        BootIdeDriveInit(IDE_BASE1, 1, DRIVETYPE_ATAPI);        // Slave is a CD-Drive
        
	if(tsaHarddiskInfo[0].m_fDriveExists) 
	{
		unsigned int uIoBase = tsaHarddiskInfo[0].m_fwPortBase;
		tsIdeCommandParams tsicp = IDE_DEFAULT_COMMAND;

		tsicp.m_bDrivehead = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS | IDE_DH_DRIVE(0);
		IoOutputByte(IDE_REG_DRIVEHEAD(uIoBase), tsicp.m_bDrivehead);

		if(!BootIdeIssueAtaCommand(uIoBase, IDE_CMD_GET_INFO, &tsicp)) 
		{
			WORD waBuffer[256];
			BootIdeWaitDataReady(uIoBase);
			if(!BootIdeReadData(uIoBase, (BYTE *)&waBuffer[0], IDE_SECTOR_SIZE)) 
			{
//				printk("%04X ", waBuffer[80]);
				if( ((waBuffer[93]&0xc000)!=0) && ((waBuffer[93]&0x8000)==0) && ((waBuffer[93]&0xe000)!=0x6000)) 	
				{
					tsaHarddiskInfo[0].m_bCableConductors=80;
				}

			} else {
				printk("Error getting final GET_INFO\n");
			}
		}
	}

	if(tsaHarddiskInfo[0].m_bCableConductors==40) 
	{
		printk("UDMA2\n");
	} else 
	{
		int nAta=0;
		if(tsaHarddiskInfo[0].m_wAtaRevisionSupported&2) nAta=1;
		if(tsaHarddiskInfo[0].m_wAtaRevisionSupported&4) nAta=2;
		if(tsaHarddiskInfo[0].m_wAtaRevisionSupported&8) nAta=3;
		if(tsaHarddiskInfo[0].m_wAtaRevisionSupported&16) nAta=4;
		if(tsaHarddiskInfo[0].m_wAtaRevisionSupported&32) nAta=5;
		printk("UDMA%d\n", nAta);
	}
	
	return 0;
}

/* -------------------------------------------------------------------------------- */

/////////////////////////////////////////////////
//  BootIdeAtapiModeSense
//
//  returns the ATAPI extra error info block

int BootIdeAtapiModeSense(int nDriveIndex, BYTE bCodePage, BYTE * pba, int nLengthMaxReturn) 
{
	unsigned uIoBase = tsaHarddiskInfo[nDriveIndex].m_fwPortBase;

	BYTE ba[2048];
	int nReturn;

	if(!tsaHarddiskInfo[nDriveIndex].m_fDriveExists) return 4;

	memset(ba, 0, sizeof(ba));
	//memset(&ba[0], 0, 12);
	ba[0]=0x5a;
	ba[2]=bCodePage;
	ba[7]=(BYTE)(sizeof(ba)>>8); 
	ba[8]=(BYTE)sizeof(ba);

	if(BootIdeIssueAtapiPacketCommandAndPacket(nDriveIndex, &ba[0])) 
	{
//			BYTE bStatus=IoInputByte(IDE_REG_ALTSTATUS(uIoBase)), bError=IoInputByte(IDE_REG_ERROR(uIoBase));
//			printk("  Drive %d: BootIdeAtapiAdditionalSenseCode FAILED, status=%02X, error=0x%02X, ASC unavailable\n", nDriveIndex, bStatus, bError);
			return 1;
	}

	nReturn=IoInputByte(IDE_REG_CYLINDER_LSB(uIoBase));
	nReturn |=IoInputByte(IDE_REG_CYLINDER_MSB(uIoBase))<<8;
	if(nReturn>nLengthMaxReturn) nReturn=nLengthMaxReturn;
	BootIdeReadData(uIoBase, pba, nReturn);

	return nReturn;
}

/* -------------------------------------------------------------------------------- */

/////////////////////////////////////////////////
//  BootIdeAtapiAdditionalSenseCode
//
//  returns the ATAPI extra error info block

int BootIdeAtapiAdditionalSenseCode(int nDriveIndex, BYTE * pba, int nLengthMaxReturn) 
{
	unsigned uIoBase = tsaHarddiskInfo[nDriveIndex].m_fwPortBase;

	BYTE ba[2048];
	int nReturn;

	if(!tsaHarddiskInfo[nDriveIndex].m_fDriveExists) return 4;

	//memset(&ba[0], 0, 12);
	memset(ba, 0, sizeof(ba));
	ba[0]=0x03;
	ba[4]=0xfe;

	if(BootIdeIssueAtapiPacketCommandAndPacket(nDriveIndex, &ba[0])) 
	{
//			BYTE bStatus=IoInputByte(IDE_REG_ALTSTATUS(uIoBase)), bError=IoInputByte(IDE_REG_ERROR(uIoBase));
//			printk("  Drive %d: BootIdeAtapiAdditionalSenseCode 3 Atapi Wait for data ready FAILED, status=%02X, error=0x%02X, ASC unavailable\n", nDriveIndex, bStatus, bError);
			return 1;
	}

	nReturn=IoInputByte(IDE_REG_CYLINDER_LSB(uIoBase));
	nReturn |=IoInputByte(IDE_REG_CYLINDER_MSB(uIoBase))<<8;
	if(nReturn>nLengthMaxReturn) nReturn=nLengthMaxReturn;
	BootIdeReadData(uIoBase, pba, nReturn);

	return nReturn;
}
/* -------------------------------------------------------------------------------- */

bool BootIdeAtapiReportFriendlyError(int nDriveIndex, char * szErrorReturn, int nMaxLengthError)
{
	BYTE ba[2048];
	char szError[512];
	int nReturn;
	bool f=true;

	memset(ba, 0, sizeof(ba));
	nReturn=BootIdeAtapiAdditionalSenseCode(nDriveIndex, &ba[0], sizeof(ba));
	if(nReturn<12) {
		sprintf(szError, "Unable to get Sense Code\n");
		f=false;
	} else {
		sprintf(szError, "Sense key 0x%02X (%s), ASC=0x%02X, qualifier=0x%02X\n", ba[2]&0x0f, szaSenseKeys[ba[2]&0x0f], ba[12], ba[13]);
		VideoDumpAddressAndData(0, &ba[0], nReturn);
	}

	_strncpy(szErrorReturn, szError, nMaxLengthError);
	return f;
}

/* -------------------------------------------------------------------------------- */

void BootIdeAtapiPrintkFriendlyError(int nDriveIndex)
{
	char sz[512];
	memset(&sz,0x00,sizeof(sz));
	BootIdeAtapiReportFriendlyError(nDriveIndex, sz, sizeof(sz));
	printk(sz);
}


/* -------------------------------------------------------------------------------- */

/////////////////////////////////////////////////
//  BootIdeReadSector
//
//  Read an absolute sector from the device
//  knows if it should use ATA or ATAPI according to HDD or DVD
//  This is the main function for reading things from a drive

int BootIdeReadSector(int nDriveIndex, void * pbBuffer, unsigned int block, int byte_offset, int n_bytes) 
{
	tsIdeCommandParams tsicp = IDE_DEFAULT_COMMAND;
	unsigned uIoBase;
	unsigned char baBufferSector[IDE_SECTOR_SIZE];
	unsigned int track;
	int status;
	unsigned char ideReadCommand = IDE_CMD_READ_MULTI_RETRY; /* 48-bit LBA */
	
	if(!tsaHarddiskInfo[nDriveIndex].m_fDriveExists) return 4;

	uIoBase = tsaHarddiskInfo[nDriveIndex].m_fwPortBase;

	tsicp.m_bDrivehead = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS | IDE_DH_DRIVE(nDriveIndex);
	IoOutputByte(IDE_REG_DRIVEHEAD(uIoBase), tsicp.m_bDrivehead);

	if ((nDriveIndex < 0) || (nDriveIndex >= 2) ||
	    (tsaHarddiskInfo[nDriveIndex].m_fDriveExists == 0))
	{
		printk("unknown drive\n");
		return 1;
	}

	if(tsaHarddiskInfo[nDriveIndex].m_fAtapi) 
	{
               	// CD - DVD ROM
		BYTE ba[12];
		int nReturn;

		IoInputByte(IDE_REG_STATUS(uIoBase));
		if(IoInputByte(IDE_REG_STATUS(uIoBase)&1)) 
		{ 	// sticky error
			if(IoInputByte(IDE_REG_ERROR(uIoBase)&0x20)) 
			{ 	// needs attention
				if(BootIdeAtapiAdditionalSenseCode(nDriveIndex, &ba[0], 2048)<12) 
				{ 	// needed as it clears NEED ATTENTION
//					printk("BootIdeReadSector sees unit needs attention but failed giving it\n");
				} else {
//					printk("BootIdeReadSector sees unit needs attention, gave it, current Error=%02X\n", IoInputByte(IDE_REG_ERROR(uIoBase)));
				}
			}
		}

		BootIdeWaitNotBusy(uIoBase);

		if(n_bytes<2048) 
		{
			printk("Must have 2048 byte sector for ATAPI!!!!!\n");
			return 1;
		}

		tsicp.m_wCylinder=2048;
		memset(ba, 0, sizeof(ba));
		//memset(&ba[0], 0, 12);
		ba[0]=0x28; 
		ba[2]=block>>24; 
		ba[3]=block>>16; 
		ba[4]=block>>8; 
		ba[5]=block; 
		ba[7]=0; 
		ba[8]=1;

		if(BootIdeIssueAtapiPacketCommandAndPacket(nDriveIndex, &ba[0])) 
		{
//			printk("BootIdeReadSector Unable to issue ATAPI command\n");
			return 1;
		}

//		printk("BootIdeReadSector issued ATAPI command\n");

		nReturn=IoInputByte(IDE_REG_CYLINDER_LSB(uIoBase));
		nReturn |=IoInputByte(IDE_REG_CYLINDER_MSB(uIoBase))<<8;
//		printk("nReturn = %x\n", nReturn);

		if(nReturn>2048) nReturn=2048;
		status = BootIdeReadData(uIoBase, pbBuffer, nReturn);
		if (status != 0) 
		{
			while(1) 
			{
				wait_ms(50);
				status = BootIdeReadData(uIoBase, pbBuffer, nReturn);				
				if (status == 0) 
				{
					break;
				}
			}
		}
		return 0;
	}

	if (tsaHarddiskInfo[nDriveIndex].m_wCountHeads > 8) 
	{
		IoOutputByte(IDE_REG_CONTROL(uIoBase), 0x0a);
	} else {
		IoOutputByte(IDE_REG_CONTROL(uIoBase), 0x02);
	}

	tsicp.m_bCountSector = 1;

	
	
	if( block >= 0x10000000 ) 
	{ 	
		/* 48-bit LBA access required for this block */ 
		
		tsicp.m_bCountSectorExt = 0;   
		
		 /* This routine can have a max LBA of 32 bits (due to unsigned int data type used for block parameter) */   
		
		tsicp.m_wCylinderExt = 0; /* 47:32 */   
		tsicp.m_bSectorExt = (block >> 24) & 0xff; /* 31:24 */   
		tsicp.m_wCylinder = (block >> 8) & 0xffff; /* 23:8 */   
		tsicp.m_bSector = block & 0xff; /* 7:0 */   
		tsicp.m_bDrivehead = IDE_DH_DRIVE(nDriveIndex) | IDE_DH_LBA;   
		ideReadCommand = IDE_CMD_READ_EXT;   
    
	} else {
        	// Looks Like we do not have LBA 48 need
        	if (tsaHarddiskInfo[nDriveIndex].m_bLbaMode == IDE_DH_CHS) 
        	{ 

			track = block / tsaHarddiskInfo[nDriveIndex].m_wCountSectorsPerTrack;
        	
			tsicp.m_bSector = 1+(block % tsaHarddiskInfo[nDriveIndex].m_wCountSectorsPerTrack);
			tsicp.m_wCylinder = track / tsaHarddiskInfo[nDriveIndex].m_wCountHeads;
			tsicp.m_bDrivehead = IDE_DH_DEFAULT |
				IDE_DH_HEAD(track % tsaHarddiskInfo[nDriveIndex].m_wCountHeads) |
				IDE_DH_DRIVE(nDriveIndex) |
				IDE_DH_CHS;
		} else {

			tsicp.m_bSector = block & 0xff; /* lower byte of block (lba) */
			tsicp.m_wCylinder = (block >> 8) & 0xffff; /* middle 2 bytes of block (lba) */
			tsicp.m_bDrivehead = IDE_DH_DEFAULT | /* set bits that must be on */
				((block >> 24) & 0x0f) | /* lower nibble of byte 3 of block */
				IDE_DH_DRIVE(nDriveIndex) |
				IDE_DH_LBA;
		}
        }       
        
	if(BootIdeIssueAtaCommand(uIoBase, ideReadCommand, &tsicp)) 
	{
		printk("ide error %02X...\n", IoInputByte(IDE_REG_ERROR(uIoBase)));
		return 1;
	}
	
	if (n_bytes != IDE_SECTOR_SIZE)
	{
		status = BootIdeReadData(uIoBase, baBufferSector, IDE_SECTOR_SIZE);
		if (status == 0) {
			memcpy(pbBuffer, baBufferSector+byte_offset, n_bytes);
		
		} else {
			// UPS, it failed, but we are brutal, we try again ....
			while(1) {
				wait_ms(50);
				status = BootIdeReadData(uIoBase, baBufferSector, IDE_SECTOR_SIZE);
				if (status == 0) {
					memcpy(pbBuffer, baBufferSector+byte_offset, n_bytes);
					break;
				}
			}
			
		}
	
	} else {
	
		status = BootIdeReadData(uIoBase, pbBuffer, IDE_SECTOR_SIZE);
		if (status!=0) {
			// UPS, it failed, but we are brutal, we try again ....
			while(1) {
				wait_ms(50);
				status = BootIdeReadData(uIoBase, pbBuffer, IDE_SECTOR_SIZE);		
				if (status == 0) {
					break;
				}
			}
		}
	}
	return status;
}

/* -------------------------------------------------------------------------------- */



/////////////////////////////////////////////////
//  BootIdeWriteSector
//
// !!!!! EXPERIMENTAL

int BootIdeWriteSector(int nDriveIndex, void * pbBuffer, unsigned int block) 
{
	tsIdeCommandParams tsicp = IDE_DEFAULT_COMMAND;
	unsigned uIoBase;
	unsigned int track;
	int status;
	unsigned char ideWriteCommand = IDE_CMD_WRITE_MULTI_RETRY; 
	
	if(!tsaHarddiskInfo[nDriveIndex].m_fDriveExists) return 4;

	uIoBase = tsaHarddiskInfo[nDriveIndex].m_fwPortBase;

	tsicp.m_bDrivehead = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS | IDE_DH_DRIVE(nDriveIndex);
	IoOutputByte(IDE_REG_DRIVEHEAD(uIoBase), tsicp.m_bDrivehead);

	if ((nDriveIndex < 0) || (nDriveIndex >= 2) ||
	    (tsaHarddiskInfo[nDriveIndex].m_fDriveExists == 0))
	{
		printk("unknown drive\n");
		return 1;
	}

	if (tsaHarddiskInfo[nDriveIndex].m_wCountHeads > 8) 
	{
		IoOutputByte(IDE_REG_CONTROL(uIoBase), 0x0a);
	} else {
		IoOutputByte(IDE_REG_CONTROL(uIoBase), 0x02);
	}

	tsicp.m_bCountSector = 1;

	
	
	if( block >= 0x10000000 ) 
	{ 	
		/* 48-bit LBA access required for this block */ 
		
		tsicp.m_bCountSectorExt = 0;   
		
		 /* This routine can have a max LBA of 32 bits (due to unsigned int data type used for block parameter) */   
		
		tsicp.m_wCylinderExt = 0; /* 47:32 */   
		tsicp.m_bSectorExt = (block >> 24) & 0xff; /* 31:24 */   
		tsicp.m_wCylinder = (block >> 8) & 0xffff; /* 23:8 */   
		tsicp.m_bSector = block & 0xff; /* 7:0 */   
		tsicp.m_bDrivehead = IDE_DH_DRIVE(nDriveIndex) | IDE_DH_LBA;   
		ideWriteCommand = IDE_CMD_WRITE_MULTI_RETRY;
    
	} else {
        	// Looks Like we do not have LBA 48 need
        	if (tsaHarddiskInfo[nDriveIndex].m_bLbaMode == IDE_DH_CHS) 
        	{ 

			track = block / tsaHarddiskInfo[nDriveIndex].m_wCountSectorsPerTrack;
        	
			tsicp.m_bSector = 1+(block % tsaHarddiskInfo[nDriveIndex].m_wCountSectorsPerTrack);
			tsicp.m_wCylinder = track / tsaHarddiskInfo[nDriveIndex].m_wCountHeads;
			tsicp.m_bDrivehead = IDE_DH_DEFAULT |
				IDE_DH_HEAD(track % tsaHarddiskInfo[nDriveIndex].m_wCountHeads) |
				IDE_DH_DRIVE(nDriveIndex) |
				IDE_DH_CHS;
		} else {

			tsicp.m_bSector = block & 0xff; /* lower byte of block (lba) */
			tsicp.m_wCylinder = (block >> 8) & 0xffff; /* middle 2 bytes of block (lba) */
			tsicp.m_bDrivehead = IDE_DH_DEFAULT | /* set bits that must be on */
				((block >> 24) & 0x0f) | /* lower nibble of byte 3 of block */
				IDE_DH_DRIVE(nDriveIndex) |
				IDE_DH_LBA;
		}
        }       
        
	if(BootIdeIssueAtaCommand(uIoBase, ideWriteCommand, &tsicp)) 
	{
		printk("ide error %02X...\n", IoInputByte(IDE_REG_ERROR(uIoBase)));
		return 1;
	}
	
	status = BootIdeWriteData(uIoBase, pbBuffer, IDE_SECTOR_SIZE);
	

	return status;
}






/* -------------------------------------------------------------------------------- */

///////////////////////////////////////////////
//      BootIdeBootSectorHddOrElTorito
//
//  Attempts to load boot code from Hdd or from CDROM/DVDROM
//   If HDD, loads MBR from Sector 0, if CDROM, uses El Torito to load default boot sector
//
// returns 0 if *pbaResult loaded with (512-byte/Hdd, 2048-byte/Cdrom) boot sector
//  otherwise nonzero return indicates error type

int BootIdeBootSectorHddOrElTorito(int nDriveIndex, BYTE * pbaResult)
{
	static const BYTE baCheck11hFormat[] = {
			0x00,0x43,0x44,0x30,0x30,0x31,0x01,0x45,
			0x4C,0x20,0x54,0x4F,0x52,0x49,0x54,0x4F,
			0x20,0x53,0x50,0x45,0x43,0x49,0x46,0x49,
			0x43,0x41,0x54,0x49,0x4F,0x4E
	};
	int n;
	DWORD * pdw;

	if(!tsaHarddiskInfo[nDriveIndex].m_fDriveExists) return 4;

	if(tsaHarddiskInfo[nDriveIndex].m_fAtapi) {

/******   Numbnut's guide to El Torito CD Booting   ********

  Sector 11h of a bootable CDROM looks like this (11h is a magic number)
  The DWORD starting at +47h is the sector index of the 'boot catalog'

00000000: 00 43 44 30 30 31 01 45 : 4C 20 54 4F 52 49 54 4F    .CD001.EL TORITO
00000010: 20 53 50 45 43 49 46 49 : 43 41 54 49 4F 4E 00 00     SPECIFICATION..
00000020: 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00    ................
00000030: 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00    ................
00000040: 00 00 00 00 00 00 00 13 : 00 00 00 00 00 00 00 00    ................
*/

		if(BootIdeReadSector(nDriveIndex, &pbaResult[0], 0x11, 0, 2048)) {
			bprintf("Unable to get first sector\n");
			return 1;
		}

		for(n=0;n<sizeof(baCheck11hFormat);n++) {
			if(pbaResult[n]!=baCheck11hFormat[n]) {
				bprintf("Sector 11h not bootable format\n");
				return 2;
			}
		}

		pdw=(DWORD *)&pbaResult[0x47];

/*
At sector 13h (in this example only), the boot catalog:

00000000: 01 00 00 00 4D 69 63 72 : 6F 73 6F 66 74 20 43 6F    ....Microsoft Co
00000010: 72 70 6F 72 61 74 69 6F : 6E 00 00 00 4C 49 55 AA    rporation...LIU.
(<--- validation entry)
00000020: 88 00 00 00 00 00 04 00 : 25 01 00 00 00 00 00 00    ........%.......
(<-- initial/default entry - 88=bootable, 04 00 = 4 x (512-byte virtual sectors),
  = 1 x 2048-byte CDROM sector in boot, 25 01 00 00 = starts at sector 0x125)
*/

		if(BootIdeReadSector(nDriveIndex, &pbaResult[0], *pdw,  0, 2048)) {
			bprintf("Unable to get boot catalog\n");
			return 3;
		}

		if((pbaResult[0]!=1) || (pbaResult[0x1e]!=0x55) || (pbaResult[0x1f]!=0xaa)) {
			bprintf("Boot catalog header corrupt\n");
			return 4;
		}

		if(pbaResult[0x20]!=0x88) {
			bprintf("Default boot catalog entry is not bootable\n");
			return 4;
		}

		pdw=(DWORD *)&pbaResult[0x28];
/*
And so at sector 0x125 (in this example only), we finally see the boot code

00000000: FA 33 C0 8E D0 BC 00 7C : FB 8C C8 8E D8 52 E8 00    .3.....|.....R..
00000010: 00 5E 81 EE 11 00 74 12 : 81 FE 00 7C 75 75 8C C8    .^....t....|uu..
00000020: 3D 00 00 75 7F EA 37 00 : C0 07 C6 06 AE 01 33 90    =..u..7.......3.
...
000007E0: 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00    ................
000007F0: 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 55 AA    ..............U.
*/

		if(BootIdeReadSector(nDriveIndex, &pbaResult[0], *pdw,  0, 2048)) {
			bprintf("Unable to get boot catalog\n");
			return 3;
		}

		if((pbaResult[0x7fe]!=0x55) || (pbaResult[0x7ff]!=0xaa)) {
			bprintf("Boot sector does not have boot signature!\n");
			return 4;
		}

		return 0; // success

	} else { // HDD boot

		if(BootIdeReadSector(nDriveIndex, &pbaResult[0], 0,  0, 512)) {
			bprintf("Unable to get MBR\n");
			return 3;
		}

		if((pbaResult[0x1fe]!=0x55) || (pbaResult[0x1ff]!=0xaa)) {
			bprintf("Boot sector does not have boot signature!\n");
			return 4;
		}

		return 0; // succes
	}
}

	// these guys are used by grub
/* -------------------------------------------------------------------------------- */

int get_diskinfo (int drive, struct geometry *geometry)
{
//	printk("get_diskinfo for drive %d\n", drive);
	if(drive>1) return 1; // fail
	geometry->cylinders=tsaHarddiskInfo[drive].m_wCountCylinders;
	geometry->heads=tsaHarddiskInfo[drive].m_wCountHeads;
	geometry->sectors=tsaHarddiskInfo[drive].m_wCountSectorsPerTrack;
	geometry->total_sectors=tsaHarddiskInfo[drive].m_dwCountSectorsTotal;
	geometry->flags=0;

//	printk("geometry->total_sectors=0x%X\n", geometry->total_sectors);

	return 0; // success
}

/* -------------------------------------------------------------------------------- */

int BootIdeSetTransferMode(int nIndexDrive, int nMode)
{
	tsIdeCommandParams tsicp = IDE_DEFAULT_COMMAND;
	unsigned int uIoBase = tsaHarddiskInfo[nIndexDrive].m_fwPortBase;
	BYTE b;
//	DWORD dw;

	tsicp.m_bDrivehead = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS | IDE_DH_DRIVE(nIndexDrive);
	IoOutputByte(IDE_REG_DRIVEHEAD(uIoBase), tsicp.m_bDrivehead);
	
	IoOutputByte(0xff60+2, 0x62); // DMA possible for both drives

	if(tsaHarddiskInfo[nIndexDrive].m_fAtapi) {
		IoOutputByte(IDE_REG_CONTROL(uIoBase), 0x08); // enable interrupt,
		IoOutputByte(IDE_REG_FEATURE(uIoBase), 0x01); // enable DMA
		return 0;
	}

	IoOutputByte(IDE_REG_CONTROL(uIoBase), 0x08); // enable interrupt
	IoOutputByte(IDE_REG_FEATURE(uIoBase), 0x01); // enable DMA

	if(tsaHarddiskInfo[nIndexDrive].m_bCableConductors==80) {

	}

	if(BootIdeWaitNotBusy(uIoBase)) {
			printk("  Drive %d: Not Ready\n", nIndexDrive);
			return 1;
	}
	{
		int nReturn=0;

		tsicp.m_bCountSector = (BYTE)nMode;
		IoOutputByte(IDE_REG_FEATURE(uIoBase), 3); // set transfer mode subcmd

		nReturn=BootIdeIssueAtaCommand(uIoBase, IDE_CMD_SET_FEATURES, &tsicp);

//		printk("BootIdeSetTransferMode nReturn = %x/ error %02X\n", nReturn, IoInputByte(IDE_REG_ERROR(uIoBase)) );

		switch(nMode&7) {
			case 0: b=3; break;
			case 1: b=1; break;
			case 2: b=0; break;
			case 3: b=4; break;
			case 4: b=5; break;
			case 5: b=6; break;
			default: b=6; break;
		}
		/*
		b|=0xc0;
		dw=PciReadDword(BUS_0, DEV_9, FUNC_0, 0x60);
		if(nIndexDrive) { // slave
			dw&=0xff00ff00;
			dw|=(b<<16) | (b);
		} else { // primary
			dw&=0x00ff00ff;
			dw|=(b<<24) | (b<<8);
		}
//		PciWriteDword(BUS_0, DEV_9, FUNC_0, 0x60, dw); // new
                 */
		return nReturn;
	}
	return 0;
}

// Would be much better of course to pull out the hd security stuff to it's own function
// instead of duplicating everywhere.
int BootIdeUnlockHD(void)
{
	if ((tsaHarddiskInfo[0].m_wSecurityFlags & IDE_SECURITY_SUPPORTED) && 
		(tsaHarddiskInfo[0].m_wSecurityFlags & IDE_SECURITY_ENABLED))
	{
		BYTE baBuffer[512];
		unsigned uIoBase = IDE_BASE1;
		// 'security' is in force, unlock the drive (was 0x104/0x104)
		BYTE baMagic[0x200], baKeyFromEEPROM[0x10], baEeprom[0x30];
		bool fUnlocked=false;
		int n,nVersionHashing;
		tsIdeCommandParams tsicp = IDE_DEFAULT_COMMAND;
		tsIdeCommandParams tsicp1 = IDE_DEFAULT_COMMAND;
		DWORD dwMagicFromEEPROM;
		DWORD BootHddKeyGenerateEepromKeyData(BYTE *eeprom_data,BYTE *HDKey);
		int nVersionSuccessfulDecrypt=0;
//		printk(" Lck[%x]", tsaHarddiskInfo[0].m_wSecurityFlags);

		dwMagicFromEEPROM=0;
		nVersionHashing = 0;
		memcpy(&baEeprom[0], &eeprom, 0x30); // first 0x30 bytes from EEPROM image we picked up earlier
	
		for(n=0;n<0x10;n++) baKeyFromEEPROM[n]=0;
		nVersionHashing = BootHddKeyGenerateEepromKeyData( &baEeprom[0], &baKeyFromEEPROM[0]);
	
		for(n=0;n<0x200;n++) baMagic[n]=0;
	
		#ifdef HDD_DEBUG
			printk("Model='%s', Serial='%s'\n", tsaHarddiskInfo[0].m_szIdentityModelNumber, tsaHarddiskInfo[0].m_szSerial);
			VideoDumpAddressAndData(0, &baKeyFromEEPROM[0], 0x10);
		#endif
	
		// Claculate the hdd pw from EErpom and Serial / Model Number
		HMAC_SHA1 (&baMagic[2], baKeyFromEEPROM, 0x10,
			tsaHarddiskInfo[0].m_szIdentityModelNumber,
			tsaHarddiskInfo[0].m_length,
			tsaHarddiskInfo[0].m_szSerial,
			tsaHarddiskInfo[0].s_length);
	                       
		if (nVersionHashing == 0)
		{
//			printk("Got a 0 return from BootHddKeyGenerateEepromKeyData\n");
			// ERRORR -- Corrupt EEprom or Newer Version of EEprom - key
		}
	
	
		nVersionSuccessfulDecrypt=nVersionHashing;
	
	
	
		// clear down the unlock packet, except for b8 set in first word (high security unlock)
		{
			WORD * pword=(WORD *)&baMagic[0];
			*pword=0;  // try user
		}
	
		if(BootIdeWaitNotBusy(uIoBase)) 
		{
//			printk("  %d:  Not Ready\n", 0);
			return 1;
		}
		tsicp1.m_bDrivehead = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS | IDE_DH_DRIVE(0);
	
		if(BootIdeIssueAtaCommand(uIoBase, IDE_CMD_SECURITY_UNLOCK, &tsicp1)) 
		{
//			printk("  %d:  when issuing unlock command FAILED, error=%02X\n", 0, IoInputByte(IDE_REG_ERROR(uIoBase)));
			return 1;
		}
		BootIdeWaitDataReady(uIoBase);
		BootIdeWriteData(uIoBase, &baMagic[0], IDE_SECTOR_SIZE);
        	
		if (BootIdeWaitNotBusy(uIoBase))	
		{
//			printk("..");
			{
				WORD * pword=(WORD *)&baMagic[0];
				*pword=1;  // try master
			}

			if(BootIdeIssueAtaCommand(uIoBase, IDE_CMD_SECURITY_UNLOCK, &tsicp1)) 
			{
//				printk("  %d:  when issuing unlock command FAILED, error=%02X\n", 0, IoInputByte(IDE_REG_ERROR(uIoBase)));
				return 1;
			}
			BootIdeWaitDataReady(uIoBase);
			BootIdeWriteData(uIoBase, &baMagic[0], IDE_SECTOR_SIZE);

			if (BootIdeWaitNotBusy(uIoBase))	
			{
//						printk("  - Both Master and User unlock attempts failed\n");
			}
		}

		// check that we are unlocked

		tsicp.m_bDrivehead = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS | IDE_DH_DRIVE(0);
		if(BootIdeIssueAtaCommand(uIoBase, IDE_CMD_GET_INFO, &tsicp)) 
		{
//			printk("  %d:  on issuing get status after unlock detect FAILED, error=%02X\n", 0, IoInputByte(IDE_REG_ERROR(uIoBase)));
			return 1;
		}
		BootIdeWaitDataReady(uIoBase);
		if(BootIdeReadData(uIoBase, baBuffer, IDE_SECTOR_SIZE)) 
		{
//			printk("  %d:  BootIdeReadData FAILED, error=%02X\n", 0, IoInputByte(IDE_REG_ERROR(uIoBase)));
			return 1;
		}
		tsaHarddiskInfo[0].m_wSecurityFlags = ((unsigned short *)baBuffer)[128];

		if(tsaHarddiskInfo[0].m_wSecurityFlags&0x0004) 
		{
//			printk("  %d:  FAILED to unlock drive, security: %04x\n", 0, tsaHarddiskInfo[0].m_wSecurityFlags);
		} else {
			fUnlocked=true;

		}

			

		if(!fUnlocked)
		{
			return 1;
		}

		if(BootIdeIssueAtaCommand(uIoBase, IDE_CMD_SECURITY_DISABLE, &tsicp1)) 
		{
//			printk("  %d:  when issuing unlock command FAILED, error=%02X\n", 0, IoInputByte(IDE_REG_ERROR(uIoBase)));
			return 1;
		}
		BootIdeWaitDataReady(uIoBase);
		BootIdeWriteData(uIoBase, &baMagic[0], IDE_SECTOR_SIZE);
		
		tsicp.m_bDrivehead = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS | IDE_DH_DRIVE(0);
		if(BootIdeIssueAtaCommand(uIoBase, IDE_CMD_GET_INFO, &tsicp)) 
		{
//			printk("  %d:  on issuing get status after unlock detect FAILED, error=%02X\n", 0, IoInputByte(IDE_REG_ERROR(uIoBase)));
			return 1;
		}
		BootIdeWaitDataReady(uIoBase);
		if(BootIdeReadData(uIoBase, baBuffer, IDE_SECTOR_SIZE)) 
		{
//			printk("  %d:  BootIdeReadData FAILED, error=%02X\n", 0, IoInputByte(IDE_REG_ERROR(uIoBase)));
			return 1;
		}
		tsaHarddiskInfo[0].m_wSecurityFlags = ((unsigned short *)baBuffer)[128];
//		printk("post-unlock sec status: 0x%x\n", drive_info[128]);
		if ((tsaHarddiskInfo[0].m_wSecurityFlags & IDE_SECURITY_SUPPORTED) && 
			(tsaHarddiskInfo[0].m_wSecurityFlags & IDE_SECURITY_ENABLED))
		{
			return 1;
		}
	}
	
	return 0;	
}

// Would be much better of course to pull out the hd security stuff to it's own function
// instead of duplicating everywhere
int BootIdeLockHD(void)
{
	if ((tsaHarddiskInfo[0].m_wSecurityFlags & IDE_SECURITY_SUPPORTED) && !(tsaHarddiskInfo[0].m_wSecurityFlags & IDE_SECURITY_ENABLED))
	{
		BYTE baBuffer[512];
		unsigned uIoBase = IDE_BASE1;
		// 'security' is in force, unlock the drive (was 0x104/0x104)
		BYTE baMagic[0x200], baKeyFromEEPROM[0x10], baEeprom[0x30];
		bool fUnlocked=false;
		int n,nVersionHashing;
		tsIdeCommandParams tsicp = IDE_DEFAULT_COMMAND;
		tsIdeCommandParams tsicp1 = IDE_DEFAULT_COMMAND;
		DWORD dwMagicFromEEPROM;
		DWORD BootHddKeyGenerateEepromKeyData(BYTE *eeprom_data,BYTE *HDKey);
		int nVersionSuccessfulDecrypt=0;
//		printk(" Lck[%x]", tsaHarddiskInfo[0].m_wSecurityFlags);

		dwMagicFromEEPROM=0;
		nVersionHashing = 0;
		memcpy(&baEeprom[0], &eeprom, 0x30); // first 0x30 bytes from EEPROM image we picked up earlier
	
		for(n=0;n<0x10;n++) baKeyFromEEPROM[n]=0;
		nVersionHashing = BootHddKeyGenerateEepromKeyData( &baEeprom[0], &baKeyFromEEPROM[0]);
	
		for(n=0;n<0x200;n++) baMagic[n]=0;
	
		#ifdef HDD_DEBUG
			printk("Model='%s', Serial='%s'\n", tsaHarddiskInfo[0].m_szIdentityModelNumber, tsaHarddiskInfo[0].m_szSerial);
			VideoDumpAddressAndData(0, &baKeyFromEEPROM[0], 0x10);
		#endif
	
		// Claculate the hdd pw from EErpom and Serial / Model Number
		HMAC_SHA1 (&baMagic[2], baKeyFromEEPROM, 0x10,
			tsaHarddiskInfo[0].m_szIdentityModelNumber,
			tsaHarddiskInfo[0].m_length,
			tsaHarddiskInfo[0].m_szSerial,
			tsaHarddiskInfo[0].s_length);
	                       
		if (nVersionHashing == 0)
		{
//			printk("Got a 0 return from BootHddKeyGenerateEepromKeyData\n");
			// ERRORR -- Corrupt EEprom or Newer Version of EEprom - key
		}
	
	
		nVersionSuccessfulDecrypt=nVersionHashing;
	
	
	
		// clear down the lock packet, except for b8 set in first word (high security lock)
		{
			WORD * pword=(WORD *)&baMagic[0];
			*pword=0;  // set user
		}
	
		if(BootIdeWaitNotBusy(uIoBase)) 
		{
//			printk("  %d:  Not Ready\n", 0);
			return 1;
		}
		tsicp1.m_bDrivehead = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS | IDE_DH_DRIVE(0);
	
		if(BootIdeIssueAtaCommand(uIoBase, IDE_CMD_SECURITY_SETPASSWORD, &tsicp1)) 
		{
//			printk("  %d:  when issuing unlock command FAILED, error=%02X\n", 0, IoInputByte(IDE_REG_ERROR(uIoBase)));
			return 1;
		}
		BootIdeWaitDataReady(uIoBase);
		BootIdeWriteData(uIoBase, &baMagic[0], IDE_SECTOR_SIZE);
        	
/*		if (BootIdeWaitNotBusy(uIoBase))	
		{
			printk("..");
			{
				WORD * pword=(WORD *)&baMagic[0];
				*pword=1;  // try master
			}

			if(BootIdeIssueAtaCommand(uIoBase, IDE_CMD_SECURITY_UNLOCK, &tsicp1)) 
			{
				printk("  %d:  when issuing unlock command FAILED, error=%02X\n", 0, IoInputByte(IDE_REG_ERROR(uIoBase)));
				return 1;
			}
			BootIdeWaitDataReady(uIoBase);
			BootIdeWriteData(uIoBase, &baMagic[0], IDE_SECTOR_SIZE);

			if (BootIdeWaitNotBusy(uIoBase))	
			{
//						printk("  - Both Master and User unlock attempts failed\n");
			}
		}
*/
		// check that we are locked
		BootIdeWaitNotBusy(uIoBase);	

		tsicp.m_bDrivehead = IDE_DH_DEFAULT | IDE_DH_HEAD(0) | IDE_DH_CHS | IDE_DH_DRIVE(0);
		if(BootIdeIssueAtaCommand(uIoBase, IDE_CMD_GET_INFO, &tsicp)) 
		{
//			printk("  %d:  on issuing get status after unlock detect FAILED, error=%02X\n", 0, IoInputByte(IDE_REG_ERROR(uIoBase)));
			return 1;
		}
		BootIdeWaitDataReady(uIoBase);
		if(BootIdeReadData(uIoBase, baBuffer, IDE_SECTOR_SIZE)) 
		{
//			printk("  %d:  BootIdeReadData FAILED, error=%02X\n", 0, IoInputByte(IDE_REG_ERROR(uIoBase)));
			return 1;
		}
		tsaHarddiskInfo[0].m_wSecurityFlags = ((unsigned short *)baBuffer)[128];

		if ((tsaHarddiskInfo[0].m_wSecurityFlags & IDE_SECURITY_SUPPORTED) && 
			!(tsaHarddiskInfo[0].m_wSecurityFlags & IDE_SECURITY_ENABLED))
		{
			return 1;
		}
	}
	return 0;	
}

/* -------------------------------------------------------------------------------- */


/*



	#ifdef HDD_SANITY

		printk("\n");

		{ // algorithm sanity check  Andy's drive
			const BYTE baAndysOriginalv10Box[] = {
				0x6D, 0xAB, 0x59, 0xA2, 0xB8, 0x82, 0x09, 0xAB, 0x21, 0x84, 0xB2, 0x50, 0x8A, 0x7F, 0x4F, 0x43,
				0x54, 0x01, 0x1E, 0x52, 0xD3, 0xB6, 0x3A, 0x5C, 0x32, 0xA6, 0x11, 0x28, 0x72, 0x07, 0xAE, 0x3C,
				0x36, 0xD4, 0x83, 0xFB, 0xE0, 0x29, 0xEE, 0xA8, 0x1C, 0x9D, 0x14, 0xEF, 0x44, 0x39, 0x65, 0x37
			};
			const BYTE baEepKeyAndysOriginalv10Box[] = {
				0x53, 0x70, 0x17, 0x9C, 0x73, 0x06, 0x94, 0x0A, 0x18, 0xF8, 0x70, 0x69, 0x4C, 0x63, 0x5E, 0xB0
			};
			const BYTE baPasswordAndysOriginalV10Box[] = {
				0x45, 0x79, 0x8D, 0xF1, 0xE9, 0xF5, 0x90, 0xA3, 0x3F, 0xDA, 0xAB, 0x3C, 0xE2, 0x5B, 0x1a, 0x41, 0x9d, 0x24, 0xb6, 0xcc
			};
			const char szModelAndysOriginalv10Box[] = "ST310211A";
			const char szSerialAndysOriginalv10Box[] = "6DB2WF1K";

			if(BootHddKeyGenerateEepromKeyData( (BYTE *)&baEepromAndysOriginalv10Box[0], &baKeyFromEEPROM[0])!=10) {
				printk("HDD p/w sanity check fail: not v1.0 sig\n");
				while(1);
			}

			if(_memcmp(baEepKeyAndysOriginalv10Box, baKeyFromEEPROM, 16)) {
				printk("HDD p/w sanity check fail: EEP key mismatch\n");
				VideoDumpAddressAndData(0, &baKeyFromEEPROM[0], 0x10);
				while(1);
			}

			HMAC_SHA1 (&baMagic[2], baKeyFromEEPROM, 0x10,
					 (char *)szModelAndysOriginalv10Box,
					 strlen(szModelAndysOriginalv10Box),
					 (char *)szSerialAndysOriginalv10Box,
					 strlen(szSerialAndysOriginalv10Box)
			);

			if(_memcmp(baPasswordAndysOriginalV10Box, &baMagic[2], 20)) {
				printk("HDD p/w sanity check fail: Password mismatch\n");
				VideoDumpAddressAndData(0, &baMagic[2], 20);
				while(1);
			}

			printk("HDD p/w: Passed sanity check for Andy's drive\n");
		}


		{ // algorithm sanity check  ED's drive
			const BYTE baEepromEdsOriginalv10Box[] = {
					0x47, 0xa9, 0x03, 0x95, 0xed, 0x0c, 0xc1, 0x72, 0x15, 0x11, 0xbe, 0x91, 0x5e, 0x80, 0xd9, 0xa6,
					0xba, 0x91, 0xaf, 0x63, 0x32, 0x6e, 0x26, 0xf6, 0x96, 0x77, 0xa3, 0xf7, 0x55, 0x4f, 0xb6, 0x5f,
					0x58, 0xf9, 0x33, 0x48, 0x0f, 0xdb, 0x3e, 0xfc, 0xf8, 0xab, 0x33, 0x55, 0xb7, 0xcc, 0x4b, 0x81
			};
			const BYTE baEepKeyEdsOriginalv10Box[] = {
				0x04, 0x24, 0xa0, 0x49, 0x5f, 0x0b, 0x98, 0x90, 0x50, 0xe1, 0x34, 0x46, 0x3f, 0x1a, 0x0e, 0x34
			};
			const BYTE baPasswordEdsOriginalV10Box[] = {
//				0x0F, 0xC0, 0xEC, 0xA1, 0x30, 0xE9, 0x62, 0x78, 0xE8, 0xE2, 0x87, 0xD6, 0xF1, 0x28, 0x93, 0x15, 0xBF, 0x7D, 0xCC, 0x46
				0xc8, 0x76, 0xd2, 0xaf, 0xf4, 0x59, 0x53, 0x59, 0xc2, 0xc9, 0x56, 0x07, 0x5b, 0xb7, 0x87, 0x18, 0xe4, 0x20, 0xcf, 0x04
			};
			const char szModelEdsOriginalv10Box[] = "WDC WD80EB-28CGH1";
			const char szSerialEdsOriginalv10Box[] = "WD-WMA9N4505862\0\0\0\0\0\0\0\0\0\0";

			if(BootHddKeyGenerateEepromKeyData( (BYTE *)&baEepromEdsOriginalv10Box[0], &baKeyFromEEPROM[0])!=10) {
				printk("Ed's HDD p/w sanity check fail: not v1.0 sig\n");
				while(1);
			}

			if(_memcmp(baEepKeyEdsOriginalv10Box, baKeyFromEEPROM, 16)) {
				printk("Ed's HDD p/w sanity check fail: EEP key mismatch\n");
				VideoDumpAddressAndData(0, &baKeyFromEEPROM[0], 0x10);
				while(1);
			}

			HMAC_SHA1 (&baMagic[2], baKeyFromEEPROM, 0x10,
					 (char *)szModelEdsOriginalv10Box,
					 strlen(szModelEdsOriginalv10Box),
					 (char *)szSerialEdsOriginalv10Box,
					 20 // strlen(szSerialEdsOriginalv10Box)
			);

			if(_memcmp(baPasswordEdsOriginalV10Box, &baMagic[2], 20)) {
				printk("Ed's HDD p/w sanity check fail: Password mismatch\n");
				VideoDumpAddressAndData(0, &baMagic[2], 20);
				while(1);
			}

			printk("HDD p/w: Passed sanity check for Ed's drive\n");
		}
	#endif

  */
