/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef _SPPWK_CMD_UTILS_H_
#define _SPPWK_CMD_UTILS_H_

#include <netinet/in.h>

/**
 * @file cmd_utils.h
 *
 * Command utility functions for SPP worker thread.
 */

#include "shared/common.h"

/**
 * TODO(Yamashita) change type names.
 *  "merge" -> "merger", "forward" -> "forwarder".
 */
/** Identifier string for each component (status command) */
#define SPPWK_TYPE_CLS_STR "classifier_mac"
#define SPPWK_TYPE_MRG_STR "merge"
#define SPPWK_TYPE_FWD_STR "forward"
#define SPPWK_TYPE_MIR_STR "mirror"
#define SPPWK_TYPE_NONE_STR "unuse"

/** Identifier string for each interface */
#define SPP_IFTYPE_NIC_STR   "phy"
#define SPP_IFTYPE_VHOST_STR "vhost"
#define SPP_IFTYPE_RING_STR  "ring"

/** Update wait timer (micro sec) */
#define SPP_CHANGE_UPDATE_INTERVAL 10

#define SPP_INFO_AREA_MAX 2  /* The max number of buffer for management */

#define STR_LEN_SHORT 32  /* Size of short string. */
#define STR_LEN_NAME 128  /* Size of string for names. */

#define SPP_PORT_ABILITY_MAX 4  /* Max num of port abilities. */

/** Maximum VLAN PCP */
#define SPP_VLAN_PCP_MAX 7

/* Max number of core status check */
#define SPP_CORE_STATUS_CHECK_MAX 5

/** Character sting for default port of classifier */
#define SPPWK_TERM_DEFAULT "default"

/** Value for default MAC address of classifier */
#define SPP_DEFAULT_CLASSIFIED_DMY_ADDR     0x010000000000

/** Character sting for default MAC address of classifier */
#define SPP_DEFAULT_CLASSIFIED_DMY_ADDR_STR "00:00:00:00:00:01"

/* Sampling interval timer for latency evaluation */
#define SPP_RING_LATENCY_STATS_SAMPLING_INTERVAL 1000000

/**
 * TODO(Yamashita) change type names.
 *  "merge" -> "merger", "forward" -> "forwarder".
 */
/* Name string for each component */
#define CORE_TYPE_CLASSIFIER_MAC_STR "classifier_mac"
#define CORE_TYPE_MERGE_STR	     "merge"
#define CORE_TYPE_FORWARD_STR	     "forward"
#define CORE_TYPE_MIRROR_STR	     "mirror"

/* State on component */
enum sppwk_lcore_status {
	SPP_CORE_UNUSE,        /**< Not used */
	SPP_CORE_STOP,         /**< Stopped */
	SPP_CORE_IDLE,         /**< Idling */
	SPP_CORE_FORWARD,      /**< Forwarding  */
	SPP_CORE_STOP_REQUEST, /**< Request stopping */
	SPP_CORE_IDLE_REQUEST /**< Request idling */
};

/* Type of SPP worker thread. */
enum sppwk_worker_type {
	SPPWK_TYPE_NONE,  /**< Not used */
	SPPWK_TYPE_CLS,  /**< Classifier_mac */
	SPPWK_TYPE_MRG,  /**< Merger */
	SPPWK_TYPE_FWD,  /**< Forwarder */
	SPPWK_TYPE_MIR,  /**< Mirror */
};

/* Classifier Type */
enum spp_classifier_type {
	SPP_CLASSIFIER_TYPE_NONE, /**< Type none */
	SPP_CLASSIFIER_TYPE_MAC,  /**< MAC address */
	SPP_CLASSIFIER_TYPE_VLAN  /**< VLAN ID */
};

/* Direction of RX or TX on a port. */
enum sppwk_port_dir {
	SPP_PORT_RXTX_NONE, /**< none */
	SPP_PORT_RXTX_RX,   /**< rx port */
	SPP_PORT_RXTX_TX,   /**< tx port */
	SPP_PORT_RXTX_ALL,  /**< rx/tx port */
};

