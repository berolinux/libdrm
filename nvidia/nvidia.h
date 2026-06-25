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

#include <stddef.h>
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
	/* tick106: MIG/SLI topology hints (1 = single subdevice default) */
	uint32_t subdevice_count;
	/* tick107: NV0080_CTRL_CMD_GPU_GET_VIRTUALIZATION_MODE (baremetal/vGPU/guest) */
	uint32_t virtualization_mode; /* NV0080_CTRL_GPU_VIRTUALIZATION_MODE_* */
	uint32_t is_grid_build;       /* non-zero if GRID/vGPU host build flag */
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
	/* tick101: refined FB_GET_INFO_V2 (BAR1 / heap / ECC) */
	uint64_t fb_heap_size;        /* HEAP_SIZE (KB<<10 when stored as bytes) */
	uint64_t fb_heap_start;       /* HEAP_START offset when reported */
	uint64_t bar1_size;           /* BAR1 aperture size */
	uint64_t bar1_avail_size;     /* BAR1 available for mapping */
	uint32_t fbpa_ecc_enabled;    /* non-zero if FBPA ECC enabled */
	uint32_t sm_version;
	uint32_t gpc_count;
	uint32_t tpc_count;
	/* tick102/104: GR_GET_INFO_V2 for QMD / CTA limits */
	uint32_t max_warps_per_sm;       /* NV2080 MAX_WARPS_PER_SM */
	uint32_t thread_stack_scaling;   /* THREAD_STACK_SCALING_FACTOR */
	uint32_t max_threads_per_warp;   /* usually 32; validate on silicon */
	uint32_t max_sp_per_sm;          /* shader processors per SM */
	uint32_t gpu_core_count;         /* NV2080 GPU_CORE_COUNT (SM total) */
	/* tick96: refined probe (FB regions / page size / PCI ids from RM) */
	uint32_t fb_region_count;     /* NV2080 FB_GET_FB_REGION_INFO numFBRegions */
	uint64_t fb_region0_base;     /* first region base (phys) */
	uint64_t fb_region0_limit;    /* first region limit (phys, inclusive-style) */
	uint64_t max_page_size;       /* GPU_GET_MAX_SUPPORTED_PAGE_SIZE */
	uint32_t rm_pci_device_id;    /* BUS_GET_PCI_INFO pciDeviceId (vend|dev) */
	uint32_t rm_pci_subsystem_id;
	uint32_t rm_pci_revision_id;
	/* tick98: NV0000 system probe (client-level; same for all GPUs on client) */
	uint32_t rm_changelist;       /* SYSTEM_GET_BUILD_VERSION changelistNumber */
	uint32_t rm_official_cl;      /* officialChangelistNumber */
	uint32_t rm_platform_type;    /* SYSTEM_GET_PLATFORM_TYPE systemType */
	char     rm_driver_version[64]; /* e.g. "610.43.02" when RM fills buffer */
	char     rm_build_branch[64];
	/* tick99: GPU_GET_GID_INFO ASCII UUID (e.g. GPU-xxxxxxxx-xxxx-...) */
	char     gpu_uuid[48];
	uint8_t  gpu_gid_binary[16];  /* SHA-1 binary GID when available */
	uint32_t gpu_gid_binary_len;
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
 * tick93: Dup an RM object into this client (NV_ESC_RM_DUP_OBJECT / NVOS55).
 * h_parent_dst: parent under destination client (often same as h_client for mem).
 * *h_object_dst: 0 to let RM assign; returns assigned handle.
 * h_client_src/h_object_src: source (may equal destination client for self-dup).
 */
int nvidia_rm_dup_object(nvidia_device_handle device,
			 uint32_t h_parent_dst,
			 uint32_t *h_object_dst,
			 uint32_t h_client_src,
			 uint32_t h_object_src,
			 uint32_t flags);

/**
 * Drain one pending OS event from the RM event queue (NV_ESC_RM_GET_EVENT_DATA).
 * Fills *h_object / *notify_index / info fields from NvUnixEvent.
 * *more_events_out: non-zero if more events remain queued.
 */
int nvidia_rm_get_event_data(nvidia_device_handle device,
			     uint32_t *h_object_out,
			     uint32_t *notify_index_out,
			     uint32_t *info32_out,
			     uint16_t *info16_out,
			     uint32_t *more_events_out);

