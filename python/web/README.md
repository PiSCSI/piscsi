# PiSCSI Web

## Setup local dev env

```bash
# Change to python/web/src
$ cd python/web
# Make a virtual env named venv
$ python3 -m venv venv
# Use that virtual env in this shell
$ source venv/bin/activate
# Install requirements
$ pip install -r requirements.txt
# Use mocks and a temp dir - start the web server
$ BASE_DIR=/tmp/images/ PATH=$PATH:`pwd`/mock/bin/ cd python/web && PYTHON_COMMON_PATH=$(dirname $PWD)/common/src PYTHONPATH=$PWD/src:${PYTHON_COMMON_PATH} python3 src/web.py
```

### Mocks for local development

You may edit the files under `mock/bin` to simulate Linux command responses.
TODO:  piscsi-web uses protobuf commands to send and receive data from piscsi.
A separate mocking solution will be needed for this interface.

## (Optional) Pushing to the Pi via git

This is a setup for pushing code changes from your local development environment to the Raspberry Pi without a roundtrip to the remote GitHub repository.

Setup a bare repo on the piscsi
```
$ ssh pi@piscsi
$ mkdir /home/pi/dev.git && cd /home/pi/dev.git
$ git --bare init
Initialized empty Git repository in /home/pi/dev.git
```

Locally
```
$ cd ~/source/PISCSI
$ git remote add pi ssh://pi@piscsi/home/pi/dev.git
$ git push pi master
```

## Localizing the Web Interface

We use the Flask-Babel library and Flask/Jinja2 extension for internationalization (i18n).

It uses the 'pybabel' command line tool for extracting and compiling localizations. The Web Interface start script will automatically compile localizations upon launch.

Activate the Python venv in src/web/ to use it:
```
$ cd python/web
$ source venv/bin/activate
$ pybabel --help
```

To create a new localization, it needs to be added to the LANGAUGES constant in
web/settings.py. To localize messages coming from the PiSCSI backend, update also code in
raspberrypi/localizer.cpp in the PiSCSI C++ code.

Once this is done, it is time to localize the Python code. The below steps are derived from the [Flask-Babel documentation](https://python-babel.github.io/flask-babel/index.html#translating-applications).

First, generate the raw messages.pot file containing extracted strings.

```
$ pybabel extract -F babel.cfg -o messages.pot .
```

### Initialize a new localization

When adding a localization for a new language, initialize the directory structure. Replace 'xx' with the two character code for the language.

```
$ pybabel init -i messages.pot -d src/translations -l xx
```

### Update an existing loclization

After strings have been added or changed in the code, update the existing localizations.

```
pybabel update -i messages.pot -d src/translations
```

Then edit the updated messages.po file for your language. Make sure to update fuzzy strings and translate new ones.

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

### Contributing to the project

New or updated localizations are treated just like any other code change. See the [project README](https://github.com/PiSCSI/piscsi/tree/master#how-do-i-contribute) for further information.

### (Optional) See translation stats for a localization
Install the gettext package and use msgfmt to see the translation progress.
```
$ sudo apt install gettext
$ cd python/web/src
$ msgfmt --statistics translations/sv/LC_MESSAGES/messages.po
215 translated messages, 1 untranslated message.
```
