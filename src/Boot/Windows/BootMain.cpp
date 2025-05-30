/*
 Derived from source code of TrueCrypt 7.1a, which is
 Copyright (c) 2008-2012 TrueCrypt Developers Association and which is governed
 by the TrueCrypt License 3.0.

 Modifications and additions to the original source code (contained in this file)
 and all other portions of this file are Copyright (c) 2013-2025 AM Crypto
 and are governed by the Apache License 2.0 the full text of which is
 contained in the file License.txt included in VeraCrypt binary and source
 code distribution packages.
*/

#include "Crc.h"
#include "Crypto.h"
#include "Password.h"
#include "Volumes.h"

#include "Platform.h"
#include "Bios.h"
#include "BootConfig.h"
#include "BootMain.h"
#include "BootDefs.h"
#include "BootCommon.h"
#include "BootConsoleIo.h"
#include "BootDebug.h"
#include "BootDiskIo.h"
#include "BootEncryptedIo.h"
#include "BootMemory.h"
#include "BootStrings.h"
#include "IntFilter.h"


static void InitScreen ()
{
	ClearScreen();

	const char *title =
#ifndef TC_WINDOWS_BOOT_RESCUE_DISK_MODE
		" VeraCrypt Boot Loader "
#else
		" VeraCrypt Rescue Disk "
#endif
		VERSION_STRING "\r\n";

	Print (title);

	PrintRepeatedChar ('\xDC', TC_BIOS_MAX_CHARS_PER_LINE);

#ifndef TC_WINDOWS_BOOT_RESCUE_DISK_MODE
	if (CustomUserMessage[0])
	{
		PrintEndl();
		Print (CustomUserMessage);
	}
#endif

	PrintEndl (2);
}


static void PrintMainMenu ()
{
	if (PreventBootMenu)
		return;

	Print ("    Keyboard Controls:\r\n");
	Print ("    [F5]   Hide/Show Password and PIM\r\n");
	Print ("    [Esc]  ");

#ifndef TC_WINDOWS_BOOT_RESCUE_DISK_MODE

	Print ((BootSectorFlags & TC_BOOT_CFG_MASK_HIDDEN_OS_CREATION_PHASE) != TC_HIDDEN_OS_CREATION_PHASE_NONE
		? "Boot Non-Hidden System (Boot Manager)"
		: "Skip Authentication (Boot Manager)");

#else // TC_WINDOWS_BOOT_RESCUE_DISK_MODE

	Print ("Skip Authentication (Boot Manager)");
	Print ("\r\n    [F8]   "); Print ("Repair Options");

#endif // TC_WINDOWS_BOOT_RESCUE_DISK_MODE

	PrintEndl (3);
}


static bool IsMenuKey (uint8 scanCode)
{
#ifdef TC_WINDOWS_BOOT_RESCUE_DISK_MODE
	return scanCode == TC_MENU_KEY_REPAIR;
#else
	return false;
#endif
}


static bool AskYesNo (const char *message)
{
	Print (message);
	Print ("? (y/n): ");
	while (true)
	{
		switch (GetKeyboardChar())
		{
		case 'y':
		case 'Y':
		case 'z':
		case 'Z':
			Print ("y\r\n");
			return true;

		case 'n':
		case 'N':
			Print ("n\r\n");
			return false;

		default:
			Beep();
		}
	}
}


static int AskSelection (const char *options[], size_t optionCount)
{
	for (int i = 0; i < optionCount; ++i)
	{
		Print ("["); Print (i + 1); Print ("]    ");
		Print (options[i]);
		PrintEndl();
	}
	Print ("[Esc]  Cancel\r\n\r\n");

	Print ("To select, press 1-9: ");

	char str;

	while (true)
	{
		if (GetString (&str, 1) == 0)
			return 0;

		if (str >= '1' && str <= optionCount + '0')
			return str - '0';

		Beep();
		PrintBackspace();
	}
}


