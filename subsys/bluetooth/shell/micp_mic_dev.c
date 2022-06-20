/** @file
 *  @brief Bluetooth MICP shell.
 *
 * Copyright (c) 2020 Bose Corporation
 * Copyright (c) 2020-2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/audio/micp.h>
#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include <stdio.h>

#include "bt.h"

static struct bt_micp *micp;

static void micp_mute_cb(struct bt_micp *micp, int err, uint8_t mute)
{
	if (err != 0) {
		shell_error(ctx_shell, "Mute get failed (%d)", err);
	} else {
		shell_print(ctx_shell, "Mute value %u", mute);
	}
}

static struct bt_micp_cb micp_cbs = {
	.mute = micp_mute_cb,
};

#if defined(CONFIG_BT_MICP_AICS)
static struct bt_micp_included micp_included;

static void micp_aics_state_cb(struct bt_aics *inst, int err, int8_t gain,
			       uint8_t mute, uint8_t mode)
{
	if (err != 0) {
		shell_error(ctx_shell, "AICS state get failed (%d) for "
			    "inst %p", err, inst);
	} else {
		shell_print(ctx_shell, "AICS inst %p state gain %d, mute %u, "
			    "mode %u", inst, gain, mute, mode);
	}

}
static void micp_aics_gain_setting_cb(struct bt_aics *inst, int err,
				      uint8_t units, int8_t minimum,
				      int8_t maximum)
{
	if (err != 0) {
		shell_error(ctx_shell, "AICS gain settings get failed (%d) for "
			    "inst %p", err, inst);
	} else {
		shell_print(ctx_shell, "AICS inst %p gain settings units %u, "
			    "min %d, max %d", inst, units, minimum,
			    maximum);
	}

}
static void micp_aics_input_type_cb(struct bt_aics *inst, int err,
				    uint8_t input_type)
{
	if (err != 0) {
		shell_error(ctx_shell, "AICS input type get failed (%d) for "
			    "inst %p", err, inst);
	} else {
		shell_print(ctx_shell, "AICS inst %p input type %u",
			    inst, input_type);
	}

}
static void micp_aics_status_cb(struct bt_aics *inst, int err, bool active)
{
	if (err != 0) {
		shell_error(ctx_shell, "AICS status get failed (%d) for "
			    "inst %p", err, inst);
	} else {
		shell_print(ctx_shell, "AICS inst %p status %s",
			    inst, active ? "active" : "inactive");
	}

}
static void micp_aics_description_cb(struct bt_aics *inst, int err,
				     char *description)
{
	if (err != 0) {
		shell_error(ctx_shell, "AICS description get failed (%d) for "
			    "inst %p", err, inst);
	} else {
		shell_print(ctx_shell, "AICS inst %p description %s",
			    inst, description);
	}
}

static struct bt_aics_cb aics_cb = {
	.state = micp_aics_state_cb,
	.gain_setting = micp_aics_gain_setting_cb,
	.type = micp_aics_input_type_cb,
	.status = micp_aics_status_cb,
	.description = micp_aics_description_cb,
};
#endif /* CONFIG_BT_MICP_AICS */

static int cmd_micp_param(const struct shell *sh, size_t argc, char **argv)
{
	int result;
	struct bt_micp_register_param micp_param;

	if (ctx_shell == NULL) {
		ctx_shell = sh;
	}

	(void)memset(&micp_param, 0, sizeof(micp_param));

#if defined(CONFIG_BT_MICP_AICS)
	char input_desc[CONFIG_BT_MICP_AICS_INSTANCE_COUNT][16];

	for (int i = 0; i < ARRAY_SIZE(micp_param.aics_param); i++) {
		micp_param.aics_param[i].desc_writable = true;
		snprintf(input_desc[i], sizeof(input_desc[i]),
			 "Input %d", i + 1);
		micp_param.aics_param[i].description = input_desc[i];
		micp_param.aics_param[i].type = BT_AICS_INPUT_TYPE_UNSPECIFIED;
		micp_param.aics_param[i].status = true;
		micp_param.aics_param[i].gain_mode = BT_AICS_MODE_MANUAL;
		micp_param.aics_param[i].units = 1;
		micp_param.aics_param[i].min_gain = -100;
		micp_param.aics_param[i].max_gain = 100;
		micp_param.aics_param[i].cb = &aics_cb;
	}
#endif /* CONFIG_BT_MICP_AICS */

	micp_param.cb = &micp_cbs;

	result = bt_micp_register(&micp_param, &micp);
	if (result != 0) {
		shell_error(sh, "MICP register failed: %d", result);
		return result;
	}

	shell_print(sh, "MICP initialized: %d", result);

#if defined(CONFIG_BT_MICP_AICS)
	result = bt_micp_included_get(NULL, &micp_included);
	if (result != 0) {
		shell_error(sh, "MICP get failed: %d", result);
	}
#endif /* CONFIG_BT_MICP_AICS */

	return result;
}

