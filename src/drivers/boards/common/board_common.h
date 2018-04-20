/****************************************************************************
 *
 *   Copyright (C) 2016 PX4 Development Team. All rights reserved.
 *                 Author: David Sidrane <david_s5@nscdg.com>
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
 * @file board_common.h
 *
 * Provide the the common board interfaces
 */

#pragma once

/************************************************************************************
 * Included Files
 ************************************************************************************/
#include <errno.h>
#include <stdint.h>
/************************************************************************************
 * Definitions
 ************************************************************************************/

/* SPI bus defining tools
 *
 * For new boards we use a board_config.h to define all the SPI functionality
 *  A board provides SPI bus definitions and a set of buses that should be
 *  enumerated as well as chip selects that will be iterateable
 *
 * We will use these in macros place of the uint32_t enumeration to select a
 * specific SPI device on given SPI1 bus.
 *
 * These macros will define BUS:DEV For clarity and indexing
 *
 * The board config then defines:
 * 1) PX4_SPI_BUS_xxx Ids -the buses as a 1 based PX4_SPI_BUS_xxx as n+1 aping to SPI n
 *       where n is {1-highest SPI supported by SoC}
 * 2) PX4_SPIDEV_yyyy handles - PX4_SPIDEV_xxxxx handles using the macros below.
 * 3) PX4_xxxx_BUS_CS_GPIO - a set of chip selects that are indexed by the handles. and of set to 0 are
 *       ignored.
 * 4) PX4_xxxx_BUS_FIRST_CS and PX4_xxxxx_BUS_LAST_CS as  PX4_SPIDEV_lll for the first CS and
 *       PX4_SPIDEV_hhhh as the last CS
 *
 * Example:
 *
 * The PX4_SPI_BUS_xxx
 * #define PX4_SPI_BUS_SENSORS  1
 * #define PX4_SPI_BUS_MEMORY   2
 *
 * the PX4_SPIDEV_yyyy
 * #define PX4_SPIDEV_ICM_20689      PX4_MK_SPI_SEL(PX4_SPI_BUS_SENSORS,0)
 * #define PX4_SPIDEV_ICM_20602      PX4_MK_SPI_SEL(PX4_SPI_BUS_SENSORS,1)
 * #define PX4_SPIDEV_BMI055_GYRO    PX4_MK_SPI_SEL(PX4_SPI_BUS_SENSORS,2)
 *
 * The PX4_xxxx_BUS_CS_GPIO
 * #define PX4_SENSOR_BUS_CS_GPIO    {GPIO_SPI_CS_ICM20689, GPIO_SPI_CS_ICM20602, GPIO_SPI_CS_BMI055_GYR,...
 *
 * The PX4_xxxx_BUS_FIRST_CS and PX4_xxxxx_BUS_LAST_CS
 * #define PX4_SENSORS_BUS_FIRST_CS  PX4_SPIDEV_ICM_20689
 * #define PX4_SENSORS_BUS_LAST_CS   PX4_SPIDEV_BMI055_ACCEL
 *
 *
 */
#define PX4_SPIDEV_ID(type, index)  ((((type) & 0xffff) << 16) | ((index) & 0xffff))

#define PX4_SPI_DEVICE_ID         (1 << 12)
#define PX4_MK_SPI_SEL(b,d)       PX4_SPIDEV_ID(PX4_SPI_DEVICE_ID, ((((b) & 0xff) << 8) | ((d) & 0xff)))
#define PX4_SPI_BUS_ID(devid)     (((devid) >> 8) & 0xff)
#define PX4_SPI_DEV_ID(devid)     ((devid) & 0xff)
#define PX4_CHECK_ID(devid)       ((devid) & PX4_SPI_DEVICE_ID)

/* I2C PX4 clock configuration
 *
 * A board may override BOARD_NUMBER_I2C_BUSES and BOARD_I2C_BUS_CLOCK_INIT
 * simply by defining the #defines.
 *
 * If none are provided the default number of I2C busses  will be taken from
 * the px4 micro hal and the init will be from the legacy values of 100K.
 */
#if !defined(BOARD_NUMBER_I2C_BUSES)
# define BOARD_NUMBER_I2C_BUSES PX4_NUMBER_I2C_BUSES
#endif

