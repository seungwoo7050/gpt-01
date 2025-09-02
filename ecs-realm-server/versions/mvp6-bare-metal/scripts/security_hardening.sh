#!/bin/bash
# [SEQUENCE: 1] Security hardening for bare metal game server

set -e

# [SEQUENCE: 2] Check root
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 
   exit 1
fi

# [SEQUENCE: 3] Install security tools
install_security_tools() {
    echo "Installing security tools..."
    
    if command -v apt-get &> /dev/null; then
        apt-get update
        apt-get install -y \
            fail2ban \
            iptables-persistent \
            auditd \
            aide \
            rkhunter \
            lynis
    elif command -v yum &> /dev/null; then
        yum install -y \
            fail2ban \
            iptables-services \
            audit \
            aide \
            rkhunter \
            lynis
    fi
}

# [SEQUENCE: 4] Configure firewall rules
configure_firewall() {
    echo "Configuring firewall..."
    
    # Flush existing rules
    iptables -F
    iptables -X
    
    # Default policies
    iptables -P INPUT DROP
    iptables -P FORWARD DROP
    iptables -P OUTPUT ACCEPT
    
    # Allow loopback
    iptables -A INPUT -i lo -j ACCEPT
    
    # Allow established connections
    iptables -A INPUT -m state --state ESTABLISHED,RELATED -j ACCEPT
    
    # Allow SSH (adjust port as needed)
    iptables -A INPUT -p tcp --dport 22 -m state --state NEW -m limit --limit 3/min --limit-burst 3 -j ACCEPT
    
    # Allow game server port
    iptables -A INPUT -p tcp --dport 7777 -m state --state NEW -j ACCEPT
    
    # DDoS protection for game port
    iptables -A INPUT -p tcp --dport 7777 -m connlimit --connlimit-above 3 --connlimit-mask 32 -j REJECT
    iptables -A INPUT -p tcp --dport 7777 -m limit --limit 100/second --limit-burst 150 -j ACCEPT
    
    # Drop invalid packets
    iptables -A INPUT -m state --state INVALID -j DROP
    
    # Protection against port scanning
    iptables -N PORT_SCAN
    iptables -A PORT_SCAN -j LOG --log-prefix "Port Scan: " --log-level 4
    iptables -A PORT_SCAN -j DROP
    
    # Save rules
    if command -v netfilter-persistent &> /dev/null; then
        netfilter-persistent save
    elif [ -f /etc/sysconfig/iptables ]; then
        service iptables save
    fi
}

# [SEQUENCE: 5] Configure fail2ban for game server
configure_fail2ban() {
    echo "Configuring fail2ban..."
    
    # Create game server jail
    cat > /etc/fail2ban/jail.d/gameserver.conf << 'EOF'
[gameserver-auth]
enabled = true
port = 7777
filter = gameserver-auth
logpath = /var/log/mmorpg/auth.log
maxretry = 5
findtime = 300
bantime = 3600
action = iptables[name=GameServer, port=7777, protocol=tcp]

[gameserver-ddos]
enabled = true
port = 7777
filter = gameserver-ddos
logpath = /var/log/mmorpg/server.log
maxretry = 100
findtime = 60
bantime = 600
action = iptables[name=GameServerDDoS, port=7777, protocol=tcp]
EOF

    # Create filters
    cat > /etc/fail2ban/filter.d/gameserver-auth.conf << 'EOF'
[Definition]
failregex = Failed login attempt from <HOST>
            Invalid credentials from <HOST>
            Authentication failed for .* from <HOST>
ignoreregex =
EOF

    cat > /etc/fail2ban/filter.d/gameserver-ddos.conf << 'EOF'
[Definition]
failregex = Connection flood detected from <HOST>
            Excessive requests from <HOST>
ignoreregex =
EOF

    # Restart fail2ban
    systemctl restart fail2ban
    systemctl enable fail2ban
}