/**
 * Allocate NV01_EVENT_OS_EVENT under h_parent (usually subdevice or device),
 * bound to h_src_resource (object generating events) and Linux event fd.
 * notify_index: NV2080_NOTIFIERS_* value, optionally OR'd with NV01_EVENT_* flags.
 * Also call nvidia_rm_alloc_os_event() to associate fd with the device ioctl path.
 */
int nvidia_rm_alloc_event_os(nvidia_device_handle device,
			     uint32_t h_parent,
			     uint32_t h_src_resource,
			     int os_event_fd,
			     uint32_t notify_index,
			     uint32_t *h_event_out);

/**
 * Enable/disable subdevice event notification (NV2080_CTRL_CMD_EVENT_SET_NOTIFICATION).
 * action: NV2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_{DISABLE,SINGLE,REPEAT}.
 * Requires NV01_EVENT previously bound to the subdevice.
 */
int nvidia_rm_event_set_notification(nvidia_device_handle device,
				     uint32_t h_subdevice,
				     uint32_t event,
				     uint32_t action,
				     bool notify_state,
				     uint32_t info32,
				     uint16_t info16);

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

/**
 * tick100: RmAlloc via NV_MEMORY_ALLOCATION_PARAMS with optional 2D/VAS fields.
 * h_vaspace 0 = default; pitch 0 = RM chooses when width/height set.
 */
int nvidia_rm_memory_alloc_ex(nvidia_device_handle device,
			      uint32_t h_parent, uint32_t *h_memory_out,
			      uint32_t h_class, uint32_t type, uint32_t flags,
			      uint32_t attr, uint32_t attr2, uint32_t format,
			      uint32_t width, uint32_t height, int32_t pitch,
			      uint64_t size, uint64_t alignment,
			      uint32_t h_vaspace,
			      uint64_t *offset_out, uint64_t *limit_out,
			      int32_t *pitch_out);

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

/**
 * Bind channel to engine runlist (NVA06F_CTRL_CMD_BIND 0xa06f0104).
 * Pass7/8: always before SCHEDULE on graphics path (glcore a52b69).
 */
int nvidia_rm_gpfifo_bind(nvidia_device_handle device,
			  uint32_t h_channel,
			  uint32_t engine_type);

/** Bind then schedule (canonical cold-path order). */
int nvidia_rm_gpfifo_bind_and_schedule(nvidia_device_handle device,
				       uint32_t h_channel,
				       uint32_t engine_type,
				       bool enable);

/**
 * Optional A06F recovery/priority (pass8/9; rare in graphics imm scan).
 * SET/GET_INTERLEAVE_LEVEL (0xa06f0109/0110), RESTART_RUNLIST (0111),
 * STOP_CHANNEL (0112), GET_CONTEXT_ID (0113).  Best-effort; RM may reject.
 */
int nvidia_rm_gpfifo_set_interleave_level(nvidia_device_handle device,
					  uint32_t h_channel,
					  uint32_t tsg_interleave_level);
int nvidia_rm_gpfifo_get_interleave_level(nvidia_device_handle device,
					  uint32_t h_channel,
					  uint32_t *tsg_interleave_level_out);
int nvidia_rm_gpfifo_restart_runlist(nvidia_device_handle device,
				     uint32_t h_channel,
				     bool bypass_wait_for_eng_idle);
int nvidia_rm_gpfifo_stop_channel(nvidia_device_handle device,
				  uint32_t h_channel,
				  bool in_preempt_timeout);
int nvidia_rm_gpfifo_get_context_id(nvidia_device_handle device,
				    uint32_t h_channel,
				    uint32_t *context_id_out);

/**
 * A06F SET_ERROR_NOTIFIER (0xa06f0108): attach/refresh channel error notifier
 * policy.  notify_each_channel_in_tsg mirrors bNotifyEachChannelInTSG.
 */
int nvidia_rm_gpfifo_set_error_notifier(nvidia_device_handle device,
					uint32_t h_channel,
					bool notify_each_channel_in_tsg);

/**
 * tick91: KEPLER_CHANNEL_GROUP_A (A06C) TSG management (target h_channel_group).
 * Used by multi-channel / CUDA-style groups; GL often uses single channel (A06F).
 */
int nvidia_rm_tsg_set_timeslice(nvidia_device_handle device,
				uint32_t h_channel_group,
				uint64_t timeslice_us);
