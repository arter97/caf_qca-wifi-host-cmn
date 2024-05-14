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

#if  !defined(_I_QDF_PAGE_POOL_H)
#define _I_QDF_PAGE_POOL_H

#include <linux/version.h>
#include "qdf_types.h"
#include "i_qdf_trace.h"

#ifdef DP_FEATURE_RX_BUFFER_RECYCLE

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
#include <net/page_pool/helpers.h>
#include <net/page_pool/types.h>
#else
#include <net/page_pool.h>
#endif

typedef struct page_pool *__qdf_page_pool_t;

/**
 * __qdf_page_pool_create() - Create page_pool
 *
 * @osdev: Device handle
 * @pool_size: Pool Size
 * @pp_page_size: Page pool page size
 *
 * Return: Page Pool Reference
 */
__qdf_page_pool_t __qdf_page_pool_create(qdf_device_t osdev, size_t pool_size,
					 size_t pp_page_size);

/**
 * __qdf_page_pool_destroy() - Destroy page_pool
 * @pp: Page Pool Reference
 *
 * Return: None
 */
void __qdf_page_pool_destroy(__qdf_page_pool_t pp);

#else

typedef void *__qdf_page_pool_t;

/**
 * __qdf_page_pool_create() - Create page_pool
 *
 * @osdev: Device handle
 * @pool_size: Pool Size
 * @pp_page_size: Page pool page size
 *
 * Return: Page Pool Reference
 */
static inline __qdf_page_pool_t
__qdf_page_pool_create(qdf_device_t osdev, size_t pool_size,
		       size_t pp_page_size)
{
	return NULL;
}

/**
 * __qdf_page_pool_destroy() - Destroy page_pool
 * @pp: Page Pool Reference
 *
 * Return: None
 */
static inline void __qdf_page_pool_destroy(__qdf_page_pool_t pp)
{
}
#endif /* DP_FEATURE_RX_BUFFER_RECYCLE */
#endif /* _I_QDF_PAGE_POOL_H */
