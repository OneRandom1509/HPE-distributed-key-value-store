#!/bin/bash
# Enhanced KV Store Node Launcher Script
# Supports both in-memory and persistent storage modes
# Default configuration
BUILD_DIR="../build"
SHARED_MEM_SIZE="500M"  # Default 100MB as per your C++ code
PERSISTENCE_MODE=""
SERVER_PORT="8080"

# Color codes for better output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to display usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -d PATH     Build directory path (default: ../build)"
    echo "  -m SIZE     Memory size with suffix K/M/G (default: 100M)"
    echo "  -p PORT     Server port (default: 8080)"
    echo "  -s MODE     Storage mode: 'memory' or 'persistent'"
    echo "  -h          Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                           # Interactive mode with defaults"
    echo "  $0 -d ./build -m 500M        # Custom build dir and memory"
    echo "  $0 -s persistent -m 1G        # Persistent mode with 1GB memory"
    echo "  $0 -s memory -p 9090          # Memory mode on port 9090"
}

# Parse command line arguments
while getopts ":d:m:p:s:h" opt; do
    case $opt in
        d) BUILD_DIR="$OPTARG" ;;
        m) SHARED_MEM_SIZE="$OPTARG" ;;
        p) SERVER_PORT="$OPTARG" ;;
        s)
            if [[ "$OPTARG" == "memory" || "$OPTARG" == "persistent" ]]; then
                PERSISTENCE_MODE="$OPTARG"
            else
                print_error "Invalid storage mode: $OPTARG. Use 'memory' or 'persistent'"
                exit 1
            fi
            ;;
        h) show_usage; exit 0 ;;
        \?) print_error "Invalid option: -$OPTARG"; show_usage; exit 1 ;;
        :) print_error "Option -$OPTARG requires an argument."; show_usage; exit 1 ;;
    esac
done

# Validate memory size format
if [[ ! "$SHARED_MEM_SIZE" =~ ^[0-9]+[KMG]?$ ]]; then
    print_error "Invalid memory size format: $SHARED_MEM_SIZE"
    print_info "Use format like: 100M, 1G, 500K, or plain numbers for bytes"
    exit 1
fi

# Check for server binary
if [ ! -f "$BUILD_DIR/kvm_server" ]; then
    print_error "kvm_server executable not found in $BUILD_DIR"
    print_info "Please ensure the project is built and the path is correct"
    show_usage
    exit 1
fi

# Make the binary executable if it isn't
if [ ! -x "$BUILD_DIR/kvm_server" ]; then
    print_warning "Making kvm_server executable..."
    chmod +x "$BUILD_DIR/kvm_server"
fi

# Interactive storage mode selection if not specified
if [ -z "$PERSISTENCE_MODE" ]; then
    echo ""
    print_info "=== KV Store Configuration ==="
    echo ""
    echo "Choose storage mode:"
    echo ""
    echo "  1. In-Memory Storage"
    echo "     • Faster performance"
    echo "     • Data lost on restart"
    echo "     • Uses shared memory"
    echo "     • Temporary files (.dat/.txt) cleaned on exit"
    echo ""
    echo "  2. Persistent Storage"
    echo "     • Data survives restarts"
    echo "     • Slightly slower performance"
    echo "     • Uses memory-mapped files"
    echo "     • File location: ./kvstore_persistent.dat"
    echo ""
    while true; do
        read -p "Enter your choice (1 or 2): " choice
        case $choice in
            1)
                PERSISTENCE_MODE="memory"
                print_success "Selected: In-Memory Storage"
                break
                ;;
            2)
                PERSISTENCE_MODE="persistent"
                print_success "Selected: Persistent Storage"
                # Check if persistent file already exists
                if [ -f "./kvstore_persistent.dat" ]; then
                    print_info "Found existing persistent storage file"
                    print_warning "Existing data will be loaded on startup"
                fi
                break
                ;;
            *)
                print_error "Invalid choice. Please enter 1 or 2."
                ;;
        esac
    done
fi

# Set FI_PROVIDER to avoid network fabric ambiguity
export FI_PROVIDER=tcp

# Get system information
HOSTNAME=$(hostname)
HOST_IP=$(hostname -I | awk '{print $1}')

