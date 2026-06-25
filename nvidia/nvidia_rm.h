/*
 * Copyright 2026 - Open NVIDIA userspace driver project
 * SPDX-License-Identifier: MIT
 *
 * RM (Resource Manager) ioctl interface definitions derived from
 * open-gpu-kernel-modules (nv-ioctl*.h, nv_escape.h, nvos parameter layouts).
 * These mirror the kernel module ABI; do not modify without verifying against
 * the canonical kernel sources.
 */

#ifndef _NVIDIA_RM_H_
#define _NVIDIA_RM_H_

#include <stdint.h>
#include <stdbool.h>
#include <sys/ioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Basic NVIDIA types (subset of nvtypes.h) --- */
typedef uint8_t  NvU8;
typedef uint16_t NvU16;
typedef uint32_t NvU32;
typedef uint64_t NvU64;
typedef int16_t  NvS16;
typedef int32_t  NvS32;
typedef int64_t  NvS64;
typedef NvU32    NvHandle;
typedef NvU32    NvV32;
typedef NvU64    NvU64_ALIGN8;
typedef void    *NvP64;
typedef NvU8     NvBool;

#define NV_ALIGN_BYTES(a) __attribute__((aligned(a)))
#define NV_TRUE  ((NvBool)1)
#define NV_FALSE ((NvBool)0)

/* --- ioctl magic / escape numbers (nv-ioctl-numbers.h, nv_escape.h) --- */
#define NV_IOCTL_MAGIC      'F'
#define NV_IOCTL_BASE       200

#define NV_ESC_CARD_INFO             (NV_IOCTL_BASE + 0)
#define NV_ESC_REGISTER_FD           (NV_IOCTL_BASE + 1)
#define NV_ESC_ALLOC_OS_EVENT        (NV_IOCTL_BASE + 6)
#define NV_ESC_FREE_OS_EVENT         (NV_IOCTL_BASE + 7)
#define NV_ESC_STATUS_CODE           (NV_IOCTL_BASE + 9)
#define NV_ESC_CHECK_VERSION_STR     (NV_IOCTL_BASE + 10)
#define NV_ESC_IOCTL_XFER_CMD        (NV_IOCTL_BASE + 11)
#define NV_ESC_ATTACH_GPUS_TO_FD     (NV_IOCTL_BASE + 12)
#define NV_ESC_QUERY_DEVICE_INTR     (NV_IOCTL_BASE + 13)
#define NV_ESC_SYS_PARAMS            (NV_IOCTL_BASE + 14)
#define NV_ESC_EXPORT_TO_DMABUF_FD   (NV_IOCTL_BASE + 17)
#define NV_ESC_WAIT_OPEN_COMPLETE    (NV_IOCTL_BASE + 18)

/* RM escape commands (nv_escape.h) - passed as cmd in ioctl with NV_IOCTL_MAGIC */
#define NV_ESC_RM_ALLOC_MEMORY                      0x27
#define NV_ESC_RM_ALLOC_OBJECT                      0x28
#define NV_ESC_RM_FREE                              0x29
#define NV_ESC_RM_CONTROL                           0x2A
#define NV_ESC_RM_ALLOC                             0x2B
#define NV_ESC_RM_DUP_OBJECT                        0x34
#define NV_ESC_RM_SHARE                             0x35
#define NV_ESC_RM_IDLE_CHANNELS                     0x41
#define NV_ESC_RM_VID_HEAP_CONTROL                  0x4A
#define NV_ESC_RM_ACCESS_REGISTRY                   0x4D
#define NV_ESC_RM_MAP_MEMORY                        0x4E
#define NV_ESC_RM_UNMAP_MEMORY                      0x4F
#define NV_ESC_RM_GET_EVENT_DATA                    0x52
#define NV_ESC_RM_MAP_MEMORY_DMA                    0x57
#define NV_ESC_RM_UNMAP_MEMORY_DMA                  0x58
#define NV_ESC_RM_BIND_CONTEXT_DMA                  0x59
#define NV_ESC_RM_EXPORT_OBJECT_TO_FD               0x5C
#define NV_ESC_RM_IMPORT_OBJECT_FROM_FD             0x5D
#define NV_ESC_RM_UPDATE_DEVICE_MAPPING_INFO        0x5E

#define NV_IOC(cmd, type) _IOWR(NV_IOCTL_MAGIC, cmd, type)
#define NV_IOC_SIZE(cmd, size) _IOC(_IOC_READ|_IOC_WRITE, NV_IOCTL_MAGIC, cmd, size)

/* --- Well-known RM class handles (class/cl0000.h etc.) --- */
#define NV01_NULL_OBJECT        0x00000000
#define NV01_ROOT               0x00000000
#define NV01_ROOT_USER          0x00000041  /* alias for NV01_ROOT_CLIENT */
#define NV01_ROOT_CLIENT        0x00000041
#define NV01_DEVICE_0           0x00000080
#define NV20_SUBDEVICE_0        0x00002080
#define NV01_MEMORY_SYSTEM      0x0000003e
#define NV01_MEMORY_LOCAL_USER  0x0000003b
#define NV01_MEMORY_VIRTUAL     0x00000070
#define NV01_CONTEXT_DMA        0x00000002
#define NV01_EVENT              0x00000005
#define NV50_CHANNEL_GPFIFO     0x0000506f
#define KEPLER_CHANNEL_GPFIFO_A 0x0000a06f
#define PASCAL_CHANNEL_GPFIFO_A 0x0000c06f
#define VOLTA_CHANNEL_GPFIFO_A  0x0000c36f
#define TURING_CHANNEL_GPFIFO_A 0x0000c46f
#define AMPERE_CHANNEL_GPFIFO_A 0x0000c56f
#define HOPPER_CHANNEL_GPFIFO_A 0x0000c76f

/* NV0000 / NV0080 / NV2080 control command families (subset) */
#define NV0000_CTRL_CMD_GPU_GET_ATTACHED_IDS        0x201
#define NV0000_CTRL_CMD_GPU_GET_ID_INFO             0x205
#define NV0000_CTRL_CMD_GPU_GET_ID_INFO_V2          0x273
#define NV0000_CTRL_CMD_GPU_GET_PROBED_IDS          0x214
#define NV0000_CTRL_CMD_SYSTEM_GET_BUILD_VERSION    0x101
#define NV0000_CTRL_CMD_SYSTEM_GET_CPU_INFO         0x102
#define NV0000_CTRL_CMD_SYSTEM_GET_CHIPSET_INFO     0x104
#define NV0000_CTRL_CMD_SYSTEM_GET_PLATFORM_TYPE    0x111
#define NV0000_CTRL_CMD_CLIENT_GET_ADDR_SPACE_TYPE  0x0d04
#define NV0000_CTRL_GPU_INVALID_ID                  0xffffffff

/* tick98: SYSTEM_GET_BUILD_VERSION (two-phase: size then buffers) */
#define NV0000_CTRL_SYSTEM_BUILD_STRING_MAX         256u
typedef struct {
	NvU32 sizeOfStrings;
	NvU64 pDriverVersionBuffer NV_ALIGN_BYTES(8);
	NvU64 pVersionBuffer NV_ALIGN_BYTES(8);
	NvU64 pTitleBuffer NV_ALIGN_BYTES(8);
	NvU32 changelistNumber;
	NvU32 officialChangelistNumber;
} NV0000_CTRL_SYSTEM_GET_BUILD_VERSION_PARAMS;

typedef struct {
	NvU32 systemType; /* platform type enum */
} NV0000_CTRL_CMD_SYSTEM_GET_PLATFORM_TYPE_PARAMS;

#define NV0000_CTRL_SYSTEM_PLATFORM_TYPE_UNKNOWN    0u
#define NV0000_CTRL_SYSTEM_PLATFORM_TYPE_DESKTOP    1u
#define NV0000_CTRL_SYSTEM_PLATFORM_TYPE_MOBILE     2u

#define NV0080_CTRL_CMD_GPU_GET_CLASSLIST           0x800201
#define NV0080_CTRL_CMD_GPU_GET_NUM_SUBDEVICES      0x800280
#define NV0080_CTRL_CMD_GPU_GET_VIRTUALIZATION_MODE 0x800289
/* NV01_DEVICE_0 FIFO (ctrl0080fifo.h); target = h_device (0080), not subdevice */
#define NV0080_CTRL_CMD_FIFO_GET_CAPS                0x801701
#define NV0080_CTRL_CMD_FIFO_GET_ENGINE_CONTEXT_PROPERTIES 0x801707
#define NV0080_CTRL_CMD_FIFO_GET_CHANNELLIST         0x80170d
#define NV0080_CTRL_CMD_FIFO_GET_LATENCY_BUFFER_SIZE 0x80170e
#define NV0080_CTRL_CMD_FIFO_SET_CHANNEL_PROPERTIES  0x80170f
#define NV0080_CTRL_CMD_FIFO_STOP_RUNLIST            0x801711
#define NV0080_CTRL_CMD_FIFO_START_RUNLIST           0x801712
#define NV0080_CTRL_CMD_FIFO_GET_CAPS_V2             0x801713
#define NV0080_CTRL_CMD_FIFO_IDLE_CHANNELS           0x801714
#define NV0080_CTRL_FIFO_CAPS_TBL_SIZE               2
#define NV0080_CTRL_CMD_FIFO_IDLE_CHANNELS_MAX_CHANNELS 4096
/* Practical max for userspace helper (avoids 16KB stack/heap on every call) */
#define NV0080_CTRL_FIFO_IDLE_CHANNELS_HELPER_MAX    64

typedef struct {
	NvU32 engineID;
	NvU32 gpEntries;
	NvU32 pbEntries;
} NV0080_CTRL_FIFO_GET_LATENCY_BUFFER_SIZE_PARAMS;

typedef struct {
	NvU32 engineID;
} NV0080_CTRL_FIFO_STOP_RUNLIST_PARAMS;

typedef struct {
	NvU32 engineID;
} NV0080_CTRL_FIFO_START_RUNLIST_PARAMS;

typedef struct {
	NvU8 capsTbl[NV0080_CTRL_FIFO_CAPS_TBL_SIZE];
} NV0080_CTRL_FIFO_GET_CAPS_V2_PARAMS;

/* Full RM param has 4096 handles; helper uses compact form via custom call */
typedef struct {
	NvU32    numChannels;
	NvU32    flags;
	NvU32    timeout;
	/* Caller supplies handles via separate array in helper API */
} NV0080_CTRL_FIFO_IDLE_CHANNELS_META;

