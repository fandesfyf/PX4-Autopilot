/****************************************************************************
 *
 *   Copyright (c) 2018 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file init.c
 *
 * PX4FMU-specific early startup code.  This file implements the
 * nsh_archinitialize() function that is called early by nsh during startup.
 *
 * Code here is run before the rcS script is invoked; it should start required
 * subsystems and perform board-specific initialisation.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <px4_config.h>
#include <px4_tasks.h>
#include <px4_log.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <debug.h>
#include <errno.h>

#include <nuttx/board.h>
#include <nuttx/spi/spi.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/sdio.h>
#include <nuttx/mmcsd.h>
#include <nuttx/analog/adc.h>
#include <nuttx/mm/gran.h>

#include <chip.h>
#include "board_config.h"

#include <stm32_uart.h>

#include <arch/board/board.h>

#include <drivers/drv_hrt.h>
#include <drivers/drv_board_led.h>

#include <systemlib/px4_macros.h>

#include <px4_init.h>
#include <px4_i2c.h>

#include "up_internal.h"

static int configure_switch(void);

/************************************************************************************
 * Name: board_rc_input
 *
 * Description:
 *   All boards my optionally provide this API to invert the Serial RC input.
 *   This is needed on SoCs that support the notion RXINV or TXINV as apposed to
 *   and external XOR controlled by a GPIO
 *
 ************************************************************************************/

__EXPORT void board_rc_input(bool invert_on, uint32_t uxart_base)
{

	irqstate_t irqstate = px4_enter_critical_section();

	uint32_t cr1 =	getreg32(STM32_USART_CR1_OFFSET + uxart_base);
	uint32_t cr2 =	getreg32(STM32_USART_CR2_OFFSET + uxart_base);
	uint32_t regval = cr1;

	/* {R|T}XINV bit fields can only be written when the USART is disabled (UE=0). */

	regval &= ~USART_CR1_UE;

	putreg32(regval, STM32_USART_CR1_OFFSET + uxart_base);

	if (invert_on) {
#if defined(BOARD_HAS_RX_TX_SWAP) &&	RC_SERIAL_PORT_IS_SWAPED == 1

		/* This is only ever turned on */

		cr2 |= (USART_CR2_RXINV | USART_CR2_TXINV | USART_CR2_SWAP);
#else
		cr2 |= (USART_CR2_RXINV | USART_CR2_TXINV);
#endif

	} else {
		cr2 &= ~(USART_CR2_RXINV | USART_CR2_TXINV);
	}

	putreg32(cr2, STM32_USART_CR2_OFFSET + uxart_base);
	putreg32(cr1, STM32_USART_CR1_OFFSET + uxart_base);

	leave_critical_section(irqstate);
}

/************************************************************************************
 * Name: board_on_reset
 *
 * Description:
 * Optionally provided function called on entry to board_system_reset
 * It should perform any house keeping prior to the rest.
 *
 * status - 1 if resetting to boot loader
 *          0 if just resetting
 *
 ************************************************************************************/
__EXPORT void board_on_reset(int status)
{
	/* configure the GPIO pins to outputs and keep them low */

	const uint32_t gpio[] = PX4_GPIO_PWM_INIT_LIST;
	board_gpio_init(gpio, arraySize(gpio));

	if (status >= 0) {
		up_mdelay(6);
	}
}


/************************************************************************************
 * Name: stm32_boardinitialize
 *
 * Description:
 *   All STM32 architectures must provide the following entry point.  This entry point
 *   is called early in the initialization -- after all memory has been configured
 *   and mapped but before any devices have been initialized.
 *
 ************************************************************************************/

__EXPORT void
stm32_boardinitialize(void)
{
	board_on_reset(-1); /* Reset PWM first thing */

	/* configure LEDs */
	board_autoled_initialize();

	/* configure pins */
	const uint32_t gpio[] = PX4_GPIO_INIT_LIST;
	board_gpio_init(gpio, arraySize(gpio));

	/* configure SPI interfaces */
	stm32_spiinitialize();
}

/****************************************************************************
 * Name: board_app_initialize
 *
 * Description:
 *   Perform application specific initialization.  This function is never
 *   called directly from application code, but only indirectly via the
 *   (non-standard) boardctl() interface using the command BOARDIOC_INIT.
 *
 * Input Parameters:
 *   arg - The boardctl() argument is passed to the board_app_initialize()
 *         implementation without modification.  The argument has no
 *         meaning to NuttX; the meaning of the argument is a contract
 *         between the board-specific initialization logic and the the
 *         matching application logic.  The value cold be such things as a
 *         mode enumeration value, a set of DIP switch switch settings, a
 *         pointer to configuration data read from a file or serial FLASH,
 *         or whatever you would like to do with it.  Every implementation
 *         should accept zero/NULL as a default configuration.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned on
 *   any failure to indicate the nature of the failure.
 *
 ****************************************************************************/


__EXPORT int board_app_initialize(uintptr_t arg)
{
	px4_platform_init();

	/* configure the DMA allocator */

	if (board_dma_alloc_init() < 0) {
		PX4_ERR("DMA alloc FAILED");
	}

	/* set up the serial DMA polling */
	static struct hrt_call serial_dma_call;
	struct timespec ts;

	/*
	 * Poll at 1ms intervals for received bytes that have not triggered
	 * a DMA event.
	 */
	ts.tv_sec = 0;
	ts.tv_nsec = 1000000;

	hrt_call_every(&serial_dma_call,
		       ts_to_abstime(&ts),
		       ts_to_abstime(&ts),
		       (hrt_callout)stm32_serial_dma_poll,
		       NULL);

	(void) board_hardfault_init(2, true);


#ifdef CONFIG_SPI
	int ret = stm32_spi_bus_initialize();

	if (ret != OK) {
		PX4_ERR("SPI init failed");
		return ret;
	}

#endif

#ifdef CONFIG_MMCSD

	ret = stm32_sdio_initialize();

	if (ret != OK) {
		PX4_ERR("SDIO init failed");
		return ret;
	}

#endif

	configure_switch();

	return OK;
}

/************************************************************************************
 * Name: configure_switch
 *
 * Description:
 * Configure KSZ9897R ethernet switch on i2c
 *
 ************************************************************************************/
static int configure_switch(void)
{
	int ret = PX4_ERROR;

	// attach to the i2c bus
	struct i2c_master_s *i2c = px4_i2cbus_initialize(PX4_I2C_BUS_ONBOARD);

	if (i2c == NULL) {
		PX4_ERR("I2C device not opened");
	}

	// ethernet switch enable
	uint8_t txdata[] = {0x51, 0x00, 0x21, 0x00}; //0x5100, 0x2100 MSB to LSB here.

	struct px4_i2c_msg_t msgv;

	msgv.frequency = 100000;
	msgv.addr = 0x5F;
	msgv.flags = 0;
	msgv.buffer = &txdata[0];
	msgv.length = sizeof(txdata);

	unsigned retry_count = 0;
	const unsigned retries = 5;

	do {
		ret = I2C_TRANSFER(i2c, &msgv, 1);

		/* success */
		if (ret == PX4_OK) {
			break;

		} else {
			PX4_ERR("ETH switch I2C fail: %d, retrying", ret);
		}

		/* if we have already retried once, or we are going to give up, then reset the bus */
		if ((retry_count >= 1) || (retry_count >= retries)) {
			I2C_RESET(i2c);
		}

	} while (retry_count++ < retries);

	px4_i2cbus_uninitialize(i2c);

	return ret;
}
