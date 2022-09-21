ARG OS_DISTRO=debian
ARG OS_VERSION=buster
ARG OS_ARCH=amd64
FROM "${OS_ARCH}/${OS_DISTRO}:${OS_VERSION}"

EXPOSE 6868

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends sudo rsyslog patch

RUN groupadd pi
RUN useradd --create-home --shell /bin/bash -g pi pi
RUN echo "pi ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

USER pi
COPY --chown=pi:pi . /home/pi/RASCSI
WORKDIR /home/pi/RASCSI

# Workaround for Bullseye amd64 compilation error
# https://github.com/akuker/RASCSI/issues/821
RUN patch -p0 < docker/rascsi/cfilesystem.patch

# Install RaSCSI standalone
RUN ./easyinstall.sh --run_choice=10 --cores=`nproc` --skip-token

USER root
COPY docker/rascsi/rascsi_wrapper.sh /usr/local/bin/rascsi_wrapper.sh
RUN chmod +x /usr/local/bin/rascsi_wrapper.sh
CMD ["/usr/local/bin/rascsi_wrapper.sh", "-L", "trace", "-r", "7", "-F", "/home/pi/images"]
