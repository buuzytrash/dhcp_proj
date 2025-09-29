# DHCP CLIENT
Implementation of the dhcp-client

## Usage
sudo ./bin/dhcp_client <interface_name> (for example: eth0 or wlp2s0)

## Test usage in docker:
docker compose up --build
This will start:
    - dhcp server (dnsmasq) in separate container
    - dhcp client in another container
Test environment network settings:
    - Network: 192.168.91.0/24
    - DHCP Server: 192.168.91.2
    - Gateway: 192.168.91.1
    - IP address range: 192.168.91.100 - 192.168.91.200
    - DNS Server: 8.8.8.8
