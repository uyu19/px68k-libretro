#ifndef _winx68k_windrv
#define _winx68k_windrv

#include "../win32/common.h"

void WinDrv_Init(void);
BYTE FASTCALL WinDrv_Read(DWORD adr);
void FASTCALL WinDrv_Write(DWORD adr, BYTE data);

#define MAX_PATH_TBL	64
typedef struct {
	short int	plen;	//path文字列長
	short int	drive;	//ドライブ番号
	int sum;			//一致検出用加算値
	int prio;
	char path_str[66];
	char filename[8];
	char fileext[4];
	char filenameopt[10];
	char winfile[MAX_PATH];
} path_buff_s;

#define MAX_FILES_TBL	128
typedef struct {
	int key;
	int files_num;
	int fatr;
	unsigned char pname[256];
} files_buff_s;

//FCB定義
enum {
	XFCB_FILEPTR=6,
	XFCB_FILENAME=36,
	XFCB_FILEEXT=44,
	XFCB_FILEATR=47,
	XFCB_FILENAME2=48,
	XFCB_FILETIME=58,
	XFCB_FILEDATE=60,
	XFCB_FATNO=62,
	XFCB_FILESIZE=64,
	XFCB_MAGIC=68,
	XFCB_MAGIC2=72
};

//ファイル定義
//files参照 p183
enum {
	XF_ATR=0,
	XF_DRIVENO=1,
	XF_DIRSLS=2,
	XF_DIRFAT=4,
	XF_DIRCES=6,
	XF_DIRPOS=8,
	XF_FILENAME=10,
	XF_EXT=18,
	XF_ATRCHK=21,
	XF_LASTTIME=22,
	XF_LASTDATE=24,
	XF_FILESIZE=26,
	XF_FILENAMEEXT=30,
	XF_ENDFILEBUF=53
};

//nameck p151参照
enum {
	XN_DRV=0,
	XN_PATH=2,
	XN_FILENAME=67,
	XN_EXT=86,
	XN_ENDBUFFER=91
};

//namests
enum {
	XS_WILD=0,
	XS_DRV=1,
	XS_PATH=2,
	XS_FILENAME=67,
	XS_EXT=75,
	XS_FILENAME2=78,
	XS_ENDBFFER=88
};

#endif



