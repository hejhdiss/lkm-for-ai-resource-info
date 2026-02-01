/*
 * NeuroShell LKM  - Advanced Hardware Introspection Kernel Module
 * 
 * Provides detailed system hardware information via sysfs interface
 * at /sys/kernel/neuroshell/
 *
 * Copyright (C) 2025 HejHdiss
 * Licensed under GPL v3
 *
 * Features:
 * - CPU topology and capabilities
 * - Memory configuration
 * - NUMA architecture details
 * - GPU detection (NVIDIA, AMD, Intel)
 * - AI accelerator detection
 * - Cache hierarchy information
 * - Thermal zones
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
#include <linux/cpufreq.h>
#include <linux/topology.h>
#include <linux/mmzone.h>
#include <linux/swap.h>

#ifdef CONFIG_X86
#include <asm/processor.h>
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HejHdiss");
MODULE_DESCRIPTION("NeuroShell: Advanced hardware introspection for AI workloads");
MODULE_VERSION("0.3");

static struct kobject *neuroshell_kobj;

/* --------------------------------------------------
 * PCI Device Scanning
 * -------------------------------------------------- */
 
/**
 * scan_pci_devices - Enumerate and classify PCI devices
 * @gpu_count: Total number of GPU devices found
 * @nvidia: Number of NVIDIA GPUs
 * @amd: Number of AMD GPUs
 * @intel: Number of Intel GPUs
 * @accelerators: Number of processing accelerators
 */
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

        /* GPU: VGA controller (0x03) */
        if (class == 0x03) {
            (*gpu_count)++;

            switch (vendor) {
                case 0x10de: (*nvidia)++; break;  /* NVIDIA */
                case 0x1002: (*amd)++; break;     /* AMD */
                case 0x8086: (*intel)++; break;   /* Intel */
            }
        }

        /* Processing accelerator (0x12) - NPUs, TPUs, etc. */
        if (class == 0x12) {
            (*accelerators)++;
        }
    }
}

/* --------------------------------------------------
 * Sysfs Attribute Show Functions
 * -------------------------------------------------- */

/**
 * cpu_count_show - Display number of online CPUs
 */
static ssize_t cpu_count_show(struct kobject *kobj,
                              struct kobj_attribute *attr,
                              char *buf)
{
    return sprintf(buf, "%u\n", num_online_cpus());
}

/**
 * cpu_total_show - Display total number of CPUs (online + offline)
 */
static ssize_t cpu_total_show(struct kobject *kobj,
                              struct kobj_attribute *attr,
                              char *buf)
{
    return sprintf(buf, "%u\n", num_possible_cpus());
}

/**
 * cpu_topology_show - Display CPU topology information
 */
static ssize_t cpu_topology_show(struct kobject *kobj,
                                 struct kobj_attribute *attr,
                                 char *buf)
{
    int cpu;
    ssize_t len = 0;
    
    len += sprintf(buf + len, "CPU Core_ID Socket_ID\n");
    
    for_each_online_cpu(cpu) {
        len += sprintf(buf + len, "%3d %7d %9d\n",
                      cpu,
                      topology_core_id(cpu),
                      topology_physical_package_id(cpu));
        
        if (len > PAGE_SIZE - 100)
            break;
    }
    
    return len;
}

/**
 * cpu_info_show - Display detailed CPU information
 */
static ssize_t cpu_info_show(struct kobject *kobj,
                             struct kobj_attribute *attr,
                             char *buf)
{
    ssize_t len = 0;
    
#ifdef CONFIG_X86
    struct cpuinfo_x86 *c = &boot_cpu_data;
    len += sprintf(buf + len, "vendor=%s\n", c->x86_vendor_id);
    len += sprintf(buf + len, "model=%s\n", c->x86_model_id);
    len += sprintf(buf + len, "family=%d\n", c->x86);
    len += sprintf(buf + len, "model_num=%d\n", c->x86_model);
    len += sprintf(buf + len, "stepping=%d\n", c->x86_stepping);
    len += sprintf(buf + len, "cache_size=%d KB\n", c->x86_cache_size);
    len += sprintf(buf + len, "cache_alignment=%d\n", c->x86_cache_alignment);
#else
    len += sprintf(buf + len, "architecture=non-x86\n");
    len += sprintf(buf + len, "cpus=%u\n", num_online_cpus());
#endif
    
    return len;
}

/**
 * mem_total_show - Display total system memory in bytes
 */
static ssize_t mem_total_show(struct kobject *kobj,
                              struct kobj_attribute *attr,
                              char *buf)
{
    unsigned long total_bytes = totalram_pages() << PAGE_SHIFT;
    return sprintf(buf, "%lu\n", total_bytes);
}

/**
 * mem_info_show - Display detailed memory information
 */
