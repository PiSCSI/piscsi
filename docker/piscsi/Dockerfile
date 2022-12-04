ARG OS_DISTRO=debian
ARG OS_VERSION=buster
ARG OS_ARCH=amd64
FROM "${OS_ARCH}/${OS_DISTRO}:${OS_VERSION}"

EXPOSE 6868

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends sudo systemd rsyslog patch wget

RUN groupadd pi
RUN useradd --create-home --shell /bin/bash -g pi pi
RUN echo "pi ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

USER pi
WORKDIR /home/pi/PISCSI
COPY --chown=pi:pi . .

# Install PiSCSI standalone
RUN ./easyinstall.sh --run_choice=10 --cores=`nproc`

USER root
WORKDIR /home/pi
COPY docker/piscsi/piscsi_wrapper.sh /usr/local/bin/piscsi_wrapper.sh
RUN chmod +x /usr/local/bin/piscsi_wrapper.sh
CMD ["/usr/local/bin/piscsi_wrapper.sh", "-L", "trace", "-r", "7", "-F", "/home/pi/images"]
