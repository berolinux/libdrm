/*
 * Copyright 2026 - Open NVIDIA userspace driver project
 * SPDX-License-Identifier: MIT
 *
 * Device open/close and RM client/device/subdevice setup.
 * Flow mirrors what the proprietary libcuda/libGLX_nvidia perform against
 * /dev/nvidiactl + /dev/nvidiaN, based on open-gpu-kernel-modules ioctl ABI.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "xf86drm.h"
#include "nvidia_internal.h"

static pthread_mutex_t dev_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct nvidia_device *dev_list;

int
nvidia_device_open_ctl(void)
{
	int fd = open(NVIDIA_CTL_DEVICE_PATH, O_RDWR | O_CLOEXEC);

	if (fd < 0)
		return -errno;
	return fd;
}

int
nvidia_device_open_gpu(int minor)
{
	char path[64];
	int fd;

	snprintf(path, sizeof(path), NVIDIA_GPU_DEVICE_PATH, minor);
	fd = open(path, O_RDWR | O_CLOEXEC);
	if (fd < 0)
		return -errno;
	return fd;
}

uint32_t
nvidia_device_new_handle(struct nvidia_device *dev)
{
	uint32_t h;

	pthread_mutex_lock(&dev->handle_mutex);
	/* RM accepts userspace-chosen handles; avoid 0 (NULL object) */
	if (dev->next_handle < 0x1000)
		dev->next_handle = 0x1000;
	h = dev->next_handle++;
	pthread_mutex_unlock(&dev->handle_mutex);
	return h;
}

bool
nvidia_probe_available(void)
{
	int fd = open(NVIDIA_CTL_DEVICE_PATH, O_RDWR | O_CLOEXEC);

	if (fd < 0)
		return false;
	close(fd);
	return true;
}

bool
nvidia_drm_check_fd(int fd)
{
	drmVersionPtr ver;
	bool ok = false;

	if (fd < 0)
		return false;
	ver = drmGetVersion(fd);
	if (!ver)
		return false;
	if (ver->name && (strcmp(ver->name, "nvidia-drm") == 0 ||
			  strcmp(ver->name, "nvidia") == 0))
		ok = true;
	drmFreeVersion(ver);
	return ok;
}

int
nvidia_device_rm_setup_client(struct nvidia_device *dev)
{
	NvHandle h_client;
	int ret;

	if (dev->rm_client_allocated)
		return 0;

	/*
	 * Allocate root client: parent = NV01_NULL_OBJECT, class = NV01_ROOT_USER.
	 * This is the first call every NVIDIA userspace driver makes.
	 */
	/* Prefer NV01_ROOT_CLIENT (0x41); fall back to NV01_ROOT_USER alias then NV01_ROOT */
	h_client = nvidia_device_new_handle(dev);
	ret = nvidia_rm_alloc_raw(dev->fd_ctl, 0 /* hRoot unused for first alloc */,
				  NV01_NULL_OBJECT, &h_client, NV01_ROOT_CLIENT,
				  NULL, 0);
	if (ret != 0) {
		h_client = nvidia_device_new_handle(dev);
		ret = nvidia_rm_alloc_raw(dev->fd_ctl, 0, NV01_NULL_OBJECT,
					  &h_client, NV01_ROOT_USER, NULL, 0);
	}
	if (ret != 0) {
		h_client = nvidia_device_new_handle(dev);
		ret = nvidia_rm_alloc_raw(dev->fd_ctl, 0, NV01_NULL_OBJECT,
					  &h_client, NV01_ROOT, NULL, 0);
		if (ret != 0)
			return ret;
	}

	/* Some modules need WAIT_OPEN_COMPLETE after first client alloc */
	nvidia_rm_wait_open_complete_raw(dev->fd_ctl, NULL, NULL);

	dev->h_client = h_client;
	dev->rm_client_allocated = true;
	return 0;
}