static ssize_t mem_info_show(struct kobject *kobj,
                             struct kobj_attribute *attr,
                             char *buf)
{
    struct sysinfo si;
    ssize_t len = 0;
    unsigned long available;
    
    si_meminfo(&si);
    
    len += sprintf(buf + len, "total=%lu KB\n", si.totalram * (PAGE_SIZE / 1024));
    len += sprintf(buf + len, "free=%lu KB\n", si.freeram * (PAGE_SIZE / 1024));
    
    /* Calculate available memory safely */
    available = si.freeram + si.bufferram;
    len += sprintf(buf + len, "available=%lu KB\n", available * (PAGE_SIZE / 1024));
    len += sprintf(buf + len, "buffers=%lu KB\n", si.bufferram * (PAGE_SIZE / 1024));
    
    /* Get cached memory from global state */
    len += sprintf(buf + len, "cached=%lu KB\n", 
                   global_node_page_state(NR_FILE_PAGES) * (PAGE_SIZE / 1024));
    len += sprintf(buf + len, "shared=%lu KB\n", si.sharedram * (PAGE_SIZE / 1024));
    len += sprintf(buf + len, "page_size=%lu bytes\n", PAGE_SIZE);
    
    return len;
}

/**
 * numa_nodes_show - Display number of NUMA nodes
 */
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

/**
 * numa_info_show - Display NUMA node information
 */
static ssize_t numa_info_show(struct kobject *kobj,
                              struct kobj_attribute *attr,
                              char *buf)
{
#ifdef CONFIG_NUMA
    int nid;
    ssize_t len = 0;
    struct pglist_data *pgdat;
    
    len += sprintf(buf + len, "Node Total_MB Free_MB\n");
    
    for_each_online_node(nid) {
        pgdat = NODE_DATA(nid);
        if (pgdat) {
            unsigned long total = pgdat->node_present_pages;
            unsigned long free = 0;
            struct zone *zone;
            
            /* Sum free pages across all zones in this node */
            for (zone = pgdat->node_zones; 
                 zone < pgdat->node_zones + MAX_NR_ZONES; zone++) {
                if (populated_zone(zone)) {
                    free += zone_page_state(zone, NR_FREE_PAGES);
                }
            }
            
            len += sprintf(buf + len, "%4d %8lu %7lu\n",
                          nid,
                          (total * PAGE_SIZE) >> 20,
                          (free * PAGE_SIZE) >> 20);
        }
        
        if (len > PAGE_SIZE - 100)
            break;
    }
    
    return len;
#else
    return sprintf(buf, "NUMA not configured\n");
#endif
}

/**
 * gpu_info_show - Display GPU information
 */
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

/**
 * gpu_details_show - Display detailed GPU information
 */
static ssize_t gpu_details_show(struct kobject *kobj,
                                struct kobj_attribute *attr,
                                char *buf)
{
    struct pci_dev *pdev = NULL;
    ssize_t len = 0;
    int gpu_num = 0;
    
    len += sprintf(buf + len, "ID Vendor Device_ID Bus_Addr\n");
    
    for_each_pci_dev(pdev) {
        u8 class = (pdev->class >> 16) & 0xff;
        
        if (class == 0x03) { /* VGA controller */
            len += sprintf(buf + len, "%2d 0x%04x 0x%04x   %s\n",
                          gpu_num++,
                          pdev->vendor,
                          pdev->device,
                          pci_name(pdev));
            
            if (len > PAGE_SIZE - 200)
                break;
        }
    }
    
    if (gpu_num == 0)
        len = sprintf(buf, "No GPUs detected\n");
    
    return len;
}

/**
 * accelerator_count_show - Display number of AI accelerators
 */
static ssize_t accelerator_count_show(struct kobject *kobj,
                                       struct kobj_attribute *attr,
                                       char *buf)
{
    int g, n, a, i, accel;
    scan_pci_devices(&g, &n, &a, &i, &accel);

    return sprintf(buf, "%d\n", accel);
}

/**
 * accelerator_details_show - Display detailed accelerator information
 */
static ssize_t accelerator_details_show(struct kobject *kobj,
                                        struct kobj_attribute *attr,
                                        char *buf)
{
    struct pci_dev *pdev = NULL;
    ssize_t len = 0;
    int accel_num = 0;
    
    len += sprintf(buf + len, "ID Vendor Device_ID Bus_Addr\n");
    
    for_each_pci_dev(pdev) {
        u8 class = (pdev->class >> 16) & 0xff;
        
        if (class == 0x12) { /* Processing accelerator */
            len += sprintf(buf + len, "%2d 0x%04x 0x%04x   %s\n",
                          accel_num++,
                          pdev->vendor,
                          pdev->device,
                          pci_name(pdev));
            
            if (len > PAGE_SIZE - 200)
                break;
        }
    }
    
    if (accel_num == 0)
        len = sprintf(buf, "No accelerators detected\n");
    
    return len;
}

/**
 * system_summary_show - Display overall system summary
 */
