/*
 * Copyright 2026 - Open NVIDIA userspace driver project
 * SPDX-License-Identifier: MIT
 *
 * Public API for libdrm_nvidia - userspace interface to the NVIDIA RM kernel
 * module (/dev/nvidiactl, /dev/nvidiaN) and nvidia-drm KMS nodes.
 *
 * Structure mirrors libdrm_amdgpu: device / buffer-object / RM control.
 * Implementation details derived from open-gpu-kernel-modules ioctl ABI.
 */

#ifndef _NVIDIA_DRM_USER_H_
#define _NVIDIA_DRM_USER_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct nvidia_device;
struct nvidia_bo;

typedef struct nvidia_device *nvidia_device_handle;
typedef struct nvidia_bo *nvidia_bo_handle;

/* Special timeout: infinite wait */
#define NVIDIA_TIMEOUT_INFINITE 0xffffffffffffffffull

/* PCI vendor ID for NVIDIA */
#define NVIDIA_PCI_VENDOR_ID 0x10de

/* Maximum GPUs we track per control fd (matches kernel NV_MAX_DEVICES) */
#define NVIDIA_MAX_GPUS 32

/* BO domains / heaps */
enum nvidia_bo_domain {
	NVIDIA_BO_DOMAIN_CPU = 0,      /* system memory, host-visible */
	NVIDIA_BO_DOMAIN_VRAM = 1,     /* local video memory (FB) */
	NVIDIA_BO_DOMAIN_GART = 2,     /* system memory GPU-accessible (GART/IOMMU) */
};

/* BO allocation flags */
enum nvidia_bo_flags {
	NVIDIA_BO_FLAGS_CPU_ACCESS     = (1 << 0),
	NVIDIA_BO_FLAGS_NO_CPU_ACCESS  = (1 << 1),
	NVIDIA_BO_FLAGS_NO_SCANOUT     = (1 << 2),
	NVIDIA_BO_FLAGS_CONTIGUOUS     = (1 << 3),
	NVIDIA_BO_FLAGS_PINNED         = (1 << 4),
};

/* Handle export/import types */
enum nvidia_bo_handle_type {
	NVIDIA_BO_HANDLE_TYPE_GEM_FLINK = 0,
	NVIDIA_BO_HANDLE_TYPE_KMS = 1,
	NVIDIA_BO_HANDLE_TYPE_DMA_BUF_FD = 2,
	NVIDIA_BO_HANDLE_TYPE_RM_HANDLE = 3,
};

/* GPU architecture families (from NV2080_CTRL_MC_GET_ARCH_INFO architecture field) */
enum nvidia_gpu_arch {
	NVIDIA_GPU_ARCH_UNKNOWN = 0,
	NVIDIA_GPU_ARCH_KEPLER  = 0xe0,
	NVIDIA_GPU_ARCH_MAXWELL = 0x110,
	NVIDIA_GPU_ARCH_PASCAL  = 0x130,
	NVIDIA_GPU_ARCH_VOLTA   = 0x140,
	NVIDIA_GPU_ARCH_TURING  = 0x160,
	NVIDIA_GPU_ARCH_AMPERE  = 0x170,
	NVIDIA_GPU_ARCH_ADA     = 0x190,
	NVIDIA_GPU_ARCH_HOPPER  = 0x180,
	NVIDIA_GPU_ARCH_BLACKWELL = 0x1a0,
};

/**
 * Per-GPU information returned by nvidia_query_gpu_info().
 */
struct nvidia_gpu_info {
	uint32_t gpu_id;              /* RM gpuId */
	uint32_t device_instance;     /* NV0080 device instance */
	uint32_t subdevice_instance;
	uint32_t pci_domain;
	uint32_t pci_bus;
	uint32_t pci_device;
	uint32_t pci_function;
	uint16_t pci_vendor_id;
	uint16_t pci_device_id;
	uint32_t architecture;        /* NV2080 MC arch */
	uint32_t implementation;
	uint32_t revision;
	uint64_t fb_size;             /* total framebuffer bytes */
	uint64_t fb_usable;           /* usable RAM size */
	uint64_t fb_free;             /* current free (may be 0 if not queried) */
	uint64_t reg_address;
	uint64_t reg_size;
	uint64_t fb_bar_address;
	uint64_t fb_bar_size;
	uint32_t sm_version;
	uint32_t gpc_count;
	uint32_t tpc_count;
	char     name[64];
	char     short_name[16];
	bool     valid;
};

