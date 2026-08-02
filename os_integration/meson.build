install_data('piscsi.conf', install_dir : 'etc' / 'rsyslog.d')

# These assets are declarative and inert after installation. A later explicit
# profile-selection action creates the NetworkManager connection or selects the
# proxy-ARP uplink; package installation never changes live connectivity.
install_data(
    'network.conf',
    install_dir : get_option('sysconfdir') / 'piscsi',
    install_mode : 'rw-r--r--',
)
install_data(
    'piscsi-network-profile',
    install_dir : get_option('sbindir'),
    install_mode : 'rwxr-xr-x',
)
install_data(
    'NetworkManager/piscsi-bridge.nmconnection.in',
    'NetworkManager/piscsi-bridge-port.nmconnection.in',
    install_dir : get_option('datadir') / 'piscsi' / 'network',
)
install_data(
    'proxyarp/piscsi-proxyarp.service',
    'proxyarp/piscsi-dhcp-relay.service',
    install_dir : '/usr/lib/systemd/system',
)
install_data(
    'proxyarp/90-piscsi-proxyarp.rules',
    install_dir : '/usr/lib/udev/rules.d',
)
install_data(
    'proxyarp/90-piscsi-proxyarp.conf',
    install_dir : get_option('sysconfdir') / 'sysctl.d',
)
install_data(
    'proxyarp/NetworkManager/90-piscsi-proxyarp.conf',
    install_dir : get_option('sysconfdir') / 'NetworkManager' / 'conf.d',
)
install_data(
    'proxyarp/piscsi-proxyarp-prepare',
    'proxyarp/piscsi-proxyarp-cleanup',
    install_dir : '/usr/lib/piscsi',
    install_mode : 'rwxr-xr-x',
)
install_data(
    'proxyarp/NetworkManager/dispatcher.d/90-piscsi-proxyarp',
    install_dir : get_option('sysconfdir') / 'NetworkManager' / 'dispatcher.d',
    install_mode : 'rwxr-xr-x',
)
