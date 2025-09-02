#!/bin/bash
# [SEQUENCE: 1] Install prerequisites for different Linux distributions

set -e

# [SEQUENCE: 2] Detect OS
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
    VER=$VERSION_ID
else
    echo "Cannot detect OS"
    exit 1
fi

# [SEQUENCE: 3] Install for Ubuntu/Debian
install_ubuntu() {
    echo "Installing prerequisites for Ubuntu/Debian..."
    
    # Update package list
    apt-get update
    
    # Development tools
    apt-get install -y \
        build-essential \
        cmake \
        ninja-build \
        git \
        pkg-config \
        ccache
    
    # C++ libraries
    apt-get install -y \
        libboost-all-dev \
        libprotobuf-dev \
        protobuf-compiler \
        libspdlog-dev \
        libtbb-dev \
        libssl-dev \
        libcurl4-openssl-dev
    
    # System tools
    apt-get install -y \
        htop \
        iotop \
        sysstat \
        net-tools \
        dstat \
        bc
    
    # Performance tools
    apt-get install -y \
        linux-tools-common \
        linux-tools-generic \
        linux-tools-$(uname -r) \
        perf-tools-unstable
}

# [SEQUENCE: 4] Install for CentOS/RHEL/Rocky
install_rhel() {
    echo "Installing prerequisites for RHEL-based systems..."
    
    # Enable EPEL
    yum install -y epel-release
    
    # Development tools
    yum groupinstall -y "Development Tools"
    yum install -y \
        cmake3 \
        ninja-build \
        git \
        pkgconfig \
        ccache
    
    # C++ libraries
    yum install -y \
        boost-devel \
        protobuf-devel \
        protobuf-compiler \
        spdlog-devel \
        tbb-devel \
        openssl-devel \
        libcurl-devel
    
    # System tools
    yum install -y \
        htop \
        iotop \
        sysstat \
        net-tools \
        dstat \
        bc
    
    # Performance tools
    yum install -y \
        perf \
        kernel-tools
}

# [SEQUENCE: 5] Install for Fedora
install_fedora() {
    echo "Installing prerequisites for Fedora..."
    
    # Development tools
    dnf groupinstall -y "Development Tools"
    dnf install -y \
        cmake \
        ninja-build \
        git \
        pkgconfig \
        ccache
    
    # C++ libraries
    dnf install -y \
        boost-devel \
        protobuf-devel \
        protobuf-compiler \
        spdlog-devel \
        tbb-devel \
        openssl-devel \
        libcurl-devel
    
    # System tools
    dnf install -y \
        htop \
        iotop \
        sysstat \
        net-tools \
        dstat \
        bc
    
    # Performance tools
    dnf install -y \
        perf \
        kernel-tools
}

# [SEQUENCE: 6] Install for Arch Linux
install_arch() {
    echo "Installing prerequisites for Arch Linux..."
    
    # Update system
    pacman -Syu --noconfirm
    
    # Development tools
    pacman -S --noconfirm \
        base-devel \
        cmake \
        ninja \
        git \
        pkg-config \
        ccache
    
    # C++ libraries
    pacman -S --noconfirm \
        boost \
        protobuf \
        spdlog \
        intel-tbb \
        openssl \
        curl
    
    # System tools
    pacman -S --noconfirm \
        htop \
        iotop \
        sysstat \
        net-tools \
        dstat \
        bc
    
    # Performance tools
    pacman -S --noconfirm \
        perf \
        linux-tools
}

# [SEQUENCE: 7] Main logic
case $OS in
    ubuntu|debian)
        install_ubuntu
        ;;
    centos|rhel|rocky|almalinux)
        install_rhel
        ;;
    fedora)
        install_fedora
        ;;
    arch|manjaro)
        install_arch
        ;;
    *)
        echo "Unsupported OS: $OS"
        echo "Please install the following manually:"
        echo "- CMake 3.16+"
        echo "- GCC 9+ or Clang 10+"
        echo "- Boost 1.70+"
        echo "- Protocol Buffers"
        echo "- spdlog"
        echo "- Intel TBB"
        echo "- OpenSSL"
        exit 1
        ;;
esac

echo "Prerequisites installed successfully!"