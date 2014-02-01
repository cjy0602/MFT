#include "../NTFSLib/NTFS.h"
#include <stdio.h>
#include <windows.h>
#include "time.h"
#include <iostream>
#include <Setupapi.h>
#include <Ntddstor.h>
#include <string>

#pragma comment( lib, "setupapi.lib" )

using namespace std;

struct mftstruct{
	ULONGLONG entry;
	ULONGLONG ParentRef;
	char FILENAME[100]; 
	SYSTEMTIME SI_writeTm, SI_createTm, SI_accessTm, SI_mftTm;
	SYSTEMTIME FN_writeTm, FN_createTm, FN_accessTm, FN_mftTm;
};
typedef struct mftstruct MFTSTRUCT;

int totalfiles = 0;
int totaldirs = 0;

#define START_ERROR_CHK()           \
    DWORD error = ERROR_SUCCESS;    \
    DWORD failedLine;               \
    string failedApi;

#define CHK( expr, api )            \
    if ( !( expr ) ) {              \
        error = GetLastError( );    \
        failedLine = __LINE__;      \
        failedApi = ( api );        \
        goto Error_Exit;            \
    }

#define END_ERROR_CHK()             \
    error = ERROR_SUCCESS;          \
    Error_Exit:                     \
    if ( ERROR_SUCCESS != error ) { \
        cout << failedApi << " failed at " << failedLine << " : Error Code - " << error << endl;    \
    }

int getPhysicalDrive() {

    HDEVINFO diskClassDevices;
    GUID diskClassDeviceInterfaceGuid = GUID_DEVINTERFACE_DISK;
    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA deviceInterfaceDetailData;
    DWORD requiredSize;
    DWORD deviceIndex;

    HANDLE disk = INVALID_HANDLE_VALUE;
    STORAGE_DEVICE_NUMBER diskNumber;
    DWORD bytesReturned;

    START_ERROR_CHK();

    //
    // Get the handle to the device information set for installed
    // disk class devices. Returns only devices that are currently
    // present in the system and have an enabled disk device
    // interface.
    //
    diskClassDevices = SetupDiGetClassDevs( &diskClassDeviceInterfaceGuid,
                                            NULL,
                                            NULL,
                                            DIGCF_PRESENT |
                                            DIGCF_DEVICEINTERFACE );
    CHK( INVALID_HANDLE_VALUE != diskClassDevices,
         "SetupDiGetClassDevs" );

    ZeroMemory( &deviceInterfaceData, sizeof( SP_DEVICE_INTERFACE_DATA ) );
    deviceInterfaceData.cbSize = sizeof( SP_DEVICE_INTERFACE_DATA );
    deviceIndex = 0;

    while ( SetupDiEnumDeviceInterfaces( diskClassDevices,
                                         NULL,
                                         &diskClassDeviceInterfaceGuid,
                                         deviceIndex,
                                         &deviceInterfaceData ) ) {

        ++deviceIndex;

        SetupDiGetDeviceInterfaceDetail( diskClassDevices,
                                         &deviceInterfaceData,
                                         NULL,
                                         0,
                                         &requiredSize,
                                         NULL );
        CHK( ERROR_INSUFFICIENT_BUFFER == GetLastError( ),
             "SetupDiGetDeviceInterfaceDetail - 1" );

        deviceInterfaceDetailData = ( PSP_DEVICE_INTERFACE_DETAIL_DATA ) malloc( requiredSize );
        CHK( NULL != deviceInterfaceDetailData,
             "malloc" );

        ZeroMemory( deviceInterfaceDetailData, requiredSize );
        deviceInterfaceDetailData->cbSize = sizeof( SP_DEVICE_INTERFACE_DETAIL_DATA );

        CHK( SetupDiGetDeviceInterfaceDetail( diskClassDevices,
                                              &deviceInterfaceData,
                                              deviceInterfaceDetailData,
                                              requiredSize,
                                              NULL,
                                              NULL ),
             "SetupDiGetDeviceInterfaceDetail - 2" );

        disk = CreateFile( deviceInterfaceDetailData->DevicePath,
                           GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL,
                           NULL );
        CHK( INVALID_HANDLE_VALUE != disk,
             "CreateFile" );

        CHK( DeviceIoControl( disk,
                              IOCTL_STORAGE_GET_DEVICE_NUMBER,
                              NULL,
                              0,
                              &diskNumber,
                              sizeof( STORAGE_DEVICE_NUMBER ),
                              &bytesReturned,
                              NULL ),
             "IOCTL_STORAGE_GET_DEVICE_NUMBER" );

        CloseHandle( disk );
        disk = INVALID_HANDLE_VALUE;

        cout << deviceInterfaceDetailData->DevicePath << endl;
        cout << "\\\\?\\PhysicalDrive" << diskNumber.DeviceNumber << endl;
        cout << endl;
    }
    CHK( ERROR_NO_MORE_ITEMS == GetLastError( ),
         "SetupDiEnumDeviceInterfaces" );

    END_ERROR_CHK();

Exit:

    if ( INVALID_HANDLE_VALUE != diskClassDevices ) {
        SetupDiDestroyDeviceInfoList( diskClassDevices );
    }

    if ( INVALID_HANDLE_VALUE != disk ) {
        CloseHandle( disk );
    }

    return error;
}