#define NV2080_CTRL_CMD_GPU_GET_NAME_STRING         0x20800110
#define NV2080_CTRL_CMD_GPU_GET_SHORT_NAME_STRING   0x20800111
#define NV2080_CTRL_CMD_GPU_GET_SIMULATION_INFO     0x20800119
/* tick99: OGKM ctrl2080gpu.h uses 0x2080014a (was mis-coded 0x12a in some trees) */
#define NV2080_CTRL_CMD_GPU_GET_GID_INFO            0x2080014a
#define NV2080_GPU_MAX_GID_LENGTH                   0x100u
#define NV2080_GPU_CMD_GPU_GET_GID_FLAGS_FORMAT_ASCII  0x00000000u
#define NV2080_GPU_CMD_GPU_GET_GID_FLAGS_FORMAT_BINARY 0x00000002u
#define NV2080_GPU_CMD_GPU_GET_GID_FLAGS_TYPE_SHA1     0x00000000u

typedef struct {
	NvU32 index;
	NvU32 flags;
	NvU32 length;
	NvU8  data[NV2080_GPU_MAX_GID_LENGTH];
} NV2080_CTRL_GPU_GET_GID_INFO_PARAMS;
#define NV2080_CTRL_CMD_GPU_GET_ENGINES             0x20800123
#define NV2080_CTRL_CMD_GPU_GET_ENGINES_V2          0x20800170
#define NV2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST    0x20800124
#define NV2080_GPU_MAX_ENGINES_LIST_SIZE            0x54
#define NV2080_CTRL_GPU_MAX_CLASSLIST               128
#define NV2080_CTRL_CMD_MC_GET_ARCH_INFO            0x20801701
#define NV2080_CTRL_CMD_FB_GET_INFO                 0x20801301
#define NV2080_CTRL_CMD_FB_GET_INFO_V2              0x20801303
#define NV2080_CTRL_CMD_FB_GET_FB_REGION_INFO       0x20801320
#define NV2080_CTRL_CMD_GR_GET_INFO                 0x20801201
#define NV2080_CTRL_CMD_GR_GET_INFO_V2              0x20801210
#define NV2080_CTRL_CMD_BUS_GET_INFO                0x20801801
#define NV2080_CTRL_CMD_BUS_GET_INFO_V2             0x20801803
/* BUS_GET_PCI_INFO shares 0x20801801 in OGKM as first bus cmd; keep BUS_GET_INFO alias */
#define NV2080_CTRL_CMD_BUS_GET_PCI_INFO            0x20801801
#define NV2080_CTRL_CMD_GPU_GET_MAX_SUPPORTED_PAGE_SIZE 0x20800188
#define NV2080_CTRL_CMD_TIMER_GET_TIME              0x20800403
#define NV2080_CTRL_CMD_EVENT_SET_NOTIFICATION      0x20800301

/* tick96: FB region / page size / PCI (ctrl2080fb / gpu / bus) */
#define NV2080_CTRL_CMD_FB_GET_FB_REGION_INFO_MEM_TYPES   18u
#define NV2080_CTRL_CMD_FB_GET_FB_REGION_INFO_MAX_ENTRIES 16u

typedef struct {
	NvU64  base NV_ALIGN_BYTES(8);
	NvU64  limit NV_ALIGN_BYTES(8);
	NvU64  reserved NV_ALIGN_BYTES(8);
	NvU32  performance;
	NvBool supportCompressed;
	NvBool supportISO;
	NvBool bProtected;
	NvBool blackList[NV2080_CTRL_CMD_FB_GET_FB_REGION_INFO_MEM_TYPES];
	NvU32  regionTag; /* NV2080_FB_REGION_TAG; keep as NvU32 for portability */
} NV2080_CTRL_CMD_FB_GET_FB_REGION_FB_REGION_INFO;

typedef struct {
	NvU32 numFBRegions;
	NV2080_CTRL_CMD_FB_GET_FB_REGION_FB_REGION_INFO
		fbRegion[NV2080_CTRL_CMD_FB_GET_FB_REGION_INFO_MAX_ENTRIES]
		NV_ALIGN_BYTES(8);
} NV2080_CTRL_CMD_FB_GET_FB_REGION_INFO_PARAMS;

typedef struct {
	NvU64 maxSupportedPageSize NV_ALIGN_BYTES(8);
} NV2080_CTRL_GPU_GET_MAX_SUPPORTED_PAGE_SIZE_PARAMS;

typedef struct {
	NvU32 pciDeviceId;
	NvU32 pciSubSystemId;
	NvU32 pciRevisionId;
	NvU32 pciExtDeviceId;
} NV2080_CTRL_BUS_GET_PCI_INFO_PARAMS;

/* tick95: timer + Unix object export/import (ctrl2080tmr / ctrl0000unix) */
typedef struct {
	NvU64 time_nsec NV_ALIGN_BYTES(8);
} NV2080_CTRL_TIMER_GET_TIME_PARAMS;

#define NV0000_CTRL_CMD_OS_UNIX_EXPORT_OBJECT_TO_FD   0x3d05
#define NV0000_CTRL_CMD_OS_UNIX_IMPORT_OBJECT_FROM_FD 0x3d06
#define NV0000_CTRL_OS_UNIX_EXPORT_OBJECT_TYPE_NONE   0x0u
#define NV0000_CTRL_OS_UNIX_EXPORT_OBJECT_TYPE_RM     0x1u
#define NV0000_CTRL_OS_UNIX_EXPORT_OBJECT_TO_FD_FLAGS_EMPTY_FD_FALSE 0x0u
#define NV0000_CTRL_OS_UNIX_EXPORT_OBJECT_TO_FD_FLAGS_EMPTY_FD_TRUE  0x1u

typedef struct {
	NvU32 type; /* NV0000_CTRL_OS_UNIX_EXPORT_OBJECT_TYPE_* */
	union {
		struct {
			NvHandle hDevice;
			NvHandle hParent;
			NvHandle hObject;
		} rmObject;
	} data;
} NV0000_CTRL_OS_UNIX_EXPORT_OBJECT;

typedef struct {
	NV0000_CTRL_OS_UNIX_EXPORT_OBJECT object;
	NvS32 fd;
	NvU32 flags;
} NV0000_CTRL_OS_UNIX_EXPORT_OBJECT_TO_FD_PARAMS;

typedef struct {
	NvS32 fd;
	NV0000_CTRL_OS_UNIX_EXPORT_OBJECT object;
} NV0000_CTRL_OS_UNIX_IMPORT_OBJECT_FROM_FD_PARAMS;

/* Common subdevice notifiers (cl2080.h subset; for EVENT_SET_NOTIFICATION) */
#define NV2080_NOTIFIERS_SW                         0x00000000u
#define NV2080_NOTIFIERS_HOTPLUG                    0x00000001u
#define NV2080_NOTIFIERS_POWER_CONNECTOR            0x00000002u
#define NV2080_NOTIFIERS_THERMAL_SW                 0x00000003u
#define NV2080_NOTIFIERS_THERMAL_HW                 0x00000004u
#define NV2080_NOTIFIERS_FULL_SCREEN_CHANGE         0x00000005u
#define NV2080_NOTIFIERS_EVENTBUFFER                0x00000006u
#define NV2080_NOTIFIERS_RC                         0x00000017u  /* channel RC recovery */

/* NVOS32 vidheap functions — exact values from open-gpu-kernel-modules nvos.h */
#define NVOS32_FUNCTION_ALLOC_SIZE                  2
#define NVOS32_FUNCTION_FREE                        3
#define NVOS32_FUNCTION_INFO                        5
#define NVOS32_FUNCTION_ALLOC_TILED_PITCH_HEIGHT    6
#define NVOS32_FUNCTION_ALLOC_SIZE_RANGE            14
#define NVOS32_FUNCTION_REACQUIRE_COMPR             15
#define NVOS32_FUNCTION_RELEASE_COMPR               16
#define NVOS32_FUNCTION_GET_MEM_ALIGNMENT           18
#define NVOS32_FUNCTION_HW_ALLOC                    19
#define NVOS32_FUNCTION_HW_FREE                     20
#define NVOS32_FUNCTION_ALLOC_OS_DESCRIPTOR         27

/* NVOS32 descriptor types (for ALLOC_OS_DESCRIPTOR / import paths) */
#define NVOS32_DESCRIPTOR_TYPE_VIRTUAL_ADDRESS      0
#define NVOS32_DESCRIPTOR_TYPE_OS_PAGE_ARRAY        1
#define NVOS32_DESCRIPTOR_TYPE_OS_IO_MEMORY         2
#define NVOS32_DESCRIPTOR_TYPE_OS_PHYS_ADDR         3
#define NVOS32_DESCRIPTOR_TYPE_OS_FILE_HANDLE       4
#define NVOS32_DESCRIPTOR_TYPE_OS_DMA_BUF_PTR       5
#define NVOS32_DESCRIPTOR_TYPE_OS_SGT_PTR           6
#define NVOS32_DESCRIPTOR_TYPE_KERNEL_VIRTUAL_ADDRESS 7

#define NVOS32_TYPE_IMAGE                           0
#define NVOS32_TYPE_DEPTH                           1
#define NVOS32_TYPE_TEXTURE                         2
#define NVOS32_TYPE_VIDEO                           3
#define NVOS32_TYPE_FONT                            4
#define NVOS32_TYPE_CURSOR                          5
#define NVOS32_TYPE_DMA                             6
#define NVOS32_TYPE_INSTANCE                        7
#define NVOS32_TYPE_PRIMARY                         8
#define NVOS32_TYPE_ZCULL                           9
#define NVOS32_TYPE_OWNER_RM                        10
#define NVOS32_TYPE_NOTIFIER                        11
#define NVOS32_TYPE_SHADER_PROGRAM                  12
#define NVOS32_TYPE_RESERVED                        13
#define NVOS32_TYPE_PM                              14
#define NVOS32_TYPE_HEAP_RESERVED                   15
#define NVOS32_TYPE_STENCIL                         16

/* NVOS32_ALLOC_FLAGS_* - exact values from open-gpu-kernel-modules nvos.h */
#define NVOS32_ALLOC_FLAGS_IGNORE_BANK_PLACEMENT    0x00000001
#define NVOS32_ALLOC_FLAGS_FORCE_MEM_GROWS_UP       0x00000002
#define NVOS32_ALLOC_FLAGS_FORCE_MEM_GROWS_DOWN     0x00000004
#define NVOS32_ALLOC_FLAGS_FORCE_ALIGN_HOST_PAGE    0x00000008
#define NVOS32_ALLOC_FLAGS_FIXED_ADDRESS_ALLOCATE   0x00000010
#define NVOS32_ALLOC_FLAGS_BANK_HINT                0x00000020
#define NVOS32_ALLOC_FLAGS_BANK_FORCE               0x00000040
#define NVOS32_ALLOC_FLAGS_ALIGNMENT_HINT           0x00000080
#define NVOS32_ALLOC_FLAGS_ALIGNMENT_FORCE          0x00000100
#define NVOS32_ALLOC_FLAGS_BANK_GROW_DOWN           0x00000200
#define NVOS32_ALLOC_FLAGS_LAZY                     0x00000400
#define NVOS32_ALLOC_FLAGS_NO_SCANOUT               0x00001000
#define NVOS32_ALLOC_FLAGS_PITCH_FORCE              0x00002000
#define NVOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED   0x00004000
#define NVOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED         0x00008000
#define NVOS32_ALLOC_FLAGS_PERSISTENT_VIDMEM        0x00010000
#define NVOS32_ALLOC_FLAGS_USE_BEGIN_END            0x00020000
#define NVOS32_ALLOC_FLAGS_TURBO_CIPHER_ENCRYPTED   0x00040000
#define NVOS32_ALLOC_FLAGS_VIRTUAL                  0x00080000
#define NVOS32_ALLOC_FLAGS_KERNEL_MAPPING_MAP       0x02000000
#define NVOS32_ALLOC_FLAGS_SPARSE                   0x04000000
#define NVOS32_ALLOC_FLAGS_USER_READ_ONLY           0x04000000
#define NVOS32_ALLOC_FLAGS_DEVICE_READ_ONLY         0x08000000