int nvidia_rm_tsg_get_timeslice(nvidia_device_handle device,
				uint32_t h_channel_group,
				uint64_t *timeslice_us_out);
/** PREEMPT: wait=true blocks until preempt completes (or RM timeout). */
int nvidia_rm_tsg_preempt(nvidia_device_handle device, uint32_t h_channel_group,
			  bool wait, bool manual_timeout, uint32_t timeout_us);
int nvidia_rm_tsg_get_info(nvidia_device_handle device, uint32_t h_channel_group,
			   uint32_t *tsg_id_out);
int nvidia_rm_tsg_set_interleave_level(nvidia_device_handle device,
				       uint32_t h_channel_group,
				       uint32_t tsg_interleave_level);
int nvidia_rm_tsg_get_interleave_level(nvidia_device_handle device,
				       uint32_t h_channel_group,
				       uint32_t *tsg_interleave_level_out);
/** MAKE_REALTIME: privileged; best-effort (may fail for unprivileged clients). */
int nvidia_rm_tsg_make_realtime(nvidia_device_handle device,
				uint32_t h_channel_group, bool realtime);

/** Get work submit token for doorbell (Volta+; NVC36F_CTRL_CMD_GPFIFO_GET_WORK_SUBMIT_TOKEN) */
int nvidia_rm_gpfifo_get_work_submit_token(nvidia_device_handle device,
					   uint32_t h_channel,
					   uint32_t *token_out);

/**
 * tick92: NV0080 device FIFO controls (target = device handle NV01_DEVICE_0).
 * STOP/START_RUNLIST: per-engine runlist pause/resume (round-robin mode only).
 * IDLE_CHANNELS: deschedule + wait for listed channels (max helper 64 handles).
 * GET_LATENCY_BUFFER_SIZE: engine gp/pb entry counts for ring sizing diagnostics.
 * GET_CAPS_V2: 2-byte FIFO capability table.
 */
int nvidia_rm_fifo_stop_runlist(nvidia_device_handle device, uint32_t engine_id);
int nvidia_rm_fifo_start_runlist(nvidia_device_handle device, uint32_t engine_id);
int nvidia_rm_fifo_get_latency_buffer_size(nvidia_device_handle device,
					   uint32_t engine_id,
					   uint32_t *gp_entries_out,
					   uint32_t *pb_entries_out);
int nvidia_rm_fifo_get_caps_v2(nvidia_device_handle device,
			       uint8_t *caps_tbl_out, size_t caps_tbl_bytes);
int nvidia_rm_fifo_idle_channels(nvidia_device_handle device,
				 const uint32_t *h_channels, uint32_t num_channels,
				 uint32_t flags, uint32_t timeout_us);

/**
 * C36F UPDATE_FAULT_METHOD_BUFFER (0xc36f0109): set bar2 fault method buffer
 * addresses for runqueue 0/1 (SR-IOV/vGPU; optional on bare metal).
 */
int nvidia_rm_gpfifo_update_fault_method_buffer(nvidia_device_handle device,
						uint32_t h_channel,
						uint64_t bar2_addr_rq0,
						uint64_t bar2_addr_rq1);

/** Pack an 8-byte GPFIFO entry (NV506F/NVC36F format) into entry[2] */
void nvidia_gp_entry_pack(uint32_t entry[2], uint64_t gpu_addr,
			  uint32_t length_dwords, bool wait, bool priv);
/** wait/priv mapped via NV_GP_ENTRY_F_* flags (SYNC_WAIT separate from LEVEL). */
void nvidia_gp_entry_pack_flags(uint32_t entry[2], uint64_t gpu_addr,
				uint32_t length_dwords, uint32_t flags);

/**
 * True if gpfifo_class should ring usermode doorbell after GPPut.
 * 610.43.02 glcore@ac5557: only when class > 0xC36E (C36F+). class==0 => yes.
 */
bool nvidia_gpfifo_class_needs_doorbell(uint32_t gpfifo_class);

/** Zero USERD (incl. GPGet/GPPut) before first GPFIFO submit; userd_bytes >= 0x90. */
void nvidia_userd_init_host(volatile void *userd, size_t userd_bytes);

/**
 * Map GPU_GET_MAX_SUPPORTED_PAGE_SIZE (or any max) to FERMI_VASPACE_A bigPageSize.
 * Returns 0 for RM default when max is 0 or below 64K.
 */
