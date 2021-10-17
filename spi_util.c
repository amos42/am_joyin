/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <asm/io.h>

#include "spi_util.h"
#include "gpio_util.h"



/*! Return the minimum of 2 numbers */
#ifndef MIN
#define MIN(a, b) (a < b ? a : b)
#endif

/*! Speed of the core clock core_clk */
#define BCM2835_CORE_CLK_HZ		250000000	/*!< 250 MHz */

/*! On RPi2 with BCM2836, and all recent OSs, the base of the peripherals is read from a /proc file */
#define BMC2835_RPI2_DT_FILENAME "/proc/device-tree/soc/ranges"
/*! Offset into BMC2835_RPI2_DT_FILENAME for the peripherals base address */
#define BMC2835_RPI2_DT_PERI_BASE_ADDRESS_OFFSET 4
/*! Offset into BMC2835_RPI2_DT_FILENAME for the peripherals size address */
#define BMC2835_RPI2_DT_PERI_SIZE_OFFSET 8

/*! Physical addresses for various peripheral register sets
  Base Physical Address of the BCM 2835 peripheral registers
  Note this is different for the RPi2 BCM2836, where this is derived from /proc/device-tree/soc/ranges
  If /proc/device-tree/soc/ranges exists on a RPi 1 OS, it would be expected to contain the
  following numbers:
*/
/*! Peripherals block base address on RPi 1 */
#define BCM2835_PERI_BASE               0x20000000
/*! Size of the peripherals block on RPi 1 */
#define BCM2835_PERI_SIZE               0x01000000

/*! Offsets for the bases of various peripherals within the peripherals block
  /   Base Address of the System Timer registers
*/
#define BCM2835_ST_BASE					0x3000
/*! Base Address of the Pads registers */
#define BCM2835_GPIO_PADS               0x100000
/*! Base Address of the Clock/timer registers */
#define BCM2835_CLOCK_BASE              0x101000
/*! Base Address of the GPIO registers */
#define BCM2835_GPIO_BASE               0x200000
/*! Base Address of the SPI0 registers */
#define BCM2835_SPI0_BASE               0x204000
/*! Base Address of the BSC0 registers */
#define BCM2835_BSC0_BASE 				0x205000
/*! Base Address of the PWM registers */
#define BCM2835_GPIO_PWM                0x20C000
/*! Base Address of the AUX registers */
#define BCM2835_AUX_BASE				0x215000
/*! Base Address of the AUX_SPI1 registers */
#define BCM2835_SPI1_BASE				0x215080
/*! Base Address of the AUX_SPI2 registers */
#define BCM2835_SPI2_BASE				0x2150C0
/*! Base Address of the BSC1 registers */
#define BCM2835_BSC1_BASE				0x804000


/*! Physical address and size of the peripherals block
  May be overridden on RPi2
*/
extern uint32_t *bcm2835_peripherals_base;
/*! Size of the peripherals block to be mapped */
extern uint32_t bcm2835_peripherals_size;

/*! Virtual memory address of the mapped peripherals block */
extern uint32_t *bcm2835_peripherals;


/*! Base of the SPI0 registers.
  Available after bcm2835_init has been called (as root)
*/
extern volatile uint32_t *bcm2835_spi0;



volatile uint32_t *bcm2835_spi0;

/*! \brief bcm2835RegisterBase
  Register bases for bcm2835_regbase()
*/
typedef enum
{
    BCM2835_REGBASE_ST   = 1, /*!< Base of the ST (System Timer) registers. */
    BCM2835_REGBASE_GPIO = 2, /*!< Base of the GPIO registers. */
    BCM2835_REGBASE_PWM  = 3, /*!< Base of the PWM registers. */
    BCM2835_REGBASE_CLK  = 4, /*!< Base of the CLK registers. */
    BCM2835_REGBASE_PADS = 5, /*!< Base of the PADS registers. */
    BCM2835_REGBASE_SPI0 = 6, /*!< Base of the SPI0 registers. */
    BCM2835_REGBASE_BSC0 = 7, /*!< Base of the BSC0 registers. */
    BCM2835_REGBASE_BSC1 = 8,  /*!< Base of the BSC1 registers. */
	BCM2835_REGBASE_AUX  = 9,  /*!< Base of the AUX registers. */
	BCM2835_REGBASE_SPI1 = 10  /*!< Base of the SPI1 registers. */
} bcm2835RegisterBase;

/*! Size of memory page on RPi */
#define BCM2835_PAGE_SIZE               (4*1024)
/*! Size of memory block on RPi */
#define BCM2835_BLOCK_SIZE              (4*1024)


