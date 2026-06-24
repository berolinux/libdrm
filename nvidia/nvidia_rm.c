/*
#include <stdbool.h>
 * Copyright 2026 - Open NVIDIA userspace driver project
 * SPDX-License-Identifier: MIT
 *
 * RM ioctl wrappers. Parameter layouts and escape codes taken from
 * open-gpu-kernel-modules (nv-ioctl.h, nv_escape.h, escape.c, nvos.h).
 */

#include <errno.h>
#include <stdio.h>
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
	return nvidia_rm_control_raw(fd, h_client, h_channel,
				     NVA06F_CTRL_CMD_GPFIFO_SCHEDULE,
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
	static const NvV32 classes[] = {
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

		if (classes[i] == HOPPER_USERMODE_A) {
			memset(&hp, 0, sizeof(hp));
			parms = &hp;
			parms_size = sizeof(hp);
		}

		h_um = *h_usermode_out ? *h_usermode_out : 0;
		ret = nvidia_rm_alloc_raw(fd, h_root, h_subdevice, &h_um,
					  classes[i], parms, parms_size);
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
	doorbell = (volatile NvU32 *)((uint8_t *)usermode_map +
				      NVC361_NOTIFY_CHANNEL_PENDING);
	*doorbell = work_submit_token;
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
	/* NV506F/NVC36F GPFIFO entry: 8 bytes, GET in bits 31:2 of word0 */
	entry[0] = (NvU32)((gpu_addr >> NV_GP_ENTRY0_GET_SHIFT) << NV_GP_ENTRY0_GET_SHIFT);
	entry[1] = ((NvU32)(gpu_addr >> 32) & NV_GP_ENTRY1_GET_HI_MASK) |
		   ((length_dwords & NV_GP_ENTRY1_LENGTH_MASK) << NV_GP_ENTRY1_LENGTH_SHIFT);
	if (priv)
		entry[1] |= (1u << NV_GP_ENTRY1_PRIV_SHIFT);
	if (wait)
		entry[1] |= (1u << NV_GP_ENTRY1_LEVEL_SHIFT); /* LEVEL_SUBROUTINE often used with wait semantics on some gens; SYNC is separate on older */
	(void)wait;
}

/*
 * Host-side GPFIFO ring submit: mirrors mesa nv_channel_kickoff / nvidia-push.
 * Writes one entry, advances put, publishes USERD GPPut, optional doorbell.
 */
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
	volatile nvidia_userd_control_t *ud;
	uint32_t put_idx, next_put;
	uint32_t entry[2];
	struct timespec ts;
	uint64_t start_ns = 0, now_ns, deadline_ns;

	if (!gpfifo_cpu || !gpfifo_put_inout || !userd || !gpfifo_entries ||
	    !pb_dwords)
		return -EINVAL;

	ud = (volatile nvidia_userd_control_t *)userd;
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

	nvidia_gp_entry_pack(entry, pb_gpu_addr, pb_dwords, false, false);
	gpfifo_cpu[put_idx * 2 + 0] = entry[0];
	gpfifo_cpu[put_idx * 2 + 1] = entry[1];
	__sync_synchronize();

	*gpfifo_put_inout = next_put;
	ud->GPPut = next_put;
	__sync_synchronize();

	if (has_work_submit_token && usermode_map)
		nvidia_rm_doorbell_ring(usermode_map, work_submit_token);

	return 0;
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
