/*
 * Copyright 2026 - Open NVIDIA userspace driver project
 * SPDX-License-Identifier: MIT
 *
 * RM ioctl wrappers. Parameter layouts and escape codes taken from
 * open-gpu-kernel-modules (nv-ioctl.h, nv_escape.h, escape.c, nvos.h).
 */

#include <errno.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "xf86drm.h"
#include "nvidia_internal.h"

int
nvidia_ioctl(int fd, int request, void *arg)
{
	int ret;

	do {
		ret = ioctl(fd, request, arg);
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN));

	return ret;
}

/*
 * Large parameter structures use NV_ESC_IOCTL_XFER_CMD with nv_ioctl_xfer_t.
 * Smaller ones issue the escape number directly as the ioctl request number
 * with NV_IOCTL_MAGIC.
 */
int
nvidia_rm_ioctl_xfer(int fd, uint32_t cmd, void *ptr, uint32_t size)
{
	nv_ioctl_xfer_t xfer;
	int req;

	memset(&xfer, 0, sizeof(xfer));
	xfer.cmd = cmd;
	xfer.size = size;
	xfer.ptr = (NvU64)(uintptr_t)ptr;

	req = _IOC(_IOC_READ | _IOC_WRITE, NV_IOCTL_MAGIC, NV_ESC_IOCTL_XFER_CMD,
		   sizeof(xfer));
	return nvidia_ioctl(fd, req, &xfer);
}

static int
rm_ioctl_direct(int fd, uint32_t esc, void *ptr, uint32_t size)
{
	int req = _IOC(_IOC_READ | _IOC_WRITE, NV_IOCTL_MAGIC, esc, size);

	return nvidia_ioctl(fd, req, ptr);
}

/*
 * Helper: try direct ioctl first; if ENOTTY/EINVAL fall back to xfer form.
 * The kernel module accepts RM escapes both ways depending on size.
 */
static int
rm_ioctl_auto(int fd, uint32_t esc, void *ptr, uint32_t size)
{
	int ret;

	ret = rm_ioctl_direct(fd, esc, ptr, size);
	if (ret == 0)
		return 0;
	if (errno == ENOTTY || errno == EINVAL || errno == ENOSYS)
		return nvidia_rm_ioctl_xfer(fd, esc, ptr, size);
	return ret;
}

int
nvidia_rm_check_version(int fd_ctl, char *version_out, int version_len)
{
	nv_ioctl_rm_api_version_t ver;
	int req;
	int ret;

	memset(&ver, 0, sizeof(ver));
	ver.cmd = NV_RM_API_VERSION_CMD_QUERY;

	req = _IOC(_IOC_READ | _IOC_WRITE, NV_IOCTL_MAGIC, NV_ESC_CHECK_VERSION_STR,
		   sizeof(ver));
	ret = nvidia_ioctl(fd_ctl, req, &ver);
	if (ret != 0)
		return -errno;

	if (version_out && version_len > 0) {
		strncpy(version_out, ver.versionString, version_len - 1);
		version_out[version_len - 1] = '\0';
	}
	return 0;
}

int
nvidia_rm_card_info(int fd_ctl, nv_ioctl_card_info_t *cards, int max_cards,
		    int *count_out)
{
	/* Kernel fills an array of NV_MAX_DEVICES entries */
	nv_ioctl_card_info_t buf[NV_MAX_DEVICES];
	int req;
	int ret;
	int i, count = 0;
	int n;

	memset(buf, 0, sizeof(buf));
	n = (max_cards < NV_MAX_DEVICES) ? max_cards : NV_MAX_DEVICES;

	req = _IOC(_IOC_READ | _IOC_WRITE, NV_IOCTL_MAGIC, NV_ESC_CARD_INFO,
		   sizeof(buf));
	ret = nvidia_ioctl(fd_ctl, req, buf);
	if (ret != 0)
		return -errno;

	for (i = 0; i < NV_MAX_DEVICES; i++) {
		if (!buf[i].valid)
			continue;
		if (count < n)
			cards[count] = buf[i];
		count++;
	}
	if (count_out)
		*count_out = count;
	return 0;
}

int
nvidia_rm_sys_params(int fd_ctl, nv_ioctl_sys_params_t *params)
{
	int req;
	int ret;

	memset(params, 0, sizeof(*params));
	req = _IOC(_IOC_READ | _IOC_WRITE, NV_IOCTL_MAGIC, NV_ESC_SYS_PARAMS,
		   sizeof(*params));
	ret = nvidia_ioctl(fd_ctl, req, params);
	if (ret != 0)
		return -errno;
	return 0;
}

int
nvidia_rm_register_fd(int fd_gpu, int fd_ctl)
{
	nv_ioctl_register_fd_t reg;
	int req;
	int ret;

	memset(&reg, 0, sizeof(reg));
	reg.ctl_fd = fd_ctl;
	req = _IOC(_IOC_READ | _IOC_WRITE, NV_IOCTL_MAGIC, NV_ESC_REGISTER_FD,
		   sizeof(reg));
	ret = nvidia_ioctl(fd_gpu, req, &reg);
	if (ret != 0)
		return -errno;
	return 0;
}

int
nvidia_rm_alloc_raw(int fd, NvHandle h_root, NvHandle h_parent,
		    NvHandle *h_new, NvV32 h_class,
		    void *alloc_parms, uint32_t alloc_parms_size)
{
	/*
	 * Prefer NVOS64 (NV_ESC_RM_ALLOC) which supports rights + flags + paramsSize.
	 * Kernel copies alloc_parms from userspace via pAllocParms/paramsSize.
	 * Fall back to NVOS21 (also with paramsSize) on older modules.
	 */
	NVOS64_PARAMETERS p64;
	NVOS21_PARAMETERS p21;
	int ret;

	memset(&p64, 0, sizeof(p64));
	p64.hRoot = h_root;
	p64.hObjectParent = h_parent;
	p64.hObjectNew = h_new ? *h_new : 0;
	p64.hClass = h_class;
	p64.pAllocParms = alloc_parms ? (NvU64)(uintptr_t)alloc_parms : 0;
	p64.pRightsRequested = 0;
	p64.paramsSize = alloc_parms_size;
	p64.flags = 0;
	p64.status = NV_ERR_GENERIC;

	ret = rm_ioctl_auto(fd, NV_ESC_RM_ALLOC, &p64, sizeof(p64));
	if (ret == 0 && p64.status == NV_OK) {
		if (h_new)
			*h_new = p64.hObjectNew;
		return 0;
	}

	/* Fallback: NVOS21 layout (also carries paramsSize in current nvos.h) */
	memset(&p21, 0, sizeof(p21));
	p21.hRoot = h_root;
	p21.hObjectParent = h_parent;
	p21.hObjectNew = h_new ? *h_new : 0;
	p21.hClass = h_class;
	p21.pAllocParms = alloc_parms ? (NvU64)(uintptr_t)alloc_parms : 0;
	p21.paramsSize = alloc_parms_size;
	p21.status = NV_ERR_GENERIC;

	ret = rm_ioctl_auto(fd, NV_ESC_RM_ALLOC, &p21, sizeof(p21));
	if (ret != 0)
		return -errno;
	if (p21.status != NV_OK)
		return -(int)p21.status;
	if (h_new)
		*h_new = p21.hObjectNew;
	return 0;
}

int
nvidia_rm_free_raw(int fd, NvHandle h_root, NvHandle h_parent, NvHandle h_object)
{
	NVOS00_PARAMETERS p;
	int ret;

	memset(&p, 0, sizeof(p));
	p.hRoot = h_root;
	p.hObjectParent = h_parent;
	p.hObjectOld = h_object;
	p.status = NV_ERR_GENERIC;

	ret = rm_ioctl_auto(fd, NV_ESC_RM_FREE, &p, sizeof(p));
	if (ret != 0)
		return -errno;
	if (p.status != NV_OK)
		return -(int)p.status;
	return 0;
}

/* tick93: NV_ESC_RM_DUP_OBJECT (NVOS55) */
int
nvidia_rm_dup_object_raw(int fd, NvHandle h_client_dst, NvHandle h_parent_dst,
			 NvHandle *h_object_dst_inout, NvHandle h_client_src,
			 NvHandle h_object_src, NvU32 flags)
{
	NVOS55_PARAMETERS p;
	int ret;

	memset(&p, 0, sizeof(p));
	p.hClient = h_client_dst;
	p.hParent = h_parent_dst;
	p.hObject = h_object_dst_inout ? *h_object_dst_inout : 0;
	p.hClientSrc = h_client_src;
	p.hObjectSrc = h_object_src;
	p.flags = flags;
	p.status = NV_ERR_GENERIC;

	ret = rm_ioctl_auto(fd, NV_ESC_RM_DUP_OBJECT, &p, sizeof(p));
	if (ret != 0)
		return -errno;
	if (p.status != NV_OK)
		return -(int)p.status;
	if (h_object_dst_inout)
		*h_object_dst_inout = p.hObject;
	return 0;
}

