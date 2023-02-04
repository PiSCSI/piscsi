make all -j4 DEBUG=1 
sudo systemctl stop piscsi 
sudo make install 
sudo systemctl start piscsi
sleep 1
rasctl -c attach -i 4 -f powerview
#rasctl -c attach -i 0 -f /home/pi/images/RaSCSI-BootstrapV3.hda
#rasctl -c attach -i 0 -f /home/pi/images/claris_works.hda
rasctl -l
tail -f /var/log/piscsi.log


