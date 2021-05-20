/* 
 * Copyright (c) 2003 NONAKA Kimihiro
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "common.h"

#include "winx68k.h"
#include "winui.h"

#include "bg.h"
#include "crtc.h"
#include "gvram.h"
#include "mouse.h"
#include "palette.h"
#include "prop.h"
#include "status.h"
#include "tvram.h"
#include "joystick.h"
#include "keyboard.h"

#include <locale.h>

#ifdef __LIBRETRO__
extern unsigned short *videoBuffer;
#endif
WORD menu_buffer[800*600];
#if 0
#include "../icons/keropi.xpm"
#endif

extern BYTE Debug_Text, Debug_Grp, Debug_Sp;


WORD *ScrBuf = 0;

int Draw_Opaque;
int FullScreenFlag = 0;
extern BYTE Draw_RedrawAllFlag;
BYTE Draw_DrawFlag = 1;
BYTE Draw_ClrMenu = 0;

BYTE Draw_BitMask[800];
BYTE Draw_TextBitMask[800];

int winx = 0, winy = 0;
DWORD winh = 0, winw = 0;
DWORD root_width, root_height;
WORD FrameCount = 0;
int SplashFlag = 0;

WORD WinDraw_Pal16B, WinDraw_Pal16R, WinDraw_Pal16G;

DWORD WindowX = 0;
DWORD WindowY = 0;

void WinDraw_InitWindowSize(WORD width, WORD height)
{
	static BOOL inited = FALSE;

	if (!inited) {
		inited = TRUE;
	}

	winw = width;
	winh = height;

	if (root_width < winw)
		winx = (root_width - winw) / 2;
	else if (winx < 0)
		winx = 0;
	else if ((winx + winw) > root_width)
		winx = root_width - winw;
	if (root_height < winh)
		winy = (root_height - winh) / 2;
	else if (winy < 0)
		winy = 0;
	else if ((winy + winh) > root_height)
		winy = root_height - winh;
}

void WinDraw_ChangeSize(void)
{
	DWORD oldx = WindowX, oldy = WindowY;
	int dif;

	Mouse_ChangePos();

	switch (Config.WinStrech) {
	case 0:
		WindowX = TextDotX;
		WindowY = TextDotY;
		break;

	case 1:
		WindowX = 768;
		WindowY = 512;
		break;

	case 2:
		if (TextDotX <= 384)
			WindowX = TextDotX * 2;
		else
			WindowX = TextDotX;
		if (TextDotY <= 256)
			WindowY = TextDotY * 2;
		else
			WindowY = TextDotY;
		break;

	case 3:
		if (TextDotX <= 384)
			WindowX = TextDotX * 2;
		else
			WindowX = TextDotX;
		if (TextDotY <= 256)
			WindowY = TextDotY * 2;
		else
			WindowY = TextDotY;
		dif = WindowX - WindowY;
		if ((dif > -32) && (dif < 32)) {
			// 正方形に近い画面なら、としておこう
			WindowX = (int)(WindowX * 1.25);
		}
		break;
	}

	if ((WindowX > 768) || (WindowX <= 0)) {
		if (oldx)
			WindowX = oldx;
		else
			WindowX = oldx = 768;
	}
	if ((WindowY > 512) || (WindowY <= 0)) {
		if (oldy)
			WindowY = oldy;
		else
			WindowY = oldy = 512;
	}

	if ((oldx == WindowX) && (oldy == WindowY))
		return;

	WinDraw_InitWindowSize((WORD)WindowX, (WORD)WindowY);
	StatBar_Show(Config.WindowFDDStat);
	Mouse_ChangePos();
}

//static int dispflag = 0;
void WinDraw_StartupScreen(void)
{
}

void WinDraw_CleanupScreen(void)
{
}

void WinDraw_ChangeMode(int flag)
{

	/* full screen mode(TRUE) <-> window mode(FALSE) */
	(void)flag;
}

void WinDraw_ShowSplash(void)
{
}

void WinDraw_HideSplash(void)
{
}


static void draw_kbd_to_tex(void);

int WinDraw_Init(void)
{
	int i, j;

	WindowX = 768;
	WindowY = 512;

	WinDraw_Pal16R = 0xf800;
	WinDraw_Pal16G = 0x07e0;
	WinDraw_Pal16B = 0x001f;

	p6logd("R: %x, G: %x, B: %x\n", WinDraw_Pal16R, WinDraw_Pal16G, WinDraw_Pal16B);

	ScrBuf = malloc(800 * 600 * 2);

	return TRUE;
}

void WinDraw_Cleanup( void )
{
}

void WinDraw_Redraw( void )
{

	TVRAM_SetAllDirty();
}


extern int retrow,retroh,CHANGEAV;

void FASTCALL WinDraw_Draw( void )
{

	static int oldtextx = -1, oldtexty = -1;

	if (oldtextx != TextDotX) {
		oldtextx = TextDotX;
		p6logd("TextDotX: %d\n", TextDotX);
		CHANGEAV=1;
	}
	if (oldtexty != TextDotY) {
		oldtexty = TextDotY;
		p6logd("TextDotY: %d\n", TextDotY);
		CHANGEAV=1;
	}

	if(CHANGEAV==1){
		retrow=TextDotX;
		retroh=TextDotY;
	}

	videoBuffer=(unsigned short int *)ScrBuf;

	FrameCount++;
	if (!Draw_DrawFlag/* && is_installed_idle_process()*/)
		return;
	Draw_DrawFlag = 0;

	if (SplashFlag)
		WinDraw_ShowSplash();
}

#define WD_MEMCPY(src) memcpy(&ScrBuf[adr], (src), TextDotX * 2)

#define WD_LOOP(start, end, sub)			\
{ 							\
	for (i = (start); i < (end); i++, adr++) {	\
		sub();					\
	}						\
}

#define WD_SUB(SUFFIX, src)			\
{						\
	w = (src);				\
	if (w != 0)				\
		ScrBuf##SUFFIX[adr] = w;	\
}


INLINE void WinDraw_DrawGrpLine(int opaq)
{
#define _DGL_SUB(SUFFIX) WD_SUB(SUFFIX, Grp_LineBuf[i])

	DWORD adr = VLINE*FULLSCREEN_WIDTH;
	WORD w;
	int i;

	if (opaq) {
		WD_MEMCPY(Grp_LineBuf);
	} else {
		WD_LOOP(0,  TextDotX, _DGL_SUB);
	}
}

INLINE void WinDraw_DrawGrpLineNonSP(int opaq)
{
#define _DGL_NSP_SUB(SUFFIX) WD_SUB(SUFFIX, Grp_LineBufSP2[i])

	DWORD adr = VLINE*FULLSCREEN_WIDTH;
	WORD w;
	int i;

	if (opaq) {
		WD_MEMCPY(Grp_LineBufSP2);
	} else {
		WD_LOOP(0,  TextDotX, _DGL_NSP_SUB);
	}
}