static ssize_t system_summary_show(struct kobject *kobj,
                                   struct kobj_attribute *attr,
                                   char *buf)
{
    int gpus, nvidia, amd, intel, accel;
    ssize_t len = 0;
    
    scan_pci_devices(&gpus, &nvidia, &amd, &intel, &accel);
    
    len += sprintf(buf + len, "=== NeuroShell System Summary ===\n\n");
    
    len += sprintf(buf + len, "CPU:\n");
    len += sprintf(buf + len, "  Online: %u\n", num_online_cpus());
    len += sprintf(buf + len, "  Total:  %u\n\n", num_possible_cpus());
    
    len += sprintf(buf + len, "Memory:\n");
    len += sprintf(buf + len, "  Total: %lu MB\n\n", 
                   (totalram_pages() << PAGE_SHIFT) >> 20);
    
#ifdef CONFIG_NUMA
    len += sprintf(buf + len, "NUMA:\n");
    len += sprintf(buf + len, "  Nodes: %d\n\n", num_online_nodes());
#endif
    
    len += sprintf(buf + len, "GPUs:\n");
    len += sprintf(buf + len, "  Total:  %d\n", gpus);
    if (nvidia) len += sprintf(buf + len, "  NVIDIA: %d\n", nvidia);
    if (amd) len += sprintf(buf + len, "  AMD:    %d\n", amd);
    if (intel) len += sprintf(buf + len, "  Intel:  %d\n", intel);
    len += sprintf(buf + len, "\n");
    
    len += sprintf(buf + len, "Accelerators:\n");
    len += sprintf(buf + len, "  Count: %d\n", accel);
    
    return len;
}

/* --------------------------------------------------
 * Sysfs Attribute Definitions
 * -------------------------------------------------- */

static struct kobj_attribute cpu_count_attr =
    __ATTR(cpu_count, 0444, cpu_count_show, NULL);

static struct kobj_attribute cpu_total_attr =
    __ATTR(cpu_total, 0444, cpu_total_show, NULL);

static struct kobj_attribute cpu_topology_attr =
    __ATTR(cpu_topology, 0444, cpu_topology_show, NULL);

static struct kobj_attribute cpu_info_attr =
    __ATTR(cpu_info, 0444, cpu_info_show, NULL);

static struct kobj_attribute mem_total_attr =
    __ATTR(mem_total_bytes, 0444, mem_total_show, NULL);

static struct kobj_attribute mem_info_attr =
    __ATTR(mem_info, 0444, mem_info_show, NULL);

static struct kobj_attribute numa_nodes_attr =
    __ATTR(numa_nodes, 0444, numa_nodes_show, NULL);

static struct kobj_attribute numa_info_attr =
    __ATTR(numa_info, 0444, numa_info_show, NULL);

static struct kobj_attribute gpu_info_attr =
    __ATTR(gpu_info, 0444, gpu_info_show, NULL);

static struct kobj_attribute gpu_details_attr =
    __ATTR(gpu_details, 0444, gpu_details_show, NULL);

static struct kobj_attribute accel_count_attr =
    __ATTR(accelerator_count, 0444, accelerator_count_show, NULL);

static struct kobj_attribute accel_details_attr =
    __ATTR(accelerator_details, 0444, accelerator_details_show, NULL);

static struct kobj_attribute system_summary_attr =
    __ATTR(system_summary, 0444, system_summary_show, NULL);

static struct attribute *neuroshell_attrs[] = {
    &cpu_count_attr.attr,
    &cpu_total_attr.attr,
    &cpu_topology_attr.attr,
    &cpu_info_attr.attr,
    &mem_total_attr.attr,
    &mem_info_attr.attr,
    &numa_nodes_attr.attr,
    &numa_info_attr.attr,
    &gpu_info_attr.attr,
    &gpu_details_attr.attr,
    &accel_count_attr.attr,
    &accel_details_attr.attr,
    &system_summary_attr.attr,
    NULL,
};

static struct attribute_group neuroshell_group = {
    .attrs = neuroshell_attrs,
};

/* --------------------------------------------------
 * Module Init and Exit
 * -------------------------------------------------- */

static int __init neuroshell_init(void)
{
    int ret;

    pr_info("neuroshell: initializing v0.3\n");

    /* Create kobject under /sys/kernel/ */
    neuroshell_kobj = kobject_create_and_add("neuroshell", kernel_kobj);
    if (!neuroshell_kobj) {
        pr_err("neuroshell: failed to create kobject\n");
        return -ENOMEM;
    }

    /* Create sysfs group */
    ret = sysfs_create_group(neuroshell_kobj, &neuroshell_group);
    if (ret) {
        pr_err("neuroshell: failed to create sysfs group\n");
        kobject_put(neuroshell_kobj);
        return ret;
    }

    pr_info("neuroshell: loaded successfully - interface at /sys/kernel/neuroshell/\n");
    return 0;
}

static void __exit neuroshell_exit(void)
{
    sysfs_remove_group(neuroshell_kobj, &neuroshell_group);
    kobject_put(neuroshell_kobj);
    pr_info("neuroshell: module unloaded\n");
}

module_init(neuroshell_init);
module_exit(neuroshell_exit);
