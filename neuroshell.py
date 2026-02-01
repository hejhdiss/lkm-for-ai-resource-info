#!/usr/bin/env python3
"""
NeuroShell LKM  Python Interface
A utility for querying system hardware information via NeuroShell kernel module
"""

import os
import json
import sys
from pathlib import Path
from typing import Dict, Optional, Any

NEUROSHELL_PATH = Path("/sys/kernel/neuroshell")

class NeuroShellError(Exception):
    """Exception raised when NeuroShell module is not available"""
    pass

class NeuroShell:
    """Interface to NeuroShell kernel module"""
    
    def __init__(self):
        """Initialize NeuroShell interface"""
        if not NEUROSHELL_PATH.exists():
            raise NeuroShellError(
                "NeuroShell module not loaded. "
                "Please run 'sudo insmod neuroshell_enhanced.ko' first."
            )
    
    def read_attr(self, attribute: str) -> Optional[str]:
        """
        Read a NeuroShell sysfs attribute
        
        Args:
            attribute: Name of the attribute file
            
        Returns:
            Content of the attribute as string, or None if not found
        """
        attr_path = NEUROSHELL_PATH / attribute
        try:
            return attr_path.read_text().strip()
        except FileNotFoundError:
            return None
        except PermissionError:
            print(f"Permission denied reading {attribute}", file=sys.stderr)
            return None
    
    def parse_key_value(self, content: str) -> Dict[str, Any]:
        """
        Parse key=value format content
        
        Args:
            content: String with key=value pairs
            
        Returns:
            Dictionary of parsed values
        """
        result = {}
        for line in content.split('\n'):
            line = line.strip()
            if '=' in line:
                key, value = line.split('=', 1)
                # Try to convert to int
                try:
                    result[key] = int(value)
                except ValueError:
                    result[key] = value
        return result
    
    def get_cpu_info(self) -> Dict[str, Any]:
        """Get CPU information"""
        info = {}
        
        # Basic counts
        cpu_count = self.read_attr('cpu_count')
        if cpu_count:
            info['online'] = int(cpu_count)
        
        cpu_total = self.read_attr('cpu_total')
        if cpu_total:
            info['total'] = int(cpu_total)
        
        # Detailed info
        cpu_info = self.read_attr('cpu_info')
        if cpu_info:
            info.update(self.parse_key_value(cpu_info))
        
        # Topology
        topology = self.read_attr('cpu_topology')
        if topology:
            info['topology'] = topology
        
        return info
    
    def get_memory_info(self) -> Dict[str, Any]:
        """Get memory information"""
        info = {}
        
        # Total bytes
        mem_total = self.read_attr('mem_total_bytes')
        if mem_total:
            total_bytes = int(mem_total)
            info['total_bytes'] = total_bytes
            info['total_mb'] = total_bytes // (1024 * 1024)
            info['total_gb'] = total_bytes / (1024 * 1024 * 1024)
        
        # Detailed info
        mem_info = self.read_attr('mem_info')
        if mem_info:
            info.update(self.parse_key_value(mem_info))
        
        return info
    
    def get_numa_info(self) -> Dict[str, Any]:
        """Get NUMA information"""
        info = {}
        
        # Node count
        numa_nodes = self.read_attr('numa_nodes')
        if numa_nodes:
            info['nodes'] = int(numa_nodes)
        
        # Detailed info
        numa_info = self.read_attr('numa_info')
        if numa_info:
            info['details'] = numa_info
        
        return info
    
    def get_gpu_info(self) -> Dict[str, Any]:
        """Get GPU information"""
        info = {}
        
        # Summary
        gpu_info = self.read_attr('gpu_info')
        if gpu_info:
            info.update(self.parse_key_value(gpu_info))
        
        # Details
        gpu_details = self.read_attr('gpu_details')
        if gpu_details:
            info['details'] = gpu_details
        
        return info
    
    def get_accelerator_info(self) -> Dict[str, Any]:
        """Get AI accelerator information"""
        info = {}
        
        # Count
        accel_count = self.read_attr('accelerator_count')
        if accel_count:
            info['count'] = int(accel_count)
        
        # Details
        accel_details = self.read_attr('accelerator_details')
        if accel_details:
            info['details'] = accel_details
        
        return info
    
    def get_system_summary(self) -> str:
        """Get complete system summary"""
        return self.read_attr('system_summary') or "Summary not available"
    
    def get_all_info(self) -> Dict[str, Any]:
        """Get all available information"""
        return {
            'cpu': self.get_cpu_info(),
            'memory': self.get_memory_info(),
            'numa': self.get_numa_info(),
            'gpu': self.get_gpu_info(),
            'accelerator': self.get_accelerator_info(),
        }
    
    def check_requirements(self, 
                          min_cpus: int = 1,
                          min_memory_gb: float = 0,
                          min_gpus: int = 0) -> bool:
        """
        Check if system meets minimum requirements
        
        Args:
            min_cpus: Minimum number of CPUs required
            min_memory_gb: Minimum memory in GB required
            min_gpus: Minimum number of GPUs required
            
        Returns:
            True if all requirements are met
        """
        cpu_info = self.get_cpu_info()
        mem_info = self.get_memory_info()
        gpu_info = self.get_gpu_info()
        
        checks = {
            'cpus': cpu_info.get('online', 0) >= min_cpus,
            'memory': mem_info.get('total_gb', 0) >= min_memory_gb,
            'gpus': gpu_info.get('total', 0) >= min_gpus,
        }
        
        print("Requirement Check:")
        print(f"  CPUs: {cpu_info.get('online', 0)} >= {min_cpus} : {'✓' if checks['cpus'] else '✗'}")
        print(f"  Memory: {mem_info.get('total_gb', 0):.1f}GB >= {min_memory_gb}GB : {'✓' if checks['memory'] else '✗'}")
        print(f"  GPUs: {gpu_info.get('total', 0)} >= {min_gpus} : {'✓' if checks['gpus'] else '✗'}")
        
        return all(checks.values())