/* tick93: NV_ESC_RM_GET_EVENT_DATA (NVOS41 + NvUnixEvent buffer) */
int
nvidia_rm_get_event_data_raw(int fd, NvUnixEvent *event_out,
			     NvU32 *more_events_out)
{
	NVOS41_PARAMETERS p;
	NvUnixEvent ev;
	int ret;

	if (!event_out)
		return -EINVAL;
	memset(&ev, 0, sizeof(ev));
	memset(&p, 0, sizeof(p));
	p.pEvent = (NvU64)(uintptr_t)&ev;
	p.MoreEvents = 0;
	p.status = NV_ERR_GENERIC;

	ret = rm_ioctl_auto(fd, NV_ESC_RM_GET_EVENT_DATA, &p, sizeof(p));
	if (ret != 0)
		return -errno;
	if (p.status != NV_OK)
		return -(int)p.status;
	*event_out = ev;
	if (more_events_out)
		*more_events_out = p.MoreEvents;
	return 0;
}

/*
 * tick93: RmAlloc(NV01_EVENT_OS_EVENT) with NV0005_ALLOC_PARAMETERS.
 * data = OS event fd pointer; hSrcResource = object that generates events.
 */
int
nvidia_rm_alloc_os_event_object_raw(int fd, NvHandle h_client,
				    NvHandle h_parent, NvHandle h_src_resource,
				    NvHandle *h_event_out, NvV32 notify_index,
				    int os_event_fd)
{
	NV0005_ALLOC_PARAMETERS ap;
	NvHandle h_new = 0;
	int ret;

	if (!h_event_out || os_event_fd < 0)
		return -EINVAL;
	memset(&ap, 0, sizeof(ap));
	ap.hParentClient = h_client;
	ap.hSrcResource = h_src_resource;
	ap.hClass = NV01_EVENT_OS_EVENT;
	ap.notifyIndex = notify_index;
	/* Linux: pass fd as opaque pointer (proprietary driver convention) */
	ap.data = (NvU64)(uintptr_t)(intptr_t)os_event_fd;

	ret = nvidia_rm_alloc_raw(fd, h_client, h_parent, &h_new, NV01_EVENT,
				  &ap, sizeof(ap));
	if (ret != 0) {
		/* Fallback: try class NV01_EVENT_OS_EVENT as hClass directly */
		h_new = 0;
		ret = nvidia_rm_alloc_raw(fd, h_client, h_parent, &h_new,
					  NV01_EVENT_OS_EVENT, &ap, sizeof(ap));
	}
	if (ret != 0)
		return ret;
	*h_event_out = h_new;
	return 0;
}

int
nvidia_rm_event_set_notification_raw(int fd, NvHandle h_client,
				     NvHandle h_subdevice, NvU32 event,
				     NvU32 action, NvBool notify_state,
				     NvU32 info32, NvU16 info16)
{
	NV2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS params;

	memset(&params, 0, sizeof(params));
	params.event = event;
	params.action = action;
	params.bNotifyState = notify_state;
	params.info32 = info32;
	params.info16 = info16;
	return nvidia_rm_control_raw(fd, h_client, h_subdevice,
				     NV2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
				     &params, sizeof(params));
}

int
nvidia_rm_control_raw(int fd, NvHandle h_client, NvHandle h_object,
		      NvV32 cmd, void *params, uint32_t params_size)
{
	NVOS54_PARAMETERS p;
	int ret;

	memset(&p, 0, sizeof(p));
	p.hClient = h_client;
	p.hObject = h_object;
	p.cmd = cmd;
	p.flags = 0;
	p.params = params ? (NvU64)(uintptr_t)params : 0;
	p.paramsSize = params_size;
	p.status = NV_ERR_GENERIC;

	ret = rm_ioctl_auto(fd, NV_ESC_RM_CONTROL, &p, sizeof(p));
	if (ret != 0)
		return -errno;
	if (p.status != NV_OK)
		return -(int)p.status;
	return 0;
}

int
nvidia_rm_map_memory_raw(int fd, NvHandle h_client, NvHandle h_device,
			 NvHandle h_memory, NvU64 offset, NvU64 length,
			 void **cpu_ptr, NvU32 flags)
{
	nv_ioctl_nvos33_parameters_with_fd wrap;
	int ret;

	memset(&wrap, 0, sizeof(wrap));
	wrap.params.hClient = h_client;
	wrap.params.hDevice = h_device;
	wrap.params.hMemory = h_memory;
	wrap.params.offset = offset;
	wrap.params.length = length;
	wrap.params.pLinearAddress = 0;
	wrap.params.status = NV_ERR_GENERIC;
	wrap.params.flags = flags;
	wrap.fd = -1;

	ret = rm_ioctl_auto(fd, NV_ESC_RM_MAP_MEMORY, &wrap, sizeof(wrap));
	if (ret != 0)
		return -errno;
	if (wrap.params.status != NV_OK)
		return -(int)wrap.params.status;

	if (cpu_ptr)
		*cpu_ptr = (void *)(uintptr_t)wrap.params.pLinearAddress;
	return 0;
}

int
nvidia_rm_unmap_memory_raw(int fd, NvHandle h_client, NvHandle h_device,
			   NvHandle h_memory, void *cpu_ptr, NvU32 flags)
{
	NVOS34_PARAMETERS p;
	int ret;

	memset(&p, 0, sizeof(p));
	p.hClient = h_client;
	p.hDevice = h_device;
	p.hMemory = h_memory;
	p.pLinearAddress = (NvU64)(uintptr_t)cpu_ptr;
	p.status = NV_ERR_GENERIC;
	p.flags = flags;

	ret = rm_ioctl_auto(fd, NV_ESC_RM_UNMAP_MEMORY, &p, sizeof(p));
	if (ret != 0)
		return -errno;
	if (p.status != NV_OK)
		return -(int)p.status;
	return 0;
}

int
nvidia_rm_vidheap_alloc_raw(int fd, NvHandle h_root, NvHandle h_parent,
			    NvU32 type, NvU32 flags, NvU64 size, NvU64 align,
			    NvU32 attr, NvU32 attr2,
			    NvHandle *h_memory, NvU64 *offset, NvU64 *limit)
{
	/* Full NVOS32_PARAMETERS with AllocSize member (nvos.h layout). */
	NVOS32_PARAMETERS p;
	int ret;

	memset(&p, 0, sizeof(p));
	p.hRoot = h_root;
	p.hObjectParent = h_parent;
	p.function = NVOS32_FUNCTION_ALLOC_SIZE;
	p.status = NV_ERR_GENERIC;
	p.data.AllocSize.owner = h_root ? h_root : 0x10de;
	p.data.AllocSize.hMemory = h_memory ? *h_memory : 0;
	p.data.AllocSize.type = type ? type : NVOS32_TYPE_DMA;
	p.data.AllocSize.flags = flags | NVOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED |
				 NVOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED;
	p.data.AllocSize.attr = attr ? attr : NV_OS32_ATTR_VIDMEM_4K_UNCACHED;
	p.data.AllocSize.attr2 = attr2;
	p.data.AllocSize.size = size;
	p.data.AllocSize.alignment = align ? align : NVIDIA_DEFAULT_ALIGNMENT;

	ret = rm_ioctl_auto(fd, NV_ESC_RM_VID_HEAP_CONTROL, &p, sizeof(p));
	if (ret != 0)
		return -errno;
	if (p.status != NV_OK)
		return -(int)p.status;

	if (h_memory)
		*h_memory = p.data.AllocSize.hMemory;
	if (offset)
		*offset = p.data.AllocSize.offset;
	if (limit)
		*limit = p.data.AllocSize.limit;
	return 0;
}

int
nvidia_rm_vidheap_free_raw(int fd, NvHandle h_root, NvHandle h_parent,
			   NvHandle h_memory)
{
	NVOS32_PARAMETERS p;
	int ret;

	memset(&p, 0, sizeof(p));
	p.hRoot = h_root;
	p.hObjectParent = h_parent;
	p.function = NVOS32_FUNCTION_FREE;
	p.status = NV_ERR_GENERIC;
	p.data.Free.owner = h_root ? h_root : 0x10de;
	p.data.Free.hMemory = h_memory;
	p.data.Free.flags = NVOS32_FREE_FLAGS_MEMORY_HANDLE_PROVIDED;

	ret = rm_ioctl_auto(fd, NV_ESC_RM_VID_HEAP_CONTROL, &p, sizeof(p));
	if (ret != 0)
		return -errno;
	if (p.status != NV_OK)
		return -(int)p.status;
	return 0;
}

/*
 * Preferred memory allocation path: RmAlloc(NV01_MEMORY_LOCAL_USER /
 * NV01_MEMORY_SYSTEM) with NV_MEMORY_ALLOCATION_PARAMS.  This is what
 * nvidia-push / nvkms / the binary driver use; vidheap is the older path.
 */