#if !defined(BOARD_I2C_BUS_CLOCK_INIT)
#  if (BOARD_NUMBER_I2C_BUSES) == 1
#    define BOARD_I2C_BUS_CLOCK_INIT {100000}
#  elif (BOARD_NUMBER_I2C_BUSES) == 2
#    define BOARD_I2C_BUS_CLOCK_INIT {100000, 100000}
#  elif (BOARD_NUMBER_I2C_BUSES) == 3
#    define BOARD_I2C_BUS_CLOCK_INIT {100000, 100000, 100000}
#  elif (BOARD_NUMBER_I2C_BUSES) == 4
#    define BOARD_I2C_BUS_CLOCK_INIT {100000, 100000, 100000, 100000}
#  else
#    error BOARD_NUMBER_I2C_BUSES not supported
#  endif
#endif
/* ADC defining tools
 * We want to normalize the V5 Sensing to V = (adc_dn) * ADC_V5_V_FULL_SCALE/(2 ^ ADC_BITS) * ADC_V5_SCALE)
 */

/* Provide overrideable defaults ADC Full scale ranges and Divider ratios
 * If the board has a different ratio or full scale range for any voltage sensing
 * the board_congig.h file should define the constants that differ from these
 * defaults
 */
#if !defined(ADC_V5_V_FULL_SCALE)
#define ADC_V5_V_FULL_SCALE             (6.6f)  // 5 volt Rail full scale voltage
#endif
#if !defined(ADC_V5_SCALE)
#define ADC_V5_SCALE                    (2.0f) // The scale factor defined by HW's resistive divider (Rt+Rb)/ Rb
#endif

#if !defined(ADC_3V3_V_FULL_SCALE)
#define ADC_3V3_V_FULL_SCALE             (3.6f)  // 3.3V volt Rail full scale voltage
#endif
#if !defined(ADC_3V3_SCALE)
#define ADC_3V3_SCALE                    (2.0f) // The scale factor defined by HW's resistive divider (Rt+Rb)/ Rb
#endif

/* Provide define for Bricks and Battery */

/* Define the default maximum voltage resulting from the bias on ADC termination */

#if !defined(BOARD_ADC_OPEN_CIRCUIT_V)
#  define BOARD_ADC_OPEN_CIRCUIT_V  (1.5f)
#endif

/* Define the default Under voltage Window on the LTC4417 as set by resistors on the
 * board. Default is that of the FMUv2 at 3.7V
 */

#if !defined(BOARD_VALID_UV)
#  define BOARD_VALID_UV  (3.7f)
#endif

/* Legacy default */

#if !defined(BOARD_NUMBER_BRICKS)
#  define BOARD_NUMBER_BRICKS 1
#  if !defined(BOARD_ADC_BRICK_VALID)
#    define BOARD_ADC_BRICK_VALID (1)
#  endif
#endif

#if BOARD_NUMBER_BRICKS == 0
/* allow SITL to disable all bricks */
#elif BOARD_NUMBER_BRICKS == 1
#  define BOARD_BATT_V_LIST       {ADC_BATTERY_VOLTAGE_CHANNEL}
#  define BOARD_BATT_I_LIST       {ADC_BATTERY_CURRENT_CHANNEL}
#  define BOARD_BRICK_VALID_LIST  {BOARD_ADC_BRICK_VALID}
#elif BOARD_NUMBER_BRICKS == 2
#  define BOARD_BATT_V_LIST       {ADC_BATTERY1_VOLTAGE_CHANNEL, ADC_BATTERY2_VOLTAGE_CHANNEL}
#  define BOARD_BATT_I_LIST       {ADC_BATTERY1_CURRENT_CHANNEL, ADC_BATTERY2_CURRENT_CHANNEL}
#  define BOARD_BRICK_VALID_LIST  {BOARD_ADC_BRICK1_VALID, BOARD_ADC_BRICK2_VALID}
#elif BOARD_NUMBER_BRICKS == 3
#  define BOARD_BATT_V_LIST       {ADC_BATTERY1_VOLTAGE_CHANNEL, ADC_BATTERY2_VOLTAGE_CHANNEL, ADC_BATTERY3_VOLTAGE_CHANNEL}
#  define BOARD_BATT_I_LIST       {ADC_BATTERY1_CURRENT_CHANNEL, ADC_BATTERY2_CURRENT_CHANNEL, ADC_BATTERY3_CURRENT_CHANNEL}
#  define BOARD_BRICK_VALID_LIST  {BOARD_ADC_BRICK1_VALID, BOARD_ADC_BRICK2_VALID, BOARD_ADC_BRICK3_VALID}
#elif BOARD_NUMBER_BRICKS == 4
#  define BOARD_BATT_V_LIST       {ADC_BATTERY1_VOLTAGE_CHANNEL, ADC_BATTERY2_VOLTAGE_CHANNEL, ADC_BATTERY3_VOLTAGE_CHANNEL, ADC_BATTERY4_VOLTAGE_CHANNEL}
#  define BOARD_BATT_I_LIST       {ADC_BATTERY1_CURRENT_CHANNEL, ADC_BATTERY2_CURRENT_CHANNEL, ADC_BATTERY3_CURRENT_CHANNEL, ADC_BATTERY4_CURRENT_CHANNEL}
#  define BOARD_BRICK_VALID_LIST  {BOARD_ADC_BRICK1_VALID, BOARD_ADC_BRICK2_VALID, BOARD_ADC_BRICK3_VALID, BOARD_ADC_BRICK4_VALID}
#else
#  error Unsuported BOARD_NUMBER_BRICKS number.
#endif

