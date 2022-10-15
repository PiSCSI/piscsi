FROM python:3.7-bullseye

COPY python/web/requirements-dev.txt /requirements-dev.txt
RUN pip install -r /requirements-dev.txt

ENV PYTEST_USE_DOCKER_DEFAULTS=1
