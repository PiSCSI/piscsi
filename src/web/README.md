# RaSCSI Web

## Setup

```bash
sudo apt install genisoimage python3 python3-venv nginx -y
git clone https://github.com/erichelgeson/RaSCSI-web.git
cd RaSCSI-web
./start.sh
```

## As a startup service

```bash
## Configure NGINX
sudo cp service-infra/nginx-default.conf /etc/nginx/sites-available/default
sudo systemctl reload nginx

## Configure rascsi-web service
sudo cp rascsi-web.service /etc/systemd/system/rascsi-web.service
sudo systemctl daemon-reload
sudo systemctl enable rascsi-web
sudo systemctl start rascsi-web
```