static int cmd_micp_mute_get(const struct shell *sh, size_t argc, char **argv)
{
	int result = bt_micp_mute_get(NULL);

	if (result != 0) {
		shell_error(sh, "Fail: %d", result);
	}

	return result;
}

static int cmd_micp_mute(const struct shell *sh, size_t argc, char **argv)
{
	int result = bt_micp_mute(NULL);

	if (result != 0) {
		shell_error(sh, "Fail: %d", result);
	}

	return result;
}

static int cmd_micp_unmute(const struct shell *sh, size_t argc, char **argv)
{
	int result = bt_micp_unmute(NULL);

	if (result != 0) {
		shell_error(sh, "Fail: %d", result);
	}

	return result;
}

static int cmd_micp_mute_disable(const struct shell *sh, size_t argc,
				 char **argv)
{
	int result = bt_micp_mute_disable(micp);

	if (result != 0) {
		shell_error(sh, "Fail: %d", result);
	}

	return result;
}

#if defined(CONFIG_BT_MICP_AICS)
static int cmd_micp_aics_deactivate(const struct shell *sh, size_t argc,
				    char **argv)
{
	int result;
	int index = strtol(argv[1], NULL, 0);

	if (index >= micp_included.aics_cnt) {
		shell_error(sh, "Index shall be less than %u, was %u",
			    micp_included.aics_cnt, index);
		return -ENOEXEC;
	}

	result = bt_aics_deactivate(micp_included.aics[index]);
	if (result != 0) {
		shell_error(sh, "Fail: %d", result);
	}

	return result;
}

static int cmd_micp_aics_activate(const struct shell *sh, size_t argc,
				  char **argv)
{
	int result;
	int index = strtol(argv[1], NULL, 0);

	if (index >= micp_included.aics_cnt) {
		shell_error(sh, "Index shall be less than %u, was %u",
			    micp_included.aics_cnt, index);
		return -ENOEXEC;
	}

	result = bt_aics_activate(micp_included.aics[index]);
	if (result != 0) {
		shell_error(sh, "Fail: %d", result);
	}

	return result;
}

static int cmd_micp_aics_input_state_get(const struct shell *sh, size_t argc,
					 char **argv)
{
	int result;
	int index = strtol(argv[1], NULL, 0);

	if (index >= micp_included.aics_cnt) {
		shell_error(sh, "Index shall be less than %u, was %u",
			    micp_included.aics_cnt, index);
		return -ENOEXEC;
	}

	result = bt_aics_state_get(micp_included.aics[index]);
	if (result != 0) {
		shell_error(sh, "Fail: %d", result);
	}

	return result;
}

static int cmd_micp_aics_gain_setting_get(const struct shell *sh, size_t argc,
					  char **argv)
{
	int result;
	int index = strtol(argv[1], NULL, 0);

	if (index >= micp_included.aics_cnt) {
		shell_error(sh, "Index shall be less than %u, was %u",
			    micp_included.aics_cnt, index);
		return -ENOEXEC;
	}

	result = bt_aics_gain_setting_get(micp_included.aics[index]);
	if (result != 0) {
		shell_error(sh, "Fail: %d", result);
	}

	return result;
}

static int cmd_micp_aics_input_type_get(const struct shell *sh, size_t argc,
					char **argv)
{
	int result;
	int index = strtol(argv[1], NULL, 0);

	if (index >= micp_included.aics_cnt) {
		shell_error(sh, "Index shall be less than %u, was %u",
			    micp_included.aics_cnt, index);
		return -ENOEXEC;
	}

	result = bt_aics_type_get(micp_included.aics[index]);
	if (result != 0) {
		shell_error(sh, "Fail: %d", result);
	}

	return result;
}