int
nvidia_device_rm_setup_device(struct nvidia_device *dev, int gpu_index)
{
	NV0080_ALLOC_PARAMETERS dev_alloc;
	NV2080_ALLOC_PARAMETERS sub_alloc;
	NvHandle h_device, h_subdevice;
	nv_ioctl_card_info_t *card;
	int ret;
	int minor;

	if (gpu_index < 0 || gpu_index >= dev->gpu_count)
		return -EINVAL;
	if (dev->rm_device_allocated && dev->gpu_index == gpu_index)
		return 0;

	/* Tear down previous device if switching GPUs */
	if (dev->rm_subdevice_allocated) {
		nvidia_rm_free_raw(dev->fd_ctl, dev->h_client, dev->h_device,
				   dev->h_subdevice);
		dev->rm_subdevice_allocated = false;
		dev->h_subdevice = 0;
	}
	if (dev->rm_device_allocated) {
		nvidia_rm_free_raw(dev->fd_ctl, dev->h_client, dev->h_client,
				   dev->h_device);
		dev->rm_device_allocated = false;
		dev->h_device = 0;
	}
	if (dev->fd_gpu >= 0) {
		close(dev->fd_gpu);
		dev->fd_gpu = -1;
	}

	card = &dev->cards[gpu_index];
	minor = (int)card->minor_number;

	/* Open /dev/nvidiaN and register with control fd */
	ret = nvidia_device_open_gpu(minor);
	if (ret < 0) {
		/* Try gpu_index as minor if minor_number was zero */
		ret = nvidia_device_open_gpu(gpu_index);
		if (ret < 0)
			return ret;
	}
	dev->fd_gpu = ret;

	ret = nvidia_rm_register_fd(dev->fd_gpu, dev->fd_ctl);
	/* Non-fatal on some driver versions if already registered */
	(void)ret;

	ret = nvidia_device_rm_setup_client(dev);
	if (ret != 0)
		return ret;

	/* Allocate NV01_DEVICE_0 */
	memset(&dev_alloc, 0, sizeof(dev_alloc));
	dev_alloc.deviceId = card->gpu_id != 0 ? 0 /* instance */ :
			     (NvU32)gpu_index;
	/* deviceId is the device instance index (0, 1, ...), not gpu_id */
	dev_alloc.deviceId = (NvU32)gpu_index;
	dev_alloc.hClientShare = dev->h_client;
	dev_alloc.vaMode = 2; /* NV_DEVICE_ALLOCATION_VAMODE_MULTIPLE_VASPACES style */

	h_device = nvidia_device_new_handle(dev);
	ret = nvidia_rm_alloc_raw(dev->fd_ctl, dev->h_client, dev->h_client,
				  &h_device, NV01_DEVICE_0,
				  &dev_alloc, sizeof(dev_alloc));
	if (ret != 0)
		return ret;

	dev->h_device = h_device;
	dev->rm_device_allocated = true;

	/* Allocate NV20_SUBDEVICE_0 */
	memset(&sub_alloc, 0, sizeof(sub_alloc));
	sub_alloc.subDeviceId = 0;

	h_subdevice = nvidia_device_new_handle(dev);
	ret = nvidia_rm_alloc_raw(dev->fd_ctl, dev->h_client, h_device,
				  &h_subdevice, NV20_SUBDEVICE_0,
				  &sub_alloc, sizeof(sub_alloc));
	if (ret != 0) {
		nvidia_rm_free_raw(dev->fd_ctl, dev->h_client, dev->h_client,
				   h_device);
		dev->rm_device_allocated = false;
		dev->h_device = 0;
		return ret;
	}

	dev->h_subdevice = h_subdevice;
	dev->rm_subdevice_allocated = true;
	dev->gpu_index = gpu_index;
	return 0;
}

int
nvidia_device_refresh_gpu_info(struct nvidia_device *dev, int gpu_index)
{
	struct nvidia_gpu_info *info;
	nv_ioctl_card_info_t *card;
	NV2080_CTRL_MC_GET_ARCH_INFO_PARAMS arch;
	NV2080_CTRL_GPU_GET_NAME_STRING_PARAMS name_params;
	NV2080_CTRL_FB_GET_INFO_V2_PARAMS fb;
	NV2080_CTRL_GR_GET_INFO_V2_PARAMS gr;
	int ret;
	uint32_t i;

	if (gpu_index < 0 || gpu_index >= dev->gpu_count)
		return -EINVAL;

	info = &dev->gpu_info_cache[gpu_index];
	card = &dev->cards[gpu_index];
	memset(info, 0, sizeof(*info));

	info->gpu_id = card->gpu_id;
	info->pci_domain = card->pci_info.domain;
	info->pci_bus = card->pci_info.bus;
	info->pci_device = card->pci_info.slot;
	info->pci_function = card->pci_info.function;
	info->pci_vendor_id = card->pci_info.vendor_id;
	info->pci_device_id = card->pci_info.device_id;
	info->reg_address = card->reg_address;
	info->reg_size = card->reg_size;
	info->fb_bar_address = card->fb_address;
	info->fb_bar_size = card->fb_size;
	info->device_instance = (uint32_t)gpu_index;
	info->valid = true;

	/* Need subdevice for the rich queries */
	if (!dev->rm_subdevice_allocated || dev->gpu_index != gpu_index) {
		ret = nvidia_device_rm_setup_device(dev, gpu_index);
		if (ret != 0) {
			/* Still return basic PCI info */
			dev->gpu_info_valid[gpu_index] = true;
			return 0;
		}
	}

	/* Architecture */
	memset(&arch, 0, sizeof(arch));
	ret = nvidia_rm_control_raw(dev->fd_ctl, dev->h_client, dev->h_subdevice,
				    NV2080_CTRL_CMD_MC_GET_ARCH_INFO,
				    &arch, sizeof(arch));
	if (ret == 0) {
		info->architecture = arch.architecture;
		info->implementation = arch.implementation;
		info->revision = arch.revision;
	}

	/* Name */
	memset(&name_params, 0, sizeof(name_params));
	ret = nvidia_rm_control_raw(dev->fd_ctl, dev->h_client, dev->h_subdevice,
				    NV2080_CTRL_CMD_GPU_GET_NAME_STRING,
				    &name_params, sizeof(name_params));
	if (ret == 0) {
		memcpy(info->name, name_params.gpuNameString,
		       sizeof(info->name) - 1);
	}

	/* Framebuffer info */
	memset(&fb, 0, sizeof(fb));
	fb.fbInfoListSize = 8;
	fb.fbInfoList[0].index = NV2080_CTRL_FB_INFO_INDEX_RAM_SIZE;
	fb.fbInfoList[1].index = NV2080_CTRL_FB_INFO_INDEX_USABLE_RAM_SIZE;
	fb.fbInfoList[2].index = NV2080_CTRL_FB_INFO_INDEX_HEAP_SIZE;
	fb.fbInfoList[3].index = NV2080_CTRL_FB_INFO_INDEX_HEAP_FREE;
	fb.fbInfoList[4].index = NV2080_CTRL_FB_INFO_INDEX_HEAP_START;
	ret = nvidia_rm_control_raw(dev->fd_ctl, dev->h_client, dev->h_subdevice,
				    NV2080_CTRL_CMD_FB_GET_INFO_V2,
				    &fb, sizeof(fb));
	if (ret == 0) {
		for (i = 0; i < fb.fbInfoListSize && i < 8; i++) {
			switch (fb.fbInfoList[i].index) {
			case NV2080_CTRL_FB_INFO_INDEX_RAM_SIZE:
				/* RM reports in KB for some indices; store as given * 1024 if small */
				info->fb_size = (uint64_t)fb.fbInfoList[i].data << 10;
				break;
			case NV2080_CTRL_FB_INFO_INDEX_USABLE_RAM_SIZE:
				info->fb_usable = (uint64_t)fb.fbInfoList[i].data << 10;
				break;
			case NV2080_CTRL_FB_INFO_INDEX_HEAP_FREE:
				info->fb_free = (uint64_t)fb.fbInfoList[i].data << 10;
				break;
			default:
				break;
			}
		}
	}

	/* Graphics / SM info */
	memset(&gr, 0, sizeof(gr));
	gr.grInfoListSize = 6;
	gr.grInfoList[0].index = NV2080_CTRL_GR_INFO_INDEX_SM_VERSION;
	gr.grInfoList[1].index = NV2080_CTRL_GR_INFO_INDEX_SHADER_PIPE_COUNT;
	gr.grInfoList[2].index = NV2080_CTRL_GR_INFO_INDEX_SHADER_PIPE_SUB_COUNT;
	gr.grInfoList[3].index = NV2080_CTRL_GR_INFO_INDEX_MAX_WARPS_PER_SM;
	ret = nvidia_rm_control_raw(dev->fd_ctl, dev->h_client, dev->h_subdevice,
				    NV2080_CTRL_CMD_GR_GET_INFO_V2,
				    &gr, sizeof(gr));
	if (ret == 0) {
		for (i = 0; i < gr.grInfoListSize && i < 6; i++) {
			switch (gr.grInfoList[i].index) {
			case NV2080_CTRL_GR_INFO_INDEX_SM_VERSION:
				info->sm_version = gr.grInfoList[i].data;
				break;
			case NV2080_CTRL_GR_INFO_INDEX_SHADER_PIPE_COUNT:
				info->gpc_count = gr.grInfoList[i].data;
				break;
			case NV2080_CTRL_GR_INFO_INDEX_SHADER_PIPE_SUB_COUNT:
				info->tpc_count = gr.grInfoList[i].data;
				break;
			default:
				break;
			}
		}
	}

	dev->gpu_info_valid[gpu_index] = true;
	return 0;
}