/* Defines for GPIO
   The BCM2835 has 54 GPIO pins.
   BCM2835 data sheet, Page 90 onwards.
*/
/*! GPIO register offsets from BCM2835_GPIO_BASE. 
  Offsets into the GPIO Peripheral block in bytes per 6.1 Register View 
*/
#define BCM2835_GPFSEL0                      0x0000 /*!< GPIO Function Select 0 */
#define BCM2835_GPFSEL1                      0x0004 /*!< GPIO Function Select 1 */
#define BCM2835_GPFSEL2                      0x0008 /*!< GPIO Function Select 2 */
#define BCM2835_GPFSEL3                      0x000c /*!< GPIO Function Select 3 */
#define BCM2835_GPFSEL4                      0x0010 /*!< GPIO Function Select 4 */
#define BCM2835_GPFSEL5                      0x0014 /*!< GPIO Function Select 5 */
#define BCM2835_GPSET0                       0x001c /*!< GPIO Pin Output Set 0 */
#define BCM2835_GPSET1                       0x0020 /*!< GPIO Pin Output Set 1 */
#define BCM2835_GPCLR0                       0x0028 /*!< GPIO Pin Output Clear 0 */
#define BCM2835_GPCLR1                       0x002c /*!< GPIO Pin Output Clear 1 */
#define BCM2835_GPLEV0                       0x0034 /*!< GPIO Pin Level 0 */
#define BCM2835_GPLEV1                       0x0038 /*!< GPIO Pin Level 1 */
#define BCM2835_GPEDS0                       0x0040 /*!< GPIO Pin Event Detect Status 0 */
#define BCM2835_GPEDS1                       0x0044 /*!< GPIO Pin Event Detect Status 1 */
#define BCM2835_GPREN0                       0x004c /*!< GPIO Pin Rising Edge Detect Enable 0 */
#define BCM2835_GPREN1                       0x0050 /*!< GPIO Pin Rising Edge Detect Enable 1 */
#define BCM2835_GPFEN0                       0x0058 /*!< GPIO Pin Falling Edge Detect Enable 0 */
#define BCM2835_GPFEN1                       0x005c /*!< GPIO Pin Falling Edge Detect Enable 1 */
#define BCM2835_GPHEN0                       0x0064 /*!< GPIO Pin High Detect Enable 0 */
#define BCM2835_GPHEN1                       0x0068 /*!< GPIO Pin High Detect Enable 1 */
#define BCM2835_GPLEN0                       0x0070 /*!< GPIO Pin Low Detect Enable 0 */
#define BCM2835_GPLEN1                       0x0074 /*!< GPIO Pin Low Detect Enable 1 */
#define BCM2835_GPAREN0                      0x007c /*!< GPIO Pin Async. Rising Edge Detect 0 */
#define BCM2835_GPAREN1                      0x0080 /*!< GPIO Pin Async. Rising Edge Detect 1 */
#define BCM2835_GPAFEN0                      0x0088 /*!< GPIO Pin Async. Falling Edge Detect 0 */
#define BCM2835_GPAFEN1                      0x008c /*!< GPIO Pin Async. Falling Edge Detect 1 */
#define BCM2835_GPPUD                        0x0094 /*!< GPIO Pin Pull-up/down Enable */
#define BCM2835_GPPUDCLK0                    0x0098 /*!< GPIO Pin Pull-up/down Enable Clock 0 */
#define BCM2835_GPPUDCLK1                    0x009c /*!< GPIO Pin Pull-up/down Enable Clock 1 */

/*!   \brief bcm2835PortFunction
  Port function select modes for bcm2835_gpio_fsel()
*/
typedef enum
{
    BCM2835_GPIO_FSEL_INPT  = 0x00,   /*!< Input 0b000 */
    BCM2835_GPIO_FSEL_OUTP  = 0x01,   /*!< Output 0b001 */
    BCM2835_GPIO_FSEL_ALT0  = 0x04,   /*!< Alternate function 0 0b100 */
    BCM2835_GPIO_FSEL_ALT1  = 0x05,   /*!< Alternate function 1 0b101 */
    BCM2835_GPIO_FSEL_ALT2  = 0x06,   /*!< Alternate function 2 0b110, */
    BCM2835_GPIO_FSEL_ALT3  = 0x07,   /*!< Alternate function 3 0b111 */
    BCM2835_GPIO_FSEL_ALT4  = 0x03,   /*!< Alternate function 4 0b011 */
    BCM2835_GPIO_FSEL_ALT5  = 0x02,   /*!< Alternate function 5 0b010 */
    BCM2835_GPIO_FSEL_MASK  = 0x07    /*!< Function select bits mask 0b111 */
} bcm2835FunctionSelect;

/*! \brief bcm2835PUDControl
  Pullup/Pulldown defines for bcm2835_gpio_pud()
*/
typedef enum
{
    BCM2835_GPIO_PUD_OFF     = 0x00,   /*!< Off ? disable pull-up/down 0b00 */
    BCM2835_GPIO_PUD_DOWN    = 0x01,   /*!< Enable Pull Down control 0b01 */
    BCM2835_GPIO_PUD_UP      = 0x02    /*!< Enable Pull Up control 0b10  */
} bcm2835PUDControl;

/*! Pad control register offsets from BCM2835_GPIO_PADS */
#define BCM2835_PADS_GPIO_0_27               0x002c /*!< Pad control register for pads 0 to 27 */
#define BCM2835_PADS_GPIO_28_45              0x0030 /*!< Pad control register for pads 28 to 45 */
#define BCM2835_PADS_GPIO_46_53              0x0034 /*!< Pad control register for pads 46 to 53 */

/*! Pad Control masks */
#define BCM2835_PAD_PASSWRD                  (0x5A << 24)  /*!< Password to enable setting pad mask */
#define BCM2835_PAD_SLEW_RATE_UNLIMITED      0x10 /*!< Slew rate unlimited */
#define BCM2835_PAD_HYSTERESIS_ENABLED       0x08 /*!< Hysteresis enabled */
#define BCM2835_PAD_DRIVE_2mA                0x00 /*!< 2mA drive current */
#define BCM2835_PAD_DRIVE_4mA                0x01 /*!< 4mA drive current */
#define BCM2835_PAD_DRIVE_6mA                0x02 /*!< 6mA drive current */
#define BCM2835_PAD_DRIVE_8mA                0x03 /*!< 8mA drive current */
#define BCM2835_PAD_DRIVE_10mA               0x04 /*!< 10mA drive current */
#define BCM2835_PAD_DRIVE_12mA               0x05 /*!< 12mA drive current */
#define BCM2835_PAD_DRIVE_14mA               0x06 /*!< 14mA drive current */
#define BCM2835_PAD_DRIVE_16mA               0x07 /*!< 16mA drive current */

