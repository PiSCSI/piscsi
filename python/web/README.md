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
$ BASE_DIR=/tmp/images/ PATH=$PATH:`pwd`/mock/bin/ cd src && python3 web.py
```

### Mocks for local development

You may edit the files under `mock/bin` to simulate Linux command responses.
TODO:  rascsi-web uses protobuf commands to send and receive data from rascsi.
A separate mocking solution will be needed for this interface.

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

It uses the 'pybabel' command line tool for extracting and compiling localizations.
Activate the Python venv in src/web/ to use it:
```
$ cd src/web/
$ source venv/bin/activate
$ pybabel --help
```

To create a new localization, it needs to be added to the LANGAUGES constant in
web/settings.py. To localize messages coming from the RaSCSI backend, update also code in
raspberrypi/localizer.cpp in the RaSCSI C++ code.

Once this is done, follow the steps in the [Flask-Babel documentation](https://flask-babel.tkte.ch/#translating-applications)
to generate the messages.po for the new language.

Updating the strings in an existing messages.po is also covered above.

When you are ready to contribute new or updated localizations, use the same Gitflow Workflow as used for any code contributions to submit PRs against the develop branch.

### Working with PO files

See the [GNU gettext documentation](https://www.gnu.org/software/gettext/manual/html_node/PO-Files.html) for an introduction to the PO file format.

We make heavy use of __python-format__ for formatting, for instance:
```
#: file_cmds.py:353
#, python-format
msgid "%(file_name)s downloaded to %(save_dir)s"
msgstr "Laddade ner %(file_name)s till %(save_dir)s"
```

There are also a few instances of formatting in JavaScript:
```
#: templates/index.html:381
msgid "Server responded with code: {{statusCode}}"
msgstr "Servern svarade med kod: {{statusCode}}"
```

And with html tags:
```
#: templates/index.html:304
#, python-format
msgid ""
"Emulates a SCSI DaynaPORT Ethernet Adapter. <a href=\"%(url)s\">Host "
"drivers and configuration required</a>."
msgstr ""
"Emulerar en SCSI DaynaPORT ethernet-adapter. <a href=\"%(url)s\">Kräver "
"drivrutiner och inställningar</a>."
```

### (Optional) See translation stats for a localization
Install the gettext package and use msgfmt to see the translation progress.
```
$ sudo apt install gettext
$ cd src/web/
$ msgfmt --statistics translations/sv/LC_MESSAGES/messages.po
215 translated messages, 1 untranslated message.
```