int
nvidia_rm_memory_alloc_raw(int fd, NvHandle h_root, NvHandle h_parent,
			   NvHandle *h_memory, NvV32 h_class,
			   NvU32 owner, NvU32 type, NvU32 flags,
			   NvU32 attr, NvU32 attr2,
			   NvU64 size, NvU64 alignment,
			   NvU64 *offset_out, NvU64 *limit_out)
{
	NV_MEMORY_ALLOCATION_PARAMS mp;
	NvHandle h_mem;
	int ret;

	if (!h_memory || size == 0)
		return -EINVAL;

	memset(&mp, 0, sizeof(mp));
	mp.owner = owner ? owner : h_root;
	mp.type = type ? type : NVOS32_TYPE_DMA;
	mp.flags = flags | NVOS32_ALLOC_FLAGS_ALIGNMENT_FORCE |
		   NVOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED;
	mp.attr = attr;
	mp.attr2 = attr2;
	mp.size = size;
	mp.alignment = alignment ? alignment : NVIDIA_DEFAULT_ALIGNMENT;
	mp.numaNode = -1;

	h_mem = *h_memory;
	ret = nvidia_rm_alloc_raw(fd, h_root, h_parent, &h_mem, h_class,
				  &mp, sizeof(mp));
	if (ret != 0)
		return ret;

	*h_memory = h_mem;
	if (offset_out)
		*offset_out = mp.offset;
	if (limit_out)
		*limit_out = mp.limit ? mp.limit : (mp.size ? mp.size - 1 : 0);
	return 0;
}

int
nvidia_rm_alloc_os_event_raw(int fd_ctl, NvHandle h_client, NvHandle h_device,
			     int event_fd, NvU32 *status_out)
{
	nv_ioctl_alloc_os_event_t p;
	int req;
	int ret;

	memset(&p, 0, sizeof(p));
	p.hClient = h_client;
	p.hDevice = h_device;
	p.fd = (NvU32)event_fd;
	p.Status = NV_ERR_GENERIC;

	req = _IOC(_IOC_READ | _IOC_WRITE, NV_IOCTL_MAGIC, NV_ESC_ALLOC_OS_EVENT,
		   sizeof(p));
	ret = nvidia_ioctl(fd_ctl, req, &p);
	if (ret != 0)
		return -errno;
	if (status_out)
		*status_out = p.Status;
	if (p.Status != NV_OK)
		return -(int)p.Status;
	return 0;
}

int
nvidia_rm_free_os_event_raw(int fd_ctl, NvHandle h_client, NvHandle h_device,
			    int event_fd)
{
	nv_ioctl_free_os_event_t p;
	int req;
	int ret;

	memset(&p, 0, sizeof(p));
	p.hClient = h_client;
	p.hDevice = h_device;
	p.fd = (NvU32)event_fd;
	p.Status = NV_ERR_GENERIC;

	req = _IOC(_IOC_READ | _IOC_WRITE, NV_IOCTL_MAGIC, NV_ESC_FREE_OS_EVENT,
		   sizeof(p));
	ret = nvidia_ioctl(fd_ctl, req, &p);
	if (ret != 0)
		return -errno;
	if (p.Status != NV_OK)
		return -(int)p.Status;
	return 0;
}

int
nvidia_rm_wait_open_complete_raw(int fd_ctl, NvS32 *rc_out, NvU32 *adapter_status_out)
{
	nv_ioctl_wait_open_complete_t p;
	int req;
	int ret;

	memset(&p, 0, sizeof(p));
	req = _IOC(_IOC_READ | _IOC_WRITE, NV_IOCTL_MAGIC,
		   NV_ESC_WAIT_OPEN_COMPLETE, sizeof(p));
	ret = nvidia_ioctl(fd_ctl, req, &p);
	if (ret != 0)
		return -errno;
	if (rc_out)
		*rc_out = p.rc;
	if (adapter_status_out)
		*adapter_status_out = p.adapterStatus;
	return 0;
}

int
nvidia_rm_gpfifo_schedule_raw(int fd, NvHandle h_client, NvHandle h_channel,
			      NvBool enable)
{
	NVA06F_CTRL_GPFIFO_SCHEDULE_PARAMS params;

	memset(&params, 0, sizeof(params));
	params.bEnable = enable;
	params.bSkipSubmit = NV_FALSE;
	params.bSkipEnable = NV_FALSE;
	return nvidia_rm_control_raw(fd, h_client, h_channel,
				     NVA06F_CTRL_CMD_GPFIFO_SCHEDULE,
				     &params, sizeof(params));
}

/*
 * Pass9: NVA06F_CTRL_CMD_BIND (0xa06f0104) — engineType = NV2080_ENGINE_TYPE_*.
 * vdpau@345c8 / glcore@a52b69: always before SCHEDULE on graphics/video paths.
 * paramsSize = 4 (NvU32 only).
 */
int
nvidia_rm_gpfifo_bind_raw(int fd, NvHandle h_client, NvHandle h_channel,
			  NvU32 engine_type)
{
	NVA06F_CTRL_BIND_PARAMS params;

	memset(&params, 0, sizeof(params));
	params.engineType = engine_type;
	return nvidia_rm_control_raw(fd, h_client, h_channel,
				     NVA06F_CTRL_CMD_BIND,
				     &params, sizeof(params));
}

/* tick90: A06F recovery/priority (ctrla06fgpfifo.h; pass8 rare in graphics) */
int
nvidia_rm_gpfifo_set_interleave_level_raw(int fd, NvHandle h_client,
					  NvHandle h_channel,
					  NvU32 tsg_interleave_level)
{
	NVA06F_CTRL_INTERLEAVE_LEVEL_PARAMS params;

	memset(&params, 0, sizeof(params));
	params.tsgInterleaveLevel = tsg_interleave_level;
	return nvidia_rm_control_raw(fd, h_client, h_channel,
				     NVA06F_CTRL_CMD_SET_INTERLEAVE_LEVEL,
				     &params, sizeof(params));
}

int
nvidia_rm_gpfifo_get_interleave_level_raw(int fd, NvHandle h_client,
					  NvHandle h_channel,
					  NvU32 *tsg_interleave_level_out)
{
	NVA06F_CTRL_INTERLEAVE_LEVEL_PARAMS params;
	int ret;

	memset(&params, 0, sizeof(params));
	ret = nvidia_rm_control_raw(fd, h_client, h_channel,
				    NVA06F_CTRL_CMD_GET_INTERLEAVE_LEVEL,
				    &params, sizeof(params));
	if (ret == 0 && tsg_interleave_level_out)
		*tsg_interleave_level_out = params.tsgInterleaveLevel;
	return ret;
}

int
nvidia_rm_gpfifo_restart_runlist_raw(int fd, NvHandle h_client,
				     NvHandle h_channel,
				     NvBool bypass_wait_for_eng_idle)
{
	NVA06F_CTRL_RESTART_RUNLIST_PARAMS params;

	memset(&params, 0, sizeof(params));
	params.bBypassWaitForEngIdle = bypass_wait_for_eng_idle;
	return nvidia_rm_control_raw(fd, h_client, h_channel,
				     NVA06F_CTRL_CMD_RESTART_RUNLIST,
				     &params, sizeof(params));
}

int
nvidia_rm_gpfifo_stop_channel_raw(int fd, NvHandle h_client,
				  NvHandle h_channel,
				  NvBool in_preempt_timeout)
{
	NVA06F_CTRL_STOP_CHANNEL_PARAMS params;

	memset(&params, 0, sizeof(params));
	params.bInPreemptTimeout = in_preempt_timeout;
	return nvidia_rm_control_raw(fd, h_client, h_channel,
				     NVA06F_CTRL_CMD_STOP_CHANNEL,
				     &params, sizeof(params));
}

int
nvidia_rm_gpfifo_get_context_id_raw(int fd, NvHandle h_client,
				    NvHandle h_channel,
				    NvU32 *context_id_out)
{
	NVA06F_CTRL_GET_CONTEXT_ID_PARAMS params;
	int ret;

	memset(&params, 0, sizeof(params));
	ret = nvidia_rm_control_raw(fd, h_client, h_channel,
				    NVA06F_CTRL_CMD_GET_CONTEXT_ID,
				    &params, sizeof(params));
	if (ret == 0 && context_id_out)
		*context_id_out = params.contextId;
	return ret;
}

/* tick91: A06F SET_ERROR_NOTIFIER (0xa06f0108; pass8 cuda-primary in imm scan) */
int
nvidia_rm_gpfifo_set_error_notifier_raw(int fd, NvHandle h_client,
					NvHandle h_channel,
					NvBool notify_each_channel_in_tsg)
{
	NVA06F_CTRL_SET_ERROR_NOTIFIER_PARAMS params;

	memset(&params, 0, sizeof(params));
	params.bNotifyEachChannelInTSG = notify_each_channel_in_tsg;
	return nvidia_rm_control_raw(fd, h_client, h_channel,
				     NVA06F_CTRL_CMD_SET_ERROR_NOTIFIER,
				     &params, sizeof(params));
}

/* tick91: A06C TSG controls (ctrla06c.h; target = channel group handle) */
int
nvidia_rm_tsg_set_timeslice_raw(int fd, NvHandle h_client,
				NvHandle h_channel_group, NvU64 timeslice_us)
{
	NVA06C_CTRL_SET_TIMESLICE_PARAMS params;

	memset(&params, 0, sizeof(params));
	params.timesliceUs = timeslice_us;
	return nvidia_rm_control_raw(fd, h_client, h_channel_group,
				     NVA06C_CTRL_CMD_SET_TIMESLICE,
				     &params, sizeof(params));
}