/* RM status codes (subset of nvstatus.h) */
#define NV_OK                                       0x00000000
#define NV_ERR_GENERIC                              0x0000ffff
#define NV_ERR_INVALID_ARGUMENT                     0x0000001f
#define NV_ERR_INVALID_OBJECT_HANDLE                0x00000031
#define NV_ERR_INVALID_OBJECT_PARENT                0x00000032
#define NV_ERR_INSUFFICIENT_RESOURCES               0x00000038
#define NV_ERR_INVALID_OPERATION                    0x0000003c
#define NV_ERR_NOT_SUPPORTED                        0x0000004b
#define NV_ERR_GPU_IS_LOST                          0x0000005b
#define NV_ERR_NO_MEMORY                            0x00000051
#define NV_ERR_OPERATING_SYSTEM                     0x0000003e

/* --- ioctl parameter structures (from nv-ioctl.h) --- */

typedef struct {
	NvU32 domain;
	NvU8  bus;
	NvU8  slot;
	NvU8  function;
	NvU16 vendor_id;
	NvU16 device_id;
} nv_pci_info_t;

typedef struct nv_ioctl_xfer {
	NvU32 cmd;
	NvU32 size;
	NvU64 ptr NV_ALIGN_BYTES(8);
} nv_ioctl_xfer_t;

typedef struct nv_ioctl_card_info {
	NvBool        valid;
	nv_pci_info_t pci_info;
	NvU32         gpu_id;
	NvU16         interrupt_line;
	NvU64         reg_address NV_ALIGN_BYTES(8);
	NvU64         reg_size    NV_ALIGN_BYTES(8);
	NvU64         fb_address  NV_ALIGN_BYTES(8);
	NvU64         fb_size     NV_ALIGN_BYTES(8);
	NvU32         minor_number;
	NvU8          dev_name[10];
} nv_ioctl_card_info_t;

#define NV_MAX_DEVICES 32

typedef struct nv_ioctl_alloc_os_event {
	NvHandle hClient;
	NvHandle hDevice;
	NvU32    fd;
	NvU32    Status;
} nv_ioctl_alloc_os_event_t;

typedef struct nv_ioctl_free_os_event {
	NvHandle hClient;
	NvHandle hDevice;
	NvU32    fd;
	NvU32    Status;
} nv_ioctl_free_os_event_t;

typedef struct nv_ioctl_status_code {
	NvU32 domain;
	NvU8  bus;
	NvU8  slot;
	NvU32 status;
} nv_ioctl_status_code_t;

#define NV_RM_API_VERSION_STRING_LENGTH 64

typedef struct nv_ioctl_rm_api_version {
	NvU32 cmd;
	NvU32 reply;
	char  versionString[NV_RM_API_VERSION_STRING_LENGTH];
} nv_ioctl_rm_api_version_t;

#define NV_RM_API_VERSION_CMD_STRICT   0
#define NV_RM_API_VERSION_CMD_RELAXED  '1'
#define NV_RM_API_VERSION_CMD_QUERY    '2'
#define NV_RM_API_VERSION_REPLY_UNRECOGNIZED 0
#define NV_RM_API_VERSION_REPLY_RECOGNIZED   1

typedef struct nv_ioctl_sys_params {
	NvU64 memblock_size NV_ALIGN_BYTES(8);
} nv_ioctl_sys_params_t;

typedef struct nv_ioctl_register_fd {
	int ctl_fd;
} nv_ioctl_register_fd_t;

#define NV_DMABUF_EXPORT_MAX_HANDLES 128
#define NV_DMABUF_EXPORT_MAPPING_TYPE_DEFAULT    0
#define NV_DMABUF_EXPORT_MAPPING_TYPE_FORCE_PCIE 1

typedef struct nv_ioctl_export_to_dma_buf_fd {
	int      fd;
	NvHandle hClient;
	NvU32    totalObjects;
	NvU32    numObjects;
	NvU32    index;
	NvU64    totalSize NV_ALIGN_BYTES(8);
	NvU8     mappingType;
	NvBool   bAllowMmap;
	NvHandle handles[NV_DMABUF_EXPORT_MAX_HANDLES];
	NvU64    offsets[NV_DMABUF_EXPORT_MAX_HANDLES] NV_ALIGN_BYTES(8);
	NvU64    sizes[NV_DMABUF_EXPORT_MAX_HANDLES]   NV_ALIGN_BYTES(8);
	NvU32    status;
} nv_ioctl_export_to_dma_buf_fd_t;

/* --- NVOS parameter structures (from nvos.h layouts in kernel module) --- */

/* NVOS00: free */
typedef struct {
	NvHandle hRoot;
	NvHandle hObjectParent;
	NvHandle hObjectOld;
	NvV32    status;
} NVOS00_PARAMETERS;

/* NVOS02: alloc memory */
typedef struct {
	NvHandle hRoot;
	NvHandle hObjectParent;
	NvHandle hObjectNew;
	NvV32    hClass;
	NvU32    flags;
	NvU64    pMemory NV_ALIGN_BYTES(8);
	NvU64    limit   NV_ALIGN_BYTES(8);
	NvV32    status;
} NVOS02_PARAMETERS;

typedef struct {
	NVOS02_PARAMETERS params;
	int fd;
} nv_ioctl_nvos02_parameters_with_fd;

/* NVOS05: alloc object (legacy) */
typedef struct {
	NvHandle hRoot;
	NvHandle hObjectParent;
	NvHandle hObjectNew;
	NvV32    hClass;
	NvV32    status;
} NVOS05_PARAMETERS;

/* NVOS21 / NVOS64: alloc (generic object) - from nvos.h */
typedef struct {
	NvHandle hRoot;
	NvHandle hObjectParent;
	NvHandle hObjectNew;
	NvV32    hClass;
	NvU64    pAllocParms NV_ALIGN_BYTES(8);
	NvU32    paramsSize;
	NvV32    status;
} NVOS21_PARAMETERS;

typedef struct {
	NvHandle hRoot;
	NvHandle hObjectParent;
	NvHandle hObjectNew;
	NvV32    hClass;
	NvU64    pAllocParms NV_ALIGN_BYTES(8);
	NvU64    pRightsRequested NV_ALIGN_BYTES(8);
	NvU32    paramsSize;
	NvU32    flags;
	NvV32    status;
} NVOS64_PARAMETERS;

/* NVOS33: map memory */
typedef struct {
	NvHandle hClient;
	NvHandle hDevice;
	NvHandle hMemory;
	NvU64    offset  NV_ALIGN_BYTES(8);
	NvU64    length  NV_ALIGN_BYTES(8);
	NvU64    pLinearAddress NV_ALIGN_BYTES(8);
	NvU32    status;
	NvU32    flags;
} NVOS33_PARAMETERS;

typedef struct {
	NVOS33_PARAMETERS params;
	int fd;
} nv_ioctl_nvos33_parameters_with_fd;

/* NVOS34: unmap memory */
typedef struct {
	NvHandle hClient;
	NvHandle hDevice;
	NvHandle hMemory;
	NvU64    pLinearAddress NV_ALIGN_BYTES(8);
	NvU32    status;
	NvU32    flags;
} NVOS34_PARAMETERS;

/* NVOS54: control (RmControl) */
typedef struct {
	NvHandle hClient;
	NvHandle hObject;
	NvV32    cmd;
	NvU32    flags;
	NvU64    params NV_ALIGN_BYTES(8);
	NvU32    paramsSize;
	NvU32    status;
} NVOS54_PARAMETERS;

/* NVOS55: dup object (NV_ESC_RM_DUP_OBJECT) */
typedef struct {
	NvHandle hClient;
	NvHandle hParent;
	NvHandle hObject;
	NvHandle hClientSrc;
	NvHandle hObjectSrc;
	NvU32    flags;
	NvU32    status;
} NVOS55_PARAMETERS;

#define NV04_DUP_HANDLE_FLAGS_NONE                        0x00000000u
#define NV04_DUP_HANDLE_FLAGS_REJECT_KERNEL_DUP_PRIVILEGE 0x00000001u

/* NVOS41: get event data (NV_ESC_RM_GET_EVENT_DATA) */
typedef struct {
	NvHandle hObject;
	NvV32    NotifyIndex;
	NvV32    info32;
	NvU16    info16;
} NvUnixEvent;

typedef struct {
	NvU64 pEvent NV_ALIGN_BYTES(8); /* pointer to NvUnixEvent */
	NvV32 MoreEvents;
	NvV32 status;
} NVOS41_PARAMETERS;

/* NV0005: NV01_EVENT alloc params (cl0005.h) */
typedef struct {
	NvHandle hParentClient;
	NvHandle hSrcResource;
	NvV32    hClass;
	NvV32    notifyIndex;
	NvU64    data NV_ALIGN_BYTES(8); /* OS event fd as pointer/handle on Linux */
} NV0005_ALLOC_PARAMETERS;

/* NV01_EVENT notifyIndex flags (OR into notifyIndex; nvos.h) */
#define NV01_EVENT_BROADCAST                                       0x80000000u
#define NV01_EVENT_PERMIT_NON_ROOT_EVENT_KERNEL_CALLBACK_CREATION  0x40000000u
#define NV01_EVENT_SUBDEVICE_SPECIFIC                              0x20000000u
#define NV01_EVENT_WITHOUT_EVENT_DATA                              0x10000000u
#define NV01_EVENT_NONSTALL_INTR                                   0x08000000u
#define NV01_EVENT_CLIENT_RM                                       0x04000000u

/* NV2080_CTRL_CMD_EVENT_SET_NOTIFICATION (ctrl2080event.h; target = subdevice) */
typedef struct {
	NvU32  event;
	NvU32  action;
	NvBool bNotifyState;
	NvU32  info32;
	NvU16  info16;
} NV2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS;

