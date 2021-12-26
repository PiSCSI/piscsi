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
A separate mocking solution will be needed for this interface.

### Static analysis with pylint

It is recommended to run pylint against new code to protect against bugs
and keep the code readable and maintainable.
The local pylint configuration lives in .pylintrc

```
sudo apt install pylint3
pylint3 python_source_file.py
```

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

## Localizing the Web Interface

We use the Flask-Babel library and Flask/Jinja2 extension for i18n.

To create a new localization, it needs to be added to accept_languages in
the get_locale() method, and also to localizer.cpp in the RaSCSI C++ code.

Once this is done, follow the steps in the [Flask-Babel documentation](https://flask-babel.tkte.ch/#translating-applications)
to generate the messages.po for the new language.

Updating an existing messages.po is also covered above.