/**
 * Port ability operation which indicates vlan tag operation on the port
 * (e.g. add vlan tag or delete vlan tag)
 */
enum sppwk_port_abl_ops {
	SPPWK_PORT_ABL_OPS_NONE,
	SPPWK_PORT_ABL_OPS_ADD_VLANTAG,
	SPPWK_PORT_ABL_OPS_DEL_VLANTAG,
};

/* getopt_long return value for long option */
enum SPP_LONGOPT_RETVAL {
	SPP_LONGOPT_RETVAL__ = 127,

	/* Return value definition for getopt_long(). Only for long option. */
	SPP_LONGOPT_RETVAL_CLIENT_ID,    /* For `--client-id` */
	SPP_LONGOPT_RETVAL_VHOST_CLIENT  /* For `--vhost-client` */
};

/* Flag of processing type to copy management information */
enum copy_mng_flg {
	COPY_MNG_FLG_NONE,
	COPY_MNG_FLG_UPDCOPY,
	COPY_MNG_FLG_ALLCOPY,
};

/* secondary process type used only from spp_vf and spp_mirror */
/* TODO(yasufum) rename `secondary_type` to `sppwk_proc_type`. */
enum secondary_type {
	SECONDARY_TYPE_NONE,
	SECONDARY_TYPE_VF,
	SECONDARY_TYPE_MIRROR,
};

/** VLAN tag information */
struct spp_vlantag_info {
	int vid; /**< VLAN ID */
	int pcp; /**< Priority Code Point */
	int tci; /**< Tag Control Information */
};

/* Ability for vlantag for a port. */
union spp_ability_data {
	/** VLAN tag information */
	struct spp_vlantag_info vlantag;
};

/* Port ability information. */
struct spp_port_ability {
	enum sppwk_port_abl_ops ops;   /**< Port ability Operations */
	enum sppwk_port_dir dir;  /**< Direction of RX, TX or both */
	union spp_ability_data data;   /**< Port ability data */
};

/* Attributes for classifying. */
struct sppwk_cls_attrs {
	uint64_t mac_addr;  /**< Mac address (binary) */
	char mac_addr_str[STR_LEN_SHORT];  /**< Mac address (text) */
	struct spp_vlantag_info vlantag;   /**< VLAN tag information */
};

/**
 * Simply define type and index of resource UID such as phy:0. For detailed
 * attributions, use `sppwk_port_info` which has additional port params.
 */
struct sppwk_port_idx {
	enum port_type iface_type;  /**< phy, vhost or ring. */
	int iface_no;
};

/* Define detailed port params in addition to `sppwk_port_idx`. */
struct sppwk_port_info {
	enum port_type iface_type;  /**< phy, vhost or ring */
	int iface_no;
	int ethdev_port_id;  /**< Consistent ID of ethdev */
	struct sppwk_cls_attrs cls_attrs;
	struct spp_port_ability ability[SPP_PORT_ABILITY_MAX];
};

/* Attributes of SPP worker thread named as `component`. */
struct sppwk_comp_info {
	char name[STR_LEN_NAME];  /**< Component name */
	enum sppwk_worker_type wk_type;  /**< Type of worker thread */
	unsigned int lcore_id;
	int comp_id;  /**< Component ID */
	int nof_rx;  /**< The number of rx ports */
	int nof_tx;  /**< The number of tx ports */
	struct sppwk_port_info *rx_ports[RTE_MAX_ETHPORTS]; /**< rx ports */
	struct sppwk_port_info *tx_ports[RTE_MAX_ETHPORTS]; /**< tx ports */
};

/* Manage given options as global variable */
struct startup_param {
	int client_id;  /* Client ID */
	char server_ip[INET_ADDRSTRLEN];  /* IP address of spp-ctl */
	int server_port;   /* Port Number of spp-ctl */
	int vhost_client;  /* Flag for --vhost-client option */
	enum secondary_type secondary_type;
};