int
nvidia_rm_tsg_get_timeslice_raw(int fd, NvHandle h_client,
				NvHandle h_channel_group,
				NvU64 *timeslice_us_out)
{
	NVA06C_CTRL_GET_TIMESLICE_PARAMS params;
	int ret;

	memset(&params, 0, sizeof(params));
	ret = nvidia_rm_control_raw(fd, h_client, h_channel_group,
				    NVA06C_CTRL_CMD_GET_TIMESLICE,
				    &params, sizeof(params));
	if (ret == 0 && timeslice_us_out)
		*timeslice_us_out = params.timesliceUs;
	return ret;
}

int
nvidia_rm_tsg_preempt_raw(int fd, NvHandle h_client, NvHandle h_channel_group,
			  NvBool wait, NvBool manual_timeout, NvU32 timeout_us)
{
	NVA06C_CTRL_PREEMPT_PARAMS params;

	memset(&params, 0, sizeof(params));
	params.bWait = wait;
	params.bManualTimeout = manual_timeout;
	params.timeoutUs = timeout_us;
	if (manual_timeout &&
	    params.timeoutUs > NVA06C_CTRL_CMD_PREEMPT_MAX_MANUAL_TIMEOUT_US)
		params.timeoutUs = NVA06C_CTRL_CMD_PREEMPT_MAX_MANUAL_TIMEOUT_US;
	return nvidia_rm_control_raw(fd, h_client, h_channel_group,
				     NVA06C_CTRL_CMD_PREEMPT,
				     &params, sizeof(params));
}

int
nvidia_rm_tsg_get_info_raw(int fd, NvHandle h_client, NvHandle h_channel_group,
			   NvU32 *tsg_id_out)
{
	NVA06C_CTRL_GET_INFO_PARAMS params;
	int ret;

	memset(&params, 0, sizeof(params));
	ret = nvidia_rm_control_raw(fd, h_client, h_channel_group,
				    NVA06C_CTRL_CMD_GET_INFO,
				    &params, sizeof(params));
	if (ret == 0 && tsg_id_out)
		*tsg_id_out = params.tsgID;
	return ret;
}

int
nvidia_rm_tsg_set_interleave_level_raw(int fd, NvHandle h_client,
				       NvHandle h_channel_group,
				       NvU32 tsg_interleave_level)
{
	NVA06C_CTRL_SET_INTERLEAVE_LEVEL_PARAMS params;

	memset(&params, 0, sizeof(params));
	params.tsgInterleaveLevel = tsg_interleave_level;
	return nvidia_rm_control_raw(fd, h_client, h_channel_group,
				     NVA06C_CTRL_CMD_SET_INTERLEAVE_LEVEL,
				     &params, sizeof(params));
}

int
nvidia_rm_tsg_get_interleave_level_raw(int fd, NvHandle h_client,
				       NvHandle h_channel_group,
				       NvU32 *tsg_interleave_level_out)
{
	NVA06C_CTRL_GET_INTERLEAVE_LEVEL_PARAMS params;
	int ret;

	memset(&params, 0, sizeof(params));
	ret = nvidia_rm_control_raw(fd, h_client, h_channel_group,
				    NVA06C_CTRL_CMD_GET_INTERLEAVE_LEVEL,
				    &params, sizeof(params));
	if (ret == 0 && tsg_interleave_level_out)
		*tsg_interleave_level_out = params.tsgInterleaveLevel;
	return ret;
}

int
nvidia_rm_tsg_make_realtime_raw(int fd, NvHandle h_client,
				NvHandle h_channel_group, NvBool realtime)
{
	NVA06C_CTRL_MAKE_REALTIME_PARAMS params;

	memset(&params, 0, sizeof(params));
	params.bRealtime = realtime;
	return nvidia_rm_control_raw(fd, h_client, h_channel_group,
				     NVA06C_CTRL_CMD_MAKE_REALTIME,
				     &params, sizeof(params));
}

/* tick92: NV0080 FIFO device-level (ctrl0080fifo.h; object = h_device) */
int
nvidia_rm_fifo_stop_runlist_raw(int fd, NvHandle h_client, NvHandle h_device,
				NvU32 engine_id)
{
	NV0080_CTRL_FIFO_STOP_RUNLIST_PARAMS params;

	memset(&params, 0, sizeof(params));
	params.engineID = engine_id;
	return nvidia_rm_control_raw(fd, h_client, h_device,
				     NV0080_CTRL_CMD_FIFO_STOP_RUNLIST,
				     &params, sizeof(params));
}

int
nvidia_rm_fifo_start_runlist_raw(int fd, NvHandle h_client, NvHandle h_device,
				 NvU32 engine_id)
{
	NV0080_CTRL_FIFO_START_RUNLIST_PARAMS params;

	memset(&params, 0, sizeof(params));
	params.engineID = engine_id;
	return nvidia_rm_control_raw(fd, h_client, h_device,
				     NV0080_CTRL_CMD_FIFO_START_RUNLIST,
				     &params, sizeof(params));
}

int
nvidia_rm_fifo_get_latency_buffer_size_raw(int fd, NvHandle h_client,
					   NvHandle h_device, NvU32 engine_id,
					   NvU32 *gp_entries_out,
					   NvU32 *pb_entries_out)
{
	NV0080_CTRL_FIFO_GET_LATENCY_BUFFER_SIZE_PARAMS params;
	int ret;

	memset(&params, 0, sizeof(params));
	params.engineID = engine_id;
	ret = nvidia_rm_control_raw(fd, h_client, h_device,
				    NV0080_CTRL_CMD_FIFO_GET_LATENCY_BUFFER_SIZE,
				    &params, sizeof(params));
	if (ret == 0) {
		if (gp_entries_out)
			*gp_entries_out = params.gpEntries;
		if (pb_entries_out)
			*pb_entries_out = params.pbEntries;
	}
	return ret;
}

int
nvidia_rm_fifo_get_caps_v2_raw(int fd, NvHandle h_client, NvHandle h_device,
			       NvU8 *caps_tbl_out, size_t caps_tbl_bytes)
{
	NV0080_CTRL_FIFO_GET_CAPS_V2_PARAMS params;
	int ret;

	memset(&params, 0, sizeof(params));
	ret = nvidia_rm_control_raw(fd, h_client, h_device,
				    NV0080_CTRL_CMD_FIFO_GET_CAPS_V2,
				    &params, sizeof(params));
	if (ret == 0 && caps_tbl_out && caps_tbl_bytes > 0) {
		size_t n = caps_tbl_bytes;
		if (n > NV0080_CTRL_FIFO_CAPS_TBL_SIZE)
			n = NV0080_CTRL_FIFO_CAPS_TBL_SIZE;
		memcpy(caps_tbl_out, params.capsTbl, n);
	}
	return ret;
}

/*
 * IDLE_CHANNELS uses a huge fixed array in RM params.  Allocate exact RM-sized
 * buffer, copy handles, issue control on h_device.
 */
int
nvidia_rm_fifo_idle_channels_raw(int fd, NvHandle h_client, NvHandle h_device,
				 const NvHandle *h_channels, NvU32 num_channels,
				 NvU32 flags, NvU32 timeout_us)
{
	/* Layout must match NV0080_CTRL_FIFO_IDLE_CHANNELS_PARAMS */
	struct {
		NvU32    numChannels;
		NvHandle hChannels[NV0080_CTRL_CMD_FIFO_IDLE_CHANNELS_MAX_CHANNELS];
		NvU32    flags;
		NvU32    timeout;
	} *params;
	size_t psz;
	int ret;
	NvU32 i;

	if (!h_channels || num_channels == 0)
		return -EINVAL;
	if (num_channels > NV0080_CTRL_CMD_FIFO_IDLE_CHANNELS_MAX_CHANNELS)
		return -EINVAL;
	if (num_channels > NV0080_CTRL_FIFO_IDLE_CHANNELS_HELPER_MAX)
		return -E2BIG;

	psz = sizeof(*params);
	params = calloc(1, psz);
	if (!params)
		return -ENOMEM;
	params->numChannels = num_channels;
	for (i = 0; i < num_channels; i++)
		params->hChannels[i] = h_channels[i];
	params->flags = flags;
	params->timeout = timeout_us;
	ret = nvidia_rm_control_raw(fd, h_client, h_device,
				    NV0080_CTRL_CMD_FIFO_IDLE_CHANNELS,
				    params, (NvU32)psz);
	free(params);
	return ret;
}

int
nvidia_rm_gpfifo_update_fault_method_buffer_raw(int fd, NvHandle h_client,
						NvHandle h_channel,
						NvU64 bar2_addr_rq0,
						NvU64 bar2_addr_rq1)
{
	NVC36F_CTRL_GPFIFO_UPDATE_FAULT_METHOD_BUFFER_PARAMS params;

	memset(&params, 0, sizeof(params));
	params.bar2Addr[0] = bar2_addr_rq0;
	params.bar2Addr[1] = bar2_addr_rq1;
	return nvidia_rm_control_raw(fd, h_client, h_channel,
				     NVC36F_CTRL_CMD_GPFIFO_UPDATE_FAULT_METHOD_BUFFER,
				     &params, sizeof(params));
}