/*! \brief bcm2835PadGroup
  Pad group specification for bcm2835_gpio_pad()
*/
typedef enum
{
    BCM2835_PAD_GROUP_GPIO_0_27         = 0, /*!< Pad group for GPIO pads 0 to 27 */
    BCM2835_PAD_GROUP_GPIO_28_45        = 1, /*!< Pad group for GPIO pads 28 to 45 */
    BCM2835_PAD_GROUP_GPIO_46_53        = 2  /*!< Pad group for GPIO pads 46 to 53 */
} bcm2835PadGroup;

/*! \brief GPIO Pin Numbers
  
  Here we define Raspberry Pin GPIO pins on P1 in terms of the underlying BCM GPIO pin numbers.
  These can be passed as a pin number to any function requiring a pin.
  Not all pins on the RPi 26 bin IDE plug are connected to GPIO pins
  and some can adopt an alternate function.
  RPi version 2 has some slightly different pinouts, and these are values RPI_V2_*.
  RPi B+ has yet differnet pinouts and these are defined in RPI_BPLUS_*.
  At bootup, pins 8 and 10 are set to UART0_TXD, UART0_RXD (ie the alt0 function) respectively
  When SPI0 is in use (ie after bcm2835_spi_begin()), SPI0 pins are dedicated to SPI
  and cant be controlled independently.
  If you are using the RPi Compute Module, just use the GPIO number: there is no need to use one of these
  symbolic names
*/
typedef enum
{
    RPI_GPIO_P1_03        =  0,  /*!< Version 1, Pin P1-03 */
    RPI_GPIO_P1_05        =  1,  /*!< Version 1, Pin P1-05 */
    RPI_GPIO_P1_07        =  4,  /*!< Version 1, Pin P1-07 */
    RPI_GPIO_P1_08        = 14,  /*!< Version 1, Pin P1-08, defaults to alt function 0 UART0_TXD */
    RPI_GPIO_P1_10        = 15,  /*!< Version 1, Pin P1-10, defaults to alt function 0 UART0_RXD */
    RPI_GPIO_P1_11        = 17,  /*!< Version 1, Pin P1-11 */
    RPI_GPIO_P1_12        = 18,  /*!< Version 1, Pin P1-12, can be PWM channel 0 in ALT FUN 5 */
    RPI_GPIO_P1_13        = 21,  /*!< Version 1, Pin P1-13 */
    RPI_GPIO_P1_15        = 22,  /*!< Version 1, Pin P1-15 */
    RPI_GPIO_P1_16        = 23,  /*!< Version 1, Pin P1-16 */
    RPI_GPIO_P1_18        = 24,  /*!< Version 1, Pin P1-18 */
    RPI_GPIO_P1_19        = 10,  /*!< Version 1, Pin P1-19, MOSI when SPI0 in use */
    RPI_GPIO_P1_21        =  9,  /*!< Version 1, Pin P1-21, MISO when SPI0 in use */
    RPI_GPIO_P1_22        = 25,  /*!< Version 1, Pin P1-22 */
    RPI_GPIO_P1_23        = 11,  /*!< Version 1, Pin P1-23, CLK when SPI0 in use */
    RPI_GPIO_P1_24        =  8,  /*!< Version 1, Pin P1-24, CE0 when SPI0 in use */
    RPI_GPIO_P1_26        =  7,  /*!< Version 1, Pin P1-26, CE1 when SPI0 in use */

    /* RPi Version 2 */
    RPI_V2_GPIO_P1_03     =  2,  /*!< Version 2, Pin P1-03 */
    RPI_V2_GPIO_P1_05     =  3,  /*!< Version 2, Pin P1-05 */
    RPI_V2_GPIO_P1_07     =  4,  /*!< Version 2, Pin P1-07 */
    RPI_V2_GPIO_P1_08     = 14,  /*!< Version 2, Pin P1-08, defaults to alt function 0 UART0_TXD */
    RPI_V2_GPIO_P1_10     = 15,  /*!< Version 2, Pin P1-10, defaults to alt function 0 UART0_RXD */
    RPI_V2_GPIO_P1_11     = 17,  /*!< Version 2, Pin P1-11 */
    RPI_V2_GPIO_P1_12     = 18,  /*!< Version 2, Pin P1-12, can be PWM channel 0 in ALT FUN 5 */
    RPI_V2_GPIO_P1_13     = 27,  /*!< Version 2, Pin P1-13 */
    RPI_V2_GPIO_P1_15     = 22,  /*!< Version 2, Pin P1-15 */
    RPI_V2_GPIO_P1_16     = 23,  /*!< Version 2, Pin P1-16 */
    RPI_V2_GPIO_P1_18     = 24,  /*!< Version 2, Pin P1-18 */
    RPI_V2_GPIO_P1_19     = 10,  /*!< Version 2, Pin P1-19, MOSI when SPI0 in use */
    RPI_V2_GPIO_P1_21     =  9,  /*!< Version 2, Pin P1-21, MISO when SPI0 in use */
    RPI_V2_GPIO_P1_22     = 25,  /*!< Version 2, Pin P1-22 */
    RPI_V2_GPIO_P1_23     = 11,  /*!< Version 2, Pin P1-23, CLK when SPI0 in use */
    RPI_V2_GPIO_P1_24     =  8,  /*!< Version 2, Pin P1-24, CE0 when SPI0 in use */
    RPI_V2_GPIO_P1_26     =  7,  /*!< Version 2, Pin P1-26, CE1 when SPI0 in use */
    RPI_V2_GPIO_P1_29     =  5,  /*!< Version 2, Pin P1-29 */
    RPI_V2_GPIO_P1_31     =  6,  /*!< Version 2, Pin P1-31 */
    RPI_V2_GPIO_P1_32     = 12,  /*!< Version 2, Pin P1-32 */
    RPI_V2_GPIO_P1_33     = 13,  /*!< Version 2, Pin P1-33 */
    RPI_V2_GPIO_P1_35     = 19,  /*!< Version 2, Pin P1-35, can be PWM channel 1 in ALT FUN 5  */
    RPI_V2_GPIO_P1_36     = 16,  /*!< Version 2, Pin P1-36 */
    RPI_V2_GPIO_P1_37     = 26,  /*!< Version 2, Pin P1-37 */
    RPI_V2_GPIO_P1_38     = 20,  /*!< Version 2, Pin P1-38 */
    RPI_V2_GPIO_P1_40     = 21,  /*!< Version 2, Pin P1-40 */

    /* RPi Version 2, new plug P5 */
    RPI_V2_GPIO_P5_03     = 28,  /*!< Version 2, Pin P5-03 */
    RPI_V2_GPIO_P5_04     = 29,  /*!< Version 2, Pin P5-04 */
    RPI_V2_GPIO_P5_05     = 30,  /*!< Version 2, Pin P5-05 */
    RPI_V2_GPIO_P5_06     = 31,  /*!< Version 2, Pin P5-06 */

    /* RPi B+ J8 header, also RPi 2 40 pin GPIO header */
    RPI_BPLUS_GPIO_J8_03     =  2,  /*!< B+, Pin J8-03 */
    RPI_BPLUS_GPIO_J8_05     =  3,  /*!< B+, Pin J8-05 */
    RPI_BPLUS_GPIO_J8_07     =  4,  /*!< B+, Pin J8-07 */
    RPI_BPLUS_GPIO_J8_08     = 14,  /*!< B+, Pin J8-08, defaults to alt function 0 UART0_TXD */
    RPI_BPLUS_GPIO_J8_10     = 15,  /*!< B+, Pin J8-10, defaults to alt function 0 UART0_RXD */
    RPI_BPLUS_GPIO_J8_11     = 17,  /*!< B+, Pin J8-11 */
    RPI_BPLUS_GPIO_J8_12     = 18,  /*!< B+, Pin J8-12, can be PWM channel 0 in ALT FUN 5 */
    RPI_BPLUS_GPIO_J8_13     = 27,  /*!< B+, Pin J8-13 */
    RPI_BPLUS_GPIO_J8_15     = 22,  /*!< B+, Pin J8-15 */
    RPI_BPLUS_GPIO_J8_16     = 23,  /*!< B+, Pin J8-16 */
    RPI_BPLUS_GPIO_J8_18     = 24,  /*!< B+, Pin J8-18 */
    RPI_BPLUS_GPIO_J8_19     = 10,  /*!< B+, Pin J8-19, MOSI when SPI0 in use */
    RPI_BPLUS_GPIO_J8_21     =  9,  /*!< B+, Pin J8-21, MISO when SPI0 in use */
    RPI_BPLUS_GPIO_J8_22     = 25,  /*!< B+, Pin J8-22 */
    RPI_BPLUS_GPIO_J8_23     = 11,  /*!< B+, Pin J8-23, CLK when SPI0 in use */
    RPI_BPLUS_GPIO_J8_24     =  8,  /*!< B+, Pin J8-24, CE0 when SPI0 in use */
    RPI_BPLUS_GPIO_J8_26     =  7,  /*!< B+, Pin J8-26, CE1 when SPI0 in use */
    RPI_BPLUS_GPIO_J8_29     =  5,  /*!< B+, Pin J8-29,  */
    RPI_BPLUS_GPIO_J8_31     =  6,  /*!< B+, Pin J8-31,  */
    RPI_BPLUS_GPIO_J8_32     = 12,  /*!< B+, Pin J8-32,  */
    RPI_BPLUS_GPIO_J8_33     = 13,  /*!< B+, Pin J8-33,  */
    RPI_BPLUS_GPIO_J8_35     = 19,  /*!< B+, Pin J8-35, can be PWM channel 1 in ALT FUN 5 */
    RPI_BPLUS_GPIO_J8_36     = 16,  /*!< B+, Pin J8-36,  */
    RPI_BPLUS_GPIO_J8_37     = 26,  /*!< B+, Pin J8-37,  */
    RPI_BPLUS_GPIO_J8_38     = 20,  /*!< B+, Pin J8-38,  */
    RPI_BPLUS_GPIO_J8_40     = 21   /*!< B+, Pin J8-40,  */
} RPiGPIOPin;