static uint8 AskPassword (Password &password, int& pim)
{
	size_t pos = 0;
	uint8 scanCode;
	uint8 asciiCode;
	uint8 hidePassword = 1;

	pim = 0;

	Print ("Enter password");
	Print (PreventNormalSystemBoot ? " for hidden system:\r\n" : ": ");

	while (true)
	{
		asciiCode = GetKeyboardChar (&scanCode);

		switch (scanCode)
		{
		case TC_BIOS_KEY_ENTER:
			password.Length = pos;
			Print ("\r");
			if (!PreventNormalSystemBoot)
				Print ("Enter password: ");
			pos = 0;
			while (pos < MAX_PASSWORD)
			{
				pos++;
				if (pos < MAX_PASSWORD)
					PrintChar ('*');
				else
					PrintCharAtCursor ('*');
			}

			ClearBiosKeystrokeBuffer();
			PrintEndl();

			break;

		case TC_BIOS_KEY_BACKSPACE:
			if (pos > 0)
			{
				if (pos < MAX_PASSWORD)
					PrintBackspace();
				else
					PrintCharAtCursor (' ');

				--pos;
			}
			continue;

		case TC_BIOS_KEY_F5:
			hidePassword ^= 0x01;
			continue;

		default:
			if (scanCode == TC_BIOS_KEY_ESC || IsMenuKey (scanCode))
			{
				burn (password.Text, sizeof (password.Text));
				ClearBiosKeystrokeBuffer();

				PrintEndl();
				return scanCode;
			}
		}

		if (TC_BIOS_KEY_ENTER == scanCode)
			break;

		if (!IsPrintable (asciiCode) || pos == MAX_PASSWORD)
		{
			Beep();
			continue;
		}

		password.Text[pos++] = asciiCode;
		if (hidePassword) asciiCode = '*';
		if (pos < MAX_PASSWORD)
			PrintChar (asciiCode);
		else
			PrintCharAtCursor (asciiCode);
	}

#ifndef TC_WINDOWS_BOOT_RESCUE_DISK_MODE
	if (PimValueOrHiddenVolumeStartUnitNo.LowPart != -1)
	{
		pim = (int) PimValueOrHiddenVolumeStartUnitNo.LowPart;
		// reset stored PIM value to allow requesting PIM next time in case the stored value is wrong
		PimValueOrHiddenVolumeStartUnitNo.LowPart = -1;
		return TC_BIOS_KEY_ENTER;
	}
	else
#endif
	{
		pos = 0;
		Print ("PIM: ");

		while (true)
		{
			asciiCode = GetKeyboardChar (&scanCode);

			switch (scanCode)
			{
			case TC_BIOS_KEY_ENTER:
				Print ("\rPIM: ");
				pos =0;
				while (pos < MAX_PIM)
				{
					PrintChar ('*');
					pos++;
				}

				ClearBiosKeystrokeBuffer();
				PrintEndl();

				return TC_BIOS_KEY_ENTER;

			case TC_BIOS_KEY_BACKSPACE:
				if (pos > 0)
				{
					if (pos < MAX_PIM)
						PrintBackspace();
					else
						PrintCharAtCursor (' ');

					--pos;
					pim /= 10;
				}
				continue;

			case TC_BIOS_KEY_F5:
				hidePassword ^= 0x01;
				continue;

			default:
				if (scanCode == TC_BIOS_KEY_ESC || IsMenuKey (scanCode))
				{
					burn (password.Text, sizeof (password.Text));
					ClearBiosKeystrokeBuffer();

					PrintEndl();
					return scanCode;
				}
			}

			if (!IsDigit (asciiCode) || pos == MAX_PIM)
			{
				Beep();
				continue;
			}

			pim = 10*pim + (asciiCode - '0');
			pos++;

			if (hidePassword) asciiCode = '*';
			if (pos < MAX_PIM)
				PrintChar (asciiCode);
			else
				PrintCharAtCursor (asciiCode);
		}
	}
}


static void ExecuteBootSector (uint8 drive, uint8 *sectorBuffer)
{
	Print ("Booting...\r\n");
	CopyMemory (sectorBuffer, 0x0000, 0x7c00, TC_LB_SIZE);

	BootStarted = true;

	uint32 addr = 0x7c00;
	__asm
	{
		cli
		mov dl, drive	// Boot drive
		mov dh, 0
		xor ax, ax
		mov si, ax
		mov ds, ax
		mov es, ax
		mov ss, ax
		mov sp, 0x7c00
		sti

		jmp cs:addr
	}
}


static bool OpenVolume (uint8 drive, Password &password, int pim, CRYPTO_INFO **cryptoInfo, uint32 *headerSaltCrc32, bool skipNormal, bool skipHidden)
{
	int volumeType;
	bool hiddenVolume;
	uint64 headerSec;

	AcquireSectorBuffer();

	for (volumeType = 1; volumeType <= 2; ++volumeType)
	{
		hiddenVolume = (volumeType == 2);

		if (hiddenVolume)
		{
			if (skipHidden || PartitionFollowingActive.Drive != drive || PartitionFollowingActive.SectorCount <= ActivePartition.SectorCount)
				continue;

			headerSec = PartitionFollowingActive.StartSector + TC_HIDDEN_VOLUME_HEADER_OFFSET / TC_LB_SIZE;
		}
		else
		{
			if (skipNormal)
				continue;

			headerSec.HighPart = 0;
			headerSec.LowPart = TC_BOOT_VOLUME_HEADER_SECTOR;
		}

		if (ReadSectors (SectorBuffer, drive, headerSec, 1) != BiosResultSuccess)
			continue;

		if (ReadVolumeHeader (!hiddenVolume, SectorBuffer, &password, pim, cryptoInfo, nullptr) == ERR_SUCCESS)
		{
			// Prevent opening a non-system hidden volume
			if (hiddenVolume && !((*cryptoInfo)->HeaderFlags & TC_HEADER_FLAG_ENCRYPTED_SYSTEM))
			{
				crypto_close (*cryptoInfo);
				continue;
			}

			if (headerSaltCrc32)
				*headerSaltCrc32 = GetCrc32 (SectorBuffer, PKCS5_SALT_SIZE);

			break;
		}
	}

	ReleaseSectorBuffer();
	return volumeType != 3;
}


static bool CheckMemoryRequirements ()
{
	uint16 codeSeg;
	__asm mov codeSeg, cs
	if (codeSeg == TC_BOOT_LOADER_LOWMEM_SEGMENT)
	{
		PrintErrorNoEndl ("BIOS reserved too much memory: ");

		uint16 memFree;
		__asm
		{
			push es
			xor ax, ax
			mov es, ax
			mov ax, es:[0x413]
			mov memFree, ax
			pop es
		}

		Print (memFree);
		PrintEndl();
		Print (TC_BOOT_STR_UPGRADE_BIOS);

		return false;
	}

	return true;
}