int
nvidia_rm_gpfifo_get_work_submit_token_raw(int fd, NvHandle h_client,
					   NvHandle h_channel,
					   NvU32 *token_out)
{
	NVC36F_CTRL_CMD_GPFIFO_GET_WORK_SUBMIT_TOKEN_PARAMS params;
	int ret;

	memset(&params, 0, sizeof(params));
	ret = nvidia_rm_control_raw(fd, h_client, h_channel,
				    NVC36F_CTRL_CMD_GPFIFO_GET_WORK_SUBMIT_TOKEN,
				    &params, sizeof(params));
	if (ret == 0 && token_out)
		*token_out = params.workSubmitToken;
	return ret;
}

/*
 * Best-effort: set error-context notifier slot for work_submit_token
 * (ctrlc36f.h).  Default index NV_CHANNELGPFIFO_NOTIFICATION_TYPE_WORK_SUBMIT_TOKEN.
 * Non-fatal if RM rejects; GET_WORK_SUBMIT_TOKEN is still the primary path.
 */
int
nvidia_rm_gpfifo_set_work_submit_token_notif_index_raw(int fd, NvHandle h_client,
						       NvHandle h_channel,
						       NvU32 index)
{
	NVC36F_CTRL_GPFIFO_SET_WORK_SUBMIT_TOKEN_NOTIF_INDEX_PARAMS params;

	memset(&params, 0, sizeof(params));
	params.index = index;
	return nvidia_rm_control_raw(fd, h_client, h_channel,
				     NVC36F_CTRL_CMD_GPFIFO_SET_WORK_SUBMIT_TOKEN_NOTIF_INDEX,
				     &params, sizeof(params));
}

int
nvidia_rm_vaspace_alloc_raw(int fd, NvHandle h_root, NvHandle h_device,
			    NvHandle *h_vaspace_out,
			    NvU32 index, NvU32 flags,
			    NvU64 va_size, NvU64 va_base,
			    NvU32 big_page_size,
			    NvU64 *va_size_out, NvU64 *va_base_out)
{
	NV_VASPACE_ALLOCATION_PARAMETERS vp;
	NvHandle h_vas = 0;
	int ret;

	if (!h_vaspace_out)
		return -EINVAL;

	memset(&vp, 0, sizeof(vp));
	vp.index = index;
	vp.flags = flags;
	vp.vaSize = va_size;
	vp.vaBase = va_base;
	vp.bigPageSize = big_page_size;

	if (*h_vaspace_out)
		h_vas = *h_vaspace_out;

	ret = nvidia_rm_alloc_raw(fd, h_root, h_device, &h_vas,
				  FERMI_VASPACE_A, &vp, sizeof(vp));
	if (ret != 0)
		return ret;

	*h_vaspace_out = h_vas;
	if (va_size_out)
		*va_size_out = vp.vaSize;
	if (va_base_out)
		*va_base_out = vp.vaBase;
	return 0;
}

int
nvidia_rm_map_memory_dma_raw(int fd, NvHandle h_client, NvHandle h_device,
			     NvHandle h_dma, NvHandle h_memory,
			     NvU64 offset, NvU64 length, NvU32 flags,
			     NvU64 *dma_offset_inout)
{
	NVOS46_PARAMETERS p;
	int ret;

	memset(&p, 0, sizeof(p));
	p.hClient = h_client;
	p.hDevice = h_device;
	p.hDma = h_dma;
	p.hMemory = h_memory;
	p.offset = offset;
	p.length = length;
	p.flags = flags;
	p.flags2 = 0;
	p.kindOverride = 0;
	p.dmaOffset = dma_offset_inout ? *dma_offset_inout : 0;
	p.status = NV_ERR_GENERIC;

	ret = rm_ioctl_auto(fd, NV_ESC_RM_MAP_MEMORY_DMA, &p, sizeof(p));
	if (ret != 0)
		return -errno;
	if (p.status != NV_OK)
		return -(int)p.status;
	if (dma_offset_inout)
		*dma_offset_inout = p.dmaOffset;
	return 0;
}

int
nvidia_rm_unmap_memory_dma_raw(int fd, NvHandle h_client, NvHandle h_device,
			       NvHandle h_dma, NvHandle h_memory,
			       NvU64 dma_offset, NvU64 size, NvU32 flags)
{
	NVOS47_PARAMETERS p;
	int ret;

	memset(&p, 0, sizeof(p));
	p.hClient = h_client;
	p.hDevice = h_device;
	p.hDma = h_dma;
	p.hMemory = h_memory;
	p.flags = flags;
	p.dmaOffset = dma_offset;
	p.size = size;
	p.status = NV_ERR_GENERIC;

	ret = rm_ioctl_auto(fd, NV_ESC_RM_UNMAP_MEMORY_DMA, &p, sizeof(p));
	if (ret != 0)
		return -errno;
	if (p.status != NV_OK)
		return -(int)p.status;
	return 0;
}

int
nvidia_rm_usermode_alloc_raw(int fd, NvHandle h_root, NvHandle h_subdevice,
			     NvHandle *h_usermode_out, NvV32 *h_class_out)
{
	NvHandle h_um = 0;
	int ret;
	/* Newest-first (610.43.02 glcore/eglcore/glsi embed C761/C661/C361) */
	static const NvV32 classes[] = {
		BLACKWELL_USERMODE_A,
		HOPPER_USERMODE_A,
		VOLTA_USERMODE_A,
	};
	unsigned i;

	if (!h_usermode_out)
		return -EINVAL;

	if (*h_usermode_out)
		h_um = *h_usermode_out;

	for (i = 0; i < sizeof(classes) / sizeof(classes[0]); i++) {
		NV_HOPPER_USERMODE_A_PARAMS hp;
		void *parms = NULL;
		uint32_t parms_size = 0;

		/* Hopper+ alloc may take optional params; Volta/Blackwell try NULL first */
		if (classes[i] == HOPPER_USERMODE_A ||
		    classes[i] == BLACKWELL_USERMODE_A) {
			memset(&hp, 0, sizeof(hp));
			parms = &hp;
			parms_size = sizeof(hp);
		}

		h_um = *h_usermode_out ? *h_usermode_out : 0;
		ret = nvidia_rm_alloc_raw(fd, h_root, h_subdevice, &h_um,
					  classes[i], parms, parms_size);
		if (ret != 0 && (classes[i] == HOPPER_USERMODE_A ||
				 classes[i] == BLACKWELL_USERMODE_A)) {
			/* Retry without params block if RM rejects Hopper-style args */
			h_um = *h_usermode_out ? *h_usermode_out : 0;
			ret = nvidia_rm_alloc_raw(fd, h_root, h_subdevice, &h_um,
						  classes[i], NULL, 0);
		}
		if (ret == 0) {
			*h_usermode_out = h_um;
			if (h_class_out)
				*h_class_out = classes[i];
			return 0;
		}
	}
	return ret ? ret : -ENODEV;
}

void
nvidia_rm_doorbell_ring(volatile void *usermode_map, NvU32 work_submit_token)
{
	volatile NvU32 *doorbell;

	if (!usermode_map)
		return;
	/*
	 * NVC361_NOTIFY_CHANNEL_PENDING @ 0x90 (clc361.h / VOLTA_USERMODE_A).
	 * 610.43.02 glcore/eglcore embed class 0xC361/C661/C761; kick is write
	 * work_submit_token here after GPFIFO entry + USERD GPPut + sfence.
	 */
	doorbell = (volatile NvU32 *)((uint8_t *)usermode_map +
				      NVC361_NOTIFY_CHANNEL_PENDING);
	__sync_synchronize();
	*doorbell = work_submit_token;
	__sync_synchronize();
}

/*
 * Host-side USERD init before first submit: zero control block so GPGet/GPPut
 * and PB Put/Get start at 0 (RM may not clear sysmem USERD on alloc).
 */
void
nvidia_userd_init_host(volatile void *userd, size_t userd_bytes)
{
	volatile nvidia_userd_control_t *ud;

	if (!userd || userd_bytes < sizeof(nvidia_userd_control_t))
		return;
	memset((void *)userd, 0, userd_bytes);
	ud = (volatile nvidia_userd_control_t *)userd;
	ud->GPGet = 0;
	ud->GPPut = 0;
	ud->Put = 0;
	ud->Get = 0;
	__sync_synchronize();
}

int
nvidia_rm_context_dma_alloc_raw(int fd, NvHandle h_root, NvHandle h_parent,
				NvHandle *h_ctxdma_out, NvV32 h_class,
				NvHandle h_subdevice, NvHandle h_memory,
				NvU64 offset, NvU64 limit, NvU32 flags)
{
	NV_CONTEXT_DMA_ALLOCATION_PARAMS cp;
	NvHandle h_cd = 0;
	int ret;

	if (!h_ctxdma_out || !h_memory)
		return -EINVAL;

	memset(&cp, 0, sizeof(cp));
	cp.hSubDevice = h_subdevice;
	cp.flags = flags;
	cp.hMemory = h_memory;
	cp.offset = offset;
	cp.limit = limit;

	if (*h_ctxdma_out)
		h_cd = *h_ctxdma_out;

	ret = nvidia_rm_alloc_raw(fd, h_root, h_parent, &h_cd,
				  h_class ? h_class : NV01_CONTEXT_ERROR_TO_MEMORY,
				  &cp, sizeof(cp));
	if (ret != 0)
		return ret;
	*h_ctxdma_out = h_cd;
	return 0;
}

