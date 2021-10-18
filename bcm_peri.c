/**
 * am_bcm_peri_base_probe - Find the peripherals address base for
 * the running Raspberry Pi model. It needs a kernel with runtime Device-Tree
 * overlay support.
 *
 * Based on the userland 'bcm_host' library code from
 * https://github.com/raspberrypi/userland/blob/2549c149d8aa7f18ff201a1c0429cb26f9e2535a/host_applications/linux/libs/bcm_host/bcm_host.c#L150
 *
 * Reference: https://www.raspberrypi.org/documentation/hardware/raspberrypi/peripheral_addresses.md
 *
 * If any error occurs reading the device tree nodes/properties, then return 0.
 */

#include <linux/kernel.h>
#include <linux/of_platform.h>


#define BCM2708_PERI_BASE (0x3F000000) // RPI 2
#define BCM2709_PERI_BASE (0x20000000) // abobe RPI 2B v1.2
#define BCM2711_PERI_BASE (0x7e215000) // abobe RPI 4


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


/*! Physical address and size of the peripherals block
  May be overridden on RPi2
*/
// extern uint32_t *bcm2835_peripherals_base;
// /*! Size of the peripherals block to be mapped */
// extern uint32_t bcm2835_peripherals_size;

// /*! Virtual memory address of the mapped peripherals block */
// extern uint32_t *bcm2835_peripherals;


// /*! Base of the SPI0 registers.
//   Available after bcm2835_init has been called (as root)
// */
// extern volatile uint32_t *bcm2835_spi0;


static u32 peri_base = 0x00000000;


u32 bcm_peri_base_probe(void) 
{
    char *path = "/soc";
    struct device_node *dt_node;
    u32 base_address = 1;

    if (peri_base != 0x00000000) {
        return peri_base;
    }

    dt_node = of_find_node_by_path(path);
    if (!dt_node) {
        pr_err("failed to find device-tree node: %s\n", path);
        return 0;
    }

    if (of_property_read_u32_index(dt_node, "ranges", 1, &base_address)) {
        pr_err("failed to read range index 1\n");
        return 0;
    }

    if (base_address == 0) {
        if (of_property_read_u32_index(dt_node, "ranges", 2, &base_address)) {
            pr_err("failed to read range index 2\n");
            return 0;
        }
    }

    peri_base = (base_address == 1) ? BCM2709_PERI_BASE : base_address;

    return peri_base;
}

/* Read with memory barriers from peripheral
 *
 */
uint32_t bcm_peri_read(volatile uint32_t* paddr)
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
uint32_t bcm_peri_read_nb(volatile uint32_t* paddr)
{
	return *paddr;
}

/* Write with memory barriers to peripheral
 */

void bcm_peri_write(volatile uint32_t* paddr, uint32_t value)
{
	__sync_synchronize();
	*paddr = value;
	__sync_synchronize();
}

/* write to peripheral without the write barrier */
void bcm_peri_write_nb(volatile uint32_t* paddr, uint32_t value)
{
	*paddr = value;
}

/* Set/clear only the bits in value covered by the mask
 * This is not atomic - can be interrupted.
 */
void bcm_peri_set_bits(volatile uint32_t* paddr, uint32_t value, uint32_t mask)
{
    uint32_t v = bcm_peri_read(paddr);
    v = (v & ~mask) | (value & mask);
    bcm_peri_write(paddr, v);
}
