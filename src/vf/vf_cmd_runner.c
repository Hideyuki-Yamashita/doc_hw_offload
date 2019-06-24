/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include "classifier_mac.h"
#include "spp_forward.h"
#include "shared/secondary/return_codes.h"
#include "shared/secondary/string_buffer.h"
#include "shared/secondary/json_helper.h"
#include "shared/secondary/spp_worker_th/cmd_parser.h"
#include "shared/secondary/spp_worker_th/cmd_runner.h"
#include "shared/secondary/spp_worker_th/cmd_res_formatter.h"
#include "shared/secondary/spp_worker_th/vf_deps.h"

#define RTE_LOGTYPE_VF_CMD_RUNNER RTE_LOGTYPE_USER1

/**
 * List of classifier type. The order of items should be same as the order of
 * enum `spp_classifier_type` defined in cmd_utils.h.
 */
/* TODO(yasufum) fix similar var in cmd_parser.c */
const char *CLS_TYPE_A_LIST[] = {
	"none",
	"mac",
	"vlan",
	"",  /* termination */
};

/* Update classifier table with given action, add or del. */
static int
update_cls_table(enum sppwk_action wk_action,
		enum spp_classifier_type type __attribute__ ((unused)),
		int vid, const char *mac_str,
		const struct sppwk_port_idx *port)
{
	/**
	 * Use two types of mac addr in int64_t and uint64_t because first
	 * one is checked if converted value from string  is negative for error.
	 * If it is invalid, convert it to uint64_t.
	 */
	int64_t mac_int64;
	uint64_t mac_uint64;
	struct sppwk_port_info *port_info;

	RTE_LOG(DEBUG, VF_CMD_RUNNER, "Called __func__ with "
			"type `mac`, mac_addr `%s`, and port `%d:%d`.\n",
			mac_str, port->iface_type, port->iface_no);

	mac_int64 = sppwk_convert_mac_str_to_int64(mac_str);
	if (unlikely(mac_int64 == -1)) {
		RTE_LOG(ERR, VF_CMD_RUNNER, "Invalid MAC address `%s`.\n",
				mac_str);
		return SPP_RET_NG;
	}
	mac_uint64 = (uint64_t)mac_int64;

	port_info = get_sppwk_port(port->iface_type, port->iface_no);
	if (unlikely(port_info == NULL)) {
		RTE_LOG(ERR, VF_CMD_RUNNER, "Failed to get port %d:%d.\n",
				port->iface_type, port->iface_no);
		return SPP_RET_NG;
	}
	if (unlikely(port_info->iface_type == UNDEF)) {
		RTE_LOG(ERR, VF_CMD_RUNNER, "Port %d:%d doesn't exist.\n",
				port->iface_type, port->iface_no);
		return SPP_RET_NG;
	}

	if (wk_action == SPPWK_ACT_DEL) {
		if ((port_info->cls_attrs.vlantag.vid != 0) &&
				port_info->cls_attrs.vlantag.vid != vid) {
			RTE_LOG(ERR, VF_CMD_RUNNER,
					"Unexpected VLAN ID `%d`.\n", vid);
			return SPP_RET_NG;
		}
		if ((port_info->cls_attrs.mac_addr != 0) &&
				port_info->cls_attrs.mac_addr != mac_uint64) {
			RTE_LOG(ERR, VF_CMD_RUNNER, "Unexpected MAC %s.\n",
					mac_str);
			return SPP_RET_NG;
		}

		/* Initialize deleted attributes again. */
		port_info->cls_attrs.vlantag.vid = ETH_VLAN_ID_MAX;
		port_info->cls_attrs.mac_addr = 0;
		memset(port_info->cls_attrs.mac_addr_str, 0x00, STR_LEN_SHORT);
	} else if (wk_action == SPPWK_ACT_ADD) {
		if (unlikely(port_info->cls_attrs.vlantag.vid !=
				ETH_VLAN_ID_MAX)) {
			/* TODO(yasufum) why two vids are required in msg ? */
			RTE_LOG(ERR, VF_CMD_RUNNER, "Used port %d:%d, vid %d != %d.\n",
					port->iface_type, port->iface_no,
					port_info->cls_attrs.vlantag.vid, vid);
			return SPP_RET_NG;
		}
		if (unlikely(port_info->cls_attrs.mac_addr != 0)) {
			/* TODO(yasufum) why two macs are required in msg ? */
			RTE_LOG(ERR, VF_CMD_RUNNER, "Used port %d:%d, mac %s != %s.\n",
					port->iface_type, port->iface_no,
					port_info->cls_attrs.mac_addr_str,
					mac_str);
			return SPP_RET_NG;
		}

		/* Update attrs with validated params. */
		port_info->cls_attrs.vlantag.vid = vid;
		port_info->cls_attrs.mac_addr = mac_uint64;
		strcpy(port_info->cls_attrs.mac_addr_str, mac_str);
	}

	set_component_change_port(port_info, SPP_PORT_RXTX_TX);
	return SPP_RET_OK;
}