int
nvidia_rm_channel_group_alloc_raw(int fd, NvHandle h_root, NvHandle h_device,
				  NvHandle *h_group_out,
				  NvHandle h_object_error,
				  NvHandle h_vaspace,
				  NvU32 engine_type)
{
	NV_CHANNEL_GROUP_ALLOCATION_PARAMETERS gp;
	NvHandle h_grp = 0;
	int ret;

	if (!h_group_out)
		return -EINVAL;

	memset(&gp, 0, sizeof(gp));
	gp.hObjectError = h_object_error;
	gp.hObjectEccError = 0;
	gp.hVASpace = h_vaspace;
	gp.engineType = engine_type;
	gp.bIsCallingContextVgpuPlugin = NV_FALSE;

	if (*h_group_out)
		h_grp = *h_group_out;

	ret = nvidia_rm_alloc_raw(fd, h_root, h_device, &h_grp,
				  KEPLER_CHANNEL_GROUP_A, &gp, sizeof(gp));
	if (ret != 0)
		return ret;
	*h_group_out = h_grp;
	return 0;
}

int
nvidia_rm_ctxshare_alloc_raw(int fd, NvHandle h_root, NvHandle h_parent,
			     NvHandle *h_ctxshare_out,
			     NvHandle h_vaspace, NvU32 flags)
{
	NV_CTXSHARE_ALLOCATION_PARAMETERS cp;
	NvHandle h_cs = 0;
	int ret;

	if (!h_ctxshare_out)
		return -EINVAL;

	memset(&cp, 0, sizeof(cp));
	cp.hVASpace = h_vaspace;
	cp.flags = flags;
	cp.subctxId = 0;

	if (*h_ctxshare_out)
		h_cs = *h_ctxshare_out;

	ret = nvidia_rm_alloc_raw(fd, h_root, h_parent, &h_cs,
				  FERMI_CONTEXT_SHARE_A, &cp, sizeof(cp));
	if (ret != 0)
		return ret;
	*h_ctxshare_out = h_cs;
	return 0;
}

int
nvidia_rm_channel_group_schedule_raw(int fd, NvHandle h_client,
				     NvHandle h_channel_group, NvBool enable)
{
	NVA06C_CTRL_GPFIFO_SCHEDULE_PARAMS params;

	memset(&params, 0, sizeof(params));
	params.bEnable = enable;
	params.bSkipSubmit = NV_FALSE;
	return nvidia_rm_control_raw(fd, h_client, h_channel_group,
				     NVA06C_CTRL_CMD_GPFIFO_SCHEDULE,
				     &params, sizeof(params));
}

void
nvidia_gp_entry_pack(NvU32 entry[2], NvU64 gpu_addr, NvU32 length_dwords,
		     bool wait, bool priv)
{
	/*
	 * NVC36F_GP_ENTRY: word0 GET[31:2] = pb VA >> 2; word1 GET_HI + length +
	 * PRIV/LEVEL/SYNC.  'wait' maps to SYNC_WAIT (bit 31), not LEVEL_SUBROUTINE
	 * (bit 9) — older code conflated the two and could mis-encode wait segments.
	 * Length is 21 bits at [30:10]; clamp so overflow cannot spill into SYNC bit.
	 */
	NvU32 len = length_dwords & NV_GP_ENTRY1_LENGTH_MASK;

	entry[0] = (NvU32)((gpu_addr >> NV_GP_ENTRY0_GET_SHIFT) << NV_GP_ENTRY0_GET_SHIFT);
	entry[1] = ((NvU32)(gpu_addr >> 32) & NV_GP_ENTRY1_GET_HI_MASK) |
		   (len << NV_GP_ENTRY1_LENGTH_SHIFT);
	if (priv)
		entry[1] |= (1u << NV_GP_ENTRY1_PRIV_SHIFT);
	if (wait)
		entry[1] |= (1u << NV_GP_ENTRY1_SYNC_SHIFT);
}

void
nvidia_gp_entry_pack_flags(NvU32 entry[2], NvU64 gpu_addr, NvU32 length_dwords,
			   uint32_t flags)
{
	NvU32 len = length_dwords & NV_GP_ENTRY1_LENGTH_MASK;

	entry[0] = (NvU32)((gpu_addr >> NV_GP_ENTRY0_GET_SHIFT) << NV_GP_ENTRY0_GET_SHIFT);
	entry[1] = ((NvU32)(gpu_addr >> 32) & NV_GP_ENTRY1_GET_HI_MASK) |
		   (len << NV_GP_ENTRY1_LENGTH_SHIFT);
	if (flags & NV_GP_ENTRY_F_PRIV)
		entry[1] |= (1u << NV_GP_ENTRY1_PRIV_SHIFT);
	if (flags & NV_GP_ENTRY_F_LEVEL_SUBR)
		entry[1] |= (1u << NV_GP_ENTRY1_LEVEL_SHIFT);
	if (flags & NV_GP_ENTRY_F_SYNC_WAIT)
		entry[1] |= (1u << NV_GP_ENTRY1_SYNC_SHIFT);
}

/*
 * True if channel GPFIFO class should use usermode doorbell after GPPut.
 * 610.43.02 glcore@ac5557: doorbell only when class > 0xC36E (i.e. >= C36F).
 * gpfifo_class==0 means unknown — allow doorbell (caller has token+map).
 */
bool
nvidia_gpfifo_class_needs_doorbell(uint32_t gpfifo_class)
{
	if (!gpfifo_class)
		return true;
	return gpfifo_class >= NV_GP_DOORBELL_MIN_CLASS;
}

/*
 * Host-side GPFIFO ring submit: mirrors mesa nv_channel_kickoff and 610.43.02
 * glcore@ac5540 kick order:
 *   1) write ring entry (pb VA + length@bits30:10)
 *   2) USERD.GPPut @ +0x8c  (all mapped USERDs — multi via submit_one_multi)
 *   3) if class > C36E and token+map: sfence; usermode+0x90 = work_submit_token
 *
 * gpfifo_class==0: unknown class — ring doorbell whenever token+map provided.
 * gpfifo_class<=0xC36E: GPPut-only (legacy path; no doorbell).
 *
 * Stall/full-ring check uses the first USERD (userd_maps[0] / userd).
 */
int
nvidia_gpfifo_submit_one_multi(uint32_t *gpfifo_cpu, uint32_t gpfifo_entries,
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
			       uint64_t stall_timeout_ns)
{
	volatile nvidia_userd_control_t *ud_primary;
	volatile void *first_userd = NULL;
	uint32_t put_idx, next_put;
	uint32_t entry[2];
	struct timespec ts;
	uint64_t start_ns = 0, now_ns, deadline_ns;
	bool ring_doorbell;
	unsigned i, n_userd, n_um;

	if (!gpfifo_cpu || !gpfifo_put_inout || !userd_maps || !userd_count ||
	    !gpfifo_entries || !pb_dwords)
		return -EINVAL;
	if (pb_dwords > NV_GP_ENTRY1_LENGTH_MASK)
		return -EINVAL;

	n_userd = userd_count;
	if (n_userd > NV_GP_MAX_USERD_SLOTS)
		n_userd = NV_GP_MAX_USERD_SLOTS;

	for (i = 0; i < n_userd; i++) {
		if (userd_maps[i]) {
			first_userd = userd_maps[i];
			break;
		}
	}
	if (!first_userd)
		return -EINVAL;

	ud_primary = (volatile nvidia_userd_control_t *)first_userd;
	put_idx = *gpfifo_put_inout % gpfifo_entries;
	next_put = (put_idx + 1) % gpfifo_entries;

	if (stall_timeout_ns) {
		if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
			start_ns = (uint64_t)ts.tv_sec * 1000000000ull +
				   (uint64_t)ts.tv_nsec;
		deadline_ns = start_ns + stall_timeout_ns;
	} else {
		deadline_ns = 0;
	}

	/* Ring full when GPU has not consumed the slot we would overwrite */
	while (ud_primary->GPGet == next_put) {
		if (!stall_timeout_ns)
			return -EAGAIN;
		if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
			return -ETIMEDOUT;
		now_ns = (uint64_t)ts.tv_sec * 1000000000ull +
			 (uint64_t)ts.tv_nsec;
		if (now_ns >= deadline_ns)
			return -ETIMEDOUT;
	}

	/* 1) GPFIFO ring entry (host-mapped; GPU reads via ring GPU VA) */
	nvidia_gp_entry_pack(entry, pb_gpu_addr, pb_dwords, false, false);
	gpfifo_cpu[put_idx * 2 + 0] = entry[0];
	gpfifo_cpu[put_idx * 2 + 1] = entry[1];
	/* Entry stores visible before GPPut (ac5540 does not sfence before GPPut,
	 * but WC USERD/ring maps need host ordering; harmless extra barrier). */
	__sync_synchronize();
#if defined(__x86_64__) || defined(__i386__)
	__asm__ __volatile__("sfence" ::: "memory");
#endif

	*gpfifo_put_inout = next_put;

	/* 2) Publish GPPut @ USERD+0x8c on every mapped USERD (glcore ac5540 loop) */
	__sync_synchronize();
	for (i = 0; i < n_userd; i++) {
		volatile nvidia_userd_control_t *ud;

		if (!userd_maps[i])
			continue;
		ud = (volatile nvidia_userd_control_t *)userd_maps[i];
		ud->GPPut = next_put;
	}

	/* 3) Doorbell only for Turing+ GPFIFO (class > C36E) with token+usermode */
	ring_doorbell = has_work_submit_token &&
			nvidia_gpfifo_class_needs_doorbell(gpfifo_class);
	if (!ring_doorbell)
		return 0;

	/* Match ac5585: sfence after all GPPut stores, before usermode+0x90 */
	__sync_synchronize();
#if defined(__x86_64__) || defined(__i386__)
	__asm__ __volatile__("sfence" ::: "memory");
#endif

	n_um = usermode_count;
	if (n_um > NV_GP_MAX_USERD_SLOTS)
		n_um = NV_GP_MAX_USERD_SLOTS;

	if (usermode_maps && n_um > 0) {
		unsigned rang = 0;

		for (i = 0; i < n_um; i++) {
			if (!usermode_maps[i])
				continue;
			nvidia_rm_doorbell_ring(usermode_maps[i],
						work_submit_token);
			rang++;
		}
		/* Fall back to single map if multi array had only NULLs */
		if (!rang && usermode_map)
			nvidia_rm_doorbell_ring(usermode_map, work_submit_token);
	} else if (usermode_map) {
		nvidia_rm_doorbell_ring(usermode_map, work_submit_token);
	}

	return 0;
}

