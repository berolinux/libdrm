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
#define NV0000_CTRL_CMD_SYSTEM_GET_CPU_INFO         0x108
#define NV0000_CTRL_CMD_CLIENT_GET_ADDR_SPACE_TYPE  0x0d04
#define NV0000_CTRL_GPU_INVALID_ID                  0xffffffff

#define NV0080_CTRL_CMD_GPU_GET_CLASSLIST           0x800201
#define NV0080_CTRL_CMD_GPU_GET_NUM_SUBDEVICES      0x800280
#define NV0080_CTRL_CMD_GPU_GET_VIRTUALIZATION_MODE 0x800289
#define NV0080_CTRL_CMD_FIFO_GET_ENGINE_CONTEXT_PROPERTIES 0x801701

#define NV2080_CTRL_CMD_GPU_GET_NAME_STRING         0x20800110
#define NV2080_CTRL_CMD_GPU_GET_SHORT_NAME_STRING   0x20800111
#define NV2080_CTRL_CMD_GPU_GET_SIMULATION_INFO     0x20800119
#define NV2080_CTRL_CMD_GPU_GET_GID_INFO            0x2080012a
#define NV2080_CTRL_CMD_GPU_GET_ENGINES             0x20800123
#define NV2080_CTRL_CMD_MC_GET_ARCH_INFO            0x20801701
#define NV2080_CTRL_CMD_FB_GET_INFO                 0x20801301
#define NV2080_CTRL_CMD_FB_GET_INFO_V2              0x20801303
#define NV2080_CTRL_CMD_GR_GET_INFO                 0x20801201
#define NV2080_CTRL_CMD_GR_GET_INFO_V2              0x20801210
#define NV2080_CTRL_CMD_BUS_GET_INFO                0x20801801
#define NV2080_CTRL_CMD_BUS_GET_INFO_V2             0x20801803
#define NV2080_CTRL_CMD_TIMER_GET_TIME              0x20800403
#define NV2080_CTRL_CMD_EVENT_SET_NOTIFICATION      0x20800301

/* NVOS32 vidheap functions */
#define NVOS32_FUNCTION_ALLOC_SIZE                  2
#define NVOS32_FUNCTION_FREE                        5
#define NVOS32_FUNCTION_INFO                        6
#define NVOS32_FUNCTION_ALLOC_TILED_PITCH_HEIGHT    8
#define NVOS32_FUNCTION_ALLOC_OS_DESCRIPTOR         12

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

/* NVOS55: dup object */
typedef struct {
	NvHandle hClient;
	NvHandle hParent;
	NvHandle hObject;
	NvHandle hClientSrc;
	NvHandle hObjectSrc;
	NvU32    flags;
	NvU32    status;
} NVOS55_PARAMETERS;

/* NVOS32: vid heap control - outer header + data union from nvos.h */
#define NVOS32_FREE_FLAGS_MEMORY_HANDLE_PROVIDED    0x00000001
#define NVOS32_FUNCTION_ALLOC_SIZE_RANGE            14