def main():
    """Command-line interface"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description='NeuroShell System Information Utility',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --summary              # Show system summary
  %(prog)s --cpu                  # Show CPU information
  %(prog)s --all --json           # Show all info in JSON format
  %(prog)s --check-requirements   # Check minimum requirements
        """
    )
    
    parser.add_argument('--summary', action='store_true',
                       help='Display system summary')
    parser.add_argument('--cpu', action='store_true',
                       help='Display CPU information')
    parser.add_argument('--memory', action='store_true',
                       help='Display memory information')
    parser.add_argument('--numa', action='store_true',
                       help='Display NUMA information')
    parser.add_argument('--gpu', action='store_true',
                       help='Display GPU information')
    parser.add_argument('--accelerator', action='store_true',
                       help='Display accelerator information')
    parser.add_argument('--all', action='store_true',
                       help='Display all information')
    parser.add_argument('--json', action='store_true',
                       help='Output in JSON format')
    parser.add_argument('--check-requirements', action='store_true',
                       help='Check if system meets minimum requirements')
    parser.add_argument('--min-cpus', type=int, default=4,
                       help='Minimum CPUs required (default: 4)')
    parser.add_argument('--min-memory-gb', type=float, default=8.0,
                       help='Minimum memory in GB required (default: 8.0)')
    parser.add_argument('--min-gpus', type=int, default=1,
                       help='Minimum GPUs required (default: 1)')
    
    args = parser.parse_args()
    
    try:
        ns = NeuroShell()
        
        if args.check_requirements:
            result = ns.check_requirements(
                min_cpus=args.min_cpus,
                min_memory_gb=args.min_memory_gb,
                min_gpus=args.min_gpus
            )
            sys.exit(0 if result else 1)
        
        if args.summary:
            print(ns.get_system_summary())
            return
        
        info = {}
        
        if args.all:
            info = ns.get_all_info()
        else:
            if args.cpu:
                info['cpu'] = ns.get_cpu_info()
            if args.memory:
                info['memory'] = ns.get_memory_info()
            if args.numa:
                info['numa'] = ns.get_numa_info()
            if args.gpu:
                info['gpu'] = ns.get_gpu_info()
            if args.accelerator:
                info['accelerator'] = ns.get_accelerator_info()
        
        if not info:
            # Default: show summary
            print(ns.get_system_summary())
            return
        
        if args.json:
            print(json.dumps(info, indent=2))
        else:
            for category, data in info.items():
                print(f"\n=== {category.upper()} ===")
                if isinstance(data, dict):
                    for key, value in data.items():
                        if not isinstance(value, str) or '\n' not in value:
                            print(f"  {key}: {value}")
                        else:
                            print(f"  {key}:")
                            for line in value.split('\n'):
                                print(f"    {line}")
                else:
                    print(data)
    
    except NeuroShellError as e:
        print(f"Error: {e}", file=sys.stderr)
        print("\nMake sure the NeuroShell kernel module is loaded:", file=sys.stderr)
        print("  sudo insmod neuroshell_enhanced.ko", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nInterrupted", file=sys.stderr)
        sys.exit(130)


if __name__ == '__main__':
    main()