#define NV2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_DISABLE 0x00000000u
#define NV2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_SINGLE  0x00000001u
#define NV2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT  0x00000002u

/* NVOS32: vid heap control - outer header + data union from nvos.h */
#define NVOS32_FREE_FLAGS_MEMORY_HANDLE_PROVIDED    0x00000001
#define NVOS32_FUNCTION_ALLOC_SIZE_RANGE            14

/*
 * NVOS32_ATTR / ATTR2 — exact field positions from open-gpu-kernel-modules nvos.h
 * (tick100: previous tree had wrong bit positions; allocs could mis-target memory).
 * Field values are unshifted DRF selectors; use NV_OS32_ATTR_MAKE / NV_OS32_ATTR2_MAKE.
 */
#define NVOS32_ATTR_DEPTH                           2:0
#define NVOS32_ATTR_DEPTH_UNKNOWN                   0x00000000
#define NVOS32_ATTR_DEPTH_8                         0x00000001
#define NVOS32_ATTR_DEPTH_16                        0x00000002
#define NVOS32_ATTR_DEPTH_24                        0x00000003
#define NVOS32_ATTR_DEPTH_32                        0x00000004
#define NVOS32_ATTR_DEPTH_64                        0x00000005
#define NVOS32_ATTR_DEPTH_128                       0x00000006

#define NVOS32_ATTR_ZCULL                           11:10
#define NVOS32_ATTR_ZCULL_NONE                      0x00000000
#define NVOS32_ATTR_ZCULL_REQUIRED                  0x00000001
#define NVOS32_ATTR_ZCULL_ANY                       0x00000002
#define NVOS32_ATTR_ZCULL_SHARED                    0x00000003

#define NVOS32_ATTR_COMPR                           13:12
#define NVOS32_ATTR_COMPR_NONE                      0x00000000
#define NVOS32_ATTR_COMPR_REQUIRED                  0x00000001
#define NVOS32_ATTR_COMPR_ANY                       0x00000002

#define NVOS32_ATTR_FORMAT                          17:16
#define NVOS32_ATTR_FORMAT_PITCH                    0x00000000
#define NVOS32_ATTR_FORMAT_BLOCK_LINEAR             0x00000001

#define NVOS32_ATTR_PAGE_SIZE                       24:23
#define NVOS32_ATTR_PAGE_SIZE_DEFAULT               0x00000000
#define NVOS32_ATTR_PAGE_SIZE_4KB                   0x00000001
#define NVOS32_ATTR_PAGE_SIZE_BIG                   0x00000002
#define NVOS32_ATTR_PAGE_SIZE_HUGE                  0x00000003

#define NVOS32_ATTR_LOCATION                        26:25
#define NVOS32_ATTR_LOCATION_VIDMEM                 0x00000000
#define NVOS32_ATTR_LOCATION_PCI                    0x00000001
#define NVOS32_ATTR_LOCATION_ANY                    0x00000003

#define NVOS32_ATTR_PHYSICALITY                     28:27
#define NVOS32_ATTR_PHYSICALITY_DEFAULT             0x00000000
#define NVOS32_ATTR_PHYSICALITY_NONCONTIGUOUS       0x00000001
#define NVOS32_ATTR_PHYSICALITY_CONTIGUOUS          0x00000002
#define NVOS32_ATTR_PHYSICALITY_ALLOW_NONCONTIGUOUS 0x00000003

#define NVOS32_ATTR_COHERENCY                       31:29
#define NVOS32_ATTR_COHERENCY_UNCACHED              0x00000000
#define NVOS32_ATTR_COHERENCY_CACHED                0x00000001
#define NVOS32_ATTR_COHERENCY_WRITE_COMBINE         0x00000002
#define NVOS32_ATTR_COHERENCY_WRITE_THROUGH         0x00000003
#define NVOS32_ATTR_COHERENCY_WRITE_PROTECT         0x00000004
#define NVOS32_ATTR_COHERENCY_WRITE_BACK            0x00000005

#define NVOS32_ATTR2_ZBC                            1:0
#define NVOS32_ATTR2_ZBC_DEFAULT                    0x00000000
#define NVOS32_ATTR2_ZBC_PREFER_NO_ZBC              0x00000001
#define NVOS32_ATTR2_ZBC_PREFER_ZBC                 0x00000002
#define NVOS32_ATTR2_ZBC_REQUIRE_ONLY_ZBC           0x00000003

#define NVOS32_ATTR2_GPU_CACHEABLE                  3:2
#define NVOS32_ATTR2_GPU_CACHEABLE_DEFAULT          0x00000000
#define NVOS32_ATTR2_GPU_CACHEABLE_YES              0x00000001
#define NVOS32_ATTR2_GPU_CACHEABLE_NO               0x00000002
#define NVOS32_ATTR2_GPU_CACHEABLE_INVALID          0x00000003

#define NVOS32_ATTR2_P2P_GPU_CACHEABLE              5:4
#define NVOS32_ATTR2_P2P_GPU_CACHEABLE_DEFAULT      0x00000000
#define NVOS32_ATTR2_P2P_GPU_CACHEABLE_YES          0x00000001
#define NVOS32_ATTR2_P2P_GPU_CACHEABLE_NO           0x00000002

#define NVOS32_ATTR2_32BIT_POINTER                  6:6
#define NVOS32_ATTR2_32BIT_POINTER_DISABLE          0x00000000
#define NVOS32_ATTR2_32BIT_POINTER_ENABLE           0x00000001

/* DRF-style compose: shift unshifted field value into its bit range */
#define NV_OS32_DRF_SHL(lo, hi, val) \
	(((NvU32)(val) & ((1u << ((hi) - (lo) + 1)) - 1u)) << (lo))

/* PAGE_SIZE@24:23, LOCATION@26:25, PHYSICALITY@28:27, COHERENCY@31:29 */
#define NV_OS32_ATTR_MAKE(loc, pgsz, coh, phys) \
	(NV_OS32_DRF_SHL(23, 24, (pgsz)) | \
	 NV_OS32_DRF_SHL(25, 26, (loc)) | \
	 NV_OS32_DRF_SHL(27, 28, (phys)) | \
	 NV_OS32_DRF_SHL(29, 31, (coh)))

/* ZBC@1:0, GPU_CACHEABLE@3:2 */
#define NV_OS32_ATTR2_MAKE(zbc, gpu_cache) \
	(NV_OS32_DRF_SHL(0, 1, (zbc)) | NV_OS32_DRF_SHL(2, 3, (gpu_cache)))

#define NV_OS32_ATTR_VIDMEM_4K_UNCACHED \
	NV_OS32_ATTR_MAKE(NVOS32_ATTR_LOCATION_VIDMEM, NVOS32_ATTR_PAGE_SIZE_4KB, \
			  NVOS32_ATTR_COHERENCY_UNCACHED, NVOS32_ATTR_PHYSICALITY_DEFAULT)
#define NV_OS32_ATTR_PCI_4K_UNCACHED \
	NV_OS32_ATTR_MAKE(NVOS32_ATTR_LOCATION_PCI, NVOS32_ATTR_PAGE_SIZE_4KB, \
			  NVOS32_ATTR_COHERENCY_UNCACHED, NVOS32_ATTR_PHYSICALITY_DEFAULT)
#define NV_OS32_ATTR_PCI_4K_WRITECOMBINE \
	NV_OS32_ATTR_MAKE(NVOS32_ATTR_LOCATION_PCI, NVOS32_ATTR_PAGE_SIZE_4KB, \
			  NVOS32_ATTR_COHERENCY_WRITE_COMBINE, NVOS32_ATTR_PHYSICALITY_DEFAULT)
#define NV_OS32_ATTR_VIDMEM_4K_CACHED \
	NV_OS32_ATTR_MAKE(NVOS32_ATTR_LOCATION_VIDMEM, NVOS32_ATTR_PAGE_SIZE_4KB, \
			  NVOS32_ATTR_COHERENCY_CACHED, NVOS32_ATTR_PHYSICALITY_DEFAULT)
/* allow non-contig vidmem fallback (common for large BOs) */
#define NV_OS32_ATTR_VIDMEM_4K_UNCACHED_NONCONTIG \
	NV_OS32_ATTR_MAKE(NVOS32_ATTR_LOCATION_VIDMEM, NVOS32_ATTR_PAGE_SIZE_4KB, \
			  NVOS32_ATTR_COHERENCY_UNCACHED, \
			  NVOS32_ATTR_PHYSICALITY_ALLOW_NONCONTIGUOUS)
/* tick103: block-linear vidmem (FORMAT@17:16 = BLOCK_LINEAR) for 2D/3D surfaces */
#define NV_OS32_ATTR_VIDMEM_4K_UNCACHED_BL \
	(NV_OS32_ATTR_VIDMEM_4K_UNCACHED | \
	 NV_OS32_DRF_SHL(16, 17, NVOS32_ATTR_FORMAT_BLOCK_LINEAR))
#define NV_OS32_ATTR_VIDMEM_4K_UNCACHED_BL_NONCONTIG \
	(NV_OS32_ATTR_VIDMEM_4K_UNCACHED_NONCONTIG | \
	 NV_OS32_DRF_SHL(16, 17, NVOS32_ATTR_FORMAT_BLOCK_LINEAR))
#define NV_OS32_ATTR2_GPU_CACHEABLE_NO_VAL \
	NV_OS32_ATTR2_MAKE(NVOS32_ATTR2_ZBC_DEFAULT, NVOS32_ATTR2_GPU_CACHEABLE_NO)
#define NV_OS32_ATTR2_GPU_CACHEABLE_DEFAULT_VAL \
	NV_OS32_ATTR2_MAKE(NVOS32_ATTR2_ZBC_DEFAULT, NVOS32_ATTR2_GPU_CACHEABLE_DEFAULT)
#define NV_OS32_ATTR2_GPU_CACHEABLE_YES_VAL \
	NV_OS32_ATTR2_MAKE(NVOS32_ATTR2_ZBC_DEFAULT, NVOS32_ATTR2_GPU_CACHEABLE_YES)

/* NV_MEMORY_ALLOCATION_PARAMS - RmAlloc class params (nvos.h) */
typedef struct {
	NvU32    owner;
	NvU32    type;
	NvU32    flags;
	NvU32    width;
	NvU32    height;
	NvS32    pitch;
	NvU32    attr;
	NvU32    attr2;
	NvU32    format;
	NvU32    comprCovg;
	NvU32    zcullCovg;
	NvU64    rangeLo   NV_ALIGN_BYTES(8);
	NvU64    rangeHi   NV_ALIGN_BYTES(8);
	NvU64    size      NV_ALIGN_BYTES(8);
	NvU64    alignment NV_ALIGN_BYTES(8);
	NvU64    offset    NV_ALIGN_BYTES(8);
	NvU64    limit     NV_ALIGN_BYTES(8);
	NvU64    address   NV_ALIGN_BYTES(8);
	NvU32    ctagOffset;
	NvHandle hVASpace;
	NvU32    internalflags;
	NvU32    tag;
	NvS32    numaNode;
} NV_MEMORY_ALLOCATION_PARAMS;

