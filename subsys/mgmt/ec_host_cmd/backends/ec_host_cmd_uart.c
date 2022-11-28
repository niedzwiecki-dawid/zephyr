/*
 * Copyright (c) 2022 Google LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/ec_host_cmd_periph/ec_host_cmd_periph.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/mgmt/ec_host_cmd.h>

struct ec_host_cmd_uart {
	/* uart device instance */
	const struct device *uart_dev;
	/* Context for read operation */
	const struct ec_host_cmd_transport *transport;
	struct ec_host_cmd_rx_ctx *rx_ctx;
	struct ec_host_cmd_tx_buf *tx;
	/* Buffer to store received data, copied from uart shared memory */

//	uint32_t rx_buffer_len;
//	static struct host_cmd_ctrl_blk_uart ctrl_blk;
};

#define EC_HOST_CMD_UART_DEFINE(_name)					\
	static struct ec_host_cmd_uart _name##_hc_uart;			\
	struct ec_host_cmd_transport _name = {				\
		.api = &ec_host_cmd_api,				\
		.ctx = (struct ec_host_cmd_uart *)&_name##_hc_uart,	\
	}

static void tx_handle(struct ec_host_cmd_uart *hc_uart)
{
	printk("DN: unhandled tx isr\n");
}

static void rx_handle(struct ec_host_cmd_uart *hc_uart)
{
	int rx_len;
	uint8_t buf[256];

	printk("DN: hc rx uart isr\n");
	do {
		rx_len = uart_fifo_read(hc_uart->uart_dev, buf, 256);
		printk("DN: got %d bytes: \n");
		for (int i = 0; i < rx_len; i++) {
			printk("%x ", buf[i]);
		}
	} while(rx_len > 0);
	printk("\n");
}

static void uart_callback(const struct device *uart_dev, void *user_data)
{
	struct ec_host_cmd_uart *hc_uart = (struct ec_host_cmd_uart *)user_data;

	uart_irq_update(uart_dev);

	if (uart_irq_rx_ready(uart_dev)) {
		rx_handle(hc_uart);
	}

	if (uart_irq_tx_ready(uart_dev)) {
		tx_handle(hc_uart);
	}

	/*
	if (ec_host_cmd_handle_rx(hc_uart->transport, hc_uart->rx_ctx, hc_uart->tx) == EC_HOST_CMD_SUCCESS) {
		k_sem_give(&hc_uart->rx_ctx->handler_owns);
	}
	*/
}

static int ec_host_cmd_uart_init(const struct ec_host_cmd_transport *transport,
		const void *config, struct ec_host_cmd_rx_ctx *rx_ctx,
		struct ec_host_cmd_tx_buf *tx)
{
	struct ec_host_cmd_uart *hc_uart = (struct ec_host_cmd_uart*)transport->ctx;

	hc_uart->uart_dev = (const struct device *)config;

	if (!device_is_ready(hc_uart->uart_dev)) {
		return -ENODEV;
	}

	hc_uart->rx_ctx = rx_ctx;
	hc_uart->tx = tx;
	hc_uart->transport = transport;

	uart_irq_callback_user_data_set(dev, uart_callback, (void *)hc_uart);
	uart_irq_rx_enable(hc_uart->uart_dev);

	/* The UART backend reused memory defined in the host command handler */
	rx_ctx->buf_tmp = 0;

	return 0;
}

static int ec_host_cmd_uart_send(const struct ec_host_cmd_transport *transport,
				 const struct ec_host_cmd_tx_buf *buf)
{
	/*
	struct ec_host_cmd_uart *hc_uart = (struct ec_host_cmd_uart *)transport->ctx;
	struct ec_host_cmd_response_header *resp_hdr = buf->buf;
	uint32_t result = resp_hdr->result;
	*/

	/* For uart the tx and rx buffers are the same (shared_mem) */
	//memcpy(hc_uart->rx_ctx->buf_tmp, buf->buf, buf->len);

	/* Data to transfer are already in the buffer (shared_mem) */

	return 0;
}

static const struct ec_host_cmd_transport_api ec_host_cmd_api = {
	.init = &ec_host_cmd_uart_init,
	.send = &ec_host_cmd_uart_send,
};

EC_HOST_CMD_UART_DEFINE(ec_host_cmd_uart);
struct ec_host_cmd_transport *ec_host_cmd_backend_get_uart(void)
{
	return &ec_host_cmd_uart;
}

#if DT_NODE_EXISTS(DT_CHOSEN(zephyr_host_cmd_backend))
static int host_cmd_init(const struct device *arg)
{
	ARG_UNUSED(arg);
	const struct device *const dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_host_cmd_backend));

	printk("DN: ec_host_cmd_init uart \n");

	if (!device_is_ready(dev)) {
		return -ENODEV;
	}

	ec_host_cmd_init(ec_host_cmd_backend_get_uart(), (void*)dev);
	return 0;
}
SYS_INIT(host_cmd_init, POST_KERNEL, CONFIG_EC_HOST_CMD_INIT_PRIORITY);
#endif
