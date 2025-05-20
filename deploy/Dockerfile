# PACS System Dockerfile
FROM ubuntu:22.04 AS builder

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    unzip \
    pkg-config \
    libssl-dev \
    libbz2-dev \
    libfmt-dev \
    libsqlite3-dev \
    libcrypto++-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# Install vcpkg
WORKDIR /opt
RUN git clone https://github.com/Microsoft/vcpkg.git && \
    cd vcpkg && \
    ./bootstrap-vcpkg.sh && \
    ./vcpkg integrate install

# Set up environment
ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"

# Install dependencies with vcpkg
RUN vcpkg install fmt nlohmann-json cryptopp sqlite3 dcmtk[with-openssl]

# Create build directory
WORKDIR /app
COPY . .

# Create build directory
RUN mkdir -p build

# Build
WORKDIR /app/build
RUN cmake .. -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake && \
    cmake --build . --config Release

# Create runtime image
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    libfmt8 \
    libsqlite3-0 \
    libcrypto++8 \
    && rm -rf /var/lib/apt/lists/*

# Create app directory structure
RUN mkdir -p /app/bin /app/data /app/logs /app/data/db /app/data/certs /app/config

# Copy the built binaries
COPY --from=builder /app/build/bin/pacs_server /app/bin/

# Copy default config
COPY pacs_default_config.json /app/config/

# Create data directories
RUN mkdir -p /app/data/storage /app/data/worklist

# Set default environment variables
ENV PACS_CONFIG_FILE_PATH=/app/config/pacs_config.json
ENV PACS_DATA_DIRECTORY=/app/data
ENV PACS_LOG_DIRECTORY=/app/logs
ENV PACS_DATABASE_DIRECTORY=/app/data/db
ENV PACS_AE_TITLE=PACS
ENV PACS_LOCAL_PORT=11112

# Expose the DICOM port
EXPOSE 11112

# Set working directory
WORKDIR /app

# Set entrypoint
ENTRYPOINT ["/app/bin/pacs_server"]
CMD ["/app/config/pacs_config.json"]