# [SEQUENCE: 6] Harden SSH
harden_ssh() {
    echo "Hardening SSH configuration..."
    
    # Backup original config
    cp /etc/ssh/sshd_config /etc/ssh/sshd_config.backup
    
    # Apply hardening
    cat >> /etc/ssh/sshd_config << 'EOF'

# Security hardening
Protocol 2
PermitRootLogin no
PasswordAuthentication no
PubkeyAuthentication yes
PermitEmptyPasswords no
MaxAuthTries 3
ClientAliveInterval 300
ClientAliveCountMax 2
X11Forwarding no
AllowUsers mmorpg admin
LoginGraceTime 60
StrictModes yes
EOF

    # Restart SSH
    systemctl restart sshd
}

# [SEQUENCE: 7] Configure SELinux/AppArmor
configure_mac() {
    echo "Configuring Mandatory Access Control..."
    
    if command -v getenforce &> /dev/null; then
        # SELinux
        semanage port -a -t game_port_t -p tcp 7777 2>/dev/null || true
        
        # Create custom policy
        cat > /tmp/gameserver.te << 'EOF'
module gameserver 1.0;

require {
    type init_t;
    type game_port_t;
    class tcp_socket { bind listen accept };
}

#============= gameserver_t ==============
allow gameserver_t game_port_t:tcp_socket { bind listen accept };
EOF
        
        checkmodule -M -m -o /tmp/gameserver.mod /tmp/gameserver.te 2>/dev/null || true
        semodule_package -o /tmp/gameserver.pp -m /tmp/gameserver.mod 2>/dev/null || true
        semodule -i /tmp/gameserver.pp 2>/dev/null || true
        
    elif command -v aa-status &> /dev/null; then
        # AppArmor
        cat > /etc/apparmor.d/opt.mmorpg-server.bin.mmorpg_server << 'EOF'
#include <tunables/global>

/opt/mmorpg-server/bin/mmorpg_server {
  #include <abstractions/base>
  #include <abstractions/nameservice>
  
  capability net_bind_service,
  capability setuid,
  capability setgid,
  
  network tcp,
  
  /opt/mmorpg-server/bin/mmorpg_server mr,
  /opt/mmorpg-server/lib/** mr,
  /etc/mmorpg-server/** r,
  /var/log/mmorpg-server/** w,
  /var/lib/mmorpg-server/** rw,
  /proc/sys/kernel/random/uuid r,
  /dev/urandom r,
}
EOF
        
        apparmor_parser -r /etc/apparmor.d/opt.mmorpg-server.bin.mmorpg_server || true
    fi
}

# [SEQUENCE: 8] Setup audit logging
configure_audit() {
    echo "Configuring audit logging..."
    
    # Add audit rules
    cat >> /etc/audit/rules.d/gameserver.rules << 'EOF'
# Monitor game server binary
-w /opt/mmorpg-server/bin/mmorpg_server -p x -k gameserver_exec

# Monitor configuration changes
-w /etc/mmorpg-server/ -p wa -k gameserver_config

# Monitor game data
-w /var/lib/mmorpg-server/ -p wa -k gameserver_data

# Network connections
-a always,exit -F arch=b64 -S socket -F a0=2 -F success=1 -k gameserver_socket

# Monitor privileged commands
-a always,exit -F path=/opt/mmorpg-server/bin/mmorpg_server -F perm=x -F auid>=1000 -F auid!=4294967295 -k privileged_gameserver
EOF

    # Restart auditd
    service auditd restart
}

# [SEQUENCE: 9] Setup file integrity monitoring
configure_aide() {
    echo "Setting up file integrity monitoring..."
    
    # Configure AIDE
    cat >> /etc/aide/aide.conf << 'EOF'

# Game server monitoring
/opt/mmorpg-server/bin Binary
/etc/mmorpg-server Config
/var/lib/mmorpg-server/config Config

Binary = p+i+n+u+g+s+b+m+c+md5+sha256
Config = p+i+n+u+g+md5
EOF

    # Initialize AIDE database
    aide --init
    mv /var/lib/aide/aide.db.new /var/lib/aide/aide.db
}

# [SEQUENCE: 10] Create security monitoring script
create_security_monitor() {
    cat > /opt/mmorpg-server/bin/security_monitor.sh << 'EOF'
#!/bin/bash
# [SEQUENCE: 11] Security monitoring for game server

LOG_FILE="/var/log/mmorpg-server/security.log"

log_security() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" >> $LOG_FILE
}

# Check for suspicious processes
check_processes() {
    # Look for port scanners
    if pgrep -f "nmap|masscan|zmap" > /dev/null; then
        log_security "WARNING: Port scanner detected"
    fi
    
    # Check for debuggers attached to game server
    if grep -q "TracerPid:[[:space:]]*[1-9]" /proc/$(pgrep mmorpg_server)/status 2>/dev/null; then
        log_security "CRITICAL: Debugger attached to game server"
    fi
}

# Check for unauthorized file changes
check_file_integrity() {
    aide --check | grep -E "changed|added|removed" | while read line; do
        log_security "File integrity: $line"
    done
}

# Check failed login attempts
check_failed_logins() {
    FAILED_COUNT=$(grep "Failed login" /var/log/mmorpg-server/auth.log | wc -l)
    if [ $FAILED_COUNT -gt 100 ]; then
        log_security "WARNING: High number of failed logins: $FAILED_COUNT"
    fi
}

# Check network connections
check_connections() {
    # Count connections per IP
    ss -tn state established '( dport = :7777 )' | awk '{print $4}' | cut -d: -f1 | sort | uniq -c | while read count ip; do
        if [ $count -gt 10 ]; then
            log_security "WARNING: IP $ip has $count connections"
        fi
    done
}

# Main monitoring loop
check_processes
check_file_integrity
check_failed_logins
check_connections

# Run rootkit hunter
rkhunter --check --skip-keypress --report-warnings-only | while read line; do
    log_security "RKHunter: $line"
done
EOF

    chmod +x /opt/mmorpg-server/bin/security_monitor.sh
    
    # Add to crontab
    echo "*/5 * * * * /opt/mmorpg-server/bin/security_monitor.sh" | crontab -
}

# [SEQUENCE: 12] Kernel hardening
kernel_hardening() {
    echo "Applying kernel hardening..."
    
    cat >> /etc/sysctl.d/99-security.conf << 'EOF'
# Kernel hardening
kernel.dmesg_restrict = 1
kernel.kptr_restrict = 2
kernel.yama.ptrace_scope = 1
kernel.unprivileged_bpf_disabled = 1
net.core.bpf_jit_harden = 2

# Prevent kernel module loading
# kernel.modules_disabled = 1  # Uncomment after all modules loaded

# Hide kernel pointers
kernel.kptr_restrict = 2

# Restrict access to kernel logs
kernel.dmesg_restrict = 1

# Disable magic SysRq key
kernel.sysrq = 0

# Execshield
kernel.exec-shield = 1
kernel.randomize_va_space = 2

# Restrict core dumps
fs.suid_dumpable = 0

# Disable user namespaces for non-root
user.max_user_namespaces = 0
EOF

    sysctl -p /etc/sysctl.d/99-security.conf
}

# [SEQUENCE: 13] Main execution
main() {
    echo "Starting security hardening..."
    
    install_security_tools
    configure_firewall
    configure_fail2ban
    harden_ssh
    configure_mac
    configure_audit
    configure_aide
    create_security_monitor
    kernel_hardening
    
    echo "Security hardening completed!"
    echo "Remember to:"
    echo "1. Configure SSH keys for allowed users"
    echo "2. Review and adjust firewall rules"
    echo "3. Set up log monitoring/SIEM"
    echo "4. Regular security updates"
    echo "5. Implement application-level anti-cheat"
}

main "$@"