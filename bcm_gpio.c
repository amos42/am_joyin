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


u32 bcm_peri_base_probe(void) 
{
    char *path = "/soc";
    struct device_node *dt_node;
    u32 base_address = 1;

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

    return base_address == 1 ? 0x02000000 : base_address;
}