uint32_t nvidia_rm_vaspace_normalize_big_page(uint64_t max_page_size);

/** Allocate FERMI_VASPACE_A on the device (private GPU VA space) */
int nvidia_rm_vaspace_alloc(nvidia_device_handle device,
			    uint32_t *h_vaspace_out,
			    uint32_t index, uint32_t flags,
			    uint64_t va_size, uint64_t va_base,
			    uint32_t big_page_size,
			    uint64_t *va_size_out, uint64_t *va_base_out);

/* tick97: public mirror of RS_SHARE_POLICY / NVOS57 (rs_access.h + nvos.h) */
#define NVIDIA_RS_ACCESS_DUP_OBJECT  0u
#define NVIDIA_RS_SHARE_TYPE_ALL     1u
#define NVIDIA_RS_SHARE_TYPE_CLIENT  3u
#define NVIDIA_RS_SHARE_TYPE_PID     4u
#define NVIDIA_RS_SHARE_ACTION_COMPOSE (1u << 2)

struct nvidia_rm_share_policy {
	uint32_t target;
	uint32_t access_mask_limb0; /* RS_ACCESS_MASK.limbs[0] */
	uint16_t type;              /* RS_SHARE_TYPE_* */
	uint8_t  action;            /* RS_SHARE_ACTION_FLAG_* */
	uint8_t  _pad;
};

/**
 * tick97: NVOS57 / NV_ESC_RM_SHARE — share RM object rights.
 */
int nvidia_rm_share_object(nvidia_device_handle device, uint32_t h_object,
			   const struct nvidia_rm_share_policy *policy);

/**
 * Convenience: share h_object with all clients, DUP access, compose action.
 * Useful for export/FD paths that need explicit share policy.
 */
int nvidia_rm_share_object_all_dup(nvidia_device_handle device, uint32_t h_object);

/**
 * tick98: NV0000_CTRL_CMD_SYSTEM_GET_BUILD_VERSION on client.
 * Any output pointer may be NULL. Strings truncated to provided sizes.
 */
int nvidia_rm_system_get_build_version(nvidia_device_handle device,
				       char *driver_ver_out, size_t driver_ver_sz,
				       char *branch_out, size_t branch_sz,
				       char *title_out, size_t title_sz,
				       uint32_t *changelist_out,
				       uint32_t *official_cl_out);

/** NV0000_CTRL_CMD_SYSTEM_GET_PLATFORM_TYPE (desktop/mobile/etc). */
int nvidia_rm_system_get_platform_type(nvidia_device_handle device,
				       uint32_t *platform_type_out);

/**
 * tick99: NV2080_CTRL_CMD_GPU_GET_GID_INFO (GPU UUID string / binary SHA-1).
 * Either output may be NULL. gid_ascii_sz includes space for NUL.
 */
int nvidia_rm_gpu_get_gid_info(nvidia_device_handle device,
			       char *gid_ascii_out, size_t gid_ascii_sz,
			       uint8_t *gid_binary_out, uint32_t *gid_binary_len_inout);

/**
 * NVOS32 ALLOC_TILED_PITCH_HEIGHT — 2D/scanout surface on vidheap.
 * pitch may be 0 (RM computes); pitch_out receives allocated pitch.
 */
int nvidia_rm_vidheap_alloc_tiled(nvidia_device_handle device,
				  uint32_t type, uint32_t flags,
				  uint32_t width, uint32_t height, uint32_t pitch,
				  uint32_t attr, uint32_t attr2, uint32_t format,
				  uint32_t *h_memory_out, uint64_t *offset_out,
				  uint64_t *limit_out, uint32_t *pitch_out);

/**
 * tick102: pick NVOS46 PAGE_SIZE field selector from GPU max page size /
 * mapping length.  Returns unshifted NVOS46_FLAGS_PAGE_SIZE_* value.
 * Prefer largest supported: 512M > HUGE(2M) > BIG(64/128K) > 4K > DEFAULT.
 */
uint32_t nvidia_rm_os46_pick_page_size(uint64_t max_gpu_page_size,
				       uint64_t map_length);

/**
 * tick103: pick page size considering BAR1 aperture pressure.
 * bar1_avail_bytes: from FB_GET_INFO BAR1_AVAIL_SIZE (0 = unknown, ignore).
 * When BAR1 is tight vs map_length, prefer smaller pages (BIG/4K) to reduce
 * fragmentation / wasted aperture vs huge pages spanning more BAR1 range.
 */