static bool MountVolume (uint8 drive, uint8 &exitKey, bool skipNormal, bool skipHidden)
{
	BootArguments *bootArguments = (BootArguments *) TC_BOOT_LOADER_ARGS_OFFSET;
	int incorrectPasswordCount = 0, pim = 0;

	EraseMemory (bootArguments, sizeof (*bootArguments));

	// Open volume header
	while (true)
	{
		exitKey = AskPassword (bootArguments->BootPassword, pim);

		if (exitKey != TC_BIOS_KEY_ENTER)
			return false;

		Print ("Verifying password...");

		if (OpenVolume (BootDrive, bootArguments->BootPassword, pim, &BootCryptoInfo, &bootArguments->HeaderSaltCrc32, skipNormal, skipHidden))
		{
			Print ("OK\r\n");
			break;
		}
		if (GetShiftFlags() & TC_BIOS_SHIFTMASK_CAPSLOCK)
			Print ("Warning: Caps Lock is on.\r\n");

		Print ("Incorrect password.\r\n\r\n");

		if (++incorrectPasswordCount == 4)
		{
#ifdef TC_WINDOWS_BOOT_RESCUE_DISK_MODE
			Print ("If you are sure the password is correct, the key data may be damaged.\r\n"
				   "If so, use 'Repair Options' > 'Restore key data'.\r\n\r\n");
#else
			Print ("If you are sure the password is correct, the key data may be damaged. Boot your\r\n"
				   "VeraCrypt Rescue Disk and select 'Repair Options' > 'Restore key data'.\r\n\r\n");
#endif
		}
	}

	// Setup boot arguments
	bootArguments->BootLoaderVersion = VERSION_NUM;
	bootArguments->CryptoInfoOffset = (uint16) BootCryptoInfo;
	bootArguments->CryptoInfoLength = sizeof (*BootCryptoInfo);
	bootArguments->Flags = (((uint32)pim) << 16);

	if (BootCryptoInfo->hiddenVolume)
		bootArguments->HiddenSystemPartitionStart = PartitionFollowingActive.StartSector << TC_LB_SIZE_BIT_SHIFT_DIVISOR;

	if (ExtraBootPartitionPresent)
		bootArguments->Flags |= TC_BOOT_ARGS_FLAG_EXTRA_BOOT_PARTITION;

	TC_SET_BOOT_ARGUMENTS_SIGNATURE	(bootArguments->Signature);

	// Setup virtual encrypted partition
	if (BootCryptoInfo->EncryptedAreaLength.HighPart != 0 || BootCryptoInfo->EncryptedAreaLength.LowPart != 0)
	{
		EncryptedVirtualPartition.Drive = BootDrive;

		EncryptedVirtualPartition.StartSector = BootCryptoInfo->EncryptedAreaStart >> TC_LB_SIZE_BIT_SHIFT_DIVISOR;

		PimValueOrHiddenVolumeStartUnitNo = EncryptedVirtualPartition.StartSector;
		HiddenVolumeStartSector = PartitionFollowingActive.StartSector;
		HiddenVolumeStartSector += EncryptedVirtualPartition.StartSector;

		EncryptedVirtualPartition.SectorCount = BootCryptoInfo->EncryptedAreaLength >> TC_LB_SIZE_BIT_SHIFT_DIVISOR;

		EncryptedVirtualPartition.EndSector = EncryptedVirtualPartition.SectorCount - 1;
		EncryptedVirtualPartition.EndSector += EncryptedVirtualPartition.StartSector;
	}
	else
	{
		// Drive not encrypted
		EncryptedVirtualPartition.Drive = TC_INVALID_BIOS_DRIVE;
	}

	return true;
}


static bool GetSystemPartitions (uint8 drive)
{
	size_t partCount;

	if (!GetActivePartition (drive))
		return false;

	// Find partition following the active one
	GetDrivePartitions (drive, &PartitionFollowingActive, 1, partCount, false, &ActivePartition);

	// If there is an extra boot partition, use the partitions following it.
	// The real boot partition is determined in BootEncryptedDrive().
	if (ActivePartition.SectorCount.HighPart == 0 && ActivePartition.SectorCount.LowPart <= TC_MAX_EXTRA_BOOT_PARTITION_SIZE / TC_LB_SIZE
		&& PartitionFollowingActive.Drive != TC_INVALID_BIOS_DRIVE)
	{
		ExtraBootPartitionPresent = true;

		ActivePartition = PartitionFollowingActive;
		GetDrivePartitions (drive, &PartitionFollowingActive, 1, partCount, false, &ActivePartition);
	}

	return true;
}


