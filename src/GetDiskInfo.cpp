#include <windows.h>
#include <winioctl.h>
#include <initguid.h>
#include <diskguid.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <ctype.h>

#define IOCTL_VOLUME_IS_DYNAMIC                 CTL_CODE(IOCTL_VOLUME_BASE, 18, METHOD_BUFFERED, FILE_ANY_ACCESS)

//
// Returns true if the string consists solely of digits.
//
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

//
// Returns true if the character is a valid drive letter (A-Z or a-z).
//
bool isDriveLetter(wchar_t c)
{
	return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z');
}

//
// Given a physical drive number, build the device path (for example, "\\.\PhysicalDrive0").
//
void BuildPhysicalDrivePath(int driveNumber, wchar_t* devicePath, size_t devicePathSize)
{
    swprintf(devicePath, devicePathSize / sizeof(wchar_t), L"\\\\.\\PhysicalDrive%d", driveNumber);
}

//
// Given a drive letter, build the device path (for example, "\\.\C:").
//
void BuildDrivePath(wchar_t driveLetter, wchar_t* devicePath, size_t devicePathSize)
{
    swprintf(devicePath, devicePathSize / sizeof(wchar_t), L"\\\\.\\%c:", driveLetter);
}

int wmain(int argc, wchar_t* argv[])
{
    HANDLE hPhysical = INVALID_HANDLE_VALUE;
    WCHAR devicePath[MAX_PATH] = { 0 };
    DISK_GEOMETRY_EX geometry = { 0 };
    DWORD BytesReturned = 0;
    int driveNumber = -1;

    if (argc != 2)
    {
        wprintf(L"Usage: %s <PhysicalDriveNo or DriveLetter>\n", argv[0]);
        return -1;
    }

    //
    // If the argument is numeric, assume it's a physical drive number.
    // Otherwise, assume it's a drive letter.
    //
    if (IsNumber(argv[1]))
    {
        driveNumber = _wtoi(argv[1]);
        BuildPhysicalDrivePath(driveNumber, devicePath, sizeof(devicePath));
    }
    else
    {
		// check if the argument is a drive letter (e.g. C: or C)
		// if should be either a single letter (A-Z or a-z) or a letter followed by a colo
		// if it is valid, build the device path
		if (    (wcslen(argv[1]) == 1 || (wcslen(argv[1]) == 2 && argv[1][1] == L':'))
			&& (isDriveLetter(argv[1][0]))
            )
		{
			BuildDrivePath(towupper(argv[1][0]), devicePath, sizeof(devicePath));
		}
		else
		{
			wprintf(L"Invalid argument: %s\n", argv[1]);
			return -1;
		}
    }


    wprintf(L"\nInformation for device %s\n\n", devicePath);

    //
    // Open the device.
    //
    hPhysical = CreateFileW(devicePath,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);
    if (hPhysical == INVALID_HANDLE_VALUE)
    {
        wprintf(L"CreateFileW failed with error 0x%.8X\n", GetLastError());
        return -1;
    }

    //
    // Query and display disk geometry.
    //
    if (DeviceIoControl(hPhysical,
        IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
        NULL,
        0,
        &geometry,
        sizeof(geometry),
        &BytesReturned,
        NULL))
    {
        printf("Disk Properties:\n\n");
        printf("Cylinders       = %I64d\n", geometry.Geometry.Cylinders.QuadPart);
        printf("Tracks/cylinder = %ld\n", (ULONG)geometry.Geometry.TracksPerCylinder);
        printf("Sectors/track   = %ld\n", (ULONG)geometry.Geometry.SectorsPerTrack);
        printf("Bytes/sector    = %ld\n", (ULONG)geometry.Geometry.BytesPerSector);
        printf("Media Type      = %s\n",
            (geometry.Geometry.MediaType == RemovableMedia) ? "Removable" : "Fixed");
        printf("Disk size       = %I64d (Bytes) = %I64d (Gb)\n",
            geometry.DiskSize.QuadPart,
            geometry.DiskSize.QuadPart / (1024LL * 1024LL * 1024LL));
        printf("\n\n");
    }
    else
    {
        wprintf(L"IOCTL_DISK_GET_DRIVE_GEOMETRY_EX failed, error: 0x%.8X\n", GetLastError());
        CloseHandle(hPhysical);
        return -1;
    }

    //
    // Retrieve and display drive layout.
    //
    {
        BYTE layoutBuffer[4096] = { 0 };
        if (DeviceIoControl(hPhysical,
            IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
            NULL,
            0,
            layoutBuffer,
            sizeof(layoutBuffer),
            &BytesReturned,
            NULL))
        {
            DRIVE_LAYOUT_INFORMATION_EX* pLayout = (DRIVE_LAYOUT_INFORMATION_EX*)layoutBuffer;
            printf("Drive Layout:\n");
            printf("-----------------------------------------------------------\n");

            if (pLayout->PartitionStyle == PARTITION_STYLE_MBR)
            {
                printf("Partition Style: MBR\n");
                printf("Signature      : 0x%08X\n", pLayout->Mbr.Signature);
				printf("Checksum       : 0x%08X\n", pLayout->Mbr.CheckSum);

            }
            else if (pLayout->PartitionStyle == PARTITION_STYLE_GPT)
            {
                printf("Partition Style: GPT\n");
                printf("DiskId         : ");
                for (int i = 0; i < sizeof(pLayout->Gpt.DiskId); i++)
                    printf("%02X", ((BYTE*)&pLayout->Gpt.DiskId)[i]);
                printf("\n");
				printf("StartingUsableOffset : %I64d\n", pLayout->Gpt.StartingUsableOffset.QuadPart);
				printf("UsableLength         : %I64d\n", pLayout->Gpt.UsableLength.QuadPart);
				printf("MaxPartitionCount    : %d\n", pLayout->Gpt.MaxPartitionCount);
            }
            else
            {
                printf("Partition Style: RAW\n");
            }

            printf("\nList of Partitions:\n");
            printf("Number     Partition Type     Hidden?    Boot?    Starting Offset         Partition Length\n");
            printf("------    ----------------    -------    -----    ---------------         ----------------\n");

            for (DWORD i = 0; i < pLayout->PartitionCount; i++)
            {
                PARTITION_INFORMATION_EX* pPart = &pLayout->PartitionEntry[i];
                const char* partitionStyleStr = "RAW";
                const char* typeStr = "N/A";
                if (pPart->PartitionStyle == PARTITION_STYLE_MBR)
                {
                    partitionStyleStr = "MBR";
                    switch (pPart->Mbr.PartitionType)
                    {
                    case PARTITION_ENTRY_UNUSED:
                        typeStr = "Unused";
                        break;
                    case PARTITION_FAT_12:
                        typeStr = "FAT12";
                        break;
                    case PARTITION_XENIX_1:
                        typeStr = "Xenix 1";
                        break;
                    case PARTITION_XENIX_2:
                        typeStr = "Xenix 2";
                        break;
                    case PARTITION_FAT_16:
                        typeStr = "FAT16";
                        break;
                    case PARTITION_EXTENDED:
                        typeStr = "Extended";
                        break;
                    case PARTITION_HUGE:
                        typeStr = "Huge";
                        break;
                    case PARTITION_IFS:
                        typeStr = "IFS";
                        break;
                    case PARTITION_OS2BOOTMGR:
                        typeStr = "OS2 Boot Manager";
                        break;
                    case PARTITION_FAT32:
                        typeStr = "FAT32";
                        break;
                    case PARTITION_FAT32_XINT13:
                        typeStr = "FAT32 XINT13";
                        break;
                    case PARTITION_XINT13:
                        typeStr = "XINT13";
                        break;
                    case PARTITION_XINT13_EXTENDED:
                        typeStr = "XINT13 Extended";
                        break;
                    case PARTITION_MSFT_RECOVERY:
                        typeStr = "MSFT Recovery";
                        break;
                    case PARTITION_MAIN_OS:
                        typeStr = "Main OS";
                        break;
                    case PARTIITON_OS_DATA:
                        typeStr = "OS Data";
                        break;
                    case PARTITION_PRE_INSTALLED:
                        typeStr = "PreInstalled";
                        break;
                    case PARTITION_BSP:
                        typeStr = "BSP";
                        break;
                    case PARTITION_DPP:
                        typeStr = "DPP";
                        break;
                    case PARTITION_WINDOWS_SYSTEM:
                        typeStr = "Windows System";
                        break;
                    case PARTITION_PREP:
                        typeStr = "Prep";
                        break;
                    case PARTITION_LDM:
                        typeStr = "LDM";
                        break;
                    case PARTITION_DM:
                        typeStr = "DM";
                        break;
                    case PARTITION_EZDRIVE:
                        typeStr = "EZDrive";
                        break;
                    case PARTITION_UNIX:
                        typeStr = "Unix";
                        break;
                    case PARTITION_SPACES_DATA:
                        typeStr = "Spaces Data";
                        break;
                    case PARTITION_SPACES:
                        typeStr = "Spaces";
                        break;
                    case PARTITION_GPT:
                        typeStr = "GPT";
                        break;
                    case PARTITION_SYSTEM:
                        typeStr = "System";
                        break;
                    }
                }
                else if (pPart->PartitionStyle == PARTITION_STYLE_GPT)
				{
                    partitionStyleStr = "GPT";
					GUID typeGUID = pPart->Gpt.PartitionType;
                    if (IsEqualGUID(typeGUID, PARTITION_BASIC_DATA_GUID))
                    {
						typeStr = "Basic Data";
                    }
                    else if (IsEqualGUID(typeGUID, PARTITION_SYSTEM_GUID))
                    {
						typeStr = "System";
                    }
                    else if (IsEqualGUID(typeGUID, PARTITION_MSFT_RESERVED_GUID))
                    {
						typeStr = "MSFT Reserved";
                    }
                    else if (IsEqualGUID(typeGUID, PARTITION_LDM_METADATA_GUID))
                    {
						typeStr = "LDM Metadata";
                    }
                    else if (IsEqualGUID(typeGUID, PARTITION_LDM_DATA_GUID))
                    {
						typeStr = "LDM Data";
                    }
                    else if (IsEqualGUID(typeGUID, PARTITION_MSFT_RECOVERY_GUID))
                    {
						typeStr = "MSFT Recovery";
                    }
                    else if (IsEqualGUID(typeGUID, PARTITION_ENTRY_UNUSED_GUID))
                    {
						typeStr = "Unused";
                    }
				}

                const char* hiddenStr = "N/A";
                const char* bootStr = "N/A";
                if (pPart->PartitionStyle == PARTITION_STYLE_MBR)
                {
                    hiddenStr = (pPart->Mbr.HiddenSectors > 0) ? "Yes" : "No";
                    bootStr = (pPart->Mbr.BootIndicator) ? "Yes" : "No";
                }

                printf("%6d    %16s    %7s    %5s   %16I64d         %16I64d\n",
                    pPart->PartitionNumber,
                    typeStr,
                    hiddenStr,
                    bootStr,
                    pPart->StartingOffset.QuadPart,
                    pPart->PartitionLength.QuadPart);
            }
            printf("-----------------------------------------------------------\n");
        }
        else
        {
            wprintf(L"IOCTL_DISK_GET_DRIVE_LAYOUT_EX failed, error: 0x%.8X\n", GetLastError());
        }
    }

    //
    // Send IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS to the physical disk.
    //
    {
        BYTE diskExtentsBuffer[4096] = { 0 };
        if (DeviceIoControl(hPhysical,
            IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
            NULL,
            0,
            diskExtentsBuffer,
            sizeof(diskExtentsBuffer),
            &BytesReturned,
            NULL))
        {
            VOLUME_DISK_EXTENTS* pDiskExtents = (VOLUME_DISK_EXTENTS*)diskExtentsBuffer;
            printf("\nPhysical Disk Volume Extents:\n");
            for (DWORD i = 0; i < pDiskExtents->NumberOfDiskExtents; i++)
            {
                printf("  Extent %d: DiskNumber = %d, StartingOffset = %I64d, ExtentLength = %I64d\n",
                    i,
                    pDiskExtents->Extents[i].DiskNumber,
                    pDiskExtents->Extents[i].StartingOffset.QuadPart,
                    pDiskExtents->Extents[i].ExtentLength.QuadPart);
            }
        }
        else
        {
			DWORD dwError = GetLastError();
			if (dwError == ERROR_INVALID_FUNCTION)
			{
				wprintf(L"IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS is not supported on this disk/volume.\n");
			}
			else
			{
				wprintf(L"IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed, error: 0x%.8X\n", dwError);
			}
        }
    }

    //
    // Send IOCTL_VOLUME_IS_DYNAMIC to the physical disk.
    //
    {
        BOOL isDynamic = FALSE;
        if (DeviceIoControl(hPhysical,
            IOCTL_VOLUME_IS_DYNAMIC,
            NULL,
            0,
            &isDynamic,
            sizeof(isDynamic),
            &BytesReturned,
            NULL))
        {
            wprintf(L"\nVolume is %sdynamic.\n", isDynamic ? L"" : L"not ");
        }
        else
        {
			DWORD dwError = GetLastError();
			if (dwError == ERROR_INVALID_FUNCTION)
			{
				wprintf(L"IOCTL_VOLUME_IS_DYNAMIC is not supported on this disk/volume.\n");
			}
            else
            {
                wprintf(L"IOCTL_VOLUME_IS_DYNAMIC failed, error: 0x%.8X\n", GetLastError());
            }
        }
    }

    // Close the physical drive handle.
    CloseHandle(hPhysical);

    return 0;
}