/* Choose the source for ADC_SCALED_V5_SENSE */
#if defined(ADC_5V_RAIL_SENSE)
#define ADC_SCALED_V5_SENSE ADC_5V_RAIL_SENSE
#else
#  if defined(ADC_SCALED_V5_CHANNEL)
#    define ADC_SCALED_V5_SENSE ADC_SCALED_V5_CHANNEL
#  endif
#endif

/* Define the source for ADC_SCALED_V3V3_SENSORS_SENSE */

#if defined(ADC_SCALED_VDD_3V3_SENSORS_CHANNEL)
#  define ADC_SCALED_V3V3_SENSORS_SENSE ADC_SCALED_VDD_3V3_SENSORS_CHANNEL
#endif

/* Define an overridable default of 0.0f V for batery v div
 * This is done to ensure the missing default trips a low
 * voltage lockdown
 */
#if !defined(BOARD_BATTERY1_V_DIV)
#define BOARD_BATTERY1_V_DIV 0.0f
#endif

#if !defined(BOARD_BATTERY2_V_DIV)
#define BOARD_BATTERY2_V_DIV 0.0f
#endif

/* Define an overridable default of 0.0f for A per V
 * This is done to ensure the default leads to an
 * unrealistic current value
 */
#if !defined(BOARD_BATTERY1_A_PER_V)
#define BOARD_BATTERY1_A_PER_V 0.0f
#endif

#if !defined(BOARD_BATTERY2_A_PER_V)
#define BOARD_BATTERY2_A_PER_V 0.0f
#endif

/* Conditional use of FMU GPIO
 * If the board use the PX4FMU driver and the board provides
 * BOARD_FMU_GPIO_TAB then we publish the logical BOARD_HAS_FMU_GPIO
 */
#if defined(BOARD_FMU_GPIO_TAB)
#  define BOARD_HAS_FMU_GPIO
#endif

/* Conditional use of PX4 PIO is Used to determine if the board
 * has a PX4IO processor.
 * We then publish the logical BOARD_USES_PX4IO
 */
#if defined(BOARD_USES_PX4IO_VERSION)
#  define BOARD_USES_PX4IO	1
/*  Allow a board_config to override the PX4IO FW search paths */
#  if defined(BOARD_PX4IO_FW_SEARCH_PATHS)
#    define PX4IO_FW_SEARCH_PATHS BOARD_PX4IO_FW_SEARCH_PATHS
#  else
/*  Use PX4IO FW search paths defaults based on version */
#    if BOARD_USES_PX4IO_VERSION == 2
#      define PX4IO_FW_SEARCH_PATHS {"/etc/extras/px4io-v2.bin", "/fs/microsd/px4io2.bin", "/fs/microsd/px4io.bin", nullptr }
#    endif
#  endif
#endif

/* Provide an overridable default nop
 * for BOARD_EEPROM_WP_CTRL
 */
#if !defined(BOARD_EEPROM_WP_CTRL)
#  define BOARD_EEPROM_WP_CTRL(on_true)
#endif

/*
 * Defined when a board has capture and uses channels.
 */
#if defined(DIRECT_INPUT_TIMER_CHANNELS) && DIRECT_INPUT_TIMER_CHANNELS > 0
#define BOARD_HAS_CAPTURE 1
#endif

/*
 * Defined when a supports version and type API.
 */
#if defined(BOARD_HAS_SIMPLE_HW_VERSIONING)
#  define BOARD_HAS_VERSIONING 1
#  define HW_VER_SIMPLE(s)	     0x90000+(s)

#  define HW_VER_FMUV2           HW_VER_SIMPLE(HW_VER_FMUV2_STATE)
#  define HW_VER_FMUV3           HW_VER_SIMPLE(HW_VER_FMUV3_STATE)
#  define HW_VER_FMUV2MINI       HW_VER_SIMPLE(HW_VER_FMUV2MINI_STATE)
#endif

#if defined(BOARD_HAS_HW_VERSIONING)
#  define BOARD_HAS_VERSIONING 1
#endif