static uint8 BootEncryptedDrive ()
{
	BootArguments *bootArguments = (BootArguments *) TC_BOOT_LOADER_ARGS_OFFSET;
	uint8 exitKey;
	BootCryptoInfo = NULL;

	if (!GetSystemPartitions (BootDrive))
		goto err;

	if (!MountVolume (BootDrive, exitKey, PreventNormalSystemBoot, false))
		return exitKey;

	if (!CheckMemoryRequirements ())
		goto err;

	if (BootCryptoInfo->hiddenVolume)
	{
		EncryptedVirtualPartition = ActivePartition;
		bootArguments->DecoySystemPartitionStart = ActivePartition.StartSector << TC_LB_SIZE_BIT_SHIFT_DIVISOR;
	}

	if (ExtraBootPartitionPresent && !GetActivePartition (BootDrive))
		goto err;

	if (ReadWriteMBR (false, ActivePartition.Drive) != BiosResultSuccess)
		goto err;

	bootArguments->BootDriveSignature = *(uint32 *) (SectorBuffer + 0x1b8);

	if (!InstallInterruptFilters())
		goto err;

	bootArguments->BootArgumentsCrc32 = GetCrc32 ((uint8 *) bootArguments, (uint8 *) &bootArguments->BootArgumentsCrc32 - (uint8 *) bootArguments);

	while (true)
	{
		// Execute boot sector of the active partition
		if (ReadSectors (SectorBuffer, ActivePartition.Drive, ActivePartition.StartSector, 1) == BiosResultSuccess)
		{
			if (*(uint16 *) (SectorBuffer + 510) != 0xaa55)
			{
				PrintError (TC_BOOT_STR_NO_BOOT_PARTITION);
				GetKeyboardChar();
			}

			ExecuteBootSector (ActivePartition.Drive, SectorBuffer);
		}

		GetKeyboardChar();
	}

err:
	if (BootCryptoInfo)
	{
		crypto_close (BootCryptoInfo);
		BootCryptoInfo = NULL;
	}
#ifndef TC_WINDOWS_BOOT_RESCUE_DISK_MODE
	PimValueOrHiddenVolumeStartUnitNo.LowPart = -1;
#endif
	EncryptedVirtualPartition.Drive = TC_INVALID_BIOS_DRIVE;
	EraseMemory ((void *) TC_BOOT_LOADER_ARGS_OFFSET, sizeof (BootArguments));

	uint8 scanCode;
	GetKeyboardChar (&scanCode);
	return scanCode;
}


static void BootMenu ()
{
	BiosResult result;
	Partition partitions[16];
	Partition bootablePartitions[9];
	size_t partitionCount;
	size_t bootablePartitionCount = 0;

	for (uint8 drive = TC_FIRST_BIOS_DRIVE; drive <= TC_LAST_BIOS_DRIVE; ++drive)
	{
		if (GetDrivePartitions (drive, partitions, array_capacity (partitions), partitionCount, false, nullptr, true) == BiosResultSuccess)
		{
			for (size_t i = 0; i < partitionCount; ++i)
			{
				const Partition &partition = partitions[i];
				result = ReadSectors (SectorBuffer, drive, partition.StartSector, 1);

				if (result == BiosResultSuccess && *(uint16 *) (SectorBuffer + TC_LB_SIZE - 2) == 0xaa55)
				{
					// Windows writes boot loader on all NTFS/FAT filesytems it creates and, therefore,
					// NTFS/FAT partitions must have the boot indicator set to be considered bootable.
					if (!partition.Active
						&& (*(uint32 *) (SectorBuffer + 3) == 0x5346544e  // 'NTFS'
							|| *(uint32 *) (SectorBuffer + 3) == 0x41465845	&& SectorBuffer[7] == 'T' // 'exFAT'
							|| *(uint16 *) (SectorBuffer + 54) == 0x4146 && SectorBuffer[56] == 'T' // 'FAT'
							|| *(uint16 *) (SectorBuffer + 82) == 0x4146 && SectorBuffer[84] == 'T'))
					{
						continue;
					}

					// Bootable sector found
					if (bootablePartitionCount < array_capacity (bootablePartitions))
						bootablePartitions[bootablePartitionCount++] = partition;
				}
			}
		}
	}

	if (bootablePartitionCount < 1)
	{
		PrintError (TC_BOOT_STR_NO_BOOT_PARTITION);
		GetKeyboardChar();
		return;
	}

	char partChar;
	while (true)
	{
		InitScreen();
		Print ("Bootable Partitions:\r\n");
		PrintRepeatedChar ('\xC4', 20);
		Print ("\r\n");

		for (size_t i = 0; i < bootablePartitionCount; ++i)
		{
			const Partition &partition = bootablePartitions[i];
			Print ("["); Print (i + 1); Print ("]    ");
			Print ("Drive: "); Print (partition.Drive - TC_FIRST_BIOS_DRIVE);
			Print (", Partition: "); Print (partition.Number + 1);
			Print (", Size: "); PrintSectorCountInMB (partition.SectorCount); PrintEndl();
		}

		if (bootablePartitionCount == 1)
		{
			// There's only one bootable partition so we'll boot it directly instead of showing boot manager
			partChar = '1';
		}
		else
		{
			Print ("[Esc]  Cancel\r\n\r\n");
			Print ("Press 1-9 to select partition: ");

			if (GetString (&partChar, 1) == 0)
				return;

			PrintEndl();

			if (partChar < '1' || partChar > '0' + bootablePartitionCount)
			{
				Beep();
				continue;
			}
		}

		const Partition &partition = bootablePartitions[partChar - '0' - 1];

		if (ReadSectors (SectorBuffer, partition.Drive, partition.StartSector, 1) == BiosResultSuccess)
		{
			ExecuteBootSector (partition.Drive, SectorBuffer);
		}
	}
}


#ifndef TC_WINDOWS_BOOT_RESCUE_DISK_MODE

