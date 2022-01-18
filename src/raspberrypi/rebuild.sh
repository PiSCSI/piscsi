make all -j4 DEBUG=1 && sudo systemctl stop rascsi && sudo make install && sudo systemctl start rascsi && sleep 1 && rasctl -c attach -i 4 -f powerview && rasctl -l