/* Defines for SPI
   GPIO register offsets from BCM2835_SPI0_BASE. 
   Offsets into the SPI Peripheral block in bytes per 10.5 SPI Register Map
*/
#define BCM2835_SPI0_CS                      0x0000 /*!< SPI Master Control and Status */
#define BCM2835_SPI0_FIFO                    0x0004 /*!< SPI Master TX and RX FIFOs */
#define BCM2835_SPI0_CLK                     0x0008 /*!< SPI Master Clock Divider */
#define BCM2835_SPI0_DLEN                    0x000c /*!< SPI Master Data Length */
#define BCM2835_SPI0_LTOH                    0x0010 /*!< SPI LOSSI mode TOH */
#define BCM2835_SPI0_DC                      0x0014 /*!< SPI DMA DREQ Controls */

/* Register masks for SPI0_CS */
#define BCM2835_SPI0_CS_LEN_LONG             0x02000000 /*!< Enable Long data word in Lossi mode if DMA_LEN is set */
#define BCM2835_SPI0_CS_DMA_LEN              0x01000000 /*!< Enable DMA mode in Lossi mode */
#define BCM2835_SPI0_CS_CSPOL2               0x00800000 /*!< Chip Select 2 Polarity */
#define BCM2835_SPI0_CS_CSPOL1               0x00400000 /*!< Chip Select 1 Polarity */
#define BCM2835_SPI0_CS_CSPOL0               0x00200000 /*!< Chip Select 0 Polarity */
#define BCM2835_SPI0_CS_RXF                  0x00100000 /*!< RXF - RX FIFO Full */
#define BCM2835_SPI0_CS_RXR                  0x00080000 /*!< RXR RX FIFO needs Reading (full) */
#define BCM2835_SPI0_CS_TXD                  0x00040000 /*!< TXD TX FIFO can accept Data */
#define BCM2835_SPI0_CS_RXD                  0x00020000 /*!< RXD RX FIFO contains Data */
#define BCM2835_SPI0_CS_DONE                 0x00010000 /*!< Done transfer Done */
#define BCM2835_SPI0_CS_TE_EN                0x00008000 /*!< Unused */
#define BCM2835_SPI0_CS_LMONO                0x00004000 /*!< Unused */
#define BCM2835_SPI0_CS_LEN                  0x00002000 /*!< LEN LoSSI enable */
#define BCM2835_SPI0_CS_REN                  0x00001000 /*!< REN Read Enable */
#define BCM2835_SPI0_CS_ADCS                 0x00000800 /*!< ADCS Automatically Deassert Chip Select */
#define BCM2835_SPI0_CS_INTR                 0x00000400 /*!< INTR Interrupt on RXR */
#define BCM2835_SPI0_CS_INTD                 0x00000200 /*!< INTD Interrupt on Done */
#define BCM2835_SPI0_CS_DMAEN                0x00000100 /*!< DMAEN DMA Enable */
#define BCM2835_SPI0_CS_TA                   0x00000080 /*!< Transfer Active */
#define BCM2835_SPI0_CS_CSPOL                0x00000040 /*!< Chip Select Polarity */
#define BCM2835_SPI0_CS_CLEAR                0x00000030 /*!< Clear FIFO Clear RX and TX */
#define BCM2835_SPI0_CS_CLEAR_RX             0x00000020 /*!< Clear FIFO Clear RX  */
#define BCM2835_SPI0_CS_CLEAR_TX             0x00000010 /*!< Clear FIFO Clear TX  */
#define BCM2835_SPI0_CS_CPOL                 0x00000008 /*!< Clock Polarity */
#define BCM2835_SPI0_CS_CPHA                 0x00000004 /*!< Clock Phase */
#define BCM2835_SPI0_CS_CS                   0x00000003 /*!< Chip Select */