/************************************************************************************
 * Public Data
 ************************************************************************************/

/* board reset control */

typedef enum board_reset_e {
	board_reset_normal           = 0,  /* Perform a normal reset */
	board_reset_extended         = 1,  /* Perform an extend reset as defined by board */
	board_reset_power_off        = 2,  /* Reset to the boot loader, signaling a power off */
	board_reset_enter_bootloader = 3   /* Perform a reset to the boot loader */
} board_reset_e;

/* board power button state notification */

typedef enum board_power_button_state_notification_e {
	PWR_BUTTON_IDEL,                       /* Button went up without meeting shutdown button down time */
	PWR_BUTTON_DOWN,                       /* Button went Down */
	PWR_BUTTON_UP,                         /* Button went Up */
	PWR_BUTTON_REQUEST_SHUT_DOWN,          /* Button went up after meeting shutdown button down time */

	PWR_BUTTON_RESPONSE_SHUT_DOWN_PENDING, /* Response from call back board code does nothing the
                                            * expectation is that board_shutdown will be called.
                                            */
	PWR_BUTTON_RESPONSE_SHUT_DOWN_NOW,     /* Response from call back board code does shutdown now. */
} board_power_button_state_notification_e;

/* board call back signature  */

typedef int (*power_button_state_notification_t)(board_power_button_state_notification_e request);


/* UUID
 *
 * Define the types used for board UUID, MFG UID and PX4 GUID
 *
 * A type suitable for holding the byte format of the UUID
 *
 * The original PX4 stm32 (legacy) based implementation **displayed** the
 * UUID as: ABCD EFGH IJKL
 * Where:
 *       A was bit 31 and D was bit 0
 *       E was bit 63 and H was bit 32
 *       I was bit 95 and L was bit 64
 *
 * Since the string was used by some manufactures to identify the units
 * it must be preserved. DEPRECATED - This will be removed in PX4 Release
 * 1.7.0.
 *
 * For new targets moving forward we will use an 18 byte globally unique
 * PX4 GUID in the form:
 *
 *           <ARCH MSD><ARCH LSD>[MNOP] IJKL EFGH ABCD
 *
 *  Where <ARCH MSD><ARCH LSD> are a monotonic ordinal number assigned by
 *  PX4 to a chip architecture (PX4_SOC_ARCH_ID). The 2 bytes are used to
 *  create a globally unique ID when prepended to a padded CPU UUID.
 *
 *  In the case where the MFG UUID is shorter than 16 bytes it will be
 *  padded with 0's starting at offset [2] until
 *  PX4_GUID_BYTE_LENGTH-PX4_CPU_UUID_BYTE_LENGTH -1
 *
 *  I.E. For the STM32
 *  offset:0         1     2  3  4  5  6             -            17
 *    <ARCH MSD><ARCH LSD>[0][0][0][0]<MSD CPU UUID>...<LSD CPU UUID>
 *
 *  I.E. For the Kinetis
 *  offset:0         1         2         -           17
 *    <ARCH MSD><ARCH LSD><MSD CPU UUID>...<LSD CPU UUID>
 */

/* Define the PX4 Globally unique ID (GUID) length and format size */
#define PX4_GUID_BYTE_LENGTH              18
#define PX4_GUID_FORMAT_SIZE              ((2*PX4_GUID_BYTE_LENGTH)+1)

/* DEPRICATED as of 1.7.0 A type suitable for defining the 8 bit format of the CPU UUID */
typedef uint8_t uuid_byte_t[PX4_CPU_UUID_BYTE_LENGTH];

/* DEPRICATED as of 1.7.0  A type suitable for defining the 32bit format of the CPU UUID */
typedef uint32_t uuid_uint32_t[PX4_CPU_UUID_WORD32_LENGTH];

/* A type suitable for defining the 8 bit format of the MFG UID
 * This is always returned as MSD @ index 0 -LSD @ index PX4_CPU_MFGUID_BYTE_LENGTH-1
 */
typedef uint8_t mfguid_t[PX4_CPU_MFGUID_BYTE_LENGTH];

/* A type suitable for defining the 8 bit format of the px4 globally unique
 * PX4 GUID. This is always returned as MSD @ index 0 -LSD @ index
 * PX4_CPU_GUID_BYTE_LENGTH-1
 */
typedef uint8_t px4_guid_t[PX4_GUID_BYTE_LENGTH];

/************************************************************************************
 * Private Functions
 ************************************************************************************/

/************************************************************************************
 * Public Functions
 ************************************************************************************/

/* Provide an interface for reading the connected state of VBUS */

