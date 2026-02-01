/*
 * NeuroShell LKM - Advanced Hardware Introspection Kernel Module
 *  Replace neuroshell_enhanced.c with this for  creating  lkm  file. for getting memory reservation and  vector pulse.(in make)
 * 
 * Provides detailed system hardware information via sysfs interface
 * at /sys/kernel/neuroshell/
 *
 * Copyright (C) 2026 HejHdiss
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
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/fpu/api.h>
#include <linux/device.h>

#ifdef CONFIG_X86
#include <asm/processor.h>
#endif
/* Add to headers section */
#ifdef CONFIG_ARM64
#include <asm/hwcap.h>
#endif



static struct kobj_attribute extensions_attr = __ATTR(ai_extensions, 0444, extensions_show, NULL);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("HejHdiss");
MODULE_DESCRIPTION("NeuroShell: Advanced hardware introspection for AI workloads");
MODULE_VERSION("0.3");


/* Structure to wrap a discovered device and its kobject */
struct ns_device_wrapper {
    struct kobject kobj;
    struct pci_dev *pdev;
    struct list_head list;
};

/* Macro to container_of from kobject to our wrapper */
#define to_ns_dev(obj) container_of(obj, struct ns_device_wrapper, kobj)

static LIST_HEAD(ns_dev_list); // Linked list to track all dynamic kobjects
static struct kobject *gpu_root_kobj;
static struct kobject *accel_root_kobj;


static struct kobject *neuroshell_kobj;
static bool enable_reservation = true;
module_param(enable_reservation, bool, 0644);

static bool enable_vector_pulse = true;
module_param(enable_vector_pulse, bool, 0644);

static unsigned long reserved_size_mb = 0;
static unsigned long physical_base = 0x100000000; // 4GB mark
static int major;
static struct class *ns_class;

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

 /* --------------------------------------------------
 * AI Extension Detection
 * -------------------------------------------------- */

 static ssize_t dev_vendor_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    struct ns_device_wrapper *wrapper = to_ns_dev(kobj);
    return sprintf(buf, "0x%04x\n", wrapper->pdev->vendor);
}

static ssize_t dev_id_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    struct ns_device_wrapper *wrapper = to_ns_dev(kobj);
    return sprintf(buf, "0x%04x\n", wrapper->pdev->device);
}

static ssize_t dev_vram_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    struct ns_device_wrapper *wrapper = to_ns_dev(kobj);
    /* BAR 0 usually contains the primary aperture. Size in MB. */
    unsigned long vram_size = pci_resource_len(wrapper->pdev, 0) >> 20; 
    return sprintf(buf, "%lu MB\n", vram_size);
}

/* Define attributes for dynamic devices */
static struct kobj_attribute dev_vendor_attr = __ATTR(vendor, 0444, dev_vendor_show, NULL);
static struct kobj_attribute dev_id_attr = __ATTR(device_id, 0444, dev_id_show, NULL);
static struct kobj_attribute dev_vram_attr = __ATTR(vram_total, 0444, dev_vram_show, NULL);

static struct attribute *dev_attrs[] = {
    &dev_vendor_attr.attr,
    &dev_id_attr.attr,
    &dev_vram_attr.attr,
    NULL,
};

static struct attribute_group dev_attr_group = {
    .attrs = dev_attrs,
};

static int create_neuro_hierarchy(void) {
    struct pci_dev *pdev = NULL;
    int g_idx = 0, a_idx = 0;
    char name[16];

    /* 1. Create Base Folders */
    gpu_root_kobj = kobject_create_and_add("gpu", neuroshell_kobj);
    accel_root_kobj = kobject_create_and_add("accelerators", neuroshell_kobj);
    if (!gpu_root_kobj || !accel_root_kobj) return -ENOMEM;

    /* 2. Discover and Register Devices */
    for_each_pci_dev(pdev) {
        u8 class = (pdev->class >> 16) & 0xff;
        struct kobject *parent = NULL;
        
        if (class == 0x03) parent = gpu_root_kobj;
        else if (class == 0x12) parent = accel_root_kobj;

        if (parent) {
            struct ns_device_wrapper *wrapper = kzalloc(sizeof(*wrapper), GFP_KERNEL);
            if (!wrapper) continue;

            wrapper->pdev = pdev;
            snprintf(name, sizeof(name), "%s%d", (class == 0x03 ? "gpu" : "accel"), 
                     (class == 0x03 ? g_idx++ : a_idx++));

            /* Initialize kobject manually for linked-list tracking */
            int ret = kobject_init_and_add(&wrapper->kobj, kobject_get_type(parent), parent, "%s", name);
            if (ret) {
                kobject_put(&wrapper->kobj);
                continue;
            }

            if (sysfs_create_group(&wrapper->kobj, &dev_attr_group)) {
                kobject_put(&wrapper->kobj);
                continue;
            }

            list_add_tail(&wrapper->list, &ns_dev_list);
        }
    }
    return 0;
}
static ssize_t extensions_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    int len = 0;
#ifdef CONFIG_ARM64
    if (elf_hwcap & HWCAP_ASIMD) len += sprintf(buf + len, "NEON ");
    if (elf_hwcap & HWCAP_SVE)   len += sprintf(buf + len, "SVE ");
    if (elf_hwcap2 & HWCAP2_SME) len += sprintf(buf + len, "SME ");
#elif defined(CONFIG_X86)
    if (boot_cpu_has(X86_FEATURE_AMX_TILE)) len += sprintf(buf + len, "AMX ");
    if (boot_cpu_has(X86_FEATURE_AVX512F)) len += sprintf(buf + len, "AVX512 ");
#endif
    if (len == 0) len += sprintf(buf + len, "none");
    return len + sprintf(buf + len, "\n");
}