/* Assign worker thread or remove on specified lcore. */
/* TODO(yasufum) revise func name for removing term `component` or `comp`. */
static int
update_comp(enum sppwk_action wk_action, const char *name,
		unsigned int lcore_id, enum sppwk_worker_type wk_type)
{
	int ret;
	int ret_del;
	int comp_lcore_id = 0;
	unsigned int tmp_lcore_id = 0;
	struct sppwk_comp_info *comp_info = NULL;
	/* TODO(yasufum) revise `core` to be more specific. */
	struct core_info *core = NULL;
	struct core_mng_info *info = NULL;
	struct sppwk_comp_info *comp_info_base = NULL;
	/* TODO(yasufum) revise `core_info` which is same as struct name. */
	struct core_mng_info *core_info = NULL;
	int *change_core = NULL;
	int *change_component = NULL;

	sppwk_get_mng_data(NULL, NULL, &comp_info_base, &core_info,
				&change_core, &change_component, NULL);

	switch (wk_action) {
	case SPPWK_ACT_START:
		info = (core_info + lcore_id);
		if (info->status == SPP_CORE_UNUSE) {
			RTE_LOG(ERR, VF_CMD_RUNNER, "Core %d is not available because "
				"it is in SPP_CORE_UNUSE state.\n", lcore_id);
			return SPP_RET_NG;
		}

		comp_lcore_id = sppwk_get_lcore_id(name);
		if (comp_lcore_id >= 0) {
			RTE_LOG(ERR, VF_CMD_RUNNER, "Component name '%s' is already "
				"used.\n", name);
			return SPP_RET_NG;
		}

		comp_lcore_id = get_free_lcore_id();
		if (comp_lcore_id < 0) {
			RTE_LOG(ERR, VF_CMD_RUNNER, "Cannot assign component over the "
				"maximum number.\n");
			return SPP_RET_NG;
		}

		core = &info->core[info->upd_index];

		comp_info = (comp_info_base + comp_lcore_id);
		memset(comp_info, 0x00, sizeof(struct sppwk_comp_info));
		strcpy(comp_info->name, name);
		comp_info->wk_type = wk_type;
		comp_info->lcore_id = lcore_id;
		comp_info->comp_id = comp_lcore_id;

		core->id[core->num] = comp_lcore_id;
		core->num++;
		ret = SPP_RET_OK;
		tmp_lcore_id = lcore_id;
		*(change_component + comp_lcore_id) = 1;
		break;

	case SPPWK_ACT_STOP:
		comp_lcore_id = sppwk_get_lcore_id(name);
		if (comp_lcore_id < 0)
			return SPP_RET_OK;

		comp_info = (comp_info_base + comp_lcore_id);
		tmp_lcore_id = comp_info->lcore_id;
		memset(comp_info, 0x00, sizeof(struct sppwk_comp_info));

		info = (core_info + tmp_lcore_id);
		core = &info->core[info->upd_index];

		/* initialize classifier information */
		if (comp_info->wk_type == SPPWK_TYPE_CLS)
			init_classifier_info(comp_lcore_id);

		/* The latest lcore is released if worker thread is stopped. */
		ret_del = del_comp_info(comp_lcore_id, core->num, core->id);
		if (ret_del >= 0)
			core->num--;

		ret = SPP_RET_OK;
		*(change_component + comp_lcore_id) = 0;
		break;

	default:  /* Unexpected case. */
		ret = SPP_RET_NG;
		break;
	}

	*(change_core + tmp_lcore_id) = 1;
	return ret;
}

