/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#include <linux/kernel.h>
#include <linux/delay.h>
#include <asm/io.h>

#include "spi_util.h"
#include "bcm_peri.h"
#include "gpio_util.h"


/*! Return the minimum of 2 numbers */
#ifndef MIN
#define MIN(a, b) (a < b ? a : b)
#endif

/*! Offsets for the bases of various peripherals within the peripherals block
  Base Address of the System Timer registers
*/
#define BCM2835_SPI0_BASE               0x204000
#define BCM2835_SPI1_BASE				0x215080
#define BCM2835_SPI2_BASE				0x2150C0

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
typedef enum {
    BCM2835_SPI_BIT_ORDER_LSBFIRST = 0,  /*!< LSB First */
    BCM2835_SPI_BIT_ORDER_MSBFIRST = 1   /*!< MSB First */
} bcm2835SPIBitOrder;

/*! \brief SPI Data mode
  Specify the SPI data mode to be passed to bcm2835_spi_setDataMode()
*/
typedef enum {
    BCM2835_SPI_MODE0 = 0,  /*!< CPOL = 0, CPHA = 0 */
    BCM2835_SPI_MODE1 = 1,  /*!< CPOL = 0, CPHA = 1 */
    BCM2835_SPI_MODE2 = 2,  /*!< CPOL = 1, CPHA = 0 */
    BCM2835_SPI_MODE3 = 3   /*!< CPOL = 1, CPHA = 1 */
} bcm2835SPIMode;

/*! \brief bcm2835SPIChipSelect
  Specify the SPI chip select pin(s)
*/
typedef enum {
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
typedef enum {
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


volatile uint32_t *bcm2835_spi0;


void bcm2835_gpio_fsel(uint8_t pin, uint8_t mode)
{
    // /* Function selects are 10 pins per 32 bit word, 3 bits per pin */
    // volatile uint32_t* paddr = bcm2835_gpio + BCM2835_GPFSEL0/4 + (pin/10);
    // uint8_t   shift = (pin % 10) * 3;
    // uint32_t  mask = BCM2835_GPIO_FSEL_MASK << shift;
    // uint32_t  value = mode << shift;
    // bcm_peri_set_bits(paddr, value, mask);

    if (mode == BCM2835_GPIO_FSEL_ALT0) {
        INP_GPIO(pin);
        SET_GPIO_ALT(pin, 0);
    } else if (mode == BCM2835_GPIO_FSEL_INPT) {
        //INP_GPIO(pin);
        //SET_GPIO(pin);
    }
}

// /* Set output pin */
// void bcm2835_gpio_set(uint8_t pin)
// {
//     volatile uint32_t* paddr = bcm2835_gpio + BCM2835_GPSET0/4 + pin/32;
//     uint8_t shift = pin % 32;
//     bcm_peri_write(paddr, 1 << shift);
// }

// /* Clear output pin */
// void bcm2835_gpio_clr(uint8_t pin)
// {
//     volatile uint32_t* paddr = bcm2835_gpio + BCM2835_GPCLR0/4 + pin/32;
//     uint8_t shift = pin % 32;
//     bcm_peri_write(paddr, 1 << shift);
// }


/* Initialise this library. */
int bcm2835_spi_init(u32 peri_base_addr, int size)
{
    if(size == 0) size = 0x14;

    if ((bcm2835_spi0 = ioremap(peri_base_addr + BCM2835_SPI0_BASE, size)) == NULL) {
        pr_err("io remap failed");
        return -EINVAL;
    }

    return 0;
}

/* Close this library and deallocate everything */
void bcm2835_spi_close(void)
{
    if (bcm2835_spi0 != NULL) {
        iounmap(bcm2835_spi0);
        bcm2835_spi0 = NULL;
    }
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
    bcm_peri_write(paddr, 0); /* All 0s */
    
    /* Clear TX and RX fifos */
    bcm_peri_write_nb(paddr, BCM2835_SPI0_CS_CLEAR);

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
    bcm_peri_write(paddr, divider);
}

void bcm2835_spi_setDataMode(uint8_t mode)
{
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/4;
    /* Mask in the CPO and CPHA bits of CS */
    bcm_peri_set_bits(paddr, mode << 2, BCM2835_SPI0_CS_CPOL | BCM2835_SPI0_CS_CPHA);
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
    bcm_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);

    /* Set TA = 1 */
    bcm_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);

    /* Maybe wait for TXD */
    while (!(bcm_peri_read(paddr) & BCM2835_SPI0_CS_TXD)) {}

    /* Write to FIFO, no barrier */
    bcm_peri_write_nb(fifo, value);

    /* Wait for DONE to be set */
    while (!(bcm_peri_read_nb(paddr) & BCM2835_SPI0_CS_DONE)) {}

    /* Read any byte that was sent back by the slave while we sere sending to it */
    ret = bcm_peri_read_nb(fifo);

    /* Set TA = 0, and also set the barrier */
    bcm_peri_set_bits(paddr, 0, BCM2835_SPI0_CS_TA);

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
    bcm_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);

    /* Set TA = 1 */
    bcm_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);

    /* Use the FIFO's to reduce the interbyte times */
    while((TXCnt < len) || (RXCnt < len))
    {
        /* TX fifo not full, so add some more bytes */
        while((bcm_peri_read(paddr) & BCM2835_SPI0_CS_TXD) && (TXCnt < len))
        {
           bcm_peri_write_nb(fifo, tbuf[TXCnt]);
           TXCnt++;
        }
        /* Rx fifo not empty, so get the next received bytes */
        while((bcm_peri_read(paddr) & BCM2835_SPI0_CS_RXD) && (RXCnt < len))
        {
           rbuf[RXCnt] = bcm_peri_read_nb(fifo);
           RXCnt++;
        }
    }
    /* Wait for DONE to be set */
    while (!(bcm_peri_read_nb(paddr) & BCM2835_SPI0_CS_DONE)) {}

    /* Set TA = 0, and also set the barrier */
    bcm_peri_set_bits(paddr, 0, BCM2835_SPI0_CS_TA);
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
    bcm_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);

    /* Set TA = 1 */
    bcm_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);

    for (i = 0; i < len; i++)
    {
	/* Maybe wait for TXD */
	while (!(bcm_peri_read(paddr) & BCM2835_SPI0_CS_TXD))
	    ;
	
	/* Write to FIFO, no barrier */
	bcm_peri_write_nb(fifo, tbuf[i]);
	
	/* Read from FIFO to prevent stalling */
	while (bcm_peri_read(paddr) & BCM2835_SPI0_CS_RXD)
	    (void) bcm_peri_read_nb(fifo);
    }
    
    /* Wait for DONE to be set */
    while (!(bcm_peri_read_nb(paddr) & BCM2835_SPI0_CS_DONE)) {
	while (bcm_peri_read(paddr) & BCM2835_SPI0_CS_RXD)
		(void) bcm_peri_read_nb(fifo);
    };

    /* Set TA = 0, and also set the barrier */
    bcm_peri_set_bits(paddr, 0, BCM2835_SPI0_CS_TA);
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
    bcm_peri_set_bits(paddr, cs, BCM2835_SPI0_CS_CS);
}

