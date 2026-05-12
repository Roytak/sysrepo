/**
 * @file sysrepo-notifd.h
 * @author Roman Janota <Roman.Janota@cesnet.cz>
 * @brief header of common functions for sysrepo-notifd
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

#ifndef SYSREPO_NOTIFD_H_
#define SYSREPO_NOTIFD_H_

#include <pthread.h>
#include <stdint.h>
#include <time.h>

#include "compat.h"
#include "sysrepo.h"
#include "sysrepo_types.h"

/**
 * @brief Logging macros for sysrepo-notifd.
 */
#define SRNTF_LOG_INF(...) SRPLG_LOG_INF("sysrepo-notifd", __VA_ARGS__)
#define SRNTF_LOG_WRN(...) SRPLG_LOG_WRN("sysrepo-notifd", __VA_ARGS__)
#define SRNTF_LOG_ERR(...) SRPLG_LOG_ERR("sysrepo-notifd", __VA_ARGS__)
#define SRNTF_LOG_DBG(...) SRPLG_LOG_DBG("sysrepo-notifd", __VA_ARGS__)

#define ERRMEM SRNTF_LOG_ERR("Memory allocation failed (%s:%d).", __FILE__, __LINE__)

#define CHECK_ERRMEM_GOTO(_ptr, _ret, _label) \
    do { \
        if (!(_ptr)) { \
            _ret = SR_ERR_NO_MEMORY; \
            ERRMEM; \
            goto _label; \
        } \
    } while (0)

#define CHECK_ERRMEM_RET(_ptr) \
    do { \
        if (!(_ptr)) { \
            ERRMEM; \
            return SR_ERR_NO_MEMORY; \
        } \
    } while (0)

/**
 * @brief Timeout for acquiring a subscription mutex lock, in milliseconds.
 *
 * Subscription critical sections may involve I/O operations (e.g., sending
 * notifications via UDP) and sysrepo API calls (e.g., dispatch start/stop),
 * so a longer timeout is appropriate.
 */
#define NOTIFD_SUB_LOCK_TIMEOUT_MS 10000

/**
 * @brief Timeout for acquiring a receiver instance mutex lock, in milliseconds.
 *
 * Receiver instance critical sections only perform quick in-memory
 * configuration field updates (address, port, segmentation, etc.),
 * so a shorter timeout suffices.
 */
#define NOTIFD_RECV_INST_LOCK_TIMEOUT_MS 5000

/**
 * @brief Timeout for acquiring the main context config rwlock, in milliseconds.
 *
 * The main context config lock is used to synchronize access to the entire configuration,
 * so it may be held for longer periods during e.g. connection creation.
 */
#define NOTIFD_CONTEXT_LOCK_TIMEOUT_MS 10000

/**
 * @brief Base delay in seconds for receiver reconnect exponential backoff.
 */
#define NOTIFD_RECV_RECONNECT_BASE_SEC 1

/**
 * @brief Maximum delay in seconds for receiver reconnect exponential backoff.
 */
#define NOTIFD_RECV_RECONNECT_MAX_SEC 60

/* forward declarations */
typedef struct notifd_ctx_s notifd_ctx_t;
typedef struct notif_sub_s notif_sub_t;
typedef struct notif_receiver_s notif_receiver_t;

/**
 * @brief State of a configured subscription.
 */
typedef enum {
    NOTIF_SUB_STATE_INVALID = 0,    /**< subscription parameters are not supportable */
    NOTIF_SUB_STATE_VALID,          /**< subscription is supportable with current parameters */
    NOTIF_SUB_STATE_CONCLUDED       /**< subscription has hit stop time, no active/suspended receivers */
} notif_sub_state_t;

/**
 * @brief State of a notification receiver within a subscription.
 */
typedef enum {
    NOTIF_RECV_STATE_CONNECTING = 0,    /**< awaiting initial connection and subscription-started delivery */
    NOTIF_RECV_STATE_DISCONNECTED,      /**< connection attempt failed, not currently reconnecting */
    NOTIF_RECV_STATE_ACTIVE,            /**< receiver is connected and receiving notifications */
    NOTIF_RECV_STATE_SUSPENDED          /**< publisher unable to provide notifications (not yet implemented) */
} notif_recv_state_t;

/**
 * @brief Notification transport type.
 */
typedef enum {
    NOTIF_TRANSPORT_TYPE_NONE = 0,  /**< no transport configured */
    NOTIF_TRANSPORT_TYPE_UDP        /**< UDP transport */
} notif_transport_type_t;