int
nvidia_gpfifo_submit_one_ex(uint32_t *gpfifo_cpu, uint32_t gpfifo_entries,
			    uint32_t *gpfifo_put_inout,
			    volatile void *userd,
			    uint64_t pb_gpu_addr, uint32_t pb_dwords,
			    volatile void *usermode_map,
			    uint32_t work_submit_token,
			    bool has_work_submit_token,
			    uint32_t gpfifo_class,
			    uint64_t stall_timeout_ns)
{
	volatile void *maps[1];

	maps[0] = userd;
	return nvidia_gpfifo_submit_one_multi(gpfifo_cpu, gpfifo_entries,
					      gpfifo_put_inout, maps, 1,
					      pb_gpu_addr, pb_dwords,
					      usermode_map, NULL, 0,
					      work_submit_token,
					      has_work_submit_token,
					      gpfifo_class, stall_timeout_ns);
}

int
nvidia_gpfifo_submit_one(uint32_t *gpfifo_cpu, uint32_t gpfifo_entries,
			 uint32_t *gpfifo_put_inout,
			 volatile void *userd,
			 uint64_t pb_gpu_addr, uint32_t pb_dwords,
			 volatile void *usermode_map,
			 uint32_t work_submit_token,
			 bool has_work_submit_token,
			 uint64_t stall_timeout_ns)
{
	/* class=0: allow doorbell whenever token+map (backward compatible) */
	return nvidia_gpfifo_submit_one_ex(gpfifo_cpu, gpfifo_entries,
					   gpfifo_put_inout, userd,
					   pb_gpu_addr, pb_dwords,
					   usermode_map, work_submit_token,
					   has_work_submit_token,
					   0 /* unknown class */,
					   stall_timeout_ns);
}

int
nvidia_userd_wait_gpfifo_idle(volatile void *userd, uint32_t target_put,
			      uint64_t timeout_ns)
{
	volatile nvidia_userd_control_t *ud;
	struct timespec ts;
	uint64_t start_ns = 0, now_ns, deadline_ns;

	if (!userd)
		return -EINVAL;

	ud = (volatile nvidia_userd_control_t *)userd;

	if (timeout_ns) {
		if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
			start_ns = (uint64_t)ts.tv_sec * 1000000000ull +
				   (uint64_t)ts.tv_nsec;
		deadline_ns = start_ns + timeout_ns;
	} else {
		deadline_ns = 0;
	}

	for (;;) {
		if (ud->GPGet == target_put)
			return 0;
		if (!timeout_ns)
			return -EAGAIN;
		if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
			return -ETIMEDOUT;
		now_ns = (uint64_t)ts.tv_sec * 1000000000ull +
			 (uint64_t)ts.tv_nsec;
		if (now_ns >= deadline_ns)
			return -ETIMEDOUT;
	}
}

int
nvidia_userd_read_gpfifo(volatile void *userd,
			 uint32_t *get_out, uint32_t *put_out)
{
	volatile nvidia_userd_control_t *ud;

	if (!userd)
		return -EINVAL;
	ud = (volatile nvidia_userd_control_t *)userd;
	if (get_out)
		*get_out = ud->GPGet;
	if (put_out)
		*put_out = ud->GPPut;
	return 0;
}

/*
 * Snapshot USERD ring pointers for bring-up logs (GPGet/GPPut + PB Put/Get).
 * Returns 0; leaves outs zero if userd is NULL.
 */
int
nvidia_userd_snapshot(volatile void *userd, uint32_t *gp_get_out,
		      uint32_t *gp_put_out, uint32_t *pb_get_out,
		      uint32_t *pb_put_out)
{
	volatile nvidia_userd_control_t *ud;

	if (gp_get_out)
		*gp_get_out = 0;
	if (gp_put_out)
		*gp_put_out = 0;
	if (pb_get_out)
		*pb_get_out = 0;
	if (pb_put_out)
		*pb_put_out = 0;
	if (!userd)
		return -EINVAL;
	ud = (volatile nvidia_userd_control_t *)userd;
	if (gp_get_out)
		*gp_get_out = ud->GPGet;
	if (gp_put_out)
		*gp_put_out = ud->GPPut;
	if (pb_get_out)
		*pb_get_out = ud->Get;
	if (pb_put_out)
		*pb_put_out = ud->Put;
	return 0;
}

uint32_t
nvidia_gpfifo_ring_space(uint32_t gpfifo_entries,
			 uint32_t get_idx, uint32_t put_idx)
{
	uint32_t used;

	if (!gpfifo_entries)
		return 0;
	get_idx %= gpfifo_entries;
	put_idx %= gpfifo_entries;
	if (put_idx >= get_idx)
		used = put_idx - get_idx;
	else
		used = gpfifo_entries - get_idx + put_idx;
	/* One slot reserved: ring full when put+1 == get */
	if (used >= gpfifo_entries - 1)
		return 0;
	return gpfifo_entries - 1 - used;
}

int
nvidia_gpfifo_submit_many(uint32_t *gpfifo_cpu, uint32_t gpfifo_entries,
			  uint32_t *gpfifo_put_inout,
			  volatile void *userd,
			  const uint32_t *entries_data, uint32_t entry_count,
			  volatile void *usermode_map,
			  uint32_t work_submit_token,
			  bool has_work_submit_token,
			  uint64_t stall_timeout_ns)
{
	volatile nvidia_userd_control_t *ud;
	uint32_t i, put_idx, next_put;
	struct timespec ts;
	uint64_t start_ns = 0, now_ns, deadline_ns;
	bool ring_once = false;

	if (!gpfifo_cpu || !gpfifo_put_inout || !userd || !gpfifo_entries ||
	    !entries_data || !entry_count)
		return -EINVAL;

	ud = (volatile nvidia_userd_control_t *)userd;

	if (stall_timeout_ns) {
		if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
			start_ns = (uint64_t)ts.tv_sec * 1000000000ull +
				   (uint64_t)ts.tv_nsec;
		deadline_ns = start_ns + stall_timeout_ns;
	} else {
		deadline_ns = 0;
	}

	for (i = 0; i < entry_count; i++) {
		put_idx = *gpfifo_put_inout % gpfifo_entries;
		next_put = (put_idx + 1) % gpfifo_entries;

		while (ud->GPGet == next_put) {
			if (!stall_timeout_ns)
				return -EAGAIN;
			if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
				return -ETIMEDOUT;
			now_ns = (uint64_t)ts.tv_sec * 1000000000ull +
				 (uint64_t)ts.tv_nsec;
			if (now_ns >= deadline_ns)
				return -ETIMEDOUT;
		}

		gpfifo_cpu[put_idx * 2 + 0] = entries_data[i * 2 + 0];
		gpfifo_cpu[put_idx * 2 + 1] = entries_data[i * 2 + 1];
		__sync_synchronize();

		*gpfifo_put_inout = next_put;
		ud->GPPut = next_put;
		__sync_synchronize();
		ring_once = true;
	}

	if (ring_once && has_work_submit_token && usermode_map)
		nvidia_rm_doorbell_ring(usermode_map, work_submit_token);

	return 0;
}

