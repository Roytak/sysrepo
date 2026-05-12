/**
 * @file sysrepo-notifd_internal.h
 * @author Roman Janota <Roman.Janota@cesnet.cz>
 * @brief internal declarations shared between sysrepo-notifd source files
 *
 * @copyright
 * Copyright (c) 2026 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#ifndef SYSREPO_NOTIFD_INTERNAL_H_
#define SYSREPO_NOTIFD_INTERNAL_H_

#include "compat.h"

#include <stdint.h>

#include "sysrepo.h"
#include "sysrepo-notifd.h"

#include <libyang/libyang.h>

/* field presence bitmask for subscription state-change notifications */
#define NOTIF_FIELD_STREAM         0x01
#define NOTIF_FIELD_XPATH_FILTER   0x02
#define NOTIF_FIELD_STOP_TIME      0x04
#define NOTIF_FIELD_REPLAY_START   0x08

/*
 * ---------------------------------------------------------------------------
 * Functions from sysrepo-notifd-config.c
 * ---------------------------------------------------------------------------
 */

/* helpers */
int get_descendant_mandatory(const struct lyd_node *ctx_node, const char *path, struct lyd_node **match);
const char *subscription_state2str(notif_sub_state_t state);
const char *receiver_state2str(notif_recv_state_t state);

/* find */
notif_sub_t *subscription_find_by_id(notifd_ctx_t *ctx, uint32_t sub_id);
notif_sub_t *subscription_find_by_node(notifd_ctx_t *notifd_ctx, const struct lyd_node *ctx_node);
notif_receiver_t *receiver_find_by_name(notif_sub_t *sub, const char *name);
notif_receiver_t *receiver_find_by_node(notifd_ctx_t *notifd_ctx, const struct lyd_node *ctx_node);
notif_receiver_inst_t *receiver_inst_find_by_name(notifd_ctx_t *ctx, const char *name);
notif_receiver_inst_t *receiver_inst_find_by_node(notifd_ctx_t *notifd_ctx, const struct lyd_node *ctx_node);

/* subscription CRUD */
int subscription_create_from_node(notifd_ctx_t *notifd_ctx, const struct lyd_node *node);
int subscription_destroy_from_node(notifd_ctx_t *notifd_ctx, const struct lyd_node *node);

/* receiver CRUD */
int receiver_create_from_node(notifd_ctx_t *notifd_ctx, notif_sub_t *sub, const struct lyd_node *node);
int receiver_destroy_from_node(notifd_ctx_t *notifd_ctx, notif_sub_t *sub, const struct lyd_node *node);

/* receiver instance CRUD */
int receiver_instance_create_from_node(notifd_ctx_t *notifd_ctx, const struct lyd_node *node);
int receiver_instance_destroy_from_node(notifd_ctx_t *notifd_ctx, const struct lyd_node *node);

/* subscription field change handlers */
int handle_stream(notif_sub_t *sub, const struct lyd_node *node, sr_change_oper_t op);
int handle_stream_filter_name(notifd_ctx_t *notifd_ctx, notif_sub_t *sub, const struct lyd_node *node, sr_change_oper_t op);
int handle_stream_subtree_filter(notifd_ctx_t *notifd_ctx, notif_sub_t *sub, const struct lyd_node *node, sr_change_oper_t op);
int handle_stream_xpath_filter(notif_sub_t *sub, const struct lyd_node *node, sr_change_oper_t op);
int handle_encoding(notif_sub_t *sub, const struct lyd_node *node, sr_change_oper_t op);
int handle_stop_time(notif_sub_t *sub, const struct lyd_node *node, sr_change_oper_t op);
int handle_configured_replay(notifd_ctx_t *notifd_ctx, notif_sub_t *sub, const struct lyd_node *node, sr_change_oper_t op);
int handle_purpose(notif_sub_t *sub, const struct lyd_node *node, sr_change_oper_t op);
int handle_source_address(notifd_ctx_t *notifd_ctx, notif_sub_t *sub, const struct lyd_node *node, sr_change_oper_t op);

