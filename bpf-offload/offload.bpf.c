// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/* Copyright (c) 2023 Red Hat */

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include <linux/netdevice.h>
#include <stdbool.h>
#include <errno.h>

#include "offload.h"

char LICENSE[] SEC("license") = "Dual BSD/GPL";

struct net_device {
	char name[IFNAMSIZ];
} __attribute__((preserve_access_index));

enum flow_action_hw_stats {
	FLOW_ACTION_HW_STATS_IMMEDIATE = 1,
	FLOW_ACTION_HW_STATS_DELAYED = 2,
	FLOW_ACTION_HW_STATS_DISABLED = 4
};

struct flow_stats {
	__u64	pkts;
	__u64	bytes;
	__u64	drops;
	__u64	lastused;
	enum flow_action_hw_stats used_hw_stats;
	bool used_hw_stats_valid;
} __attribute__((preserve_access_index));

enum flow_cls_command {
	FLOW_CLS_REPLACE,
	FLOW_CLS_DESTROY,
	FLOW_CLS_STATS,
	FLOW_CLS_TMPLT_CREATE,
	FLOW_CLS_TMPLT_DESTROY,
};

struct flow_cls_offload {
	enum flow_cls_command command;
	bool use_act_stats;
	struct flow_stats stats;
} __attribute__((preserve_access_index));

enum tc_setup_type {
	TC_QUERY_CAPS,
	TC_SETUP_QDISC_MQPRIO,
	TC_SETUP_CLSU32,
	TC_SETUP_CLSFLOWER,
	TC_SETUP_CLSMATCHALL,
	TC_SETUP_CLSBPF,
	TC_SETUP_BLOCK,
	TC_SETUP_QDISC_CBS,
	TC_SETUP_QDISC_RED,
	TC_SETUP_QDISC_PRIO,
	TC_SETUP_QDISC_MQ,
	TC_SETUP_QDISC_ETF,
	TC_SETUP_ROOT_QDISC,
	TC_SETUP_QDISC_GRED,
	TC_SETUP_QDISC_TAPRIO,
	TC_SETUP_FT,
	TC_SETUP_QDISC_ETS,
	TC_SETUP_QDISC_TBF,
	TC_SETUP_QDISC_FIFO,
	TC_SETUP_QDISC_HTB,
	TC_SETUP_ACT,
};

struct net_device_hw_ops {
	void (*offload)(struct net_device *dev,
			struct flow_cls_offload *off);
	int (*setup_tc)(struct net_device *dev,
			enum tc_setup_type type,
			void *type_data);
	int (*setup_ft)(enum tc_setup_type type,
			void *type_data,
			void *cb_priv);
	char name[16];
};

int flow_setup(struct flow_cls_offload *f,
	       void *cb_priv)
{
	bpf_printk("flow_setup command=%d", f->command);

	switch (f->command) {
	case FLOW_CLS_REPLACE:
	case FLOW_CLS_DESTROY:
		return 0;
	case FLOW_CLS_STATS:
		f->stats.pkts += 1;
		f->stats.bytes += 10;
		f->stats.lastused = bpf_jiffies64();
		f->stats.used_hw_stats = FLOW_ACTION_HW_STATS_DELAYED;
		f->stats.used_hw_stats_valid = true;
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}

SEC("struct_ops/setup_ft")
int BPF_PROG(setup_ft,
	     enum tc_setup_type type,
	     void *type_data,
	     void *cb_priv)
{
	switch (type) {
	case TC_SETUP_FT:
	case TC_SETUP_CLSFLOWER:
		return flow_setup(type_data, cb_priv);
	default:
		return -EOPNOTSUPP;
	}
}

SEC(".struct_ops.link")
struct net_device_hw_ops hwops = {
	.setup_ft = (void *)setup_ft,
	.name = "netdev_hwops",
};
