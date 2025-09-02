#!/bin/bash
# [SEQUENCE: 1] Bare metal deployment script for C++ MMO Server
# Provides direct server deployment without containerization for maximum performance

set -e

# [SEQUENCE: 2] Configuration variables
SERVER_NAME="mmorpg-server"
INSTALL_DIR="/opt/${SERVER_NAME}"
CONFIG_DIR="/etc/${SERVER_NAME}"
LOG_DIR="/var/log/${SERVER_NAME}"
DATA_DIR="/var/lib/${SERVER_NAME}"
SYSTEMD_DIR="/etc/systemd/system"
BUILD_DIR="./build"

# [SEQUENCE: 3] Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# [SEQUENCE: 4] Helper functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# [SEQUENCE: 5] Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    # Check if running as root
    if [[ $EUID -ne 0 ]]; then
        log_error "This script must be run as root"
        exit 1
    fi
    
    # Check required tools
    local required_tools=("cmake" "make" "g++" "systemctl")
    for tool in "${required_tools[@]}"; do
        if ! command -v $tool &> /dev/null; then
            log_error "$tool is required but not installed"
            exit 1
        fi
    done
    
    # Check OS
    if [[ ! -f /etc/os-release ]]; then
        log_error "Cannot determine OS version"
        exit 1
    fi
    
    source /etc/os-release
    log_info "Detected OS: $NAME $VERSION"
    
    # Check architecture
    ARCH=$(uname -m)
    log_info "Architecture: $ARCH"
}

# [SEQUENCE: 6] Install system dependencies
install_dependencies() {
    log_info "Installing system dependencies..."
    
    if [[ "$ID" == "ubuntu" ]] || [[ "$ID" == "debian" ]]; then
        apt-get update
        apt-get install -y \
            build-essential \
            cmake \
            libboost-all-dev \
            libprotobuf-dev \
            protobuf-compiler \
            libspdlog-dev \
            libtbb-dev \
            libssl-dev \
            pkg-config
    elif [[ "$ID" == "centos" ]] || [[ "$ID" == "rhel" ]] || [[ "$ID" == "fedora" ]]; then
        yum install -y \
            gcc-c++ \
            cmake3 \
            boost-devel \
            protobuf-devel \
            protobuf-compiler \
            spdlog-devel \
            tbb-devel \
            openssl-devel \
            pkgconfig
    else
        log_error "Unsupported OS: $ID"
        exit 1
    fi
}

# [SEQUENCE: 7] Build the server
build_server() {
    log_info "Building server..."
    
    # Create build directory
    mkdir -p $BUILD_DIR
    cd $BUILD_DIR
    
    # Configure with optimizations
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native" \
        -DENABLE_PROFILING=OFF \
        -DENABLE_SANITIZERS=OFF
    
    # Build with all cores
    make -j$(nproc)
    
    cd ..
    
    log_info "Build completed successfully"
}

# [SEQUENCE: 8] Create directory structure
create_directories() {
    log_info "Creating directory structure..."
    
    mkdir -p $INSTALL_DIR/bin
    mkdir -p $INSTALL_DIR/lib
    mkdir -p $CONFIG_DIR
    mkdir -p $LOG_DIR
    mkdir -p $DATA_DIR
    
    # Set permissions
    chmod 755 $INSTALL_DIR
    chmod 755 $CONFIG_DIR
    chmod 755 $LOG_DIR
    chmod 755 $DATA_DIR
}

