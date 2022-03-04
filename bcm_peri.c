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
#include "bcm2835.h"
#include "log_util.h"


#define BCM2708_PERI_BASE (0x3F000000) // RPI 2
#define BCM2709_PERI_BASE (0x20000000) // abobe RPI 2B v1.2
#define BCM2711_PERI_BASE (0x7e215000) // abobe RPI 4


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
        am_log_err("failed to find device-tree node: %s", path);
        return 0;
    }

    if (of_property_read_u32_index(dt_node, "ranges", 1, &base_address)) {
        am_log_err("failed to read range index 1");
        return 0;
    }

    if (base_address == 0) {
        if (of_property_read_u32_index(dt_node, "ranges", 2, &base_address)) {
            am_log_err("failed to read range index 2");
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
