#include <windows.h>
#include <string>
#include <stdio.h>

bool IsNumber (LPCWSTR val)
{
	while (*val)
	{
		if (!iswdigit (*val))
			return false;
		val++;
	}

	return true;
}

int wmain(int argc, wchar_t* argv[])
{
	HANDLE hDevice;
	WCHAR Path[MAX_PATH];
	DISK_GEOMETRY_EX geometry;
	PARTITION_INFORMATION_EX info;
	DWORD BytesReturned = 0;

	if((argc != 2) || !IsNumber (argv[1]))
	{
		printf("Usage: GetDiskInfo <PhysicalDriveNo>\n\n");
		return 0;
	}

	wsprintf(Path, L"\\\\.\\PhysicalDrive%s", argv[1]);

	wprintf(L"\nInformation for disk %s\n\n", Path);

	if((hDevice=CreateFileW(Path, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0))==INVALID_HANDLE_VALUE)
	{
		wprintf(L"CreateFileW faield with error 0x%.8X\n", GetLastError());
		return -1;
	}

	if(DeviceIoControl(hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &geometry, sizeof(geometry), &BytesReturned, (LPOVERLAPPED) NULL))
	{
		printf("Disk Properties : \n\n");
		printf("Cylinders       = %I64d\n", geometry.Geometry.Cylinders);
		printf("Tracks/cylinder = %ld\n", (ULONG) geometry.Geometry.TracksPerCylinder);
		printf("Sectors/track   = %ld\n", (ULONG) geometry.Geometry.SectorsPerTrack);
		printf("Bytes/sector    = %ld\n", (ULONG) geometry.Geometry.BytesPerSector);
		printf("Media Type      = %s\n", geometry.Geometry.MediaType == RemovableMedia? "Removable" : "Fixed");
		printf("Disk size       = %I64d (Bytes) = %I64d (Gb)\n", geometry.DiskSize.QuadPart, geometry.DiskSize.QuadPart / (1024 * 1024 * 1024));
		printf("\n\n");
		printf("List of Partitions:\n\n");		

		printf("Number    Type    Start Offset        Length    \n");
		printf("======    ====    =============    =============\n"); 

		for(int i=1; i<=256; ++i)
		{
			HANDLE hPart;

			wsprintf(Path, L"\\\\?\\GLOBALROOT\\Device\\Harddisk%s\\Partition%d", argv[1], i);
			if((hPart=CreateFileW(Path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,0,0))==INVALID_HANDLE_VALUE)
			{
				if (GetLastError() == ERROR_FILE_NOT_FOUND)
					break;
				printf("%6i    %4s    %13I64d    %13I64d\n", i, "ERROR", -1, -1);
				continue;
			}

			if(DeviceIoControl(hPart,IOCTL_DISK_GET_PARTITION_INFO_EX,NULL,0,&info,sizeof(info),&BytesReturned,(LPOVERLAPPED)NULL))
			{
				printf("%6i    %4s    %13I64d    %13I64d\n", 
					info.PartitionNumber, 
					info.PartitionStyle == PARTITION_STYLE_MBR? "MBR" : info.PartitionStyle == PARTITION_STYLE_GPT? "GPT" : "RAW", 
					info.StartingOffset.QuadPart,
					info.PartitionLength.QuadPart);
			}

			CloseHandle (hPart);
		}
	}
	else
	{
		wprintf(L"DeviceIoControl(IOCTL_DISK_GET_DRIVE_GEOMETRY_EX) faield with error 0x%.8X\n", GetLastError());
	}

	CloseHandle(hDevice);

	return 0;
}