/*! \brief bcm2835SPIBitOrder SPI Bit order
  Specifies the SPI data bit ordering for bcm2835_spi_setBitOrder()
*/
typedef enum
{
    BCM2835_SPI_BIT_ORDER_LSBFIRST = 0,  /*!< LSB First */
    BCM2835_SPI_BIT_ORDER_MSBFIRST = 1   /*!< MSB First */
}bcm2835SPIBitOrder;

/*! \brief SPI Data mode
  Specify the SPI data mode to be passed to bcm2835_spi_setDataMode()
*/
typedef enum
{
    BCM2835_SPI_MODE0 = 0,  /*!< CPOL = 0, CPHA = 0 */
    BCM2835_SPI_MODE1 = 1,  /*!< CPOL = 0, CPHA = 1 */
    BCM2835_SPI_MODE2 = 2,  /*!< CPOL = 1, CPHA = 0 */
    BCM2835_SPI_MODE3 = 3   /*!< CPOL = 1, CPHA = 1 */
}bcm2835SPIMode;

/*! \brief bcm2835SPIChipSelect
  Specify the SPI chip select pin(s)
*/
typedef enum
{
    BCM2835_SPI_CS0 = 0,     /*!< Chip Select 0 */
    BCM2835_SPI_CS1 = 1,     /*!< Chip Select 1 */
    BCM2835_SPI_CS2 = 2,     /*!< Chip Select 2 (ie pins CS1 and CS2 are asserted) */
    BCM2835_SPI_CS_NONE = 3  /*!< No CS, control it yourself */
} bcm2835SPIChipSelect;