# Check if port is already in use
if command -v netstat >/dev/null 2>&1; then
    if netstat -tuln | grep -q ":$SERVER_PORT "; then
        print_error "Port $SERVER_PORT is already in use"
        print_info "Please choose a different port with -p option"
        exit 1
    fi
fi

# Display configuration summary
echo ""
print_info "=== Starting KV Store Server ==="
echo ""
echo "  Host:           $HOSTNAME ($HOST_IP)"
echo "  Port:           $SERVER_PORT"
echo "  Memory Size:    $SHARED_MEM_SIZE"
echo "  Storage Mode:   $PERSISTENCE_MODE"
echo "  Build Dir:      $BUILD_DIR"
if [ "$PERSISTENCE_MODE" = "persistent" ]; then
    echo "  Data File:      ./kvstore_persistent.dat"
else
    echo "  Cleanup:        .dat and .txt files will be removed on exit"
fi
echo ""

# Clean up any leftover shared memory segments (for safety)
if [ "$PERSISTENCE_MODE" = "memory" ]; then
    print_info "Cleaning up any existing shared memory segments..."
    # This helps avoid conflicts from previous runs
    ipcs -m | grep $(id -u) | awk '{print $2}' | xargs -r ipcrm -m 2>/dev/null || true
fi

# Start the KV server
print_info "Launching server..."
echo ""

# Create a trap to handle cleanup on script termination
cleanup() {
    if [ ! -z "$SERVER_PID" ] && kill -0 $SERVER_PID 2>/dev/null; then
        print_info "Shutting down server (PID: $SERVER_PID)..."
        kill $SERVER_PID
        wait $SERVER_PID 2>/dev/null
        print_success "Server stopped gracefully"
    fi
    
    if [ "$PERSISTENCE_MODE" = "memory" ]; then
        print_info "Cleaning up shared memory segments..."
        ipcs -m | grep $(id -u) | awk '{print $2}' | xargs -r ipcrm -m 2>/dev/null || true
        
        # Clean up .dat and .txt files in memory mode
        print_info "Removing temporary data files (.dat and .txt)..."
        
        # Count files before removal for feedback
        dat_count=$(find . -maxdepth 1 -name "*.dat" -type f 2>/dev/null | wc -l)
        txt_count=$(find . -maxdepth 1 -name "*.txt" -type f 2>/dev/null | wc -l)
        
        if [ $dat_count -gt 0 ] || [ $txt_count -gt 0 ]; then
            # Remove .dat files
            if [ $dat_count -gt 0 ]; then
                find . -maxdepth 1 -name "*.dat" -type f -delete 2>/dev/null
                print_success "Removed $dat_count .dat file(s)"
            fi
            
            # Remove .txt files
            if [ $txt_count -gt 0 ]; then
                find . -maxdepth 1 -name "*.txt" -type f -delete 2>/dev/null
                print_success "Removed $txt_count .txt file(s)"
            fi
        else
            print_info "No temporary data files found to clean up"
        fi
    fi
}

trap cleanup EXIT INT TERM

# Launch the server with the specified parameters
if [ "$PERSISTENCE_MODE" = "persistent" ]; then
    "$BUILD_DIR/kvm_server" ofi+tcp $SERVER_PORT $SHARED_MEM_SIZE persistent &
else
    "$BUILD_DIR/kvm_server" ofi+tcp $SERVER_PORT $SHARED_MEM_SIZE memory &
fi

SERVER_PID=$!

# Wait a moment to see if the server started successfully
sleep 2


if kill -0 $SERVER_PID 2>/dev/null; then
    print_success "Server started successfully!"
    echo ""
    echo "  Server PID:     $SERVER_PID"
    echo "  Endpoint:       ofi+tcp://$HOST_IP:$SERVER_PORT"
    echo "  Status:         Running"
    echo ""
    print_info "Server is now ready to accept connections"
    if [ "$PERSISTENCE_MODE" = "persistent" ]; then
        print_info "Data will be automatically saved to disk"
        print_info "Data will persist across server restarts"
    else
        print_warning "Data will be lost when server stops"
        print_warning "Temporary .dat and .txt files will be cleaned up on exit"
    fi
    echo ""
    print_info "Press Ctrl+C to stop the server"
    echo ""
    
    # Wait for the server process to finish
    wait $SERVER_PID
else
    print_error "Failed to start server"
    print_info "Check the error messages above for details"
    exit 1
fi
