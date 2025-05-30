/*
 Legal Notice: Some portions of the source code contained in this file were
 derived from the source code of TrueCrypt 7.1a, which is
 Copyright (c) 2003-2012 TrueCrypt Developers Association and which is
 governed by the TrueCrypt License 3.0, also from the source code of
 Encryption for the Masses 2.02a, which is Copyright (c) 1998-2000 Paul Le Roux
 and which is governed by the 'License Agreement for Encryption for the Masses'
 Modifications and additions to the original source code (contained in this file)
 and all other portions of this file are Copyright (c) 2013-2025 AM Crypto
 and are governed by the Apache License 2.0 the full text of which is
 contained in the file License.txt included in VeraCrypt binary and source
 code distribution packages. */

extern volatile BOOL ProbingHostDeviceForWrite;

NTSTATUS TCOpenVolume ( PDEVICE_OBJECT DeviceObject , PEXTENSION Extension , MOUNT_STRUCT *mount , PWSTR pwszMountVolume , BOOL bRawDevice );
void TCCloseVolume ( PDEVICE_OBJECT DeviceObject , PEXTENSION Extension );
NTSTATUS TCCompletion ( PDEVICE_OBJECT DeviceObject , PIRP Irp , PVOID pUserBuffer );
static NTSTATUS TCSendHostDeviceIoControlRequest ( PDEVICE_OBJECT DeviceObject , PEXTENSION Extension , ULONG IoControlCode , void *OutputBuffer , ULONG OutputBufferSize );
static NTSTATUS TCSendHostDeviceIoControlRequestEx ( PDEVICE_OBJECT DeviceObject , PEXTENSION Extension , ULONG IoControlCode , void *InputBuffer , ULONG InputBufferSize , void *OutputBuffer , ULONG OutputBufferSize );
NTSTATUS COMPLETE_IRP ( PDEVICE_OBJECT DeviceObject , PIRP Irp , NTSTATUS IrpStatus , ULONG_PTR IrpInformation );
static void RestoreTimeStamp ( PEXTENSION Extension );
