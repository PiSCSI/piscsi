# PiSCSI NetworkManager profiles

PiSCSI supports two explicitly selected network-adapter topologies on Raspberry
Pi OS Trixie and Forky with NetworkManager. DaynaPort and Host Bridge use the
same host-side profiles. Installing the PiSCSI package only installs inert
templates, services, and `/etc/piscsi/network.conf`; it never creates a bridge,
changes an active connection, starts proxy-ARP, or changes firewall state.

The required `parprouted` and `dhcp-helper` packages are suggested rather than
installed automatically. This prevents `dhcp-helper`'s generic service from
binding DHCP on an existing host interface before the user selects proxy-ARP.

## Wired bridge

Bridge mode is the protocol-complete host-side choice: it supports DHCP,
AppleTalk, multicast, and non-IP Ethernet protocols. NetworkManager must own
the bridge, the physical Ethernet port, and the bridge address. PiSCSI owns
only the ephemeral `piscsi0` TAP membership while a network adapter is
attached. DaynaPort can use that complete Ethernet service; the Host Bridge
driver accepts only its own unicast frames and Ethernet broadcasts.

From a local console, preview and apply the named profile with:

```sh
sudo piscsi-network-profile --dry-run apply bridge eth0
sudo piscsi-network-profile --console-acknowledge apply bridge eth0
```

The command prints every change, refuses to run without NetworkManager, and
creates only the named `PiSCSI wired bridge` and `PiSCSI wired bridge port`
connections. `--console-acknowledge` is explicit approval and skips the
otherwise required local-terminal `APPLY` prompt. It can be removed with
`sudo piscsi-network-profile --console-acknowledge remove bridge`.

For manual configuration, the package provides inert keyfile templates in
`/usr/share/piscsi/network/`. Substitute unique UUIDs, the physical NIC, and
the physical Ethernet MAC in the templates, copy them to
`/etc/NetworkManager/system-connections/` with mode `0600`, then reload
NetworkManager. The bridge MAC must remain distinct from the emulated client
MAC on `piscsi0`. The equivalent commands are:

```sh
ethernet_mac=$(cat /sys/class/net/eth0/address)
nmcli connection add type bridge ifname piscsi_bridge con-name 'PiSCSI wired bridge' ipv4.method auto ipv6.method auto bridge.stp no 802-3-ethernet.cloned-mac-address "$ethernet_mac"
nmcli connection add type ethernet ifname eth0 master piscsi_bridge
nmcli connection up 'PiSCSI wired bridge'
```

Replace `eth0` with the selected physical Ethernet interface. Verify that the
bridge owns DHCP and the selected Ethernet MAC, and that the physical port has
no independent IP address before attaching a PiSCSI network adapter with
`mode=bridge:interface=piscsi_bridge`.

## Wi-Fi proxy ARP

Proxy-ARP exposes a PiSCSI network-adapter client directly on the Wi-Fi IPv4
LAN. It supports IPv4 unicast and DHCP only. It does not provide IPv6,
AppleTalk, multicast, mDNS reflection, or general Ethernet bridging. The Host
Bridge driver additionally accepts only its own unicast frames and Ethernet
broadcasts, regardless of the selected profile.

Before selecting this profile, remove any legacy PiSCSI NAT/firewall rules
manually. The package and profile command never remove, flush, or rewrite
iptables/nftables state. Keep a local console or another recovery path: Wi-Fi
proxy-ARP cannot replace a broken Wi-Fi association and is not a transparent
Ethernet bridge.

Install the suggested dependencies, then preview and apply the profile from a
local console:

```sh
sudo apt install parprouted dhcp-helper
sudo piscsi-network-profile --dry-run apply proxyarp wlan0
sudo piscsi-network-profile --console-acknowledge apply proxyarp wlan0
```

Replace `wlan0` with the NetworkManager-managed Wi-Fi interface holding the
Pi's global IPv4 lease. The command writes only
`/etc/piscsi/network.conf`, reloads PiSCSI runtime configuration, and disables
the generic `dhcp-helper.service` to avoid its UDP/67 conflict. To remove the
PiSCSI selection, use
`sudo piscsi-network-profile --console-acknowledge remove proxyarp`.
`PISCSI_PROXYARP_PROMISC=true` is a compatibility option; PiSCSI records and
restores the flag only if it enabled promiscuous mode itself.

The udev rule starts `piscsi-proxyarp.service` only after PiSCSI creates
`piscsi0`; it then starts the PiSCSI-owned DHCP relay. The relay conflicts with
the generic `dhcp-helper.service` to prevent UDP/67 contention. A NetworkManager
dispatcher restarts both PiSCSI services after the selected uplink reconnects
or receives a changed DHCP lease.

Verify an attachment with:

```sh
ip -br link show piscsi0
ip -4 addr show dev piscsi0
systemctl status piscsi-proxyarp piscsi-dhcp-relay
journalctl -u piscsi-proxyarp -u piscsi-dhcp-relay -b
```

On detach, PiSCSI removes only the exact `/32` it added to `piscsi0`, stops
only PiSCSI-owned processes, and restores promiscuous mode only when it had
changed it. It does not flush addresses, routes, or firewall state.

## Network-adapter attachment

Always submit a complete mode/interface pair. For example:

```sh
scsictl -i 6 -c attach -t scdp -f 'mode=bridge:interface=piscsi_bridge'
scsictl -i 6 -c attach -t scdp -f 'mode=proxyarp:interface=wlan0'
scsictl -i 5 -c attach -t scbr -f 'mode=bridge:interface=piscsi_bridge'
scsictl -i 5 -c attach -t scbr -f 'mode=proxyarp:interface=wlan0'
```

Do not use the retired `inet` parameter or a comma-separated interface list.
They describe the obsolete NAT/implicit-bridge path and are rejected.
