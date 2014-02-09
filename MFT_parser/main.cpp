#include "../NTFSLib/NTFS.h"
#include <stdio.h>
#include <windows.h>
#include "time.h"
#include <iostream>
#include <Setupapi.h>
#include <Ntddstor.h>
#include <iostream>
#include <string>
//#include <string.h>


#pragma comment( lib, "setupapi.lib" )

using namespace std;

unsigned int filetime_to_microseconds(const FILETIME& ft)
{
    // FILETIME �� 1601-Jan-01 �������� 10,000,000 ���� 1��(100 ������) �� ��ϵ�
    union
    {
        FILETIME asFileTime ;
        unsigned int asInt64 ;
    } myFileTime;
 
    myFileTime.asFileTime = ft;
    myFileTime.asInt64 -= 116444736000000000ULL; // 1970-Jan-01 ���� ���� ��ȯ
    return (myFileTime.asInt64 / 10); // microseconds �� ��ȯ
}

struct mftstruct{
	ULONGLONG entry;
	ULONGLONG ParentRef;
	char FILENAME[MAX_PATH]; 
	char FULLPATH[MAX_PATH];
	unsigned int SI_writeTm, SI_createTm, SI_accessTm, SI_mftTm;
	unsigned int FN_writeTm, FN_createTm, FN_accessTm, FN_mftTm;
};
typedef struct mftstruct MFTSTRUCT;

ULONGLONG entry_count;

int totalfiles = 0;
int totaldirs = 0;

char fullPath_[MAX_PATH];
char root[10] = "$ROOT";
char BackSlash[MAX_PATH] = "\\";


char *getFullPath(int entry, MFTSTRUCT *mftStruct, int saved_entry)
{
	//static char buff[MAX_PATH] = "";

	if ( entry == 5 )
	{	
		return root;
	}

	if ( entry != 5 )
	{
		//sprintf(mftStruct[saved_entry].FULLPATH, "\\%s", getFullPath(mftStruct[entry].ParentRef, mftStruct, saved_entry));

		strcat(mftStruct[saved_entry].FULLPATH,  getFullPath(mftStruct[entry].ParentRef, mftStruct, saved_entry));
		char BackSlash[MAX_PATH] = "\\";
		strcat(BackSlash, mftStruct[entry].FILENAME);
		//return mftStruct[entry].FILENAME;
		return BackSlash;
	}
}

void printStruct(MFTSTRUCT *mftStruct)
{
	for(int i=303 ;i<404 ; i++)
	//for(int i=0 ;i<entry_count ; i++)
	{
		//printf(" [i] entry values\n ");
		printf(" FILENAME = %s\n", mftStruct[i].FILENAME);
		//printf(" W_TIME = %u\n", mftStruct[i].FN_accessTm);
		//printf(" A_TIME = %u\n", mftStruct[i].FN_createTm);
		//printf(" C_TIME = %u\n", mftStruct[i].FN_mftTm);
		//printf(" Entry Num = %u\n", mftStruct[i].entry);
		//printf(" ParentReg Num = %u\n\n", mftStruct[i].ParentRef);
	}
}


void usage()
{
	printf("\n# Invalid parameter\n");
	printf("# Usage: mftparset.exe c:\n");
}

// get volume name 'C', 'D', ...
// *ppath -> "c:\program files\common files"
char getvolume(char **ppath)
{
	char *p = *ppath;
	char volname;

	// skip leading blank and "
	while (*p)
	{
		if (*p == ' ' || *p == '"')
			p++;
		else
			break;
	}
	if (*p == '\0')
		return '\0';
	else
	{
		volname = *p;
		p++;
	}

	// skip blank
	while(*p)
	{
		if (*p == ' ')
			p++;
		else
			break;
	}
	if (*p == '\0')
		return '\0';

	if (*p != ':')
		return '\0';

	// forward to '\' or string end
	while (*p)
	{
		if (*p != '\\')
			p++;
		else
			break;
	}
	// forward to not '\' and not ", or string end
	while (*p)
	{
		if (*p == '\\' || *p == '"')
			p++;
		else
			break;
	}

	*ppath = p;
	return volname;
}

