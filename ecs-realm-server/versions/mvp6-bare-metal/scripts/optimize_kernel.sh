#!/bin/bash
# [SEQUENCE: 1] Linux kernel optimization for game server performance

set -e

# [SEQUENCE: 2] Check if running as root
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 
   exit 1
fi

# [SEQUENCE: 3] Backup current sysctl settings
cp /etc/sysctl.conf /etc/sysctl.conf.backup.$(date +%Y%m%d_%H%M%S)

# [SEQUENCE: 4] Network stack optimizations
cat >> /etc/sysctl.conf << 'EOF'

# === Game Server Network Optimizations ===

# [SEQUENCE: 5] Core network settings
# Increase network buffer sizes for high throughput
net.core.rmem_default = 31457280
net.core.rmem_max = 134217728
net.core.wmem_default = 31457280
net.core.wmem_max = 134217728
net.core.netdev_max_backlog = 65536
net.core.optmem_max = 65536

# [SEQUENCE: 6] TCP optimizations
# Enable TCP Fast Open
net.ipv4.tcp_fastopen = 3

# Use BBR congestion control (better for game traffic)
net.core.default_qdisc = fq
net.ipv4.tcp_congestion_control = bbr

# Optimize TCP memory
net.ipv4.tcp_mem = 786432 1048576 26777216
net.ipv4.tcp_rmem = 4096 87380 134217728
net.ipv4.tcp_wmem = 4096 65536 134217728

# Reduce TCP timeouts for faster detection
net.ipv4.tcp_fin_timeout = 15
net.ipv4.tcp_keepalive_time = 300
net.ipv4.tcp_keepalive_probes = 5
net.ipv4.tcp_keepalive_intvl = 15

# Enable TCP timestamps, window scaling, and SACK
net.ipv4.tcp_timestamps = 1
net.ipv4.tcp_window_scaling = 1
net.ipv4.tcp_sack = 1

# Increase connection tracking
net.netfilter.nf_conntrack_max = 1000000
net.nf_conntrack_max = 1000000

# [SEQUENCE: 7] UDP optimizations for game traffic
net.core.netdev_budget = 600
net.core.netdev_budget_usecs = 5000

# [SEQUENCE: 8] IP settings
# Increase local port range
net.ipv4.ip_local_port_range = 1024 65535

# Enable IP forwarding if needed
net.ipv4.ip_forward = 0

# Disable source routing
net.ipv4.conf.all.accept_source_route = 0
net.ipv4.conf.default.accept_source_route = 0

# [SEQUENCE: 9] Security hardening
# Prevent SYN flood attacks
net.ipv4.tcp_syncookies = 1
net.ipv4.tcp_max_syn_backlog = 8192
net.ipv4.tcp_synack_retries = 2

# Ignore ICMP redirects
net.ipv4.conf.all.accept_redirects = 0
net.ipv4.conf.default.accept_redirects = 0
net.ipv4.conf.all.secure_redirects = 0
net.ipv4.conf.default.secure_redirects = 0

# Ignore ping broadcasts
net.ipv4.icmp_echo_ignore_broadcasts = 1

# [SEQUENCE: 10] Virtual memory optimizations
# Reduce swappiness (prefer RAM)
vm.swappiness = 10

# Increase dirty ratio for better I/O
vm.dirty_ratio = 40
vm.dirty_background_ratio = 10

# Memory overcommit for game server
vm.overcommit_memory = 1

# [SEQUENCE: 11] File system optimizations
# Increase file handle limits
fs.file-max = 2097152
fs.nr_open = 1048576

# Increase inotify limits
fs.inotify.max_user_watches = 524288
fs.inotify.max_user_instances = 512

# [SEQUENCE: 12] Process scheduling
# Use deadline scheduler for better latency
kernel.sched_migration_cost_ns = 5000000
kernel.sched_autogroup_enabled = 0

EOF

# [SEQUENCE: 13] Apply sysctl settings
sysctl -p

# [SEQUENCE: 14] Configure CPU governor
if command -v cpupower &> /dev/null; then
    cpupower frequency-set -g performance
else
    # Fallback method
    for gov in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
        echo performance > $gov 2>/dev/null || true
    done
fi

