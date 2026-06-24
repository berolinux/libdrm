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
int nvidia_rm_gpfifo_bind_raw(int fd, NvHandle h_client, NvHandle h_channel,
			      NvU32 engine_type);
int nvidia_rm_gpfifo_set_interleave_level_raw(int fd, NvHandle h_client,
					      NvHandle h_channel,
					      NvU32 tsg_interleave_level);
int nvidia_rm_gpfifo_get_interleave_level_raw(int fd, NvHandle h_client,
					      NvHandle h_channel,
					      NvU32 *tsg_interleave_level_out);
int nvidia_rm_gpfifo_restart_runlist_raw(int fd, NvHandle h_client,
					 NvHandle h_channel,
					 NvBool bypass_wait_for_eng_idle);
int nvidia_rm_gpfifo_stop_channel_raw(int fd, NvHandle h_client,
				      NvHandle h_channel,
				      NvBool in_preempt_timeout);
int nvidia_rm_gpfifo_get_context_id_raw(int fd, NvHandle h_client,
					NvHandle h_channel,
					NvU32 *context_id_out);
/* tick91: A06F SET_ERROR_NOTIFIER (0xa06f0108) */
int nvidia_rm_gpfifo_set_error_notifier_raw(int fd, NvHandle h_client,
					    NvHandle h_channel,
					    NvBool notify_each_channel_in_tsg);
/* tick91: A06C TSG controls (target = h_channel_group, not channel) */
int nvidia_rm_tsg_set_timeslice_raw(int fd, NvHandle h_client,
				    NvHandle h_channel_group, NvU64 timeslice_us);
int nvidia_rm_tsg_get_timeslice_raw(int fd, NvHandle h_client,
				    NvHandle h_channel_group,
				    NvU64 *timeslice_us_out);
int nvidia_rm_tsg_preempt_raw(int fd, NvHandle h_client,
			      NvHandle h_channel_group, NvBool wait,
			      NvBool manual_timeout, NvU32 timeout_us);
int nvidia_rm_tsg_get_info_raw(int fd, NvHandle h_client,
			       NvHandle h_channel_group, NvU32 *tsg_id_out);
int nvidia_rm_tsg_set_interleave_level_raw(int fd, NvHandle h_client,
					   NvHandle h_channel_group,
					   NvU32 tsg_interleave_level);
int nvidia_rm_tsg_get_interleave_level_raw(int fd, NvHandle h_client,
					   NvHandle h_channel_group,
					   NvU32 *tsg_interleave_level_out);
int nvidia_rm_tsg_make_realtime_raw(int fd, NvHandle h_client,
				    NvHandle h_channel_group, NvBool realtime);
/* tick92: NV0080 FIFO device-level runlist/idle (target h_device) */
int nvidia_rm_fifo_stop_runlist_raw(int fd, NvHandle h_client, NvHandle h_device,
				    NvU32 engine_id);
int nvidia_rm_fifo_start_runlist_raw(int fd, NvHandle h_client, NvHandle h_device,
				     NvU32 engine_id);
int nvidia_rm_fifo_get_latency_buffer_size_raw(int fd, NvHandle h_client,
					       NvHandle h_device, NvU32 engine_id,
					       NvU32 *gp_entries_out,
					       NvU32 *pb_entries_out);
int nvidia_rm_fifo_get_caps_v2_raw(int fd, NvHandle h_client, NvHandle h_device,
				   NvU8 *caps_tbl_out, size_t caps_tbl_bytes);
int nvidia_rm_fifo_idle_channels_raw(int fd, NvHandle h_client, NvHandle h_device,
				     const NvHandle *h_channels, NvU32 num_channels,
				     NvU32 flags, NvU32 timeout_us);
int nvidia_rm_gpfifo_update_fault_method_buffer_raw(int fd, NvHandle h_client,
						    NvHandle h_channel,
						    NvU64 bar2_addr_rq0,
						    NvU64 bar2_addr_rq1);
int nvidia_rm_gpfifo_get_work_submit_token_raw(int fd, NvHandle h_client,
					       NvHandle h_channel,
					       NvU32 *token_out);
int nvidia_rm_gpfifo_set_work_submit_token_notif_index_raw(int fd,
							   NvHandle h_client,
							   NvHandle h_channel,
							   NvU32 index);
int nvidia_rm_vaspace_alloc_raw(int fd, NvHandle h_root, NvHandle h_device,
				NvHandle *h_vaspace_out,
				NvU32 index, NvU32 flags,
				NvU64 va_size, NvU64 va_base,
				NvU32 big_page_size,
				NvU64 *va_size_out, NvU64 *va_base_out);