/* NVOS32_PARAMETERS full layout (outer + data union) */
typedef struct {
	NvHandle hRoot;
	NvHandle hObjectParent;
	NvU32    function;
	NvHandle hVASpace;
	NvS16    ivcHeapNumber;
	NvU16    pad;
	NvV32    status;
	NvU64    total NV_ALIGN_BYTES(8);
	NvU64    free  NV_ALIGN_BYTES(8);
	union {
		struct {
			NvU32    owner;
			NvHandle hMemory;
			NvU32    type;
			NvU32    flags;
			NvU32    attr;
			NvU32    format;
			NvU32    comprCovg;
			NvU32    zcullCovg;
			NvU32    partitionStride;
			NvU32    width;
			NvU32    height;
			NvU64    size      NV_ALIGN_BYTES(8);
			NvU64    alignment NV_ALIGN_BYTES(8);
			NvU64    offset    NV_ALIGN_BYTES(8);
			NvU64    limit     NV_ALIGN_BYTES(8);
			NvU64    address   NV_ALIGN_BYTES(8);
			NvU64    rangeBegin NV_ALIGN_BYTES(8);
			NvU64    rangeEnd   NV_ALIGN_BYTES(8);
			NvU32    attr2;
			NvU32    ctagOffset;
			NvS32    numaNode;
		} AllocSize;
		struct {
			NvU32    owner;
			NvHandle hMemory;
			NvU32    type;
			NvU32    flags;
			NvU32    height;
			NvS32    pitch;
			NvU32    attr;
			NvU32    width;
			NvU32    format;
			NvU32    comprCovg;
			NvU32    zcullCovg;
			NvU32    partitionStride;
			NvU64    size      NV_ALIGN_BYTES(8);
			NvU64    alignment NV_ALIGN_BYTES(8);
			NvU64    offset    NV_ALIGN_BYTES(8);
			NvU64    limit     NV_ALIGN_BYTES(8);
			NvU64    address   NV_ALIGN_BYTES(8);
			NvU64    rangeBegin NV_ALIGN_BYTES(8);
			NvU64    rangeEnd   NV_ALIGN_BYTES(8);
			NvU32    attr2;
			NvU32    ctagOffset;
			NvS32    numaNode;
		} AllocTiledPitchHeight;
		struct {
			NvU32    owner;
			NvHandle hMemory;
			NvU32    flags;
		} Free;
		struct {
			NvU32 attr;
			NvU64 offset NV_ALIGN_BYTES(8);
			NvU64 size   NV_ALIGN_BYTES(8);
			NvU64 base   NV_ALIGN_BYTES(8);
		} Info;
	} data;
} NVOS32_PARAMETERS;

/* Back-compat alias used by older code paths */
typedef NVOS32_PARAMETERS NVOS32_PARAMETERS_ALLOC_SIZE;

/* Channel / GPFIFO allocation (alloc_channel.h) */
#define NV_MAX_SUBDEVICES 8
#define CC_CHAN_ALLOC_IV_SIZE_DWORD    3
#define CC_CHAN_ALLOC_NONCE_SIZE_DWORD 8

typedef struct {
	NvU64 base NV_ALIGN_BYTES(8);
	NvU64 size NV_ALIGN_BYTES(8);
	NvU32 addressSpace;
	NvU32 cacheAttrib;
} NV_MEMORY_DESC_PARAMS;

typedef struct {
	NvHandle hObjectError;
	NvHandle hObjectBuffer;
	NvU64    gpFifoOffset NV_ALIGN_BYTES(8);
	NvU32    gpFifoEntries;
	NvU32    flags;
	NvHandle hContextShare;
	NvHandle hVASpace;
	NvHandle hHandleVASpace;
	NvHandle hUserdMemory[NV_MAX_SUBDEVICES];
	NvU64    userdOffset[NV_MAX_SUBDEVICES] NV_ALIGN_BYTES(8);
	NvU32    engineType;
	NvU32    cid;
	NvU32    subDeviceId;
	NvHandle hObjectEccError;
	NV_MEMORY_DESC_PARAMS instanceMem;
	NV_MEMORY_DESC_PARAMS userdMem;
	NV_MEMORY_DESC_PARAMS ramfcMem;
	NV_MEMORY_DESC_PARAMS mthdbufMem;
	NvHandle hPhysChannelGroup;
	NvU32    internalFlags;
	NV_MEMORY_DESC_PARAMS errorNotifierMem;
	NV_MEMORY_DESC_PARAMS eccErrorNotifierMem;
	NvU32    ProcessID;
	NvU32    SubProcessID;
	NvU32    encryptIv[CC_CHAN_ALLOC_IV_SIZE_DWORD];
	NvU32    decryptIv[CC_CHAN_ALLOC_IV_SIZE_DWORD];
	NvU32    hmacNonce[CC_CHAN_ALLOC_NONCE_SIZE_DWORD];
	NvU32    tpcConfigID;
	NvU32    pad_end;
} NV_CHANNEL_ALLOC_PARAMS;

typedef NV_CHANNEL_ALLOC_PARAMS NV_CHANNELGPFIFO_ALLOCATION_PARAMETERS;

/* Engine types (ctrl2080gpu.h / open-gpu-kernel-modules subset) */
#define NV2080_ENGINE_TYPE_NULL         0x00000000
#define NV2080_ENGINE_TYPE_GRAPHICS     0x00000001
#define NV2080_ENGINE_TYPE_COPY0        0x0000000f
#define NV2080_ENGINE_TYPE_COPY1        0x00000010
#define NV2080_ENGINE_TYPE_COPY2        0x0000001a
/* Alternate numbering in some kernels (cl2080); mesa refine walks GET_ENGINES */
#define NV2080_ENGINE_TYPE_COPY3        0x0000000c
#define NV2080_ENGINE_TYPE_COPY4        0x0000000d
#define NV2080_ENGINE_TYPE_COPY5        0x0000000e
#define NV2080_ENGINE_TYPE_COPY6        0x00000011
#define NV2080_ENGINE_TYPE_COPY7        0x00000012
#define NV2080_ENGINE_TYPE_COPY8        0x00000015
#define NV2080_ENGINE_TYPE_COPY9        0x00000016
#define NV2080_ENGINE_TYPE_BSP          0x00000013
#define NV2080_ENGINE_TYPE_VP           0x00000014
#define NV2080_ENGINE_TYPE_SEC2         0x0000002c
#define NV2080_ENGINE_TYPE_NVDEC0       0x0000001b
#define NV2080_ENGINE_TYPE_NVDEC1       0x00000014  /* may alias VP on some gens */
#define NV2080_ENGINE_TYPE_NVDEC2       0x0000001e
#define NV2080_ENGINE_TYPE_NVDEC3       0x0000001f
#define NV2080_ENGINE_TYPE_NVDEC4       0x00000021
#define NV2080_ENGINE_TYPE_NVDEC5       0x00000022
#define NV2080_ENGINE_TYPE_NVDEC6       0x00000023
#define NV2080_ENGINE_TYPE_NVDEC7       0x00000024
#define NV2080_ENGINE_TYPE_NVENC0       0x0000001c
#define NV2080_ENGINE_TYPE_NVENC1       0x0000001d
#define NV2080_ENGINE_TYPE_NVENC2       0x00000025
#define NV2080_ENGINE_TYPE_NVJPEG0      0x0000002d
#define NV2080_ENGINE_TYPE_OFA          0x0000002e
#define NV2080_ENGINE_TYPE_SW           0x00000020
#define NV2080_ENGINE_TYPE_GR           NV2080_ENGINE_TYPE_GRAPHICS

/* Embedded engine/class list queries (ctrl2080gpu.h V2 / classlist) */
typedef struct {
	NvU32 engineCount;
	NvU32 engineList[NV2080_GPU_MAX_ENGINES_LIST_SIZE];
} NV2080_CTRL_GPU_GET_ENGINES_V2_PARAMS;

typedef struct {
	NvU32 engineType;
	NvU32 numClasses;
	NvU32 classList[NV2080_CTRL_GPU_MAX_CLASSLIST];
} NV2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS;

/* GPFIFO schedule control (Kepler+ channel class, works for later GPFIFO too) */
#define NVA06F_CTRL_CMD_GPFIFO_SCHEDULE  0xa06f0103
/* ctrla06fgpfifo.h — optional error notifier (TSG per-channel notify flag) */
#define NVA06F_CTRL_CMD_SET_ERROR_NOTIFIER 0xa06f0108
/* Pass8: glcore/vksc use SET_INTERLEAVE_LEVEL (rare; recovery/priority) */
#define NVA06F_CTRL_CMD_SET_INTERLEAVE_LEVEL 0xa06f0109
#define NVA06F_CTRL_CMD_GET_INTERLEAVE_LEVEL 0xa06f0110
#define NVA06F_CTRL_CMD_RESTART_RUNLIST      0xa06f0111
#define NVA06F_CTRL_CMD_STOP_CHANNEL         0xa06f0112
#define NVA06F_CTRL_CMD_GET_CONTEXT_ID       0xa06f0113
typedef struct {
	NvBool bNotifyEachChannelInTSG;
} NVA06F_CTRL_SET_ERROR_NOTIFIER_PARAMS;
/* Interleave level params (ctrla06fgpfifo.h) */
typedef struct {
	NvU32 tsgInterleaveLevel;
} NVA06F_CTRL_INTERLEAVE_LEVEL_PARAMS;
typedef struct {
	NvBool bBypassWaitForEngIdle;
} NVA06F_CTRL_RESTART_RUNLIST_PARAMS;
typedef struct {
	NvBool bInPreemptTimeout;
} NVA06F_CTRL_STOP_CHANNEL_PARAMS;
typedef struct {
	NvU32 contextId;
} NVA06F_CTRL_GET_CONTEXT_ID_PARAMS;
#define NVC36F_CTRL_CMD_GPFIFO_GET_WORK_SUBMIT_TOKEN 0xc36f0108
/* Optional: set error-notifier slot for work_submit_token (ctrlc36f.h) */
#define NVC36F_CTRL_CMD_GPFIFO_SET_WORK_SUBMIT_TOKEN_NOTIF_INDEX 0xc36f010a
/* Default notifier index for doorbell token (channel GPFIFO notification types) */
#define NV_CHANNELGPFIFO_NOTIFICATION_TYPE_WORK_SUBMIT_TOKEN 0x1

/*
 * Pass9 vdpau@3461b / glcore@a52bbc: RmControl paramsSize=3 for SCHEDULE
 * (bEnable + bSkipSubmit + bSkipEnable). Older code omitted bSkipEnable.
 */