# [SEQUENCE: 15] Disable CPU frequency scaling
if [ -f /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
    echo 0 > /sys/devices/system/cpu/intel_pstate/no_turbo
fi

# [SEQUENCE: 16] Configure IRQ affinity
# Bind network IRQs to specific CPUs
configure_irq_affinity() {
    local interface=$1
    local cpu_list=$2
    
    if [ -z "$interface" ] || [ -z "$cpu_list" ]; then
        return
    fi
    
    # Find IRQs for network interface
    for irq in $(grep $interface /proc/interrupts | awk '{print $1}' | sed 's/://g'); do
        if [ -f /proc/irq/$irq/smp_affinity_list ]; then
            echo $cpu_list > /proc/irq/$irq/smp_affinity_list
        fi
    done
}

# Example: configure_irq_affinity eth0 "0-3"

# [SEQUENCE: 17] Enable huge pages
echo 2048 > /proc/sys/vm/nr_hugepages

# [SEQUENCE: 18] Configure transparent huge pages
if [ -f /sys/kernel/mm/transparent_hugepage/enabled ]; then
    echo always > /sys/kernel/mm/transparent_hugepage/enabled
fi

if [ -f /sys/kernel/mm/transparent_hugepage/defrag ]; then
    echo madvise > /sys/kernel/mm/transparent_hugepage/defrag
fi

# [SEQUENCE: 19] Set up limits for game server user
cat > /etc/security/limits.d/99-gameserver.conf << EOF
# Limits for game server
mmorpg soft nofile 1048576
mmorpg hard nofile 1048576
mmorpg soft nproc 65536
mmorpg hard nproc 65536
mmorpg soft memlock unlimited
mmorpg hard memlock unlimited
mmorpg soft stack 65536
mmorpg hard stack 65536
EOF

# [SEQUENCE: 20] Create systemd drop-in for better resource limits
mkdir -p /etc/systemd/system.conf.d/
cat > /etc/systemd/system.conf.d/99-gameserver.conf << EOF
[Manager]
DefaultLimitNOFILE=1048576
DefaultLimitNPROC=65536
DefaultLimitMEMLOCK=infinity
DefaultTasksMax=infinity
EOF

# [SEQUENCE: 21] Reload systemd
systemctl daemon-reload

# [SEQUENCE: 22] Create network optimization service
cat > /etc/systemd/system/gameserver-network-optimize.service << EOF
[Unit]
Description=Game Server Network Optimization
After=network.target

[Service]
Type=oneshot
ExecStart=/usr/local/bin/optimize_network_game.sh
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF

# [SEQUENCE: 23] Create the network optimization script
cat > /usr/local/bin/optimize_network_game.sh << 'EOF'
#!/bin/bash
# Network interface optimization

# Find main network interface
INTERFACE=$(ip route | grep default | awk '{print $5}' | head -n1)

if [ -n "$INTERFACE" ]; then
    # Increase ring buffer sizes
    ethtool -G $INTERFACE rx 4096 tx 4096 2>/dev/null || true
    
    # Enable offloading features
    ethtool -K $INTERFACE rx on tx on gso on tso on 2>/dev/null || true
    
    # Set interrupt coalescing for lower latency
    ethtool -C $INTERFACE rx-usecs 10 tx-usecs 10 2>/dev/null || true
fi

# Set up CPU affinity for network interrupts
# This spreads network interrupts across first 4 cores
for irq in $(grep -E "eth|ens|enp" /proc/interrupts | awk '{print $1}' | sed 's/://g'); do
    if [ -f /proc/irq/$irq/smp_affinity_list ]; then
        echo "0-3" > /proc/irq/$irq/smp_affinity_list 2>/dev/null || true
    fi
done

exit 0
EOF

chmod +x /usr/local/bin/optimize_network_game.sh

# [SEQUENCE: 24] Enable the optimization service
systemctl enable gameserver-network-optimize.service
systemctl start gameserver-network-optimize.service

echo "Kernel optimization completed!"
echo "Reboot recommended for all changes to take effect."
echo ""
echo "Optimizations applied:"
echo "- Network buffers increased"
echo "- TCP BBR congestion control enabled"
echo "- CPU governor set to performance"
echo "- Huge pages enabled"
echo "- File handle limits increased"
echo "- IRQ affinity optimizations prepared"
echo ""
echo "Note: Some settings may require reboot to fully apply."