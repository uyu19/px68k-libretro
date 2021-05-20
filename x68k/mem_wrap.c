/*	$Id: mem_wrap.c,v 1.2 2003/12/05 18:07:19 nonaka Exp $	*/

#include "common.h"
#include "memory.h"
#include "../m68000/m68000.h"
#include "winx68k.h"

#include "adpcm.h"
#include "bg.h"
#include "crtc.h"
#include "dmac.h"
#include "fdc.h"
#include "gvram.h"
#include "mercury.h"
#include "mfp.h"
#include "midi.h"
#include "ioc.h"
#include "palette.h"
#include "pia.h"
#include "rtc.h"
#include "sasi.h"
#include "scc.h"
#include "scsi.h"
#include "sram.h"
#include "sysport.h"
#include "tvram.h"

#include "fmg_wrap.h"

void AdrError(DWORD, DWORD);
void BusError(DWORD, DWORD);

static void wm_main(DWORD addr, BYTE val);
static void wm_cnt(DWORD addr, BYTE val);
static void wm_buserr(DWORD addr, BYTE val);
static void wm_opm(DWORD addr, BYTE val);
static void wm_e82(DWORD addr, BYTE val);
static void wm_nop(DWORD addr, BYTE val);

static BYTE rm_main(DWORD addr);
static BYTE rm_font(DWORD addr);
static BYTE rm_ipl(DWORD addr);
static BYTE rm_nop(DWORD addr);
static BYTE rm_opm(DWORD addr);
static BYTE rm_e82(DWORD addr);
static BYTE rm_buserr(DWORD addr);

void cpu_setOPbase24(DWORD addr);
void Memory_ErrTrace(void);

BYTE (*MemReadTable[])(DWORD) = {
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read, TVRAM_Read,
	CRTC_Read, rm_e82, DMA_Read, rm_nop, MFP_Read, RTC_Read, rm_nop, SysPort_Read,
	rm_opm, ADPCM_Read, FDC_Read, SASI_Read, SCC_Read, PIA_Read, IOC_Read, rm_nop,
	SCSI_Read, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, MIDI_Read,
	BG_Read, BG_Read, BG_Read, BG_Read, BG_Read, BG_Read, BG_Read, BG_Read,
#ifndef	NO_MERCURY
	rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, Mcry_Read, rm_buserr,
#else
	rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr,
#endif
	SRAM_Read, SRAM_Read, SRAM_Read, SRAM_Read, SRAM_Read, SRAM_Read, SRAM_Read, SRAM_Read,
	rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr,
	rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr, rm_buserr,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
	rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font, rm_font,
/* SCSI の場合は rm_buserr になる？ */
	rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl,
	rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl,
	rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl,
	rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl, rm_ipl,
};

void (*MemWriteTable[])(DWORD, BYTE) = {
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write, TVRAM_Write,
	CRTC_Write, wm_e82, DMA_Write, wm_nop, MFP_Write, RTC_Write, wm_nop, SysPort_Write,
	wm_opm, ADPCM_Write, FDC_Write, SASI_Write, SCC_Write, PIA_Write, IOC_Write, wm_nop,
	SCSI_Write, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, MIDI_Write,
	BG_Write, BG_Write, BG_Write, BG_Write, BG_Write, BG_Write, BG_Write, BG_Write,
#ifndef	NO_MERCURY
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, Mcry_Write, wm_buserr,
#else
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
#endif
	SRAM_Write, SRAM_Write, SRAM_Write, SRAM_Write, SRAM_Write, SRAM_Write, SRAM_Write, SRAM_Write,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
/* ROMエリアへの書きこみは全てバスエラー */
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
	wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr, wm_buserr,
};

BYTE *IPL;
BYTE *MEM;
BYTE *OP_ROM;
BYTE *FONT;

DWORD BusErrFlag = 0;
DWORD BusErrHandling = 0;
DWORD BusErrAdr;
DWORD MemByteAccess = 0;

/*
 * write function
 */
void 
dma_writemem24(DWORD addr, BYTE val)
{

	MemByteAccess = 0;

	wm_main(addr, val);
}

void 
dma_writemem24_word(DWORD addr, WORD val)
{

	MemByteAccess = 0;

	if (addr & 1) {
		BusErrFlag |= 4;
		return;
	}

	wm_main(addr, (val >> 8) & 0xff);
	wm_main(addr + 1, val & 0xff);
}

void 
dma_writemem24_dword(DWORD addr, DWORD val)
{

	MemByteAccess = 0;

	if (addr & 1) {
		BusErrFlag |= 4;
		return;
	}

	wm_main(addr, (val >> 24) & 0xff);
	wm_main(addr + 1, (val >> 16) & 0xff);
	wm_main(addr + 2, (val >> 8) & 0xff);
	wm_main(addr + 3, val & 0xff);
}