/**
 * Device open / close.
 *
 * Opens /dev/nvidiactl (control) plus optionally /dev/nvidiaN for a specific
 * GPU. fd_drm may be an nvidia-drm render/primary node for KMS/GEM interop;
 * pass -1 if not using DRM.
 */
int nvidia_device_initialize(int fd_drm, uint32_t *major_version,
			     uint32_t *minor_version,
			     nvidia_device_handle *device_out);

/**
 * Open control + specific GPU by index (0..N-1 as enumerated by CARD_INFO).
 * fd_drm may be -1.
 */
int nvidia_device_initialize_gpu(int fd_drm, int gpu_index,
				 uint32_t *major_version, uint32_t *minor_version,
				 nvidia_device_handle *device_out);

int nvidia_device_deinitialize(nvidia_device_handle device);

/** Get the control fd (/dev/nvidiactl) */
int nvidia_device_get_fd(nvidia_device_handle device);

/** Get the per-GPU fd (/dev/nvidiaN), or -1 if not opened */
int nvidia_device_get_gpu_fd(nvidia_device_handle device);

/** Get the DRM fd passed at init, or -1 */
int nvidia_device_get_drm_fd(nvidia_device_handle device);

/** RM client handle (hClient / hRoot) */
uint32_t nvidia_device_get_client_handle(nvidia_device_handle device);

/** RM device object handle (NV01_DEVICE_0) */
uint32_t nvidia_device_get_device_handle(nvidia_device_handle device);

/** RM subdevice object handle (NV20_SUBDEVICE_0) */
uint32_t nvidia_device_get_subdevice_handle(nvidia_device_handle device);

/** Number of GPUs found via NV_ESC_CARD_INFO */
int nvidia_device_get_gpu_count(nvidia_device_handle device);

/** Query info for GPU index (0 .. gpu_count-1) */
int nvidia_query_gpu_info(nvidia_device_handle device, int gpu_index,
			  struct nvidia_gpu_info *info);

/** Query info for the GPU selected at initialize_gpu time */
int nvidia_query_selected_gpu_info(nvidia_device_handle device,
				   struct nvidia_gpu_info *info);

/** Kernel module API version string (NV_ESC_CHECK_VERSION_STR) */
int nvidia_query_rm_api_version(nvidia_device_handle device,
				char *version_out, int version_len);

/* --- RM object management --- */

/**
 * Allocate a generic RM object (NV_ESC_RM_ALLOC / NVOS64 or NVOS21).
 * h_class is an NV class id (e.g. NV01_DEVICE_0). alloc_params may be NULL.
 * On success, *h_object_new receives the assigned handle (or pass a requested
 * non-zero handle).
 */
int nvidia_rm_alloc(nvidia_device_handle device,
		    uint32_t h_parent,
		    uint32_t *h_object_new,
		    uint32_t h_class,
		    void *alloc_params,
		    uint32_t alloc_params_size);

/** Free an RM object (NV_ESC_RM_FREE / NVOS00) */
int nvidia_rm_free(nvidia_device_handle device,
		   uint32_t h_parent,
		   uint32_t h_object);

/**
 * RmControl (NV_ESC_RM_CONTROL / NVOS54).
 * params is an in/out buffer of params_size bytes for the given cmd.
 */
int nvidia_rm_control(nvidia_device_handle device,
		      uint32_t h_object,
		      uint32_t cmd,
		      void *params,
		      uint32_t params_size);

/** Map RM memory object into CPU address space (NV_ESC_RM_MAP_MEMORY) */
int nvidia_rm_map_memory(nvidia_device_handle device,
			 uint32_t h_device,
			 uint32_t h_memory,
			 uint64_t offset,
			 uint64_t length,
			 void **cpu_ptr_out,
			 uint32_t flags);

/** Unmap previously mapped memory (NV_ESC_RM_UNMAP_MEMORY) */
int nvidia_rm_unmap_memory(nvidia_device_handle device,
			   uint32_t h_device,
			   uint32_t h_memory,
			   void *cpu_ptr,
			   uint32_t flags);