# [SEQUENCE: 9] Install server files
install_server() {
    log_info "Installing server files..."
    
    # Copy binary
    cp $BUILD_DIR/src/server/mmorpg_server $INSTALL_DIR/bin/
    chmod +x $INSTALL_DIR/bin/mmorpg_server
    
    # Copy libraries if any
    if [ -d "$BUILD_DIR/lib" ]; then
        cp -r $BUILD_DIR/lib/* $INSTALL_DIR/lib/
    fi
    
    # Set library path
    echo "$INSTALL_DIR/lib" > /etc/ld.so.conf.d/${SERVER_NAME}.conf
    ldconfig
}

# [SEQUENCE: 10] Install configuration files
install_configs() {
    log_info "Installing configuration files..."
    
    # Create main config
    cat > $CONFIG_DIR/server.conf << EOF
# [SEQUENCE: 11] Server configuration
[server]
name = "MMORPG Production Server"
bind_address = "0.0.0.0"
bind_port = 7777
max_connections = 10000
worker_threads = 0  # 0 = auto-detect CPU cores

[performance]
# CPU affinity - bind to specific cores
cpu_affinity = true
cpu_cores = "0-7"  # Use first 8 cores

# Memory settings
huge_pages = true
memory_pool_size = 4096  # MB
socket_buffer_size = 65536

# Network optimizations
tcp_nodelay = true
tcp_quickack = true
socket_reuse = true

[game]
tick_rate = 30
world_size = 10000
max_entities = 100000
spatial_grid_size = 100

[database]
type = "postgresql"
host = "localhost"
port = 5432
name = "mmorpg"
user = "mmorpg_user"
max_connections = 100

[logging]
level = "info"
file = "$LOG_DIR/server.log"
max_size = 100  # MB
max_files = 10
console = false
EOF
    
    # Create performance tuning script
    cat > $CONFIG_DIR/tune_performance.sh << 'EOF'
#!/bin/bash
# [SEQUENCE: 12] System performance tuning for game server

# Increase file descriptors
ulimit -n 1000000

# Network tuning
sysctl -w net.core.rmem_max=134217728
sysctl -w net.core.wmem_max=134217728
sysctl -w net.core.netdev_max_backlog=5000
sysctl -w net.ipv4.tcp_rmem="4096 87380 134217728"
sysctl -w net.ipv4.tcp_wmem="4096 65536 134217728"
sysctl -w net.ipv4.tcp_congestion_control=bbr
sysctl -w net.ipv4.tcp_fastopen=3

# CPU performance
cpupower frequency-set -g performance

# Disable CPU frequency scaling
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo performance > $cpu
done

# Enable huge pages
echo 2048 > /proc/sys/vm/nr_hugepages

# Process scheduling
echo -1000 > /proc/$(pidof mmorpg_server)/oom_score_adj

echo "Performance tuning applied"
EOF
    
    chmod +x $CONFIG_DIR/tune_performance.sh
}

# [SEQUENCE: 13] Create systemd service
create_systemd_service() {
    log_info "Creating systemd service..."
    
    cat > $SYSTEMD_DIR/${SERVER_NAME}.service << EOF
[Unit]
Description=C++ MMORPG Game Server
After=network.target postgresql.service
Wants=postgresql.service

[Service]
Type=notify
ExecStartPre=$CONFIG_DIR/tune_performance.sh
ExecStart=$INSTALL_DIR/bin/mmorpg_server --config $CONFIG_DIR/server.conf
ExecReload=/bin/kill -HUP \$MAINPID
KillMode=mixed
KillSignal=SIGTERM
Restart=on-failure
RestartSec=5s

# Security
User=mmorpg
Group=mmorpg
NoNewPrivileges=true
PrivateTmp=true

# Resource limits
LimitNOFILE=1000000
LimitNPROC=64000
LimitCORE=infinity
TasksMax=infinity

# CPU affinity
CPUAffinity=0-7

# Memory
MemoryMax=8G
MemoryHigh=6G

# IO
IOWeight=1000

# Environment
Environment="LD_LIBRARY_PATH=$INSTALL_DIR/lib"
WorkingDirectory=$DATA_DIR

[Install]
WantedBy=multi-user.target
EOF
    
    systemctl daemon-reload
}

# [SEQUENCE: 14] Create monitoring scripts
create_monitoring() {
    log_info "Creating monitoring scripts..."
    
    # Health check script
    cat > $INSTALL_DIR/bin/health_check.sh << 'EOF'
#!/bin/bash
# [SEQUENCE: 15] Health check for game server

SERVER_HOST="localhost"
SERVER_PORT="7777"
TIMEOUT=5

# Check if process is running
if ! pgrep -x "mmorpg_server" > /dev/null; then
    echo "CRITICAL: Server process not running"
    exit 2
fi

# Check CPU usage
CPU_USAGE=$(ps aux | grep mmorpg_server | grep -v grep | awk '{print $3}')
if (( $(echo "$CPU_USAGE > 90" | bc -l) )); then
    echo "WARNING: High CPU usage: ${CPU_USAGE}%"
    exit 1
fi

# Check memory usage
MEM_USAGE=$(ps aux | grep mmorpg_server | grep -v grep | awk '{print $4}')
if (( $(echo "$MEM_USAGE > 80" | bc -l) )); then
    echo "WARNING: High memory usage: ${MEM_USAGE}%"
    exit 1
fi

# Check port connectivity
if ! timeout $TIMEOUT bash -c "</dev/tcp/$SERVER_HOST/$SERVER_PORT"; then
    echo "CRITICAL: Cannot connect to server port"
    exit 2
fi

# Check active connections
CONNECTIONS=$(ss -tan | grep ":$SERVER_PORT" | grep ESTAB | wc -l)
echo "OK: Server running with $CONNECTIONS active connections"
exit 0
EOF
    
    chmod +x $INSTALL_DIR/bin/health_check.sh
    
    # Performance metrics script
    cat > $INSTALL_DIR/bin/collect_metrics.sh << 'EOF'
#!/bin/bash
# [SEQUENCE: 16] Collect performance metrics

PID=$(pgrep -x "mmorpg_server")
if [ -z "$PID" ]; then
    echo "Server not running"
    exit 1
fi

# CPU stats
CPU_STATS=$(ps -p $PID -o %cpu,cputime --no-headers)

# Memory stats
MEM_STATS=$(ps -p $PID -o rss,vsz,%mem --no-headers)

# Network stats
NET_STATS=$(ss -tan | grep ":7777" | awk '{print $1}' | sort | uniq -c)

# Thread count
THREADS=$(ps -p $PID -o nlwp --no-headers)

# File descriptors
FDS=$(ls /proc/$PID/fd | wc -l)

echo "=== Server Metrics ==="
echo "CPU: $CPU_STATS"
echo "Memory (RSS VSZ %): $MEM_STATS"
echo "Threads: $THREADS"
echo "File Descriptors: $FDS"
echo "Network Connections:"
echo "$NET_STATS"
EOF
    
    chmod +x $INSTALL_DIR/bin/collect_metrics.sh
}

# [SEQUENCE: 17] Create backup script
create_backup_script() {
    log_info "Creating backup script..."
    
    cat > $INSTALL_DIR/bin/backup.sh << EOF
#!/bin/bash
# [SEQUENCE: 18] Backup game server data

BACKUP_DIR="/backup/${SERVER_NAME}"
DATE=\$(date +%Y%m%d_%H%M%S)
BACKUP_FILE="\$BACKUP_DIR/backup_\$DATE.tar.gz"

# Create backup directory
mkdir -p \$BACKUP_DIR

# Stop writes (if supported)
echo "FLUSH TABLES WITH READ LOCK;" | psql -U mmorpg_user mmorpg

# Backup data directory
tar -czf \$BACKUP_FILE -C $DATA_DIR .

# Backup database
pg_dump -U mmorpg_user mmorpg | gzip > "\$BACKUP_DIR/db_\$DATE.sql.gz"

# Resume writes
echo "UNLOCK TABLES;" | psql -U mmorpg_user mmorpg

# Keep only last 7 days
find \$BACKUP_DIR -name "backup_*.tar.gz" -mtime +7 -delete
find \$BACKUP_DIR -name "db_*.sql.gz" -mtime +7 -delete

echo "Backup completed: \$BACKUP_FILE"
EOF
    
    chmod +x $INSTALL_DIR/bin/backup.sh
}

# [SEQUENCE: 19] Create user and set permissions
create_user() {
    log_info "Creating service user..."
    
    # Create user if not exists
    if ! id "mmorpg" &>/dev/null; then
        useradd -r -s /bin/false -d $DATA_DIR mmorpg
    fi
    
    # Set ownership
    chown -R mmorpg:mmorpg $INSTALL_DIR
    chown -R mmorpg:mmorpg $CONFIG_DIR
    chown -R mmorpg:mmorpg $LOG_DIR
    chown -R mmorpg:mmorpg $DATA_DIR
}

# [SEQUENCE: 20] Configure firewall
configure_firewall() {
    log_info "Configuring firewall..."
    
    if command -v firewall-cmd &> /dev/null; then
        firewall-cmd --permanent --add-port=7777/tcp
        firewall-cmd --reload
    elif command -v ufw &> /dev/null; then
        ufw allow 7777/tcp
    else
        log_warn "No firewall detected, please manually open port 7777"
    fi
}

# [SEQUENCE: 21] Setup log rotation
setup_logrotate() {
    log_info "Setting up log rotation..."
    
    cat > /etc/logrotate.d/${SERVER_NAME} << EOF
$LOG_DIR/*.log {
    daily
    missingok
    rotate 14
    compress
    delaycompress
    notifempty
    create 0640 mmorpg mmorpg
    sharedscripts
    postrotate
        systemctl reload ${SERVER_NAME} > /dev/null 2>&1 || true
    endscript
}
EOF
}

# [SEQUENCE: 22] Main installation flow
main() {
    log_info "Starting bare metal deployment..."
    
    check_prerequisites
    install_dependencies
    build_server
    create_directories
    install_server
    install_configs
    create_systemd_service
    create_monitoring
    create_backup_script
    create_user
    configure_firewall
    setup_logrotate
    
    # Enable and start service
    systemctl enable ${SERVER_NAME}
    systemctl start ${SERVER_NAME}
    
    # Check status
    sleep 3
    if systemctl is-active --quiet ${SERVER_NAME}; then
        log_info "Server successfully deployed and running!"
        log_info "Check status: systemctl status ${SERVER_NAME}"
        log_info "View logs: journalctl -u ${SERVER_NAME} -f"
        log_info "Health check: $INSTALL_DIR/bin/health_check.sh"
    else
        log_error "Server failed to start. Check logs: journalctl -u ${SERVER_NAME} -n 50"
        exit 1
    fi
}

# Run main function
main "$@"