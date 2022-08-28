ARG OS_DISTRO=debian
ARG OS_VERSION=buster
ARG OS_ARCH=amd64
FROM "${OS_ARCH}/${OS_DISTRO}:${OS_VERSION}"

EXPOSE 80 443

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends sudo rsyslog procps

RUN groupadd pi
RUN useradd --create-home --shell /bin/bash -g pi pi
RUN echo "pi ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

WORKDIR /home/pi
USER pi
COPY --chown=pi:pi . RASCSI
RUN cd RASCSI && ./easyinstall.sh --run_choice=11 --skip-token

USER root
RUN pip3 install watchdog
COPY docker/rascsi-web/start.sh /usr/local/bin/start.sh
RUN chmod +x /usr/local/bin/start.sh
CMD ["/usr/local/bin/start.sh"]