typedef struct {
	NvBool bEnable;
	NvBool bSkipSubmit;
	NvBool bSkipEnable;
} NVA06F_CTRL_GPFIFO_SCHEDULE_PARAMS;

typedef struct {
	NvU32 workSubmitToken;
} NVC36F_CTRL_CMD_GPFIFO_GET_WORK_SUBMIT_TOKEN_PARAMS;

typedef struct {
	NvU32 index;
} NVC36F_CTRL_GPFIFO_SET_WORK_SUBMIT_TOKEN_NOTIF_INDEX_PARAMS;

/*
 * NVC36F_CTRL_CMD_GPFIFO_UPDATE_FAULT_METHOD_BUFFER (ctrlc36f.h 0xc36f0109)
 * SR-IOV/vGPU guest virtual-channel fault method buffer (bar2Addr[runqueue]).
 * Pass6 RE: 0 hits in normal glcore/eglcore/cuda — optional/non-fatal for host bring-up.
 */
#define NVC36F_CTRL_CMD_GPFIFO_UPDATE_FAULT_METHOD_BUFFER 0xc36f0109
#define NVC36F_CTRL_CMD_GPFIFO_FAULT_METHOD_BUFFER_MAX_RUNQUEUES 0x2

typedef struct {
	NvU64 bar2Addr[NVC36F_CTRL_CMD_GPFIFO_FAULT_METHOD_BUFFER_MAX_RUNQUEUES] NV_ALIGN_BYTES(8);
} NVC36F_CTRL_GPFIFO_UPDATE_FAULT_METHOD_BUFFER_PARAMS;

/* USERD / channel control block layout (Nv906fControl / Nvc36fControl compatible subset) */
typedef volatile struct {
	NvU32 Ignored00[0x10];  /* 0x00 - 0x3f */
	NvU32 Put;              /* 0x40 */
	NvU32 Get;              /* 0x44 */
	NvU32 Reference;        /* 0x48 */
	NvU32 PutHi;            /* 0x4c */
	NvU32 Ignored01[0x02];  /* 0x50 - 0x57 */
	NvU32 TopLevelGet;      /* 0x58 */
	NvU32 TopLevelGetHi;    /* 0x5c */
	NvU32 GetHi;            /* 0x60 */
	NvU32 Ignored02[0x07];  /* 0x64 - 0x7f */
	NvU32 Ignored03;        /* 0x80 */
	NvU32 Ignored04[0x01];  /* 0x84 */
	NvU32 GPGet;            /* 0x88 */
	NvU32 GPPut;            /* 0x8c */
} nvidia_userd_control_t;

/* GPFIFO entry format (NV506F/NVC36F - 8 bytes; see clc36f.h NVC36F_GP_ENTRY*)
 *
 * entry[0]: FETCH[0], GET[31:2] = (pb_gpu_va >> 2) in bits 31:2 (4-byte aligned VA)
 * entry[1]: GET_HI[7:0] = pb_gpu_va[39:32], PRIV[8], LEVEL[9], LENGTH[30:10] (21 bits,
 *           length in dwords), SYNC[31]
 *
 * 610.43.02 glcore RE (a317c2): length intermediate masked with 0x1fffff then ORed into
 * upper flag bits — matches LENGTH field width (bits 30:10), not "low 21 bits of entry1".
 *
 * Kick (glcore ac5540): write ring entry → USERD.GPPut@+0x8c (all USERDs) → if
 * gpfifo_class > 0xC36E: sfence → usermode+0x90 = work_submit_token.
 */
#define NV_GP_ENTRY_SIZE                8
#define NV_GP_ENTRY0_GET_SHIFT          2
#define NV_GP_ENTRY1_GET_HI_MASK        0xff
#define NV_GP_ENTRY1_PRIV_SHIFT         8
#define NV_GP_ENTRY1_LEVEL_SHIFT        9
#define NV_GP_ENTRY1_LENGTH_SHIFT       10
#define NV_GP_ENTRY1_LENGTH_MASK        0x1fffff  /* 21 bits at [30:10] */
#define NV_GP_ENTRY1_SYNC_SHIFT         31  /* SYNC_WAIT: wait for prior PB segment */
/* GPFIFO classes > this threshold use usermode doorbell (glcore ac5557) */
#define NV_GP_DOORBELL_MIN_CLASS        0xc36f  /* first class strictly > 0xC36E */
/* glcore@ac5526: multi-USERD / multi-doorbell loop upper bound (MIG / multi-subdevice) */
#define NV_GP_MAX_USERD_SLOTS           9
/* nvidia_gp_entry_pack flags (OR together) */
#define NV_GP_ENTRY_F_PRIV              (1u << 0)  /* PRIV_KERNEL */
#define NV_GP_ENTRY_F_LEVEL_SUBR        (1u << 1)  /* LEVEL_SUBROUTINE */
#define NV_GP_ENTRY_F_SYNC_WAIT         (1u << 2)  /* SYNC_WAIT before fetch */

/* Class IDs for channel/memory/context */
#define NV01_ROOT_NON_PRIV              0x00000001
#define NV01_EVENT_OS_EVENT             0x00000079
#define NV01_MEMORY_DEVICELESS          0x0000003f
#define NV01_MEMORY_FRAMEBUFFER_CONSOLE 0x0000003c
#define NV01_MEMORY_LIST_SYSTEM         0x00000082
#define NV01_MEMORY_LIST_FBMEM          0x00000083
#define NV01_MEMORY_LIST_OBJECT         0x00000081
#define NV50_MEMORY_VIRTUAL             0x000050a0
#define FERMI_CONTEXT_SHARE_A           0x00009067
#define KEPLER_CHANNEL_GROUP_A          0x0000a06c
#define GF100_CHANNEL_GPFIFO            0x0000906f
#define KEPLER_CHANNEL_GPFIFO_A         0x0000a06f
#define KEPLER_CHANNEL_GPFIFO_B         0x0000a16f
#define KEPLER_CHANNEL_GPFIFO_C         0x0000a26f
#define MAXWELL_CHANNEL_GPFIFO_A        0x0000b06f
#define PASCAL_CHANNEL_GPFIFO_A         0x0000c06f
#define VOLTA_CHANNEL_GPFIFO_A          0x0000c36f
#define TURING_CHANNEL_GPFIFO_A         0x0000c46f
#define AMPERE_CHANNEL_GPFIFO_A         0x0000c56f
#define HOPPER_CHANNEL_GPFIFO_A         0x0000c76f
#define BLACKWELL_CHANNEL_GPFIFO_A      0x0000c86f

/* Context DMA / error notifier */
#define NV01_CONTEXT_DMA_FROM_MEMORY    0x00000002
#define NV01_CONTEXT_ERROR_TO_MEMORY    0x00000003
#define NV_EVENT_BUFFER_CHANNEL         0x0000907e
/* FERMI_CONTEXT_SHARE_A / KEPLER_CHANNEL_GROUP_A defined with class IDs below */

/* NV_CONTEXT_DMA_ALLOCATION_PARAMS (nvos.h) - RmAlloc NV01_CONTEXT_DMA_* */
typedef struct {
	NvHandle hSubDevice;
	NvV32    flags;
	NvHandle hMemory;
	NvU64    offset NV_ALIGN_BYTES(8);
	NvU64    limit NV_ALIGN_BYTES(8);
} NV_CONTEXT_DMA_ALLOCATION_PARAMS;

/* tick94: NVOS49 bind context DMA to channel (NV_ESC_RM_BIND_CONTEXT_DMA) */
typedef struct {
	NvHandle hClient;
	NvHandle hChannel;
	NvHandle hCtxDma;
	NvV32    status;
} NVOS49_PARAMETERS;

/* tick94: NVOS30 idle channels (NV_ESC_RM_IDLE_CHANNELS) — single-channel helper */
typedef struct {
	NvHandle hClient;
	NvHandle hDevice;
	NvHandle hChannel;
	NvV32    numChannels;
	NvU64    phClients NV_ALIGN_BYTES(8);
	NvU64    phDevices NV_ALIGN_BYTES(8);
	NvU64    phChannels NV_ALIGN_BYTES(8);
	NvV32    flags;
	NvV32    timeout;
	NvV32    status;
} NVOS30_PARAMETERS;

#define NVOS30_FLAGS_BEHAVIOR_SPIN           0x00000000u
#define NVOS30_FLAGS_BEHAVIOR_SLEEP          0x00000001u
#define NVOS30_FLAGS_BEHAVIOR_QUERY          0x00000002u
#define NVOS30_FLAGS_CHANNEL_SINGLE          0x00000010u  /* bit 4 set in CHANNEL field */
#define NVOS30_FLAGS_IDLE_PUSH_BUFFER        0x00000100u  /* shifted IDLE field; use composed flags */
/* Practical composed flag: spin + single channel + push buffer idle */
#define NVOS30_FLAGS_HELPER_SPIN_SINGLE_PB   0x00000110u

/* tick94: NV01_MEMORY_VIRTUAL (see line ~90) / NV50_MEMORY_VIRTUAL alloc params */
#define NV_MEMORY_VIRTUAL_SYSMEM_DYNAMIC_HVASPACE 0xffffffffu

typedef struct {
	NvU64    offset NV_ALIGN_BYTES(8);
	NvU64    limit NV_ALIGN_BYTES(8);
	NvHandle hVASpace;
} NV_MEMORY_VIRTUAL_ALLOCATION_PARAMS;

/* NV_CHANNEL_GROUP_ALLOCATION_PARAMETERS (Kepler+ TSG) */
typedef struct {
	NvHandle hObjectError;
	NvHandle hObjectEccError;
	NvHandle hVASpace;
	NvU32    engineType;
	NvBool   bIsCallingContextVgpuPlugin;
} NV_CHANNEL_GROUP_ALLOCATION_PARAMETERS;

/* FERMI_CONTEXT_SHARE_A alloc params */
typedef struct {
	NvHandle hVASpace;
	NvU32    flags;
	NvU32    subctxId;
} NV_CTXSHARE_ALLOCATION_PARAMETERS;

#define NV_CTXSHARE_ALLOCATION_FLAGS_SUBCONTEXT_SYNC   0x00000000
#define NV_CTXSHARE_ALLOCATION_FLAGS_SUBCONTEXT_ASYNC  0x00000001