void 
cpu_writemem24(DWORD addr, DWORD val)
{

	MemByteAccess = 0;
	BusErrFlag = 0;

	wm_cnt(addr, val & 0xff);
	if (BusErrFlag & 2) {
		Memory_ErrTrace();
		BusError(addr, val);
	}
}

void 
cpu_writemem24_word(DWORD addr, DWORD val)
{

	MemByteAccess = 0;

	if (addr & 1) {
		AdrError(addr, val);
		return;
	}

	BusErrFlag = 0;

	wm_cnt(addr, (val >> 8) & 0xff);
	wm_main(addr + 1, val & 0xff);

	if (BusErrFlag & 2) {
		Memory_ErrTrace();
		BusError(addr, val);
	}
}

void 
cpu_writemem24_dword(DWORD addr, DWORD val)
{

	MemByteAccess = 0;

	if (addr & 1) {
		AdrError(addr, val);
		return;
	}

	BusErrFlag = 0;

	wm_cnt(addr, (val >> 24) & 0xff);
	wm_main(addr + 1, (val >> 16) & 0xff);
	wm_main(addr + 2, (val >> 8) & 0xff);
	wm_main(addr + 3, val & 0xff);

	if (BusErrFlag & 2) {
		Memory_ErrTrace();
		BusError(addr, val);
	}
}

static void 
wm_main(DWORD addr, BYTE val)
{

	if ((BusErrFlag & 7) == 0)
		wm_cnt(addr, val);
}

static void 
wm_cnt(DWORD addr, BYTE val)
{

	addr &= 0x00ffffff;
	if (addr < 0x00c00000) {	// Use RAM upto 12MB
		MEM[addr ^ 1] = val;
	} else if (addr < 0x00e00000) {
		GVRAM_Write(addr, val);
	} else {
		MemWriteTable[(addr >> 13) & 0xff](addr, val);
	}
}

static void 
wm_buserr(DWORD addr, BYTE val)
{

	BusErrFlag = 2;
	BusErrAdr = addr;
	(void)val;
}

static void 
wm_opm(DWORD addr, BYTE val)
{
	BYTE t;
#ifdef RFMDRV
	char buf[2];
#endif

	t = addr & 3;
	if (t == 1) {
		OPM_Write(0, val);
	} else if (t == 3) {
		OPM_Write(1, val);
	}
#ifdef RFMDRV
	buf[0] = t;
	buf[1] = val;
	send(rfd_sock, buf, sizeof(buf), 0);
#endif
}

static void 
wm_e82(DWORD addr, BYTE val)
{

	if (addr < 0x00e82400) {
		Pal_Write(addr, val);
	} else if (addr < 0x00e82700) {
		VCtrl_Write(addr, val);
	}
}

static void 
wm_nop(DWORD addr, BYTE val)
{

	/* Nothing to do */
	(void)addr;
	(void)val;
}

/*
 * read function
 */
BYTE 
dma_readmem24(DWORD addr)
{

	return rm_main(addr);
}

WORD 
dma_readmem24_word(DWORD addr)
{
	WORD v;

	if (addr & 1) {
		BusErrFlag = 3;
		return 0;
	}

	v = rm_main(addr++) << 8;
	v |= rm_main(addr);
	return v;
}

DWORD 
dma_readmem24_dword(DWORD addr)
{
	DWORD v;

	if (addr & 1) {
		BusErrFlag = 3;
		return 0;
	}

	v = rm_main(addr++) << 24;
	v |= rm_main(addr++) << 16;
	v |= rm_main(addr++) << 8;
	v |= rm_main(addr);
	return v;
}

DWORD 
cpu_readmem24(DWORD addr)
{
	BYTE v;

	v = rm_main(addr);
	if (BusErrFlag & 1) {
		p6logd("func = %s addr = %x flag = %d\n", __func__, addr, BusErrFlag);
		Memory_ErrTrace();
		BusError(addr, 0);
	}
	return (DWORD) v;
}

DWORD 
cpu_readmem24_word(DWORD addr)
{
	WORD v;

	if (addr & 1) {
		AdrError(addr, 0);
		return 0;
	}

	BusErrFlag = 0;

	v = rm_main(addr++) << 8;
	v |= rm_main(addr);
	if (BusErrFlag & 1) {
		p6logd("func = %s addr = %x flag = %d\n", __func__, addr, BusErrFlag);
		Memory_ErrTrace();
		BusError(addr, 0);
	}
	return (DWORD) v;
}

DWORD 
cpu_readmem24_dword(DWORD addr)
{
	DWORD v;

	MemByteAccess = 0;

	if (addr & 1) {
		BusErrFlag = 3;
		p6logd("func = %s addr = %x\n", __func__, addr);
		return 0;
	}

	BusErrFlag = 0;

	v = rm_main(addr++) << 24;
	v |= rm_main(addr++) << 16;
	v |= rm_main(addr++) << 8;
	v |= rm_main(addr);
	return v;
}

