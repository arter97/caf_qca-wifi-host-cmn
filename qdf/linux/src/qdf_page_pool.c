/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <qdf_page_pool.h>

__qdf_page_pool_t __qdf_page_pool_create(qdf_device_t osdev, size_t pool_size,
					 size_t pp_page_size)
{
	struct page_pool_params pp_params = {0};
	struct page_pool *pp;
	int ret;

	pp_params.order = get_order(pp_page_size);
	pp_params.flags = PP_FLAG_DMA_MAP | PP_FLAG_DMA_SYNC_DEV,
	pp_params.nid = NUMA_NO_NODE,
	pp_params.dev = osdev->dev,
	pp_params.dma_dir = DMA_FROM_DEVICE,
	pp_params.offset = 0,
	pp_params.max_len = pp_page_size;

	if (pp_params.order > 1)
		pp_params.flags |= PP_FLAG_PAGE_FRAG;

	pp = page_pool_create(&pp_params);
	if (IS_ERR(pp)) {
		ret = PTR_ERR(pp);
		qdf_rl_nofl_err("Failed to create page pool: %d", ret);
		return NULL;
	}

	return pp;
}

void __qdf_page_pool_destroy(__qdf_page_pool_t pp)
{
	return page_pool_destroy(pp);
}
