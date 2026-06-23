/*
 * Copyright 2026 - Open NVIDIA userspace driver project
 * SPDX-License-Identifier: MIT
 *
 * RM ioctl wrappers. Parameter layouts and escape codes taken from
 * open-gpu-kernel-modules (nv-ioctl.h, nv_escape.h, escape.c, nvos.h).
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
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
	 * Prefer NVOS64 (NV_ESC_RM_ALLOC) which supports rights + flags.
	 * If alloc_parms is non-NULL we pass its pointer; the kernel copies
	 * from userspace via the pAllocParms field.
	 */
	NVOS64_PARAMETERS p64;
	int ret;

	memset(&p64, 0, sizeof(p64));
	p64.hRoot = h_root;
	p64.hObjectParent = h_parent;
	p64.hObjectNew = h_new ? *h_new : 0;
	p64.hClass = h_class;
	p64.pAllocParms = alloc_parms ? (NvU64)(uintptr_t)alloc_parms : 0;
	p64.pRightsRequested = 0;
	p64.flags = 0;
	p64.status = NV_ERR_GENERIC;

	ret = rm_ioctl_auto(fd, NV_ESC_RM_ALLOC, &p64, sizeof(p64));
	if (ret != 0)
		return -errno;

	if (p64.status != NV_OK) {
		/* Fallback: try NVOS21 layout on older modules */
		NVOS21_PARAMETERS p21;

		memset(&p21, 0, sizeof(p21));
		p21.hRoot = h_root;
		p21.hObjectParent = h_parent;
		p21.hObjectNew = h_new ? *h_new : 0;
		p21.hClass = h_class;
		p21.pAllocParms = alloc_parms ? (NvU64)(uintptr_t)alloc_parms : 0;
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

	if (h_new)
		*h_new = p64.hObjectNew;
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
	/*
	 * NVOS32 vidheap uses a large union; we populate the alloc-size
	 * fields matching the kernel's NVOS32_PARAMETERS layout as used by
	 * nvkms and the binary driver.  The function field selects the op.
	 *
	 * Full NVOS32_PARAMETERS is much larger; we send a buffer sized to
	 * the kernel expectation via xfer.  For now send our simplified
	 * structure which covers the common alloc-size path.
	 */
	NVOS32_PARAMETERS_ALLOC_SIZE p;
	int ret;

	memset(&p, 0, sizeof(p));
	p.hRoot = h_root;
	p.hObjectParent = h_parent;
	p.function = NVOS32_FUNCTION_ALLOC_SIZE;
	p.owner = 0x10de; /* NVIDIA RM owner tag commonly used by userspace */
	p.type = type ? type : NVOS32_TYPE_DMA;
	p.flags = flags | NVOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED |
		  NVOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED;
	p.align = align ? align : NVIDIA_DEFAULT_ALIGNMENT;
	p.size = size;
	p.attr = attr;
	p.attr2 = attr2;
	p.hMemory = h_memory ? *h_memory : 0;
	p.status = NV_ERR_GENERIC;

	ret = rm_ioctl_auto(fd, NV_ESC_RM_VID_HEAP_CONTROL, &p, sizeof(p));
	if (ret != 0)
		return -errno;
	if (p.status != NV_OK)
		return -(int)p.status;

	if (h_memory)
		*h_memory = p.hMemory;
	if (offset)
		*offset = p.offset;
	if (limit)
		*limit = p.limit;
	return 0;
}

int
nvidia_rm_vidheap_free_raw(int fd, NvHandle h_root, NvHandle h_parent,
			   NvHandle h_memory)
{
	NVOS32_PARAMETERS_ALLOC_SIZE p;
	int ret;

	memset(&p, 0, sizeof(p));
	p.hRoot = h_root;
	p.hObjectParent = h_parent;
	p.function = NVOS32_FUNCTION_FREE;
	p.hMemory = h_memory;
	p.status = NV_ERR_GENERIC;

	ret = rm_ioctl_auto(fd, NV_ESC_RM_VID_HEAP_CONTROL, &p, sizeof(p));
	if (ret != 0)
		return -errno;
	if (p.status != NV_OK)
		return -(int)p.status;
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