uint32_t nvidia_rm_os46_pick_page_size_bar1(uint64_t max_gpu_page_size,
					    uint64_t map_length,
					    uint64_t bar1_avail_bytes);

/** Build NVOS46 flags for RW access with selected page size (no fixed VA).
 *  Implemented in nvidia_rm.c (needs NVOS46_MAKE_FLAGS from nvidia_rm.h). */
uint32_t nvidia_rm_os46_flags_rw(uint32_t page_size_sel);

/**
 * tick105: recommend NVOS32 ATTR page-size selector from GPU max_page_size.
 * Returns unshifted NVOS32_ATTR_PAGE_SIZE_* (DEFAULT/4KB/BIG/HUGE).
 * Used when building memory_alloc_ex ATTR beyond the fixed 4K presets.
 */
uint32_t nvidia_rm_os32_pick_attr_page_size(uint64_t max_gpu_page_size,
					    uint64_t alloc_size);

/** Map physical/sysmem BO into a VASpace or CTXDMA (NVOS46 / MAP_MEMORY_DMA) */
int nvidia_rm_map_memory_dma(nvidia_device_handle device,
			     uint32_t h_device,
			     uint32_t h_dma,
			     uint32_t h_memory,
			     uint64_t offset,
			     uint64_t length,
			     uint32_t flags,
			     uint64_t *dma_offset_inout);

/**
 * tick102: NVOS46 map with automatic page-size retry ladder (BIG/HUGE/4K/default).
 * max_gpu_page_size: from GPU_GET_MAX_SUPPORTED_PAGE_SIZE (0 = try all reasonable).
 * Tries largest feasible page size first; falls back if RM rejects.
 */
int nvidia_rm_map_memory_dma_auto(nvidia_device_handle device,
				  uint32_t h_device,
				  uint32_t h_dma,
				  uint32_t h_memory,
				  uint64_t offset,
				  uint64_t length,
				  uint64_t max_gpu_page_size,
				  uint64_t *dma_offset_inout,
				  uint32_t *flags_used_out);

/**
 * tick103: same as map_memory_dma_auto but applies BAR1-aware page preference
 * (bar1_avail_bytes=0 disables BAR1 clamp, same as auto).
 */
int nvidia_rm_map_memory_dma_auto_bar1(nvidia_device_handle device,
				       uint32_t h_device,
				       uint32_t h_dma,
				       uint32_t h_memory,
				       uint64_t offset,
				       uint64_t length,
				       uint64_t max_gpu_page_size,
				       uint64_t bar1_avail_bytes,
				       uint64_t *dma_offset_inout,
				       uint32_t *flags_used_out);

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
/**
 * tick94: Bind an allocated CTXDMA to a channel (NVOS49 / NV_ESC_RM_BIND_CONTEXT_DMA).
 * Required before some engines can reference error/notify CTXDMA on the channel.
 */
int nvidia_rm_bind_context_dma(nvidia_device_handle device,
			       uint32_t h_channel,
			       uint32_t h_ctxdma);

/**
 * tick94: Idle a single channel (NVOS30 / NV_ESC_RM_IDLE_CHANNELS).
 * flags: NVOS30_FLAGS_*; timeout_us: GPU timeout (0 = RM default).
 * Prefer NV0080_CTRL_CMD_FIFO_IDLE_CHANNELS for device-level multi-channel idle.
 */
int nvidia_rm_idle_channel(nvidia_device_handle device,
			   uint32_t h_channel,
			   uint32_t flags,
			   uint32_t timeout_us);

/**
 * tick94: Allocate NV01_MEMORY_VIRTUAL (or NV50_MEMORY_VIRTUAL) covering a VA range.
 * h_vaspace: FERMI_VASPACE_A handle, or 0 for default GPU VAS; use
 * NV_MEMORY_VIRTUAL_SYSMEM_DYNAMIC_HVASPACE for dynamic-sysmem compat.
 * *limit_inout: 0 = max limit (returned); non-zero requested limit.
 */
int nvidia_rm_memory_virtual_alloc(nvidia_device_handle device,
				   uint32_t h_parent,
				   uint32_t *h_memory_out,
				   uint32_t h_vaspace,
				   uint64_t offset,
				   uint64_t *limit_inout,
				   uint32_t h_class);