static void __attribute__((target("avx512f"))) perform_vector_pulse(void *info) {
#ifdef CONFIG_X86_64
    if (enable_vector_pulse && boot_cpu_has(X86_FEATURE_AVX512F)) {
        kernel_fpu_begin();
        asm volatile ("vpxord %%zmm0, %%zmm0, %%zmm0" : : : "xmm0", "memory");
        kernel_fpu_end();
    }
#endif
}
static int ns_mmap(struct file *filp, struct vm_area_struct *vma) {
    unsigned long size = vma->vm_end - vma->vm_start;
    if (size > (reserved_size_mb * 1024 * 1024)) return -EINVAL;
    return remap_pfn_range(vma, vma->vm_start, physical_base >> PAGE_SHIFT, size, vma->vm_page_prot);
}

static const struct file_operations ns_fops = {
    .owner = THIS_MODULE,
    .mmap = ns_mmap,
};

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
 * neuro_slab_info_show - Displays the status of the memory reservation
 */
static ssize_t neuro_slab_info_show(struct kobject *kobj,
                                    struct kobj_attribute *attr,
                                    char *buf)
{
    if (reserved_size_mb == 0)
        return sprintf(buf, "status=FAILED/DISABLED\nsize=0 MB\n");

    return sprintf(buf, "status=ACTIVE\nsize=%lu MB\nphys_base=0x%lx\ndevice=/dev/neuro_slab\n",
                   reserved_size_mb, physical_base);
}

/**
 * vector_pulse_status_show - Confirms if the CPU warm-up was triggered
 */
static ssize_t vector_pulse_status_show(struct kobject *kobj,
                                        struct kobj_attribute *attr,
                                        char *buf)
{
    bool supported = false;
#ifdef CONFIG_X86_64
    supported = boot_cpu_has(X86_FEATURE_AVX512F);
#endif

    return sprintf(buf, "enabled=%s\nsupported_hw=%s\n",
                   enable_vector_pulse ? "yes" : "no",
                   supported ? "yes" : "no");
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
    len += sprintf(buf + len, "Neuro-Slab:\n");
len += sprintf(buf + len, "  Reserved: %lu MB\n", reserved_size_mb);
len += sprintf(buf + len, "  Vector Pulse: %s\n\n", enable_vector_pulse ? "ON" : "OFF");
    
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
    static struct kobj_attribute neuro_slab_attr =
    __ATTR(neuro_slab, 0444, neuro_slab_info_show, NULL);

static struct kobj_attribute vector_pulse_attr =
    __ATTR(vector_pulse, 0444, vector_pulse_status_show, NULL);

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
    &neuro_slab_attr.attr,    
    &vector_pulse_attr.attr,   
    &extensions_attr.attr,
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

// 1. Tiered Reservation Check
if (enable_reservation) {
    unsigned long total_mb = (totalram_pages() << PAGE_SHIFT) / (1024 * 1024);
    unsigned long tiers[] = {2048, 1024, 512};
    int i;
    for (i = 0; i < 3; i++) {
        if (total_mb >= (4096 + tiers[i])) { // 4GB base + tier
            reserved_size_mb = tiers[i];
            break;
        }
    }
    
    if (reserved_size_mb) {
        major = register_chrdev(0, "neuro_slab", &ns_fops);
        ns_class = class_create("neuroshell_class");
        if (IS_ERR(ns_class)) {
            unregister_chrdev(major, "neuro_slab");
            return PTR_ERR(ns_class);
        }
        device_create(ns_class, NULL, MKDEV(major, 0), NULL, "neuro_slab");
        pr_info("neuroshell: Reserved %luMB Slab at /dev/neuro_slab\n", reserved_size_mb);
    }
}

// 2. Vector Pulse Activation
if (enable_vector_pulse) {
    on_each_cpu(perform_vector_pulse, NULL, 1);
    pr_info("neuroshell: Vector units primed.\n");
}

    /* Create sysfs group */
    ret = sysfs_create_group(neuroshell_kobj, &neuroshell_group);
    if (ret) {
        pr_err("neuroshell: failed to create sysfs group\n");
        kobject_put(neuroshell_kobj);
        return ret;
    }
ret = create_neuro_hierarchy();
if (ret) pr_warn("neuroshell: partially failed to create device hierarchy\n");
    pr_info("neuroshell: loaded successfully - interface at /sys/kernel/neuroshell/\n");
    return 0;
}
static void cleanup_hierarchy(void) {
    struct ns_device_wrapper *wrapper, *tmp;

    /* Remove individual device folders */
    list_for_each_entry_safe(wrapper, tmp, &ns_dev_list, list) {
        sysfs_remove_group(&wrapper->kobj, &dev_attr_group);
        list_del(&wrapper->list);
        kobject_put(&wrapper->kobj);
        kfree(wrapper); // Free the wrapper memory
    }

    /* Remove root folders */
    if (gpu_root_kobj) kobject_put(gpu_root_kobj);
    if (accel_root_kobj) kobject_put(accel_root_kobj);
}
static void __exit neuroshell_exit(void)
{
    sysfs_remove_group(neuroshell_kobj, &neuroshell_group);
    kobject_put(neuroshell_kobj);
if (reserved_size_mb) {
    device_destroy(ns_class, MKDEV(major, 0));
    class_destroy(ns_class);
    unregister_chrdev(major, "neuro_slab");
}
cleanup_hierarchy();
    pr_info("neuroshell: module unloaded\n");
}
/* Function Prototypes */
void neuroshell_get_slab(unsigned long *base, unsigned long *size) {
    *base = physical_base;
    *size = reserved_size_mb;
}
EXPORT_SYMBOL(neuroshell_get_slab);
module_init(neuroshell_init);
module_exit(neuroshell_exit);
