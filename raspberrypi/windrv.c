//
// (c) yamama
// EX68 windrv
//

// Customize for Keropi by Kenjo

//#define WDDEBUG		// WinDrvのログ取り用

#include "../win32/common.h"
#include "../win32/winx68k.h"
#include "../win32/prop.h"
#include "../win32/cdrom.h"
#include "m68000.h"
#include "memory.h"
#include "sram.h"
#include "tvram.h"
#include "gvram.h"
#include "windrv.h"
#include <direct.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mbstring.h>
#include <winbase.h>

	path_buff_s	path_tbl[MAX_PATH_TBL];	//FCBからpathを検索する為のテーブル
	int	path_cur_prio;
	char	win_letter;
	int	num_win_drv;
	char	win_drvs[32];

	files_buff_s files_tbl[MAX_FILES_TBL];
	int files_next;		//pointer
	int files_num;
	int fatr;

	int	gr_files = 0;
	char	gr_buf[16][256];

// -------------------------------------------------------
// テーブル初期化
// -------------------------------------------------------
void init_path_tbl(void)
{
	int i;
	for(i=0; i<MAX_PATH_TBL; i++)
	{
		path_tbl[i].plen=0;
		path_tbl[i].prio=0;
	}
	path_cur_prio=0;
}

void init_files_tbl(void)
{
	int i;
	for(i=0; i<MAX_FILES_TBL; i++)
	{
		files_tbl[i].key=-1;
		files_tbl[i].files_num=-1;
	}
	files_next=0;
}


// -------------------------------------------------------
// get_real
//   X68メモリへのポインタからWinメモリの位置を計算し、バッファの実内容を求める
// -------------------------------------------------------
char* get_real(int adr)
{
	int pos;
	for (pos=0; pos<256; pos++)
		gr_buf[gr_files][pos] = Memory_ReadB(adr+pos);

	gr_files++;

	return (char*)(&gr_buf[gr_files-1][0]);
}


// -------------------------------------------------------
// gen_path(char *path, int drv, unsigned char *ppname)
//   ドライブ名を含むパスを作成
// -------------------------------------------------------
int gen_path(char *path, int drv, unsigned char *ppname)
{
	int i;

	path[0] = drv;
	path[1] = ':';
	for (i=XN_PATH; i<XN_FILENAME; i++)
	{
		char ch = ppname[i];
		if (ch == 9) ch = '\\';
		path[i] = ch;
		if (ch == 0)
		{
			if (path[i-1] == '\\')
				i--;
			break;
		}
	}
	path[i]=0;
	return i;
}


// -------------------------------------------------------
// addname(char *path, unsigned char *ppname)
//   pathにファイル名を追加する。
// -------------------------------------------------------
void addname(char *path, unsigned char *ppname)
{
	int i=0, j;

	for(j=XS_FILENAME; j<XS_EXT; j++)
	{
		if (ppname[j] == ' ')
			break;
		path[i++] = ppname[j];
	}

	if (Config.LongFileName!=0)
	{
		for(j=XS_FILENAME2; j<XS_ENDBFFER; j++)
		{
			if (ppname[j] == 0)
				break;
			path[i++] = ppname[j];
		}
	}
	path[i++]='.';
	for(j=XS_EXT; j<XS_FILENAME2; j++)
	{
		if (ppname[j] == 0)
			break;
		if (ppname[j] == ' ')
			break;
		path[i++] = ppname[j];
	}
	path[i]=0;
}


// -------------------------------------------------------
// get_space(int b_addr)
//   カレントドライブの容量を得る
//   ret: 空き容量（0=error）
// -------------------------------------------------------
int get_space(int b_addr)
{
	unsigned long lpSectorsPerCluster;
	unsigned long lpBytesPerSector;
	unsigned long lpFreeClusters;
	unsigned long lpClusters;
	char path[16];

	sprintf(path, "%c:\\", win_letter);	//ドライブ名
	if (GetDiskFreeSpace(path,		//パラメータを読み出す。
		&lpSectorsPerCluster, 
		&lpBytesPerSector, 
		&lpFreeClusters, 
		&lpClusters))
	{
		if (lpFreeClusters>0xffff) lpFreeClusters = 0xffff;	// ワード範囲を超えた場合
		if (lpClusters>0xffff) lpClusters = 0xffff;
		Memory_WriteW(b_addr,   (WORD)lpFreeClusters);
		Memory_WriteW(b_addr+2, (WORD)lpClusters);
		Memory_WriteW(b_addr+4, (WORD)lpSectorsPerCluster);
		Memory_WriteW(b_addr+6, (WORD)lpBytesPerSector);
		return (lpFreeClusters)*(lpSectorsPerCluster)*(lpBytesPerSector);
	}
	return 0;
}


// -------------------------------------------------------
// get_volume(int file)
//   ボリューム名の取得
//   file: 出力バッファ
// -------------------------------------------------------
int get_volume(int file)
{
	char lpVolumeNameBuffer[MAX_PATH];	/* ボリュームの名前のアドレス	*/
	DWORD nVolumeNameSize=12;		/* lpVolumeNameBufferの長さ */
	DWORD lpMaximumComponentLength; /* システムのファイル名の最大長のアドレス	*/
	DWORD lpFileSystemFlags;		/* ファイル システムのフラグのアドレス	*/
	char path[16];
	int i;

	sprintf(path,"%c:\\",win_letter);

#if 0
	if (GetVolumeInformation(path, 
			lpVolumeNameBuffer, 
			nVolumeNameSize,
			NULL, 
			&lpMaximumComponentLength, 
			&lpFileSystemFlags,
			NULL, 
			0))
	{
		for (i=0; i<(XF_ENDFILEBUF-XF_FILENAMEEXT); i++)
			Memory_WriteB(file+XF_FILENAMEEXT+i, lpVolumeNameBuffer[i]);
	} else {
		Memory_WriteB(file+XF_FILENAMEEXT, 0);
	}
	if (lpVolumeNameBuffer[0])
		return 0;
	return -1;
#else
	if (GetVolumeInformation(path,
			lpVolumeNameBuffer,
			nVolumeNameSize,
			NULL,
			&lpMaximumComponentLength,
			&lpFileSystemFlags,
			NULL,
			0))
	{
		for (i=0; i<(XF_ENDFILEBUF-XF_FILENAMEEXT); i++)
			Memory_WriteB(file+XF_FILENAMEEXT+i, lpVolumeNameBuffer[i]);
		return 0;	// 取得成功
	}
	return -2;		// 取得失敗
#endif
}


// -------------------------------------------------------
// gen_file_FCB(char *out, int fcb)
//   FCBからファイル名を作成する
// -------------------------------------------------------
void gen_file_FCB(char *out, int fcb)
{
	unsigned char *pfcb = get_real(fcb);
	char ch;
	int i,j;

	j=0;
	for(i=XFCB_FILENAME; i<XFCB_FILEEXT; i++)
	{
		ch=pfcb[i];
		if (ch==0) break;
		if (ch==' ') break;
		out[j++]=ch;
	}
	if (Config.LongFileName!=0)
	{	//long file name
		for(i=XFCB_FILENAME2; i<XFCB_FILETIME; i++)
		{
			ch=pfcb[i];
			if (ch==0) break;
			if (ch==' ') break;
			out[j++]=ch;
		}
	}

	out[j++]='.';
	for(i=XFCB_FILEEXT; i<XFCB_FILEATR; i++)
	{
		ch=pfcb[i];
		if (ch==0) break;
		if (ch==' ') break;
		out[j++]=ch;
	}
	out[j]=0;
}


// -------------------------------------------------------
// replace_tail(char *temp,int num)
// -------------------------------------------------------
void replace_tail(char *temp,int num)
{
	int pos;

	if (strlen(temp)==0) return;
	if (temp[num-1] != '?') return;
	pos = strlen(temp)-1;
	while((pos>=0) && (temp[pos] == '?'))
		pos--;
	temp[++pos]='*';
	temp[++pos]=0;
}