/* Manage number of interfaces  and port information as global variable. */
struct iface_info {
	int nof_phys;    /* Number of phy ports */
	int nof_vhosts;  /* Number of vhost ports */
	int nof_rings;   /* Number of ring ports */
	struct sppwk_port_info nic[RTE_MAX_ETHPORTS];
	struct sppwk_port_info vhost[RTE_MAX_ETHPORTS];
	struct sppwk_port_info ring[RTE_MAX_ETHPORTS];
};

/* Manage component running in core as global variable. */
struct core_info {
	int num;  /* Number of IDs below */
	int id[RTE_MAX_LCORE];  /* IDs of components run on the lcore. */
};

/* Manage core status and component info as global variable. */
struct core_mng_info {
	volatile enum sppwk_lcore_status status;
	volatile int ref_index;  /* index for reference */
	volatile int upd_index;  /* index for update */
	struct core_info core[SPP_INFO_AREA_MAX];  /* info of each core */
};

/* Manage data used for backup. */
struct cancel_backup_info {
	struct core_mng_info core[RTE_MAX_LCORE];
	struct sppwk_comp_info component[RTE_MAX_LCORE];
	struct iface_info interface;
};

/* TODO(yasufum) revise using term `iterate`, or comments. */
struct spp_iterate_core_params;
/**
 * Define func to iterate lcore to list core information for showing status
 * or so, as a member of struct `spp_iterate_core_params`.
 */
typedef int (*spp_iterate_core_element_proc)(
		struct spp_iterate_core_params *params,
		const unsigned int lcore_id,
		const char *wk_name,  /* Name of worker named as component. */
		const char *wk_type,  /* Type of worker named as component. */
		const int nof_rx,  /* Number of RX ports */
		const struct sppwk_port_idx *rx_ports,
		const int nof_tx,  /* Number of TX ports */
		const struct sppwk_port_idx *tx_ports);

/**
 * iterate core table parameters used to list content of lcore table for.
 * showing status or so.
 */
struct spp_iterate_core_params {
	char *output;  /* Buffer used for output */
	/** The function for creating core information */
	spp_iterate_core_element_proc element_proc;
};

struct spp_iterate_classifier_table_params;
/**
 * Define func to iterate classifier for showing status or so, as a member
 * member of struct `spp_iterate_classifier_table_params`.
 */
typedef int (*spp_iterate_classifier_element_proc)(
		struct spp_iterate_classifier_table_params *params,
		enum spp_classifier_type type,
		int vid, const char *mac,
		const struct sppwk_port_idx *port);

/**
 * iterate classifier table parameters which is used when listing classifier
 * table content for status command or so.
 */
struct spp_iterate_classifier_table_params {
	void *output;  /* Buffer used for output */
	/* The function for creating classifier table information */
	spp_iterate_classifier_element_proc element_proc;
};

/**
 * Hexdump `addr` for logging, used for core_info or component info.
 *
 * @param name Name of object to be dumped.
 * @param addr Address of dumped value.
 * @param size Size of dumped value.
 */
void log_hexdumped(const char *obj_name, const void *obj_addr,
		const size_t size);

/**
 * Add ring pmd for owned proccess or thread.
 *
 * @param[in] ring_id added ring id.
 * @return ring port ID, or -1 if failed.
 */
int spp_vf_add_ring_pmd(int ring_id);

/**
 * Add ring pmd for owned proccess or thread.
 *
 * @param[in] index Vohst port id.
 * @param[in] client Client id.
 * @return Vhost port ID, or -1 if failed.
 */
int spp_vf_add_vhost_pmd(int index, int client);

/**
 * Get core status
 *
 * @param[in] lcore_id Logical core ID.
 * @return Status of specified logical core.
 */
enum sppwk_lcore_status spp_get_core_status(unsigned int lcore_id);

/**
 * Get component type of target component_info
 *
 * @param id Component ID.
 * @return Type of component executed
 */
enum sppwk_worker_type spp_get_component_type(int id);