int nvidia_rm_map_memory_dma_raw(int fd, NvHandle h_client, NvHandle h_device,
				 NvHandle h_dma, NvHandle h_memory,
				 NvU64 offset, NvU64 length, NvU32 flags,
				 NvU64 *dma_offset_inout);
int nvidia_rm_unmap_memory_dma_raw(int fd, NvHandle h_client, NvHandle h_device,
				   NvHandle h_dma, NvHandle h_memory,
				   NvU64 dma_offset, NvU64 size, NvU32 flags);
int nvidia_rm_usermode_alloc_raw(int fd, NvHandle h_root, NvHandle h_subdevice,
				 NvHandle *h_usermode_out, NvV32 *h_class_out);
void nvidia_rm_doorbell_ring(volatile void *usermode_map, NvU32 work_submit_token);
void nvidia_userd_init_host(volatile void *userd, size_t userd_bytes);
int nvidia_rm_context_dma_alloc_raw(int fd, NvHandle h_root, NvHandle h_parent,
				    NvHandle *h_ctxdma_out, NvV32 h_class,
				    NvHandle h_subdevice, NvHandle h_memory,
				    NvU64 offset, NvU64 limit, NvU32 flags);
int nvidia_rm_channel_group_alloc_raw(int fd, NvHandle h_root, NvHandle h_device,
				      NvHandle *h_group_out,
				      NvHandle h_object_error,
				      NvHandle h_vaspace,
				      NvU32 engine_type);
int nvidia_rm_ctxshare_alloc_raw(int fd, NvHandle h_root, NvHandle h_parent,
				 NvHandle *h_ctxshare_out,
				 NvHandle h_vaspace, NvU32 flags);
int nvidia_rm_channel_group_schedule_raw(int fd, NvHandle h_client,
					 NvHandle h_channel_group, NvBool enable);
void nvidia_gp_entry_pack(NvU32 entry[2], NvU64 gpu_addr, NvU32 length_dwords,
			  bool wait, bool priv);
void nvidia_gp_entry_pack_flags(NvU32 entry[2], NvU64 gpu_addr,
				NvU32 length_dwords, uint32_t flags);
bool nvidia_gpfifo_class_needs_doorbell(uint32_t gpfifo_class);
int nvidia_gpfifo_submit_one(uint32_t *gpfifo_cpu, uint32_t gpfifo_entries,
			     uint32_t *gpfifo_put_inout,
			     volatile void *userd,
			     uint64_t pb_gpu_addr, uint32_t pb_dwords,
			     volatile void *usermode_map,
			     uint32_t work_submit_token,
			     bool has_work_submit_token,
			     uint64_t stall_timeout_ns);
int nvidia_gpfifo_submit_one_ex(uint32_t *gpfifo_cpu, uint32_t gpfifo_entries,
				uint32_t *gpfifo_put_inout,
				volatile void *userd,
				uint64_t pb_gpu_addr, uint32_t pb_dwords,
				volatile void *usermode_map,
				uint32_t work_submit_token,
				bool has_work_submit_token,
				uint32_t gpfifo_class,
				uint64_t stall_timeout_ns);
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
int nvidia_userd_wait_gpfifo_idle(volatile void *userd, uint32_t target_put,
				  uint64_t timeout_ns);
int nvidia_userd_read_gpfifo(volatile void *userd,
			     uint32_t *get_out, uint32_t *put_out);
int nvidia_userd_snapshot(volatile void *userd, uint32_t *gp_get_out,
			  uint32_t *gp_put_out, uint32_t *pb_get_out,
			  uint32_t *pb_put_out);
uint32_t nvidia_gpfifo_ring_space(uint32_t gpfifo_entries,
				  uint32_t get_idx, uint32_t put_idx);
int nvidia_gpfifo_submit_many(uint32_t *gpfifo_cpu, uint32_t gpfifo_entries,
			      uint32_t *gpfifo_put_inout,
			      volatile void *userd,
			      const uint32_t *entries_data, uint32_t entry_count,
			      volatile void *usermode_map,
			      uint32_t work_submit_token,
			      bool has_work_submit_token,
			      uint64_t stall_timeout_ns);
int nvidia_notifier_status(volatile void *notifier,
			   uint16_t *status_out, uint32_t *info32_out);
int nvidia_notifier_wait(volatile void *notifier, bool clear_on_ok,
			 uint64_t timeout_ns);
void nvidia_notifier_reset(volatile void *notifier);
int nvidia_sema_wait_geq(volatile uint32_t *sema_cpu, uint32_t payload,
			 uint64_t timeout_ns);
bool nvidia_sema_signaled_geq(volatile uint32_t *sema_cpu, uint32_t payload);
int nvidia_submit_wait_complete(volatile void *userd, uint32_t target_put,
				volatile uint32_t *sema_cpu, uint32_t sema_payload,
				volatile void *notifier, uint64_t timeout_ns);
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
