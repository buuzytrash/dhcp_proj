FROM alpine:latest

RUN apk update && apk add --no-cache \
    gcc \
    musl-dev \
    linux-headers \
    make \
    bash \
    iproute2 \
    iputils \
    tcpdump \
    dnsmasq \
    net-tools

WORKDIR /app
COPY . .
RUN make clean && make

CMD ["sh", "-c", "ip addr flush dev eth0 && ip link set eth0 up && ./bin/dhcp_client eth0"]