static bool CopySystemPartitionToHiddenVolume (uint8 drive, uint8 &exitKey)
{
	bool status = false;

	uint64 sectorsRemaining;
	uint64 sectorOffset;
	sectorOffset.LowPart = 0;
	sectorOffset.HighPart = 0;

	int fragmentSectorCount = 0x7f; // Maximum safe value supported by BIOS
	int statCount = 0;

	if (!CheckMemoryRequirements ())
		goto err;

	if (!GetSystemPartitions (drive))
		goto err;

	if (PartitionFollowingActive.Drive == TC_INVALID_BIOS_DRIVE)
		TC_THROW_FATAL_EXCEPTION;

	// Check if BIOS can read the last sector of the hidden system
	AcquireSectorBuffer();

	if (ReadSectors (SectorBuffer, PartitionFollowingActive.Drive, PartitionFollowingActive.EndSector - (TC_VOLUME_HEADER_GROUP_SIZE / TC_LB_SIZE - 2), 1) != BiosResultSuccess
		|| GetCrc32 (SectorBuffer, sizeof (SectorBuffer)) != OuterVolumeBackupHeaderCrc)
	{
		PrintErrorNoEndl ("Your BIOS does not support large drives");
		Print (IsLbaSupported (PartitionFollowingActive.Drive) ? " due to a bug" : "\r\n- Enable LBA in BIOS");
		PrintEndl();
		Print (TC_BOOT_STR_UPGRADE_BIOS);

		ReleaseSectorBuffer();
		goto err;
	}

	ReleaseSectorBuffer();

	if (!MountVolume (drive, exitKey, true, false))
		return false;

	sectorsRemaining = EncryptedVirtualPartition.SectorCount;

	if (!(sectorsRemaining == ActivePartition.SectorCount))
		TC_THROW_FATAL_EXCEPTION;

	InitScreen();
	Print ("\r\nCopying system to hidden volume. To abort, press Esc.\r\n\r\n");

	while (sectorsRemaining.HighPart != 0 || sectorsRemaining.LowPart != 0)
	{
		if (EscKeyPressed())
		{
			Print ("\rIf aborted, copying will have to start from the beginning (if attempted again).\r\n");
			if (AskYesNo ("Abort"))
				break;
		}

		if (sectorsRemaining.HighPart == 0 && sectorsRemaining.LowPart < fragmentSectorCount)
			fragmentSectorCount = (int) sectorsRemaining.LowPart;

		if (ReadWriteSectors (false, TC_BOOT_LOADER_BUFFER_SEGMENT, 0, drive, ActivePartition.StartSector + sectorOffset, fragmentSectorCount, false) != BiosResultSuccess)
		{
			Print ("To fix bad sectors: 1) Terminate 2) Encrypt and decrypt sys partition 3) Retry\r\n");
			crypto_close (BootCryptoInfo);
			goto err;
		}

		AcquireSectorBuffer();

		for (int i = 0; i < fragmentSectorCount; ++i)
		{
			CopyMemory (TC_BOOT_LOADER_BUFFER_SEGMENT, i * TC_LB_SIZE, SectorBuffer, TC_LB_SIZE);

			uint64 s = PimValueOrHiddenVolumeStartUnitNo + sectorOffset + i;
			EncryptDataUnits (SectorBuffer, &s, 1, BootCryptoInfo);

			CopyMemory (SectorBuffer, TC_BOOT_LOADER_BUFFER_SEGMENT, i * TC_LB_SIZE, TC_LB_SIZE);
		}

		ReleaseSectorBuffer();

		if (ReadWriteSectors (true, TC_BOOT_LOADER_BUFFER_SEGMENT, 0, drive, HiddenVolumeStartSector + sectorOffset, fragmentSectorCount, false) != BiosResultSuccess)
		{
			crypto_close (BootCryptoInfo);
			goto err;
		}

		sectorsRemaining = sectorsRemaining - fragmentSectorCount;
		sectorOffset = sectorOffset + fragmentSectorCount;

		if (!(statCount++ & 0xf))
		{
			Print ("\rRemaining: ");
			PrintSectorCountInMB (sectorsRemaining);
		}
	}

	crypto_close (BootCryptoInfo);

	if (sectorsRemaining.HighPart == 0 && sectorsRemaining.LowPart == 0)
	{
		status = true;
		Print ("\rCopying completed.");
	}

	PrintEndl (2);
	goto ret;

err:
	exitKey = TC_BIOS_KEY_ESC;
	GetKeyboardChar();

ret:
	PimValueOrHiddenVolumeStartUnitNo.LowPart = -1;
	EraseMemory ((void *) TC_BOOT_LOADER_ARGS_OFFSET, sizeof (BootArguments));
	return status;
}


#else // TC_WINDOWS_BOOT_RESCUE_DISK_MODE