static int cmd_micp_aics_input_status_get(const struct shell *sh, size_t argc,
					  char **argv)
{
	int result;
	int index = strtol(argv[1], NULL, 0);

	if (index >= micp_included.aics_cnt) {
		shell_error(sh, "Index shall be less than %u, was %u",
			    micp_included.aics_cnt, index);
		return -ENOEXEC;
	}

	result = bt_aics_status_get(micp_included.aics[index]);
	if (result != 0) {
		shell_error(sh, "Fail: %d", result);
	}

	return result;
}

static int cmd_micp_aics_input_unmute(const struct shell *sh, size_t argc,
				      char **argv)
{
	int result;
	int index = strtol(argv[1], NULL, 0);

	if (index >= micp_included.aics_cnt) {
		shell_error(sh, "Index shall be less than %u, was %u",
			    micp_included.aics_cnt, index);
		return -ENOEXEC;
	}

	result = bt_aics_unmute(micp_included.aics[index]);
	if (result != 0) {
		shell_error(sh, "Fail: %d", result);
	}

	return result;
}

static int cmd_micp_aics_input_mute(const struct shell *sh, size_t argc,
				    char **argv)
{
	int result;
	int index = strtol(argv[1], NULL, 0);

	if (index >= micp_included.aics_cnt) {
		shell_error(sh, "Index shall be less than %u, was %u",
			    micp_included.aics_cnt, index);
		return -ENOEXEC;
	}

	result = bt_aics_mute(micp_included.aics[index]);
	if (result != 0) {
		shell_error(sh, "Fail: %d", result);
	}

	return result;
}

static int cmd_micp_aics_manual_input_gain_set(const struct shell *sh,
					       size_t argc, char **argv)
{
	int result;
	int index = strtol(argv[1], NULL, 0);

	if (index >= micp_included.aics_cnt) {
		shell_error(sh, "Index shall be less than %u, was %u",
			    micp_included.aics_cnt, index);
		return -ENOEXEC;
	}

	result = bt_aics_manual_gain_set(micp_included.aics[index]);
	if (result != 0) {
		shell_error(sh, "Fail: %d", result);
	}

	return result;
}

static int cmd_micp_aics_automatic_input_gain_set(const struct shell *sh,
						  size_t argc, char **argv)
{
	int result;
	int index = strtol(argv[1], NULL, 0);

	if (index >= micp_included.aics_cnt) {
		shell_error(sh, "Index shall be less than %u, was %u",
			    micp_included.aics_cnt, index);
		return -ENOEXEC;
	}

	result = bt_aics_automatic_gain_set(micp_included.aics[index]);
	if (result != 0) {
		shell_error(sh, "Fail: %d", result);
	}

	return result;
}

static int cmd_micp_aics_gain_set(const struct shell *sh, size_t argc,
				  char **argv)
{
	int result;
	int index = strtol(argv[1], NULL, 0);
	int gain = strtol(argv[2], NULL, 0);

	if (index >= micp_included.aics_cnt) {
		shell_error(sh, "Index shall be less than %u, was %u",
			    micp_included.aics_cnt, index);
		return -ENOEXEC;
	}

	if (gain > INT8_MAX || gain < INT8_MIN) {
		shell_error(sh, "Offset shall be %d-%d, was %d",
			    INT8_MIN, INT8_MAX, gain);
		return -ENOEXEC;
	}

	result = bt_aics_gain_set(micp_included.aics[index], gain);
	if (result != 0) {
		shell_error(sh, "Fail: %d", result);
	}

	return result;
}

static int cmd_micp_aics_input_description_get(const struct shell *sh,
					       size_t argc, char **argv)
{
	int result;
	int index = strtol(argv[1], NULL, 0);

	if (index >= micp_included.aics_cnt) {
		shell_error(sh, "Index shall be less than %u, was %u",
			    micp_included.aics_cnt, index);
		return -ENOEXEC;
	}

	result = bt_aics_description_get(micp_included.aics[index]);
	if (result != 0) {
		shell_error(sh, "Fail: %d", result);
	}

	return result;
}

