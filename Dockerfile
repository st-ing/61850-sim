FROM alpine

ARG VERSION
ARG BUILD_NUMBER
ARG BUILD_DATE

LABEL maintainer="Dusan Stojkovic <dusan@sting.dev>"

LABEL org.opencontainers.image.title="Fuzzy IEC61850 Simulator"
LABEL org.opencontainers.image.version=$VERSION
LABEL org.opencontainers.image.revision=$BUILD_NUMBER
LABEL org.opencontainers.image.created=$BUILD_DATE
LABEL org.opencontainers.image.vendor="sting GmbH"
LABEL org.opencontainers.image.base.name="stinging/61850-sim"

RUN apk add linux-headers build-base openjdk8

# simulation related
COPY include ./opt/include
COPY lib ./opt/lib
COPY src ./opt/src

# model related
COPY tools ./opt/tools

WORKDIR /opt

COPY docker-entrypoint /usr/bin/
RUN chmod +x /usr/bin/docker-entrypoint

# TBD - non-root, healthcheck

ENTRYPOINT ["/usr/bin/docker-entrypoint"]