/**
 * @brief Notification encoding type.
 */
typedef enum {
    NOTIF_ENCODING_UNSET = 0,   /**< not set, left to the underlying transport default */
    NOTIF_ENCODING_XML,         /**< XML encoding */
    NOTIF_ENCODING_JSON,        /**< JSON encoding */
    NOTIF_ENCODING_CBOR         /**< CBOR encoding */
} notif_encoding_t;

/**
 * @brief UDP notification receiver configuration.
 *
 * Corresponds to /ietf-subscribed-notifications:receiver-instances/receiver-instance/ietf-udp-notif-transport:udp-notif-receiver
 */
typedef struct udp_notif_receiver_s {
    char *remote_address;       /**< destination address (hostname or IP) */
    uint16_t remote_port;       /**< destination UDP port */

    int enable_segmentation;    /**< whether payload segmentation is enabled */
    uint16_t max_segment_size;  /**< maximum segment size in bytes */

    ATOMIC_T message_id;        /**< monotonically increasing message ID, starts at 1 */
    uint32_t publisher_id;      /**< message publisher ID, identifies this publisher instance */
} udp_notif_receiver_t;

/**
 * @brief Receiver instance shared by subscriptions.
 * Corresponds to /ietf-subscribed-notifications:receiver-instances/receiver-instance.
 *
 * Holds transport configuration and is referenced by subscription receivers (by name).
 */
typedef struct notif_receiver_inst_s {
    char *name;                     /**< receiver instance name */

    int modified;                   /**< whether the instance was modified and needs reconnect */

    notif_transport_type_t type;    /**< configured transport type */

    union {
        udp_notif_receiver_t udp;   /**< UDP transport configuration */
    };
} notif_receiver_inst_t;

/**
 * @brief UDP transport connection context for a notification receiver.
 */
typedef struct {
    int sockfd;     /**< connected UDP socket file descriptor, -1 if not connected */
} udp_conn_ctx_t;

/**
 * @brief Callback data passed to ::srsn_notif_cb().
 */
typedef struct notif_cb_data_s {
    notifd_ctx_t *ctx;          /**< main daemon context */
    notif_receiver_t *recv;     /**< receiver to send the notification to */
    notif_encoding_t encoding;  /**< notification encoding to use for sending */
} notif_cb_data_t;

/**
 * @brief Receiver within a subscription.
 * Corresponds to /ietf-subscribed-notifications:subscriptions/subscription/receivers/receiver
 *
 * References a receiver instance, which holds the actual transport configuration.
 */
typedef struct notif_receiver_s {
    char *name;                                 /**< receiver name */

    notif_recv_state_t state;                   /**< current receiver state */
    notif_receiver_inst_t *inst;                /**< resolved reference to the receiver instance */

    notif_transport_type_t transport;           /**< transport type, copied from the instance for quick access */

    union {
        udp_conn_ctx_t udp;     /**< UDP socket connection context */
    } conn_ctx;                                 /**< transport-specific connection context */

    struct {
        sr_subscription_ctx_t *sr_subscr;   /**< sysrepo subscription context */
        uint32_t sub_id;                    /**< srsn subscription ID */
        int fd;                             /**< notification pipe FD from srsn_subscribe */
    } srsn_data;                                /**< srsn subscription and dispatch data */
    notif_cb_data_t cb_data;                    /**< callback data for srsn dispatch */

    notif_sub_t *sub;                           /**< back-pointer to the parent subscription */
    struct timespec last_reconnect_attempt;     /**< time of the last reconnect attempt */
    uint32_t reconnect_attempts;                /**< number of consecutive failed reconnect attempts */

} notif_receiver_t;

/**
 * @brief Configured subscription context.
 * Corresponds to /ietf-subscribed-notifications:subscriptions/subscription
 */
typedef struct notif_sub_s {
    uint32_t id;                        /**< subscription ID */

    notif_sub_state_t state;            /**< subscription state */

    int resubscribe;                    /**< whether the subscription needs to be resubscribed to apply changes */
    int modified;                       /**< whether the subscription was modified */
    const char *modif_err_reason;       /**< YANG identity-ref reason the subscription is invalid, used in subscription-terminated */

    char *stream;                       /**< stream name */
    char *filter_ref;                   /**< optional stream filter name (e.g., XPath or subtree filter) */
    char *xpath_filter;                 /**< optional XPath filter */
    notif_encoding_t encoding;          /**< notification encoding */
    struct timespec stop_time;          /**< optional stop time */
    int replay;                         /**< whether to replay notifications */
    struct timespec start_time;         /**< requested replay start time for configured-replay */
    struct timespec replay_start_time;  /**< actual replay start time returned by srsn_subscribe */
    char *purpose;                      /**< purpose of the subscription */
    char *local_address;                /**< local address to send notifications from (NULL means OS default) */

    notif_receiver_t *receivers;        /**< receivers of this subscription (sized-array, see libyang docs) */
} notif_sub_t;