static void DecryptDrive (uint8 drive)
{
	uint8 exitKey;
	if (!MountVolume (drive, exitKey, false, true))
		return;

	BootArguments *bootArguments = (BootArguments *) TC_BOOT_LOADER_ARGS_OFFSET;

	bool headerUpdateRequired = false;
	uint64 sectorsRemaining = EncryptedVirtualPartition.EndSector + 1 - EncryptedVirtualPartition.StartSector;
	uint64 sector = EncryptedVirtualPartition.EndSector + 1;

	int fragmentSectorCount = 0x7f; // Maximum safe value supported by BIOS
	int statCount = 0;

	bool skipBadSectors = false;

	Print ("\r\nUse only if Windows cannot start. Decryption under Windows is much faster\r\n"
			"(in VeraCrypt, select 'System' > 'Permanently Decrypt').\r\n\r\n");

	if (!AskYesNo ("Decrypt now"))
	{
		crypto_close (BootCryptoInfo);
		goto ret;
	}

	if (EncryptedVirtualPartition.Drive == TC_INVALID_BIOS_DRIVE)
	{
		// Drive already decrypted
		sectorsRemaining.HighPart = 0;
		sectorsRemaining.LowPart = 0;
	}
	else
	{
		Print ("\r\nTo safely interrupt and defer decryption, press Esc.\r\n"
			"WARNING: You can turn off power only after you press Esc.\r\n\r\n");
	}

	while (sectorsRemaining.HighPart != 0 || sectorsRemaining.LowPart != 0)
	{
		if (EscKeyPressed())
			break;

		if (sectorsRemaining.HighPart == 0 && sectorsRemaining.LowPart < fragmentSectorCount)
			fragmentSectorCount = (int) sectorsRemaining.LowPart;

		sector = sector - fragmentSectorCount;

		if (!(statCount++ & 0xf))
		{
			Print ("\rRemaining: ");
			PrintSectorCountInMB (sectorsRemaining);
		}

		if (ReadWriteSectors (false, TC_BOOT_LOADER_BUFFER_SEGMENT, 0, drive, sector, fragmentSectorCount, skipBadSectors) == BiosResultSuccess)
		{
			AcquireSectorBuffer();

			for (int i = 0; i < fragmentSectorCount; ++i)
			{
				CopyMemory (TC_BOOT_LOADER_BUFFER_SEGMENT, i * TC_LB_SIZE, SectorBuffer, TC_LB_SIZE);

				uint64 s = sector + i;
				DecryptDataUnits (SectorBuffer, &s, 1, BootCryptoInfo);

				CopyMemory (SectorBuffer, TC_BOOT_LOADER_BUFFER_SEGMENT, i * TC_LB_SIZE, TC_LB_SIZE);
			}

			ReleaseSectorBuffer();

			if (ReadWriteSectors (true, TC_BOOT_LOADER_BUFFER_SEGMENT, 0, drive, sector, fragmentSectorCount, skipBadSectors) != BiosResultSuccess && !skipBadSectors)
				goto askBadSectorSkip;
		}
		else if (!skipBadSectors)
			goto askBadSectorSkip;

		sectorsRemaining = sectorsRemaining - fragmentSectorCount;
		headerUpdateRequired = true;
		continue;

askBadSectorSkip:
		if (!AskYesNo ("Skip all bad sectors"))
			break;

		skipBadSectors = true;
		sector = sector + fragmentSectorCount;
		fragmentSectorCount = 1;
	}

	crypto_close (BootCryptoInfo);

	if (headerUpdateRequired)
	{
		Print ("\rUpdating header...");

		AcquireSectorBuffer();
		uint64 headerSector;
		headerSector.HighPart = 0;
		headerSector.LowPart = TC_BOOT_VOLUME_HEADER_SECTOR;

		// Update encrypted area size in volume header

		CRYPTO_INFO *headerCryptoInfo = crypto_open();
		while (ReadSectors (SectorBuffer, drive, headerSector, 1) != BiosResultSuccess);

		if (ReadVolumeHeader (TRUE, SectorBuffer, &bootArguments->BootPassword, (int) (bootArguments->Flags >> 16), NULL, headerCryptoInfo) == 0)
		{
			DecryptBuffer (SectorBuffer + HEADER_ENCRYPTED_DATA_OFFSET, HEADER_ENCRYPTED_DATA_SIZE, headerCryptoInfo);

			uint64 encryptedAreaLength = sectorsRemaining << TC_LB_SIZE_BIT_SHIFT_DIVISOR;

			for (int i = 7; i >= 0; --i)
			{
				SectorBuffer[TC_HEADER_OFFSET_ENCRYPTED_AREA_LENGTH + i] = (uint8) encryptedAreaLength.LowPart;
				encryptedAreaLength = encryptedAreaLength >> 8;
			}

			uint32 headerCrc32 = GetCrc32 (SectorBuffer + TC_HEADER_OFFSET_MAGIC, TC_HEADER_OFFSET_HEADER_CRC - TC_HEADER_OFFSET_MAGIC);

			for (i = 3; i >= 0; --i)
			{
				SectorBuffer[TC_HEADER_OFFSET_HEADER_CRC + i] = (uint8) headerCrc32;
				headerCrc32 >>= 8;
			}

			EncryptBuffer (SectorBuffer + HEADER_ENCRYPTED_DATA_OFFSET, HEADER_ENCRYPTED_DATA_SIZE, headerCryptoInfo);
		}

		crypto_close (headerCryptoInfo);

		while (WriteSectors (SectorBuffer, drive, headerSector, 1) != BiosResultSuccess);
		ReleaseSectorBuffer();

		Print ("Done!\r\n");
	}

	if (sectorsRemaining.HighPart == 0 && sectorsRemaining.LowPart == 0)
		Print ("\rDrive decrypted.\r\n");
	else
		Print ("\r\nDecryption deferred.\r\n");

	GetKeyboardChar();
ret:
	EraseMemory (bootArguments, sizeof (*bootArguments));
}