/************************************************************************************
 * Name: board_read_VBUS_state
 *
 * Description:
 *   All boards must provide a way to read the state of VBUS, this my be simple
 *   digital input on a GPIO. Or something more complicated like a Analong input
 *   or reading a bit from a USB controller register.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   0 if connected.
 *
 ************************************************************************************/

#if defined(GPIO_OTGFS_VBUS)
#  define board_read_VBUS_state() (px4_arch_gpioread(GPIO_OTGFS_VBUS) ? 0 : 1)
#else
int board_read_VBUS_state(void);
#endif

/************************************************************************************
 * Name: board_dma_alloc_init
 *
 * Description:
 *   All boards may optionally provide this API to instantiate a pool of
 *   memory for uses with FAST FS DMA operations.
 *
 *   Provision is controlled by declaring BOARD_DMA_ALLOC_POOL_SIZE in board_config.h
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned on failure
 *   EPERM - board does not support function
 *   ENOMEM - There is not enough memory to satisfy allocation.
 *
 ************************************************************************************/
#if defined(BOARD_DMA_ALLOC_POOL_SIZE)
__EXPORT int board_dma_alloc_init(void);
#else
#define board_dma_alloc_init() (-EPERM)
#endif

/************************************************************************************
 * Name: board_get_dma_usage
 *
 * Description:
 *   All boards may optionally provide this API to supply instrumentation for a pool of
 *   memory used for DMA operations.
 *
 *   Provision is controlled by declaring BOARD_DMA_ALLOC_POOL_SIZE in board_config.h
 *
 * Input Parameters:
 *   dma_total     -  A pointer to receive the total allocation size of the memory
 *                    allocated with board_dma_alloc_init. It should be equal to
 *                    BOARD_DMA_ALLOC_POOL_SIZE.
 *   dma_used      -  A pointer to receive the current allocation in use.
 *   dma_peak_used -  A pointer to receive the peak allocation used.
 *
 * Returned Value:
 *   Zero (OK) is returned on success;
 *
 ************************************************************************************/
#if defined(BOARD_DMA_ALLOC_POOL_SIZE)
__EXPORT int board_get_dma_usage(uint16_t *dma_total, uint16_t *dma_used, uint16_t *dma_peak_used);
#else
#define board_get_dma_usage(dma_total,dma_used, dma_peak_used) (-ENOMEM)
#endif

/************************************************************************************
 * Name: board_rc_input
 *
 * Description:
 *   All boards my optionally provide this API to invert the Serial RC input.
 *   This is needed on SoCs that support the notion RXINV or TXINV as opposed to
 *   and external XOR controlled by a GPIO
 *
 * Input Parameters:
 *   invert_on - A positive logic value, that when true (on) will set the HW in
 *               inverted NRZ mode where a MARK will be 0 and SPACE will be a 1.
 *
 * Returned Value:
 *   None
 *
 ************************************************************************************/
#if defined(INVERT_RC_INPUT)
#  if !defined(GPIO_SBUS_INV)
__EXPORT void board_rc_input(bool invert_on);
#  endif
#endif

/************************************************************************************
 * Name: board_on_reset
 *
 * Description:
 * Optionally provided function called on entry to board_system_reset
 * It should perform any house keeping prior to the rest.
 * For example setting PWM outputs to the off state to avoid
 * triggering a motor spin.
 *
 * As a workaround for the bug seen on some ESC, the code will delay
 * rebooting the flight controller to insure that the 3.2 ms pulse
 * that occurs from the delay from reset to GPIO init due to memory
 * initialization is pushed out from the last PWM by > 6 Ms.
 *
 * Input Parameters:
 *  status - 1 Resetting to boot loader
 *           0 Just resetting CPU
 *           -1 used internally by board init to to initialize
 *            PWM IO pins with delay.
 *
 * Returned Value:
 *   None
 *
 ************************************************************************************/

#if defined(BOARD_HAS_NO_RESET) || !defined(BOARD_HAS_ON_RESET)
#  define board_on_reset(status)
#else
__EXPORT void board_on_reset(int status);
#endif

/************************************************************************************
 * Name: board_reset
 *
 * Description:
 *   All boards my optionally provide this API to reset the board
 *
 * Input Parameters:
 *  status - 1 Resetting to boot loader
 *           0 Just resetting CPU
 *
 * Returned Value:
 *   If function is supported by board it will not return.
 *   If not supported it is a noop.
 *
 ************************************************************************************/
#if defined(BOARD_HAS_NO_RESET)
#  define board_system_reset(status)
#else
__EXPORT void board_system_reset(int status) noreturn_function;
#endif