// -------------------------------------------------------
// gen_file_name(char *out, int adr)
//   namests構造からpath+8.3形式、long file nameへ変換する
//   in: namestsバッファ、out 出力バッファ
// -------------------------------------------------------
void gen_file_name(char *out, int adr)
{
	char temp[MAX_PATH], temp2[MAX_PATH];
	char in[XS_ENDBFFER];
	int i;

	for (i=0; i<XS_ENDBFFER; i++)
		in[i] = Memory_ReadB(adr+i);

	//pathをコピーする
	i=gen_path(out,win_letter,(unsigned char *)in);
	out[i++]='\\';
	out[i++]=0;
	//ファイル名部分
	memcpy(temp,&in[XS_FILENAME],8); temp[8]=0;
	for(i=0; i<8; i++)
		if (temp[i]==' ') temp[i]=0;
	if (strcmp(temp,"????????")==0)
	{
		strcat(out,"*");
	}
	else
	{
		strcpy(temp2,temp);
		// file name +
		memcpy(temp,&in[XS_FILENAME2],10); temp[10]=0;
		for(i=0; i<10; i++)
			if (temp[i]==' ') temp[i]=0;
		strcat(temp2,temp);
		replace_tail(temp2,18);
		strcat(out,temp2);
	}

	//拡張子
	memcpy(temp,&in[XS_EXT],3); temp[3]=0;
	for(i=0; i<3; i++)
		if (temp[i]==' ') temp[i]=0;
	replace_tail(temp,3);
	if (temp[0])
	{
		if (strcmp(temp,"???")==0)
		{
			strcat(out,".*");
		}
		else
		{
			strcat(out,".");
			strcat(out,temp);
		}
	}
}


// -------------------------------------------------------
// gen_file_name2(char *out, char* adr)
//   namests構造からpath+8.3形式、long file nameへ変換する
//   in: namestsバッファ、out 出力バッファ
// -------------------------------------------------------
void gen_file_name2(char *out, char* in)
{
	char temp[MAX_PATH], temp2[MAX_PATH];
	int i;

	//pathをコピーする
	i = gen_path(out,win_letter,(unsigned char *)in);
	out[i++]='\\';
	out[i++]=0;
	//ファイル名部分
	memcpy(temp,&in[XS_FILENAME],8); temp[8]=0;
	for(i=0; i<8; i++)
		if (temp[i]==' ') temp[i]=0;
	if (strcmp(temp,"????????")==0)
	{
		strcat(out,"*");
	}
	else
	{
		strcpy(temp2,temp);
		// file name +
		memcpy(temp,&in[XS_FILENAME2],10); temp[10]=0;
		for(i=0; i<10; i++)
			if (temp[i]==' ') temp[i]=0;
		strcat(temp2,temp);
		replace_tail(temp2,18);
		strcat(out,temp2);
	}

	//拡張子
	memcpy(temp,&in[XS_EXT],3); temp[3]=0;
	for(i=0; i<3; i++)
		if (temp[i]==' ') temp[i]=0;
	replace_tail(temp,3);
	if (temp[0])
	{
		if (strcmp(temp,"???")==0)
		{
			strcat(out,".*");
		}
		else
		{
			strcat(out,".");
			strcat(out,temp);
		}
	}
}


// -------------------------------------------------------
// spr_name83(char *in,char *name,char *ext)
//   名前と拡張子に分解する
//   余白はスペースにする
//   in → name / ext
// -------------------------------------------------------
void spr_name83(char *in,char *name,char *ext)
{
	char ch;
	int i, j, pos;

	if (Config.LongFileName!=0)
	{	//long file name
		strcpy(name,"                  ");
		strcpy(ext,"   ");
		pos=-1;
		for (i=0; in[i]!=0; i++)
		{
			if (in[i]=='.') pos=i;
		}
		if (pos==-1)
		{	// no .
			memcpy(name, in, min(i,18));
		}
		else
		{	//
			memcpy(name, in, pos);
			if (i>pos)
				memcpy(ext, &in[pos+1], i-pos);
		}
	}
	else
	{	//8.3
		strcpy(name,"        ");
		strcpy(ext,"   ");
		i=0; j=0;
		while(1)
		{
			ch=in[i++];
			if (ch=='.')	//次は拡張子
				break;
			if (ch==0)
				return;
			name[j++]=ch;
		}
		if (in[i])
		{
			j=0;
			while(1)
			{
				ch=in[i++];
				if (ch==0)
					return;
				ext[j++]=ch;
			}
		}
	}
}


// -------------------------------------------------------
// mer_name83(char *out,char *name,char *ext)
//   83形式に合成する
//   out ← name.ext
// -------------------------------------------------------
void mer_name83(char *out,char *name,char *ext)
{
	char ch;
	int i,j;

	if (Config.LongFileName!=0)
	{	//long file name
		j=0;
		for(i=0; i<(8+10); i++)
		{
			ch=name[i];
			if (ch==0) break;
			if (ch==' ') break;
			out[j++]=ch;
		}
		out[j++]='.';
		for(i=0; i<3; i++)
		{
			ch=ext[i];
			if (ch==0) break;
			if (ch==' ') break;
			out[j++]=ch;
		}
		out[j]=0;
	}
	else
	{	//8.3 file name
		j=0;
		for(i=0; i<8; i++)
		{
			ch=name[i];
			if (ch==0) break;
			if (ch==' ') break;
			out[j++]=ch;
		}
		out[j++]='.';
		for(i=0; i<3; i++)
		{
			ch=ext[i];
			if (ch==0) break;
			if (ch==' ') break;
			out[j++]=ch;
		}
		out[j]=0;
	}
}

enum {
	HU_READONLY=1,
	HU_HIDDEN=2,
	HU_SYSTEM=4,
	HU_ARCHIVE=0x20
};


// -------------------------------------------------------
// conv_atr(int atr)
//   Windowsのファイル属性をHumanのファイル属性に変換する
//   atr : windows file attribute
//   return: Human file attribute
// -------------------------------------------------------
int conv_atr(int atr)
{
	int res=0;

//	if (atr&FILE_ATTRIBUTE_ARCHIVE)
//		res|=HU_ARCHIVE;
	if (atr&FILE_ATTRIBUTE_HIDDEN)
		res|=HU_HIDDEN;
	if (atr&FILE_ATTRIBUTE_READONLY)
		res|=HU_READONLY;
	if (atr&FILE_ATTRIBUTE_SYSTEM)
		res|=HU_SYSTEM;
	return res;
}


// -------------------------------------------------------
// xconv_atr(int atr)
//   Humanのファイル属性をWindowsのファイル属性に変換する
//   atr : Human file attribute
//   return: Windows file attribute
// -------------------------------------------------------
int xconv_atr(int atr)
{
	int res=0;

//	if (atr&HU_ARCHIVE)
		res|=FILE_ATTRIBUTE_ARCHIVE;   
	if (atr&HU_HIDDEN)
		res|=FILE_ATTRIBUTE_HIDDEN;
	if (atr&HU_READONLY)
		res|=FILE_ATTRIBUTE_READONLY;
	if (atr&HU_SYSTEM)
		res|=FILE_ATTRIBUTE_SYSTEM;
	return res;
}


// -------------------------------------------------------
// spacetounderbar(char *cFileName)
//   X68では使えないスペースをアンダーバーに変換する。
// -------------------------------------------------------
void spacetounderbar(char *cFileName)
{
	int size=strlen(cFileName), i;
	for(i=0; i<size; i++)
	{
		if (cFileName[i]==0)
			return;
		else if (cFileName[i]==' ')
			cFileName[i]='_';
	}
}