/*! \brief bcm2835SPIClockDivider
  Specifies the divider used to generate the SPI clock from the system clock.
  Figures below give the divider, clock period and clock frequency.
  Clock divided is based on nominal core clock rate of 250MHz on RPi1 and RPi2, and 400MHz on RPi3.
  It is reported that (contrary to the documentation) any even divider may used.
  The frequencies shown for each divider have been confirmed by measurement on RPi1 and RPi2.
  The system clock frequency on RPi3 is different, so the frequency you get from a given divider will be different.
  See comments in 'SPI Pins' for information about reliable SPI speeds.
  Note: it is possible to change the core clock rate of the RPi 3 back to 250MHz, by putting 
  \code
  core_freq=250
  \endcode
  in the config.txt
*/
typedef enum
{
    BCM2835_SPI_CLOCK_DIVIDER_65536 = 0,       /*!< 65536 = 3.814697260kHz on Rpi2, 6.1035156kHz on RPI3 */
    BCM2835_SPI_CLOCK_DIVIDER_32768 = 32768,   /*!< 32768 = 7.629394531kHz on Rpi2, 12.20703125kHz on RPI3 */
    BCM2835_SPI_CLOCK_DIVIDER_16384 = 16384,   /*!< 16384 = 15.25878906kHz on Rpi2, 24.4140625kHz on RPI3 */
    BCM2835_SPI_CLOCK_DIVIDER_8192  = 8192,    /*!< 8192 = 30.51757813kHz on Rpi2, 48.828125kHz on RPI3 */
    BCM2835_SPI_CLOCK_DIVIDER_4096  = 4096,    /*!< 4096 = 61.03515625kHz on Rpi2, 97.65625kHz on RPI3 */
    BCM2835_SPI_CLOCK_DIVIDER_2048  = 2048,    /*!< 2048 = 122.0703125kHz on Rpi2, 195.3125kHz on RPI3 */
    BCM2835_SPI_CLOCK_DIVIDER_1024  = 1024,    /*!< 1024 = 244.140625kHz on Rpi2, 390.625kHz on RPI3 */
    BCM2835_SPI_CLOCK_DIVIDER_512   = 512,     /*!< 512 = 488.28125kHz on Rpi2, 781.25kHz on RPI3 */
    BCM2835_SPI_CLOCK_DIVIDER_256   = 256,     /*!< 256 = 976.5625kHz on Rpi2, 1.5625MHz on RPI3 */
    BCM2835_SPI_CLOCK_DIVIDER_128   = 128,     /*!< 128 = 1.953125MHz on Rpi2, 3.125MHz on RPI3 */
    BCM2835_SPI_CLOCK_DIVIDER_64    = 64,      /*!< 64 = 3.90625MHz on Rpi2, 6.250MHz on RPI3 */
    BCM2835_SPI_CLOCK_DIVIDER_32    = 32,      /*!< 32 = 7.8125MHz on Rpi2, 12.5MHz on RPI3 */
    BCM2835_SPI_CLOCK_DIVIDER_16    = 16,      /*!< 16 = 15.625MHz on Rpi2, 25MHz on RPI3 */
    BCM2835_SPI_CLOCK_DIVIDER_8     = 8,       /*!< 8 = 31.25MHz on Rpi2, 50MHz on RPI3 */
    BCM2835_SPI_CLOCK_DIVIDER_4     = 4,       /*!< 4 = 62.5MHz on Rpi2, 100MHz on RPI3. Dont expect this speed to work reliably. */
    BCM2835_SPI_CLOCK_DIVIDER_2     = 2,       /*!< 2 = 125MHz on Rpi2, 200MHz on RPI3, fastest you can get. Dont expect this speed to work reliably.*/
    BCM2835_SPI_CLOCK_DIVIDER_1     = 1        /*!< 1 = 3.814697260kHz on Rpi2, 6.1035156kHz on RPI3, same as 0/65536 */
} bcm2835SPIClockDivider;


/* Defines for ST
   GPIO register offsets from BCM2835_ST_BASE.
   Offsets into the ST Peripheral block in bytes per 12.1 System Timer Registers
   The System Timer peripheral provides four 32-bit timer channels and a single 64-bit free running counter.
   BCM2835_ST_CLO is the System Timer Counter Lower bits register.
   The system timer free-running counter lower register is a read-only register that returns the current value
   of the lower 32-bits of the free running counter.
   BCM2835_ST_CHI is the System Timer Counter Upper bits register.
   The system timer free-running counter upper register is a read-only register that returns the current value
   of the upper 32-bits of the free running counter.
*/
#define BCM2835_ST_CS 			0x0000 /*!< System Timer Control/Status */
#define BCM2835_ST_CLO 			0x0004 /*!< System Timer Counter Lower 32 bits */
#define BCM2835_ST_CHI 			0x0008 /*!< System Timer Counter Upper 32 bits */


void bcm2835_gpio_fsel(uint8_t pin, uint8_t mode)
{
    // /* Function selects are 10 pins per 32 bit word, 3 bits per pin */
    // volatile uint32_t* paddr = bcm2835_gpio + BCM2835_GPFSEL0/4 + (pin/10);
    // uint8_t   shift = (pin % 10) * 3;
    // uint32_t  mask = BCM2835_GPIO_FSEL_MASK << shift;
    // uint32_t  value = mode << shift;
    // bcm2835_peri_set_bits(paddr, value, mask);

    if (mode == BCM2835_GPIO_FSEL_ALT0) {
        INP_GPIO(pin);
        SET_GPIO_ALT(pin, 0);
    } else if (mode == BCM2835_GPIO_FSEL_INPT) {
        // INP_GPIO(pin);
        // SET_GPIO(pin);
    }
}

// /* Set output pin */
// void bcm2835_gpio_set(uint8_t pin)
// {
//     volatile uint32_t* paddr = bcm2835_gpio + BCM2835_GPSET0/4 + pin/32;
//     uint8_t shift = pin % 32;
//     bcm2835_peri_write(paddr, 1 << shift);
// }

// /* Clear output pin */
// void bcm2835_gpio_clr(uint8_t pin)
// {
//     volatile uint32_t* paddr = bcm2835_gpio + BCM2835_GPCLR0/4 + pin/32;
//     uint8_t shift = pin % 32;
//     bcm2835_peri_write(paddr, 1 << shift);
// }