/** Vid heap alloc-by-size (NV_ESC_RM_VID_HEAP_CONTROL / NVOS32 alloc size) */
int nvidia_rm_vidheap_alloc(nvidia_device_handle device,
			    uint32_t h_parent,
			    uint32_t type,
			    uint32_t flags,
			    uint64_t size,
			    uint64_t align,
			    uint32_t attr,
			    uint32_t attr2,
			    uint32_t *h_memory_out,
			    uint64_t *offset_out,
			    uint64_t *limit_out);

/** Vid heap free */
int nvidia_rm_vidheap_free(nvidia_device_handle device,
			   uint32_t h_parent,
			   uint32_t h_memory);

/** Export RM memory objects to a dma-buf fd (NV_ESC_EXPORT_TO_DMABUF_FD) */
int nvidia_rm_export_dmabuf(nvidia_device_handle device,
			    uint32_t *handles,
			    uint64_t *offsets,
			    uint64_t *sizes,
			    uint32_t num_objects,
			    uint64_t total_size,
			    int *dmabuf_fd_out);

/* --- Buffer objects (higher-level convenience over RM memory) --- */

struct nvidia_bo_alloc_request {
	uint64_t size;
	uint64_t alignment;           /* 0 = default page alignment */
	enum nvidia_bo_domain domain;
	uint32_t flags;               /* nvidia_bo_flags */
	uint32_t rm_type;             /* NVOS32_TYPE_*, 0 = DMA default */
};

struct nvidia_bo_metadata {
	uint64_t size;
	uint64_t aligned_size;
	uint64_t gpu_offset;          /* RM offset / GPU VA if known */
	uint32_t rm_handle;
	enum nvidia_bo_domain domain;
	uint32_t flags;
	bool cpu_accessible;
};

int nvidia_bo_alloc(nvidia_device_handle device,
		    struct nvidia_bo_alloc_request *req,
		    nvidia_bo_handle *bo_out);

int nvidia_bo_free(nvidia_bo_handle bo);

int nvidia_bo_query_metadata(nvidia_bo_handle bo,
			     struct nvidia_bo_metadata *meta);

/** CPU map; returns pointer in *cpu_ptr. Only valid if CPU_ACCESS was requested. */
int nvidia_bo_cpu_map(nvidia_bo_handle bo, void **cpu_ptr);

int nvidia_bo_cpu_unmap(nvidia_bo_handle bo);

/** Export BO as dma-buf fd (caller owns fd) */
int nvidia_bo_export_dmabuf(nvidia_bo_handle bo, int *dmabuf_fd_out);

/** Get underlying RM memory handle */
uint32_t nvidia_bo_get_rm_handle(nvidia_bo_handle bo);

/** Reference counting */
int nvidia_bo_ref(nvidia_bo_handle bo);
int nvidia_bo_unref(nvidia_bo_handle bo);

/* --- Utility --- */

/** Translate RM NV_STATUS code to a short string (static storage) */
const char *nvidia_rm_status_string(uint32_t status);

/** Translate GPU architecture id to a human-readable family name */
const char *nvidia_gpu_arch_name(uint32_t architecture);

/** Check whether an fd looks like an nvidia-drm node */
bool nvidia_drm_check_fd(int fd);

/** Probe /dev/nvidiactl availability without fully initializing */
bool nvidia_probe_available(void);

#ifdef __cplusplus
}
#endif

#endif /* _NVIDIA_DRM_USER_H_ */

/* --- Extended RM helpers (channel / events / memory class alloc) --- */

/** RmAlloc memory via NV_MEMORY_ALLOCATION_PARAMS (preferred over vidheap) */
int nvidia_rm_memory_alloc(nvidia_device_handle device,
			   uint32_t h_parent,
			   uint32_t *h_memory_out,
			   uint32_t h_class,
			   uint32_t type,
			   uint32_t flags,
			   uint32_t attr,
			   uint32_t attr2,
			   uint64_t size,
			   uint64_t alignment,
			   uint64_t *offset_out,
			   uint64_t *limit_out);

/** Allocate /dev/nvidia event fd association (NV_ESC_ALLOC_OS_EVENT) */
int nvidia_rm_alloc_os_event(nvidia_device_handle device,
			     uint32_t h_device,
			     int event_fd);

int nvidia_rm_free_os_event(nvidia_device_handle device,
			    uint32_t h_device,
			    int event_fd);

/** Wait for RM adapter init (NV_ESC_WAIT_OPEN_COMPLETE) */
int nvidia_rm_wait_open_complete(nvidia_device_handle device,
				 int32_t *rc_out,
				 uint32_t *adapter_status_out);