static struct nvidia_device *
nvidia_device_alloc_struct(void)
{
	struct nvidia_device *dev = calloc(1, sizeof(*dev));

	if (!dev)
		return NULL;
	dev->fd_ctl = -1;
	dev->fd_gpu = -1;
	dev->fd_drm = -1;
	dev->gpu_index = -1;
	dev->next_handle = 0x1000;
	atomic_set(&dev->refcount, 1);
	pthread_mutex_init(&dev->handle_mutex, NULL);
	pthread_mutex_init(&dev->bo_mutex, NULL);
	return dev;
}

static void
nvidia_device_free_struct(struct nvidia_device *dev)
{
	if (!dev)
		return;

	if (dev->rm_subdevice_allocated)
		nvidia_rm_free_raw(dev->fd_ctl, dev->h_client, dev->h_device,
				   dev->h_subdevice);
	if (dev->rm_device_allocated)
		nvidia_rm_free_raw(dev->fd_ctl, dev->h_client, dev->h_client,
				   dev->h_device);
	if (dev->rm_client_allocated)
		nvidia_rm_free_raw(dev->fd_ctl, dev->h_client, NV01_NULL_OBJECT,
				   dev->h_client);

	if (dev->fd_gpu >= 0)
		close(dev->fd_gpu);
	if (dev->fd_ctl >= 0)
		close(dev->fd_ctl);
	/* do not close fd_drm - borrowed from caller */

	pthread_mutex_destroy(&dev->handle_mutex);
	pthread_mutex_destroy(&dev->bo_mutex);
	free(dev);
}

static int
nvidia_device_init_common(struct nvidia_device *dev, int fd_drm)
{
	int ret;
	int count = 0;

	dev->fd_drm = fd_drm;

	ret = nvidia_device_open_ctl();
	if (ret < 0)
		return ret;
	dev->fd_ctl = ret;

	ret = nvidia_rm_check_version(dev->fd_ctl, dev->rm_version,
				      sizeof(dev->rm_version));
	if (ret == 0)
		dev->rm_version_valid = true;
	/* Version check failure is non-fatal for basic probing */

	ret = nvidia_rm_card_info(dev->fd_ctl, dev->cards, NV_MAX_DEVICES, &count);
	if (ret != 0)
		return ret;
	dev->gpu_count = count;

	ret = nvidia_device_rm_setup_client(dev);
	/* Client setup may fail if no permissions; allow ctl-only mode */
	(void)ret;

	return 0;
}