/************************************************************************************
 * Name: board_set_bootload_mode
 *
 * Description:
 *   All boards my optionally provide this API to enter configure the entry to
 *   boot loader mode on the next system reset.
 *
 * Input Parameters:
 *   mode -  is an board_reset_e that controls the type of reset.
 *           board_reset_normal  Perform a normal reset
 *           board_reset_extended Perform an extend reset as defined by board
 *           board_reset_power_off Reset to the boot loader, signaling a power off
 *           board_reset_enter_bootloader  Perform a reset to the boot loader
 *
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated EINVAL value is returned if an
 *             invalid mode is requested.
 *
 ************************************************************************************/

#if defined(BOARD_HAS_NO_BOOTLOADER)
#  define board_set_bootload_mode(mode)
#else
__EXPORT int board_set_bootload_mode(board_reset_e mode);
#endif

/************************************************************************************
 * Name: board_get_hw_type
 *
 * Description:
 *   Optional returns a 0 terminated string defining the HW type.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   a 0 terminated string defining the HW type. This may be a 0 length string ""
 *
 ************************************************************************************/

#if defined(BOARD_HAS_VERSIONING)
__EXPORT const char *board_get_hw_type_name(void);
#else
#define board_get_hw_type_name() ""
#endif

/************************************************************************************
 * Name: board_get_hw_version
 *
 * Description:
 *   Optional returns a integer HW version
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   An integer value of this boards hardware version.
 *   A value of -1 is the default for boards not supporting the BOARD_HAS_VERSIONING API.
 *   A value of 0 is the default for boards supporting the API but not having version.
 *
 ************************************************************************************/

#if defined(BOARD_HAS_VERSIONING)
__EXPORT int board_get_hw_version(void);
#else
#define board_get_hw_version() (-1)
#endif

/************************************************************************************
 * Name: board_get_hw_revision
 *
 * Description:
 *   Optional returns a integer HW revision
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   An integer value of this boards hardware revision.
 *   A value of -1 is the default for boards not supporting the BOARD_HAS_VERSIONING API.
 *   A value of 0 is the default for boards supporting the API but not having revision.
 *
 ************************************************************************************/

#if defined(BOARD_HAS_VERSIONING)
__EXPORT int board_get_hw_revision(void);
#else
#define board_get_hw_revision() (-1)
#endif

#if !defined(BOARD_OVERRIDE_UUID)
/************************************************************************************
 * Name: board_get_uuid DEPRICATED use board_get_px4_guid
 *
 * Description:
 *   All boards either provide a way to read a uuid of PX4_CPU_UUID_BYTE_LENGTH
 *   from PX4_CPU_UUID_ADDRESS in the SoC's address space OR define
 *   BOARD_OVERRIDE_UUID as an array of bytes that is PX4_CPU_UUID_BYTE_LENGTH
 *
 * Input Parameters:
 *   uuid_bytes - uuid_byte_t and array of bytes PX4_CPU_UUID_BYTE_LENGTH in length.
 *
 * Returned Value:
 *   The uuid_bytes array is populated with the CPU uuid in the legacy format for
 *   STM32.
 *
 ************************************************************************************/

__EXPORT void board_get_uuid(uuid_byte_t uuid_bytes); // DEPRICATED use board_get_px4_guid

/************************************************************************************
 * Name: board_get_uuid32 DEPRICATED use board_get_px4_guid
 *
 * Description:
 *   All boards either provide a way to read a uuid of PX4_CPU_UUID_WORD32_LENGTH
 *   from PX4_CPU_UUID_ADDRESS in the Soc's address space OR define
 *   BOARD_OVERRIDE_UUID as an array of bytes that is PX4_CPU_UUID_BYTE_LENGTH
 *   On Legacy (stm32) targets the uuid_words format is the result of coping
 *   returning the 32bit words from low memory to high memory. On new targets the result
 *   will be an array of words with the MSW at index 0 and the LSW: at index
 *   PX4_CPU_UUID_WORD32_LENGTH-1.
 *
 * Input Parameters:
 *   uuid_words - a uuid_uint32_t and array of 32 bit words PX4_CPU_UUID_WORD32_
 *   LENGTH in length.
 *
 * Returned Value:
 *   The uuid_words array is populated with the CPU uuid.
 *
 ************************************************************************************/
__EXPORT void board_get_uuid32(uuid_uint32_t uuid_words); // DEPRICATED use board_get_px4_guid

