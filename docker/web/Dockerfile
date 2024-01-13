ARG DEBIAN_FRONTEND=noninteractive
ARG OS_VERSION=bullseye

FROM "debian:${OS_VERSION}-slim"

RUN apt-get update \
    && apt-get install -y --no-install-recommends sudo systemd rsyslog procps man-db wget git gcc \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN groupadd pi \
    && useradd --create-home --shell /bin/bash -g pi pi \
    && echo "pi ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers \
    && echo "pi:piscsi" | chpasswd

# Make mock commands available when executing sudo
RUN sed -i 's/^Defaults\tsecure_path="\/usr/Defaults\tsecure_path="\/home\/pi\/piscsi\/python\/web\/mock\/bin:\/usr/' /etc/sudoers

RUN mkdir -p /home/pi/shared_files \
    && mkdir /home/pi/images \
    && mkdir -p /etc/network/interfaces.d \
    && touch /etc/dhcpcd.conf

USER pi
WORKDIR /home/pi/piscsi

COPY --chown=pi:pi easyinstall.sh .
COPY --chown=pi:pi os_integration os_integration
COPY --chown=pi:pi cpp/piscsi_interface.proto cpp/piscsi_interface.proto
COPY --chown=pi:pi python/web python/web
COPY --chown=pi:pi python/common python/common

# Install standalone PiSCSI Web UI
RUN ./easyinstall.sh --run_choice=12 \
    && sudo apt-get remove build-essential --yes \
    && sudo apt autoremove -y \
    && sudo apt-get clean \
    && sudo rm -rf /var/lib/apt/lists/*

# Enable web UI authentication
RUN ./easyinstall.sh --run_choice=14

# Setup wired network bridge
RUN ./easyinstall.sh --run_choice=5 --headless

USER root
WORKDIR /home/pi
RUN pip3 install --no-cache-dir PyYAML watchdog
COPY docker/web/web_start_wrapper.sh /usr/local/bin/web_start_wrapper.sh
RUN chmod +x /usr/local/bin/web_start_wrapper.sh

EXPOSE 80 443
ENTRYPOINT ["/usr/local/bin/web_start_wrapper.sh"]

HEALTHCHECK --interval=5m --timeout=3s \
  CMD wget --quiet --server-response http://localhost/healthcheck 2>&1 | grep "200 OK"
