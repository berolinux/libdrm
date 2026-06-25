/*
 * Copyright 2026 - Open NVIDIA userspace driver project
 * SPDX-License-Identifier: MIT
 *
 * Buffer object allocation/mapping on top of RM vidheap / system memory.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "nvidia_internal.h"

static uint64_t
align_u64(uint64_t value, uint64_t alignment)
{
	if (alignment == 0)
		alignment = NVIDIA_DEFAULT_ALIGNMENT;
	return (value + alignment - 1) & ~(alignment - 1);
}

struct nvidia_bo *
nvidia_bo_create_internal(struct nvidia_device *dev)
{
	struct nvidia_bo *bo = calloc(1, sizeof(*bo));

	if (!bo)
		return NULL;
	bo->dev = dev;
	atomic_set(&bo->refcount, 1);
	pthread_mutex_init(&bo->map_mutex, NULL);
	return bo;
}

int
nvidia_bo_alloc(nvidia_device_handle device,
		struct nvidia_bo_alloc_request *req,
		nvidia_bo_handle *bo_out)
{
	struct nvidia_bo *bo;
	uint64_t align, size;
	uint32_t flags = 0;
	uint32_t h_parent;
	int ret;

	if (!device || !req || !bo_out || req->size == 0)
		return -EINVAL;
	if (!device->rm_device_allocated)
		return -ENODEV;

	bo = nvidia_bo_create_internal(device);
	if (!bo)
		return -ENOMEM;

	align = req->alignment ? req->alignment : NVIDIA_DEFAULT_ALIGNMENT;
	size = align_u64(req->size, align);
	bo->size = req->size;
	bo->aligned_size = size;
	bo->domain = req->domain;
	bo->flags = req->flags;
	bo->rm_type = req->rm_type ? req->rm_type : NVOS32_TYPE_DMA;
	bo->h_parent = device->h_device;
	h_parent = device->h_device;

	/* Build NVOS32 flags from domain/request flags */
	flags = NVOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED |
		NVOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED;

	if (req->flags & NVIDIA_BO_FLAGS_NO_SCANOUT)
		flags |= NVOS32_ALLOC_FLAGS_NO_SCANOUT;

	switch (req->domain) {
	case NVIDIA_BO_DOMAIN_VRAM:
		/*
		 * Local video memory: prefer RmAlloc(NV01_MEMORY_LOCAL_USER) with
		 * NV_MEMORY_ALLOCATION_PARAMS (nvidia-push / nvkms path).  Fall
		 * back to NVOS32 vidheap, then bare class alloc.
		 */
		bo->rm_handle = nvidia_device_new_handle(device);
		flags = NVOS32_ALLOC_FLAGS_ALIGNMENT_FORCE |
			NVOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED |
			NVOS32_ALLOC_FLAGS_PERSISTENT_VIDMEM;
		if (req->flags & NVIDIA_BO_FLAGS_NO_SCANOUT)
			flags |= NVOS32_ALLOC_FLAGS_NO_SCANOUT;
		/* tick100: correct nvos.h ATTR bits; try allow-noncontig then strict */
		ret = nvidia_rm_memory_alloc_raw(device->fd_ctl, device->h_client,
						 h_parent, &bo->rm_handle,
						 NV01_MEMORY_LOCAL_USER,
						 device->h_client, bo->rm_type,
						 flags,
						 NV_OS32_ATTR_VIDMEM_4K_UNCACHED_NONCONTIG,
						 NV_OS32_ATTR2_GPU_CACHEABLE_NO_VAL,
						 size, align,
						 &bo->gpu_offset, &bo->limit);
		if (ret != 0) {
			bo->rm_handle = nvidia_device_new_handle(device);
			ret = nvidia_rm_memory_alloc_raw(device->fd_ctl,
							 device->h_client,
							 h_parent, &bo->rm_handle,
							 NV01_MEMORY_LOCAL_USER,
							 device->h_client,
							 bo->rm_type, flags,
							 NV_OS32_ATTR_VIDMEM_4K_UNCACHED,
							 NV_OS32_ATTR2_GPU_CACHEABLE_NO_VAL,
							 size, align,
							 &bo->gpu_offset,
							 &bo->limit);
		}
		if (ret != 0) {
			bo->rm_handle = nvidia_device_new_handle(device);
			ret = nvidia_rm_vidheap_alloc_raw(device->fd_ctl,
							  device->h_client,
							  h_parent, bo->rm_type,
							  flags, size, align,
							  NV_OS32_ATTR_VIDMEM_4K_UNCACHED_NONCONTIG,
							  NV_OS32_ATTR2_GPU_CACHEABLE_NO_VAL,
							  &bo->rm_handle,
							  &bo->gpu_offset,
							  &bo->limit);
		}
		if (ret != 0) {
			bo->rm_handle = nvidia_device_new_handle(device);
			ret = nvidia_rm_vidheap_alloc_raw(device->fd_ctl,
							  device->h_client,
							  h_parent, bo->rm_type,
							  flags, size, align,
							  NV_OS32_ATTR_VIDMEM_4K_UNCACHED,
							  NV_OS32_ATTR2_GPU_CACHEABLE_NO_VAL,
							  &bo->rm_handle,
							  &bo->gpu_offset,
							  &bo->limit);
		}
		if (ret != 0) {
			free(bo);
			return ret;
		}
		bo->allocated = true;
		bo->cpu_accessible = (req->flags & NVIDIA_BO_FLAGS_CPU_ACCESS) != 0 &&
				     (req->flags & NVIDIA_BO_FLAGS_NO_CPU_ACCESS) == 0;
		break;

	case NVIDIA_BO_DOMAIN_GART:
	case NVIDIA_BO_DOMAIN_CPU:
	default:
		/*
		 * System memory: RmAlloc(NV01_MEMORY_SYSTEM) with
		 * NV_MEMORY_ALLOCATION_PARAMS (PCI / write-combine for CPU maps).
		 * tick112: prefer non-contig WC/uncached with page size from
		 * device max_page_size (nvidia_rm_os32_attr_sysmem_mappable).
		 */
		bo->rm_handle = nvidia_device_new_handle(device);
		flags = NVOS32_ALLOC_FLAGS_ALIGNMENT_FORCE |
			NVOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED |
			NVOS32_ALLOC_FLAGS_NO_SCANOUT;
		{
			bool wc = (req->flags & NVIDIA_BO_FLAGS_CPU_ACCESS) != 0;
			uint64_t max_ps = 0x1000ull;
			int gi = device->gpu_index;
			if (gi >= 0 && gi < NVIDIA_MAX_GPUS &&
			    device->gpu_info_valid[gi] &&
			    device->gpu_info_cache[gi].max_page_size)
				max_ps = device->gpu_info_cache[gi].max_page_size;
			uint32_t pgsz = nvidia_rm_os32_pick_attr_page_size(max_ps,
									  size);
			NvU32 attr = nvidia_rm_os32_attr_sysmem_mappable(pgsz, wc);
			ret = nvidia_rm_memory_alloc_raw(device->fd_ctl,
							 device->h_client,
							 h_parent, &bo->rm_handle,
							 NV01_MEMORY_SYSTEM,
							 device->h_client,
							 bo->rm_type, flags,
							 attr, 0, size, align,
							 &bo->gpu_offset,
							 &bo->limit);
			/* Fallback: strict 4K WC/uncached contig defaults */
			if (ret != 0) {
				attr = wc ? NV_OS32_ATTR_PCI_4K_WRITECOMBINE
					  : NV_OS32_ATTR_PCI_4K_UNCACHED;
				ret = nvidia_rm_memory_alloc_raw(
					device->fd_ctl, device->h_client,
					h_parent, &bo->rm_handle,
					NV01_MEMORY_SYSTEM, device->h_client,
					bo->rm_type, flags, attr, 0, size,
					align, &bo->gpu_offset, &bo->limit);
			}
		}
		if (ret != 0) {
			ret = nvidia_rm_vidheap_alloc_raw(device->fd_ctl,
							  device->h_client,
							  h_parent, bo->rm_type,
							  flags, size, align,
							  NV_OS32_ATTR_PCI_4K_UNCACHED,
							  0, &bo->rm_handle,
							  &bo->gpu_offset,
							  &bo->limit);
		}
		if (ret != 0) {
			free(bo);
			return ret;
		}
		bo->allocated = true;
		bo->cpu_accessible = (req->flags & NVIDIA_BO_FLAGS_NO_CPU_ACCESS) == 0;
		break;
	}

	/* Eager CPU map if requested */
	if (bo->cpu_accessible && (req->flags & NVIDIA_BO_FLAGS_CPU_ACCESS)) {
		ret = nvidia_rm_map_memory_raw(device->fd_ctl, device->h_client,
					       device->h_device, bo->rm_handle,
					       0, size, &bo->cpu_ptr, 0);
		if (ret == 0)
			bo->cpu_mapped = true;
		/* Map failure is non-fatal; caller can retry via cpu_map */
	}

	*bo_out = bo;
	return 0;
}