/**
 * @brief Main daemon context.
 */
struct notifd_ctx_s {
    sr_session_ctx_t *sr_sess;              /**< sysrepo session used by the daemon */
    pthread_rwlock_t state_rwlock;          /**< synchronize access to daemon shared state (subscriptions,
                                                 receiver instances, and related runtime fields) */
    pthread_mutex_t config_apply_mutex;     /**< serialize config-apply operations; keeps a single apply
                                                 transaction active so state_rwlock write-lock ownership
                                                 cannot be stolen across temporary unlock/relock windows */

    notif_sub_t **subs;                     /**< configured subscriptions (sized-array, see libyang docs) */
    notif_receiver_inst_t **recv_insts;     /**< configured receiver instances (sized-array, see libyang docs) */
};

/*
 * General utility functions
*/

/**
 * @brief Acquire a mutex lock with optional timeout and error reporting.
 *
 * @param[in] mutex Mutex to lock.
 * @param[in] timeout_ms Timeout in milliseconds, 0 for blocking.
 * @param[in] func Calling function name for error reporting.
 * @return SR_ERR_OK on success, SR_ERR_TIME_OUT on timeout, SR_ERR_INTERNAL on other errors.
 */
int notifd_mutex_lock(pthread_mutex_t *mutex, uint32_t timeout_ms, const char *func);

/**
 * @brief Unlock a mutex lock.
 *
 * @param[in] mutex Mutex to unlock.
 * @param[in] func Calling function name for error reporting.
 */
void notifd_mutex_unlock(pthread_mutex_t *mutex, const char *func);

/**
 * @brief Acquire a read or write lock on a rwlock with optional timeout and error reporting.
 *
 * @param[in] lock RW lock to lock.
 * @param[in] is_write Whether to acquire a write lock (1) or read lock (0).
 * @param[in] timeout_ms Timeout in milliseconds, 0 for blocking.
 * @param[in] func Calling function name for error reporting.
 * @return SR_ERR_OK on success, SR_ERR_TIME_OUT on timeout, SR_ERR_INTERNAL on other errors.
 */
int notifd_rwlock_lock(pthread_rwlock_t *lock, int is_write, uint32_t timeout_ms, const char *func);

/**
 * @brief Unlock a rwlock.
 *
 * @param[in] lock RW lock to unlock.
 * @param[in] func Calling function name for error reporting.
 */
void notifd_rwlock_unlock(pthread_rwlock_t *lock, const char *func);

/*
 * UDP transport functions (sysrepo-notifd-udp.c)
 */

/**
 * @brief Connect a UDP notification receiver.
 *
 * @param[in] sub Subscription to connect the receiver instance for.
 * @param[in] recv Receiver to connect the instance for.
 * @param[in] udp_recv UDP receiver instance to connect.
 * @return SR_ERR_OK on success, error code on failure.
 */
int udp_notif_receiver_connect(notif_sub_t *sub, notif_receiver_t *recv, udp_notif_receiver_t *udp_recv);

/**
 * @brief Disconnect a UDP notification receiver.
 *
 * @param[in] recv Receiver to disconnect the instance for.
 */
void udp_notif_receiver_disconnect(notif_receiver_t *recv);

/**
 * @brief Send a notification via UDP-Notif protocol.
 *
 * @param[in] recv Notification receiver. Must be a UDP transport instance and must be connected.
 * @param[in] udp_recv UDP receiver instance to send the notification through.
 * @param[in] notif Notification data tree to send.
 * @param[in] timestamp Notification timestamp.
 * @param[in] encoding Encoding to use for the notification message.
 * @return SR_ERR_OK on success, error code on failure.
 */
int udp_notif_receiver_send(notif_receiver_t *recv, udp_notif_receiver_t *udp_recv, const struct lyd_node *notif,
        const struct timespec *timestamp, notif_encoding_t encoding);

#endif /* SYSREPO_NOTIFD_H_ */
