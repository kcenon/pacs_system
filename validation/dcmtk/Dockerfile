# The host driver builds the peer image from lock.json before this image.
ARG DCMTK_IMAGE
FROM ${DCMTK_IMAGE}
USER root
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git ca-certificates python3 python3-pydicom python3-numpy \
    libsqlite3-dev libssl-dev libfmt-dev libjpeg62-turbo-dev libopenjp2-7-dev \
    libcharls-dev libpugixml-dev libcurl4-openssl-dev libasio-dev libicu-dev \
    libpng-dev zlib1g-dev lsof \
    libopenjp2-tools dicom3tools ffmpeg openjdk-21-jre-headless && rm -rf /var/lib/apt/lists/*
COPY pacs_system/.github/actions/checkout-kcenon-deps/action.yml /workspace/pacs_system/.github/actions/checkout-kcenon-deps/action.yml
COPY harness/bootstrap.py /tmp/bootstrap.py
RUN python3 /tmp/bootstrap.py
COPY pacs_system /workspace/pacs_system
COPY dcmtk-tests /peer-tests
ARG BUILD_JOBS=2
RUN cmake -S /workspace/pacs_system -B /workspace/build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_COMPILE_WARNING_AS_ERROR=OFF \
    -DPACS_WARNINGS_AS_ERRORS=OFF -DPACS_BUILD_TESTS=ON \
    -DPACS_BUILD_EXAMPLES=ON -DPACS_BUILD_STORAGE=ON -DPACS_REQUIRE_CODECS=ON \
    && cmake --build /workspace/build --parallel ${BUILD_JOBS}
# HTJ2K corpus input: the same pinned OpenJPH source, built as its own command-line tool.
RUN cmake -S /workspace/build/_deps/openjph-src -B /workspace/ojph-tools -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DOJPH_BUILD_EXECUTABLES=ON -DOJPH_ENABLE_TIFF_SUPPORT=OFF -DBUILD_SHARED_LIBS=OFF \
    && cmake --build /workspace/ojph-tools --parallel ${BUILD_JOBS} \
    && install -m 755 /workspace/ojph-tools/src/apps/ojph_compress/ojph_compress /usr/local/bin/ojph_compress
# Keep report/probe edits independent of the expensive C++ build layer.
COPY harness /workspace/pacs_system/validation/dcmtk
# DIMSE-N peers, each verified by its digest: dcm4che needs the JRE above.
COPY peers /workspace/pacs_system/validation/peers
RUN python3 /workspace/pacs_system/validation/peers/dimse-n/install.py /opt/pacs-peers
COPY driver.py /workspace/pacs_system/scripts/run_dcmtk_validation.py
RUN python3 -m unittest discover -s /workspace/pacs_system/validation/dcmtk/tests -p '*test*.py' -v
WORKDIR /workspace/build
ENTRYPOINT ["python3", "/workspace/pacs_system/validation/dcmtk/run.py"]