static void RepairMenu ()
{
	DriveGeometry bootLoaderDriveGeometry;

	if (GetDriveGeometry (BootLoaderDrive, bootLoaderDriveGeometry, true) != BiosResultSuccess)
	{
		// Some BIOSes may fail to get the geometry of an emulated floppy drive
		bootLoaderDriveGeometry.Cylinders = 80;
		bootLoaderDriveGeometry.Heads = 2;
		bootLoaderDriveGeometry.Sectors = 18;
	}

	while (true)
	{
		InitScreen();
		Print ("Available "); Print ("Repair Options"); Print (":\r\n");
		PrintRepeatedChar ('\xC4', 25);
		PrintEndl();

		enum
		{
			RestoreNone = 0,
			DecryptVolume,
			RestoreTrueCryptLoader,
			RestoreVolumeHeader,
			RestoreOriginalSystemLoader
		};

		static const char *options[] = { "Permanently decrypt system partition/drive", "Restore VeraCrypt Boot Loader", "Restore key data (volume header)", "Restore original system loader" };

		int selection = AskSelection (options,
			(BootSectorFlags & TC_BOOT_CFG_FLAG_RESCUE_DISK_ORIG_SYS_LOADER) ? array_capacity (options) : array_capacity (options) - 1);

		PrintEndl();

		switch (selection)
		{
			case RestoreNone:
				return;

			case DecryptVolume:
				DecryptDrive (BootDrive);
				continue;

			case RestoreOriginalSystemLoader:
				if (!AskYesNo ("Is the system partition/drive decrypted"))
				{
					Print ("Please decrypt it first.\r\n");
					GetKeyboardChar();
					continue;
				}
				break;
		}

		bool writeConfirmed = false;
		BiosResult result;

		uint64 sector;
		sector.HighPart = 0;
		ChsAddress chs;

		uint8 mbrPartTable[TC_LB_SIZE - TC_MAX_MBR_BOOT_CODE_SIZE];
		AcquireSectorBuffer();

		for (int i = (selection == RestoreVolumeHeader ? TC_BOOT_VOLUME_HEADER_SECTOR : TC_MBR_SECTOR);
			i < TC_BOOT_LOADER_AREA_SECTOR_COUNT; ++i)
		{
			sector.LowPart = i;

			if (selection == RestoreOriginalSystemLoader)
				sector.LowPart += TC_ORIG_BOOT_LOADER_BACKUP_SECTOR;
			else if (selection == RestoreTrueCryptLoader)
				sector.LowPart += TC_BOOT_LOADER_BACKUP_RESCUE_DISK_SECTOR;

			// The backup medium may be a floppy-emulated bootable CD. The emulation may fail if LBA addressing is used.
			// Therefore, only CHS addressing can be used.
			LbaToChs (bootLoaderDriveGeometry, sector, chs);
			sector.LowPart = i;

			if (i == TC_MBR_SECTOR)
			{
				// Read current partition table
				result = ReadSectors (SectorBuffer, TC_FIRST_BIOS_DRIVE, sector, 1);
				if (result != BiosResultSuccess)
					goto err;

				memcpy (mbrPartTable, SectorBuffer + TC_MAX_MBR_BOOT_CODE_SIZE, sizeof (mbrPartTable));
			}

			result = ReadSectors (SectorBuffer, BootLoaderDrive, chs, 1);
			if (result != BiosResultSuccess)
				goto err;

			if (i == TC_MBR_SECTOR)
			{
				// Preserve current partition table
				memcpy (SectorBuffer + TC_MAX_MBR_BOOT_CODE_SIZE, mbrPartTable, sizeof (mbrPartTable));
			}

			// Volume header
			if (i == TC_BOOT_VOLUME_HEADER_SECTOR)
			{
				if (selection == RestoreTrueCryptLoader)
					continue;

				if (selection == RestoreVolumeHeader)
				{
					while (true)
					{
						bool validHeaderPresent = false;
						uint32 masterKeyScheduleCrc;

						Password password;
						int pim;
						uint8 exitKey = AskPassword (password, pim);

						if (exitKey != TC_BIOS_KEY_ENTER)
							goto abort;

						CRYPTO_INFO *cryptoInfo;

						CopyMemory (SectorBuffer, TC_BOOT_LOADER_BUFFER_SEGMENT, 0, TC_LB_SIZE);
						ReleaseSectorBuffer();

						// Restore volume header only if the current one cannot be used
						if (OpenVolume (TC_FIRST_BIOS_DRIVE, password, pim, &cryptoInfo, nullptr, false, true))
						{
							validHeaderPresent = true;
							masterKeyScheduleCrc = GetCrc32 (cryptoInfo->ks, sizeof (cryptoInfo->ks));
							crypto_close (cryptoInfo);
						}

						AcquireSectorBuffer();
						CopyMemory (TC_BOOT_LOADER_BUFFER_SEGMENT, 0, SectorBuffer, TC_LB_SIZE);

						if (ReadVolumeHeader (TRUE, SectorBuffer, &password, pim, &cryptoInfo, nullptr) == 0)
						{
							if (validHeaderPresent)
							{
								if (masterKeyScheduleCrc == GetCrc32 (cryptoInfo->ks, sizeof (cryptoInfo->ks)))
								{
									Print ("Original header preserved.\r\n");
									goto err;
								}

								Print ("WARNING: Drive 0 contains a valid header!\r\n");
							}

							crypto_close (cryptoInfo);
							break;
						}

						Print ("Incorrect password.\r\n\r\n");
					}
				}
			}

			if (!writeConfirmed && !AskYesNo ("Modify drive 0"))
				goto abort;
			writeConfirmed = true;

			if (WriteSectors (SectorBuffer, TC_FIRST_BIOS_DRIVE, sector, 1) != BiosResultSuccess)
				goto err;
		}
done:
		switch (selection)
		{
		case RestoreTrueCryptLoader:
			Print ("VeraCrypt Boot Loader");
			break;

		case RestoreVolumeHeader:
			Print ("Header");
			break;

		case RestoreOriginalSystemLoader:
			Print ("System loader");
			break;
		}
		Print (" restored.\r\n");

err:	GetKeyboardChar();
abort:	ReleaseSectorBuffer();
	}
}