/* Check if over the maximum num of rx and tx ports of component. */
static int
check_vf_port_count(int component_type, enum spp_port_rxtx rxtx, int num_rx,
								int num_tx)
{
	RTE_LOG(INFO, VF_CMD_RUNNER, "port count, port_type=%d,"
				" rx=%d, tx=%d\n", rxtx, num_rx, num_tx);
	if (rxtx == SPP_PORT_RXTX_RX)
		num_rx++;
	else
		num_tx++;
	/* Add rx or tx port appointed in port_type. */
	RTE_LOG(INFO, VF_CMD_RUNNER, "Num of ports after count up,"
				" port_type=%d, rx=%d, tx=%d\n",
				rxtx, num_rx, num_tx);
	switch (component_type) {
	case SPPWK_TYPE_FWD:
		if (num_rx > 1 || num_tx > 1)
			return SPP_RET_NG;
		break;

	case SPPWK_TYPE_MRG:
		if (num_tx > 1)
			return SPP_RET_NG;
		break;

	case SPPWK_TYPE_CLS:
		if (num_rx > 1)
			return SPP_RET_NG;
		break;

	default:
		/* Illegal component type. */
		return SPP_RET_NG;
	}

	return SPP_RET_OK;
}

/* Port add or del to execute it */
static int
update_port(enum sppwk_action wk_action,
		const struct sppwk_port_idx *port,
		enum spp_port_rxtx rxtx,
		const char *name,
		const struct spp_port_ability *ability)
{
	int ret = SPP_RET_NG;
	int port_idx;
	int ret_del = -1;
	int comp_lcore_id = 0;
	int cnt = 0;
	struct sppwk_comp_info *comp_info = NULL;
	struct sppwk_port_info *port_info = NULL;
	int *nof_ports = NULL;
	struct sppwk_port_info **ports = NULL;
	struct sppwk_comp_info *comp_info_base = NULL;
	int *change_component = NULL;

	comp_lcore_id = sppwk_get_lcore_id(name);
	if (comp_lcore_id < 0) {
		RTE_LOG(ERR, VF_CMD_RUNNER, "Unknown component by port command. "
				"(component = %s)\n", name);
		return SPP_RET_NG;
	}
	sppwk_get_mng_data(NULL, NULL,
			&comp_info_base, NULL, NULL, &change_component, NULL);
	comp_info = (comp_info_base + comp_lcore_id);
	port_info = get_sppwk_port(port->iface_type, port->iface_no);
	if (rxtx == SPP_PORT_RXTX_RX) {
		nof_ports = &comp_info->nof_rx;
		ports = comp_info->rx_ports;
	} else {
		nof_ports = &comp_info->nof_tx;
		ports = comp_info->tx_ports;
	}

	switch (wk_action) {
	case SPPWK_ACT_ADD:
		/* Check if over the maximum num of ports of component. */
		if (check_vf_port_count(comp_info->wk_type, rxtx,
				comp_info->nof_rx,
				comp_info->nof_tx) != SPP_RET_OK)
			return SPP_RET_NG;

		/* Check if the port_info is included in array `ports`. */
		port_idx = get_idx_port_info(port_info, *nof_ports, ports);
		if (port_idx >= SPP_RET_OK) {
			/* registered */
			if (ability->ops == SPPWK_PORT_ABL_OPS_ADD_VLANTAG) {
				while ((cnt < SPP_PORT_ABILITY_MAX) &&
					    (port_info->ability[cnt].ops !=
					    SPPWK_PORT_ABL_OPS_ADD_VLANTAG))
					cnt++;
				if (cnt >= SPP_PORT_ABILITY_MAX) {
					RTE_LOG(ERR, VF_CMD_RUNNER, "update VLAN tag "
						"Non-registratio\n");
					return SPP_RET_NG;
				}
				memcpy(&port_info->ability[cnt], ability,
					sizeof(struct spp_port_ability));

				ret = SPP_RET_OK;
				break;
			}
			return SPP_RET_OK;
		}

		if (*nof_ports >= RTE_MAX_ETHPORTS) {
			RTE_LOG(ERR, VF_CMD_RUNNER, "Cannot assign port over the "
				"maximum number.\n");
			return SPP_RET_NG;
		}

		if (ability->ops != SPPWK_PORT_ABL_OPS_NONE) {
			while ((cnt < SPP_PORT_ABILITY_MAX) &&
					(port_info->ability[cnt].ops !=
					SPPWK_PORT_ABL_OPS_NONE)) {
				cnt++;
			}
			if (cnt >= SPP_PORT_ABILITY_MAX) {
				RTE_LOG(ERR, VF_CMD_RUNNER,
						"No space of port ability.\n");
				return SPP_RET_NG;
			}
			memcpy(&port_info->ability[cnt], ability,
					sizeof(struct spp_port_ability));
		}

		port_info->iface_type = port->iface_type;
		ports[*nof_ports] = port_info;
		(*nof_ports)++;

		ret = SPP_RET_OK;
		break;

	case SPPWK_ACT_DEL:
		for (cnt = 0; cnt < SPP_PORT_ABILITY_MAX; cnt++) {
			if (port_info->ability[cnt].ops ==
					SPPWK_PORT_ABL_OPS_NONE)
				continue;

			if (port_info->ability[cnt].rxtx == rxtx)
				memset(&port_info->ability[cnt], 0x00,
					sizeof(struct spp_port_ability));
		}

		ret_del = delete_port_info(port_info, *nof_ports, ports);
		if (ret_del == 0)
			(*nof_ports)--; /* If deleted, decrement number. */

		ret = SPP_RET_OK;
		break;

	default:  /* This case cannot be happend without invlid wk_action. */
		return SPP_RET_NG;
	}

	*(change_component + comp_lcore_id) = 1;
	return ret;
}