static BYTE 
rm_main(DWORD addr)
{
	BYTE v;

	addr &= 0x00ffffff;
	if (addr < 0x00c00000) {	// Use RAM upto 12MB
		v = MEM[addr ^ 1];
	} else if (addr < 0x00e00000) {
		v = GVRAM_Read(addr);
	} else {
		v = MemReadTable[(addr >> 13) & 0xff](addr);
	}

	return v;
}

static BYTE 
rm_font(DWORD addr)
{

	return FONT[addr & 0xfffff];
}

static BYTE 
rm_ipl(DWORD addr)
{

	return IPL[(addr & 0x3ffff) ^ 1];
}

static BYTE 
rm_nop(DWORD addr)
{

	(void)addr;
	return 0;
}

static BYTE 
rm_opm(DWORD addr)
{

	if ((addr & 3) == 3) {
		return OPM_Read(0);
	}
	return 0;
}

static BYTE 
rm_e82(DWORD addr)
{

	if (addr < 0x00e82400) {
		return Pal_Read(addr);
	} else if (addr < 0x00e83000) {
		return VCtrl_Read(addr);
	}
	return 0;
}

static BYTE 
rm_buserr(DWORD addr)
{
    p6logd("func = %s addr = %x flag = %d\n", __func__, addr, BusErrFlag);

	BusErrFlag = 1;
	BusErrAdr = addr;

	return 0;
}

/*
 * Memory misc
 */
void Memory_Init(void)
{

//        cpu_setOPbase24((DWORD)C68k_Get_Reg(&C68K, C68K_PC));
#if defined (HAVE_CYCLONE)
	cpu_setOPbase24((DWORD)m68000_get_reg(M68K_PC));
#elif defined (HAVE_C68K)
    cpu_setOPbase24((DWORD)C68k_Get_PC(&C68K));
#endif /* HAVE_C68K */
}

void 
cpu_setOPbase24(DWORD addr)
{

	switch ((addr >> 20) & 0xf) {
	case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
	case 8: case 9: case 0xa: case 0xb:
		OP_ROM = MEM;
		break;

	case 0xc: case 0xd:
		OP_ROM = GVRAM - 0x00c00000;
		break;

	case 0xe:
		if (addr < 0x00e80000) 
			OP_ROM = TVRAM - 0x00e00000;
		else if ((addr >= 0x00ea0000) && (addr < 0x00ea2000))
			OP_ROM = SCSIIPL - 0x00ea0000;
		else if ((addr >= 0x00ed0000) && (addr < 0x00ed4000))
			OP_ROM = SRAM - 0x00ed0000;
		else {
			BusErrFlag = 3;
			BusErrAdr = addr;
			Memory_ErrTrace();
			BusError(addr, 0);
		}
		break;

	case 0xf:
		if (addr >= 0x00fe0000)
			OP_ROM = IPL - 0x00fe0000;
		else {
			BusErrFlag = 3;
			BusErrAdr = addr;
			Memory_ErrTrace();
			BusError(addr, 0);
		}
		break;
	}
}

void 
Memory_SetSCSIMode(void)
{
	int i;

	for (i = 0xe0; i < 0xf0; i++) {
		MemReadTable[i] = rm_buserr;
	}
}

void Memory_ErrTrace(void)
{
#ifdef WIN68DEBUG
	FILE *fp;
	fp=fopen("_buserr.txt", "a");
	if (BusErrFlag==3)
		fprintf(fp, "BusErr - SetOP to $%08X  @ $%08X\n", BusErrAdr, regs.pc);
	else if (BusErrFlag==2)
		fprintf(fp, "BusErr - Write to $%08X  @ $%08X\n", BusErrAdr, regs.pc);
	else
		fprintf(fp, "BusErr - Read from $%08X  @ $%08X\n", BusErrAdr, regs.pc);
	fclose(fp);
//	traceflag ++;
//	m68000_ICount = 0;
#endif
}

void 
Memory_IntErr(int i)
{
#ifdef WIN68DEBUG
	FILE *fp;
	fp=fopen("_interr.txt", "a");
	fprintf(fp, "IntErr - Int.No%d  @ $%08X\n", i, regs.pc);
	fclose(fp);
#else
	(void)i;
#endif
}

void
AdrError(DWORD adr, DWORD unknown)
{

	(void)adr;
	(void)unknown;
	p6logd("AdrError: %x\n", adr);
	//	assert(0);
}

void
BusError(DWORD adr, DWORD unknown)
{

	(void)adr;
	(void)unknown;

	p6logd("BusError: %x\n", adr);
	BusErrHandling = 1;
	//assert(0);
}