int
nvidia_device_initialize(int fd_drm, uint32_t *major_version,
			 uint32_t *minor_version,
			 nvidia_device_handle *device_out)
{
	struct nvidia_device *dev;
	int ret;

	if (!device_out)
		return -EINVAL;

	dev = nvidia_device_alloc_struct();
	if (!dev)
		return -ENOMEM;

	ret = nvidia_device_init_common(dev, fd_drm);
	if (ret != 0) {
		nvidia_device_free_struct(dev);
		return ret;
	}

	if (major_version)
		*major_version = 1;
	if (minor_version)
		*minor_version = 0;

	pthread_mutex_lock(&dev_list_mutex);
	dev->next = dev_list;
	dev_list = dev;
	pthread_mutex_unlock(&dev_list_mutex);

	*device_out = dev;
	return 0;
}

int
nvidia_device_initialize_gpu(int fd_drm, int gpu_index,
			     uint32_t *major_version, uint32_t *minor_version,
			     nvidia_device_handle *device_out)
{
	struct nvidia_device *dev;
	int ret;

	if (!device_out || gpu_index < 0)
		return -EINVAL;

	dev = nvidia_device_alloc_struct();
	if (!dev)
		return -ENOMEM;

	ret = nvidia_device_init_common(dev, fd_drm);
	if (ret != 0) {
		nvidia_device_free_struct(dev);
		return ret;
	}

	if (gpu_index >= dev->gpu_count) {
		nvidia_device_free_struct(dev);
		return -ENODEV;
	}

	ret = nvidia_device_rm_setup_device(dev, gpu_index);
	if (ret != 0) {
		nvidia_device_free_struct(dev);
		return ret;
	}

	nvidia_device_refresh_gpu_info(dev, gpu_index);

	if (major_version)
		*major_version = 1;
	if (minor_version)
		*minor_version = 0;

	pthread_mutex_lock(&dev_list_mutex);
	dev->next = dev_list;
	dev_list = dev;
	pthread_mutex_unlock(&dev_list_mutex);

	*device_out = dev;
	return 0;
}

int
nvidia_device_deinitialize(nvidia_device_handle device)
{
	struct nvidia_device *dev = device;
	struct nvidia_device **slot;

	if (!dev)
		return -EINVAL;

	pthread_mutex_lock(&dev_list_mutex);
	for (slot = &dev_list; *slot; slot = &(*slot)->next) {
		if (*slot == dev) {
			*slot = dev->next;
			break;
		}
	}
	pthread_mutex_unlock(&dev_list_mutex);

	nvidia_device_free_struct(dev);
	return 0;
}

int
nvidia_device_get_fd(nvidia_device_handle device)
{
	return device ? device->fd_ctl : -1;
}

int
nvidia_device_get_gpu_fd(nvidia_device_handle device)
{
	return device ? device->fd_gpu : -1;
}

int
nvidia_device_get_drm_fd(nvidia_device_handle device)
{
	return device ? device->fd_drm : -1;
}

uint32_t
nvidia_device_get_client_handle(nvidia_device_handle device)
{
	return device ? device->h_client : 0;
}

uint32_t
nvidia_device_get_device_handle(nvidia_device_handle device)
{
	return device ? device->h_device : 0;
}

uint32_t
nvidia_device_get_subdevice_handle(nvidia_device_handle device)
{
	return device ? device->h_subdevice : 0;
}

int
nvidia_device_get_gpu_count(nvidia_device_handle device)
{
	return device ? device->gpu_count : 0;
}

int
nvidia_query_gpu_info(nvidia_device_handle device, int gpu_index,
		      struct nvidia_gpu_info *info)
{
	int ret;

	if (!device || !info || gpu_index < 0 || gpu_index >= device->gpu_count)
		return -EINVAL;

	if (!device->gpu_info_valid[gpu_index]) {
		ret = nvidia_device_refresh_gpu_info(device, gpu_index);
		if (ret != 0 && !device->gpu_info_cache[gpu_index].valid)
			return ret;
	}

	*info = device->gpu_info_cache[gpu_index];
	return 0;
}

int
nvidia_query_selected_gpu_info(nvidia_device_handle device,
			       struct nvidia_gpu_info *info)
{
	if (!device || device->gpu_index < 0)
		return -EINVAL;
	return nvidia_query_gpu_info(device, device->gpu_index, info);
}

int
nvidia_query_rm_api_version(nvidia_device_handle device,
			    char *version_out, int version_len)
{
	if (!device || !version_out || version_len <= 0)
		return -EINVAL;

	if (!device->rm_version_valid) {
		int ret = nvidia_rm_check_version(device->fd_ctl, device->rm_version,
						  sizeof(device->rm_version));
		if (ret != 0)
			return ret;
		device->rm_version_valid = true;
	}

	strncpy(version_out, device->rm_version, version_len - 1);
	version_out[version_len - 1] = '\0';
	return 0;
}

/* --- Public RM wrappers on device handle --- */

