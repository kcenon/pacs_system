#!/bin/bash

# PACS Initial Admin Setup Script
# This script helps create the initial admin user with a secure password

set -e

echo "======================================"
echo "PACS System Initial Admin Setup"
echo "======================================"
echo

# Check if users.json already exists
USERS_FILE="./data/security/users.json"
if [ -f "$USERS_FILE" ]; then
    echo "WARNING: Users file already exists at $USERS_FILE"
    read -p "Do you want to overwrite it? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Setup cancelled."
        exit 1
    fi
fi

# Create security directory if it doesn't exist
mkdir -p ./data/security

# Get admin username (default: admin)
read -p "Enter admin username [admin]: " ADMIN_USERNAME
ADMIN_USERNAME=${ADMIN_USERNAME:-admin}

# Get admin full name
read -p "Enter admin full name [Administrator]: " ADMIN_FULLNAME
ADMIN_FULLNAME=${ADMIN_FULLNAME:-Administrator}

# Get admin email
read -p "Enter admin email [admin@hospital.local]: " ADMIN_EMAIL
ADMIN_EMAIL=${ADMIN_EMAIL:-admin@hospital.local}

# Get admin password
while true; do
    read -s -p "Enter admin password (min 8 chars): " ADMIN_PASSWORD
    echo
    
    if [ ${#ADMIN_PASSWORD} -lt 8 ]; then
        echo "Password must be at least 8 characters long."
        continue
    fi
    
    read -s -p "Confirm admin password: " ADMIN_PASSWORD_CONFIRM
    echo
    
    if [ "$ADMIN_PASSWORD" != "$ADMIN_PASSWORD_CONFIRM" ]; then
        echo "Passwords do not match. Please try again."
        continue
    fi
    
    break
done

# Create temporary config file with admin credentials
TEMP_CONFIG=$(mktemp)
cat > "$TEMP_CONFIG" << EOF
{
    "security": {
        "admin": {
            "username": "$ADMIN_USERNAME",
            "password": "$ADMIN_PASSWORD",
            "fullname": "$ADMIN_FULLNAME",
            "email": "$ADMIN_EMAIL"
        },
        "create": {
            "default": {
                "user": "false"
            }
        }
    }
}
EOF

echo
echo "Creating initial admin user..."

# Run PACS server briefly to create the admin user
# Note: This assumes the PACS server binary is available
if [ -f "./bin/pacs_server" ]; then
    # Set environment to use temporary config
    export PACS_CONFIG_FILE="$TEMP_CONFIG"
    
    # Start server in background
    ./bin/pacs_server --create-admin-only 2>/dev/null &
    SERVER_PID=$!
    
    # Wait a moment for initialization
    sleep 2
    
    # Stop server
    kill $SERVER_PID 2>/dev/null || true
    
    # Clean up
    rm -f "$TEMP_CONFIG"
    
    echo "Initial admin user created successfully!"
    echo
    echo "You can now start the PACS server with:"
    echo "  ./bin/pacs_server"
    echo
    echo "Login with:"
    echo "  Username: $ADMIN_USERNAME"
    echo "  Password: [the password you entered]"
    echo
else
    echo "ERROR: PACS server binary not found at ./bin/pacs_server"
    echo "Please build the project first with ./build.sh"
    rm -f "$TEMP_CONFIG"
    exit 1
fi

echo "======================================"
echo "Setup completed successfully!"
echo "======================================"