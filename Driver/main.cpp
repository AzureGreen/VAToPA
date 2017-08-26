#include "main.h"

EXTERN_C_START

NTSTATUS
DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegisterPath)
{
	PDEVICE_OBJECT  DeviceObject;
	NTSTATUS        Status;

	UNICODE_STRING  uniDeviceName;
	UNICODE_STRING  uniLinkName;

	RtlInitUnicodeString(&uniDeviceName, DEVICE_NAME);
	RtlInitUnicodeString(&uniLinkName, LINK_NAME);

	// 创建设备对象;

	Status = IoCreateDevice(DriverObject, 0, &uniDeviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
	if (!NT_SUCCESS(Status))
	{
		return Status;
	}

	Status = IoCreateSymbolicLink(&uniLinkName, &uniDeviceName);

	for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		DriverObject->MajorFunction[i] = DefaultPassThrough;
	}

	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IoControlPassThrough;

	DriverObject->DriverUnload = UnloadDriver;


#ifdef WIN64

	DbgPrint("WIN64: UserIo IS RUNNING!!!");
#else

	DbgPrint("WIN32: UserIo IS RUNNING!!!");

#endif

	return STATUS_SUCCESS;
}

NTSTATUS
DefaultPassThrough(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

VOID
UnloadDriver(IN PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING  uniLinkName = { 0 };
	PDEVICE_OBJECT	NextDeviceObject = NULL;
	PDEVICE_OBJECT  CurrentDeviceObject = NULL;
	RtlInitUnicodeString(&uniLinkName, LINK_NAME);

	IoDeleteSymbolicLink(&uniLinkName);
	CurrentDeviceObject = DriverObject->DeviceObject;
	while (CurrentDeviceObject != NULL)
	{

		NextDeviceObject = CurrentDeviceObject->NextDevice;
		IoDeleteDevice(CurrentDeviceObject);
		CurrentDeviceObject = NextDeviceObject;
	}

	DbgPrint("INFO:VAToPA Is Stopped!\r\n");
}

/************************************************************************
*  Name : IoControlPassThrough
*  Param: DeviceObject			设备对象
*  Param: Irp					Irp请求
*  Ret  : NTSTATUS
*  IoCtl派遣例程
************************************************************************/

NTSTATUS
IoControlPassThrough(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	NTSTATUS			 Status = STATUS_SUCCESS;
	PVOID				 InputBuffer = NULL;
	PVOID				 OutputBuffer = NULL;
	UINT32				 InputLength = 0;
	UINT32				 OutputLength = 0;
	UINT32				 IoControlCode = 0;
	PIO_STACK_LOCATION   IrpStack = NULL;

	IrpStack = IoGetCurrentIrpStackLocation(Irp);				// 获得当前Irp堆栈
	InputBuffer = IrpStack->Parameters.DeviceIoControl.Type3InputBuffer;
	OutputBuffer = Irp->UserBuffer;
	InputLength = IrpStack->Parameters.DeviceIoControl.InputBufferLength;
	OutputLength = IrpStack->Parameters.DeviceIoControl.OutputBufferLength;
	IoControlCode = IrpStack->Parameters.DeviceIoControl.IoControlCode;

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	switch (IrpStack->MajorFunction)
	{
	case IRP_MJ_DEVICE_CONTROL:
	{
		switch (IoControlCode)
		{
		case IOCTL_VATOPA:
		{
			DbgPrint("VA to PA\r\n");

			if (InputLength >= sizeof(UINT_PTR) && OutputLength >= sizeof(UINT64) && InputBuffer)
			{
				__try
				{
					ProbeForRead(InputBuffer, InputLength, sizeof(UINT_PTR));
					ProbeForWrite(OutputBuffer, OutputLength, sizeof(UINT64));
					
					ConvertVAToPA(*(PUINT_PTR)InputBuffer, (PUINT64)OutputBuffer);
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{
					DbgPrint("Catch Exception\r\n");
					Status = STATUS_UNSUCCESSFUL;
				}
			}
			else
			{
				Irp->IoStatus.Status = STATUS_INFO_LENGTH_MISMATCH;
			}

			break;
		}
		default:
			Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
			break;
		}
		break;
	}
	default:
		break;
	}

	Status = Irp->IoStatus.Status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return Status;
}


UINT32
GetMaxPhyAddrBits()
{
	INT32 CpuInfo[4] = { -1 };
	UINT32 MaxPhyAddr = 0;

	__cpuidex(CpuInfo, 0x80000000, 32);
	if (CpuInfo[0] == 0x80000008)   // 检测是否支持 80000008H leaf
	{
		RtlZeroMemory(CpuInfo, sizeof(CpuInfo));
		__cpuid(CpuInfo, 0x80000008);
		MaxPhyAddr = CpuInfo[0] & 0xff;		// CPUID:800000008:EAX[7:0]
	}
	else
	{
		RtlZeroMemory(CpuInfo, sizeof(CpuInfo));
		__cpuid(CpuInfo, 0x01);
		if (CpuInfo[3] & 0x20000)		// 检测 CPUID.01H:EDX[17]=1 是否支持PSE-36 ; bt edx, 17
		{
			MaxPhyAddr = 36;
		}
		else
		{
			MaxPhyAddr = 32;
		}
	}
	return MaxPhyAddr;
}



VOID
ConvertVAToPA(IN UINT_PTR VA, OUT PUINT64 PA)
{
	UINT64 PageMapLevel4Table = 0;						// PML4T
	PHYSICAL_ADDRESS PageMapLevel4Entry = { 0 };		// PML4表项
	PUINT64 PML4E_MapVA = NULL;	

	UINT64 PageDirectoryPointerTable = 0;				// PDPT = cr3 & ~0x1f;
	PHYSICAL_ADDRESS PageDirectoryPointerEntry = { 0 };	// PDPTE表项，物理地址 PDPE = VA[31:30]*8 + PDPT
	PUINT64 PDPTE_MapVA = NULL;							// PDPTE物理地址映射虚拟地址

	UINT64 PageDirectoryTable = 0;						// PDT = *PDPE
	PHYSICAL_ADDRESS PageDirectoryEntry = { 0 };		// PDTE表项，物理地址 PDE = VA[29:21]*8 + PDT
	PUINT64 PDTE_MapVA = NULL;

	UINT64 PageTable = 0;								// PT = *PDE
	PHYSICAL_ADDRESS PageTableEntry = { 0 };			// PTE表项，物理地址 PTE = VA[20:12]*8 + PT
	PUINT64 PTE_MapVA = NULL;

	UINT64 PageFrame = 0;								// 物理地址所在页基地址


	//
	// ->获得MAXPHYADDR
	//
	UINT32 MaxPhyAddrBits = GetMaxPhyAddrBits();
	DbgPrint("MAXPHYADDR: %p\r\n", MaxPhyAddrBits);

	UINT64 MaxPhyAddr = (((UINT64)1) << MaxPhyAddrBits) - 1;	// 最大物理地址

#ifdef _WIN64
	//
	// ->获得PML4 PML4E 相关信息
	//

	PageMapLevel4Table = __readcr3() & MaxPhyAddr & ~PAGE_MASK;

	// 根据VA[47:39]获得 PML4E
	PageMapLevel4Entry.QuadPart = GetPxeAddress(VA, PageMapLevel4Table);

	DbgPrint("PDPTE: %p\r\n", PageMapLevel4Entry.QuadPart);

	PML4E_MapVA = (PUINT64)MmMapIoSpace(PageMapLevel4Entry, sizeof(PageMapLevel4Entry), MmNonCached);

	//
	// ->获得PDT PDTE 相关信息
	//

	PageDirectoryPointerTable = *PML4E_MapVA & MaxPhyAddr & ~PAGE_MASK;

	DbgPrint("PDPT: %p\r\n", PageDirectoryPointerTable);

	// 根据VA[38:30]获得 PDPTE
	PageDirectoryPointerEntry.QuadPart = GetPpeAddress(VA, PageDirectoryPointerTable);

	DbgPrint("PDPTE: %p\r\n", PageDirectoryPointerEntry.QuadPart);

	PDPTE_MapVA = (PUINT64)MmMapIoSpace(PageDirectoryPointerEntry, sizeof(PageDirectoryPointerEntry), MmNonCached);	// 将物理地址映射到一个虚拟地址，可以让程序直接访问虚拟地址得到物理地址保存的值

	if (!IsBitZero(*PDPTE_MapVA, 7))    // 判断PS是否等于1
	{
		// 1G
		DbgPrint("1G page frame!\r\n");

		//
		// ->获得PageFrame基地址，根据VA[29:0]获得页内偏移，拿到VA对应的PA
		//

		PageFrame = *PDTE_MapVA & MaxPhyAddr & ~PAGE_1G_MASK;		// PTDE[51:30] 获得页面基地址

		DbgPrint("page frame: %p\r\n", PageFrame);

		*PA = GetPhysicalAddress_1G(VA, PageFrame);

		DbgPrint("PA: %x\r\n", *PA);
	}
	else
	{
#else

	//
	// ->获得PDPT PDPTE相关信息
	//
	
	// 读取cr3，得到PDPT基地址
	PageDirectoryPointerTable = (UINT32)(__readcr3() & ~0x1f);		// 低5位忽略

	DbgPrint("PDPT: %p\r\n", PageDirectoryPointerTable);

	// 根据VA[31:30]获得 PDPTE
	PageDirectoryPointerEntry.QuadPart = GetPpeAddress(VA, PageDirectoryPointerTable);

	DbgPrint("PDPTE: %p\r\n", PageDirectoryPointerEntry.QuadPart);

	PDPTE_MapVA = (PUINT64)MmMapIoSpace(PageDirectoryPointerEntry, sizeof(PageDirectoryPointerEntry), MmNonCached);	// 将物理地址映射到一个虚拟地址，可以让程序直接访问虚拟地址得到物理地址保存的值

#endif

		// 4K/2M
		//
		// ->获得PDT PDTE 相关信息
		//

		// 读取PDPTE物理地址存储值
		PageDirectoryTable = *PDPTE_MapVA & MaxPhyAddr & ~PAGE_MASK;

		DbgPrint("PDT: %p\r\n", PageDirectoryTable);

		// 根据VA[29:21]获得 PDTE
		PageDirectoryEntry.QuadPart = GetPdeAddress(VA, PageDirectoryTable);

		DbgPrint("PDE: %p\r\n", PageDirectoryEntry.QuadPart);

		PDTE_MapVA = (PUINT64)MmMapIoSpace(PageDirectoryEntry, sizeof(PageDirectoryEntry), MmNonCached);

		//
		// ->根据 PDTE.PS 判断 2M/4K 进行下一步
		//

		// 通过 PDTE.PS 使用4k page frame or 2M page frame
		if (*PDTE_MapVA & 0x80)  // PDTE.PS = 1 --> 2M
		{
			DbgPrint("2M page frame!\r\n");

			//
			// ->获得PageFrame基地址，根据VA[20:0]获得页内偏移，拿到VA对应的PA
			//

			PageFrame = *PDTE_MapVA & MaxPhyAddr & ~PAGE_2M_MASK;		// PTDE[51:21] 获得页面基地址

			DbgPrint("page frame: %p\r\n", PageFrame);

			*PA = GetPhysicalAddress_2M(VA, PageFrame);

			DbgPrint("PA: %x\r\n", *PA);
		}
		else					  // PDTE.PS = 0 --> 4K
		{
			DbgPrint("4K page frame!\r\n");

			//
			// 5->获得PT PTE相关信息
			//

			PageTable = *PDTE_MapVA & MaxPhyAddr & ~PAGE_MASK;

			DbgPrint("PT: %p\r\n", PageTable);

			// 根据VA[20:12]获得 PTE
			PageTableEntry.QuadPart = (UINT32)GetPteAddress(VA, PageTable);

			DbgPrint("PTE: %p\r\n", PageTableEntry.QuadPart);

			PTE_MapVA = (PUINT64)MmMapIoSpace(PageTableEntry, sizeof(PageTableEntry), MmNonCached);

			//
			// ->根据PTE拿到Page Frame，VA[11:0]获得offset
			//
			PageFrame = *PTE_MapVA & MaxPhyAddr & ~PAGE_MASK;
			*PA = GetPhysicalAddress(VA, PageFrame);

			DbgPrint("PA: %x\r\n", *PA);
		}
#ifdef _WIN64
	}
#endif
	
	// 释放资源
	if (PML4E_MapVA)
	{
		MmUnmapIoSpace(PML4E_MapVA, sizeof(PageMapLevel4Table));
	}
	if (PDPTE_MapVA)
	{
		MmUnmapIoSpace(PDPTE_MapVA, sizeof(PageDirectoryPointerTable));
	}
	if (PDTE_MapVA)
	{
		MmUnmapIoSpace(PDTE_MapVA, sizeof(PageDirectoryTable));
	}
	if (PTE_MapVA)
	{
		MmUnmapIoSpace(PTE_MapVA, sizeof(PageTable));
	}
}



EXTERN_C_END