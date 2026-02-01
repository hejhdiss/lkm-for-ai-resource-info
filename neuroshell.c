
/* NeuroShell LKM 
*  Licensed under GPL v3
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/mm.h>
#include <linux/cpu.h>
#include <linux/numa.h>
#include <linux/pci.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HejHdiss");
MODULE_DESCRIPTION("NeuroShell kernel interface (CPU/GPU/Accelerator introspection)");
MODULE_VERSION("0.2");

static struct kobject *neuroshell_kobj;

/* --------------------------------------------------
 * Helpers: PCI scanning
 * -------------------------------------------------- */
static void scan_pci_devices(
    int *gpu_count,
    int *nvidia,
    int *amd,
    int *intel,
    int *accelerators)
{
    struct pci_dev *pdev = NULL;

    *gpu_count = 0;
    *nvidia = 0;
    *amd = 0;
    *intel = 0;
    *accelerators = 0;

    for_each_pci_dev(pdev) {
        u8 class = (pdev->class >> 16) & 0xff;
        u16 vendor = pdev->vendor;

        /* GPU: VGA or 3D controller */
        if (class == 0x03) {
            (*gpu_count)++;

            switch (vendor) {
                case 0x10de: (*nvidia)++; break; /* NVIDIA */
                case 0x1002: (*amd)++; break;    /* AMD */
                case 0x8086: (*intel)++; break;  /* Intel */
            }
        }

        /* Processing accelerator (NPUs, TPUs, etc.) */
        if (class == 0x12) {
            (*accelerators)++;
        }
    }
}

/* --------------------------------------------------
 * Sysfs show functions
 * -------------------------------------------------- */

static ssize_t cpu_count_show(struct kobject *kobj,
                              struct kobj_attribute *attr,
                              char *buf)
{
    return sprintf(buf, "%u\n", num_online_cpus());
}

static ssize_t mem_total_show(struct kobject *kobj,
                              struct kobj_attribute *attr,
                              char *buf)
{
    return sprintf(buf, "%lu\n", totalram_pages() << PAGE_SHIFT);
}

static ssize_t numa_nodes_show(struct kobject *kobj,
                               struct kobj_attribute *attr,
                               char *buf)
{
#ifdef CONFIG_NUMA
    return sprintf(buf, "%d\n", num_online_nodes());
#else
    return sprintf(buf, "1\n");
#endif
}

static ssize_t gpu_info_show(struct kobject *kobj,
                             struct kobj_attribute *attr,
                             char *buf)
{
    int gpus, nvidia, amd, intel, accel;

    scan_pci_devices(&gpus, &nvidia, &amd, &intel, &accel);

    return sprintf(buf,
        "total=%d\nnvidia=%d\namd=%d\nintel=%d\n",
        gpus, nvidia, amd, intel);
}

static ssize_t accelerator_count_show(struct kobject *kobj,
                                       struct kobj_attribute *attr,
                                       char *buf)
{
    int g, n, a, i, accel;
    scan_pci_devices(&g, &n, &a, &i, &accel);

    return sprintf(buf, "%d\n", accel);
}

/* --------------------------------------------------
 * Sysfs attributes
 * -------------------------------------------------- */
static struct kobj_attribute cpu_attr =
    __ATTR(cpu_count, 0444, cpu_count_show, NULL);

static struct kobj_attribute mem_attr =
    __ATTR(mem_total_bytes, 0444, mem_total_show, NULL);

static struct kobj_attribute numa_attr =
    __ATTR(numa_nodes, 0444, numa_nodes_show, NULL);

static struct kobj_attribute gpu_attr =
    __ATTR(gpu_info, 0444, gpu_info_show, NULL);

static struct kobj_attribute accel_attr =
    __ATTR(accelerator_count, 0444, accelerator_count_show, NULL);

static struct attribute *neuroshell_attrs[] = {
    &cpu_attr.attr,
    &mem_attr.attr,
    &numa_attr.attr,
    &gpu_attr.attr,
    &accel_attr.attr,
    NULL,
};

static struct attribute_group neuroshell_group = {
    .attrs = neuroshell_attrs,
};

/* --------------------------------------------------
 * Init / Exit
 * -------------------------------------------------- */
static int __init neuroshell_init(void)
{
    int ret;

    pr_info("neuroshell: loading\n");

    neuroshell_kobj = kobject_create_and_add("neuroshell", kernel_kobj);
    if (!neuroshell_kobj)
        return -ENOMEM;

    ret = sysfs_create_group(neuroshell_kobj, &neuroshell_group);
    if (ret) {
        kobject_put(neuroshell_kobj);
        return ret;
    }

    pr_info("neuroshell: loaded successfully\n");
    return 0;
}

static void __exit neuroshell_exit(void)
{
    sysfs_remove_group(neuroshell_kobj, &neuroshell_group);
    kobject_put(neuroshell_kobj);
    pr_info("neuroshell: unloaded\n");
}

module_init(neuroshell_init);
module_exit(neuroshell_exit);