// -------------------------------------------------------
// isx68filename(const char *name)
//   X68の扱えるファイル名かどうかをチェック
//   TRUE: OK、FALSE: Win Only
// -------------------------------------------------------
int isx68filename(const char *name)
{
	int i, pos, spos;

	pos=-1;
	spos=-1;
	for (i=0; name[i]!=0; i++)
	{
		if (name[i]=='.') pos=i;
		if (name[i]==' ') spos=i;
	}
	if (pos > 18)
		return FALSE;	//拡張子以前が18文字を超える
	if (pos != -1)
	{
		if ((i-pos) > 4)
			return FALSE;	//拡張子が4文字を超える
	}
	else
	{
		if (i>18)
			return FALSE;	//拡張子の無いファイル名の長さが18文字以上
	}
	if (spos != -1)
		return FALSE;	//スペースが入っている
	return TRUE;
}


// -------------------------------------------------------
// set_file_name(int file, WIN32_FIND_DATA lpffd)
//   Windowsのファイル名がX68のファイル名として使えるならlong file nameを
//   使えないならmsdosファイル名を使う
//   lpffd: Winのファイル、file: 結果バッファ
// -------------------------------------------------------
void set_file_name(int file, WIN32_FIND_DATA lpffd)
{
	int i;
		//ロングファイル名対応
	if (Config.LongFileName!=0)
	{
		if (!lpffd.cFileName[0])
		{
			for (i=0; i<12; i++)
				Memory_WriteB(file+XF_FILENAMEEXT+i, lpffd.cAlternateFileName[i]);
		}
		else 
		{
			if ((lpffd.cAlternateFileName[0]) && (!isx68filename(lpffd.cFileName)))
			{
				for (i=0; i<12; i++)
					Memory_WriteB(file+XF_FILENAMEEXT+i, lpffd.cAlternateFileName[i]);
				//strncpy((char*)&pfile[XF_FILENAMEEXT],lpffd.cAlternateFileName,12);
			} else {
				for (i=0; i<23; i++)
					Memory_WriteB(file+XF_FILENAMEEXT+i, lpffd.cFileName[i]);
				//strncpy((char*)&pfile[XF_FILENAMEEXT],lpffd.cFileName,23);
			}
		}
	}
	else
	{	//8.3
		if (lpffd.cAlternateFileName[0])
		{
			for (i=0; i<12; i++)
				Memory_WriteB(file+XF_FILENAMEEXT+i, lpffd.cAlternateFileName[i]);
			//strncpy((char*)&pfile[XF_FILENAMEEXT],lpffd.cAlternateFileName,12);
		} else {
			for (i=0; i<23; i++)
				Memory_WriteB(file+XF_FILENAMEEXT+i, lpffd.cFileName[i]);
			//strncpy((char*)&pfile[XF_FILENAMEEXT],lpffd.cFileName,23);
		}
	}
}


// -------------------------------------------------------
// visivle(char *f)
//   可視のファイルを検出する
// -------------------------------------------------------
int visible(char* f)
{
	int i;

	for(i=0; f[i]!=0; i++)
	{
		if (f[i]&0x80)
		{
			i++; continue;	//２バイト文字は検出しない
		}
		if ( (f[i]=='[') || (f[i]==']') || (f[i]=='=') || (f[i]=='+') )
			return FALSE;	//不可視文字検出 falseへ
	}
	return TRUE;
}


// -------------------------------------------------------
// get_files(int pfile)
//   ファイルまたはディレクトリを１つ調べる
//   pfile: 検索結果のバッファアドレス
// -------------------------------------------------------
int get_files(char* name, int pfile, int atr)
{
	int number=-1;
	WORD lpwDOSDate,lpwDOSTime;
	FILETIME lpftLocal;

	WIN32_FIND_DATA lpffd;	/* 返される情報のアドレス	*/
	HANDLE file;

	int first=1;

	while(1)
	{
		if (first)	//最初
		{
			file = FindFirstFile(name, &lpffd);
			if (file==INVALID_HANDLE_VALUE)
			{
 				Memory_WriteB(pfile+1, 0);
				return -2;		//見つからない
			}
			first=0;
		}
		else	//２回目以降の検索
		{
			if (!FindNextFile(file, &lpffd))
			{
 				Memory_WriteB(pfile+1, 0);
				FindClose(file);
				return -2;		//見つからない
			}
		}

		if (lpffd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)	//ディレクトリ
		{
			if (atr&0x10)	//ディレクトリも検索中
			{
				if (!visible(lpffd.cFileName))
					continue;
				number++;
				if (number != files_num)	//順番をチェック
					continue;
				//ファイル属性の変換
 				Memory_WriteB(pfile+XF_ATRCHK, (BYTE)(0x10|conv_atr(lpffd.dwFileAttributes)));

				//ロングファイル名対応
				set_file_name(pfile,lpffd);

				Memory_WriteD(pfile+26, 0);	//lpffd.nFileSizeHigh

				//タイムスタンプ対応
				FileTimeToLocalFileTime(&lpffd.ftLastWriteTime, &lpftLocal);
				if (FileTimeToDosDateTime(&lpftLocal, &lpwDOSDate, &lpwDOSTime))
				{
					Memory_WriteW(pfile+XF_LASTTIME, lpwDOSTime);
					Memory_WriteW(pfile+XF_LASTDATE, lpwDOSDate);
				}
				Memory_WriteD(pfile+2, 0xa1);
				Memory_WriteW(pfile+6, 0xf);
				Memory_WriteW(pfile+8, (WORD)(number*0x20));
				FindClose(file);
				return 0;	//0xfffffff2;
			}
		}
		else
		{
			if ((atr&0x20))	//ファイルを検索中
			{
				if (!visible(lpffd.cFileName))
					continue;
				number++;
				if (number != files_num)	//順番をチェック
					continue;
				//ファイル属性対応
 				Memory_WriteB(pfile+XF_ATRCHK, (BYTE)(0x20|conv_atr(lpffd.dwFileAttributes)));

				//ロングファイル名対応
				set_file_name(pfile, lpffd);

				//ファイルサイズ対応
				Memory_WriteD(pfile+XF_FILESIZE, lpffd.nFileSizeLow);

				//タイムスタンプ対応
				FileTimeToLocalFileTime(&lpffd.ftLastWriteTime, &lpftLocal);
				if (FileTimeToDosDateTime(&lpftLocal, &lpwDOSDate, &lpwDOSTime))
				{
					Memory_WriteW(pfile+XF_LASTTIME, lpwDOSTime);
					Memory_WriteW(pfile+XF_LASTDATE, lpwDOSDate);
				}
				Memory_WriteD(pfile+2, 0xa1);
				Memory_WriteW(pfile+6, 0xf);
				Memory_WriteW(pfile+8, (WORD)(number*0x20));
 				FindClose(file);
				return 0;	//0xfffffff2;
			}
		}
	}
}


// -------------------------------------------------------
// files(int atr, int name, int file)
//   ファイルまたはディレクトリの検索（1回目）
//   atr: 検索ファイルのアトリビュート、name: ファイル名へのポインタ、file: 結果バッファ
// -------------------------------------------------------
int files(int atr, int name, int file)
{
	int ret, i;
	char pname[MAX_PATH];

	ZeroMemory(pname, MAX_PATH);
	files_tbl[files_next].key = file;
	files_num=files_tbl[files_next].files_num = 0;
	files_tbl[files_next].fatr = atr;
	for (i=0; i<256; i++)
		files_tbl[files_next].pname[i] = (BYTE)Memory_ReadB(name+i);
	files_next = (files_next+1)&(MAX_FILES_TBL-1);
	fatr=atr;


	if ((atr&0x38)==8)
	{	//ボリュームラベル
		return get_volume(file);
	}
	else
	{
		gen_file_name(pname, name);	//namestsからpath+8.3形式へ変換
#ifdef WDDEBUG
{
FILE *fp;
fp=fopen("_windrv.txt", "a");
fprintf(fp, "FILES - %s\n", pname);
fclose(fp);
}
#endif
		ret=get_files(pname, file, atr);
		return ret;
	}
	return -1;
}