/** Schedule / enable GPFIFO channel (NVA06F_CTRL_CMD_GPFIFO_SCHEDULE) */
int nvidia_rm_gpfifo_schedule(nvidia_device_handle device,
			      uint32_t h_channel,
			      bool enable);

/** Get work submit token for doorbell (Volta+; NVC36F_CTRL_CMD_GPFIFO_GET_WORK_SUBMIT_TOKEN) */
int nvidia_rm_gpfifo_get_work_submit_token(nvidia_device_handle device,
					   uint32_t h_channel,
					   uint32_t *token_out);

/** Pack an 8-byte GPFIFO entry (NV506F/NVC36F format) into entry[2] */
void nvidia_gp_entry_pack(uint32_t entry[2], uint64_t gpu_addr,
			  uint32_t length_dwords, bool wait, bool priv);

/** Allocate FERMI_VASPACE_A on the device (private GPU VA space) */
int nvidia_rm_vaspace_alloc(nvidia_device_handle device,
			    uint32_t *h_vaspace_out,
			    uint32_t index, uint32_t flags,
			    uint64_t va_size, uint64_t va_base,
			    uint32_t big_page_size,
			    uint64_t *va_size_out, uint64_t *va_base_out);

/** Map physical/sysmem BO into a VASpace or CTXDMA (NVOS46 / MAP_MEMORY_DMA) */
int nvidia_rm_map_memory_dma(nvidia_device_handle device,
			     uint32_t h_device,
			     uint32_t h_dma,
			     uint32_t h_memory,
			     uint64_t offset,
			     uint64_t length,
			     uint32_t flags,
			     uint64_t *dma_offset_inout);

/** Unmap a prior NVOS46 mapping (NVOS47) */
int nvidia_rm_unmap_memory_dma(nvidia_device_handle device,
			       uint32_t h_device,
			       uint32_t h_dma,
			       uint32_t h_memory,
			       uint64_t dma_offset,
			       uint64_t size,
			       uint32_t flags);

/**
 * Allocate VOLTA_USERMODE_A / HOPPER_USERMODE_A on the subdevice and CPU-map it.
 * *usermode_map_out receives the mapped page; ring doorbell via
 * nvidia_rm_doorbell_ring(map, work_submit_token).
 */
int nvidia_rm_usermode_alloc_map(nvidia_device_handle device,
				 uint32_t *h_usermode_out,
				 uint32_t *h_class_out,
				 void **usermode_map_out);

/** Write work_submit_token to NVC361_NOTIFY_CHANNEL_PENDING in usermode region */
void nvidia_rm_doorbell_ring(volatile void *usermode_map,
			     uint32_t work_submit_token);

/**
 * Allocate NV01_CONTEXT_ERROR_TO_MEMORY (or FROM_MEMORY) over an existing
 * memory object; used for channel error notifiers.
 */
int nvidia_rm_context_dma_alloc(nvidia_device_handle device,
				uint32_t h_parent,
				uint32_t *h_ctxdma_out,
				uint32_t h_class,
				uint32_t h_memory,
				uint64_t offset,
				uint64_t limit,
				uint32_t flags);

/** Allocate KEPLER_CHANNEL_GROUP_A (TSG) for multi-channel/engine groups */
int nvidia_rm_channel_group_alloc(nvidia_device_handle device,
				  uint32_t *h_group_out,
				  uint32_t h_object_error,
				  uint32_t h_vaspace,
				  uint32_t engine_type);

/** Allocate FERMI_CONTEXT_SHARE_A (subcontext) for a TSG channel */
int nvidia_rm_ctxshare_alloc(nvidia_device_handle device,
			     uint32_t h_parent,
			     uint32_t *h_ctxshare_out,
			     uint32_t h_vaspace,
			     uint32_t flags);

/** Schedule/enable a channel group (NVA06C_CTRL_CMD_GPFIFO_SCHEDULE) */
int nvidia_rm_channel_group_schedule(nvidia_device_handle device,
				     uint32_t h_channel_group,
				     bool enable);

