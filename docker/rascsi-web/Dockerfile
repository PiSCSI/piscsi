ARG OS_DISTRO=debian
ARG OS_VERSION=buster
ARG OS_ARCH=amd64
FROM "${OS_ARCH}/${OS_DISTRO}:${OS_VERSION}"

EXPOSE 80 443

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
    sudo \
    systemd \
    rsyslog \
    procps \
    man-db \
    wget \
    git

RUN groupadd pi
RUN useradd --create-home --shell /bin/bash -g pi pi
RUN echo "pi ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
RUN echo "pi:rascsi" | chpasswd 

RUN mkdir /home/pi/shared_files
RUN touch /etc/dhcpcd.conf
RUN mkdir -p /etc/network/interfaces.d/

USER pi
WORKDIR /home/pi/RASCSI
COPY --chown=pi:pi . .

# Install standalone RaSCSI web UI
RUN ./easyinstall.sh --run_choice=11

# Enable web UI authentication
RUN ./easyinstall.sh --run_choice=13

# Setup wired network bridge
RUN ./easyinstall.sh --run_choice=5 --headless

USER root
WORKDIR /home/pi
RUN pip3 install watchdog
COPY docker/rascsi-web/start.sh /usr/local/bin/start.sh
RUN chmod +x /usr/local/bin/start.sh
CMD ["/usr/local/bin/start.sh"]
