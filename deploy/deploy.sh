#!/bin/bash
# PACS System deployment script

# Exit on error
set -e

# Default values
USE_TLS=false
DATA_DIR="./data"
CONFIG_DIR="./config"
LOGS_DIR="./logs"
PORT=11112

# Parse command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --tls)
      USE_TLS=true
      shift
      ;;
    --data-dir)
      DATA_DIR="$2"
      shift 2
      ;;
    --config-dir)
      CONFIG_DIR="$2"
      shift 2
      ;;
    --logs-dir)
      LOGS_DIR="$2"
      shift 2
      ;;
    --port)
      PORT="$2"
      shift 2
      ;;
    --help)
      echo "PACS System deployment script"
      echo "Usage: $0 [options]"
      echo "Options:"
      echo "  --tls               Enable TLS encryption"
      echo "  --data-dir DIR      Set data directory (default: ./data)"
      echo "  --config-dir DIR    Set config directory (default: ./config)"
      echo "  --logs-dir DIR      Set logs directory (default: ./logs)"
      echo "  --port PORT         Set DICOM port (default: 11112)"
      echo "  --help              Show this help message"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Use --help for usage information"
      exit 1
      ;;
  esac
done

# Create directories
echo "Creating directories..."
mkdir -p "$DATA_DIR"
mkdir -p "$CONFIG_DIR"
mkdir -p "$LOGS_DIR"
mkdir -p "$DATA_DIR/storage"
mkdir -p "$DATA_DIR/worklist"
mkdir -p "$DATA_DIR/db"
mkdir -p "$DATA_DIR/security"
mkdir -p "$DATA_DIR/certs"

# Copy configuration file
echo "Copying configuration files..."
if [ "$USE_TLS" = true ]; then
  cp "$(dirname "$0")/pacs_config_tls.json" "$CONFIG_DIR/pacs_config.json"
  echo "Using TLS configuration"
  
  # Generate certificates if they don't exist
  if [ ! -f "$DATA_DIR/certs/server.crt" ]; then
    echo "Generating TLS certificates..."
    
    # Get the directory of this script
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    
    # Run the certificate generation script
    cd "$SCRIPT_DIR/.."
    ./generate_certificates.sh
    
    # Copy certificates to the data directory
    cp -r ./data/certs/* "$DATA_DIR/certs/"
    
    echo "TLS certificates generated"
  fi
else
  cp "$(dirname "$0")/pacs_config.json" "$CONFIG_DIR/pacs_config.json"
  echo "Using standard configuration (no TLS)"
fi

# Update port in configuration
sed -i "s/\"localPort\": 11112/\"localPort\": $PORT/g" "$CONFIG_DIR/pacs_config.json"

# Set up environment variables file for docker-compose
echo "Creating environment file..."
cat > "$(dirname "$0")/.env" << EOF
PACS_PORT=$PORT
PACS_DATA_DIR=$DATA_DIR
PACS_CONFIG_DIR=$CONFIG_DIR
PACS_LOGS_DIR=$LOGS_DIR
EOF

# Update docker-compose.yml with environment variables
echo "Updating docker-compose configuration..."
sed -i "s/- \"11112:11112\"/- \"$PORT:$PORT\"/g" "$(dirname "$0")/docker-compose.yml"
sed -i "s|- ./config:/app/config|- $CONFIG_DIR:/app/config|g" "$(dirname "$0")/docker-compose.yml"
sed -i "s|- ./data:/app/data|- $DATA_DIR:/app/data|g" "$(dirname "$0")/docker-compose.yml"
sed -i "s|- ./logs:/app/logs|- $LOGS_DIR:/app/logs|g" "$(dirname "$0")/docker-compose.yml"
sed -i "s/- PACS_LOCAL_PORT=11112/- PACS_LOCAL_PORT=$PORT/g" "$(dirname "$0")/docker-compose.yml"

echo "Deployment setup complete!"
echo "To start the PACS System:"
echo "  cd $(dirname "$0") && docker-compose up -d"
echo "To stop the PACS System:"
echo "  cd $(dirname "$0") && docker-compose down"
echo "To view logs:"
echo "  cd $(dirname "$0") && docker-compose logs -f"