/* Execute one command. */
int
exec_one_cmd(const struct sppwk_cmd_attrs *cmd)
{
	int ret;

	RTE_LOG(INFO, VF_CMD_RUNNER, "Exec `%s` cmd.\n",
			sppwk_cmd_type_str(cmd->type));

	switch (cmd->type) {
	case SPPWK_CMDTYPE_CLS_MAC:
	case SPPWK_CMDTYPE_CLS_VLAN:
		ret = update_cls_table(cmd->spec.cls_table.wk_action,
				cmd->spec.cls_table.type,
				cmd->spec.cls_table.vid,
				cmd->spec.cls_table.mac,
				&cmd->spec.cls_table.port);
		if (ret == 0) {
			RTE_LOG(INFO, VF_CMD_RUNNER, "Exec flush.\n");
			ret = flush_cmd();
		}
		break;

	case SPPWK_CMDTYPE_WORKER:
		ret = update_comp(
				cmd->spec.comp.wk_action,
				cmd->spec.comp.name,
				cmd->spec.comp.core,
				cmd->spec.comp.wk_type);
		if (ret == 0) {
			RTE_LOG(INFO, VF_CMD_RUNNER, "Exec flush.\n");
			ret = flush_cmd();
		}
		break;

	case SPPWK_CMDTYPE_PORT:
		RTE_LOG(INFO, VF_CMD_RUNNER, "with action `%s`.\n",
				sppwk_action_str(cmd->spec.port.wk_action));
		ret = update_port(cmd->spec.port.wk_action,
				&cmd->spec.port.port, cmd->spec.port.rxtx,
				cmd->spec.port.name, &cmd->spec.port.ability);
		if (ret == 0) {
			RTE_LOG(INFO, VF_CMD_RUNNER, "Exec flush.\n");
			ret = flush_cmd();
		}
		break;

	default:
		/* Do nothing. */
		ret = SPP_RET_OK;
		break;
	}

	return ret;
}

/* Iterate core information to create response to status command */
static int
spp_iterate_core_info(struct spp_iterate_core_params *params)
{
	int ret;
	int lcore_id, cnt;
	struct core_info *core = NULL;
	struct sppwk_comp_info *comp_info_base = NULL;
	struct sppwk_comp_info *comp_info = NULL;

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (spp_get_core_status(lcore_id) == SPP_CORE_UNUSE)
			continue;

		core = get_core_info(lcore_id);
		if (core->num == 0) {
			ret = (*params->element_proc)(
				params, lcore_id,
				"", SPPWK_TYPE_NONE_STR,
				0, NULL, 0, NULL);
			if (unlikely(ret != 0)) {
				RTE_LOG(ERR, VF_CMD_RUNNER,
						"Cannot iterate core "
						"information. "
						"(core = %d, type = %d)\n",
						lcore_id, SPPWK_TYPE_NONE);
				return SPP_RET_NG;
			}
			continue;
		}

		for (cnt = 0; cnt < core->num; cnt++) {
			sppwk_get_mng_data(NULL, NULL, &comp_info_base,
							NULL, NULL, NULL, NULL);
			comp_info = (comp_info_base + core->id[cnt]);

			if (comp_info->wk_type == SPPWK_TYPE_CLS) {
				ret = get_classifier_status(lcore_id,
						core->id[cnt], params);
			} else {
				ret = get_forwarder_status(lcore_id,
						core->id[cnt], params);
			}

			if (unlikely(ret != 0)) {
				RTE_LOG(ERR, VF_CMD_RUNNER,
						"Cannot iterate core "
						"information. "
						"(core = %d, type = %d)\n",
						lcore_id, comp_info->wk_type);
				return SPP_RET_NG;
			}
		}
	}

	return SPP_RET_OK;
}