/* NVOS32_ATTR bitfields (subset used by userspace alloc) */
#define NVOS32_ATTR_LOCATION                        1:0
#define NVOS32_ATTR_LOCATION_VIDMEM                 0x00000000
#define NVOS32_ATTR_LOCATION_PCI                    0x00000001
#define NVOS32_ATTR_LOCATION_ANY                    0x00000003
#define NVOS32_ATTR_PAGE_SIZE                       5:3
#define NVOS32_ATTR_PAGE_SIZE_DEFAULT               0x00000000
#define NVOS32_ATTR_PAGE_SIZE_4KB                   0x00000001
#define NVOS32_ATTR_PAGE_SIZE_BIG                   0x00000002
#define NVOS32_ATTR_PAGE_SIZE_HUGE                  0x00000003
#define NVOS32_ATTR_COHERENCY                       9:7
#define NVOS32_ATTR_COHERENCY_UNCACHED              0x00000000
#define NVOS32_ATTR_COHERENCY_CACHED                0x00000001
#define NVOS32_ATTR_COHERENCY_WRITE_COMBINE         0x00000002
#define NVOS32_ATTR_PHYSICALITY                     13:12
#define NVOS32_ATTR_PHYSICALITY_DEFAULT             0x00000000
#define NVOS32_ATTR_PHYSICALITY_NONCONTIGUOUS       0x00000001
#define NVOS32_ATTR_PHYSICALITY_CONTIGUOUS          0x00000002
#define NVOS32_ATTR_FORMAT                          17:14
#define NVOS32_ATTR_FORMAT_PITCH                    0x00000000
#define NVOS32_ATTR_FORMAT_BLOCK_LINEAR             0x00000001
#define NVOS32_ATTR_DEPTH                           21:18
#define NVOS32_ATTR_DEPTH_UNKNOWN                   0x00000000
#define NVOS32_ATTR_DEPTH_8                         0x00000001
#define NVOS32_ATTR_DEPTH_16                        0x00000002
#define NVOS32_ATTR_DEPTH_24                        0x00000003
#define NVOS32_ATTR_DEPTH_32                        0x00000004
#define NVOS32_ATTR_DEPTH_64                        0x00000005
#define NVOS32_ATTR_DEPTH_128                       0x00000006
#define NVOS32_ATTR_COMPR                           25:23
#define NVOS32_ATTR_COMPR_NONE                      0x00000000
#define NVOS32_ATTR_ZCULL                           29:28
#define NVOS32_ATTR_ZCULL_NONE                      0x00000000

#define NVOS32_ATTR2_GPU_CACHEABLE                  1:0
#define NVOS32_ATTR2_GPU_CACHEABLE_DEFAULT          0x00000000
#define NVOS32_ATTR2_GPU_CACHEABLE_YES              0x00000001
#define NVOS32_ATTR2_GPU_CACHEABLE_NO               0x00000002
#define NVOS32_ATTR2_ZBC                            3:2
#define NVOS32_ATTR2_ZBC_DEFAULT                    0x00000000
#define NVOS32_ATTR2_ZBC_PREFER_NO_ZBC              0x00000001
#define NVOS32_ATTR2_ZBC_PREFER_ZBC                 0x00000002
#define NVOS32_ATTR2_ZBC_REQUIRE_ONLY_ZBC           0x00000003

/* Build ATTR values as composed bitfields matching kernel DRF_DEF patterns.
 * LOCATION in bits 1:0, PAGE_SIZE in 5:3, COHERENCY in 9:7, PHYSICALITY in 13:12
 */
#define NV_OS32_ATTR_MAKE(loc, pgsz, coh, phys) \
	(((NvU32)(loc) & 3u) | (((NvU32)(pgsz) & 7u) << 3) | \
	 (((NvU32)(coh) & 7u) << 7) | (((NvU32)(phys) & 3u) << 12))

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
#define NV_OS32_ATTR2_GPU_CACHEABLE_NO_VAL \
	(NVOS32_ATTR2_GPU_CACHEABLE_NO)

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

/* Engine types (ctrl2080gpu.h subset) */
#define NV2080_ENGINE_TYPE_NULL         0x00000000
#define NV2080_ENGINE_TYPE_GRAPHICS     0x00000001
#define NV2080_ENGINE_TYPE_COPY0        0x0000000f
#define NV2080_ENGINE_TYPE_COPY1        0x00000010
#define NV2080_ENGINE_TYPE_COPY2        0x0000001a
#define NV2080_ENGINE_TYPE_BSP          0x00000013
#define NV2080_ENGINE_TYPE_VP           0x00000014
#define NV2080_ENGINE_TYPE_SEC2         0x0000002c
#define NV2080_ENGINE_TYPE_NVDEC0       0x0000001b
#define NV2080_ENGINE_TYPE_NVENC0       0x0000001c
#define NV2080_ENGINE_TYPE_NVENC1       0x0000001d
#define NV2080_ENGINE_TYPE_NVJPEG0      0x0000002d
#define NV2080_ENGINE_TYPE_OFA          0x0000002e
#define NV2080_ENGINE_TYPE_SW           0x00000020

/* GPFIFO schedule control (Kepler+ channel class, works for later GPFIFO too) */
#define NVA06F_CTRL_CMD_GPFIFO_SCHEDULE  0xa06f0103
#define NVC36F_CTRL_CMD_GPFIFO_GET_WORK_SUBMIT_TOKEN 0xc36f0108