/**
 * tick95: GPU timer nanoseconds (NV2080_CTRL_CMD_TIMER_GET_TIME on subdevice).
 * Useful for sema/timeout correlation vs host clock.
 */
int nvidia_rm_timer_get_time(nvidia_device_handle device,
			     uint64_t *time_nsec_out);

/**
 * tick96: FB region map (NV2080_CTRL_CMD_FB_GET_FB_REGION_INFO) — up to 16 regions.
 * Fills *num_regions_out and optionally copies first region base/limit.
 */
int nvidia_rm_fb_get_region_info(nvidia_device_handle device,
				 uint32_t *num_regions_out,
				 uint64_t *region0_base_out,
				 uint64_t *region0_limit_out);

/** GPU maximum supported page size (huge-page VAS alloc hints). */
int nvidia_rm_gpu_get_max_page_size(nvidia_device_handle device,
				    uint64_t *max_page_size_out);

/** BUS_GET_PCI_INFO on subdevice (RM-internal PCI ids; complements CARD_INFO). */
int nvidia_rm_bus_get_pci_info(nvidia_device_handle device,
			       uint32_t *pci_device_id_out,
			       uint32_t *pci_subsystem_id_out,
			       uint32_t *pci_revision_id_out);

/**
 * Export an RM object to a Unix FD (NV0000_CTRL_CMD_OS_UNIX_EXPORT_OBJECT_TO_FD).
 * *fd_inout: pass -1 to request RM/library assign (kernel may still want pre-opened fd);
 *            on success contains the export fd (caller owns).
 * h_parent: parent of object (often device or channel parent).
 */
int nvidia_rm_export_object_to_fd(nvidia_device_handle device,
				  uint32_t h_parent,
				  uint32_t h_object,
				  int *fd_inout,
				  uint32_t flags);

/**
 * Import an RM object handle from an export FD into this client.
 * *h_object_out: new handle under h_parent in this client.
 */
int nvidia_rm_import_object_from_fd(nvidia_device_handle device,
				    uint32_t h_parent,
				    int import_fd,
				    uint32_t *h_object_out);

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
 * Order matches 610.43.02 glcore@ac5540 (entry → GPPut@0x8c → sfence → doorbell@0x90).
 * gpfifo_cpu: host pointer to ring (2 dwords per entry)
 * gpfifo_entries: ring capacity
 * *gpfifo_put_inout: in/out next write index (wrapped)
 * userd: mapped USERD control block (GPPut/GPGet)
 * pb_gpu_addr / pb_dwords: pushbuffer segment to submit (dwords; max 0x1fffff)
 * usermode_map / work_submit_token: Volta+ doorbell (map may be NULL)
 * stall_timeout_ns: max wait if ring full (0 = no wait, return -EAGAIN)
 *
 * submit_one_ex: pass gpfifo_class so doorbell is skipped for class <= 0xC36E.
 * submit_one: class=0 (doorbell whenever token+map — backward compatible).
 */
int nvidia_gpfifo_submit_one(uint32_t *gpfifo_cpu, uint32_t gpfifo_entries,
			     uint32_t *gpfifo_put_inout,
			     volatile void *userd,
			     uint64_t pb_gpu_addr, uint32_t pb_dwords,
			     volatile void *usermode_map,
			     uint32_t work_submit_token,
			     bool has_work_submit_token,
			     uint64_t stall_timeout_ns);

/** Like submit_one but gates doorbell on gpfifo_class (> 0xC36E only; 0 = allow). */
int nvidia_gpfifo_submit_one_ex(uint32_t *gpfifo_cpu, uint32_t gpfifo_entries,
				uint32_t *gpfifo_put_inout,
				volatile void *userd,
				uint64_t pb_gpu_addr, uint32_t pb_dwords,
				volatile void *usermode_map,
				uint32_t work_submit_token,
				bool has_work_submit_token,
				uint32_t gpfifo_class,
				uint64_t stall_timeout_ns);