INLINE void WinDraw_DrawTextLine(int opaq, int td)
{
#define _DTL_SUB2(SUFFIX) WD_SUB(SUFFIX, BG_LineBuf[i])
#define _DTL_SUB(SUFFIX)		\
{					\
	if (Text_TrFlag[i] & 1) {	\
		_DTL_SUB2(SUFFIX);	\
	}				\
}	

	DWORD adr = VLINE*FULLSCREEN_WIDTH;
	WORD w;
	int i;

	if (opaq) {
		WD_MEMCPY(&BG_LineBuf[16]);
	} else {
		if (td) {
			WD_LOOP(16, TextDotX + 16, _DTL_SUB);
		} else {
			WD_LOOP(16, TextDotX + 16, _DTL_SUB2);
		}
	}
}

INLINE void WinDraw_DrawTextLineTR(int opaq)
{
#define _DTL_TR_SUB(SUFFIX)			   \
{						   \
	w = Grp_LineBufSP[i - 16];		   \
	if (w != 0) {				   \
		w &= Pal_HalfMask;		   \
		v = BG_LineBuf[i];		   \
		if (v & Ibit)			   \
			w += Pal_Ix2;		   \
		v &= Pal_HalfMask;		   \
		v += w;				   \
		v >>= 1;			   \
	} else {				   \
		if (Text_TrFlag[i] & 1)		   \
			v = BG_LineBuf[i];	   \
		else				   \
			v = 0;			   \
	}					   \
	ScrBuf##SUFFIX[adr] = (WORD)v;		   \
}

#define _DTL_TR_SUB2(SUFFIX)			   \
{						   \
	if (Text_TrFlag[i] & 1) {		   \
		w = Grp_LineBufSP[i - 16];	   \
		v = BG_LineBuf[i];		   \
						   \
		if (v != 0) {			   \
			if (w != 0) {			\
				w &= Pal_HalfMask;	\
				if (v & Ibit)		\
					w += Pal_Ix2;	\
				v &= Pal_HalfMask;	\
				v += w;			\
				v >>= 1;		\
			}				\
			ScrBuf##SUFFIX[adr] = (WORD)v;	\
		}					\
	}						\
}

	DWORD adr = VLINE*FULLSCREEN_WIDTH;
	DWORD v;
	WORD w;
	int i;

	if (opaq) {
		WD_LOOP(16, TextDotX + 16, _DTL_TR_SUB);
	} else {
		WD_LOOP(16, TextDotX + 16, _DTL_TR_SUB2);
	}
}

INLINE void WinDraw_DrawBGLine(int opaq, int td)
{
#define _DBL_SUB2(SUFFIX) WD_SUB(SUFFIX, BG_LineBuf[i])
#define _DBL_SUB(SUFFIX)			 \
{						 \
	if (Text_TrFlag[i] & 2) {		 \
		_DBL_SUB2(SUFFIX); \
	} \
}

	DWORD adr = VLINE*FULLSCREEN_WIDTH;
	WORD w;
	int i;

#if 0 // debug for segv
	static int log_start = 0;

	if (TextDotX == 128 && TextDotY == 128) {
		log_start = 1;
	}
	if (log_start) {
		printf("opaq/td: %d/%d VLINE: %d, TextDotX: %d\n", opaq, td, VLINE, TextDotX);
	}
#endif

	if (opaq) {
		WD_MEMCPY(&BG_LineBuf[16]);
	} else {
		if (td) {
			WD_LOOP(16, TextDotX + 16, _DBL_SUB);
		} else {
			WD_LOOP(16, TextDotX + 16, _DBL_SUB2);
		}
	}
}

INLINE void WinDraw_DrawBGLineTR(int opaq)
{

#define _DBL_TR_SUB3()			\
{					\
	if (w != 0) {			\
		w &= Pal_HalfMask;	\
		if (v & Ibit)		\
			w += Pal_Ix2;	\
		v &= Pal_HalfMask;	\
		v += w;			\
		v >>= 1;		\
	}				\
}

#define _DBL_TR_SUB(SUFFIX) \
{					\
	w = Grp_LineBufSP[i - 16];	\
	v = BG_LineBuf[i];		\
					\
	_DBL_TR_SUB3()			\
	ScrBuf##SUFFIX[adr] = (WORD)v;	\
}

#define _DBL_TR_SUB2(SUFFIX) \
{							\
	if (Text_TrFlag[i] & 2) {  			\
		w = Grp_LineBufSP[i - 16];		\
		v = BG_LineBuf[i];			\
							\
		if (v != 0) {				\
			_DBL_TR_SUB3()			\
			ScrBuf##SUFFIX[adr] = (WORD)v;	\
		}					\
	}						\
}

	DWORD adr = VLINE*FULLSCREEN_WIDTH;
	DWORD v;
	WORD w;
	int i;

	if (opaq) {
		WD_LOOP(16, TextDotX + 16, _DBL_TR_SUB);
	} else {
		WD_LOOP(16, TextDotX + 16, _DBL_TR_SUB2);
	}

}

INLINE void WinDraw_DrawPriLine(void)
{
#define _DPL_SUB(SUFFIX) WD_SUB(SUFFIX, Grp_LineBufSP[i])

	DWORD adr = VLINE*FULLSCREEN_WIDTH;
	WORD w;
	int i;

	WD_LOOP(0, TextDotX, _DPL_SUB);
}