typedef struct {
	NvBool bEnable;
	NvBool bSkipSubmit;
} NVA06F_CTRL_GPFIFO_SCHEDULE_PARAMS;

typedef struct {
	NvU32 workSubmitToken;
} NVC36F_CTRL_CMD_GPFIFO_GET_WORK_SUBMIT_TOKEN_PARAMS;

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

/* GPFIFO entry format (NV506F/NVC36F - 8 bytes) */
#define NV_GP_ENTRY_SIZE                8
#define NV_GP_ENTRY0_GET_SHIFT          2
#define NV_GP_ENTRY1_GET_HI_MASK        0xff
#define NV_GP_ENTRY1_PRIV_SHIFT         8
#define NV_GP_ENTRY1_LEVEL_SHIFT        9
#define NV_GP_ENTRY1_LENGTH_SHIFT       10
#define NV_GP_ENTRY1_LENGTH_MASK        0x1fffff

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

/* VASpace / virtual memory / usermode doorbell (class headers + nvos.h) */
#define FERMI_VASPACE_A                 0x000090f1
#define VOLTA_USERMODE_A                0x0000c361
#define HOPPER_USERMODE_A               0x0000c661
#define NVC361_NV_USERMODE__SIZE        65536
#define NVC361_NOTIFY_CHANNEL_PENDING   0x00000090

#define NV_VASPACE_ALLOCATION_INDEX_GPU_NEW     0x00
#define NV_VASPACE_ALLOCATION_INDEX_GPU_HOST    0x01
#define NV_VASPACE_ALLOCATION_INDEX_GPU_GLOBAL  0x02
#define NV_VASPACE_ALLOCATION_INDEX_GPU_DEVICE  0x03
#define NV_VASPACE_ALLOCATION_INDEX_GPU_FLA     0x04

#define NV_VASPACE_ALLOCATION_FLAGS_NONE                    0x00000000
#define NV_VASPACE_ALLOCATION_FLAGS_MINIMIZE_PTETABLE_SIZE  (1u << 0)
#define NV_VASPACE_ALLOCATION_FLAGS_RETRY_PTE_ALLOC_IN_SYS  (1u << 1)
#define NV_VASPACE_ALLOCATION_FLAGS_SHARED_MANAGEMENT       (1u << 2)
#define NV_VASPACE_ALLOCATION_FLAGS_ENABLE_PAGE_FAULTING    (1u << 6)

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

/* NVOS46: map memory into a DMA / VASpace (NV_ESC_RM_MAP_MEMORY_DMA) */
#define NVOS46_FLAGS_ACCESS_READ_WRITE     0x00000000
#define NVOS46_FLAGS_ACCESS_READ_ONLY      0x00000001
#define NVOS46_FLAGS_ACCESS_WRITE_ONLY     0x00000002
#define NVOS46_FLAGS_PAGE_SIZE_DEFAULT     0x00000000
#define NVOS46_FLAGS_PAGE_SIZE_4KB         (0x00000001u << 8)
#define NVOS46_FLAGS_PAGE_SIZE_BIG         (0x00000002u << 8)
#define NVOS46_FLAGS_DMA_OFFSET_FIXED      (1u << 4)
#define NVOS46_FLAGS_DMA_OFFSET_FIXED_TRUE (1u << 4)

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
#define NV2080_CTRL_FB_INFO_INDEX_HEAP_SIZE              0
#define NV2080_CTRL_FB_INFO_INDEX_HEAP_FREE              1
#define NV2080_CTRL_FB_INFO_INDEX_HEAP_START             2
#define NV2080_CTRL_FB_INFO_INDEX_RAM_SIZE               4
#define NV2080_CTRL_FB_INFO_INDEX_USABLE_RAM_SIZE        23
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
#define NV2080_CTRL_GR_INFO_INDEX_SM_VERSION             1
#define NV2080_CTRL_GR_INFO_INDEX_THREAD_STACK_SCALING_FACTOR 9
#define NV2080_CTRL_GR_INFO_INDEX_MAX_WARPS_PER_SM       13
#define NV2080_CTRL_GR_INFO_INDEX_SHADER_PIPE_COUNT      14
#define NV2080_CTRL_GR_INFO_INDEX_SHADER_PIPE_SUB_COUNT  15
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