/* TODO(yasufum) revise the name of func. */
/**
 * Run check_core_status() several times with interval, up to
 * SPP_CORE_STATUS_CHECK_MAX times.
 *
 * @param[in] status Status for checking.
 * @retval  0 If succeeded.
 * @retval -1 If failed.
 */
int check_core_status_wait(enum sppwk_lcore_status status);

/**
 * Set core status
 *
 * @param lcore_id Lcore ID.
 * @param status Status to be set.
 *
 */
void set_core_status(unsigned int lcore_id, enum sppwk_lcore_status status);

/**
 * Set all core status to given one.
 *
 * @param status status to be set.
 */
void set_all_core_status(enum sppwk_lcore_status status);

/**
 * Set all comp status to SPP_CORE_STOP_REQUEST if received SIGTERM or SIGINT.
 *
 * @param[in] signal Received signal.
 */
void stop_process(int signal);

/* Return sppwk_port_info of given type and num of interface. */
struct sppwk_port_info *
get_sppwk_port(enum port_type iface_type, int iface_no);

/* Output log message for core information */
void log_core_info(const struct core_mng_info *core_info);

/* Output log message for component information */
void log_component_info(const struct sppwk_comp_info *component_info);

/* Output log message for interface information */
void log_interface_info(const struct iface_info *iface_info);

/* Output log message for all management information */
void log_all_mng_info(
		const struct core_mng_info *core,
		const struct sppwk_comp_info *component,
		const struct iface_info *interface);

/* Copy management information */
void copy_mng_info(
		struct core_mng_info *dst_core,
		struct sppwk_comp_info *dst_component,
		struct iface_info *dst_interface,
		const struct core_mng_info *src_core,
		const struct sppwk_comp_info *src_component,
		const struct iface_info *src_interface,
		enum copy_mng_flg flg);

/* Backup the management information */
void backup_mng_info(struct cancel_backup_info *backup);

/* Setup management info for spp_vf */
int init_mng_data(void);

#ifdef SPP_RINGLATENCYSTATS_ENABLE
/* Print statistics of time for packet processing in ring interface */
void print_ring_latency_stats(void);
#endif /* SPP_RINGLATENCYSTATS_ENABLE */

/* Remove sock file if spp is not running */
void  del_vhost_sockfile(struct sppwk_port_info *vhost);

/**
 * Get core ID of target component
 *
 * @param component_id Unique component ID.
 * @return Logical core id of specified component.
 */
unsigned int spp_get_component_core(int component_id);

/* Get core information which is in use */
struct core_info *get_core_info(unsigned int lcore_id);

/**
 * Check core index change
 *
 * @param lcore_id Lcore ID.
 * @retval SPP_RET_OK If index is updated.
 * @retval SPP_RET_NG If index is not updated.
 */
int spp_check_core_update(unsigned int lcore_id);

/**
 * Check if component is using port.
 *
 * @param iface_type Interface type to be validated.
 * @param iface_no Interface number to be validated.
 * @param rxtx Value of spp_port_rxtx to be validated.
 * @retval 0~127      If match component ID
 * @retval SPP_RET_NG If failed.
 */
int spp_check_used_port(
		enum port_type iface_type,
		int iface_no,
		enum sppwk_port_dir dir);

/**
 * Set component update flag for given port.
 *
 * @param port
 *  sppwk_port_info address
 * @param rxtx
 *  enum spp_port_rxtx
 *
 */
void
set_component_change_port(struct sppwk_port_info *port,
		enum sppwk_port_dir dir);

/**
 * Get ID of unused lcore.
 *
 * @retval 0~127 Component ID.
 * @retval -1    failed.
 */
int get_free_lcore_id(void);

/**
 * Get component ID from given name.
 *
 * @param[in] name Component name.
 * @retval 0~127 Component ID.
 * @retval SPP_RET_NG if failed.
 */
int sppwk_get_lcore_id(const char *comp_name);

