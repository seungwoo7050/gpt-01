# MVP6: Bare Metal Deployment

This directory contains scripts and configurations for deploying the MMORPG server directly on bare metal Linux servers for maximum performance.

## Overview

Bare metal deployment provides:
- Direct hardware access
- No virtualization overhead
- Fine-tuned kernel parameters
- Custom security hardening
- Maximum network performance

## Directory Structure

```
mvp6-bare-metal/
├── deploy_bare_metal.sh      # Main deployment script
├── scripts/
│   ├── install_prerequisites.sh  # Install required packages
│   ├── optimize_kernel.sh       # Kernel performance tuning
│   └── security_hardening.sh    # Security configuration
└── configs/
    └── server.conf              # Server configuration template
```

## Quick Start

1. **Prepare the server** (Ubuntu 20.04+ or CentOS 8+ recommended):
```bash
# As root user
cd /path/to/game-server
chmod +x versions/mvp6-bare-metal/*.sh
chmod +x versions/mvp6-bare-metal/scripts/*.sh
```

2. **Install prerequisites**:
```bash
./versions/mvp6-bare-metal/scripts/install_prerequisites.sh
```

3. **Run deployment**:
```bash
./versions/mvp6-bare-metal/deploy_bare_metal.sh
```

4. **Apply optimizations** (optional but recommended):
```bash
./versions/mvp6-bare-metal/scripts/optimize_kernel.sh
./versions/mvp6-bare-metal/scripts/security_hardening.sh
```

## Features

### 1. Automated Installation
- Detects OS and installs appropriate packages
- Builds server with optimizations
- Creates systemd service
- Sets up monitoring

### 2. Performance Optimization
- CPU governor set to performance mode
- Network stack tuning for low latency
- Huge pages support
- IRQ affinity configuration
- Custom compile flags (-O3, -march=native)

### 3. Security Hardening
- Firewall rules with DDoS protection
- Fail2ban integration
- SSH hardening
- SELinux/AppArmor policies
- File integrity monitoring (AIDE)
- Audit logging

### 4. Monitoring & Management
- Health check script
- Performance metrics collection
- Automated backup system
- Log rotation
- Systemd integration

## Configuration

### Server Configuration
Edit `/etc/mmorpg-server/server.conf`:
```ini
[server]
bind_port = 7777
max_connections = 10000
worker_threads = 0  # auto-detect

[performance]
cpu_affinity = true
huge_pages = true
tcp_nodelay = true
```

### Performance Tuning
The deployment automatically applies:
- Network buffer increases
- TCP BBR congestion control
- CPU frequency scaling disabled
- Process priority optimization

### Security Settings
- Firewall allows only port 7777 (game) and 22 (SSH)
- Connection rate limiting
- Automatic IP banning for abuse
- Kernel hardening applied

## Management Commands

```bash
# Service control
systemctl start mmorpg-server
systemctl stop mmorpg-server
systemctl restart mmorpg-server
systemctl status mmorpg-server

# View logs
journalctl -u mmorpg-server -f

# Health check
/opt/mmorpg-server/bin/health_check.sh

# Collect metrics
/opt/mmorpg-server/bin/collect_metrics.sh

# Manual backup
/opt/mmorpg-server/bin/backup.sh
```

## Hardware Requirements

### Minimum (1000 players)
- CPU: 8 cores @ 3.0GHz
- RAM: 16GB DDR4
- Network: 1Gbps
- Storage: 100GB SSD

### Recommended (5000 players)
- CPU: 16 cores @ 3.5GHz
- RAM: 64GB DDR4
- Network: 10Gbps
- Storage: 500GB NVMe SSD

### Production (10000+ players)
- CPU: 32+ cores @ 4.0GHz
- RAM: 128GB+ DDR4
- Network: 10Gbps+ dedicated
- Storage: 1TB+ NVMe RAID

## Scaling Considerations

### Vertical Scaling
1. Add more CPU cores
2. Increase RAM
3. Upgrade network to 10/25/40Gbps
4. Use NVMe storage

### Horizontal Scaling
1. Deploy multiple game servers
2. Use load balancer for distribution
3. Shared Redis for session management
4. Database replication

## Troubleshooting

### Server won't start
```bash
# Check logs
journalctl -u mmorpg-server -n 100

# Verify permissions
ls -la /opt/mmorpg-server/

# Check port availability
ss -tlnp | grep 7777
```

### High CPU usage
```bash
# Check process details
top -p $(pgrep mmorpg_server)

# View thread activity
htop -H

# Profile with perf
perf top -p $(pgrep mmorpg_server)
```

### Network issues
```bash
# Check connections
ss -tan | grep :7777 | wc -l

# Monitor bandwidth
iftop -i eth0

# Check packet loss
mtr gameserver.example.com
```

## Best Practices

1. **Regular Updates**
   - Keep OS packages updated
   - Monitor security advisories
   - Schedule maintenance windows

2. **Monitoring**
   - Use Prometheus + Grafana
   - Set up alerts for high CPU/memory
   - Monitor player counts and latency

3. **Backups**
   - Automated daily backups
   - Test restore procedures
   - Off-site backup storage

4. **Security**
   - Regular security audits
   - Keep logs for analysis
   - Implement rate limiting

## Integration with CI/CD

Example deployment pipeline:
```yaml
deploy:
  stage: deploy
  script:
    - scp build/mmorpg_server user@server:/tmp/
    - ssh user@server 'sudo systemctl stop mmorpg-server'
    - ssh user@server 'sudo cp /tmp/mmorpg_server /opt/mmorpg-server/bin/'
    - ssh user@server 'sudo systemctl start mmorpg-server'
  only:
    - master
```

## Support

For issues specific to bare metal deployment:
1. Check system logs: `/var/log/mmorpg-server/`
2. Review kernel messages: `dmesg`
3. Verify network configuration
4. Ensure all prerequisites are installed