int
nvidia_notifier_status(volatile void *notifier,
		       uint16_t *status_out, uint32_t *info32_out)
{
	volatile nvidia_notification_t *n;

	if (!notifier)
		return -EINVAL;
	n = (volatile nvidia_notification_t *)notifier;
	if (status_out)
		*status_out = n->status;
	if (info32_out)
		*info32_out = n->info32;
	if (n->status == NVIDIA_NOTIFICATION_STATUS_IN_PROGRESS)
		return -EAGAIN;
	if (n->status != NVIDIA_NOTIFICATION_STATUS_DONE_SUCCESS &&
	    n->status != 0)
		return -EIO;
	return 0;
}

void
nvidia_notifier_reset(volatile void *notifier)
{
	volatile nvidia_notification_t *n;

	if (!notifier)
		return;
	n = (volatile nvidia_notification_t *)notifier;
	n->status = NVIDIA_NOTIFICATION_STATUS_DONE_SUCCESS;
	n->info32 = 0;
	n->info16 = 0;
	__sync_synchronize();
}

int
nvidia_notifier_wait(volatile void *notifier, bool clear_on_ok,
		     uint64_t timeout_ns)
{
	volatile nvidia_notification_t *n;
	struct timespec ts;
	uint64_t start_ns = 0, now_ns, deadline_ns;
	int r;

	if (!notifier)
		return -EINVAL;
	n = (volatile nvidia_notification_t *)notifier;

	if (timeout_ns) {
		if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
			start_ns = (uint64_t)ts.tv_sec * 1000000000ull +
				   (uint64_t)ts.tv_nsec;
		deadline_ns = start_ns + timeout_ns;
	} else {
		deadline_ns = 0;
	}

	for (;;) {
		r = nvidia_notifier_status(notifier, NULL, NULL);
		if (r == 0) {
			if (clear_on_ok)
				nvidia_notifier_reset(notifier);
			return 0;
		}
		if (r == -EIO)
			return -EIO;
		if (!timeout_ns)
			return r;
		if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
			return -ETIMEDOUT;
		now_ns = (uint64_t)ts.tv_sec * 1000000000ull +
			 (uint64_t)ts.tv_nsec;
		if (now_ns >= deadline_ns)
			return -ETIMEDOUT;
	}
}

bool
nvidia_sema_signaled_geq(volatile uint32_t *sema_cpu, uint32_t payload)
{
	if (!sema_cpu || !payload)
		return true;
	return sema_cpu[0] >= payload;
}

int
nvidia_sema_wait_geq(volatile uint32_t *sema_cpu, uint32_t payload,
		     uint64_t timeout_ns)
{
	struct timespec ts;
	uint64_t start_ns = 0, now_ns, deadline_ns;

	if (!sema_cpu)
		return -EINVAL;
	if (!payload)
		return 0;

	if (nvidia_sema_signaled_geq(sema_cpu, payload))
		return 0;

	if (!timeout_ns)
		return -EAGAIN;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
		start_ns = (uint64_t)ts.tv_sec * 1000000000ull +
			   (uint64_t)ts.tv_nsec;
	deadline_ns = start_ns + timeout_ns;

	for (;;) {
		if (nvidia_sema_signaled_geq(sema_cpu, payload))
			return 0;
		if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
			return -ETIMEDOUT;
		now_ns = (uint64_t)ts.tv_sec * 1000000000ull +
			 (uint64_t)ts.tv_nsec;
		if (now_ns >= deadline_ns)
			return -ETIMEDOUT;
		/* Brief yield so other threads / IRQ completion can progress */
		sched_yield();
	}
}

int
nvidia_submit_wait_complete(volatile void *userd, uint32_t target_put,
			    volatile uint32_t *sema_cpu, uint32_t sema_payload,
			    volatile void *notifier, uint64_t timeout_ns)
{
	int r;
	int sema_r = 0;

	/*
	 * Wait order for G1/G2/G3 bring-up:
	 *  1) sema GEQ if sema_cpu+payload (CE/QMD/3D release is the real done signal)
	 *  2) optionally also drain GPFIFO (helps distinguish sema miss vs hung ring)
	 *  3) non-blocking notifier peek for channel error
	 *
	 * If sema times out, still try GPFIFO idle with remaining budget so logs
	 * can see whether the ring advanced (caller sees sema failure first).
	 */
	if (sema_cpu && sema_payload) {
		sema_r = nvidia_sema_wait_geq(sema_cpu, sema_payload, timeout_ns);
		if (userd && sema_r != 0) {
			/* Short GPFIFO poll for diagnostics; ignore result */
			(void)nvidia_userd_wait_gpfifo_idle(userd, target_put,
							    timeout_ns > 100000000ull
								    ? 100000000ull
								    : timeout_ns);
		} else if (userd && sema_r == 0 && timeout_ns) {
			/*
			 * Sema ok: best-effort GPFIFO drain so later submits see
			 * idle ring (non-fatal if GPGet lags slightly).
			 */
			(void)nvidia_userd_wait_gpfifo_idle(userd, target_put,
							    timeout_ns > 500000000ull
								    ? 500000000ull
								    : timeout_ns);
		}
		if (sema_r)
			return sema_r;
	} else if (userd) {
		r = nvidia_userd_wait_gpfifo_idle(userd, target_put, timeout_ns);
		if (r)
			return r;
	}

	if (notifier) {
		r = nvidia_notifier_status(notifier, NULL, NULL);
		if (r == -EAGAIN)
			return 0; /* sema/idle done; notifier may lag */
		if (r)
			return r;
	}
	return 0;
}

int
nvidia_rm_export_dmabuf_raw(int fd, NvHandle h_client,
			    NvHandle *handles, NvU64 *offsets, NvU64 *sizes,
			    NvU32 num_objects, NvU64 total_size,
			    int *dmabuf_fd)
{
	nv_ioctl_export_to_dma_buf_fd_t p;
	int req;
	int ret;
	uint32_t i;

	if (!handles || !sizes || num_objects == 0 ||
	    num_objects > NV_DMABUF_EXPORT_MAX_HANDLES)
		return -EINVAL;

	memset(&p, 0, sizeof(p));
	p.fd = -1;
	p.hClient = h_client;
	p.totalObjects = num_objects;
	p.numObjects = num_objects;
	p.index = 0;
	p.totalSize = total_size;
	p.mappingType = NV_DMABUF_EXPORT_MAPPING_TYPE_DEFAULT;
	p.bAllowMmap = NV_TRUE;

	for (i = 0; i < num_objects; i++) {
		p.handles[i] = handles[i];
		p.offsets[i] = offsets ? offsets[i] : 0;
		p.sizes[i] = sizes[i];
	}
	p.status = NV_ERR_GENERIC;

	req = _IOC(_IOC_READ | _IOC_WRITE, NV_IOCTL_MAGIC,
		   NV_ESC_EXPORT_TO_DMABUF_FD, sizeof(p));
	ret = nvidia_ioctl(fd, req, &p);
	if (ret != 0)
		return -errno;
	if (p.status != NV_OK)
		return -(int)p.status;

	if (dmabuf_fd)
		*dmabuf_fd = p.fd;
	return 0;
}

const char *
nvidia_rm_status_string(uint32_t status)
{
	switch (status) {
	case NV_OK:                       return "NV_OK";
	case NV_ERR_GENERIC:              return "NV_ERR_GENERIC";
	case NV_ERR_INVALID_ARGUMENT:     return "NV_ERR_INVALID_ARGUMENT";
	case NV_ERR_INVALID_OBJECT_HANDLE:return "NV_ERR_INVALID_OBJECT_HANDLE";
	case NV_ERR_INVALID_OBJECT_PARENT:return "NV_ERR_INVALID_OBJECT_PARENT";
	case NV_ERR_INSUFFICIENT_RESOURCES:return "NV_ERR_INSUFFICIENT_RESOURCES";
	case NV_ERR_INVALID_OPERATION:    return "NV_ERR_INVALID_OPERATION";
	case NV_ERR_NOT_SUPPORTED:        return "NV_ERR_NOT_SUPPORTED";
	case NV_ERR_GPU_IS_LOST:          return "NV_ERR_GPU_IS_LOST";
	case NV_ERR_NO_MEMORY:            return "NV_ERR_NO_MEMORY";
	case NV_ERR_OPERATING_SYSTEM:     return "NV_ERR_OPERATING_SYSTEM";
	default:                          return "NV_STATUS_UNKNOWN";
	}
}

const char *
nvidia_gpu_arch_name(uint32_t architecture)
{
	switch (architecture) {
	case NVIDIA_GPU_ARCH_KEPLER:    return "Kepler";
	case NVIDIA_GPU_ARCH_MAXWELL:   return "Maxwell";
	case NVIDIA_GPU_ARCH_PASCAL:    return "Pascal";
	case NVIDIA_GPU_ARCH_VOLTA:     return "Volta";
	case NVIDIA_GPU_ARCH_TURING:    return "Turing";
	case NVIDIA_GPU_ARCH_AMPERE:    return "Ampere";
	case NVIDIA_GPU_ARCH_ADA:       return "Ada";
	case NVIDIA_GPU_ARCH_HOPPER:    return "Hopper";
	case NVIDIA_GPU_ARCH_BLACKWELL: return "Blackwell";
	default:                        return "Unknown";
	}
}