void bcm2835_spi_setChipSelectPolarity(uint8_t cs, uint8_t active)
{
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/4;
    uint8_t shift = 21 + cs;
    /* Mask in the appropriate CSPOLn bit */
    bcm_peri_set_bits(paddr, active << shift, 1 << shift);
}

void bcm2835_spi_write(uint16_t data) 
{
#if 0
	char buf[2];

	buf[0] = data >> 8;
	buf[1] = data & 0xFF;

	bcm2835_spi_transfern(buf, 2);
#else
    volatile uint32_t* paddr = bcm2835_spi0 + BCM2835_SPI0_CS/4;
    volatile uint32_t* fifo = bcm2835_spi0 + BCM2835_SPI0_FIFO/4;

    /* Clear TX and RX fifos */
    bcm_peri_set_bits(paddr, BCM2835_SPI0_CS_CLEAR, BCM2835_SPI0_CS_CLEAR);

    /* Set TA = 1 */
    bcm_peri_set_bits(paddr, BCM2835_SPI0_CS_TA, BCM2835_SPI0_CS_TA);

	/* Maybe wait for TXD */
	while (!(bcm_peri_read(paddr) & BCM2835_SPI0_CS_TXD))
	    ;

	/* Write to FIFO */
	bcm_peri_write_nb(fifo,  (uint32_t) data >> 8);
	bcm_peri_write_nb(fifo,  data & 0xFF);


    /* Wait for DONE to be set */
    while (!(bcm_peri_read_nb(paddr) & BCM2835_SPI0_CS_DONE))
	;

    /* Set TA = 0, and also set the barrier */
    bcm_peri_set_bits(paddr, 0, BCM2835_SPI0_CS_TA);
#endif
}
