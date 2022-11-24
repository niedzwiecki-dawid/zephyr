/*
 * Copyright (c) 2020 Google LLC.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/ec_host_cmd_periph/ec_host_cmd_periph.h>
#include <zephyr/mgmt/ec_host_cmd.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <string.h>

#define RX_HEADER_SIZE (sizeof(struct ec_host_cmd_request_header))
#define TX_HEADER_SIZE (sizeof(struct ec_host_cmd_response_header))

/* This config says if the transport layer defines buffer for
 * read command. It depends if the transport layer does it in the driver and
 * it can be reused e.g. eSPI. Always defining the buffer by the transport
 * layer isn't a good solution, because more than one backends can be enabled,
 * and one of them is chosen in run-time. It would cause unnecessary memory
 * usage.
 */
#define TRANSPORT_DEFINE_MEMORY CONFIG_EC_HOST_CMD_PERIPH_ESPI


/*	COND_CODE_0(CONFIG_EC_HOST_CMD_PERIPH_ESPI, (static uint8_t _name##_buffer[256];),())     */
/*	COND_CODE_0(CONFIG_EC_HOST_CMD_PERIPH_ESPI, (.buff = _name##_buffer,),()) \ */

#define EC_HOST_CMD_DEFINE(_name)      \
	static uint8_t _name##_buffer[256]; \
	static K_KERNEL_STACK_DEFINE(_name##stack, CONFIG_EC_HOST_CMD_HANDLER_STACK_SIZE); \
	static struct k_thread _name##thread;				      \
	static struct ec_host_cmd _name = {			      \
		.rx_ctx = { \
			.buf = _name##_buffer, \
		}, \
		.thread = &_name##thread,				      \
		.stack = _name##stack,					      \
	}

EC_HOST_CMD_DEFINE(ec_host_cmd);

/**
 * Used by host command handlers for their response before going over wire.
 * Host commands handlers will cast this to respective response structures that may have fields of
 * uint32_t or uint64_t, so this buffer must be aligned to protect against the unaligned access.
 */
//static uint8_t tx_buffer[CONFIG_EC_HOST_CMD_HANDLER_TX_BUFFER] __aligned(8);

static uint8_t cal_checksum(const uint8_t *const buffer, const uint16_t size)
{
	uint8_t checksum = 0;

	for (size_t i = 0; i < size; ++i) {
		checksum += buffer[i];
	}
	return (uint8_t)(-checksum);
}

static void send_error_response(const struct ec_host_cmd_transport *iface,
				struct ec_host_cmd_tx_buf *tx,
				const enum ec_host_cmd_status error)
{
	struct ec_host_cmd_response_header *const tx_header = (void *)tx->buf;

	tx_header->prtcl_ver = 3;
	tx_header->result = error;
	tx_header->data_len = 0;
	tx_header->reserved = 0;
	tx_header->checksum = 0;
	tx_header->checksum = cal_checksum((uint8_t*)tx_header, TX_HEADER_SIZE);

	tx->len = TX_HEADER_SIZE;

	iface->api->send(iface, tx);
}

enum ec_host_cmd_status ec_host_cmd_handle_rx(const struct ec_host_cmd_transport *iface,
		struct ec_host_cmd_rx_ctx *rx, struct ec_host_cmd_tx_buf *tx)
{
	enum ec_host_cmd_status res;

	/* rx buf and len now have valid incoming data */
	if (rx->len < RX_HEADER_SIZE) {
		res = EC_HOST_CMD_REQUEST_TRUNCATED;
		goto bad_packet;
	}

	const struct ec_host_cmd_request_header *rx_header;
	if (rx->buf_tmp) {
		/* If received data is in a temporary buffer */
		rx_header = (struct ec_host_cmd_request_header *)rx->buf_tmp;
	} else {
		rx_header = (struct ec_host_cmd_request_header *)rx->buf;
	}

	/* Only support version 3 */
	if (rx_header->prtcl_ver != 3) {
		res = EC_HOST_CMD_INVALID_HEADER;
		goto bad_packet;
	}

	const uint16_t rx_valid_data_size =
		rx_header->data_len + RX_HEADER_SIZE;
	/*
	 * Ensure we received at least as much data as is expected.
	 * It is okay to receive more since some hardware interfaces
	 * add on extra padding bytes at the end.
	 */
	if (rx->len < rx_valid_data_size) {
		res = EC_HOST_CMD_REQUEST_TRUNCATED;
		goto bad_packet;
	}

	/* Validate checksum */
	if (cal_checksum((uint8_t *)rx_header, rx_valid_data_size) != 0) {
		res = EC_HOST_CMD_INVALID_CHECKSUM;
		goto bad_packet;
	}

	/* If received data is in a temporary buffer, copy it to the proper one */
	if (rx->buf_tmp) {
		memcpy(rx->buf, rx->buf_tmp, rx_valid_data_size);
	}

	return EC_HOST_CMD_SUCCESS;

bad_packet:
	send_error_response(iface, tx, res);
	return res;
}

static enum ec_host_cmd_status validate_handler(
	const struct ec_host_cmd_handler *handler,
	const struct ec_host_cmd_handler_args *args)
{
	if (handler->min_rqt_size > args->input_buf_size) {
		return EC_HOST_CMD_REQUEST_TRUNCATED;
	}

	if (handler->min_rsp_size > args->output_buf_max) {
		return EC_HOST_CMD_INVALID_RESPONSE;
	}

	if (args->version > sizeof(handler->version_mask) ||
	    !(handler->version_mask & BIT(args->version))) {
		return EC_HOST_CMD_INVALID_VERSION;
	}

	return EC_HOST_CMD_SUCCESS;
}

static enum ec_host_cmd_status prepare_response(
		struct ec_host_cmd_tx_buf *tx, uint16_t len)
{
	struct ec_host_cmd_response_header *const tx_header =
		(void *)tx->buf;

	tx_header->prtcl_ver = 3;
	tx_header->result = EC_HOST_CMD_SUCCESS;
	tx_header->data_len = len;
	tx_header->reserved = 0;

	const uint16_t tx_valid_data_size =
		tx_header->data_len + TX_HEADER_SIZE;

	if (tx_valid_data_size > tx->len_max) {
		return EC_HOST_CMD_INVALID_RESPONSE;
	}

	/* Calculate checksum */
	tx_header->checksum = 0;
	tx_header->checksum =
		cal_checksum(tx->buf, tx_valid_data_size);

	tx->len = tx_valid_data_size;

	return EC_HOST_CMD_SUCCESS;
}

static void ec_host_cmd_thread(void *hc_handle, void *arg2, void *arg3)
{
	struct ec_host_cmd *hc = (struct ec_host_cmd*)hc_handle;
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);
	struct ec_host_cmd_rx_ctx* rx = &hc->rx_ctx;
	enum ec_host_cmd_status hc_status;
	const struct ec_host_cmd_handler *found_handler;
	struct ec_host_cmd_tx_buf *tx = &hc->tx;
	enum ec_host_cmd_status handler_rv;
	/* The pointer to rx buffer is constant during communication */
	const struct ec_host_cmd_request_header * const rx_header =
		(void *)rx->buf;
	struct ec_host_cmd_handler_args args = {
		.output_buf = (uint8_t*)tx->buf + TX_HEADER_SIZE,
		.output_buf_max = tx->len_max - TX_HEADER_SIZE,
		.input_buf = rx->buf + RX_HEADER_SIZE,
		.reserved = NULL,
	};

	while (1) {
		/* Wait until and RX messages is received on host interface */
		k_sem_take(&rx->handler_owns, K_FOREVER);

		/*
		hc_status = validate_rx(rx);
		if (hc_status != EC_HOST_CMD_SUCCESS) {
			send_error_response(hc, hc_status);
			continue;
		}
		*/

		found_handler = NULL;

		STRUCT_SECTION_FOREACH(ec_host_cmd_handler, handler)
		{
			if (handler->id == rx_header->cmd_id) {
				found_handler = handler;
				break;
			}
		}

		/* No handler in this image for requested command */
		if (found_handler == NULL) {
			printk("DN: hc: inv_cmd, cmd_id: %d\n", rx_header->cmd_id);
			send_error_response(hc->iface, tx, EC_HOST_CMD_INVALID_COMMAND);
			continue;
		}

		args.command = rx_header->cmd_id;
		args.version = rx_header->cmd_ver;
		args.input_buf_size = rx_header->data_len;
		args.output_buf_size = 0;

		hc_status = validate_handler(found_handler, &args);
		if (hc_status != EC_HOST_CMD_SUCCESS) {
			send_error_response(hc->iface, tx, hc_status);
			continue;
		}

		handler_rv = found_handler->handler(&args);
		if (handler_rv != EC_HOST_CMD_SUCCESS) {
			send_error_response(hc->iface, tx, handler_rv);
			continue;
		}

		hc_status = prepare_response(tx, args.output_buf_size);
		if (hc_status != EC_HOST_CMD_SUCCESS) {
			send_error_response(hc->iface, tx, hc_status);
			continue;
		}

		hc->iface->api->send(hc->iface, tx);
	}
}

int ec_host_cmd_init(struct ec_host_cmd_transport *transport, void *transport_config)
{
	struct ec_host_cmd *hc = &ec_host_cmd;
	int ret;

	hc->iface = transport;

	/* Allow writing to rx buff at startup and block on reading. */
	k_sem_init(&hc->rx_ctx.handler_owns, 0, 1);

	ret = transport->api->init(transport, transport_config, &hc->rx_ctx, &hc->tx);
	if (ret != 0) {
		return ret;
	}

	// k_tid_t tid =
	k_thread_create(hc->thread, hc->stack,
			CONFIG_EC_HOST_CMD_HANDLER_STACK_SIZE,
			ec_host_cmd_thread, (void *)hc, NULL, NULL,
			CONFIG_EC_HOST_CMD_HANDLER_PRIO, 0, K_NO_WAIT);

	return 0;
}