/* Channel group (KEPLER_CHANNEL_GROUP_A / A06C) — pass5 glcore uses BIND then SCHEDULE */
#define NVA06C_CTRL_CMD_GPFIFO_SCHEDULE  0xa06c0101
#define NVA06C_CTRL_CMD_BIND             0xa06c0102
#define NVA06C_CTRL_CMD_SET_TIMESLICE    0xa06c0103  /* pass5 imm; not GET_INTERLEAVE */
#define NVA06C_CTRL_CMD_GET_TIMESLICE    0xa06c0104
/* tick91: remaining A06C GPFIFO/TSG controls (ctrla06c.h; cuda/gl recovery paths) */
#define NVA06C_CTRL_CMD_PREEMPT              0xa06c0105
#define NVA06C_CTRL_CMD_GET_INFO             0xa06c0106
#define NVA06C_CTRL_CMD_SET_INTERLEAVE_LEVEL 0xa06c0107
#define NVA06C_CTRL_CMD_GET_INTERLEAVE_LEVEL 0xa06c0108
#define NVA06C_CTRL_CMD_MAKE_REALTIME        0xa06c0110
#define NVA06C_CTRL_CMD_PREEMPT_MAX_MANUAL_TIMEOUT_US 1000000u
#define NVA06C_CTRL_INTERLEAVE_LEVEL_LOW     0x00000000u
#define NVA06C_CTRL_INTERLEAVE_LEVEL_MEDIUM  0x00000001u
#define NVA06C_CTRL_INTERLEAVE_LEVEL_HIGH    0x00000002u

typedef struct {
	NvBool bEnable;
	NvBool bSkipSubmit;
	NvBool bSkipEnable; /* pass9: mirror A06F schedule layout when present */
} NVA06C_CTRL_GPFIFO_SCHEDULE_PARAMS;

typedef struct {
	NvU32 engineType;
} NVA06C_CTRL_BIND_PARAMS;

typedef struct {
	NvU64 timesliceUs NV_ALIGN_BYTES(8);
} NVA06C_CTRL_TIMESLICE_PARAMS;

typedef NVA06C_CTRL_TIMESLICE_PARAMS NVA06C_CTRL_SET_TIMESLICE_PARAMS;
typedef NVA06C_CTRL_TIMESLICE_PARAMS NVA06C_CTRL_GET_TIMESLICE_PARAMS;

typedef struct {
	NvBool bWait;
	NvBool bManualTimeout;
	NvU32  timeoutUs;
} NVA06C_CTRL_PREEMPT_PARAMS;

typedef struct {
	NvU32 tsgID;
} NVA06C_CTRL_GET_INFO_PARAMS;

typedef struct {
	NvU32 tsgInterleaveLevel;
} NVA06C_CTRL_INTERLEAVE_LEVEL_PARAMS;

typedef NVA06C_CTRL_INTERLEAVE_LEVEL_PARAMS NVA06C_CTRL_SET_INTERLEAVE_LEVEL_PARAMS;
typedef NVA06C_CTRL_INTERLEAVE_LEVEL_PARAMS NVA06C_CTRL_GET_INTERLEAVE_LEVEL_PARAMS;

typedef struct {
	NvBool bRealtime;
} NVA06C_CTRL_MAKE_REALTIME_PARAMS;

/* Per-channel A06F BIND (OGKM ctrla06fgpfifo.h; engine bind before schedule) */
#define NVA06F_CTRL_CMD_BIND             0xa06f0104

typedef struct {
	NvU32 engineType;
} NVA06F_CTRL_BIND_PARAMS;

/* VASpace / virtual memory / usermode doorbell (class headers + nvos.h) */
#define FERMI_VASPACE_A                 0x000090f1
#define VOLTA_USERMODE_A                0x0000c361
#define HOPPER_USERMODE_A               0x0000c661
/* 610.43.02 binaries also reference 0xC761 (Blackwell-era usermode ladder) */
#define BLACKWELL_USERMODE_A            0x0000c761
#define NVC361_NV_USERMODE__SIZE        65536
#define NVC361_NOTIFY_CHANNEL_PENDING   0x00000090

#define NV_VASPACE_ALLOCATION_INDEX_GPU_NEW     0x00
#define NV_VASPACE_ALLOCATION_INDEX_GPU_HOST    0x01
#define NV_VASPACE_ALLOCATION_INDEX_GPU_GLOBAL  0x02
#define NV_VASPACE_ALLOCATION_INDEX_GPU_DEVICE  0x03
#define NV_VASPACE_ALLOCATION_INDEX_GPU_FLA     0x04

/* nvos.h NV_VASPACE_ALLOCATION_FLAGS_* (tick97: full set for HW bring-up) */
#define NV_VASPACE_ALLOCATION_FLAGS_NONE                         0x00000000u
#define NV_VASPACE_ALLOCATION_FLAGS_MINIMIZE_PTETABLE_SIZE       (1u << 0)
#define NV_VASPACE_ALLOCATION_FLAGS_RETRY_PTE_ALLOC_IN_SYS       (1u << 1)
#define NV_VASPACE_ALLOCATION_FLAGS_SHARED_MANAGEMENT            (1u << 2)
#define NV_VASPACE_ALLOCATION_FLAGS_IS_EXTERNALLY_OWNED          (1u << 3)
#define NV_VASPACE_ALLOCATION_FLAGS_ENABLE_NVLINK_ATS            (1u << 4)
#define NV_VASPACE_ALLOCATION_FLAGS_ENABLE_PAGE_FAULTING         (1u << 6)
#define NV_VASPACE_ALLOCATION_FLAGS_VA_INTERNAL_LIMIT            (1u << 7)
#define NV_VASPACE_ALLOCATION_FLAGS_ALLOW_ZERO_ADDRESS           (1u << 8)
#define NV_VASPACE_ALLOCATION_FLAGS_IS_FLA                       (1u << 9)
#define NV_VASPACE_ALLOCATION_FLAGS_SKIP_SCRUB_MEMPOOL           (1u << 10)
#define NV_VASPACE_ALLOCATION_FLAGS_OPTIMIZE_PTETABLE_MEMPOOL_USAGE (1u << 11)
#define NV_VASPACE_ALLOCATION_FLAGS_REQUIRE_FIXED_OFFSET         (1u << 12)
#define NV_VASPACE_ALLOCATION_FLAGS_PTETABLE_HEAP_MANAGED        (1u << 13)

/* Common FERMI_VASPACE_A bigPageSize values (0 = RM default) */
#define NV_VASPACE_BIG_PAGE_SIZE_DEFAULT   0u
#define NV_VASPACE_BIG_PAGE_SIZE_64K       (64u * 1024u)
#define NV_VASPACE_BIG_PAGE_SIZE_128K      (128u * 1024u)
#define NV_VASPACE_BIG_PAGE_SIZE_2M        (2u * 1024u * 1024u)
#define NV_VASPACE_BIG_PAGE_SIZE_512M      (512u * 1024u * 1024u)

typedef struct {
	NvU32   index;
	NvV32   flags;
	NvU64   vaSize NV_ALIGN_BYTES(8);
	NvU64   vaStartInternal NV_ALIGN_BYTES(8);
	NvU64   vaLimitInternal NV_ALIGN_BYTES(8);
	NvU32   bigPageSize;
	NvU64   vaBase NV_ALIGN_BYTES(8);
	NvU32   pasid;
} NV_VASPACE_ALLOCATION_PARAMETERS;

/* tick97: NVOS57 / NV_ESC_RM_SHARE — RS_SHARE_POLICY (rs_access.h + nvos.h) */
#define RS_ACCESS_DUP_OBJECT  0u
#define RS_ACCESS_NICE        1u
#define RS_ACCESS_DEBUG       2u
#define RS_ACCESS_PERFMON     3u
#define RS_ACCESS_COUNT       4u
#define SDK_RS_ACCESS_MAX_LIMBS 1

typedef struct {
	NvU32 limbs[SDK_RS_ACCESS_MAX_LIMBS];
} RS_ACCESS_MASK;

#define RS_SHARE_TYPE_NONE              0u
#define RS_SHARE_TYPE_ALL               1u
#define RS_SHARE_TYPE_OS_SECURITY_TOKEN 2u
#define RS_SHARE_TYPE_CLIENT            3u
#define RS_SHARE_TYPE_PID               4u
#define RS_SHARE_TYPE_SMC_PARTITION     5u
#define RS_SHARE_TYPE_GPU               6u
#define RS_SHARE_TYPE_FM_CLIENT         7u
#define RS_SHARE_TYPE_MAX               8u

#define RS_SHARE_ACTION_FLAG_REVOKE     (1u << 0)
#define RS_SHARE_ACTION_FLAG_REQUIRE    (1u << 1)
#define RS_SHARE_ACTION_FLAG_COMPOSE    (1u << 2)

typedef struct {
	NvU32          target;
	RS_ACCESS_MASK accessMask;
	NvU16          type;   /* RS_SHARE_TYPE_* */
	NvU8           action; /* RS_SHARE_ACTION_FLAG_* */
} RS_SHARE_POLICY;

typedef struct {
	NvHandle        hClient;
	NvHandle        hObject;
	RS_SHARE_POLICY sharePolicy;
	NvU32           status;
} NVOS57_PARAMETERS;

/*
 * NVOS46: map memory into a DMA / VASpace (NV_ESC_RM_MAP_MEMORY_DMA)
 * tick102: full flag fields from open-gpu-kernel-modules nvos.h
 * (unshifted field values; use NV_OS32_DRF_SHL / NVOS46_MAKE_FLAGS helpers).
 */
#define NVOS46_FLAGS_ACCESS_READ_WRITE             0x00000000u
#define NVOS46_FLAGS_ACCESS_READ_ONLY              0x00000001u
#define NVOS46_FLAGS_ACCESS_WRITE_ONLY             0x00000002u

#define NVOS46_FLAGS_32BIT_POINTER_DISABLE         0x00000000u
#define NVOS46_FLAGS_32BIT_POINTER_ENABLE          0x00000001u

#define NVOS46_FLAGS_PAGE_KIND_PHYSICAL            0x00000000u
#define NVOS46_FLAGS_PAGE_KIND_VIRTUAL             0x00000001u

#define NVOS46_FLAGS_CACHE_SNOOP_DISABLE           0x00000000u
#define NVOS46_FLAGS_CACHE_SNOOP_ENABLE            0x00000001u

#define NVOS46_FLAGS_KERNEL_MAPPING_NONE           0x00000000u
#define NVOS46_FLAGS_KERNEL_MAPPING_ENABLE         0x00000001u

#define NVOS46_FLAGS_SHADER_ACCESS_DEFAULT         0x00000000u
#define NVOS46_FLAGS_SHADER_ACCESS_READ_ONLY       0x00000001u
#define NVOS46_FLAGS_SHADER_ACCESS_WRITE_ONLY      0x00000002u
#define NVOS46_FLAGS_SHADER_ACCESS_READ_WRITE      0x00000003u

/* PAGE_SIZE field values (shifted into bits 11:8) */
#define NVOS46_FLAGS_PAGE_SIZE_DEFAULT             0x00000000u
#define NVOS46_FLAGS_PAGE_SIZE_4KB                 0x00000001u
#define NVOS46_FLAGS_PAGE_SIZE_BIG                 0x00000002u
#define NVOS46_FLAGS_PAGE_SIZE_BOTH                0x00000003u
#define NVOS46_FLAGS_PAGE_SIZE_HUGE                0x00000004u
#define NVOS46_FLAGS_PAGE_SIZE_512M                0x00000005u

