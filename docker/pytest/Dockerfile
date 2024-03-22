FROM python:3.9-slim
ENV DOCKER=1

WORKDIR /src

COPY python/web/requirements-dev.txt /src/requirements-dev.txt
COPY python/web/pyproject.toml /src/pyproject.toml
COPY python/web/tests /src/tests

RUN pip install --no-cache-dir -r /src/requirements-dev.txt

ENTRYPOINT ["pytest"]