/* Add entry of core info of worker to a response in JSON such as "core:0". */
int
add_core(const char *name, char **output,
		void *tmp __attribute__ ((unused)))
{
	int ret = SPP_RET_NG;
	struct spp_iterate_core_params itr_params;
	char *tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, VF_CMD_RUNNER,
				/* TODO(yasufum) refactor no meaning err msg */
				"allocate error. (name = %s)\n",
				name);
		return SPP_RET_NG;
	}

	itr_params.output = tmp_buff;
	itr_params.element_proc = append_core_element_value;

	ret = spp_iterate_core_info(&itr_params);
	if (unlikely(ret != SPP_RET_OK)) {
		spp_strbuf_free(itr_params.output);
		return SPP_RET_NG;
	}

	ret = append_json_array_brackets(output, name, itr_params.output);
	spp_strbuf_free(itr_params.output);
	return ret;
}

/* Activate temporarily stored component info while flushing. */
int
update_comp_info(struct sppwk_comp_info *p_comp_info, int *p_change_comp)
{
	int ret = 0;
	int cnt = 0;
	struct sppwk_comp_info *comp_info = NULL;

	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++) {
		if (*(p_change_comp + cnt) == 0)
			continue;

		comp_info = (p_comp_info + cnt);
		spp_port_ability_update(comp_info);

		if (comp_info->wk_type == SPPWK_TYPE_CLS) {
			ret = update_classifier(comp_info);
			RTE_LOG(DEBUG, VF_CMD_RUNNER, "Update classifier.\n");
		} else {
			ret = update_forwarder(comp_info);
			RTE_LOG(DEBUG, VF_CMD_RUNNER, "Update forwarder.\n");
		}

		if (unlikely(ret < 0)) {
			RTE_LOG(ERR, VF_CMD_RUNNER, "Flush error. "
					"( component = %s, type = %d)\n",
					comp_info->name,
					comp_info->wk_type);
			return SPP_RET_NG;
		}
	}
	return SPP_RET_OK;
}

/**
 * Operator function called in iterator for getting each of entries of
 * classifier table named as iterate_adding_mac_entry().
 */
int
append_classifier_element_value(
		struct spp_iterate_classifier_table_params *params,
		enum spp_classifier_type type,
		int vid, const char *mac,
		const struct sppwk_port_idx *port)
{
	int ret = SPP_RET_NG;
	char *buff, *tmp_buff;
	char port_str[CMD_TAG_APPEND_SIZE];
	char value_str[STR_LEN_SHORT];
	buff = params->output;
	tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, VF_CMD_RUNNER,
				/* TODO(yasufum) refactor no meaning err msg */
				"allocate error. (name = classifier_table)\n");
		return ret;
	}

	spp_format_port_string(port_str, port->iface_type, port->iface_no);

	ret = append_json_str_value(&tmp_buff, "type", CLS_TYPE_A_LIST[type]);
	if (unlikely(ret < SPP_RET_OK))
		return ret;

	memset(value_str, 0x00, STR_LEN_SHORT);
	switch (type) {
	case SPP_CLASSIFIER_TYPE_MAC:
		sprintf(value_str, "%s", mac);
		break;
	case SPP_CLASSIFIER_TYPE_VLAN:
		sprintf(value_str, "%d/%s", vid, mac);
		break;
	default:
		/* not used */
		break;
	}

	ret = append_json_str_value(&tmp_buff, "value", value_str);
	if (unlikely(ret < 0))
		return ret;

	ret = append_json_str_value(&tmp_buff, "port", port_str);
	if (unlikely(ret < SPP_RET_OK))
		return ret;

	ret = append_json_block_brackets(&buff, "", tmp_buff);
	spp_strbuf_free(tmp_buff);
	params->output = buff;
	return ret;
}

/* Get component type from string of its name. */
/* TODO(yasufum) consider to create and move to vf_cmd_parser.c */
enum sppwk_worker_type
get_comp_type_from_str(const char *type_str)
{
	RTE_LOG(DEBUG, VF_CMD_RUNNER, "type_str is %s\n", type_str);

	if (strncmp(type_str, CORE_TYPE_CLASSIFIER_MAC_STR,
			strlen(CORE_TYPE_CLASSIFIER_MAC_STR)+1) == 0) {
		return SPPWK_TYPE_CLS;
	} else if (strncmp(type_str, CORE_TYPE_MERGE_STR,
			strlen(CORE_TYPE_MERGE_STR)+1) == 0) {
		return SPPWK_TYPE_MRG;
	} else if (strncmp(type_str, CORE_TYPE_FORWARD_STR,
			strlen(CORE_TYPE_FORWARD_STR)+1) == 0) {
		return SPPWK_TYPE_FWD;
	}

	return SPPWK_TYPE_NONE;
}