/**
 * GPFIFO submit helper: write one entry into a host-mapped GPFIFO ring,
 * advance put index, write USERD GPPut, optional doorbell ring.
 * gpfifo_cpu: host pointer to ring (2 dwords per entry)
 * gpfifo_entries: ring capacity
 * *gpfifo_put_inout: in/out next write index (wrapped)
 * userd: mapped USERD control block (GPPut/GPGet)
 * pb_gpu_addr / pb_dwords: pushbuffer segment to submit
 * usermode_map / work_submit_token: Volta+ doorbell (map may be NULL)
 * stall_timeout_ns: max wait if ring full (0 = no wait, return -EAGAIN)
 */
int nvidia_gpfifo_submit_one(uint32_t *gpfifo_cpu, uint32_t gpfifo_entries,
			     uint32_t *gpfifo_put_inout,
			     volatile void *userd,
			     uint64_t pb_gpu_addr, uint32_t pb_dwords,
			     volatile void *usermode_map,
			     uint32_t work_submit_token,
			     bool has_work_submit_token,
			     uint64_t stall_timeout_ns);

/** Poll USERD until GPGet catches GPPut (or timeout). target_put = ring put index. */
int nvidia_userd_wait_gpfifo_idle(volatile void *userd, uint32_t target_put,
				  uint64_t timeout_ns);

/** Read USERD GPGet/GPPut (optional out pointers). */
int nvidia_userd_read_gpfifo(volatile void *userd,
			     uint32_t *get_out, uint32_t *put_out);

/** Free GPFIFO ring slots (one slot reserved as full marker). */
uint32_t nvidia_gpfifo_ring_space(uint32_t gpfifo_entries,
				  uint32_t get_idx, uint32_t put_idx);

/**
 * Submit multiple pre-packed GPFIFO entries (2 dwords each in entries_data).
 * Single doorbell ring at end if any entry submitted.
 */
int nvidia_gpfifo_submit_many(uint32_t *gpfifo_cpu, uint32_t gpfifo_entries,
			      uint32_t *gpfifo_put_inout,
			      volatile void *userd,
			      const uint32_t *entries_data, uint32_t entry_count,
			      volatile void *usermode_map,
			      uint32_t work_submit_token,
			      bool has_work_submit_token,
			      uint64_t stall_timeout_ns);

/* Error notifier (NvNotification in channel error-notifier memory) */
#define NVIDIA_NOTIFIER_STATUS_DONE_SUCCESS  0x0000
#define NVIDIA_NOTIFIER_STATUS_IN_PROGRESS   0xffff

/** Read notifier status/info32; 0=ok, -EAGAIN=in progress, -EIO=error. */
int nvidia_notifier_status(volatile void *notifier,
			   uint16_t *status_out, uint32_t *info32_out);

/** Poll until notifier done/success or error; optional clear on success. */
int nvidia_notifier_wait(volatile void *notifier, bool clear_on_ok,
			 uint64_t timeout_ns);

/** Write notifier to DONE_SUCCESS (host reset before submit). */
void nvidia_notifier_reset(volatile void *notifier);

/**
 * Poll a host-mappable GPU semaphore dword until value >= payload (GEQ).
 * Used for CE/3D/host sema completion when sema memory is CPU-mapped.
 * timeout_ns 0 = try once; returns 0 ok, -ETIMEDOUT, -EINVAL.
 */
int nvidia_sema_wait_geq(volatile uint32_t *sema_cpu, uint32_t payload,
			 uint64_t timeout_ns);

/** Non-blocking: true if sema_cpu[0] >= payload. */
bool nvidia_sema_signaled_geq(volatile uint32_t *sema_cpu, uint32_t payload);

#define NVIDIA_MAX_ENGINES_LIST   84
#define NVIDIA_MAX_ENGINE_CLASSES 128

/** NV2080_CTRL_CMD_GPU_GET_ENGINES_V2 — list engine type IDs on subdevice. */
int nvidia_rm_gpu_get_engines(nvidia_device_handle device,
			      uint32_t *engine_list, uint32_t *count_inout);

/** NV2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST — classes for one engine type. */
int nvidia_rm_gpu_get_engine_classlist(nvidia_device_handle device,
				       uint32_t engine_type,
				       uint32_t *class_list,
				       uint32_t *count_inout);

/**
 * Pick highest class ID from classlist that is >= min_class and <= max_class
 * (inclusive).  Returns 0 if none match.
 */
uint32_t nvidia_pick_class_in_range(const uint32_t *class_list, uint32_t count,
				    uint32_t min_class, uint32_t max_class);
