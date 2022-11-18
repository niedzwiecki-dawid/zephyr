/*
 * Copyright (c) 2022 Google LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/ec_host_cmd_periph/ec_host_cmd_periph.h>
#include <zephyr/drivers/espi.h>
#include <zephyr/mgmt/ec_host_cmd.h>

struct ec_host_cmd_espi {
	/* eSPI device instance */
	const struct device *espi_dev;
	/* Context for read operation */
	struct ec_host_cmd_rx_ctx *rx_ctx;
	struct ec_host_cmd_tx_buf *tx;
	struct espi_callback espi_cb;
//	uint32_t rx_buffer_len;
//	uint8_t *espi_shm;
//	static struct host_cmd_ctrl_blk_espi ctrl_blk;
};

#define EC_HOST_CMD_ESPI_DEFINE(_name)					\
	static struct ec_host_cmd_espi _name##_hc_espi;		\
	struct ec_host_cmd_transport _name = {				\
		.api = &ec_host_cmd_api,				\
		.ctx = (struct ec_host_cmd_espi *)&_name##_hc_espi	\
	}

static void ec_host_cmd_periph_espi_handler(const struct device *dev, struct espi_callback *cb,
					    struct espi_event espi_evt)
{
	struct ec_host_cmd_espi *hc_espi =
		CONTAINER_OF(cb, struct ec_host_cmd_espi, espi_cb);
	uint16_t event_type = (uint16_t)espi_evt.evt_details;

	if (event_type != ESPI_PERIPHERAL_EC_HOST_CMD) {
		return;
	}

	k_sem_give(&hc_espi->rx_ctx->handler_owns);
}

static int ec_host_cmd_espi_init(const struct ec_host_cmd_transport *transport,
		const void *config, struct ec_host_cmd_rx_ctx *rx_ctx, struct ec_host_cmd_tx_buf *tx)
{
	struct ec_host_cmd_espi *hc_espi = (struct ec_host_cmd_espi*)transport->ctx;

	hc_espi->espi_dev = (const struct device *)config;

	if (!device_is_ready(hc_espi->espi_dev)) {
		return -ENODEV;
	}

	hc_espi->rx_ctx = rx_ctx;

	espi_init_callback(&hc_espi->espi_cb, ec_host_cmd_periph_espi_handler,
			   ESPI_BUS_PERIPHERAL_NOTIFICATION);
	espi_add_callback(hc_espi->espi_dev, &hc_espi->espi_cb);
	espi_read_lpc_request(hc_espi->espi_dev, ECUSTOM_HOST_CMD_GET_PARAM_MEMORY,
			(uint32_t *)&rx_ctx->buf);
	/* Set shared memory size as a size of received message. */
	espi_read_lpc_request(hc_espi->espi_dev, ECUSTOM_HOST_CMD_GET_PARAM_MEMORY_SIZE,
			      &hc_espi->rx_ctx->len);

	return 0;
//	rx_ctx->buf = data->espi_shm;
//	rx_ctx->len = &data->rx_buffer_len;
}

static int ec_host_cmd_espi_send(const struct ec_host_cmd_transport *transport,
				 const struct ec_host_cmd_tx_buf *buf)
{
	struct ec_host_cmd_espi *hc_espi = (struct ec_host_cmd_espi *)transport->ctx;
	struct ec_host_cmd_response_header *resp_hdr = buf->buf;
	uint32_t result = resp_hdr->result;

	/* For eSPI the tx and rx buffers are the same (shared_mem) */
	memcpy(hc_espi->rx_ctx->buf, buf->buf, buf->len);

	return espi_write_lpc_request(hc_espi->espi_dev, ECUSTOM_HOST_CMD_SEND_RESULT, &result);
}

static const struct ec_host_cmd_transport_api ec_host_cmd_api = {
	.init = &ec_host_cmd_espi_init,
	.send = &ec_host_cmd_espi_send,
};

EC_HOST_CMD_ESPI_DEFINE(ec_host_cmd_espi);
struct ec_host_cmd_transport *ec_host_cmd_backend_get_espi(void)
{
	return &ec_host_cmd_espi;
}

#if DT_NODE_EXISTS(DT_CHOSEN(zephyr_host_cmd_backend))
static int host_cmd_init(const struct device *arg)
{
	ARG_UNUSED(arg);
	const struct device *const dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_host_cmd_backend));

	printk("DN: ec_host_cmd_init espi \n");

	if (!device_is_ready(dev)) {
		return -ENODEV;
	}

	ec_host_cmd_init(ec_host_cmd_backend_get_espi(), (void*)dev);
	return 0;
}
SYS_INIT(host_cmd_init, POST_KERNEL, CONFIG_EC_HOST_CMD_INIT_PRIORITY);
#endif
