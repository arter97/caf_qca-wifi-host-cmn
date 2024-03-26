/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

/**
 * DOC: This file contains chipset stats implementstion
 */

#ifdef WLAN_CHIPSET_STATS
#include <qdf_mem.h>
#include <qdf_types.h>
#include <qdf_status.h>
#include <qdf_trace.h>
#include <qdf_time.h>
#include <qdf_mc_timer.h>
#include <qdf_lock.h>
#include <qdf_str.h>
#include <qdf_module.h>
#include <wlan_nlink_common.h>
#include <wlan_cp_stats_chipset_stats.h>

struct chipset_stats cstats;
static struct cstats_node *gcstats_buffer[CSTATS_MAX_TYPE];

QDF_STATUS wlan_cp_stats_cstats_init(void)
{
	qdf_list_node_t *tmp_node = NULL;
	int i, j, k;

	for (i = 0; i < CSTATS_MAX_TYPE; i++) {
		qdf_spinlock_create(&cstats.cstats_lock[i]);

		gcstats_buffer[i] = qdf_mem_valloc(MAX_CSTATS_NODE_COUNT *
						   sizeof(struct cstats_node));
		if (!gcstats_buffer[i]) {
			qdf_err("Could not allocate memory for chipset stats");
			for (k = 0; k < i ; k++) {
				qdf_spin_lock_bh(&cstats.cstats_lock[k]);
				cstats.ccur_node[k] = NULL;
				qdf_spin_unlock_bh(&cstats.cstats_lock[k]);

				if (gcstats_buffer[k])
					qdf_mem_vfree(gcstats_buffer[k]);
			}

			return QDF_STATUS_E_NOMEM;
		}

		qdf_mem_zero(gcstats_buffer[i], MAX_CSTATS_NODE_COUNT *
			     sizeof(struct cstats_node));

		qdf_spin_lock_bh(&cstats.cstats_lock[i]);
		qdf_init_list_head(&cstats.cstat_free_list[i].anchor);
		qdf_init_list_head(&cstats.cstat_filled_list[i].anchor);

		for (j = 0; j < MAX_CSTATS_NODE_COUNT; j++) {
			qdf_list_insert_front(&cstats.cstat_free_list[i],
					      &gcstats_buffer[i][j].node);
			gcstats_buffer[i][j].index = j;
		}

		qdf_list_remove_front(&cstats.cstat_free_list[i], &tmp_node);

		cstats.ccur_node[i] =
			qdf_container_of(tmp_node, struct cstats_node, node);
		cstats.cstats_no_flush[i] = true;
		qdf_spin_unlock_bh(&cstats.cstats_lock[i]);
	}

	return 0;
}

void wlan_cp_stats_cstats_deinit(void)
{
	int i;

	for (i = 0; i < CSTATS_MAX_TYPE; i++) {
		qdf_spin_lock_bh(&cstats.cstats_lock[i]);
		cstats.ccur_node[i] = NULL;
		cstats.cstat_drop_cnt[i] = 0;
		qdf_spin_unlock_bh(&cstats.cstats_lock[i]);

		if (gcstats_buffer[i]) {
			qdf_mem_vfree(gcstats_buffer[i]);
			gcstats_buffer[i] = NULL;
		}
	}
}
#endif /* WLAN_CHIPSET_STATS */