/**
 * Pass7/glcore@ac5540 multi-USERD kick: write one ring entry, publish GPPut to
 * every non-NULL USERD in userd_maps[0..userd_count-1] (max NV_GP_MAX_USERD_SLOTS),
 * then one doorbell (or multi if usermode_maps provided).
 *
 * userd_maps: array of USERD host mappings (NULL slots skipped).
 * userd_count: number of entries in userd_maps (clamped to NV_GP_MAX_USERD_SLOTS).
 * usermode_maps: optional parallel array of usermode maps for multi-doorbell; if
 *   NULL, only usermode_map (single) is rung when class needs doorbell.
 * usermode_count: length of usermode_maps (0 = use single usermode_map only).
 *
 * If userd_count<=1 and userd_maps[0] (or userd_maps NULL with userd via maps[0]
 * convention): pass userd_maps with one element, or use submit_one_ex instead.
 */
int nvidia_gpfifo_submit_one_multi(uint32_t *gpfifo_cpu, uint32_t gpfifo_entries,
				   uint32_t *gpfifo_put_inout,
				   volatile void *const *userd_maps,
				   unsigned userd_count,
				   uint64_t pb_gpu_addr, uint32_t pb_dwords,
				   volatile void *usermode_map,
				   volatile void *const *usermode_maps,
				   unsigned usermode_count,
				   uint32_t work_submit_token,
				   bool has_work_submit_token,
				   uint32_t gpfifo_class,
				   uint64_t stall_timeout_ns);

/** Poll USERD until GPGet catches GPPut (or timeout). target_put = ring put index. */
int nvidia_userd_wait_gpfifo_idle(volatile void *userd, uint32_t target_put,
				  uint64_t timeout_ns);

/** Read USERD GPGet/GPPut (optional out pointers). */
int nvidia_userd_read_gpfifo(volatile void *userd,
			     uint32_t *get_out, uint32_t *put_out);
/** Read GPGet/GPPut and PB Get/Put from USERD (bring-up diagnostics). */
int nvidia_userd_snapshot(volatile void *userd, uint32_t *gp_get_out,
			  uint32_t *gp_put_out, uint32_t *pb_get_out,
			  uint32_t *pb_put_out);

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

/**
 * After GPFIFO submit: wait sema GEQ (if sema_cpu/payload set), else wait
 * USERD GPGet==target_put.  Combines ring drain + CE/QMD sema completion.
 * notifier may be NULL; if set, non-blocking error check after sema/idle.
 */
int nvidia_submit_wait_complete(volatile void *userd, uint32_t target_put,
				volatile uint32_t *sema_cpu, uint32_t sema_payload,
				volatile void *notifier, uint64_t timeout_ns);

/**
 * Host-only smoke constants / helpers for future nvidia-smoke tool and
 * mesa trace-golden tests (no GPU required).
 */
#define NVIDIA_SMOKE_SEMA_PAYLOAD_DEFAULT  0x42u
#define NVIDIA_SMOKE_QMD_BYTES             256
#define NVIDIA_SMOKE_SPH_MIN_BYTES         256

/** Reset sema dword to 0 before a smoke submit. */
static inline void
nvidia_smoke_sema_reset(volatile uint32_t *sema_cpu)
{
	if (sema_cpu)
		sema_cpu[0] = 0;
}

/** After wait: true if sema_cpu[0] == expected (strict) or >= if allow_geq. */
static inline bool
nvidia_smoke_sema_check(volatile uint32_t *sema_cpu, uint32_t expected,
			bool allow_geq)
{
	uint32_t v;
	if (!sema_cpu)
		return false;
	v = sema_cpu[0];
	return allow_geq ? (v >= expected) : (v == expected);
}

/**
 * Present/display gate: wait sema (if set) else USERD idle, then optional
 * notifier peek.  Mirrors mesa WSI pre-present drain policy.
 */
static inline int
nvidia_smoke_present_wait(volatile void *userd, uint32_t target_put,
			  volatile uint32_t *sema_cpu, uint32_t sema_payload,
			  volatile void *notifier, uint64_t timeout_ns)
{
	return nvidia_submit_wait_complete(userd, target_put, sema_cpu,
					   sema_payload, notifier, timeout_ns);
}

/**
 * G1 bring-up recipe (userspace side, pairs with mesa nv_channel_g1_*):
 *   1. nvidia_smoke_sema_reset(sema_cpu)
 *   2. Submit CE pitch copy + LAUNCH_DMA sema one-word (or sema-only)
 *   3. nvidia_smoke_g1_wait_complete(...) — USERD/GPFIFO + sema GEQ
 *   4. nvidia_smoke_sema_check(sema_cpu, payload, true)
 *
 * Default addresses/payloads match mesa nv_smoke_selftest_g1_ce_sema_push
 * host trace (src 0x100000, dst 0x200000, sema 0x300000, payload 0x42).
 */