int
nvidia_bo_free(nvidia_bo_handle bo_handle)
{
	return nvidia_bo_unref(bo_handle);
}

static void
nvidia_bo_destroy(struct nvidia_bo *bo)
{
	struct nvidia_device *dev;

	if (!bo)
		return;
	dev = bo->dev;

	if (bo->cpu_mapped && bo->cpu_ptr) {
		nvidia_rm_unmap_memory_raw(dev->fd_ctl, dev->h_client,
					   dev->h_device, bo->rm_handle,
					   bo->cpu_ptr, 0);
		bo->cpu_mapped = false;
		bo->cpu_ptr = NULL;
	}

	if (bo->allocated && bo->rm_handle) {
		/* Try vidheap free first, then generic RM free */
		if (nvidia_rm_vidheap_free_raw(dev->fd_ctl, dev->h_client,
					       bo->h_parent, bo->rm_handle) != 0) {
			nvidia_rm_free_raw(dev->fd_ctl, dev->h_client,
					   bo->h_parent, bo->rm_handle);
		}
	}

	pthread_mutex_destroy(&bo->map_mutex);
	free(bo);
}

int
nvidia_bo_ref(nvidia_bo_handle bo_handle)
{
	struct nvidia_bo *bo = bo_handle;

	if (!bo)
		return -EINVAL;
	atomic_inc(&bo->refcount);
	return 0;
}