#endif // TC_WINDOWS_BOOT_RESCUE_DISK_MODE


#ifndef DEBUG
extern "C" void _acrtused () { }  // Required by linker
#endif


void main ()
{
	__asm mov BootLoaderDrive, dl
	__asm mov BootSectorFlags, dh

#ifdef TC_BOOT_TRACING_ENABLED
	InitDebugPort();
#endif

#ifdef TC_BOOT_STACK_CHECKING_ENABLED
	InitStackChecker();
#endif

#ifndef TC_WINDOWS_BOOT_RESCUE_DISK_MODE
	ReadBootSectorUserConfiguration();
#elif defined (TC_WINDOWS_BOOT_AES)
	EnableHwEncryption (!(BootSectorFlags & TC_BOOT_CFG_FLAG_RESCUE_DISABLE_HW_ENCRYPTION));
#endif

	InitVideoMode();
	InitScreen();

	// Determine boot drive
	BootDrive = BootLoaderDrive;
	if (BootDrive < TC_FIRST_BIOS_DRIVE)
		BootDrive = TC_FIRST_BIOS_DRIVE;

	// Query boot drive geometry
	if (GetDriveGeometry (BootDrive, BootDriveGeometry) != BiosResultSuccess)
	{
		BootDrive = TC_FIRST_BIOS_DRIVE;
		if (GetDriveGeometry (BootDrive, BootDriveGeometry) != BiosResultSuccess)
		{
#ifdef TC_WINDOWS_BOOT_RESCUE_DISK_MODE
			Print ("- Connect system drive to (SATA) port 1\r\n");
#endif
			GetKeyboardChar();
		}
		else
			BootDriveGeometryValid = true;
	}
	else
		BootDriveGeometryValid = true;

#ifdef TC_WINDOWS_BOOT_RESCUE_DISK_MODE

	// Check whether the user is not using the Rescue Disk to create a hidden system

	if (ReadWriteMBR (false, BootDrive, true) == BiosResultSuccess
		&& *(uint32 *) (SectorBuffer + 6) == 0x61726556
		&& *(uint32 *) (SectorBuffer + 10) == 0x70797243
		&& (SectorBuffer[TC_BOOT_SECTOR_CONFIG_OFFSET] & TC_BOOT_CFG_MASK_HIDDEN_OS_CREATION_PHASE) != TC_HIDDEN_OS_CREATION_PHASE_NONE)
	{
		PrintError ("It appears you are creating a hidden OS.");
		if (AskYesNo ("Is this correct"))
		{
			Print ("Please remove the Rescue Disk from the drive and restart.");
			while (true);
		}
	}

#endif // TC_WINDOWS_BOOT_RESCUE_DISK_MODE


	// Main menu

	while (true)
	{
		uint8 exitKey;
		InitScreen();

#ifndef TC_WINDOWS_BOOT_RESCUE_DISK_MODE

		// Hidden system setup
		uint8 hiddenSystemCreationPhase = BootSectorFlags & TC_BOOT_CFG_MASK_HIDDEN_OS_CREATION_PHASE;

		if (hiddenSystemCreationPhase != TC_HIDDEN_OS_CREATION_PHASE_NONE)
		{
			PreventNormalSystemBoot = true;
			PrintMainMenu();

			if (hiddenSystemCreationPhase == TC_HIDDEN_OS_CREATION_PHASE_CLONING)
			{
				if (CopySystemPartitionToHiddenVolume (BootDrive, exitKey))
				{
					BootSectorFlags = (BootSectorFlags & ~TC_BOOT_CFG_MASK_HIDDEN_OS_CREATION_PHASE) | TC_HIDDEN_OS_CREATION_PHASE_WIPING;
					UpdateBootSectorConfiguration (BootLoaderDrive);
				}
				else if (exitKey == TC_BIOS_KEY_ESC)
					goto bootMenu;
				else
					continue;
			}
		}
		else
			PrintMainMenu();

		exitKey = BootEncryptedDrive();

#else // TC_WINDOWS_BOOT_RESCUE_DISK_MODE

		PrintMainMenu();
		exitKey = BootEncryptedDrive();

		if (exitKey == TC_MENU_KEY_REPAIR)
		{
			RepairMenu();
			continue;
		}

#endif // TC_WINDOWS_BOOT_RESCUE_DISK_MODE

bootMenu:
		if (!PreventBootMenu)
			BootMenu();
	}
}
