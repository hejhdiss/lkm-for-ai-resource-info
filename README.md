# NeuroShell LKM

**Linux Kernel Module for Hardware Introspection**

A kernel module that exposes detailed hardware information via `/sys/kernel/neuroshell/` to support AI workload optimization and system introspection.

> If you want more featured , delete neuroshell_enhnaced.c  then, rename neuroshell_enhnaced_.c to neuroshell_enhnaced.c.
>   then make and  insmod,modprobe,rmmod - commands needed

---

## ⚠️ Development Disclosure

**This project was created with AI assistance (Claude by Anthropic).**

I (HejHdiss) have a solid background in pure C, some standard libraries, and some POSIX C—**all self-taught**—but **I do not have deep kernel development experience**. I created prompts and tested both LKM modules to ensure they work, but the kernel-level code was largely AI-generated.

### Why This Approach?

- I understand the *what* (system requirements) but not the *how* (kernel internals)
- The goal is to prototype the boot-time hardware discovery concept for [NeuroShell OS](https://dev.to/hejhdiss/neuroshell-os-rethinking-boot-time-design-for-ai-native-computing-4m53)
- This serves as a proof-of-concept that needs **experienced kernel developers** to make production-ready

### Current Limitations

Since I lack deep kernel knowledge, this module:
- Has not been tested in production environments
- May not follow all kernel best practices
- Cannot leverage deeper kernel APIs I'm unfamiliar with
- Needs security and performance review by experts

---

## 🤝 **CONTRIBUTORS WANTED**

**I am actively seeking kernel developers** to help transform this concept into production-quality code.

### Why Your Contribution Matters

This module is **just the basic foundation** compared to what's described in the [NeuroShell OS article](https://dev.to/hejhdiss/neuroshell-os-rethinking-boot-time-design-for-ai-native-computing-4m53). The full vision requires:

- **Real-time hardware discovery during boot**
- **Dynamic resource allocation for AI workloads**
- **Integration with bootloader and init systems**
- **Hardware-aware scheduler hooks**
- **Memory topology optimization for tensor operations**

**This is NeuroShell OS's boot design foundation.** By contributing, you're not just fixing a kernel module—you're helping build the infrastructure for AI-native operating systems.

### What I Need Help With:

1. **Code Review & Hardening**
   - Proper error handling and edge cases
   - Memory safety and race condition checks
   - Kernel coding style compliance

2. **Advanced Features**
   - Deeper hardware introspection (PCIe topology, IOMMU, etc.)
   - Real-time performance metrics
   - Integration with existing kernel subsystems
   - Hotplug event handling for dynamic hardware changes

3. **Performance & Security**
   - Optimization for minimal overhead
   - Security audit and threat modeling
   - Proper locking mechanisms
   - SMP-safe concurrent access

4. **Architecture Guidance**
   - Is this the right approach for boot-time integration?
   - Alternative kernel APIs that might be better
   - How to integrate with bootloaders and init systems
   - Designing the full NeuroShell OS boot sequence

### The Vision

This module is part of a larger goal: **NeuroShell OS** - rethinking boot-time design for AI-native computing.

**Read the full concept here:**  
📖 [NeuroShell OS: Rethinking Boot-Time Design for AI-Native Computing](https://dev.to/hejhdiss/neuroshell-os-rethinking-boot-time-design-for-ai-native-computing-4m53)

**What exists now is basic.** What's needed is a complete reimagining of how an OS boots and configures itself for AI workloads. If you're a kernel developer interested in AI infrastructure and pushing the boundaries of OS design, **your expertise could make this vision real**.

---

## 📋 Current Features

### Hardware Detection
- **CPU**: Online/total count, topology, vendor info
- **Memory**: Total/free/available, NUMA node breakdown
- **NUMA**: Node count and per-node memory stats
- **GPU**: Detection of NVIDIA, AMD, Intel GPUs with PCI details
- **Accelerators**: AI/ML accelerator enumeration (TPUs, NPUs, etc.)

### Sysfs Interface
All data exposed via `/sys/kernel/neuroshell/`:
```
/sys/kernel/neuroshell/
├── cpu_count           # Online CPUs
├── cpu_total           # Total CPUs (incl. offline)
├── cpu_info            # CPU vendor, model, cache
├── cpu_topology        # Core/socket mapping
├── mem_total_bytes     # Total RAM in bytes
├── mem_info            # Detailed memory stats
├── numa_nodes          # NUMA node count
├── numa_info           # Per-node memory info
├── gpu_info            # GPU summary (count by vendor)
├── gpu_details         # PCI address, device IDs
├── accelerator_count   # AI accelerator count
├── accelerator_details # Accelerator PCI info
└── system_summary      # Human-readable overview
```

---

## 🚀 Quick Start

### Prerequisites
```bash
sudo apt install build-essential linux-headers-$(uname -r)
```

### Build
```bash
make
```

### Load Module
```bash
# Basic module
sudo insmod neuroshell.ko

# Or enhanced version (recommended)
sudo insmod neuroshell_enhanced.ko
```

### Test
```bash
# Run test suite
chmod +x test_neuroshell.sh
./test_neuroshell.sh

# View system summary
cat /sys/kernel/neuroshell/system_summary

# Python interface
python3 neuroshell.py --all
python3 neuroshell.py --summary
python3 neuroshell.py --json
```

### Unload Module
```bash
sudo rmmod neuroshell_enhanced
# or
sudo rmmod neuroshell
```

---

## 🧪 Testing Status

### Tested On
- **Hypervisor**: VMware Workstation 17
- **OS**: Xubuntu 24.04.2 LTS
- **Kernel**: Linux 6.11

### Working Components
- ✅ `neuroshell.c` (basic module)
- ✅ `neuroshell_enhanced.c` (advanced features)
- ✅ `neuroshell.py` (Python interface)
- ✅ `test_neuroshell.sh` (test suite)

### Not Tested
- ❌ Production environments
- ❌ High-load scenarios
- ❌ Real AI accelerators (tested on VM with emulated hardware)
- ❌ Security hardening

---

## 📦 File Structure

```
neuroshell/
├── neuroshell.c              # Basic kernel module
├── neuroshell_enhanced.c     # Enhanced module with detailed info
├── neuroshell.py             # Python interface for easy querying
├── test_neuroshell.sh        # Comprehensive test suite
├── Makefile                  # Build configuration
├── LICENSE                   # GPL v3
└── README.md                 # This file
```

---

## 🐍 Python Interface Examples

```python
from neuroshell import NeuroShell

ns = NeuroShell()

# Get all system info
info = ns.get_all_info()

# Check hardware requirements
ns.check_requirements(
    min_cpus=8,
    min_memory_gb=16.0,
    min_gpus=1
)

# Get specific info
cpu_info = ns.get_cpu_info()
gpu_info = ns.get_gpu_info()
```

### Command Line
```bash
# System summary
python3 neuroshell.py --summary

# All info as JSON
python3 neuroshell.py --all --json

# Specific components
python3 neuroshell.py --cpu --memory --gpu

# Check if system meets requirements
python3 neuroshell.py --check-requirements \
    --min-cpus 4 \
    --min-memory-gb 8.0 \
    --min-gpus 1
```

---

## 🛠️ Building from Source

```bash
# Install dependencies
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r)

# Clone repository
git clone https://github.com/hejhdiss/lkm-for-ai-resource-info.git
cd lkm-for-ai-resource-info

# Build
make

# Load and test
sudo insmod neuroshell_enhanced.ko
cat /sys/kernel/neuroshell/system_summary

# Unload when done
sudo rmmod neuroshell_enhanced
```

---

## 📖 API Documentation

### Sysfs Attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `cpu_count` | int | Number of online CPUs |
| `cpu_total` | int | Total CPUs (including offline) |
| `cpu_info` | kv | CPU vendor, model, family, cache |
| `cpu_topology` | table | CPU to core/socket mapping |
| `mem_total_bytes` | int | Total RAM in bytes |
| `mem_info` | kv | Total, free, available, cached memory |
| `numa_nodes` | int | Number of NUMA nodes |
| `numa_info` | table | Per-node memory breakdown |
| `gpu_info` | kv | GPU counts by vendor |
| `gpu_details` | table | PCI details for each GPU |
| `accelerator_count` | int | Number of AI accelerators |
| `accelerator_details` | table | Accelerator PCI information |
| `system_summary` | text | Human-readable system overview |

**Format Legend:**
- `int` - Single integer value
- `kv` - Key-value pairs (key=value format)
- `table` - Tabular data with headers
- `text` - Human-readable text

---

## 🤔 FAQ

### Why create this if AI-generated?
Because the *concept* is valuable. I can validate functionality and requirements, but need experts for production readiness.

### Is this production-ready?
**No.** This is a proof-of-concept. Do not use in production without thorough review and hardening.

### Can I contribute?
**YES PLEASE!** See the "Contributors Wanted" section above.

### Why not use existing tools?
This is designed for **boot-time** integration in NeuroShell OS, where traditional tools aren't available yet. See the [full article](https://dev.to/hejhdiss/neuroshell-os-rethinking-boot-time-design-for-ai-native-computing-4m53) for context.

### What's the license?
GPL v3 - see LICENSE file

---

## 🚨 Known Issues & Limitations

- **No locking mechanisms** - not SMP-safe for concurrent access
- **Buffer overflow potential** - fixed-size buffers in sysfs show functions
- **Limited error handling** - assumes happy path in many places
- **No hotplug support** - doesn't react to CPU/memory/PCI hotplug events
- **X86-centric** - some features assume x86 architecture
- **No versioning** - sysfs ABI may change without notice

**These issues need kernel developer expertise to resolve properly.**

---

## 📞 Contact & Contributing

- **Author**: HejHdiss
- **Repository**: [github.com/hejhdiss/lkm-for-ai-resource-info](https://github.com/hejhdiss/lkm-for-ai-resource-info)
- **Project Goal**: Enable AI-native OS development
- **Contribution**: Open to PRs, issues, and design discussions
- **Background Article**: [NeuroShell OS Design](https://dev.to/hejhdiss/neuroshell-os-rethinking-boot-time-design-for-ai-native-computing-4m53)

### How to Contribute

**Remember**: This current module is just scratching the surface. The full NeuroShell OS boot design needs much more.

1. **Code Review**: Audit existing code, suggest improvements
2. **Features**: Implement advanced kernel features for boot-time AI resource discovery
3. **Documentation**: Improve kernel-specific docs and design proposals
4. **Testing**: Test on real AI hardware (TPUs, NPUs, multi-GPU systems)
5. **Architecture**: Help design the complete NeuroShell OS boot sequence and resource allocation system
6. **Integration**: Work on bootloader integration, init system hooks, and early-boot hardware configuration

**Your contribution helps build the foundation for a new class of AI-native operating systems.**

---

## 📜 License

This project is licensed under the **GNU General Public License v3.0**.

See [LICENSE](LICENSE) file for full text.

---

## 🙏 Acknowledgments

- **Claude (Anthropic)** for AI-assisted kernel module generation
- **Linux kernel community** for documentation and examples
- **Future contributors** who will help make this production-ready

---

## 🎯 Project Status

**Current Phase**: Proof of Concept  
**Next Phase**: Community Review & Hardening  
**End Goal**: Production kernel module for NeuroShell OS

**This is just the beginning. Join me in building AI-native infrastructure from the ground up.**
---
