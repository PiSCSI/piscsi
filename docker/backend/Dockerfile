ARG DEBIAN_FRONTEND=noninteractive

FROM debian:bullseye AS build
RUN apt-get update && apt-get install --assume-yes --no-install-recommends sudo
RUN groupadd pi \
    && useradd --create-home --shell /bin/bash -g pi pi \
    && echo "pi ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

USER pi
WORKDIR /home/pi/piscsi

COPY --chown=pi:pi easyinstall.sh .
COPY --chown=pi:pi cpp cpp
COPY --chown=pi:pi doc doc
RUN ./easyinstall.sh --run_choice=16 --cores=`nproc`

FROM debian:bullseye-slim AS runner
USER root
WORKDIR /home/pi

COPY --from=build /home/pi/piscsi/cpp/bin/* /usr/local/bin/
COPY docker/backend/piscsi_wrapper.sh /usr/local/bin/piscsi_wrapper.sh
RUN chmod +x /usr/local/bin/*
RUN mkdir -p /home/pi/images

RUN apt-get update \
    && apt-get install --no-install-recommends --assume-yes libpcap-dev libprotobuf-dev \
    && apt autoremove -y \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

EXPOSE 6868
ENTRYPOINT ["/usr/local/bin/piscsi_wrapper.sh", "-r", "7", "-F", "/home/pi/images"]
CMD ["-L", "trace"]

HEALTHCHECK --interval=5m --timeout=1s CMD scsictl -v