#ifdef _WIN32
#define bzero(s,d) memset(s,0,d)
#endif
void WinDraw_DrawLine(void)
{
	int opaq, ton=0, gon=0, bgon=0, tron=0, pron=0, tdrawed=0;

if(VLINE==-1){
	p6logd("%d %d\n",VLINE,VLINE);
	return;
}
	if (!TextDirtyLine[VLINE]) return;
	TextDirtyLine[VLINE] = 0;
	Draw_DrawFlag = 1;


	if (Debug_Grp)
	{
	switch(VCReg0[1]&3)
	{
	case 0:					// 16 colors
		if (VCReg0[1]&4)		// 1024dot
		{
			if (VCReg2[1]&0x10)
			{
				if ( (VCReg2[0]&0x14)==0x14 )
				{
					Grp_DrawLine4hSP();
					pron = tron = 1;
				}
				else
				{
					Grp_DrawLine4h();
					gon=1;
				}
			}
		}
		else				// 512dot
		{
			if ( (VCReg2[0]&0x10)&&(VCReg2[1]&1) )
			{
				Grp_DrawLine4SP((VCReg1[1]   )&3/*, 1*/);			// 半透明の下準備
				pron = tron = 1;
			}
			opaq = 1;
			if (VCReg2[1]&8)
			{
				Grp_DrawLine4((VCReg1[1]>>6)&3, 1);
				opaq = 0;
				gon=1;
			}
			if (VCReg2[1]&4)
			{
				Grp_DrawLine4((VCReg1[1]>>4)&3, opaq);
				opaq = 0;
				gon=1;
			}
			if (VCReg2[1]&2)
			{
				if ( ((VCReg2[0]&0x1e)==0x1e)&&(tron) )
					Grp_DrawLine4TR((VCReg1[1]>>2)&3, opaq);
				else
					Grp_DrawLine4((VCReg1[1]>>2)&3, opaq);
				opaq = 0;
				gon=1;
			}
			if (VCReg2[1]&1)
			{
//				if ( (VCReg2[0]&0x1e)==0x1e )
//				{
//					Grp_DrawLine4SP((VCReg1[1]   )&3, opaq);
//					tron = pron = 1;
//				}
//				else
				if ( (VCReg2[0]&0x14)!=0x14 )
				{
					Grp_DrawLine4((VCReg1[1]   )&3, opaq);
					gon=1;
				}
			}
		}
		break;
	case 1:	
	case 2:	
		opaq = 1;		// 256 colors
		if ( (VCReg1[1]&3) <= ((VCReg1[1]>>4)&3) )	// 同じ値の時は、GRP0が優先（ドラスピ）
		{
			if ( (VCReg2[0]&0x10)&&(VCReg2[1]&1) )
			{
				Grp_DrawLine8SP(0);			// 半透明の下準備
				tron = pron = 1;
			}
			if (VCReg2[1]&4)
			{
				if ( ((VCReg2[0]&0x1e)==0x1e)&&(tron) )
					Grp_DrawLine8TR(1, 1);
				else if ( ((VCReg2[0]&0x1d)==0x1d)&&(tron) )
					Grp_DrawLine8TR_GT(1, 1);
				else
					Grp_DrawLine8(1, 1);
				opaq = 0;
				gon=1;
			}
			if (VCReg2[1]&1)
			{
				if ( (VCReg2[0]&0x14)!=0x14 )
				{
					Grp_DrawLine8(0, opaq);
					gon=1;
				}
			}
		}
		else
		{
			if ( (VCReg2[0]&0x10)&&(VCReg2[1]&1) )
			{
				Grp_DrawLine8SP(1);			// 半透明の下準備
				tron = pron = 1;
			}
			if (VCReg2[1]&4)
			{
				if ( ((VCReg2[0]&0x1e)==0x1e)&&(tron) )
					Grp_DrawLine8TR(0, 1);
				else if ( ((VCReg2[0]&0x1d)==0x1d)&&(tron) )
					Grp_DrawLine8TR_GT(0, 1);
				else
					Grp_DrawLine8(0, 1);
				opaq = 0;
				gon=1;
			}
			if (VCReg2[1]&1)
			{
				if ( (VCReg2[0]&0x14)!=0x14 )
				{
					Grp_DrawLine8(1, opaq);
					gon=1;
				}
			}
		}
		break;
	case 3:					// 65536 colors
		if (VCReg2[1]&15)
		{
			if ( (VCReg2[0]&0x14)==0x14 )
			{
				Grp_DrawLine16SP();
				tron = pron = 1;
			}
			else
			{
				Grp_DrawLine16();
				gon=1;
			}
		}
		break;
	}
	}


//	if ( ( ((VCReg1[0]&0x30)>>4) < (VCReg1[0]&0x03) ) && (gon) )
//		gdrawed = 1;				// GrpよりBGの方が上

	if ( ((VCReg1[0]&0x30)>>2) < (VCReg1[0]&0x0c) )
	{						// BGの方が上
		if ((VCReg2[1]&0x20)&&(Debug_Text))
		{
			Text_DrawLine(1);
			ton = 1;
		}
		else
			ZeroMemory(Text_TrFlag, TextDotX+16);

		if ((VCReg2[1]&0x40)&&(BG_Regs[8]&2)&&(!(BG_Regs[0x11]&2))&&(Debug_Sp))
		{
			int s1, s2;
			s1 = (((BG_Regs[0x11]  &4)?2:1)-((BG_Regs[0x11]  &16)?1:0));
			s2 = (((CRTC_Regs[0x29]&4)?2:1)-((CRTC_Regs[0x29]&16)?1:0));
			VLINEBG = VLINE;
			VLINEBG <<= s1;
			VLINEBG >>= s2;
			if ( !(BG_Regs[0x11]&16) ) VLINEBG -= ((BG_Regs[0x0f]>>s1)-(CRTC_Regs[0x0d]>>s2));
			BG_DrawLine(!ton, 0);
			bgon = 1;
		}
	}
	else
	{						// Textの方が上
		if ((VCReg2[1]&0x40)&&(BG_Regs[8]&2)&&(!(BG_Regs[0x11]&2))&&(Debug_Sp))
		{
			int s1, s2;
			s1 = (((BG_Regs[0x11]  &4)?2:1)-((BG_Regs[0x11]  &16)?1:0));
			s2 = (((CRTC_Regs[0x29]&4)?2:1)-((CRTC_Regs[0x29]&16)?1:0));
			VLINEBG = VLINE;
			VLINEBG <<= s1;
			VLINEBG >>= s2;
			if ( !(BG_Regs[0x11]&16) ) VLINEBG -= ((BG_Regs[0x0f]>>s1)-(CRTC_Regs[0x0d]>>s2));
			ZeroMemory(Text_TrFlag, TextDotX+16);
			BG_DrawLine(1, 1);
			bgon = 1;
		}
		else
		{
			if ((VCReg2[1]&0x20)&&(Debug_Text))
			{
				int i;
				for (i = 16; i < TextDotX + 16; ++i)
					BG_LineBuf[i] = TextPal[0];
			} else {		// 20010120 （琥珀色）
				bzero(&BG_LineBuf[16], TextDotX * 2);
			}
			ZeroMemory(Text_TrFlag, TextDotX+16);
			bgon = 1;
		}

		if ((VCReg2[1]&0x20)&&(Debug_Text))
		{
			Text_DrawLine(!bgon);
			ton = 1;
		}
	}


	opaq = 1;


#if 0
					// Pri = 3（違反）に設定されている画面を表示
		if ( ((VCReg1[0]&0x30)==0x30)&&(bgon) )
		{
			if ( ((VCReg2[0]&0x5d)==0x1d)&&((VCReg1[0]&0x03)!=0x03)&&(tron) )
			{
				if ( (VCReg1[0]&3)<((VCReg1[0]>>2)&3) )
				{
					WinDraw_DrawBGLineTR(opaq);
					tdrawed = 1;
					opaq = 0;
				}
			}
			else
			{
				WinDraw_DrawBGLine(opaq, /*tdrawed*/0);
				tdrawed = 1;
				opaq = 0;
			}
		}
		if ( ((VCReg1[0]&0x0c)==0x0c)&&(ton) )
		{
			if ( ((VCReg2[0]&0x5d)==0x1d)&&((VCReg1[0]&0x03)!=0x0c)&&(tron) )
				WinDraw_DrawTextLineTR(opaq);
			else
				WinDraw_DrawTextLine(opaq, /*tdrawed*/((VCReg1[0]&0x30)==0x30));
			opaq = 0;
			tdrawed = 1;
		}
#endif
					// Pri = 2 or 3（最下位）に設定されている画面を表示
					// プライオリティが同じ場合は、GRP<SP<TEXT？（ドラスピ、桃伝、YsIII等）

					// GrpよりTextが上にある場合にTextとの半透明を行うと、SPのプライオリティも
					// Textに引きずられる？（つまり、Grpより下にあってもSPが表示される？）

					// KnightArmsとかを見ると、半透明のベースプレーンは一番上になるみたい…。

		if ( (VCReg1[0]&0x02) )
		{
			if (gon)
			{
				WinDraw_DrawGrpLine(opaq);
				opaq = 0;
			}
			if (tron)
			{
				WinDraw_DrawGrpLineNonSP(opaq);
				opaq = 0;
			}
		}
		if ( (VCReg1[0]&0x20)&&(bgon) )
		{
			if ( ((VCReg2[0]&0x5d)==0x1d)&&((VCReg1[0]&0x03)!=0x02)&&(tron) )
			{
				if ( (VCReg1[0]&3)<((VCReg1[0]>>2)&3) )
				{
					WinDraw_DrawBGLineTR(opaq);
					tdrawed = 1;
					opaq = 0;
				}
			}
			else
			{
				WinDraw_DrawBGLine(opaq, /*0*/tdrawed);
				tdrawed = 1;
				opaq = 0;
			}
		}
		if ( (VCReg1[0]&0x08)&&(ton) )
		{
			if ( ((VCReg2[0]&0x5d)==0x1d)&&((VCReg1[0]&0x03)!=0x02)&&(tron) )
				WinDraw_DrawTextLineTR(opaq);
			else
				WinDraw_DrawTextLine(opaq, tdrawed/*((VCReg1[0]&0x30)>=0x20)*/);
			opaq = 0;
			tdrawed = 1;
		}

					// Pri = 1（2番目）に設定されている画面を表示
		if ( ((VCReg1[0]&0x03)==0x01)&&(gon) )
		{
			WinDraw_DrawGrpLine(opaq);
			opaq = 0;
		}
		if ( ((VCReg1[0]&0x30)==0x10)&&(bgon) )
		{
			if ( ((VCReg2[0]&0x5d)==0x1d)&&(!(VCReg1[0]&0x03))&&(tron) )
			{
				if ( (VCReg1[0]&3)<((VCReg1[0]>>2)&3) )
				{
					WinDraw_DrawBGLineTR(opaq);
					tdrawed = 1;
					opaq = 0;
				}
			}
			else
			{
				WinDraw_DrawBGLine(opaq, ((VCReg1[0]&0xc)==0x8));
				tdrawed = 1;
				opaq = 0;
			}
		}
		if ( ((VCReg1[0]&0x0c)==0x04) && ((VCReg2[0]&0x5d)==0x1d) && (VCReg1[0]&0x03) && (((VCReg1[0]>>4)&3)>(VCReg1[0]&3)) && (bgon) && (tron) )
		{
			WinDraw_DrawBGLineTR(opaq);
			tdrawed = 1;
			opaq = 0;
			if (tron)
			{
				WinDraw_DrawGrpLineNonSP(opaq);
			}
		}
		else if ( ((VCReg1[0]&0x03)==0x01)&&(tron)&&(gon)&&(VCReg2[0]&0x10) )
		{
			WinDraw_DrawGrpLineNonSP(opaq);
			opaq = 0;
		}
		if ( ((VCReg1[0]&0x0c)==0x04)&&(ton) )
		{
			if ( ((VCReg2[0]&0x5d)==0x1d)&&(!(VCReg1[0]&0x03))&&(tron) )
				WinDraw_DrawTextLineTR(opaq);
			else
				WinDraw_DrawTextLine(opaq, ((VCReg1[0]&0x30)>=0x10));
			opaq = 0;
			tdrawed = 1;
		}

					// Pri = 0（最優先）に設定されている画面を表示
		if ( (!(VCReg1[0]&0x03))&&(gon) )
		{
			WinDraw_DrawGrpLine(opaq);
			opaq = 0;
		}
		if ( (!(VCReg1[0]&0x30))&&(bgon) )
		{
			WinDraw_DrawBGLine(opaq, /*tdrawed*/((VCReg1[0]&0xc)>=0x4));
			tdrawed = 1;
			opaq = 0;
		}
		if ( (!(VCReg1[0]&0x0c)) && ((VCReg2[0]&0x5d)==0x1d) && (((VCReg1[0]>>4)&3)>(VCReg1[0]&3)) && (bgon) && (tron) )
		{
			WinDraw_DrawBGLineTR(opaq);
			tdrawed = 1;
			opaq = 0;
			if (tron)
			{
				WinDraw_DrawGrpLineNonSP(opaq);
			}
		}
		else if ( (!(VCReg1[0]&0x03))&&(tron)&&(VCReg2[0]&0x10) )
		{
			WinDraw_DrawGrpLineNonSP(opaq);
			opaq = 0;
		}
		if ( (!(VCReg1[0]&0x0c))&&(ton) )
		{
			WinDraw_DrawTextLine(opaq, 1);
			tdrawed = 1;
			opaq = 0;
		}

					// 特殊プライオリティ時のグラフィック
		if ( ((VCReg2[0]&0x5c)==0x14)&&(pron) )	// 特殊Pri時は、対象プレーンビットは意味が無いらしい（ついんびー）
		{
			WinDraw_DrawPriLine();
		}
		else if ( ((VCReg2[0]&0x5d)==0x1c)&&(tron) )	// 半透明時に全てが透明なドットをハーフカラーで埋める
		{						// （AQUALES）

#define _DL_SUB(SUFFIX) \
{								\
	w = Grp_LineBufSP[i];					\
	if (w != 0 && (ScrBuf##SUFFIX[adr] & 0xffff) == 0)	\
		ScrBuf##SUFFIX[adr] = (w & Pal_HalfMask) >> 1;	\
}

			DWORD adr = VLINE*FULLSCREEN_WIDTH;
			WORD w;
			int i;

			WD_LOOP(0, TextDotX, _DL_SUB);
		}


	if (opaq)
	{
		DWORD adr = VLINE*FULLSCREEN_WIDTH;
#ifdef PSP
		if (TextDotX > 512) {
			bzero(&ScrBufL[adr], TextDotX * 2);
			adr = VLINE * 256;
			bzero(&ScrBufR[adr], (TextDotX - 512) * 2);
		} else {
			bzero(&ScrBufL[adr], TextDotX * 2);
		}
#else
		bzero(&ScrBuf[adr], TextDotX * 2);
#endif
	}
}

/********** menu 関連ルーチン **********/

struct _px68k_menu
{
	WORD	*surfaceBuffPtr; 	// surface buffer ptr
	WORD	*menuLocationPtr;	// menu location ptr
	WORD	fgColor;			// foreground color
	WORD	bgColor;			// background colo
	int		menuPosX;			// menuのカーソル位置X
	int		menuPosY;			// menuのカーソル位置Y
	int		menuFontSize;		// menu font size;
} px68kMenu;

//-----------------------------------------
// utf-8→sjisコード変換
//
// UTF-8のコード ビット列 内容 
// 0xxx xxxx  １バイトコード 
// 10xx xxxx  ２バイトコード、３バイトコードの２、３文字目  
// 110x xxxx  ２バイトコードの先頭バイト  
// 1110 xxxx  ３バイトコードの先頭バイト
//-----------------------------------------
extern unsigned char lowSjisTbl[], highSjisTbl[];
static void utf8ToSjis( unsigned char *src, unsigned char *dst )
{
	int		c;
	wchar_t	wc;
	
	while( c = *src++ )
	{
		if( c < 0x80 )
		{
			// 1バイトコード

			*dst++ = c;
		}
		else if( c < 0xc0 )
		{
			// UTF-8の文字列ではない？？

			continue;
		}
		else
		{
			if( c < 0xe0 )
			{
				// 2バイトコードの先頭

				wc = (c & 0x1f) << 6;

				if( c = *src++ )
				{
					wc |= (c & 0x3f);
				}
				else
				{
					break;
				}
			}
			else if( c < 0xf0 )
			{
				// 3バイトコードの先頭

				wc = (c & 0x0f) << 12;

				if( c = *src++ )
				{
					wc |= (c & 0x3f) << 6;

					if( c = *src++ )
					{
						wc |= (c & 0x3f);
					}
					else
					{
						break;
					}
				}
				else
				{
					break;
				}
			}

			*dst++ = lowSjisTbl[ wc ];
			*dst++ = highSjisTbl[ wc ];
		}
	}

	*dst = '\0';

	return;
}

//-----------------------------------------
// sjis→jisコード変換
//-----------------------------------------
static WORD sjis2jis( WORD _sjisCode )
{
	BYTE	high, low;

	high = _sjisCode >> 8;
	low  = _sjisCode & 0xff;

	high <<= 1;

	if( low < 0x9f)
	{
		high += ( high < 0x3f ) ? 0x1f : -0x61;
		low  -= ( low  > 0x7e ) ? 0x20 : 0x1f;
	}
	else
	{
		high += ( high < 0x3f ) ? 0x20 : -0x60;
		low  -= 0x7e;
	}

	return( ( high << 8 ) | low );
}


//-----------------------------------------
// JISコードから0 originのindexに変換する
// ただし0x2921-0x2f7eはX68KのROM上にないので飛ばす
//-----------------------------------------
static WORD jis2idx( WORD _jisCode )
{
	if( _jisCode >= 0x3000 )
	{
		_jisCode -= 0x3021;
	}
	else
	{
		_jisCode -= 0x2121;
	}
	
	_jisCode = ( _jisCode & 0xff ) + ( _jisCode >> 8 ) * 0x5e;

	return( _jisCode );
}

#define isHankaku(s) (((s) >= 0x20 && (s) <= 0x7e) || ((s) >= 0xa0 && (s) <= 0xdf))
#define MENU_WIDTH 800


// _fontSize : 8 or 16 or 24
// 半角文字の場合は16bitの上位8bitにデータを入れておくこと
// (半角or全角の判断ができるように)
static DWORD get_font_addr( WORD _sjisCode, int _fontSize )
{
	WORD	jisCode, jisIdx;
	BYTE	jisHigh;
	int		fontDataSize;

	if( isHankaku( _sjisCode >> 8 ) )
	{
		// 半角文字

		switch( _fontSize )
		{
			case 8 :

				return( 0x3a000 + ( _sjisCode >> 8 ) * ( 1 * 8 ) );

			case 16 :

				return( 0x3a800 + ( _sjisCode >> 8 ) * ( 1 * 16 ) );

			case 24 :

				return( 0x3d000 + ( _sjisCode >> 8 ) * ( 2 * 24 ) );

			default:

				return -1;
		}
	}
	else
	{
		// 全角文字

		switch( _fontSize )
		{
			case 16 :
			
				fontDataSize = 2 * 16;

				break;

			case 24 :

				fontDataSize = 3 * 24;

				break;

			default :

				// ここに来たらアカン

				return( -1 );
		}

		jisCode = sjis2jis( _sjisCode );
		jisIdx  = (DWORD)jis2idx( jisCode );
		jisHigh = (BYTE)( jisCode >> 8 );

	#if 0
		printf( "sjis code = 0x%x\n", _sjisCode );
		printf( "jis code = 0x%x\n", jisCode );
		printf( "jis High 0x%x jis Index 0x%x\n", jisHigh, jisIdx );
	#endif

		if( jisHigh >= 0x21 && jisHigh <= 0x28 )
		{
			// 非漢字

			return( ( ( _fontSize == 16 ) ? 0x0 : 0x40000 ) + jisIdx * fontDataSize );
		}
		else if( jisHigh >= 0x30 && jisHigh <= 0x74 )
		{
			// 漢字

			return( ( ( _fontSize == 16 ) ? 0x5e00 : 0x4d380 ) + jisIdx * fontDataSize );
		}
		else
		{
			// ここにくることはないはず

			return -1;
		}
	}
}

// RGB565
static void set_fgcolor( WORD _c )
{
	px68kMenu.fgColor = _c;
}

// mbcolor = 0 なら透明色とする
static void set_bgcolor( WORD _c )
{
	px68kMenu.bgColor = _c;
}

// グラフィック座標
static void set_mlocate( int _x, int _y )
{
	px68kMenu.menuPosX = _x;
	px68kMenu.menuPosY = _y;
}

// キャラクタ文字の座標 (横軸は1座標が半角文字幅になる)
static void set_mlocateChara( int _x, int _y )
{
	px68kMenu.menuPosX = _x * px68kMenu.menuFontSize / 2;
	px68kMenu.menuPosY = _y * px68kMenu.menuFontSize;
}

static void set_surfaceBuffPtr( WORD *_p )
{
	px68kMenu.surfaceBuffPtr = _p;
}

// menu font size (16 or 24)
static void set_menuFontSize( int _fs )
{
	px68kMenu.menuFontSize = _fs;
}

static WORD *get_menuLocationPtr( void )
{
	px68kMenu.menuLocationPtr = px68kMenu.surfaceBuffPtr + MENU_WIDTH * px68kMenu.menuPosY + px68kMenu.menuPosX;

	return( px68kMenu.menuLocationPtr );
}

//--------------------------------------------------------------------
// ・半角文字の場合は16bitの上位8bitにデータを入れておくこと
//   (半角or全角の判断ができるように)
// ・表示した分cursorは先に移動する
static void draw_char( WORD _sjisCode )
{
	DWORD	fontAddr;
	WORD	*writePtr;
	int		i, j, k, wc, w;
	BYTE	c;
	WORD	word;

	int		h = px68kMenu.menuFontSize;		// h = フォントサイズ = フォントの高さ？

	writePtr = get_menuLocationPtr();

	fontAddr = get_font_addr( _sjisCode, h );

	if( fontAddr < 0 )
	{
		// 対応してない文字コードだった
		
		return;
	}

	// h ...  8 ⇒ 半角 8 x 8
	// h ... 16 ⇒ 半角 8 x 16, 全角 16 x 16
	// h ... 24 ⇒ 半角 12 x 24, 全角 24 x 24

	w = ( h == 8 ) ? 8 : ( isHankaku( _sjisCode >> 8 ) ? h / 2 : h );

	switch( h )
	{
		default :
		case 8 :	// 半角 8 x 8 のみ
		{
			if( px68kMenu.bgColor )
			{
				// BGカラーが非透過色

				for( i = 0; i < h; i++ )
				{
					c = FONT[ fontAddr++ ];

					*writePtr = ( c & 0x80 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
					*writePtr = ( c & 0x40 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
					*writePtr = ( c & 0x20 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
					*writePtr = ( c & 0x10 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
					*writePtr = ( c & 0x08 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
					*writePtr = ( c & 0x04 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
					*writePtr = ( c & 0x02 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
					*writePtr = ( c & 0x01 ) ? px68kMenu.fgColor : px68kMenu.bgColor;

					writePtr += MENU_WIDTH - 7;
				}
			}
			else
			{
				// BGカラーが透明色なんで背景透過する

				for( i = 0; i < h; i++ )
				{
					c = FONT[ fontAddr++ ];

					if( c & 0x80 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
					if( c & 0x40 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
					if( c & 0x20 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
					if( c & 0x10 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
					if( c & 0x08 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
					if( c & 0x04 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
					if( c & 0x02 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
					if( c & 0x01 ) { *writePtr = px68kMenu.fgColor; }

					writePtr += MENU_WIDTH - 7;

				}
			}

			break;
		}
		case 16 :	// 半角 8 x 16, 全角 16 x 16
		{
			if( isHankaku( _sjisCode >> 8 ) )
			{
				// 半角

				if( px68kMenu.bgColor )
				{
					// BGカラーが非透過色

					for( i = 0; i < h; i++ )
					{
						c = FONT[ fontAddr++ ];

						*writePtr = ( c & 0x80 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( c & 0x40 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( c & 0x20 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( c & 0x10 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( c & 0x08 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( c & 0x04 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( c & 0x02 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( c & 0x01 ) ? px68kMenu.fgColor : px68kMenu.bgColor;

						writePtr += MENU_WIDTH - 7;
					}
				}
				else
				{
					// BGカラーが透明色なんで背景透過する

					for( i = 0; i < h; i++ )
					{
						c = FONT[ fontAddr++ ];

						if( c & 0x80 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( c & 0x40 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( c & 0x20 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( c & 0x10 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( c & 0x08 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( c & 0x04 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( c & 0x02 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( c & 0x01 ) { *writePtr = px68kMenu.fgColor; }

						writePtr += MENU_WIDTH - 7;

					}
				}
			}
			else
			{
				// 全角

				if( px68kMenu.bgColor )
				{
					// BGカラーが非透過色

					for( i = 0; i < h; i++ )
					{
						word = *((WORD *)(FONT + fontAddr)); fontAddr += 2;

						*writePtr = ( word & 0x0080 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0040 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0020 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0010 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0008 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0004 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0002 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0001 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x8000 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x4000 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x2000 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x1000 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0800 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0400 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0200 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0100 ) ? px68kMenu.fgColor : px68kMenu.bgColor;

						writePtr += MENU_WIDTH - 15;
					}
				}
				else
				{
					// BGカラーが透明色なんで背景透過する

					for( i = 0; i < h; i++ )
					{
						word = *((WORD *)(FONT + fontAddr)); fontAddr += 2;

						if( word & 0x0080 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0040 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0020 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0010 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0008 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0004 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0002 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0001 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x8000 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x4000 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x2000 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x1000 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0800 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0400 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0200 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0100 ) { *writePtr = px68kMenu.fgColor; }

						writePtr += MENU_WIDTH - 15;
					}
				}
			}

			break;
		}
		case 24 :	// 半角 12 x 24, 全角 24 x 24
		{
			if( isHankaku( _sjisCode >> 8 ) )
			{
				// 半角

				if( px68kMenu.bgColor )
				{
					// BGカラーが非透過色

					for( i = 0; i < h; i++ )
					{
						word = *((WORD *)(FONT + fontAddr)); fontAddr += 2;

						*writePtr = ( word & 0x0080 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0040 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0020 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0010 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0008 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0004 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0002 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0001 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x8000 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x4000 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x2000 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x1000 ) ? px68kMenu.fgColor : px68kMenu.bgColor;

						writePtr += MENU_WIDTH - 11;
					}
				}
				else
				{
					// BGカラーが透明色なんで背景透過する

					for( i = 0; i < h; i++ )
					{
						word = *((WORD *)(FONT + fontAddr)); fontAddr += 2;

						if( word & 0x0080 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0040 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0020 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0010 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0008 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0004 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0002 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0001 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x8000 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x4000 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x2000 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x1000 ) { *writePtr = px68kMenu.fgColor; }

						writePtr += MENU_WIDTH - 11;

					}
				}
			}
			else
			{
				// 全角

				if( px68kMenu.bgColor )
				{
					// BGカラーが非透過色

					for( i = 0; i < h / 2; i++ )
					{
						// 偶数ライン

						word = *((WORD *)(FONT + fontAddr)); fontAddr += 2;

						*writePtr = ( word & 0x0080 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0040 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0020 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0010 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0008 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0004 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0002 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0001 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x8000 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x4000 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x2000 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x1000 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0800 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0400 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0200 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0100 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;

						word = *((WORD *)(FONT + fontAddr)); fontAddr += 2;

						*writePtr = ( word & 0x0080 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0040 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0020 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0010 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0008 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0004 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0002 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0001 ) ? px68kMenu.fgColor : px68kMenu.bgColor;

						writePtr += MENU_WIDTH - 23;

						// 奇数ライン

						*writePtr = ( word & 0x8000 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x4000 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x2000 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x1000 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0800 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0400 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0200 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0100 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;

						word = *((WORD *)(FONT + fontAddr)); fontAddr += 2;

						*writePtr = ( word & 0x0080 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0040 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0020 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0010 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0008 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0004 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0002 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0001 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x8000 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x4000 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x2000 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x1000 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0800 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0400 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0200 ) ? px68kMenu.fgColor : px68kMenu.bgColor; writePtr++;
						*writePtr = ( word & 0x0100 ) ? px68kMenu.fgColor : px68kMenu.bgColor;

						writePtr += MENU_WIDTH - 23;
					}
				}
				else
				{
					// BGカラーが透明色なんで背景透過する

					for( i = 0; i < h / 2; i++ )
					{
						// 偶数ライン

						word = *((WORD *)(FONT + fontAddr)); fontAddr += 2;

						if( word & 0x0080 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0040 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0020 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0010 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0008 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0004 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0002 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0001 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x8000 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x4000 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x2000 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x1000 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0800 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0400 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0200 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0100 ) { *writePtr = px68kMenu.fgColor; } writePtr++;

						word = *((WORD *)(FONT + fontAddr)); fontAddr += 2;

						if( word & 0x0080 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0040 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0020 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0010 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0008 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0004 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0002 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0001 ) { *writePtr = px68kMenu.fgColor; }

						writePtr += MENU_WIDTH - 23;

						// 奇数ライン

						if( word & 0x8000 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x4000 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x2000 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x1000 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0800 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0400 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0200 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0100 ) { *writePtr = px68kMenu.fgColor; } writePtr++;

						word = *((WORD *)(FONT + fontAddr)); fontAddr += 2;

						if( word & 0x0080 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0040 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0020 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0010 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0008 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0004 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0002 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0001 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x8000 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x4000 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x2000 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x1000 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0800 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0400 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0200 ) { *writePtr = px68kMenu.fgColor; } writePtr++;
						if( word & 0x0100 ) { *writePtr = px68kMenu.fgColor; }

						writePtr += MENU_WIDTH - 23;
					}
				}
			}

			break;
		}
	}

	px68kMenu.menuPosX += w;
}

//------------------------------
// 文字列の表示ルーチン
//------------------------------
static void draw_str( char *cp )
{
	int		i, len;
	BYTE	*s;
	WORD	wc;

	len = strlen( cp );
	s = (BYTE *)cp;

	for( i = 0; i < len; i++ )
	{
		if( isHankaku( *s ) )
		{
			// 最初の8bitで半全角を判断するので半角の場合は
			// あらかじめ8bit左シフトしておく

			draw_char( (WORD)*s << 8 );
			s++;
		}
		else
		{
			wc = (WORD)(*s << 8) + *(s + 1);
			draw_char( wc );
			s += 2;
			i++;
		}
		// 8x8描画(ソフトキーボードのFUNCキーは文字幅を縮める)
		if( px68kMenu.menuFontSize == 8 )
		{
			px68kMenu.menuPosX -= 3;
		}
	}
}

int WinDraw_MenuInit( void )
{
	set_surfaceBuffPtr( menu_buffer );
	set_menuFontSize( 24 );

	set_fgcolor( 0xffff );
	set_bgcolor( 0x0000 );

	return TRUE;
}

#include "menu_str_sjis.txt"
char menu_item_desc[][60] = {
	"Reset / NMI reset / Quit",
	"Change / Eject floppy 0",
	"Change / Eject floppy 1",
	"Change / Eject HDD 0",
	"Change / Eject HDD 1"
};

//==========================================
//
// [F12]システムメニュー画面の描画
//
//==========================================
void WinDraw_DrawMenu( int menu_state, int mkey_pos, int mkey_y, int *mval_y )
{
	int		i, drv;
	char	tmp[ 256 ];
	char	sjis[ 256 ];

// ソフトウェアキーボード描画時にset_surfaceBuffPtr(kbd_buffer)されているので戻す

	set_surfaceBuffPtr( menu_buffer );
	set_menuFontSize( Config.MenuFontSize ? 24 : 16 );

	// タイトル

	set_fgcolor( 0x07ff );		// cyan

	set_mlocateChara( 0, 0 );
	draw_str( twaku_str );

	set_mlocateChara( 0, 1 );
	draw_str( twaku2_str );

	set_mlocateChara( 0, 2 );
	draw_str( twaku3_str );

	set_fgcolor( 0xffff );		// white

	set_mlocateChara( 2, 1 );
	sprintf( tmp, "%s%s", title_str, PX68KVERSTR );
	draw_str( tmp );

	set_fgcolor( 0xffe0 );		// yellow

	set_mlocateChara( 1, 4 );
	draw_str( waku_str );

	for( i = 5; i < 10; i++ )
	{
		set_mlocateChara( 1, i );
		draw_str( waku2_str );
	}

	set_mlocateChara( 1, 10 );
	draw_str( waku3_str );

	// アイテム/キーワード

	set_fgcolor(0xffff);

	for( i = 0; i < 5; i++ ) {

		set_mlocateChara( 3, 5 + i );

		if( menu_state == ms_key && i == (mkey_y - mkey_pos) )
		{
			set_fgcolor( 0x0000 );		// black
			set_bgcolor( 0xffe0 );		// yellow;
		}
		else
		{
			set_fgcolor( 0xffff );		// white
			set_bgcolor( 0x0000 );		// black
		}
		draw_str( menu_item_key[ i + mkey_pos ] );
	}

	// アイテム/現在値

	set_fgcolor( 0xffff );			// white
	set_bgcolor( 0x0000 );			// black

	for( i = 0; i < 5; i++ )
	{
		if( (menu_state == ms_value || menu_state == ms_hwjoy_set) && i == (mkey_y - mkey_pos) )
		{
			set_fgcolor( 0x0000 );		// black
			set_bgcolor( 0xffe0 );		// yellow
		}
		else
		{
			set_fgcolor( 0xffff );		// white
			set_bgcolor( 0x0000 );		// black
		}

		set_mlocateChara( 17, 5 + i );

		drv = WinUI_get_drv_num( i + mkey_pos );

		if( drv >= 0  && mval_y[ i + mkey_pos ] == 0 )
		{
			char *p;

			if( drv < 2 )
			{
				p = Config.FDDImage[ drv ];		// FDD
			}
			else
			{
				p = Config.HDImage[ drv - 2 ];	// HDD
			}

			if( p[0] == '\0' )
			{
				draw_str( " -- no disk --" );
			}
			else
			{
				// 先頭のカレントディレクトリ名を表示しない

				char ptr[ PATH_MAX ];

				if( !strncmp( cur_dir_str, p, cur_dir_slen ) )
				{
					strncpy( ptr, p + cur_dir_slen, sizeof( ptr ) );
				}
				else
				{
					strncpy( ptr, p, sizeof( ptr ) );
				}
				ptr[ 40 ] = '\0';

				utf8ToSjis( ptr, sjis );	// ファイル名をutf-8からsjisに変換しておく
				draw_str( sjis );
			}
		}
		else
		{
			draw_str( menu_items[ i + mkey_pos ][ mval_y[ i + mkey_pos ] ] );
		}
	}

	set_fgcolor( 0x07ff );		// cyan
	set_bgcolor( 0x0000 );		// black

	set_mlocateChara( 0, 11 );
	draw_str( swaku_str );

	set_mlocateChara( 0, 12 );
	draw_str( swaku2_str );

	set_mlocateChara( 0, 13 );
	draw_str( swaku3_str );

	// キャプション

	set_fgcolor( 0xffff );		// white
	set_bgcolor( 0x0000 );		// black

	set_mlocateChara( 2, 12 );
	draw_str( (char*)(menu_item_desc[ mkey_y ]) );

	videoBuffer = (unsigned short int *)menu_buffer;
}

//==========================================
//
// ファイル選択画面の描画
//
//==========================================
void WinDraw_DrawMenufile( struct menu_flist *fileList )
{
	int i;
	char sjis[ PATH_MAX ];

	// 下枠
	//set_fgcolor(0xf800); // red
	//set_fgcolor(0xf81f); // magenta

	// 枠の描画

	set_fgcolor( 0xffff );
	set_bgcolor( 0x0001 );	// black 0x0だと透過するので
	set_mlocateChara( 1, 1 );

	draw_str( swaku_str );

	for( i = 2; i < 16; i++ )
	{
		set_mlocateChara( 1, i );
		draw_str( swaku2_str );
	}

	set_mlocateChara( 1, 16 );
	draw_str( swaku3_str );

	// 中身の描画

	for( i = 0; i < 14; i++ )
	{
		if( i + 1 > fileList->num )
		{
			// 保持してるファイル数を超えたら終了

			break;
		}

		if( i == fileList->y )
		{
			// カーソル位置は反転表示

			set_fgcolor( 0x0001 );	// 文字色black。元は0x0だったけどこっちの方が良さそう
			set_bgcolor( 0xffff );	// 背景色white
		}
		else
		{
			set_fgcolor( 0xffff );	// 文字色white
			set_bgcolor( 0x0001 );	// 背景色black
		}

		// ディレクトリだったらファイル名を[]で囲う

		set_mlocateChara( 3, i + 2 );

		utf8ToSjis( fileList->name[ i + fileList->ptr ], sjis );	// ファイル名をutf-8からsjisに変換しておく

		if( fileList->type[ i + fileList->ptr ] )
		{
			draw_str( "[" );
			draw_str( sjis );
			draw_str( "]" );
		}
		else
		{
			draw_str( sjis );
		}
	}

	set_bgcolor( 0x0000 );		// 透過モードに戻しておく

	videoBuffer = (unsigned short int *)menu_buffer;
}

void WinDraw_ClearMenuBuffer(void)
{
	memset(menu_buffer, 0, 800*600*2);
}

/********** ソフトウェアキーボード描画 **********/

#if defined(PSP) || defined(USE_OGLES11)

#if defined(PSP)
// display width 480, buffer width 512
#define KBDBUF_WIDTH 512
#elif defined(USE_OGLES11)
// display width 800, buffer width 1024 だけれど 800 にしないとだめ
#define KBDBUF_WIDTH 800
#endif

#define KBD_FS 16 // keyboard font size : 16

// キーを反転する
void WinDraw_reverse_key(int x, int y)
{
	WORD *p;
	int kp;
	int i, j;
	
	kp = Keyboard_get_key_ptr(kbd_kx, kbd_ky);

	p = kbd_buffer + KBDBUF_WIDTH * kbd_key[kp].y + kbd_key[kp].x;

	for (i = 0; i < kbd_key[kp].h; i++) {
		for (j = 0; j < kbd_key[kp].w; j++) {
			*p = ~(*p);
			p++;
		}
		p = p + KBDBUF_WIDTH - kbd_key[kp].w;
	}
}

static void draw_kbd_to_tex()
{
	int i, x, y;
	WORD *p;

	// SJIS 漢字コード
	char zen[] = {0x91, 0x53, 0x00};
	char larw[] = {0x81, 0xa9, 0x00};
	char rarw[] = {0x81, 0xa8, 0x00};
	char uarw[] = {0x81, 0xaa, 0x00};
	char darw[] = {0x81, 0xab, 0x00};
	char ka[] = {0x82, 0xa9, 0x00};
	char ro[] = {0x83, 0x8d, 0x00};
	char ko[] = {0x83, 0x52, 0x00};
	char ki[] = {0x8b, 0x4c, 0x00};
	char to[] = {0x93, 0x6f, 0x00};
	char hi[] = {0x82, 0xd0, 0x00};

	kbd_key[12].s = ka;
	kbd_key[13].s = ro;
	kbd_key[14].s = ko;
	kbd_key[16].s = ki;
	kbd_key[17].s = to;
	kbd_key[76].s = uarw;
	kbd_key[94].s = larw;
	kbd_key[95].s = darw;
	kbd_key[96].s = rarw;
	kbd_key[101].s = hi;
	kbd_key[108].s = zen;
#ifdef PSP
	kbd_key[0].s = "BK";
	kbd_key[1].s = "CP";
	kbd_key[15].s = "CA";
	kbd_key[18].s = "HP";
	kbd_key[19].s = "EC";
	kbd_key[34].s = "HM";
	kbd_key[35].s = "IN";
	kbd_key[36].s = "DL";
	kbd_key[37].s = "CL";
	kbd_key[55].s = "";
	kbd_key[56].s = "RU";
	kbd_key[57].s = "RD";
	kbd_key[58].s = "UD";
	kbd_key[63].s = "CTR";
	kbd_key[81].s = "";
	kbd_key[93].s = "";
	kbd_key[100].s = "";
	kbd_key[102].s = "X1";
	kbd_key[103].s = "X2";
	kbd_key[105].s = "X3";
	kbd_key[106].s = "X4";
	kbd_key[107].s = "X5";
	kbd_key[109].s = "OP1";
	kbd_key[110].s = "OP2";
#endif

	set_surfaceBuffPtr(kbd_buffer);
	set_menuFontSize(KBD_FS);
	set_bgcolor(0);
	set_fgcolor(0);

	// キーボードの背景
	p = kbd_buffer;
	for (y = 0; y < kbd_h; y++) {
		for (x = 0; x < kbd_w; x++) {
			*p++ = (0x7800 | 0x03e0 | 0x000f);
		}
		p = p + KBDBUF_WIDTH - kbd_w;
	}

	// キーの描画
	for (i = 0; kbd_key[i].x != -1; i++) {
		p = kbd_buffer + kbd_key[i].y * KBDBUF_WIDTH + kbd_key[i].x;
		for (y = 0; y < kbd_key[i].h; y++) {
			for (x = 0; x < kbd_key[i].w; x++) {
				if (x == (kbd_key[i].w - 1)
				    || y == (kbd_key[i].h - 1)) {
					// キーに影をつけ立体的に見せる
					*p++ = 0x0000;
				} else {
					*p++ = 0xffff;
				}
			}
			p = p + KBDBUF_WIDTH - kbd_key[i].w;
		}
		if (strlen(kbd_key[i].s) == 3 && *(kbd_key[i].s) == 'F') {
			// FUNCキー刻印描画
			set_mlocate(kbd_key[i].x + kbd_key[i].w / 2
				    - strlen(kbd_key[i].s) * (8 / 2)
				    + (strlen(kbd_key[i].s) - 1) * 3 / 2,
				    kbd_key[i].y + kbd_key[i].h / 2 - 8 / 2);
			set_menuFontSize(8);
			draw_str(kbd_key[i].s);
			set_menuFontSize(KBD_FS);
		} else {
			// 刻印は上下左右ともセンタリングする
			set_mlocate(kbd_key[i].x + kbd_key[i].w / 2
				    - strlen(kbd_key[i].s) * (KBD_FS / 2 / 2),
				    kbd_key[i].y
				    + kbd_key[i].h / 2 - KBD_FS / 2);
			draw_str(kbd_key[i].s);
		}
	}

	WinDraw_reverse_key(kbd_kx, kbd_ky);
}

#endif