int
nvidia_rm_alloc(nvidia_device_handle device,
		uint32_t h_parent,
		uint32_t *h_object_new,
		uint32_t h_class,
		void *alloc_params,
		uint32_t alloc_params_size)
{
	NvHandle h_new;
	int ret;

	if (!device || !h_object_new)
		return -EINVAL;

	h_new = *h_object_new ? *h_object_new : nvidia_device_new_handle(device);
	ret = nvidia_rm_alloc_raw(device->fd_ctl, device->h_client, h_parent,
				  &h_new, h_class, alloc_params,
				  alloc_params_size);
	if (ret == 0)
		*h_object_new = h_new;
	return ret;
}

int
nvidia_rm_free(nvidia_device_handle device,
	       uint32_t h_parent,
	       uint32_t h_object)
{
	if (!device)
		return -EINVAL;
	return nvidia_rm_free_raw(device->fd_ctl, device->h_client, h_parent,
				  h_object);
}

int
nvidia_rm_control(nvidia_device_handle device,
		  uint32_t h_object,
		  uint32_t cmd,
		  void *params,
		  uint32_t params_size)
{
	if (!device)
		return -EINVAL;
	return nvidia_rm_control_raw(device->fd_ctl, device->h_client, h_object,
				     cmd, params, params_size);
}

int
nvidia_rm_map_memory(nvidia_device_handle device,
		     uint32_t h_device,
		     uint32_t h_memory,
		     uint64_t offset,
		     uint64_t length,
		     void **cpu_ptr_out,
		     uint32_t flags)
{
	if (!device)
		return -EINVAL;
	return nvidia_rm_map_memory_raw(device->fd_ctl, device->h_client, h_device,
					h_memory, offset, length, cpu_ptr_out,
					flags);
}

int
nvidia_rm_unmap_memory(nvidia_device_handle device,
		       uint32_t h_device,
		       uint32_t h_memory,
		       void *cpu_ptr,
		       uint32_t flags)
{
	if (!device)
		return -EINVAL;
	return nvidia_rm_unmap_memory_raw(device->fd_ctl, device->h_client, h_device,
					  h_memory, cpu_ptr, flags);
}

int
nvidia_rm_vidheap_alloc(nvidia_device_handle device,
			uint32_t h_parent,
			uint32_t type,
			uint32_t flags,
			uint64_t size,
			uint64_t align,
			uint32_t attr,
			uint32_t attr2,
			uint32_t *h_memory_out,
			uint64_t *offset_out,
			uint64_t *limit_out)
{
	NvHandle h_mem;
	int ret;

	if (!device || !h_memory_out)
		return -EINVAL;

	h_mem = nvidia_device_new_handle(device);
	ret = nvidia_rm_vidheap_alloc_raw(device->fd_ctl, device->h_client,
					  h_parent ? h_parent : device->h_device,
					  type, flags, size, align, attr, attr2,
					  &h_mem, offset_out, limit_out);
	if (ret == 0)
		*h_memory_out = h_mem;
	return ret;
}

int
nvidia_rm_vidheap_free(nvidia_device_handle device,
		       uint32_t h_parent,
		       uint32_t h_memory)
{
	if (!device)
		return -EINVAL;
	return nvidia_rm_vidheap_free_raw(device->fd_ctl, device->h_client,
					  h_parent ? h_parent : device->h_device,
					  h_memory);
}

int
nvidia_rm_export_dmabuf(nvidia_device_handle device,
			uint32_t *handles,
			uint64_t *offsets,
			uint64_t *sizes,
			uint32_t num_objects,
			uint64_t total_size,
			int *dmabuf_fd_out)
{
	if (!device)
		return -EINVAL;
	return nvidia_rm_export_dmabuf_raw(device->fd_ctl, device->h_client,
					   handles, offsets, sizes, num_objects,
					   total_size, dmabuf_fd_out);
}

int
nvidia_rm_memory_alloc(nvidia_device_handle device,
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
		       uint64_t *limit_out)
{
	NvHandle h_mem;
	int ret;

	if (!device || !h_memory_out)
		return -EINVAL;
	h_mem = nvidia_device_new_handle(device);
	ret = nvidia_rm_memory_alloc_raw(device->fd_ctl, device->h_client,
					 h_parent ? h_parent : device->h_device,
					 &h_mem, h_class, device->h_client,
					 type, flags, attr, attr2, size, alignment,
					 offset_out, limit_out);
	if (ret == 0)
		*h_memory_out = h_mem;
	return ret;
}

int
nvidia_rm_alloc_os_event(nvidia_device_handle device, uint32_t h_device,
			 int event_fd)
{
	if (!device)
		return -EINVAL;
	return nvidia_rm_alloc_os_event_raw(device->fd_ctl, device->h_client,
					    h_device ? h_device : device->h_device,
					    event_fd, NULL);
}

int
nvidia_rm_free_os_event(nvidia_device_handle device, uint32_t h_device,
			int event_fd)
{
	if (!device)
		return -EINVAL;
	return nvidia_rm_free_os_event_raw(device->fd_ctl, device->h_client,
					   h_device ? h_device : device->h_device,
					   event_fd);
}

int
nvidia_rm_wait_open_complete(nvidia_device_handle device,
			     int32_t *rc_out, uint32_t *adapter_status_out)
{
	if (!device)
		return -EINVAL;
	return nvidia_rm_wait_open_complete_raw(device->fd_ctl, rc_out,
						adapter_status_out);
}