#define NVIDIA_SMOKE_G1_SRC_GPU_DEFAULT   0x100000ull
#define NVIDIA_SMOKE_G1_DST_GPU_DEFAULT   0x200000ull
#define NVIDIA_SMOKE_G1_SEMA_GPU_DEFAULT  0x300000ull
#define NVIDIA_SMOKE_G1_SIZE_DEFAULT      256u
/* Fallback SET_OBJECT class when RM classlist not yet refined (mesa channel resolve) */
#define NVIDIA_SMOKE_G1_CLASS_COPY_FALLBACK  0x0000c6b5u  /* AMPERE_DMA_COPY_A */

/* G2 compute smoke (pairs with mesa nv_channel_g2_* / nv_smoke_selftest_g2_*) */
#define NVIDIA_SMOKE_G2_PROG_GPU_DEFAULT  0x100000ull
#define NVIDIA_SMOKE_G2_QMD_GPU_DEFAULT   0x400000ull
#define NVIDIA_SMOKE_G2_SEMA_GPU_DEFAULT  0x300000ull
#define NVIDIA_SMOKE_G2_REGS_DEFAULT      16u
#define NVIDIA_SMOKE_G2_SASS_DEFAULT      0x86u
#define NVIDIA_SMOKE_G2_CLASS_COMPUTE_FALLBACK  0x0000c3c0u  /* VOLTA_COMPUTE_A */
#define NVIDIA_SMOKE_G2_STORE_IMM_DEFAULT  0xdeadbeefu

/* G3 3D clear/draw sema (pairs with mesa nv_channel_g3_* / nv_smoke_selftest_g3_*) */
#define NVIDIA_SMOKE_G3_CT_GPU_DEFAULT    0x500000ull
#define NVIDIA_SMOKE_G3_SEMA_GPU_DEFAULT  0x300000ull
#define NVIDIA_SMOKE_G3_CT_W_DEFAULT      64u
#define NVIDIA_SMOKE_G3_CT_H_DEFAULT      64u
#define NVIDIA_SMOKE_G3_CLASS_3D_FALLBACK  0x0000c597u  /* TURING_A_3D_A */

/**
 * HW bring-up env (mesa nv_smoke_hw / nvrm_device):
 *   NV_SMOKE_HW=1 NV_SMOKE_HW_SLICES=1 NV_SMOKE_HW_VERBOSE=1
 * G1 first; then SLICES=3 (G1+G2) or 7 (all).  Read stderr nvrm_smoke_hw line.
 */

static inline int
nvidia_smoke_g1_wait_complete(volatile void *userd, uint32_t target_put,
			      volatile uint32_t *sema_cpu, uint32_t sema_payload,
			      volatile void *notifier, uint64_t timeout_ns)
{
	int r;

	if (!sema_payload)
		sema_payload = NVIDIA_SMOKE_SEMA_PAYLOAD_DEFAULT;
	r = nvidia_submit_wait_complete(userd, target_put, sema_cpu,
					sema_payload, notifier, timeout_ns);
	if (r)
		return r;
	if (sema_cpu && !nvidia_smoke_sema_check(sema_cpu, sema_payload, true))
		return -ETIMEDOUT;
	return 0;
}

/** G2: same wait pattern as G1 (QMD sema release0 + GPFIFO idle). */
static inline int
nvidia_smoke_g2_wait_complete(volatile void *userd, uint32_t target_put,
			      volatile uint32_t *sema_cpu, uint32_t sema_payload,
			      volatile void *notifier, uint64_t timeout_ns)
{
	return nvidia_smoke_g1_wait_complete(userd, target_put, sema_cpu,
					     sema_payload, notifier, timeout_ns);
}

/** G3: 3D report sema + GPFIFO idle (same host wait as G1/G2). */
static inline int
nvidia_smoke_g3_wait_complete(volatile void *userd, uint32_t target_put,
			      volatile uint32_t *sema_cpu, uint32_t sema_payload,
			      volatile void *notifier, uint64_t timeout_ns)
{
	return nvidia_smoke_g1_wait_complete(userd, target_put, sema_cpu,
					     sema_payload, notifier, timeout_ns);
}

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
