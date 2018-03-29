/************************************************************************************
 * Included Files
 ************************************************************************************/

#include <px4_config.h>
#include <px4_log.h>

#include <stdint.h>
#include <stdbool.h>
#include <debug.h>
#include <unistd.h>

#include <nuttx/spi/spi.h>
#include <arch/board/board.h>
#include <systemlib/px4_macros.h>

#include <up_arch.h>
#include <chip.h>
#include <stm32_gpio.h>
#include "board_config.h"
#include <systemlib/err.h>

/****************************************************************************
 * Pre-Processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/* Debug ********************************************************************/

/* Define CS GPIO array */
static const uint32_t spi1selects_gpio[] = PX4_FLOW_BUS_CS_GPIO;


/************************************************************************************
 * Public Functions
 ************************************************************************************/

/************************************************************************************
 * Name: stm32_spiinitialize
 *
 * Description:
 *   Called to configure SPI chip select GPIO pins for the PX4FMU board.
 *
 ************************************************************************************/

__EXPORT void stm32_spiinitialize(void)
{
#ifdef CONFIG_STM32_SPI1
	board_gpio_init(spi1selects_gpio, arraySize(spi1selects_gpio));
#endif

}

/************************************************************************************
 * Name: stm32_spi_bus_initialize
 *
 * Description:
 *   Called to configure SPI buses on PX4FMU board.
 *
 ************************************************************************************/
static struct spi_dev_s *spi_expansion;

__EXPORT int stm32_spi_bus_initialize(void)
{
	/* Configure SPI-based devices */

	/* Get the SPI port for the Sensors */

	spi_expansion = stm32_spibus_initialize(PX4_SPI_BUS_EXPANSION);

	if (!spi_expansion) {
		PX4_ERR("[boot] FAILED to initialize SPI port %d\n", PX4_SPI_BUS_EXPANSION);
		return -ENODEV;
	}

	/* Default PX4_SPI_BUS_SENSORS to 1MHz and de-assert the known chip selects. */

	SPI_SETFREQUENCY(spi_expansion, 10000000);
	SPI_SETBITS(spi_expansion, 8);
	SPI_SETMODE(spi_expansion, SPIDEV_MODE3);

	for (int cs = PX4_FLOW_BUS_FIRST_CS; cs <= PX4_FLOW_BUS_LAST_CS; cs++) {
		SPI_SELECT(spi_expansion, cs, false);
	}

	return OK;

}

/************************************************************************************
 * Name: stm32_spi1select and stm32_spi1status
 *
 * Description:
 *   Called by stm32 spi driver on bus 1.
 *
 ************************************************************************************/

__EXPORT void stm32_spi1select(FAR struct spi_dev_s *dev, uint32_t devid, bool selected)
{
	/* SPI select is active low, so write !selected to select the device */

	int sel = (int) devid;
	ASSERT(PX4_SPI_BUS_ID(sel) == PX4_SPI_BUS_EXPANSION);

	/* Making sure the other peripherals are not selected */

	for (size_t cs = 0; arraySize(spi1selects_gpio) > 1 && cs < arraySize(spi1selects_gpio); cs++) {
		if (spi1selects_gpio[cs] != 0) {
			stm32_gpiowrite(spi1selects_gpio[cs], 1);
		}
	}

	uint32_t gpio = spi1selects_gpio[PX4_SPI_DEV_ID(sel)];

	if (gpio) {
		stm32_gpiowrite(gpio, !selected);
	}
}

__EXPORT uint8_t stm32_spi1status(FAR struct spi_dev_s *dev, uint32_t devid)
{
	return SPI_STATUS_PRESENT;
}


/************************************************************************************
 * Name: board_spi_reset
 *
 * Description:
 *
 *
 ************************************************************************************/

__EXPORT void board_spi_reset(int ms)
{
	/* disable SPI bus */
	for (size_t cs = 0;  arraySize(spi1selects_gpio) > 1 && cs < arraySize(spi1selects_gpio); cs++) {
		if (spi1selects_gpio[cs] != 0) {
			stm32_configgpio(_PIN_OFF(spi1selects_gpio[cs]));
		}
	}

	stm32_configgpio(GPIO_SPI1_SCK_OFF);
	stm32_configgpio(GPIO_SPI1_MISO_OFF);
	stm32_configgpio(GPIO_SPI1_MOSI_OFF);

	/* set the sensor rail off */
	//stm32_gpiowrite(GPIO_VDD_3V3_SENSORS_EN, 0);

	/* wait for the sensor rail to reach GND */
	usleep(ms * 1000);
	warnx("reset done, %d ms", ms);

	/* re-enable power */

	/* switch the sensor rail back on */
	//stm32_gpiowrite(GPIO_VDD_3V3_SENSORS_EN, 1);

	/* wait a bit before starting SPI, different times didn't influence results */
	usleep(100);

	/* reconfigure the SPI pins */
	for (size_t cs = 0; arraySize(spi1selects_gpio) > 1 && cs < arraySize(spi1selects_gpio); cs++) {
		if (spi1selects_gpio[cs] != 0) {
			stm32_configgpio(spi1selects_gpio[cs]);
		}
	}

	stm32_configgpio(GPIO_SPI1_SCK);
	stm32_configgpio(GPIO_SPI1_MISO);
	stm32_configgpio(GPIO_SPI1_MOSI);

}