int
nvidia_rm_gpfifo_schedule(nvidia_device_handle device, uint32_t h_channel,
			  bool enable)
{
	if (!device || !h_channel)
		return -EINVAL;
	return nvidia_rm_gpfifo_schedule_raw(device->fd_ctl, device->h_client,
					     h_channel, enable ? NV_TRUE : NV_FALSE);
}

/*
 * Pass9 / OGKM ctrla06fgpfifo.h: BIND configures channel runlist for engineType
 * (NV2080_ENGINE_TYPE_GRAPHICS=1, COPY0=2, …). Non-fatal if already bound.
 */
int
nvidia_rm_gpfifo_bind(nvidia_device_handle device, uint32_t h_channel,
		      uint32_t engine_type)
{
	if (!device || !h_channel)
		return -EINVAL;
	return nvidia_rm_gpfifo_bind_raw(device->fd_ctl, device->h_client,
					 h_channel, engine_type);
}

/*
 * Canonical cold-path order (pass8/9): BIND then SCHEDULE.
 * BIND failure is tolerated (already-bound / wrong engine); SCHEDULE result wins.
 */
int
nvidia_rm_gpfifo_bind_and_schedule(nvidia_device_handle device,
				   uint32_t h_channel,
				   uint32_t engine_type,
				   bool enable)
{
	int ret;

	if (!device || !h_channel)
		return -EINVAL;
	(void)nvidia_rm_gpfifo_bind_raw(device->fd_ctl, device->h_client,
					h_channel, engine_type);
	ret = nvidia_rm_gpfifo_schedule_raw(device->fd_ctl, device->h_client,
					    h_channel,
					    enable ? NV_TRUE : NV_FALSE);
	return ret;
}

int
nvidia_rm_gpfifo_set_interleave_level(nvidia_device_handle device,
				      uint32_t h_channel,
				      uint32_t tsg_interleave_level)
{
	if (!device || !h_channel)
		return -EINVAL;
	return nvidia_rm_gpfifo_set_interleave_level_raw(device->fd_ctl,
							 device->h_client,
							 h_channel,
							 tsg_interleave_level);
}

int
nvidia_rm_gpfifo_get_interleave_level(nvidia_device_handle device,
				      uint32_t h_channel,
				      uint32_t *tsg_interleave_level_out)
{
	if (!device || !h_channel)
		return -EINVAL;
	return nvidia_rm_gpfifo_get_interleave_level_raw(device->fd_ctl,
							 device->h_client,
							 h_channel,
							 tsg_interleave_level_out);
}

int
nvidia_rm_gpfifo_restart_runlist(nvidia_device_handle device,
				 uint32_t h_channel,
				 bool bypass_wait_for_eng_idle)
{
	if (!device || !h_channel)
		return -EINVAL;
	return nvidia_rm_gpfifo_restart_runlist_raw(device->fd_ctl,
						    device->h_client, h_channel,
						    bypass_wait_for_eng_idle
							    ? NV_TRUE
							    : NV_FALSE);
}

int
nvidia_rm_gpfifo_stop_channel(nvidia_device_handle device, uint32_t h_channel,
			      bool in_preempt_timeout)
{
	if (!device || !h_channel)
		return -EINVAL;
	return nvidia_rm_gpfifo_stop_channel_raw(device->fd_ctl, device->h_client,
						 h_channel,
						 in_preempt_timeout ? NV_TRUE
								    : NV_FALSE);
}

int
nvidia_rm_gpfifo_get_context_id(nvidia_device_handle device, uint32_t h_channel,
				uint32_t *context_id_out)
{
	if (!device || !h_channel)
		return -EINVAL;
	return nvidia_rm_gpfifo_get_context_id_raw(device->fd_ctl,
						   device->h_client, h_channel,
						   context_id_out);
}

int
nvidia_rm_gpfifo_set_error_notifier(nvidia_device_handle device,
				    uint32_t h_channel,
				    bool notify_each_channel_in_tsg)
{
	if (!device || !h_channel)
		return -EINVAL;
	return nvidia_rm_gpfifo_set_error_notifier_raw(
		device->fd_ctl, device->h_client, h_channel,
		notify_each_channel_in_tsg ? NV_TRUE : NV_FALSE);
}

int
nvidia_rm_tsg_set_timeslice(nvidia_device_handle device,
			     uint32_t h_channel_group, uint64_t timeslice_us)
{
	if (!device || !h_channel_group)
		return -EINVAL;
	return nvidia_rm_tsg_set_timeslice_raw(device->fd_ctl, device->h_client,
					       h_channel_group, timeslice_us);
}

int
nvidia_rm_tsg_get_timeslice(nvidia_device_handle device,
			     uint32_t h_channel_group,
			     uint64_t *timeslice_us_out)
{
	if (!device || !h_channel_group)
		return -EINVAL;
	return nvidia_rm_tsg_get_timeslice_raw(device->fd_ctl, device->h_client,
					       h_channel_group,
					       timeslice_us_out);
}

int
nvidia_rm_tsg_preempt(nvidia_device_handle device, uint32_t h_channel_group,
		       bool wait, bool manual_timeout, uint32_t timeout_us)
{
	if (!device || !h_channel_group)
		return -EINVAL;
	return nvidia_rm_tsg_preempt_raw(device->fd_ctl, device->h_client,
					 h_channel_group,
					 wait ? NV_TRUE : NV_FALSE,
					 manual_timeout ? NV_TRUE : NV_FALSE,
					 timeout_us);
}