// -------------------------------------------------------
// get_files_idx(int file)
//   次のファイルまたはディレクトリの検索
//   file: 結果バッファ
// -------------------------------------------------------
int get_files_idx(int file)
{
	int i, idx;
	for(i=0; i<MAX_FILES_TBL; i++)
	{
		idx = (files_next+MAX_FILES_TBL*2-i-1)&(MAX_FILES_TBL-1);
		if (files_tbl[idx].key == file)
		{
			return idx;
		}
	}
	return -1;
}


// -------------------------------------------------------
// nfiles(int file)
//   次のファイルまたはディレクトリの検索
//   file: 結果バッファ
// -------------------------------------------------------
int nfiles(int file)
{
	int idx, ret;
	char pname[MAX_PATH];

	ZeroMemory(pname, MAX_PATH);
	idx = get_files_idx(file);
	if (idx < 0) return -2;

	gen_file_name2(pname, files_tbl[idx].pname);	//namestsからpath+8.3形式へ変換

	files_tbl[idx].files_num++;
	files_num = files_tbl[idx].files_num;
#ifdef WDDEBUG
{
FILE *fp;
fp=fopen("_windrv.txt", "a");
fprintf(fp, "NFILES - %s / Num:%d\n", pname, files_num);
fclose(fp);
}
#endif
	ret = get_files(pname, file, files_tbl[idx].fatr);

	return ret;
}


// -------------------------------------------------------
// chk_dir(int pname)
//   ディレクトリの存在をチェックする
//   ret=0: 存在、ret=-2: ないorディレクトリ以外
// -------------------------------------------------------
int chk_dir(int pname)
{
	unsigned char *ppname = get_real(pname);
	char path[MAX_PATH];
	WIN32_FIND_DATA lpffd;	/* 返される情報のアドレス	*/
	HANDLE file;
	int return_f;

	if (ppname[3] == 0)	return 0;	//ルートディレクトリは必ず存在する。

	(void)gen_path(path,win_letter,ppname);
	file = FindFirstFile(path, &lpffd);

	if (file == INVALID_HANDLE_VALUE)
	{
		return_f = -2;
	}
	else if (lpffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		FindClose(file);
		return_f = 0;
	}
	else
	{
		FindClose(file);
		return_f = -2;
	}

	return return_f;
}


// -------------------------------------------------------
// make_dir(int pname)
//   ディレクトリを作成する
// -------------------------------------------------------
int make_dir(int pname)
{
	unsigned char *ppname = get_real(pname);
	char path[MAX_PATH];
	int i;

	i=gen_path(path, win_letter, ppname);
	path[i++]='\\';
	addname(&path[i], ppname);
	return _mkdir(path);
}


// -------------------------------------------------------
// rm_dir(int pname)
//   ディレクトリを削除する
// -------------------------------------------------------
int rm_dir(int pname)
{
	unsigned char *ppname = get_real(pname);
	char path[MAX_PATH];
	int i;

	i=gen_path(path, win_letter, ppname);
	path[i++]='\\';
	addname(&path[i], ppname);
	return _rmdir(path);
}