static int cmd_micp_aics_input_description_set(const struct shell *sh,
					       size_t argc, char **argv)
{
	int result;
	int index = strtol(argv[1], NULL, 0);
	char *description = argv[2];

	if (index >= micp_included.aics_cnt) {
		shell_error(sh, "Index shall be less than %u, was %u",
			    micp_included.aics_cnt, index);
		return -ENOEXEC;
	}

	result = bt_aics_description_set(micp_included.aics[index],
					      description);
	if (result != 0) {
		shell_error(sh, "Fail: %d", result);
	}

	return result;
}
#endif /* CONFIG_BT_MICP_AICS */

static int cmd_micp(const struct shell *sh, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(sh, "%s unknown parameter: %s",
			    argv[0], argv[1]);
	} else {
		shell_error(sh, "%s Missing subcommand", argv[0]);
	}

	return -ENOEXEC;
}

SHELL_STATIC_SUBCMD_SET_CREATE(micp_cmds,
	SHELL_CMD_ARG(init, NULL,
		      "Initialize the service and register callbacks",
		      cmd_micp_param, 1, 0),
	SHELL_CMD_ARG(mute_get, NULL,
		      "Get the mute state",
		      cmd_micp_mute_get, 1, 0),
	SHELL_CMD_ARG(mute, NULL,
		      "Mute the MICP server",
		      cmd_micp_mute, 1, 0),
	SHELL_CMD_ARG(unmute, NULL,
		      "Unmute the MICP server",
		      cmd_micp_unmute, 1, 0),
	SHELL_CMD_ARG(mute_disable, NULL,
		      "Disable the MICP mute",
		      cmd_micp_mute_disable, 1, 0),
#if defined(CONFIG_BT_MICP_AICS)
	SHELL_CMD_ARG(aics_deactivate, NULL,
		      "Deactivates a AICS instance <inst_index>",
		      cmd_micp_aics_deactivate, 2, 0),
	SHELL_CMD_ARG(aics_activate, NULL,
		      "Activates a AICS instance <inst_index>",
		      cmd_micp_aics_activate, 2, 0),
	SHELL_CMD_ARG(aics_input_state_get, NULL,
		      "Get the input state of a AICS instance <inst_index>",
		      cmd_micp_aics_input_state_get, 2, 0),
	SHELL_CMD_ARG(aics_gain_setting_get, NULL,
		      "Get the gain settings of a AICS instance <inst_index>",
		      cmd_micp_aics_gain_setting_get, 2, 0),
	SHELL_CMD_ARG(aics_input_type_get, NULL,
		      "Get the input type of a AICS instance <inst_index>",
		      cmd_micp_aics_input_type_get, 2, 0),
	SHELL_CMD_ARG(aics_input_status_get, NULL,
		      "Get the input status of a AICS instance <inst_index>",
		      cmd_micp_aics_input_status_get, 2, 0),
	SHELL_CMD_ARG(aics_input_unmute, NULL,
		      "Unmute the input of a AICS instance <inst_index>",
		      cmd_micp_aics_input_unmute, 2, 0),
	SHELL_CMD_ARG(aics_input_mute, NULL,
		      "Mute the input of a AICS instance <inst_index>",
		      cmd_micp_aics_input_mute, 2, 0),
	SHELL_CMD_ARG(aics_manual_input_gain_set, NULL,
		      "Set the gain mode of a AICS instance to manual "
		      "<inst_index>",
		      cmd_micp_aics_manual_input_gain_set, 2, 0),
	SHELL_CMD_ARG(aics_automatic_input_gain_set, NULL,
		      "Set the gain mode of a AICS instance to automatic "
		      "<inst_index>",
		      cmd_micp_aics_automatic_input_gain_set, 2, 0),
	SHELL_CMD_ARG(aics_gain_set, NULL,
		      "Set the gain in dB of a AICS instance <inst_index> "
		      "<gain (-128 to 127)>",
		      cmd_micp_aics_gain_set, 3, 0),
	SHELL_CMD_ARG(aics_input_description_get, NULL,
		      "Get the input description of a AICS instance "
		      "<inst_index>",
		      cmd_micp_aics_input_description_get, 2, 0),
	SHELL_CMD_ARG(aics_input_description_set, NULL,
		      "Set the input description of a AICS instance "
		      "<inst_index> <description>",
		      cmd_micp_aics_input_description_set, 3, 0),
#endif /* CONFIG_BT_MICP_AICS */
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_ARG_REGISTER(micp, &micp_cmds, "Bluetooth MICP shell commands",
		       cmd_micp, 1, 1);