int
nvidia_rm_tsg_get_info(nvidia_device_handle device, uint32_t h_channel_group,
			uint32_t *tsg_id_out)
{
	if (!device || !h_channel_group)
		return -EINVAL;
	return nvidia_rm_tsg_get_info_raw(device->fd_ctl, device->h_client,
					  h_channel_group, tsg_id_out);
}

int
nvidia_rm_tsg_set_interleave_level(nvidia_device_handle device,
				    uint32_t h_channel_group,
				    uint32_t tsg_interleave_level)
{
	if (!device || !h_channel_group)
		return -EINVAL;
	return nvidia_rm_tsg_set_interleave_level_raw(
		device->fd_ctl, device->h_client, h_channel_group,
		tsg_interleave_level);
}

int
nvidia_rm_tsg_get_interleave_level(nvidia_device_handle device,
				    uint32_t h_channel_group,
				    uint32_t *tsg_interleave_level_out)
{
	if (!device || !h_channel_group)
		return -EINVAL;
	return nvidia_rm_tsg_get_interleave_level_raw(
		device->fd_ctl, device->h_client, h_channel_group,
		tsg_interleave_level_out);
}

int
nvidia_rm_tsg_make_realtime(nvidia_device_handle device,
			     uint32_t h_channel_group, bool realtime)
{
	if (!device || !h_channel_group)
		return -EINVAL;
	return nvidia_rm_tsg_make_realtime_raw(device->fd_ctl, device->h_client,
					       h_channel_group,
					       realtime ? NV_TRUE : NV_FALSE);
}

int
nvidia_rm_gpfifo_get_work_submit_token(nvidia_device_handle device,
				       uint32_t h_channel, uint32_t *token_out)
{
	if (!device || !h_channel)
		return -EINVAL;
	return nvidia_rm_gpfifo_get_work_submit_token_raw(device->fd_ctl,
							  device->h_client,
							  h_channel, token_out);
}

int
nvidia_rm_vaspace_alloc(nvidia_device_handle device,
			uint32_t *h_vaspace_out,
			uint32_t index, uint32_t flags,
			uint64_t va_size, uint64_t va_base,
			uint32_t big_page_size,
			uint64_t *va_size_out, uint64_t *va_base_out)
{
	NvHandle h_vas;
	int ret;

	if (!device || !h_vaspace_out || !device->h_device)
		return -EINVAL;
	h_vas = nvidia_device_new_handle(device);
	ret = nvidia_rm_vaspace_alloc_raw(device->fd_ctl, device->h_client,
					  device->h_device, &h_vas,
					  index, flags, va_size, va_base,
					  big_page_size, va_size_out, va_base_out);
	if (ret == 0)
		*h_vaspace_out = h_vas;
	return ret;
}

int
nvidia_rm_map_memory_dma(nvidia_device_handle device,
			 uint32_t h_device,
			 uint32_t h_dma,
			 uint32_t h_memory,
			 uint64_t offset,
			 uint64_t length,
			 uint32_t flags,
			 uint64_t *dma_offset_inout)
{
	if (!device || !h_dma || !h_memory)
		return -EINVAL;
	return nvidia_rm_map_memory_dma_raw(device->fd_ctl, device->h_client,
					    h_device ? h_device : device->h_device,
					    h_dma, h_memory, offset, length,
					    flags, dma_offset_inout);
}

int
nvidia_rm_unmap_memory_dma(nvidia_device_handle device,
			   uint32_t h_device,
			   uint32_t h_dma,
			   uint32_t h_memory,
			   uint64_t dma_offset,
			   uint64_t size,
			   uint32_t flags)
{
	if (!device || !h_dma || !h_memory)
		return -EINVAL;
	return nvidia_rm_unmap_memory_dma_raw(device->fd_ctl, device->h_client,
					      h_device ? h_device : device->h_device,
					      h_dma, h_memory, dma_offset, size,
					      flags);
}

int
nvidia_rm_usermode_alloc_map(nvidia_device_handle device,
			     uint32_t *h_usermode_out,
			     uint32_t *h_class_out,
			     void **usermode_map_out)
{
	NvHandle h_um;
	NvV32 h_class = 0;
	void *map = NULL;
	int ret;

	if (!device || !h_usermode_out || !device->h_subdevice)
		return -EINVAL;

	h_um = nvidia_device_new_handle(device);
	ret = nvidia_rm_usermode_alloc_raw(device->fd_ctl, device->h_client,
					   device->h_subdevice, &h_um, &h_class);
	if (ret != 0)
		return ret;

	ret = nvidia_rm_map_memory_raw(device->fd_ctl, device->h_client,
				       device->h_subdevice, h_um,
				       0, NVC361_NV_USERMODE__SIZE,
				       &map, 0);
	if (ret != 0) {
		nvidia_rm_free_raw(device->fd_ctl, device->h_client,
				   device->h_subdevice, h_um);
		return ret;
	}

	*h_usermode_out = h_um;
	if (h_class_out)
		*h_class_out = h_class;
	if (usermode_map_out)
		*usermode_map_out = map;
	return 0;
}