/* Initialise this library. */
int bcm2835_init(void)
{
    int  ok = 0;

      /* Now compute the base addresses of various peripherals, 
      // which are at fixed offsets within the mapped peripherals block
      // Caution: bcm2835_peripherals is uint32_t*, so divide offsets by 4
      */
      //bcm2835_spi0 = PERI_BASE + BCM2835_SPI0_BASE/4;
    if ((bcm2835_spi0 = ioremap(PERI_BASE + BCM2835_SPI0_BASE, 0x14)) == NULL) {
        pr_err("io remap failed");
        return -EINVAL;
    }


    return ok;
}

/* Close this library and deallocate everything */
int bcm2835_close(void)
{
    if (bcm2835_spi0 != NULL) {
        iounmap(bcm2835_spi0);
        bcm2835_spi0 = NULL;
    }

    return 1; /* Success */
}    



/* Read with memory barriers from peripheral
 *
 */
uint32_t bcm2835_peri_read(volatile uint32_t* paddr)
{
    uint32_t ret;
	__sync_synchronize();
	ret = *paddr;
	__sync_synchronize();
	return ret;
}

/* read from peripheral without the read barrier
 * This can only be used if more reads to THE SAME peripheral
 * will follow.  The sequence must terminate with memory barrier
 * before any read or write to another peripheral can occur.
 * The MB can be explicit, or one of the barrier read/write calls.
 */
uint32_t bcm2835_peri_read_nb(volatile uint32_t* paddr)
{
	return *paddr;
}

/* Write with memory barriers to peripheral
 */

void bcm2835_peri_write(volatile uint32_t* paddr, uint32_t value)
{
	__sync_synchronize();
	*paddr = value;
	__sync_synchronize();
}

/* write to peripheral without the write barrier */
void bcm2835_peri_write_nb(volatile uint32_t* paddr, uint32_t value)
{
	*paddr = value;
}

/* Set/clear only the bits in value covered by the mask
 * This is not atomic - can be interrupted.
 */
void bcm2835_peri_set_bits(volatile uint32_t* paddr, uint32_t value, uint32_t mask)
{
    uint32_t v = bcm2835_peri_read(paddr);
    v = (v & ~mask) | (value & mask);
    bcm2835_peri_write(paddr, v);
}


int bcm2835_spi_begin(void)
{
    volatile uint32_t* paddr;

    //if (bcm2835_spi0 == MAP_FAILED)
    //  return 0; /* bcm2835_init() failed, or not root */
    
    /* Set the SPI0 pins to the Alt 0 function to enable SPI0 access on them */
    bcm2835_gpio_fsel(RPI_GPIO_P1_26, BCM2835_GPIO_FSEL_ALT0); /* CE1 */
    bcm2835_gpio_fsel(RPI_GPIO_P1_24, BCM2835_GPIO_FSEL_ALT0); /* CE0 */
    bcm2835_gpio_fsel(RPI_GPIO_P1_21, BCM2835_GPIO_FSEL_ALT0); /* MISO */
    bcm2835_gpio_fsel(RPI_GPIO_P1_19, BCM2835_GPIO_FSEL_ALT0); /* MOSI */
    bcm2835_gpio_fsel(RPI_GPIO_P1_23, BCM2835_GPIO_FSEL_ALT0); /* CLK */
    
    /* Set the SPI CS register to the some sensible defaults */
    paddr = bcm2835_spi0 + BCM2835_SPI0_CS/4;
    bcm2835_peri_write(paddr, 0); /* All 0s */
    
    /* Clear TX and RX fifos */
    bcm2835_peri_write_nb(paddr, BCM2835_SPI0_CS_CLEAR);

    return 1; // OK
}

void bcm2835_spi_end(void)
{  
    /* Set all the SPI0 pins back to input */
    bcm2835_gpio_fsel(RPI_GPIO_P1_26, BCM2835_GPIO_FSEL_INPT); /* CE1 */
    bcm2835_gpio_fsel(RPI_GPIO_P1_24, BCM2835_GPIO_FSEL_INPT); /* CE0 */
    bcm2835_gpio_fsel(RPI_GPIO_P1_21, BCM2835_GPIO_FSEL_INPT); /* MISO */
    bcm2835_gpio_fsel(RPI_GPIO_P1_19, BCM2835_GPIO_FSEL_INPT); /* MOSI */
    bcm2835_gpio_fsel(RPI_GPIO_P1_23, BCM2835_GPIO_FSEL_INPT); /* CLK */
}

// void bcm2835_spi_setBitOrder(uint8_t __attribute__((unused)) order)
// {
//     /* BCM2835_SPI_BIT_ORDER_MSBFIRST is the only one supported by SPI0 */
// }

/* defaults to 0, which means a divider of 65536.
// The divisor must be a power of 2. Odd numbers
// rounded down. The maximum SPI clock rate is
// of the APB clock
*/
void bcm2835_spi_setClockDivider(uint16_t divider)
{
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CLK/4;
    bcm2835_peri_write(paddr, divider);
}

void bcm2835_spi_setDataMode(uint8_t mode)
{
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/4;
    /* Mask in the CPO and CPHA bits of CS */
    bcm2835_peri_set_bits(paddr, mode << 2, BCM2835_SPI0_CS_CPOL | BCM2835_SPI0_CS_CPHA);
}

