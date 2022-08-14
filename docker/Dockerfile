FROM phusion/baseimage:bionic-1.0.0
LABEL maintainer="Jagoan Developers <developer@jgc.xyz>"
LABEL description="Dockerised JagoanCore, built from Travis"

ARG USER_ID
ARG GROUP_ID

ENV HOME /jgc

# add user with specified (or default) user/group ids
ENV USER_ID ${USER_ID:-1000}
ENV GROUP_ID ${GROUP_ID:-1000}
RUN groupadd -g ${GROUP_ID} jgc
RUN useradd -u ${USER_ID} -g jgc -s /bin/bash -m -d /jgc jgc
RUN mkdir /jgc/.jagoancore
RUN chown jgc:jgc -R /jgc

COPY bin/* /usr/local/bin/

RUN chmod a+x /usr/local/bin/*

USER jgc

VOLUME ["/jgc"]

EXPOSE 11111 11110 22222 22220

WORKDIR /jgc