/************************************************************************************
 * Name: board_get_uuid32_formated DEPRICATED use board_get_px4_guid_formated
 *
 * Description:
 *   All boards either provide a way to retrieve a uuid and format it
 *   or define BOARD_OVERRIDE_UUID
 *   This function is used to populate a buffer with the UUID to be a printed
 *   with the optional separator
 *
 * Input Parameters:
 *   format_buffer - A pointer to a bufferer of at least PX4_CPU_UUID_WORD32_FORMAT_SIZE
 *                   that will contain a 0 terminated string formated as described
 *                   the format string and optional separator.
 *   size          - The size of the buffer (should be atleaset PX4_CPU_UUID_WORD32_FORMAT_SIZE)
 *   format        - The fort mat specifier for the hex digit see CPU_UUID_FORMAT
 *   separator     - Optional pointer to a 0 terminated string or NULL:
 *                   With separator = ":"
 *                               31-00:63-32:95-64
 *                               32383336:412038:33355110
 *                   With separator = " "
 *                               31-00:63-32:95-64
 *                               32383336 412038 33355110
 *                   With separator = NULL
 *                               31-00:63-32:95-64
 *                               3238333641203833355110
 *
 * Returned Value:
 *   The format buffer is populated with a 0 terminated string formated as described.
 *   Zero (OK) is returned on success;
 *
 ************************************************************************************/
__EXPORT int board_get_uuid32_formated(char *format_buffer, int size,
				       const char *format,
				       const char *seperator); // DEPRICATED use board_get_px4_guid_formated
#endif // !defined(BOARD_OVERRIDE_UUID)

#if !defined(BOARD_OVERRIDE_MFGUID)
/************************************************************************************
 * Name: board_get_mfguid
 *
 * Description:
 *   All boards either provide a way to retrieve a manufactures Unique ID or
 *   define BOARD_OVERRIDE_MFGUID.
 *    The MFGUID is returned as an array of bytes in
 *    MSD @ index 0 - LSD @ index PX4_CPU_MFGUID_BYTE_LENGTH-1
 *
 * Input Parameters:
 *   mfgid - mfguid_t and array of bytes PX4_CPU_MFGUID_BYTE_LENGTH in length.
 *
 * Returned Value:
 *   The mfguid_t array is populated with the CPU uuid with the MSD @ index 0
 *   and the LSD @ index PX4_CPU_MFGUID_BYTE_LENGTH-1.
 *
 ************************************************************************************/

int board_get_mfguid(mfguid_t mfgid);

/************************************************************************************
 * Name: board_get_mfguid_formated DEPRICATED use board_get_px4_guid_formated
 *
 * Description:
 *   All boards either provide a way to retrieve a formatted string of the
 *   manufactures unique ID or define BOARD_OVERRIDE_MFGUID
 *
 * Input Parameters:
 *   format_buffer - A pointer to a bufferer of at least PX4_CPU_MFGUID_FORMAT_SIZE
 *                   that will contain a 0 terminated string formated as 0 prefixed
 *                   lowercase hex. 2 charaters per digit of the mfguid_t.
 *
 * Returned Value:
 *   format_buffer is populated with a 0 terminated string of hex digits. The
 *   return value is the number of printable in the string.
 *   Usually PX4_CPU_MFGUID_FORMAT_SIZE-1
 *
 ************************************************************************************/

int board_get_mfguid_formated(char *format_buffer, int size); // DEPRICATED use board_get_px4_guid_formated
#endif // !defined(BOARD_OVERRIDE_MFGUID)

#if !defined(BOARD_OVERRIDE_PX4_GUID)
/************************************************************************************
 * Name: board_get_px4_guid
 *
 * Description:
 *   All boards either provide a way to retrieve a PX4 Globally unique ID or
 *   define BOARD_OVERRIDE_PX4_GUID.
 *
 *   The form of the GUID is as follows:
 *  offset:0         1         2         -           17
 *    <ARCH MSD><ARCH LSD><MSD CPU UUID>...<LSD CPU UUID>
 *
 *  Where <ARCH MSD><ARCH LSD> are a monotonic ordinal number assigned by
 *  PX4 to a chip architecture (PX4_SOC_ARCH_ID). The 2 bytes are used to
 *  create a globally unique ID when prepended to a padded CPU ID.
 *
 *  In the case where the CPU's UUID is shorter than 16 bytes it will be
 *  padded with 0's starting at offset [2] until
 *  PX4_CPU_MFGUID_BYTE_LENGTH-PX4_CPU_UUID_BYTE_LENGTH -1
 *  I.E. For the STM32
 *  offset:0         1     2  3  4  5  6             -            17
 *    <ARCH MSD><ARCH LSD>[0][0][0][0]<MSD CPU UUID>...<LSD CPU UUID>
 *
 *  I.E. For as CPU with a 16 byte UUID
 *  offset:0         1         2         -           17
 *    <ARCH MSD><ARCH LSD><MSD CPU UUID>...<LSD CPU UUID>
 *
 * Input Parameters:
 *   guid - a px4_guid_t which is byte array of PX4_GUID_BYTE_LENGTH length.
 *
 * Returned Value:
 *   guid is populated as  <ARCH MSD><ARCH LSD><MSD CPU UUID>...<LSD CPU UUID>
 *   the return value is PX4_GUID_BYTE_LENGTH
 *
 ************************************************************************************/