/* Writes (and reads) a single byte to SPI */
uint8_t bcm2835_spi_transfer(uint8_t value)
{
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/4;
    volatile uint32_t* fifo = bcm2835_spi0 + BCM2835_SPI0_FIFO/4;
    uint32_t ret;

    /* This is Polled transfer as per section 10.6.1
    // BUG ALERT: what happens if we get interupted in this section, and someone else
    // accesses a different peripheral? 
    // Clear TX and RX fifos
    */
    bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);

    /* Set TA = 1 */
    bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);

    /* Maybe wait for TXD */
    while (!(bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_TXD)) {}

    /* Write to FIFO, no barrier */
    bcm2835_peri_write_nb(fifo, value);

    /* Wait for DONE to be set */
    while (!(bcm2835_peri_read_nb(paddr) & BCM2835_SPI0_CS_DONE)) {}

    /* Read any byte that was sent back by the slave while we sere sending to it */
    ret = bcm2835_peri_read_nb(fifo);

    /* Set TA = 0, and also set the barrier */
    bcm2835_peri_set_bits(paddr, 0, BCM2835_SPI0_CS_TA);

    return ret;
}

/* Writes (and reads) an number of bytes to SPI */
void bcm2835_spi_transfernb(char* tbuf, char* rbuf, uint32_t len)
{
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/4;
    volatile uint32_t* fifo = bcm2835_spi0 + BCM2835_SPI0_FIFO/4;
    uint32_t TXCnt=0;
    uint32_t RXCnt=0;

    /* This is Polled transfer as per section 10.6.1
    // BUG ALERT: what happens if we get interupted in this section, and someone else
    // accesses a different peripheral? 
    */

    /* Clear TX and RX fifos */
    bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);

    /* Set TA = 1 */
    bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);

    /* Use the FIFO's to reduce the interbyte times */
    while((TXCnt < len) || (RXCnt < len))
    {
        /* TX fifo not full, so add some more bytes */
        while((bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_TXD) && (TXCnt < len))
        {
           bcm2835_peri_write_nb(fifo, tbuf[TXCnt]);
           TXCnt++;
        }
        /* Rx fifo not empty, so get the next received bytes */
        while((bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_RXD) && (RXCnt < len))
        {
           rbuf[RXCnt] = bcm2835_peri_read_nb(fifo);
           RXCnt++;
        }
    }
    /* Wait for DONE to be set */
    while (!(bcm2835_peri_read_nb(paddr) & BCM2835_SPI0_CS_DONE)) {}

    /* Set TA = 0, and also set the barrier */
    bcm2835_peri_set_bits(paddr, 0, BCM2835_SPI0_CS_TA);
}

/* Writes an number of bytes to SPI */
void bcm2835_spi_writenb(const char* tbuf, uint32_t len)
{
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/4;
    volatile uint32_t* fifo = bcm2835_spi0 + BCM2835_SPI0_FIFO/4;
    uint32_t i;

    /* This is Polled transfer as per section 10.6.1
    // BUG ALERT: what happens if we get interupted in this section, and someone else
    // accesses a different peripheral?
    // Answer: an ISR is required to issue the required memory barriers.
    */

    /* Clear TX and RX fifos */
    bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);

    /* Set TA = 1 */
    bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);

    for (i = 0; i < len; i++)
    {
	/* Maybe wait for TXD */
	while (!(bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_TXD))
	    ;
	
	/* Write to FIFO, no barrier */
	bcm2835_peri_write_nb(fifo, tbuf[i]);
	
	/* Read from FIFO to prevent stalling */
	while (bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_RXD)
	    (void) bcm2835_peri_read_nb(fifo);
    }
    
    /* Wait for DONE to be set */
    while (!(bcm2835_peri_read_nb(paddr) & BCM2835_SPI0_CS_DONE)) {
	while (bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_RXD)
		(void) bcm2835_peri_read_nb(fifo);
    };

    /* Set TA = 0, and also set the barrier */
    bcm2835_peri_set_bits(paddr, 0, BCM2835_SPI0_CS_TA);
}

/* Writes (and reads) an number of bytes to SPI
// Read bytes are copied over onto the transmit buffer
*/
void bcm2835_spi_transfern(char* buf, uint32_t len)
{
    bcm2835_spi_transfernb(buf, buf, len);
}

void bcm2835_spi_chipSelect(uint8_t cs)
{
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/4;
    /* Mask in the CS bits of CS */
    bcm2835_peri_set_bits(paddr, cs, BCM2835_SPI0_CS_CS);
}

void bcm2835_spi_setChipSelectPolarity(uint8_t cs, uint8_t active)
{
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/4;
    uint8_t shift = 21 + cs;
    /* Mask in the appropriate CSPOLn bit */
    bcm2835_peri_set_bits(paddr, active << shift, 1 << shift);
}

void bcm2835_spi_write(uint16_t data) {
#if 0
	char buf[2];

	buf[0] = data >> 8;
	buf[1] = data & 0xFF;

	bcm2835_spi_transfern(buf, 2);
#else
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/4;
    volatile uint32_t* fifo = bcm2835_spi0 + BCM2835_SPI0_FIFO/4;

    /* Clear TX and RX fifos */
    bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);

    /* Set TA = 1 */
    bcm2835_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);

	/* Maybe wait for TXD */
	while (!(bcm2835_peri_read(paddr) & BCM2835_SPI0_CS_TXD))
	    ;

	/* Write to FIFO */
	bcm2835_peri_write_nb(fifo,  (uint32_t) data >> 8);
	bcm2835_peri_write_nb(fifo,  data & 0xFF);


    /* Wait for DONE to be set */
    while (!(bcm2835_peri_read_nb(paddr) & BCM2835_SPI0_CS_DONE))
	;

    /* Set TA = 0, and also set the barrier */
    bcm2835_peri_set_bits(paddr, 0, BCM2835_SPI0_CS_TA);
#endif
}