int
nvidia_bo_unref(nvidia_bo_handle bo_handle)
{
	struct nvidia_bo *bo = bo_handle;

	if (!bo)
		return -EINVAL;
	if (atomic_dec_and_test(&bo->refcount))
		nvidia_bo_destroy(bo);
	return 0;
}

int
nvidia_bo_query_metadata(nvidia_bo_handle bo_handle,
			 struct nvidia_bo_metadata *meta)
{
	struct nvidia_bo *bo = bo_handle;

	if (!bo || !meta)
		return -EINVAL;

	memset(meta, 0, sizeof(*meta));
	meta->size = bo->size;
	meta->aligned_size = bo->aligned_size;
	meta->gpu_offset = bo->gpu_offset;
	meta->rm_handle = bo->rm_handle;
	meta->domain = bo->domain;
	meta->flags = bo->flags;
	meta->cpu_accessible = bo->cpu_accessible;
	return 0;
}

int
nvidia_bo_cpu_map(nvidia_bo_handle bo_handle, void **cpu_ptr)
{
	struct nvidia_bo *bo = bo_handle;
	int ret = 0;

	if (!bo || !cpu_ptr)
		return -EINVAL;
	if (!bo->cpu_accessible)
		return -EACCES;

	pthread_mutex_lock(&bo->map_mutex);
	if (!bo->cpu_mapped) {
		ret = nvidia_rm_map_memory_raw(bo->dev->fd_ctl, bo->dev->h_client,
					       bo->dev->h_device, bo->rm_handle,
					       0, bo->aligned_size,
					       &bo->cpu_ptr, 0);
		if (ret == 0)
			bo->cpu_mapped = true;
	}
	if (ret == 0)
		*cpu_ptr = bo->cpu_ptr;
	pthread_mutex_unlock(&bo->map_mutex);
	return ret;
}

int
nvidia_bo_cpu_unmap(nvidia_bo_handle bo_handle)
{
	struct nvidia_bo *bo = bo_handle;
	int ret = 0;

	if (!bo)
		return -EINVAL;

	pthread_mutex_lock(&bo->map_mutex);
	if (bo->cpu_mapped && bo->cpu_ptr) {
		ret = nvidia_rm_unmap_memory_raw(bo->dev->fd_ctl, bo->dev->h_client,
						 bo->dev->h_device, bo->rm_handle,
						 bo->cpu_ptr, 0);
		if (ret == 0) {
			bo->cpu_mapped = false;
			bo->cpu_ptr = NULL;
		}
	}
	pthread_mutex_unlock(&bo->map_mutex);
	return ret;
}

int
nvidia_bo_export_dmabuf(nvidia_bo_handle bo_handle, int *dmabuf_fd_out)
{
	struct nvidia_bo *bo = bo_handle;
	NvHandle handles[1];
	NvU64 offsets[1];
	NvU64 sizes[1];

	if (!bo || !dmabuf_fd_out)
		return -EINVAL;

	handles[0] = bo->rm_handle;
	offsets[0] = 0;
	sizes[0] = bo->aligned_size;

	return nvidia_rm_export_dmabuf_raw(bo->dev->fd_ctl, bo->dev->h_client,
					   handles, offsets, sizes, 1,
					   bo->aligned_size, dmabuf_fd_out);
}

uint32_t
nvidia_bo_get_rm_handle(nvidia_bo_handle bo_handle)
{
	return bo_handle ? bo_handle->rm_handle : 0;
}
