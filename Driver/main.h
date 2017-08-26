#pragma once

#include <ntifs.h>
#include <intrin.h>

EXTERN_C_START

#define DEVICE_NAME  L"\\Device\\VAToPADeviceName"
#define LINK_NAME    L"\\??\\VAToPALinkName"


#define IOCTL_VATOPA  (ULONG)CTL_CODE(FILE_DEVICE_UNKNOWN, \
	0x801, METHOD_NEITHER, FILE_READ_ACCESS | FILE_WRITE_ACCESS)


#define VIRTUAL_ADDRESS_BITS 48
#define VIRTUAL_ADDRESS_MASK ((((UINT64)1) << VIRTUAL_ADDRESS_BITS) - 1)

#ifndef PAGE_SHIFT
#define PAGE_SHIFT 12L
#endif

#ifndef PXI_SHIFT
#define PXI_SHIFT 39
#endif

#ifndef PPI_SHIFT
#define PPI_SHIFT 30
#endif

#ifndef PDI_SHIFT
#define PDI_SHIFT 21
#endif

#ifndef PTI_SHIFT
#define PTI_SHIFT 12
#endif

#ifndef PTE_SHIFT
#define PTE_SHIFT 3
#endif

#ifndef PXE_PER_PAGE
#define PXE_PER_PAGE 512
#endif

#ifdef _WIN64
#ifndef PPE_PER_PAGE
#define PPE_PER_PAGE 512
#endif
#else
#ifndef PPE_PER_PAGE
#define PPE_PER_PAGE 4
#endif
#endif // _WIN64

#ifndef PDE_PER_PAGE
#define PDE_PER_PAGE 512
#endif

#ifndef PTE_PER_PAGE
#define PTE_PER_PAGE 512			// pte表大小
#endif

#define PAGE_SIZE_2M_BITS 21
#define PAGE_SIZE_1G_BITS 29

#define PXI_MASK (PXE_PER_PAGE -1)
#define PPI_MASK (PPE_PER_PAGE - 1)
#define PDI_MASK (PDE_PER_PAGE - 1)
#define PTI_MASK (PTE_PER_PAGE - 1)
#define PAGE_MASK (PAGE_SIZE - 1)
#define PAGE_2M_MASK ((((UINT64)1) << PAGE_SIZE_2M_BITS) - 1)
#define PAGE_1G_MASK ((((UINT64)1) << PAGE_SIZE_1G_BITS) - 1)

// bit 为 0 返回TRUE; 为1 返回FALSE
#define IsBitZero(Va, Bit) \
	(((Va >> Bit) & 1) ? FALSE : TRUE)

#define GetPxeAddress(Va, Pml4Base) \
	((((((UINT64)(Va) & VIRTUAL_ADDRESS_MASK) >> PXI_SHIFT) & PXI_MASK) << PTE_SHIFT) + Pml4Base)

#define GetPpeAddress(Va, PpeBase) \
	((((((UINT64)(Va) & VIRTUAL_ADDRESS_MASK) >> PPI_SHIFT) & PPI_MASK) << PTE_SHIFT) + PpeBase)

#define GetPdeAddress(Va, PdeBase) \
	((((((UINT64)(Va) & VIRTUAL_ADDRESS_MASK) >> PDI_SHIFT) & PDI_MASK) << PTE_SHIFT) + PdeBase)

#define GetPteAddress(Va, PteBase) \
	((((((UINT64)(Va) & VIRTUAL_ADDRESS_MASK) >> PTI_SHIFT) & PTI_MASK) << PTE_SHIFT) + PteBase)

#define GetPhysicalAddress(Va, PageBase) \
	((((UINT64)(Va) & VIRTUAL_ADDRESS_MASK) & PAGE_MASK) + PageBase)

#define GetPhysicalAddress_2M(Va, PageBase) \
	((((UINT64)(Va) & VIRTUAL_ADDRESS_MASK) & PAGE_2M_MASK) + PageBase)

#define GetPhysicalAddress_1G(Va, PageBase) \
	((((UINT64)(Va) & VIRTUAL_ADDRESS_MASK) & PAGE_1G_MASK) + PageBase)


NTSTATUS 
DefaultPassThrough(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

VOID UnloadDriver(IN PDRIVER_OBJECT DriverObject);

NTSTATUS
IoControlPassThrough(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

UINT32
GetMaxPhyAddrBits();

VOID 
ConvertVAToPA(IN UINT_PTR VA, OUT PUINT64 PA);


EXTERN_C_END