int board_get_px4_guid(px4_guid_t guid);

/************************************************************************************
 * Name: board_get_mfguid_formated
 *
 * Description:
 *   All boards either provide a way to retrieve a formatted string of the
 *   manufactures Unique ID or define BOARD_OVERRIDE_PX4_GUID
 *
 * Input Parameters:
 * format_buffer - A buffer to receive the 0 terminated formated px4
 *                 guid string.
 * size          - Size of the buffer provided. Normally this would
 *                 be PX4_GUID_FORMAT_SIZE.
 *                 If the size is less than PX4_GUID_FORMAT_SIZE the string
 *                 returned will be truncated from the MSD end and even
 *                 in length.
 *
 * Returned Value:
 *   The number of printable characters. This value will be even and one less the
 *   the size passed in.
 *
 ************************************************************************************/

int board_get_px4_guid_formated(char *format_buffer, int size);
#endif // !defined(BOARD_OVERRIDE_PX4_GUID)

/************************************************************************************
 * Name: board_mcu_version
 *
 * Description:
 *   All boards either provide a way to retrieve the cpu revision
 *   Or define BOARD_OVERRIDE_CPU_VERSION
 *
 * Input Parameters:
 * rev    - The silicon revision character
 * revstr - The full chip name string
 * errata  -The eratta if any.
 *
 * Returned Value:
 *           The silicon revision / version number as integer
 *           or -1 on error and rev, revstr and errata will
 *           not be set
 */
#if defined(BOARD_OVERRIDE_CPU_VERSION)
#define board_mcu_version(rev, revstr, errata) BOARD_OVERRIDE_CPU_VERSION
#else
__EXPORT int board_mcu_version(char *rev, const char **revstr, const char **errata);
#endif // !defined(BOARD_OVERRIDE_CPU_VERSION)

#if defined(BOARD_HAS_POWER_CONTROL)
/************************************************************************************
 * Name: board_register_power_state_notification_cb
 *
 * Description:
 *   boards may provide a function to register a power button state notification
 *   call back.
 *
 *   N.B. this call back may be called off an interrupt. Do not attempt to block
 *   or run any long threads.
 *
 * Input Parameters:
 *   cb     - A pointer to a power button state notification function.
 *
 * Returned Value:
 *   Zero (OK) is returned on success;
 */

int board_register_power_state_notification_cb(power_button_state_notification_t cb);

/************************************************************************************
 * Name: board_shutdown
 *
 * Description:
 *   boards may provide a function to power off the board.
 *
 * Input Parameters:
 *   None.
 * Returned Value:
 *    - If supported the function will not return.
 *      OK, or -EINVAL if unsupported.
 */
int board_shutdown(void);

#else
static inline int board_register_power_state_notification_cb(power_button_state_notification_t cb) { return 0; }
static inline int board_shutdown(void) { return -EINVAL; }
#endif

/************************************************************************************
 * Name: px4_i2c_bus_external
 *
 ************************************************************************************/

#if defined(BOARD_HAS_SIMPLE_HW_VERSIONING)

__EXPORT bool px4_i2c_bus_external(int bus);

#else

#ifdef PX4_I2C_BUS_ONBOARD
#define px4_i2c_bus_external(bus) (bus != PX4_I2C_BUS_ONBOARD)
#else
#define px4_i2c_bus_external(bus) true
#endif /* PX4_I2C_BUS_ONBOARD */

#endif /* BOARD_HAS_SIMPLE_HW_VERSIONING */


/************************************************************************************
 * Name: px4_spi_bus_external
 *
 ************************************************************************************/

#if defined(BOARD_HAS_SIMPLE_HW_VERSIONING)

__EXPORT bool px4_spi_bus_external(int bus);

#else

#ifdef PX4_SPI_BUS_EXT
#define px4_spi_bus_external(bus) (bus == PX4_SPI_BUS_EXT)
#else
#define px4_spi_bus_external(bus) false
#endif /* PX4_SPI_BUS_EXT */

#endif /* BOARD_HAS_SIMPLE_HW_VERSIONING */


#include "board_internal_common.h"
