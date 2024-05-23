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

#if  !defined(_QDF_PAGE_POOL_H)
#define _QDF_PAGE_POOL_H

#include <i_qdf_page_pool.h>

typedef __qdf_page_pool_t qdf_page_pool_t;

/**
 * qdf_page_pool_create() - Create page_pool
 *
 * @osdev: Device handle
 * @pool_size: Pool Size
 * @pp_page_size: Page pool page size
 *
 * Return: Page Pool Reference
 */
static inline qdf_page_pool_t
qdf_page_pool_create(qdf_device_t osdev, size_t pool_size, size_t pp_page_size)
{
	return __qdf_page_pool_create(osdev, pool_size, pp_page_size);
}

/**
 * qdf_page_pool_destroy() - Destroy page_pool
 * @pp: Page Pool Reference
 *
 * Return: None
 */
static inline void qdf_page_pool_destroy(qdf_page_pool_t pp)
{
	return __qdf_page_pool_destroy(pp);
}
#endif /* _QDF_PAGE_POOL_H */
