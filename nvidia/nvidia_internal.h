/*
 * Copyright 2026 - Open NVIDIA userspace driver project
 * SPDX-License-Identifier: MIT
 *
 * Internal structures for libdrm_nvidia.
 */

#ifndef _NVIDIA_INTERNAL_H_
#define _NVIDIA_INTERNAL_H_

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "libdrm_macros.h"
#include "xf86atomic.h"
#include "nvidia.h"
#include "nvidia_rm.h"

#define NVIDIA_CTL_DEVICE_PATH   "/dev/nvidiactl"
#define NVIDIA_GPU_DEVICE_PATH   "/dev/nvidia%d"
#define NVIDIA_UVM_DEVICE_PATH   "/dev/nvidia-uvm"
#define NVIDIA_MODESET_PATH      "/dev/nvidia-modeset"

#define NVIDIA_BO_PAGE_SIZE      4096
#define NVIDIA_DEFAULT_ALIGNMENT 4096

struct nvidia_bo {
	atomic_t refcount;
	struct nvidia_device *dev;
	uint32_t rm_handle;
	uint32_t h_parent;          /* device or subdevice parent for free */
	uint64_t size;
	uint64_t aligned_size;
	uint64_t gpu_offset;
	uint64_t limit;
	enum nvidia_bo_domain domain;
	uint32_t flags;
	uint32_t rm_type;
	void *cpu_ptr;
	bool cpu_mapped;
	bool cpu_accessible;
	bool allocated;             /* true if we own the RM object */
	pthread_mutex_t map_mutex;
};

struct nvidia_device {
	int fd_ctl;                 /* /dev/nvidiactl */
	int fd_gpu;                 /* /dev/nvidiaN, or -1 */
	int fd_drm;                 /* nvidia-drm fd, or -1 */
	int gpu_index;              /* selected GPU index, or -1 for ctl-only */
	int gpu_count;

	NvHandle h_client;          /* root client (NV01_ROOT_USER) */
	NvHandle h_device;          /* NV01_DEVICE_0 */
	NvHandle h_subdevice;        /* NV20_SUBDEVICE_0 */
	bool rm_client_allocated;
	bool rm_device_allocated;
	bool rm_subdevice_allocated;

	nv_ioctl_card_info_t cards[NV_MAX_DEVICES];
	struct nvidia_gpu_info gpu_info_cache[NVIDIA_MAX_GPUS];
	bool gpu_info_valid[NVIDIA_MAX_GPUS];

	char rm_version[NV_RM_API_VERSION_STRING_LENGTH];
	bool rm_version_valid;

	/* Next handle to assign for RM objects we allocate (userspace-chosen) */
	uint32_t next_handle;
	pthread_mutex_t handle_mutex;

	pthread_mutex_t bo_mutex;
	atomic_t refcount;

	struct nvidia_device *next;
};

/* RM ioctl helpers implemented in nvidia_rm.c */
int nvidia_ioctl(int fd, int request, void *arg);
int nvidia_rm_ioctl_xfer(int fd, uint32_t cmd, void *ptr, uint32_t size);
int nvidia_rm_check_version(int fd_ctl, char *version_out, int version_len);
int nvidia_rm_card_info(int fd_ctl, nv_ioctl_card_info_t *cards, int max_cards,
			int *count_out);
int nvidia_rm_sys_params(int fd_ctl, nv_ioctl_sys_params_t *params);
int nvidia_rm_register_fd(int fd_gpu, int fd_ctl);

/* Low-level RM operations (nvidia_rm.c) */
int nvidia_rm_alloc_raw(int fd, NvHandle h_root, NvHandle h_parent,
			NvHandle *h_new, NvV32 h_class,
			void *alloc_parms, uint32_t alloc_parms_size);
int nvidia_rm_free_raw(int fd, NvHandle h_root, NvHandle h_parent,
		       NvHandle h_object);
int nvidia_rm_control_raw(int fd, NvHandle h_client, NvHandle h_object,
			  NvV32 cmd, void *params, uint32_t params_size);
int nvidia_rm_map_memory_raw(int fd, NvHandle h_client, NvHandle h_device,
			     NvHandle h_memory, NvU64 offset, NvU64 length,
			     void **cpu_ptr, NvU32 flags);
int nvidia_rm_unmap_memory_raw(int fd, NvHandle h_client, NvHandle h_device,
			       NvHandle h_memory, void *cpu_ptr, NvU32 flags);
int nvidia_rm_vidheap_alloc_raw(int fd, NvHandle h_root, NvHandle h_parent,
				NvU32 type, NvU32 flags, NvU64 size, NvU64 align,
				NvU32 attr, NvU32 attr2,
				NvHandle *h_memory, NvU64 *offset, NvU64 *limit);
int nvidia_rm_vidheap_free_raw(int fd, NvHandle h_root, NvHandle h_parent,
			       NvHandle h_memory);
int nvidia_rm_memory_alloc_raw(int fd, NvHandle h_root, NvHandle h_parent,
			       NvHandle *h_memory, NvV32 h_class,
			       NvU32 owner, NvU32 type, NvU32 flags,
			       NvU32 attr, NvU32 attr2,
			       NvU64 size, NvU64 alignment,
			       NvU64 *offset_out, NvU64 *limit_out);
int nvidia_rm_alloc_os_event_raw(int fd_ctl, NvHandle h_client, NvHandle h_device,
				 int event_fd, NvU32 *status_out);
int nvidia_rm_free_os_event_raw(int fd_ctl, NvHandle h_client, NvHandle h_device,
				int event_fd);
int nvidia_rm_wait_open_complete_raw(int fd_ctl, NvS32 *rc_out,
				     NvU32 *adapter_status_out);
int nvidia_rm_gpfifo_schedule_raw(int fd, NvHandle h_client, NvHandle h_channel,
				  NvBool enable);
int nvidia_rm_gpfifo_get_work_submit_token_raw(int fd, NvHandle h_client,
					       NvHandle h_channel,
					       NvU32 *token_out);
void nvidia_gp_entry_pack(NvU32 entry[2], NvU64 gpu_addr, NvU32 length_dwords,
			  bool wait, bool priv);
int nvidia_rm_export_dmabuf_raw(int fd, NvHandle h_client,
				NvHandle *handles, NvU64 *offsets, NvU64 *sizes,
				NvU32 num_objects, NvU64 total_size,
				int *dmabuf_fd);

/* Device helpers (nvidia_device.c) */
uint32_t nvidia_device_new_handle(struct nvidia_device *dev);
int nvidia_device_rm_setup_client(struct nvidia_device *dev);
int nvidia_device_rm_setup_device(struct nvidia_device *dev, int gpu_index);
int nvidia_device_refresh_gpu_info(struct nvidia_device *dev, int gpu_index);
int nvidia_device_open_ctl(void);
int nvidia_device_open_gpu(int minor);

/* BO helpers (nvidia_bo.c) */
struct nvidia_bo *nvidia_bo_create_internal(struct nvidia_device *dev);

#endif /* _NVIDIA_INTERNAL_H_ */