int
nvidia_rm_context_dma_alloc(nvidia_device_handle device,
			    uint32_t h_parent,
			    uint32_t *h_ctxdma_out,
			    uint32_t h_class,
			    uint32_t h_memory,
			    uint64_t offset,
			    uint64_t limit,
			    uint32_t flags)
{
	NvHandle h_cd;
	int ret;

	if (!device || !h_ctxdma_out || !h_memory)
		return -EINVAL;
	h_cd = nvidia_device_new_handle(device);
	ret = nvidia_rm_context_dma_alloc_raw(device->fd_ctl, device->h_client,
					      h_parent ? h_parent : device->h_device,
					      &h_cd,
					      h_class ? h_class : NV01_CONTEXT_ERROR_TO_MEMORY,
					      device->h_subdevice, h_memory,
					      offset, limit, flags);
	if (ret == 0)
		*h_ctxdma_out = h_cd;
	return ret;
}

int
nvidia_rm_channel_group_alloc(nvidia_device_handle device,
			      uint32_t *h_group_out,
			      uint32_t h_object_error,
			      uint32_t h_vaspace,
			      uint32_t engine_type)
{
	NvHandle h_grp;
	int ret;

	if (!device || !h_group_out || !device->h_device)
		return -EINVAL;
	h_grp = nvidia_device_new_handle(device);
	ret = nvidia_rm_channel_group_alloc_raw(device->fd_ctl, device->h_client,
						device->h_device, &h_grp,
						h_object_error, h_vaspace,
						engine_type);
	if (ret == 0)
		*h_group_out = h_grp;
	return ret;
}

int
nvidia_rm_ctxshare_alloc(nvidia_device_handle device,
			 uint32_t h_parent,
			 uint32_t *h_ctxshare_out,
			 uint32_t h_vaspace,
			 uint32_t flags)
{
	NvHandle h_cs;
	int ret;

	if (!device || !h_ctxshare_out)
		return -EINVAL;
	h_cs = nvidia_device_new_handle(device);
	ret = nvidia_rm_ctxshare_alloc_raw(device->fd_ctl, device->h_client,
					   h_parent ? h_parent : device->h_device,
					   &h_cs, h_vaspace, flags);
	if (ret == 0)
		*h_ctxshare_out = h_cs;
	return ret;
}

int
nvidia_rm_gpu_get_engines(nvidia_device_handle device,
			  uint32_t *engine_list, uint32_t *count_inout)
{
	NV2080_CTRL_GPU_GET_ENGINES_V2_PARAMS p;
	int ret;
	uint32_t i, n;

	if (!device || !count_inout)
		return -EINVAL;
	if (!device->rm_subdevice_allocated)
		return -ENODEV;

	memset(&p, 0, sizeof(p));
	ret = nvidia_rm_control_raw(device->fd_ctl, device->h_client,
				    device->h_subdevice,
				    NV2080_CTRL_CMD_GPU_GET_ENGINES_V2,
				    &p, sizeof(p));
	if (ret != 0)
		return ret;

	n = p.engineCount;
	if (n > NV2080_GPU_MAX_ENGINES_LIST_SIZE)
		n = NV2080_GPU_MAX_ENGINES_LIST_SIZE;
	if (engine_list) {
		uint32_t copy = n;
		if (copy > *count_inout)
			copy = *count_inout;
		for (i = 0; i < copy; i++)
			engine_list[i] = p.engineList[i];
	}
	*count_inout = n;
	return 0;
}

int
nvidia_rm_gpu_get_engine_classlist(nvidia_device_handle device,
				   uint32_t engine_type,
				   uint32_t *class_list,
				   uint32_t *count_inout)
{
	NV2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS p;
	int ret;
	uint32_t i, n;

	if (!device || !count_inout)
		return -EINVAL;
	if (!device->rm_subdevice_allocated)
		return -ENODEV;

	memset(&p, 0, sizeof(p));
	p.engineType = engine_type;
	p.numClasses = NV2080_CTRL_GPU_MAX_CLASSLIST;
	ret = nvidia_rm_control_raw(device->fd_ctl, device->h_client,
				    device->h_subdevice,
				    NV2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST,
				    &p, sizeof(p));
	if (ret != 0)
		return ret;

	n = p.numClasses;
	if (n > NV2080_CTRL_GPU_MAX_CLASSLIST)
		n = NV2080_CTRL_GPU_MAX_CLASSLIST;
	if (class_list) {
		uint32_t copy = n;
		if (copy > *count_inout)
			copy = *count_inout;
		for (i = 0; i < copy; i++)
			class_list[i] = p.classList[i];
	}
	*count_inout = n;
	return 0;
}

uint32_t
nvidia_pick_class_in_range(const uint32_t *class_list, uint32_t count,
			   uint32_t min_class, uint32_t max_class)
{
	uint32_t i, best = 0;

	if (!class_list || !count)
		return 0;
	for (i = 0; i < count; i++) {
		uint32_t c = class_list[i];
		if (c < min_class || c > max_class)
			continue;
		if (c > best)
			best = c;
	}
	return best;
}

int
nvidia_rm_channel_group_schedule(nvidia_device_handle device,
				 uint32_t h_channel_group,
				 bool enable)
{
	if (!device || !h_channel_group)
		return -EINVAL;
	return nvidia_rm_channel_group_schedule_raw(device->fd_ctl,
						    device->h_client,
						    h_channel_group,
						    enable ? NV_TRUE : NV_FALSE);
}