/* receiver field change handler */
int handle_receiver_instance_ref(notifd_ctx_t *notifd_ctx, notif_sub_t *sub, const struct lyd_node *node, sr_change_oper_t op);

/* receiver instance (UDP) field change handlers */
int handle_udp_notif_receiver(notif_receiver_inst_t *recv_inst, const struct lyd_node *node, sr_change_oper_t op);
int handle_udp_remote_address(notif_receiver_inst_t *recv_inst, const struct lyd_node *node, sr_change_oper_t op);
int handle_udp_remote_port(notif_receiver_inst_t *recv_inst, const struct lyd_node *node, sr_change_oper_t op);
int handle_udp_enable_segmentation(notif_receiver_inst_t *recv_inst, const struct lyd_node *node, sr_change_oper_t op);
int handle_udp_max_segment_size(notif_receiver_inst_t *recv_inst, const struct lyd_node *node, sr_change_oper_t op);

/* filter change handler (used by filter_change_cb) */
int handle_stream_filter(notifd_ctx_t *notifd_ctx, const struct lyd_node *node, int is_subtree);

/* post-processing after config changes */
void process_modified_subscriptions(notifd_ctx_t *notifd_ctx);
void process_modified_receiver_instances(notifd_ctx_t *notifd_ctx);
int subscription_resubscribe(notifd_ctx_t *notifd_ctx, notif_sub_t *sub);

/* feature check */
int module_feature_is_enabled(notifd_ctx_t *notifd_ctx, const char *module_name, const char *feature_name, int *enabled);

/*
 * ---------------------------------------------------------------------------
 * Functions from sysrepo-notifd-runtime.c
 * ---------------------------------------------------------------------------
 */

/* helper */
int timespec_cmp(const struct timespec *ts1, const struct timespec *ts2);

/* notification send (to one or all receivers of a subscription) */
int subscription_started_notif_send(notifd_ctx_t *notifd_ctx, notif_sub_t *sub, notif_receiver_t *receiver);
int subscription_terminated_notif_send(notifd_ctx_t *notifd_ctx, notif_sub_t *sub, notif_receiver_t *receiver, const char *reason);
int subscription_modified_notif_send(notifd_ctx_t *notifd_ctx, notif_sub_t *sub, notif_receiver_t *receiver);
int subscription_completed_notif_send(notifd_ctx_t *notifd_ctx, notif_sub_t *sub, notif_receiver_t *receiver);

/* receiver connection management */
int notif_receiver_is_connected(notif_receiver_t *receiver);
int notif_receiver_connect(notif_sub_t *sub, notif_receiver_t *receiver);
void notif_receiver_disconnect(notif_receiver_t *receiver);
int notif_receiver_backoff_reconnect(notifd_ctx_t *notifd_ctx, notif_receiver_t *receiver);
int notif_receiver_reconnect(notifd_ctx_t *notifd_ctx, notif_sub_t *sub, notif_receiver_t *receiver, notif_receiver_inst_t *new_inst);
int notif_receiver_send(notifd_ctx_t *notifd_ctx, notif_receiver_t *receiver, const struct lyd_node *notif,
        const struct timespec *timestamp, notif_encoding_t encoding);

/* notification dispatch (srsn integration) */
int notification_dispatch_start(notifd_ctx_t *notifd_ctx, notif_sub_t *sub, notif_receiver_t *receiver);
void notification_dispatch_stop(notifd_ctx_t *notifd_ctx, notif_receiver_t *receiver);

/* main notification callback (passed to srsn_read_dispatch_init) */
void notifd_notification_cb(const struct lyd_node *notif, const struct timespec *timestamp, void *cb_data);

#endif /* SYSREPO_NOTIFD_INTERNAL_H_ */