// -------------------------------------------------------
// file_atr(int atr_adr,int pname)
//   ファイル属性の読み出し、書き込み
// -------------------------------------------------------
int file_atr(int atr_adr, int pname)
{
	int atr = Memory_ReadB(atr_adr);	//-1なら取得
	char lpszSearchFile[MAX_PATH];		// 検索するファイルの名前のアドレス
	HANDLE file;
	WIN32_FIND_DATA lpffd;			// 返される情報のアドレス

	gen_file_name(lpszSearchFile, pname);	//namestsからpath+8.3形式へ変換
	file=FindFirstFile(lpszSearchFile, &lpffd);
	if (file==INVALID_HANDLE_VALUE)
		return -2;

	if (lpffd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
	{
		//ディレクトリなら無効。
		FindClose(file);
		return -2;
	}

	FindClose(file);
	if ((atr&0xff)==0xff)	//問い合わせ
	{
		//読み取ったファイルの属性をattr_adrに設定する。
		Memory_WriteB(atr_adr, (BYTE)conv_atr(lpffd.dwFileAttributes));
	}
	else
	{
		//atrをファイルに設定する。
		SetFileAttributes(lpszSearchFile,	// address of filename
				xconv_atr(atr));	// address of attributes to set
	}
	return 0;
}


// -------------------------------------------------------
// chkdouble(char *pathfile)
//   \の重複\\を\に修正
// -------------------------------------------------------
void chkdouble(char *pathfile)
{
	char *wpt,*pt;
	wpt=pt=pathfile;
	while(*pt)
	{
		if ((*pt=='\\') && (*(pt+1)=='\\'))
			pt++;
		*wpt++=*pt++;
	}
	*wpt=0;
}



// -------------------------------------------------------
// chk_file(int pname, int fcb)
//   xopenより。 ファイルの存在をチェックする？
// -------------------------------------------------------
int chk_file(int pname, int fcb)
{
	char lpszSearchFile[MAX_PATH];	/* 検索するファイルの名前のアドレス */
	int first=1;
	int number=-1;
	WORD lpwDOSDate,lpwDOSTime;
	FILETIME lpftLocal;

	WIN32_FIND_DATA lpffd;	/* 返される情報のアドレス	*/
	HANDLE file;
	char sname[10+10], sext[4];
	char *ppname = get_real(pname);

	int i,j,min;
	char name_buf[16+10];
	int sum=0;
	char ch, tmp;

	gen_file_name(lpszSearchFile, pname);	//namestsからpath+8.3形式へ変換

//	strcpy(lpszSearchFile,"\\*.*");
	file=FindFirstFile(lpszSearchFile, &lpffd);
	if (file==INVALID_HANDLE_VALUE)
	{
		return -2;
	}
	if (lpffd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
	{
		FindClose(file);
		return -2;	//ディレクトリである
	}

	tmp = Memory_ReadB( fcb+14 );
	if ((tmp==1) || (tmp==2))
		if (lpffd.dwFileAttributes&
			(FILE_ATTRIBUTE_HIDDEN |
			 FILE_ATTRIBUTE_READONLY |
			 FILE_ATTRIBUTE_SYSTEM))
		{
			FindClose(file);
				 return -2; //書き込みは不可である
		}

	Memory_WriteD(fcb+6, 0);
	Memory_WriteB(fcb+XFCB_FILEATR, (BYTE)lpffd.dwFileAttributes);	//file attr
	Memory_WriteD(fcb+32, 0);

//要long対応
	if (Config.LongFileName!=0)
	{
		if ((lpffd.cAlternateFileName[0]) && (!isx68filename(lpffd.cFileName)))
			spr_name83(lpffd.cAlternateFileName,sname,sext);
		else
			spr_name83(lpffd.cFileName,sname,sext);
	}
	else
	{
		if (lpffd.cAlternateFileName[0])
			spr_name83(lpffd.cAlternateFileName,sname,sext);
		else
			spr_name83(lpffd.cFileName,sname,sext);
	}
	for (i=0; i<8; i++)  Memory_WriteB(fcb+XFCB_FILENAME+i, sname[i]);
	for (i=0; i<3; i++)  Memory_WriteB(fcb+XFCB_FILEEXT+i,  sext[i]);
	for (i=0; i<10; i++) Memory_WriteB(fcb+XFCB_FILENAME2+i,sname[i+8]);

	//ファイルサイズ対応
	Memory_WriteD(fcb+XFCB_FILESIZE,lpffd.nFileSizeLow);

#ifdef WDDEBUG
{
FILE *fp;
fp=fopen("_windrv.txt", "a");
fprintf(fp, "CheckFile - %s / Size:%dbytes\n", lpffd.cFileName, lpffd.nFileSizeLow);
fclose(fp);
}
#endif

	//タイムスタンプ対応
	FileTimeToLocalFileTime(&lpffd.ftLastWriteTime, &lpftLocal);
	if (FileTimeToDosDateTime(&lpftLocal, &lpwDOSDate, &lpwDOSTime))
	{
		Memory_WriteW(fcb+XFCB_FILETIME, lpwDOSTime);
		Memory_WriteW(fcb+XFCB_FILEDATE, lpwDOSDate);
	}
	for(i=0; i<MAX_PATH_TBL; i++)
	{
		if (path_tbl[i].plen==0)	//空きを探す
			break;
	}
	if (i>=MAX_PATH_TBL)	//見つからなかったときのリカバリ
	{
		i=0;
		min=0x7fffffff;
		for(j=0; j<MAX_PATH_TBL; j++)
		{
			if (path_tbl[j].prio<min)
			{
				min=path_tbl[j].prio;
				i=j;
			}
		}
	}

//long対応
	//ファイル名と拡張子を合成
	mer_name83(name_buf,sname,sext);

	//ここでFCBとファイル名の対応表に記録する
	path_tbl[i].plen=strlen((char*)&ppname[XS_PATH])+strlen(name_buf);
	path_tbl[i].drive=ppname[XS_DRV];
	memcpy(path_tbl[i].path_str,&ppname[XS_PATH],64);
	memcpy(path_tbl[i].filename,&ppname[XS_FILENAME],11);
	memcpy(path_tbl[i].filenameopt,&ppname[XS_FILENAME2],10);

	strcpy(path_tbl[i].winfile, lpszSearchFile);
#ifdef WDDEBUG
{
FILE *fp;
fp=fopen("_windrv.txt", "a");
fprintf(fp, "Open - %s\n", lpszSearchFile);
fclose(fp);
}
#endif
	for(j=0; j<64; j++)
	{
		if (path_tbl[i].path_str[j]==9)
			path_tbl[i].path_str[j]='\\';
	}
	path_tbl[i].prio=path_cur_prio++;

	j=0;
	sum = 0;
	while((ch=path_tbl[i].path_str[j++])!=0)
		sum=sum*7+ch;
	path_tbl[i].sum = sum;
	Memory_WriteD(fcb+XFCB_MAGIC, sum);
	Memory_WriteB(fcb+XFCB_MAGIC2, (BYTE)path_tbl[i].drive);
	FindClose(file);
	if (Memory_ReadB(fcb+0xe) == 1)		//open("...","w")の時の切り詰めを行う
	{
		char pathfile[MAX_PATH];
		char filename[32];
		FILE *wfp;
	//要long file name対応
	//ファイル名を合成する。
		gen_file_FCB(filename, fcb);
		if (strcmp(path_tbl[i].path_str,"\\")==0)
			sprintf(pathfile,"%c:\\%s",win_letter,filename);
		else
			sprintf(pathfile,"%c:%s\\%s",win_letter,path_tbl[i].path_str,filename);
		chkdouble(pathfile);
		wfp=fopen(pathfile,"w");
		if (wfp!=NULL)
			fclose(wfp);
	}
	return 0;
}


// -------------------------------------------------------
// get_path_idx(int fcb)
//   FCBからファイル名の記録をたどる。
// -------------------------------------------------------
int get_path_idx(int fcb)
{
	int i,j;
	char ch;
	int chks = Memory_ReadD(fcb+XFCB_MAGIC);
	int name_len;
	int name_len2;
	int ext_len;
	int sum=0;
	char* pfcb = get_real(fcb);

//要long file name対応
	for( i=0; i<8; i++)
	{
		ch = pfcb[XFCB_FILENAME + i];
		if (ch == 0) break;
		if (ch == ' ') break;
	}
	name_len=i;	//これはファイル名の長さ
	name_len2=0;
	if (Config.LongFileName!=0)
	{
		for( i=0; i<10; i++)
		{
			ch = pfcb[XFCB_FILENAME2 + i];
			if (ch == 0) break;
		if (ch == ' ') break;
		}
		name_len2=i;
	}

	for(i=0; i<3; i++)
	{
		ch=pfcb[XFCB_FILEEXT + i];
		if (ch==0) break;
		if (ch==' ') break;
	}
	ext_len=i;	//これは拡張子の長さ

//ここから検索する。
	for(i=0; i<MAX_PATH_TBL; i++)
	{
		//ドライブ番号を比較する
		if (path_tbl[i].drive != pfcb[XFCB_MAGIC + 4]) continue;
//要long file name対応
		//ファイル名を比較する
		if (strnicmp((char*)&pfcb[XFCB_FILENAME], path_tbl[i].filename, name_len) != 0) continue;
		if (strnicmp((char*)&pfcb[XFCB_FILEEXT], &path_tbl[i].filename[8], ext_len) != 0) continue;
		if (Config.LongFileName!=0)
		{
			if (strnicmp((char*)&pfcb[XFCB_FILENAME2], path_tbl[i].filenameopt, name_len2) != 0) continue;
		}
		j = 0;
		sum = 0;
		while((ch = path_tbl[i].path_str[j++]) != 0)
			sum = sum * 7 + ch;
#ifdef WDDEBUG
{
FILE *fp;
fp=fopen("_windrv.txt", "a");
fprintf(fp, "Index Matched. %s - Sum:%08X Chks:%08X\n", path_tbl[i].filename, sum, chks);
fclose(fp);
}
#endif
		if (sum == chks)	//一致検出
			return i;
	}
	return -1;
}


// -------------------------------------------------------
// file_time(int time_adr,int fcb)
//   ファイルのタイムスタンプの読み書き
// -------------------------------------------------------
int file_time(int time_adr,int fcb)
{
	int idx, atr;
	char pathfile[MAX_PATH], filename[32];
	WIN32_FIND_DATA lpffd;		// Winから返されるファイル情報
	WORD lpwDOSDate,lpwDOSTime;
	FILETIME lpftLocal;
	HANDLE file, fp;
	DWORD ret = 0;

	atr=Memory_ReadD(time_adr);
	idx=get_path_idx(fcb);		// FCBからファイル名の記録をたどる
	if (idx<0)
		return -1;		//ファイルが見つからなかった

	//要long file name対応
	//ファイル名を合成する
	gen_file_FCB(filename, fcb);
	if (strcmp(path_tbl[idx].path_str,"\\")==0)
		sprintf(pathfile,"%c:\\%s",win_letter,filename);
	else
		sprintf(pathfile,"%c:%s\\%s",win_letter,path_tbl[idx].path_str,filename);
	chkdouble(pathfile);

	file=FindFirstFile(pathfile, &lpffd);
	if (file==INVALID_HANDLE_VALUE)
	{
		return -2;
	}

	if (lpffd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
	{
		FindClose(file);
		return -2;	//ディレクトリである
	}

	FindClose(file);

	if (atr==0) //ファイルから日時を取得する
	{
		if (!FileTimeToLocalFileTime(&lpffd.ftLastWriteTime, &lpftLocal))
			return -1;
		if (FileTimeToDosDateTime(&lpftLocal, &lpwDOSDate, &lpwDOSTime))
		{
			Memory_WriteW(fcb+XFCB_FILETIME,lpwDOSTime);	// 一応FCBの内容も更新しておく
			Memory_WriteW(fcb+XFCB_FILEDATE,lpwDOSDate);
			ret = (lpwDOSDate<<16)+lpwDOSTime;		// (a5+18)L <- Date/Time? (Kenjo)
#ifdef WDDEBUG
{
FILE *fp;
fp=fopen("_windrv.txt", "a");
fprintf(fp, "GetDate/Time - $%04X/$%04X\n", lpwDOSTime, lpwDOSDate);
fclose(fp);
}
#endif
		}
	}
	else	//ファイルに日時を設定する。
	{
//		lpwDOSTime = Memory_ReadW(fcb+XFCB_FILETIME);
//		lpwDOSDate = Memory_ReadW(fcb+XFCB_FILEDATE);
		lpwDOSTime = Memory_ReadW(time_adr+2);		// こっちが正解？ (Kenjo)
		lpwDOSDate = Memory_ReadW(time_adr);
		if (! DosDateTimeToFileTime(lpwDOSDate, lpwDOSTime, &lpftLocal))
			return -1;
		if (! LocalFileTimeToFileTime(&lpftLocal, &lpffd.ftLastWriteTime))
			return -1;
#ifdef WDDEBUG
{
FILE *fp;
fp=fopen("_windrv.txt", "a");
fprintf(fp, "SetDate/Time - $%04X/$%04X\n", lpwDOSTime, lpwDOSDate);
fclose(fp);
}
#endif
		fp = CreateFile(pathfile, GENERIC_WRITE , FILE_SHARE_READ, 
					NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		if (fp == INVALID_HANDLE_VALUE)
		{
			return -2;
		}
		if (fp == 0) return  -1;
		if (!SetFileTime(fp, &lpffd.ftLastWriteTime, &lpffd.ftLastWriteTime, &lpffd.ftLastWriteTime))
		{
			CloseHandle(fp);
			return -1;
		}
		CloseHandle(fp);
	}
	return ret;
}


//----------------------------------------------------------
// isopened(int name)
//   指定ファイルが既にオープンされているかどうかを調べる
//   ret: 0=開かれていない、1=開かれている
//----------------------------------------------------------
int isopened(int name)
{
	int i, ret = 0;
	char lpszSearchFile[MAX_PATH];	/* 検索するファイルの名前のアドレス */

	gen_file_name(lpszSearchFile, name);	//namestsからpath+8.3形式へ変換
#ifdef WDDEBUG
{
char *pname = get_real(name);
FILE *fp;
fp=fopen("_windrv.txt", "a");
fprintf(fp, "Open Check - %s\n", lpszSearchFile);
fclose(fp);
}
#endif
	for(i=0; i<MAX_PATH_TBL; i++)
	{
		if (path_tbl[i].plen==0)
			continue;
		if (strcmp(path_tbl[i].winfile, lpszSearchFile)==0)
		{
			ret = 1;
			break;
		}
	}
	return ret;
}



//----------------------------------------------------------
// xcreate(int name, int fcb, int xatr, int mode)
//   ファイルを作成してオープン
//----------------------------------------------------------
int xcreate(int name, int fcb, int xatr, int mode)
{
	unsigned char *ppname = get_real(name);		//ファイル名
	unsigned char *pfcb = get_real(fcb);
	char lpszSearchFile[MAX_PATH];	/* 検索するファイルの名前のアドレス */
	WIN32_FIND_DATA lpffd;	/* 返される情報のアドレス	*/
	HANDLE file;
	int oflag, res;
	WORD lpwDOSDate,lpwDOSTime;
	FILETIME lpftLocal;
	char sname[10+10],sext[4];
	int i, j, min, pos;
	char name_buf[16+10];
	int sum=0;
	char ch;

	if (isopened(name)) return -33;

	gen_file_name(lpszSearchFile, name);	//namestsからpath+8.3形式へ変換

	oflag=_O_BINARY|_O_CREAT|_O_TRUNC;
//	if (!mode)
//		oflag|=_O_CREAT;
//	else
//		oflag|=_O_TRUNC;

	if (xatr&1)
		oflag|=_O_RDONLY;
	else
		oflag|=_O_RDWR;
//	string strfile(lpszSearchFile);
	pos=-1;
	for (i=0; lpszSearchFile[i]!=0; i++)
	{
		if (lpszSearchFile[i]=='\\') pos=i;
	}
	if (pos != -1)
		if (!isx68filename(&lpszSearchFile[pos+1]))
			return -1;

#ifdef WDDEBUG
{
FILE *fp;
fp=fopen("_windrv.txt", "a");
fprintf(fp, "Create %s / Mode:%d XAtr:%d\n", &lpszSearchFile[pos+1], mode, xatr&1);
fclose(fp);
}
#endif
	res=_open(lpszSearchFile, oflag, _S_IREAD|_S_IWRITE);
	if (res>=0)
		_close(res);

	file=FindFirstFile(lpszSearchFile, &lpffd);

	Memory_WriteD(fcb+6, 0);
	Memory_WriteB(fcb+XFCB_FILEATR, (BYTE)xatr);	//file attr
	Memory_WriteD(fcb+32, 0);

	if (Config.LongFileName!=0)
	{
		if ((lpffd.cAlternateFileName[0]) && (!isx68filename(lpffd.cFileName)))
			spr_name83(lpffd.cAlternateFileName,sname,sext);
		else
			spr_name83(lpffd.cFileName,sname,sext);
	}
	else
	{
		if (lpffd.cAlternateFileName[0])
			spr_name83(lpffd.cAlternateFileName,sname,sext);
		else
			spr_name83(lpffd.cFileName,sname,sext);
	}
	for (i=0; i<8; i++) Memory_WriteB(fcb+XFCB_FILENAME+i, sname[i]);
	for (i=0; i<3; i++) Memory_WriteB(fcb+XFCB_FILEEXT+i, sext[i]);
	for (i=0; i<10; i++) Memory_WriteB(fcb+XFCB_FILENAME2+i, sname[i+8]);

	Memory_WriteD(fcb+XFCB_FILESIZE,lpffd.nFileSizeLow);

	FileTimeToLocalFileTime(&lpffd.ftLastWriteTime, &lpftLocal);
	if (FileTimeToDosDateTime(&lpftLocal, &lpwDOSDate, &lpwDOSTime))
	{
		Memory_WriteW(fcb+XFCB_FILETIME,lpwDOSTime);
		Memory_WriteW(fcb+XFCB_FILEDATE,lpwDOSDate);
	}

	for(i=0; i<MAX_PATH_TBL; i++)
	{
		if (path_tbl[i].plen==0)	//空きを探す
			break;
	}
	if (i>=MAX_PATH_TBL)	//見つからなかったときのリカバリ
	{
		i=0;
		min=0x7fffffff;
		for(j=0; j<MAX_PATH_TBL; j++)
		{
			if (path_tbl[j].prio<min)
			{
				min=path_tbl[j].prio;
				i=j;
			}
		}
	}

	mer_name83(name_buf, sname, sext);
	path_tbl[i].plen=strlen((char*)&ppname[XS_PATH])+strlen(name_buf);
	path_tbl[i].drive=ppname[1];
	memcpy(path_tbl[i].path_str, &ppname[XS_PATH],64);
	memcpy(path_tbl[i].filename, &ppname[XS_FILENAME],11);
	memcpy(path_tbl[i].filenameopt, &ppname[XS_FILENAME2],10);
	for(j=0; j<64; j++)
	{
		if (path_tbl[i].path_str[j]==9)
			path_tbl[i].path_str[j]='\\';
	}
	path_tbl[i].prio=path_cur_prio++;

	j=0;
	sum = 0;
	while((ch=path_tbl[i].path_str[j++])!=0)
		sum=sum*7+ch;
	path_tbl[i].sum=sum;
	Memory_WriteD(fcb+XFCB_MAGIC, sum);
	Memory_WriteB(fcb+XFCB_MAGIC2, (BYTE)path_tbl[i].drive);

	FindClose(file);
	if (res==-1)
		return -1;
	return 0;
}


//----------------------------------------------------------
// rm_file(int name)
//   ファイル削除
//----------------------------------------------------------
int rm_file(int name)
{
	char lpszSearchFile[MAX_PATH];	/* 検索するファイルの名前のアドレス */
 	gen_file_name(lpszSearchFile, name);	//namestsからpath+8.3形式へ変換
#ifdef WDDEBUG
{
FILE *fp;
fp=fopen("_windrv.txt", "a");
fprintf(fp, "Remove  %s\n", lpszSearchFile);
fclose(fp);
}
#endif
	return remove(lpszSearchFile);
}

//----------------------------------------------------------
// xopen(int name, int fcb)
//   ファイルオープン
//----------------------------------------------------------
int xopen(int name, int fcb)
{
	if (isopened(name)) return -33;
	if ( chk_file(name, fcb)<0 )
	{	//error
		return -2;
	}
	return 0;
}


//----------------------------------------------------------
// re_name(int oldname,int newname)
//   リネーム
//----------------------------------------------------------
int re_name(int oldname,int newname)
{
	char lpszSearchFileold[MAX_PATH];	// 検索するファイルの名前のアドレス
	char lpszSearchFilenew[MAX_PATH];	// 検索するファイルの名前のアドレス

	gen_file_name(lpszSearchFileold, oldname);	//namestsからpath+8.3形式へ変換
	gen_file_name(lpszSearchFilenew, newname);	//namestsからpath+8.3形式へ変換
#ifdef WDDEBUG
{
FILE *fp;
fp=fopen("_windrv.txt", "a");
fprintf(fp, "Rename  %s -> %s\n", lpszSearchFileold, lpszSearchFilenew);
fclose(fp);
}
#endif
	return rename(lpszSearchFileold, lpszSearchFilenew);
}


//----------------------------------------------------------
// xread(int buff, int size, int fcb)
//   オープン中のファイルのクローズ
//----------------------------------------------------------
int xclose(int fcb)
{
	int idx = get_path_idx(fcb);	// FCBからファイル名の記録をたどる。

	if (idx<0) return -1;	//見つからないのでなにもしない
	path_tbl[idx].plen=0;	//開放
	return 0;
}


//----------------------------------------------------------
// xread(int buff, int size, int fcb)
//   オープン中のファイルのリード
//----------------------------------------------------------
int xread(int buff, int size, int fcb)
{
	char pathfile[MAX_PATH], filename[32], temp;
	FILE *fp;
	int len=0, addlen, i, offset, idx;

#ifdef WDDEBUG
{
FILE *fp;
fp=fopen("_windrv.txt", "a");
fprintf(fp, "Read - Size:$%08X\n", size);
fclose(fp);
}
#endif
	if ((size<0) || (size>=0xfffffff))	//-1 is max size
	{
		idx=get_path_idx(fcb);	// FCBからファイル名の記録をたどる。
		if (idx<0)
			return -1;	//ファイルが見つからない。
		//ファイルをすべて。
		size = Memory_ReadD(fcb+XFCB_FILESIZE);
	}

//	if ( Memory_ReadD(buff) == 0 )
//		return -1;

	idx=get_path_idx(fcb);	// FCBからファイル名の記録をたどる。
	if (idx<0)
		return -1;	//ファイルが見つからなかった

//要long file name対応
	//ファイル名を合成する
	gen_file_FCB(filename, fcb);
	if (strcmp(path_tbl[idx].path_str,"\\")==0)
		sprintf(pathfile,"%c:\\%s",win_letter,filename);
	else
		sprintf(pathfile,"%c:%s\\%s",win_letter,path_tbl[idx].path_str,filename);
	chkdouble(pathfile);

	fp=fopen(pathfile,"rb");
	if (fp==NULL) return -1;

	//FCB内のファイルポインタを取り出す
	offset=Memory_ReadD(fcb+XFCB_FILEPTR);
	fseek(fp,offset,SEEK_SET);

	//ファイルからデータを読み出す
#ifdef WDDEBUG
{
FILE *fp;
fp=fopen("_windrv.txt", "a");
fprintf(fp, "ReadStart Size:%dbytes Offset:$%08X -> $%08X\n", size, offset, buff);
fclose(fp);
}
#endif
	for (i=0; i<size ; i++)
	{
		addlen = fread(&temp, 1, 1, fp);
		if (addlen)
		{
			len ++;
			Memory_WriteB(buff+i, temp);
		}
		else
			break;
	}
	fclose(fp);

	//FCB内のファイルポインタを更新する
	offset+=len;
	Memory_WriteD(fcb+XFCB_FILEPTR,offset);
	
	//読み出したデータの長さを返す。
	return len;
}


//----------------------------------------------------------
// xwrite(int buff, int size, int fcb)
//   オープン中のファイルのライト
//----------------------------------------------------------
int chkcount=0;

int xwrite(int buff, int size, int fcb)
{
	int idx, fsize;
	char pathfile[MAX_PATH];
	char filename[32];
	char temp;
	FILE *fp;
	int offset, len=0, i;

//	if ( Memory_ReadD(buff) == 0 )
//		return -1;

	idx = get_path_idx(fcb);	// FCBからファイル名の記録をたどる。
	if (idx < 0)
		return -1;	//該当するファイルが見つからない

//要long file name対応
//ファイル名を合成する。
	gen_file_FCB(filename, fcb);
	if (strcmp(path_tbl[idx].path_str,"\\")==0)
		sprintf(pathfile, "%c:\\%s",win_letter,filename);
	else
		sprintf(pathfile, "%c:%s\\%s",win_letter,path_tbl[idx].path_str,filename);
	chkdouble(pathfile);

	fp=fopen(pathfile, "r+b");
	if (fp==NULL) return -1;

	offset = Memory_ReadD(fcb+XFCB_FILEPTR);
	fseek(fp, offset, SEEK_SET);

#ifdef WDDEBUG
{
FILE *fp;
fp=fopen("_windrv.txt", "a");
fprintf(fp, "WriteStart Size:%dbytes Offset:$%08X <- $%08X\n", size, offset, buff);
fclose(fp);
}
#endif
	for (i=0; i<size ; i++)
	{
		temp = Memory_ReadB(buff+i);
		len += fwrite(&temp, 1, 1, fp);
	}
	fclose(fp);

	offset += len;
	Memory_WriteD(fcb+XFCB_FILEPTR, offset);
	fsize=Memory_ReadD(fcb+XFCB_FILESIZE);
	if (fsize<offset)
		Memory_WriteD(fcb+XFCB_FILESIZE, offset);
	return len;
}


//----------------------------------------------------------
// xseek(int mode,int offset,int fcb)
//   オープン中のファイルのシーク
//----------------------------------------------------------
int xseek(int mode,int offset,int fcb)
{
	unsigned char *pfcb = get_real(fcb);
	int idx = get_path_idx(fcb);	// FCBからファイル名の記録をたどる。
	int cur, max, prev;

	if (idx<0)
		return -1;	//該当するファイルがない

	cur = Memory_ReadD(fcb+XFCB_FILEPTR);
	max = Memory_ReadD(fcb+XFCB_FILESIZE);
	prev=cur;
	switch(mode)
	{
	case 0: //先頭から
		cur=offset;
		break;
	case 1: //現在位置から
		cur+=offset;
		break;
	case 2: //後ろから
		cur=max-abs(offset);
		break;
	}

	if (cur<0) return 0xffffffe7;
	else if (cur>max) 
		return 0xffffffe7;
	Memory_WriteD(fcb+XFCB_FILEPTR, cur);
	return cur;
}


//----------------------------------------------------------
// DPBを返す（未実装）
//----------------------------------------------------------
int get_dpb(int dpb)
{
	return -1;
}


//----------------------------------------------------------
// コマンド解析して各ルーチンを呼ぶ
//----------------------------------------------------------
void WinDrv_Command()
{
	int cmd = regs.d[0]&0xff;		//D0に機能番号
	int a5 = regs.a[5];			//A5にアドレス
	int res,unit,limit;

	gr_files = 0;
	regs.d[0] = 0;
	unit = Memory_ReadB(a5+1);
	if (unit < num_win_drv)
		win_letter = win_drvs[unit];

#ifdef WDDEBUG
if (cmd!=0x57) {
FILE *fp;
fp=fopen("_windrv.txt", "a");
fprintf(fp, "Command $%02X  A5:$%08X\n", cmd, a5);
fclose(fp);
}
#endif
	switch(cmd)
	{
		//init 初期化
	case 0x40: case 0xc0:
		init_path_tbl();		//テーブル初期化
		res = num_win_drv;
		limit = ('Z'+1-'A') - Memory_ReadB( a5+22 ); //ドライブレター上限Z
		if (res > limit)
			res = limit;
		Memory_WriteB( a5+13, (BYTE)res );
		break;

		//ディレクトリ検索
	case 0x41: case 0xc1:
		res = chk_dir( Memory_ReadD(a5+14) );	//ディレクトリを検索する。
		Memory_WriteD( a5+18, res );	//result status
		break;

		//ディレクトリ作成
	case 0x42: case 0xc2:
		res = make_dir( Memory_ReadD(a5+14) ); //ディレクトリを作成し結果を返す。
		Memory_WriteD( a5+18, res );	//result status
		break;

		//ディレクトリ削除
	case 0x43: case 0xc3:
		res = rm_dir( Memory_ReadD(a5+14) );	//ディレクトリを削除し結果を返す。
		Memory_WriteD( a5+18, res );	//result status
		break;

		//ファイル名変更
	case 0x44: case 0xc4:
		res = re_name( Memory_ReadD(a5+14), Memory_ReadD(a5+18) );
		Memory_WriteD( a5+18, res );	//result status
		break;

		//ファイル削除
	case 0x45: case 0xc5:
		res = rm_file( Memory_ReadD(a5+14) );	//result status
		Memory_WriteD( a5+18, res );	//result status
		break;

		//ファイル属性
	case 0x46: case 0xc6:
		res = file_atr( a5+13, Memory_ReadD(a5+14) );
		Memory_WriteD( a5+18, res );	//result status
		break;

		//FILES
	case 0x47: case 0xc7:
		res = files( Memory_ReadB(a5+13), Memory_ReadD(a5+14), Memory_ReadD(a5+18) );
		//if (res<0)
			Memory_WriteD( a5+18, res );	//result status
#ifdef WDDEBUG
{
FILE *fp;
fp=fopen("_windrv.txt", "a");
fprintf(fp, "Return from Files - Result:$%08X\n", Memory_ReadD(a5+18));
fclose(fp);
}
#endif
		break;

		//NFILES
	case 0x48: case 0xc8:
		res = nfiles( Memory_ReadD(a5+18) );
		//if (res<0)
			Memory_WriteD( a5+18, res );	//result status
#ifdef WDDEBUG
{
FILE *fp;
fp=fopen("_windrv.txt", "a");
fprintf(fp, "Return from NFiles - Result:$%08X\n", Memory_ReadD(a5+18));
fclose(fp);
}
#endif
		break;

		//ファイル作成
	case 0x49: case 0xc9:
		res = xcreate( Memory_ReadD(a5+14), Memory_ReadD(a5+22),
			       Memory_ReadB(a5+13), Memory_ReadD(a5+18) );
		Memory_WriteD( a5+18, res );	//result status
		break;

		//ファイルオープン
	case 0x4a: case 0xca:
#ifdef WDDEBUG
{
FILE *fp;
fp=fopen("_windrv.txt", "a");
fprintf(fp, "Open mode:$%02X\n", Memory_ReadD(a5+18));
fclose(fp);
}
#endif
		res = xopen( Memory_ReadD(a5+14), Memory_ReadD(a5+22) );
		Memory_WriteD( a5+18, res );	//result status
		break;

		//ファイルクローズ
	case 0x4b: case 0xcb:
		res = xclose( Memory_ReadD(a5+22) );
		Memory_WriteD( a5+18, res );	//result status
		break;

		//ファイル読み込み
	case 0x4c: case 0xcc:
		res = xread( Memory_ReadD(a5+14), Memory_ReadD(a5+18), Memory_ReadD(a5+22) );
		Memory_WriteD( a5+18, res );	//result status
		break;

		//ファイル書き込み
	case 0x4d: case 0xcd:
		res = xwrite( Memory_ReadD(a5+14), Memory_ReadD(a5+18), Memory_ReadD(a5+22) );
		Memory_WriteD( a5+18, res );	//result status
		break;

		//ファイルシーク
	case 0x4e: case 0xce:
		res = xseek( Memory_ReadB(a5+13), Memory_ReadD(a5+18), Memory_ReadD(a5+22) );
		Memory_WriteD( a5+18, res );	//result status
		break;

		//ファイル更新時刻
	case 0x4f: case 0xcf:
		res = file_time( a5+18, Memory_ReadD(a5+22) );
		Memory_WriteD( a5+18, res );	//result status
		break;

		//容量取得
	case 0x50: case 0xd0:
		res = get_space( Memory_ReadD(a5+14) );
		Memory_WriteD( a5+18, res );	//result status
		break;

		//ドライブ制御
	case 0x51: case 0xd1:
		if (Memory_ReadB(a5+13)==0)
		{
			Memory_WriteB(a5+13, 0x42);	//
		}
		break;

		//DPB取得
	case 0x52: case 0xd2:
		res = get_dpb( Memory_ReadD(a5+14)-2 );
		Memory_WriteD( a5+18, res );	//result status
		break;

		//IOCTRL入力
	case 0x53: case 0xd3:
		break;

		//IOCTRL出力
	case 0x54: case 0xd4:
		break;

		//特殊IOCTRL
	case 0x55: case 0xd5:
		break;

		//アボート
	case 0x56: case 0xd6:
		Memory_WriteD( a5+18, 0 );	//result status
		break;

		//メディア交換検査
	case 0x57: case 0xd7:
		Memory_WriteD( a5+18, 0 );	//result status
		break;

		//排他制御
	case 0x58: case 0xd8:
		Memory_WriteD( a5+18, 0 );	//result status
		break;
	default:
		break;
	}
}


//------------------------------------------------------
// I/O部（$e9f000〜$e9ffff）
//------------------------------------------------------

BYTE FASTCALL WinDrv_Read(DWORD adr)
{
	if (adr>=0xe9f800)
		return CDROM_Read(adr);		//SCSI IOCS
	if (adr>=0xe9f000) return 'W';		//Windrv 確認用ID
	BusErrFlag = 1;				// $E9E000〜E9EFFFの範囲はコプロ（LVNView2）
	return 0;
}

void FASTCALL WinDrv_Write(DWORD adr, BYTE data)
{
	if (adr>=0xe9f800)
		CDROM_Write(adr, data);
	else if (adr>=0xe9f000)
		WinDrv_Command();
	else
		BusErrFlag = 2;			// $E9E000〜E9EFFFの範囲はコプロ（LVNView2）
}

void WinDrv_Init()
{
	char drvstr[128];
	int i,j;
	int reject=0;
	DWORD mask;

	strcpy(drvstr,"A:\\");
	mask = GetLogicalDrives();	//ドライブのパターンを取り出す。

	j=0;
	//AからZ
	for(i=0; i<26; i++)
	{
		if (mask&(1<<i))
		{
			drvstr[0]='A'+i;
			if ((reject<2) &&
				(Config.WinDrvFD!=0) &&
				(GetDriveType(drvstr)==DRIVE_REMOVABLE))
			{
				reject++;	//最初の２つのリモートドライブはFDD
				continue;
			}
			reject=2;		//FDDの検出は終了
			win_drvs[j++]='A'+i;	//Windowsのドライブレターを登録する。
		}
	}

	win_drvs[j]=0;
	num_win_drv=j;		//登録したドライブ数を設定する。
}