#define NVOS46_FLAGS_SYSTEM_L3_ALLOC_DEFAULT       0x00000000u
#define NVOS46_FLAGS_SYSTEM_L3_ALLOC_ENABLE_HINT   0x00000001u

#define NVOS46_FLAGS_DMA_OFFSET_GROWS_UP           0x00000000u
#define NVOS46_FLAGS_DMA_OFFSET_GROWS_DOWN         0x00000001u

#define NVOS46_FLAGS_DMA_OFFSET_FIXED_FALSE        0x00000000u
#define NVOS46_FLAGS_DMA_OFFSET_FIXED_TRUE         0x00000001u

/* Pre-shifted convenience (legacy code used these directly in flags args) */
#define NVOS46_FLAGS_PAGE_SIZE_4KB_SHL             NV_OS32_DRF_SHL(8, 11, NVOS46_FLAGS_PAGE_SIZE_4KB)
#define NVOS46_FLAGS_PAGE_SIZE_BIG_SHL             NV_OS32_DRF_SHL(8, 11, NVOS46_FLAGS_PAGE_SIZE_BIG)
#define NVOS46_FLAGS_PAGE_SIZE_HUGE_SHL            NV_OS32_DRF_SHL(8, 11, NVOS46_FLAGS_PAGE_SIZE_HUGE)
#define NVOS46_FLAGS_PAGE_SIZE_512M_SHL            NV_OS32_DRF_SHL(8, 11, NVOS46_FLAGS_PAGE_SIZE_512M)
/* Pre-shifted FIXED bit (bit 15); unshifted field value remains _TRUE/_FALSE above */
#define NVOS46_FLAGS_DMA_OFFSET_FIXED              NV_OS32_DRF_SHL(15, 15, 1u)
#define NVOS46_FLAGS_32BIT_POINTER                 NV_OS32_DRF_SHL(2, 2, NVOS46_FLAGS_32BIT_POINTER_ENABLE)
#define NVOS46_FLAGS_CACHE_SNOOP                   NV_OS32_DRF_SHL(4, 4, NVOS46_FLAGS_CACHE_SNOOP_ENABLE)

/* Compose NVOS46 flags: access + page_size selector + optional fixed offset bit */
#define NVOS46_MAKE_FLAGS(access, page_size_sel, fixed_va) \
	(((NvU32)(access) & 3u) | \
	 NV_OS32_DRF_SHL(8, 11, (page_size_sel)) | \
	 ((fixed_va) ? NVOS46_FLAGS_DMA_OFFSET_FIXED : 0u))

typedef struct {
	NvHandle hClient;
	NvHandle hDevice;
	NvHandle hDma;
	NvHandle hMemory;
	NvU64    offset NV_ALIGN_BYTES(8);
	NvU64    length NV_ALIGN_BYTES(8);
	NvV32    flags;
	NvV32    flags2;
	NvV32    kindOverride;
	NvU64    dmaOffset NV_ALIGN_BYTES(8);
	NvV32    status;
} NVOS46_PARAMETERS;

typedef NVOS46_PARAMETERS NV_MAP_MEMORY_DMA_PARAMETERS;

/* NVOS47: unmap memory from DMA / VASpace */
#define NVOS47_FLAGS_DEFER_TLB_INVALIDATION_FALSE  0x00000000
#define NVOS47_FLAGS_DEFER_TLB_INVALIDATION_TRUE   0x00000001

typedef struct {
	NvHandle hClient;
	NvHandle hDevice;
	NvHandle hDma;
	NvHandle hMemory;
	NvV32    flags;
	NvU64    dmaOffset NV_ALIGN_BYTES(8);
	NvU64    size NV_ALIGN_BYTES(8);
	NvV32    status;
} NVOS47_PARAMETERS;

typedef NVOS47_PARAMETERS NV_UNMAP_MEMORY_DMA_PARAMETERS;

/* Hopper usermode alloc params (optional bBar1Mapping etc.) - pass zeroed for defaults */
typedef struct {
	NvU32 flags;
	NvU32 bar1Mapping;
} NV_HOPPER_USERMODE_A_PARAMS;

/* NvNotification layout (error notifier memory, sdk/nvidia/inc/nvtypes.h subset) */
typedef volatile struct {
	NvU32 timeStamp_0;   /* nanoseconds low */
	NvU32 timeStamp_1;   /* nanoseconds high */
	NvU32 info32;        /* method / status info */
	NvU16 info16;        /* additional status */
	NvU16 status;        /* NV_OK when idle/cleared; non-zero = error pending */
} nvidia_notification_t;

#define NVIDIA_NOTIFICATION_STATUS_DONE_SUCCESS  0x0000
#define NVIDIA_NOTIFICATION_STATUS_IN_PROGRESS   0xffff

/* NV0000_CTRL_GPU_GET_ATTACHED_IDS params */
#define NV0000_CTRL_GPU_MAX_ATTACHED_GPUS 32
typedef struct {
	NvU32 gpuIds[NV0000_CTRL_GPU_MAX_ATTACHED_GPUS];
} NV0000_CTRL_GPU_GET_ATTACHED_IDS_PARAMS;

/* NV0000_CTRL_GPU_GET_ID_INFO params (simplified) */
typedef struct {
	NvU32  gpuId;
	NvU32  gpuFlags;
	NvU32  deviceInstance;
	NvU32  subDeviceInstance;
	NvU32  szName;
	NvU64  gpuUuid NV_ALIGN_BYTES(8);
	NvU32  sliStatus;
	NvU32  boardId;
	NvU32  gpuInstance;
	NvU32  numaId;
} NV0000_CTRL_GPU_GET_ID_INFO_PARAMS;

/* NV2080_CTRL_GPU_GET_NAME_STRING */
#define NV2080_GPU_MAX_NAME_STRING_LENGTH 64
typedef struct {
	NvU32 gpuNameStringFlags;
	NvU8  gpuNameString[NV2080_GPU_MAX_NAME_STRING_LENGTH];
} NV2080_CTRL_GPU_GET_NAME_STRING_PARAMS;

/* NV2080_CTRL_MC_GET_ARCH_INFO */
typedef struct {
	NvU32 architecture;
	NvU32 implementation;
	NvU32 revision;
	NvU32 subRevision;
} NV2080_CTRL_MC_GET_ARCH_INFO_PARAMS;

/* NV2080_CTRL_FB_GET_INFO_V2 (simplified - index-based list) */
/* tick101: FB_GET_INFO indices from ctrl2080fb.h (subset used in probe) */
#define NV2080_CTRL_FB_INFO_INDEX_HEAP_SIZE              0
#define NV2080_CTRL_FB_INFO_INDEX_HEAP_FREE              1
#define NV2080_CTRL_FB_INFO_INDEX_HEAP_START             2
#define NV2080_CTRL_FB_INFO_INDEX_RAM_SIZE               4
#define NV2080_CTRL_FB_INFO_INDEX_BAR1_SIZE              5
#define NV2080_CTRL_FB_INFO_INDEX_USABLE_RAM_SIZE        23
#define NV2080_CTRL_FB_INFO_INDEX_BAR1_AVAIL_SIZE        29  /* 0x1D */
#define NV2080_CTRL_FB_INFO_INDEX_HEAP_START_ALT         30  /* 0x1E some RMs */
#define NV2080_CTRL_FB_INFO_INDEX_BAR1_MAX_CONTIG_AVAIL  31  /* 0x1F */
#define NV2080_CTRL_FB_INFO_FBPA_ECC_ENABLED             45  /* 0x2D */
#define NV2080_CTRL_FB_INFO_INDEX_ECC_STATUS_SIZE        53  /* 0x35 */
#define NV2080_CTRL_FB_INFO_MAX_LIST_SIZE                64

typedef struct {
	NvU32 index;
	NvU32 data;
} NV2080_CTRL_FB_INFO;

typedef struct {
	NvU32 fbInfoListSize;
	NvU32 pad;
	NV2080_CTRL_FB_INFO fbInfoList[NV2080_CTRL_FB_INFO_MAX_LIST_SIZE];
} NV2080_CTRL_FB_GET_INFO_V2_PARAMS;

/* NV2080_CTRL_GR_GET_INFO_V2 */
/* tick104: align GR_INFO indices with open-gpu-kernel-modules ctrl0080gr.h */
#define NV2080_CTRL_GR_INFO_INDEX_SHADER_PIPE_COUNT      0x00000007
#define NV2080_CTRL_GR_INFO_INDEX_THREAD_STACK_SCALING_FACTOR 0x00000008
#define NV2080_CTRL_GR_INFO_INDEX_SHADER_PIPE_SUB_COUNT  0x0000000B
#define NV2080_CTRL_GR_INFO_INDEX_SM_VERSION             0x0000000C
#define NV2080_CTRL_GR_INFO_INDEX_MAX_WARPS_PER_SM       0x0000000D
#define NV2080_CTRL_GR_INFO_INDEX_MAX_THREADS_PER_WARP   0x0000000E
#define NV2080_CTRL_GR_INFO_INDEX_MAX_SP_PER_SM          0x00000013
#define NV2080_CTRL_GR_INFO_INDEX_GPU_CORE_COUNT         0x0000001D
#define NV2080_CTRL_GR_INFO_MAX_SIZE                     64

typedef struct {
	NvU32 index;
	NvU32 data;
} NV2080_CTRL_GR_INFO;

typedef struct {
	NvU32 grInfoListSize;
	NvU32 pad;
	NV2080_CTRL_GR_INFO grInfoList[NV2080_CTRL_GR_INFO_MAX_SIZE];
} NV2080_CTRL_GR_GET_INFO_V2_PARAMS;

/* Device/subdevice alloc params (NV0080_ALLOC_PARAMETERS / NV2080) */
typedef struct {
	NvU32 deviceId;
	NvU32 hClientShare;
	NvU32 hTargetClient;
	NvU32 hTargetDevice;
	NvU32 flags;
	NvU64 vaSpaceSize NV_ALIGN_BYTES(8);
	NvU64 vaStartInternal NV_ALIGN_BYTES(8);
	NvU64 vaLimitInternal NV_ALIGN_BYTES(8);
	NvU32 vaMode;
} NV0080_ALLOC_PARAMETERS;

typedef struct {
	NvU32 subDeviceId;
} NV2080_ALLOC_PARAMETERS;

#ifdef __cplusplus
}
#endif


typedef struct nv_ioctl_wait_open_complete {
	NvS32 rc;
	NvU32 adapterStatus;
} nv_ioctl_wait_open_complete_t;

typedef struct nv_ioctl_attach_gpus_to_fd {
	NvU32 gpuIds[NV_MAX_DEVICES];
	NvU32 gpuCount;
} nv_ioctl_attach_gpus_to_fd_t;

#endif /* _NVIDIA_RM_H_ */