/**
 *  Delete component information.
 *
 * @param[in] lcore_id The lcore ID of deleted comp.
 * @param[in] nof_comps The num of elements in comp_ary.
 * @param[in] *comp_ary Set of comps from which an comp is deleted.
 *
 * @retval SPP_RET_OK If succeeded.
 * @retval SPP_RET_NG If failed.
 */
/**
 * TODO(yasufum) consider to move to cmd_runner because this func is only
 * used in.
 */
int
del_comp_info(int lcore_id, int nof_comps, int *comp_ary);

/**
 * Get index of given entry in given port info array. It returns the index,
 * or NG code if the entry is not found.
 *
 * @param[in] p_info Target port_info for getting index.
 * @param[in] nof_ports Num of ports for iterating given array.
 * @param[in] p_info_ary The array of port_info.
 * @return Index of given array, or NG code if not found.
 */
int get_idx_port_info(struct sppwk_port_info *p_info, int nof_ports,
		struct sppwk_port_info *p_info_ary[]);

/**
 *  search matched port_info from array and delete it.
 *
 * @param[in] p_info Target port to be deleted.
 * @param[in] nof_ports Number of ports of given p_info_ary.
 * @param[in] array[] Array of p_info.
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int delete_port_info(struct sppwk_port_info *p_info, int nof_ports,
		struct sppwk_port_info *p_info_ary[]);

/**
 * Activate temporarily stored port info while flushing.
 *
 * @retval SPP_RET_OK if succeeded.
 * @retval SPP_RET_NG if failed.
 */
int update_port_info(void);

/* Activate temporarily stored lcore info while flushing. */
void update_lcore_info(void);

/**
 * Port type to string.
 *
 * @param port String of port type to be converted.
 * @param iface_type Interface type.
 * @param iface_no Interface number.
 * @retval SPP_RET_OK If succeeded.
 * @retval SPP_RET_NG If failed.
 */
int
spp_format_port_string(char *port, enum port_type iface_type, int iface_no);

/**
 * Change string of MAC address to int64.
 *
 * @param macaddr String of MAC address to be converted.
 * @retval 0~N MAC address in int64 format.
 * @retval SPP_RET_NG if invalid.
 */
int64_t sppwk_convert_mac_str_to_int64(const char *macaddr);

/**
 * Set mange data address.
 *
 * @param startup_param_p Pointer to  g_startup_param address.
 * @param iface_p Pointer to g_iface_info address.
 * @param component_p Pointer to g_component_info address.
 * @param core_mng_p Pointer to g_core_info address.
 * @param change_core_p Pointer to g_change_core address.
 * @param change_component_p Pointer to g_change_component address.
 * @param backup_info_p Pointer to g_backup_info address.
 * @param main_lcore_id Lcore ID of main thread.
 * @retval SPP_RET_OK If succeeded.
 * @retval SPP_RET_NG If failed.
 */
int sppwk_set_mng_data(struct startup_param *startup_param_p,
		struct iface_info *iface_p,
		struct sppwk_comp_info *component_p,
		struct core_mng_info *core_mng_p,
		int *change_core_p,
		int *change_component_p,
		struct cancel_backup_info *backup_info_p,
		unsigned int main_lcore_id);

/**
 * Get mange data address.
 *
 * @param startup_param_p Pointer to startup params.
 * @param iface_addr Pointer to g_iface_info.
 * @param component_addr Pointer to g_component_info.
 * @param core_mng_addr Pointer to g_core_mng_info.
 * @param change_core_addr Pointer to change_core_addr.
 * @param change_component_addr Pointer to g_change_component.
 * @param backup_info_addr Pointer to g_backup_info.
 */
void sppwk_get_mng_data(struct startup_param **startup_param_p,
		struct iface_info **iface_p,
		struct sppwk_comp_info **component_p,
		struct core_mng_info **core_mng_p,
		int **change_core_p,
		int **change_component_p,
		struct cancel_backup_info **backup_info_p);

#endif /* _SPPWK_CMD_UTILS_H_ */