int main(int argc, char *argv[])
{
	clock_t start, end; // ���α׷� ���� �ð� ���� ��

	if (argc != 2)
	{
		//usage();
	}
	
	printf("# The Physical Drives of this Machine : \n");
	//getPhysicalDrive();
	system("wmic diskdrive get Caption, Name");
	printf("\n");

	printf("# The Logical Drives of this Machine : \n");
	system("wmic logicaldisk get Description, Name, FileSystem, SystemName");


		////////////////////////////////////// �м��� ����̺� �Է�.

	char Name[200];
	printf("\n# Enter the drive Name : ");
	scanf("%200s", Name);
	char *path = Name;

	char volname;
	volname = getvolume(&path); // Double Pointer !

	if (!volname)
	{
		usage();
		return -1;
	}

	CNTFSVolume volume(volname);
	if (!volume.IsVolumeOK())
	{
		printf("Cannot get NTFS BPB from boot sector of volume %c\n", volname);
		return -1;
	}


	entry_count = volume.GetRecordsCount();	// ULONGLONG GetRecordsCount() const
	printf("MFT Records Count = %d\n\n", entry_count);

	// MFT ������ ����Ǵ� ����ü �迭 �����Ҵ�.
	MFTSTRUCT *u3; 
	//u3 = (MFTSTRUCT *)malloc(sizeof(MFTSTRUCT) * entry_count);
	u3 = (MFTSTRUCT *)calloc(entry_count, sizeof(MFTSTRUCT));
	if(u3 ==NULL){
		puts("Malloc Failed...");
		exit(1);
	}


	ULONGLONG  mft_addr;
	mft_addr = volume.GetMFTAddr();
	// printf("$MFT metafile start Adderss = %p\n\n\n", mft_addr);
	// Relative start address of the $MFT metafile. Get from BPB.
	
	//////////////////////////////////////////////////////////////////////////////
	// �м��� ����̺� ���� �� ���� �������� ���������.

	// get root directory info
	//.(Root Directory)�� ������ ��Ʈ���͸��� �ǹ��ϴ� ������ 
	// ���͸� ������ ������ ������ �����ϱ� ���� INDEX ������ ����ȴ�.
	
	CFileRecord fr(&volume);

	// we only need INDEX_ROOT and INDEX_ALLOCATION
	// don't waste time and ram to parse unwanted attributes
	//fr.SetAttrMask( MASK_INDEX_ROOT | MASK_INDEX_ALLOCATION);

	fr.SetAttrMask(MASK_STANDARD_INFORMATION | MASK_FILE_NAME);

	//------------------------------------------------------------------------
	if (!fr.ParseFileRecord(MFT_IDX_ROOT))	// ������ ��Ʈ ���丮 
	{
		printf("Cannot read root directory of volume %c\n", volname);
		return -1;
	}

	if (!fr.ParseAttrs())
	{
		printf("Cannot parse attributes\n");
		return -1;
	}
	//---------------------------------------------- MFT ��Ʈ ���丮 �Ľ�

	CIndexEntry ie;
	
	// ������ MACŸ�� ���� ����.
	FILETIME SI_writeTm;
	FILETIME SI_createTm;
	FILETIME SI_accessTm;
	FILETIME SI_mftTm;

	FILETIME FN_writeTm;
	FILETIME FN_createTm;
	FILETIME FN_accessTm;
	FILETIME FN_mftTm;


	char fn[MAX_PATH];
	
	start = clock(); // ���α׷� ���� �ð� ���� (��ü MFT Entry ���ڵ� �ð�)

	//for(int i=303 ;i<404 ; i++)
	for(int i=0 ;i<entry_count ; i++)
	{
		fr.ParseFileRecord(i);

		if (!fr.ParseAttrs())	// <--- �ش� ���� ��Ʈ�� �Ľ�.
		{
			//printf("Entry NUM %d Cannot parse attributes\n", i);
			continue;
		}

		int fnlen = fr.GetFileName(fn, MAX_PATH);
		if (fnlen > 0)
		{
			fr.GetFileTime(&SI_writeTm, &SI_createTm, &SI_accessTm, &SI_mftTm, &FN_writeTm, &FN_createTm, &FN_accessTm, &FN_mftTm);

	
			unsigned int SI_writeTm_s;
			unsigned int SI_createTm_s;
			unsigned int SI_accessTm_s;
			unsigned int SI_mftTm_s;

			unsigned int FN_writeTm_s;
			unsigned int FN_createTm_s;
			unsigned int FN_accessTm_s;
			unsigned int FN_mftTm_s;

			/*
			FileTimeToSystemTime(&SI_writeTm, &SI_writeTm_s); 
			FileTimeToSystemTime(&SI_createTm, &SI_createTm_s);
			FileTimeToSystemTime(&SI_accessTm, &SI_accessTm_s);
			FileTimeToSystemTime(&SI_mftTm, &SI_mftTm_s);

			FileTimeToSystemTime(&FN_writeTm, &FN_writeTm_s); 
			FileTimeToSystemTime(&FN_createTm, &FN_createTm_s);
			FileTimeToSystemTime(&FN_accessTm, &FN_accessTm_s);
			FileTimeToSystemTime(&FN_mftTm, &FN_mftTm_s);
			*/

			SI_writeTm_s = filetime_to_microseconds(SI_writeTm);
			SI_createTm_s = filetime_to_microseconds(SI_createTm);
			SI_accessTm_s = filetime_to_microseconds(SI_accessTm);
			SI_mftTm_s = filetime_to_microseconds(SI_mftTm);

			FN_writeTm_s = filetime_to_microseconds(FN_writeTm);
			FN_createTm_s = filetime_to_microseconds(FN_createTm);
			FN_accessTm_s = filetime_to_microseconds(FN_accessTm);
			FN_mftTm_s = filetime_to_microseconds(FN_mftTm);

			if (fr.IsDirectory())
				totaldirs ++;
			else
				totalfiles ++;

			// ����ü �迭�� �� ����.
			strcpy(u3[i].FILENAME, fn);

			u3[i].SI_writeTm = SI_writeTm_s;
		    u3[i].SI_createTm = SI_createTm_s;
			u3[i].SI_accessTm = SI_accessTm_s;
			u3[i].SI_mftTm = SI_mftTm_s;
			
			u3[i].FN_writeTm = FN_writeTm_s;
		    u3[i].FN_createTm = FN_createTm_s;
			u3[i].FN_accessTm = FN_accessTm_s;
			u3[i].FN_mftTm = FN_mftTm_s;

			u3[i].entry = i;
			u3[i].ParentRef = fr.GetParentRef();

			
		
			if (1) // ȭ�鿡 ��� �ϳ� ���ϳ� ����   0 = ��¾��� / 1 = �����
			{
				printf("************************************************************\n\n");
				printf("Current MFT Entry NUM : %u\n", i);
				printf("MFT Parent Reference : %u\n", fr.GetParentRef());

				printf("SI_WRITE TIME : %d\n", SI_writeTm_s);
				printf("SI_CREATE TIME : %d\n", SI_createTm_s);
				printf("SI_ACCESS TIME : %d\n", SI_accessTm_s);
				printf("SI_MFT TIME : %d\n", SI_mftTm_s);

				printf("FN_WRITE TIME : %d\n", FN_writeTm_s);
				printf("FN_CREATE TIME : %d\n", FN_createTm_s);
				printf("FN_ACCESS TIME : %d\n", FN_accessTm_s);
				printf("FN_MFT TIME : %d\n", FN_mftTm_s);

				printf("File TYPE : \t%s", fr.IsDirectory()?"<DIR>\n":"<FILE>\n");

			   if (!fr.IsDirectory())
					printf("filesize = %I64u\n", fr.GetFileSize());
			   else
					printf("\t");

			   printf("<%c%c%c%c>\t%s\n", fr.IsReadOnly()?'R':' ',
				   fr.IsHidden()?'H':' ', fr.IsSystem()?'S':' ',fr.IsDeleted()?'D':' ',fn);

			  //printf("filename = %s\n", fn);
			}
		}
		
	}

	// ����ü�� ����� ���� ��� �Լ�.
	//printStruct(u3);

	// �� entry�� ��ü��θ� ���Ѵ�.
	//for(int k=303 ;k<404 ; k++)
	for (int k = 0 ; k < entry_count ; k++)
	{
		getFullPath(k, u3, k);
		
		printf( "entry %d = %s\n\n", k, u3[k].FULLPATH ); 
	}


	end = clock();
	printf("\n##### ��ü �ҿ�ð� : %5.2f�� #####\n", (float)(end-start)/CLOCKS_PER_SEC);

	printf("Files: %d, Directories: %d\n", totalfiles, totaldirs);

	return 0;
}