void pause(void) {
  printf("Press any key to continue . . .");
  getchar();  // �ƹ� Ű�� 1�� �Է� �ޱ�
  puts(""); // �ٹٲ�
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

void printfile(const CIndexEntry *ie) // ȣ�� X
{


	// Hide system metafiles
	if (ie->GetFileReference() < MFT_IDX_USER)
	{
		printf("MFT Metadata : Entry < 15\n");
		return;
	}

	// Ignore DOS alias file names
	if (!ie->IsWin32Name())
		return;

	FILETIME ft;
	char fn[MAX_PATH];
	int fnlen = ie->GetFileName(fn, MAX_PATH);
	if (fnlen > 0)
	{
		ie->GetFileTime(&ft);
		SYSTEMTIME st;
		if (FileTimeToSystemTime(&ft, &st))
		{
			printf("%d-%02d-%02d  %02d:%02d\t%s    ", st.wYear, st.wMonth, st.wDay,
				st.wHour, st.wMinute, ie->IsDirectory()?"<DIR>":"     ");

			if (!ie->IsDirectory())
				printf("%I64u\t", ie->GetFileSize());
			else
				printf("\t");

			printf("<%c%c%c>\t%s\n", ie->IsReadOnly()?'R':' ',
				ie->IsHidden()?'H':' ', ie->IsSystem()?'S':' ', fn);
		}

		if (ie->IsDirectory())
			totaldirs ++;
		else
			totalfiles ++;
	}
}

int main(int argc, char *argv[])
{
	clock_t start, end; // ���α׷� ���� �ð� ���� ��

	DWORD mydrives = 100;// buffer length
	char lpBuffer[100];// buffer for drive string storage
	
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

	ULONGLONG entry_count;
	entry_count = volume.GetRecordsCount();	// ULONGLONG GetRecordsCount() const
	printf("MFT Records Count = %d\n\n", entry_count);

	// MFT ������ ����Ǵ� ����ü �迭 �����Ҵ�.
	MFTSTRUCT *u3; 
	u3 = (MFTSTRUCT *)malloc(sizeof(MFTSTRUCT) * entry_count);
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

	//for(int i=30 ;i<40 ; i++)
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

			SYSTEMTIME SI_writeTm_s;
			SYSTEMTIME SI_createTm_s;
			SYSTEMTIME SI_accessTm_s;
			SYSTEMTIME SI_mftTm_s;

			SYSTEMTIME FN_writeTm_s;
			SYSTEMTIME FN_createTm_s;
			SYSTEMTIME FN_accessTm_s;
			SYSTEMTIME FN_mftTm_s;
			
			FileTimeToSystemTime(&SI_writeTm, &SI_writeTm_s); 
			FileTimeToSystemTime(&SI_createTm, &SI_createTm_s);
			FileTimeToSystemTime(&SI_accessTm, &SI_accessTm_s);
			FileTimeToSystemTime(&SI_mftTm, &SI_mftTm_s);

			FileTimeToSystemTime(&FN_writeTm, &FN_writeTm_s); 
			FileTimeToSystemTime(&FN_createTm, &FN_createTm_s);
			FileTimeToSystemTime(&FN_accessTm, &FN_accessTm_s);
			FileTimeToSystemTime(&FN_mftTm, &FN_mftTm_s);


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

				printf("SI_WRITE TIME : %d-%02d-%02d  %02d:%02d\t\n", SI_writeTm_s.wYear, SI_writeTm_s.wMonth, SI_writeTm_s.wDay,
									SI_writeTm_s.wHour, SI_writeTm_s.wMinute);
				printf("SI_CREATE TIME : %d-%02d-%02d  %02d:%02d\t\n", SI_createTm_s.wYear, SI_createTm_s.wMonth, SI_createTm_s.wDay,
									SI_createTm_s.wHour, SI_createTm_s.wMinute);
				printf("SI_ACCESS TIME : %d-%02d-%02d  %02d:%02d\t\n", SI_accessTm_s.wYear, SI_accessTm_s.wMonth, SI_accessTm_s.wDay,
									SI_accessTm_s.wHour, SI_accessTm_s.wMinute);
				printf("SI_MFT TIME : %d-%02d-%02d  %02d:%02d\t\n", SI_mftTm_s.wYear, SI_mftTm_s.wMonth, SI_mftTm_s.wDay,
									SI_mftTm_s.wHour, SI_mftTm_s.wMinute);

				printf("FN_WRITE TIME : %d-%02d-%02d  %02d:%02d\t\n", FN_writeTm_s.wYear, FN_writeTm_s.wMonth, FN_writeTm_s.wDay,
									FN_writeTm_s.wHour, FN_writeTm_s.wMinute);
				printf("FN_CREATE TIME : %d-%02d-%02d  %02d:%02d\t\n", FN_createTm_s.wYear, FN_createTm_s.wMonth, FN_createTm_s.wDay,
									FN_createTm_s.wHour, FN_createTm_s.wMinute);
				printf("FN_ACCESS TIME : %d-%02d-%02d  %02d:%02d\t\n", FN_accessTm_s.wYear, FN_accessTm_s.wMonth, FN_accessTm_s.wDay,
									FN_accessTm_s.wHour, FN_accessTm_s.wMinute);
				printf("FN_MFT TIME : %d-%02d-%02d  %02d:%02d\t\n", FN_mftTm_s.wYear, FN_mftTm_s.wMonth, FN_mftTm_s.wDay,
									FN_mftTm_s.wHour, FN_mftTm_s.wMinute);


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

	end = clock();
	printf("\n##### ��ü �ҿ�ð� : %5.2f�� #####\n", (float)(end-start)/CLOCKS_PER_SEC);

	/*
	for(int i=0 ;i<entry_count ; i++)
	{
		printf(" [i] entry values\n ");
		printf(" FILENAME = %s\n", u3[i].FILENAME);
		printf(" W_TIME = %u\n", u3[i].W_TIME);
		printf(" A_TIME = %u\n", u3[i].A_TIME);
		printf(" C_TIME = %u\n", u3[i].C_TIME);
		printf(" Entry Num = %u\n", u3[i].entry);
		printf(" ParentReg Num = %u\n\n", u3[i].ParentRef);
	}
	*/

	// list it !
	// fr.TraverseSubEntries(printfile);  // CallBack �Լ� ��� !!!!!
	printf("Files: %d, Directories: %d\n", totalfiles, totaldirs);

	return 0;
}
