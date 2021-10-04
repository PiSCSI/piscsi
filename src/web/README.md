# RaSCSI Web

## Setup local dev env

```bash
# Make a virtual env named venv
$ python3 -m venv venv
# Use that virtual env in this shell
$ source venv/bin/activate
# Install requirements
$ pip install -r requirements.txt
# Use mocks and a temp dir - start the web server
$ BASE_DIR=/tmp/images/ PATH=$PATH:`pwd`/mock/bin/ python3 web.py
```

### Mocks for local development

You may edit the files under `mock/bin` to simulate Linux command responses.
TODO:  rascsi-web uses protobuf commands to send and receive data from rascsi.
A separate mocking solusion will be needed for this interface.

## Pushing to the Pi via git

Setup a bare repo on the rascsi
```
$ ssh pi@rascsi
$ mkdir /home/pi/dev.git && cd /home/pi/dev.git
$ git --bare init
Initialized empty Git repository in /home/pi/dev.git
```

Locally
```
$ cd ~/source/RASCSI
$ git remote add pi ssh://pi@rascsi/home/pi/dev.git
$ git push pi master
```
