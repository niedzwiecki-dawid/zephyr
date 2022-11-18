/*
 * Copyright (c) 2020 Google LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/ec_host_cmd_periph/ec_host_cmd_periph.h>
#include <zephyr/mgmt/ec_host_cmd.h>
#include <string.h>

#ifndef CONFIG_ARCH_POSIX
#error Simulator only valid on posix
#endif

struct ec_host_cmd_sim {
	/* eSPI device instance */
	/* Context for read operation */
	struct ec_host_cmd_rx_ctx *rx_ctx;
//	uint32_t rx_buffer_len;
//	uint8_t *espi_shm;
//	static struct host_cmd_ctrl_blk_espi ctrl_blk;
};

#define EC_HOST_CMD_SIM_DEFINE(_name)					\
	static struct ec_host_cmd_sim _name##_hc_sim;		\
	struct ec_host_cmd_transport _name = {				\
		.api = &ec_host_cmd_api,				\
		.ctx = (struct ec_host_cmd_sim *)&_name##_hc_sim,	\
	}

static ec_host_cmd_transport_api_send tx;

static int ec_host_cmd_sim_init(const struct ec_host_cmd_transport *transport,
		const void *config, struct ec_host_cmd_rx_ctx *rx_ctx)
{
	ARG_UNUSED(config);
	struct ec_host_cmd_sim *hc_sim = (struct ec_host_cmd_sim*)transport->ctx;

	hc_sim->rx_ctx = rx_ctx;

	return 0;
}

static int ec_host_cmd_sim_send(const struct ec_host_cmd_transport *transport,
				 const struct ec_host_cmd_periph_tx_buf *buf)
{
	if (tx != NULL) {
		return tx(transport, buf);
	}

	return 0;
}

static const struct ec_host_cmd_transport_api ec_host_cmd_api = {
	.init = &ec_host_cmd_sim_init,
	.send = &ec_host_cmd_sim_send,
};
EC_HOST_CMD_SIM_DEFINE(ec_host_cmd_sim);


void ec_host_cmd_periph_sim_install_send_cb(ec_host_cmd_transport_api_send cb)
{
	tx = cb;
}

int ec_host_cmd_periph_sim_data_received(const uint8_t *buffer, size_t len)
{
	struct ec_host_cmd_sim *hc_sim = (struct ec_host_cmd_sim *)ec_host_cmd_sim.ctx;

	if (k_sem_take(&hc_sim->rx_ctx->dev_owns, K_NO_WAIT) != 0) {
		return -EBUSY;
	}

	memcpy(hc_sim->rx_ctx->buf, buffer, len);
	hc_sim->rx_ctx->len = len;

	k_sem_give(&hc_sim->rx_ctx->handler_owns);
	return 0;
}

static int host_cmd_init(const struct device *arg)
{
	ARG_UNUSED(arg);

	ec_host_cmd_init(&ec_host_cmd_sim, NULL);
	return 0;
}
SYS_INIT(host_cmd_init, POST_KERNEL, CONFIG_EC_HOST_CMD_INIT_PRIORITY);
