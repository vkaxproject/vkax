FROM phusion/baseimage:bionic-1.0.0
LABEL maintainer="Vkax Developers <developer@vkax.xyz>"
LABEL description="Dockerised VkaxCore, built from Travis"

ARG USER_ID
ARG GROUP_ID

ENV HOME /vkax

# add user with specified (or default) user/group ids
ENV USER_ID ${USER_ID:-1000}
ENV GROUP_ID ${GROUP_ID:-1000}
RUN groupadd -g ${GROUP_ID} vkax
RUN useradd -u ${USER_ID} -g vkax -s /bin/bash -m -d /vkax vkax
RUN mkdir /vkax/.vkaxcore
RUN chown vkax:vkax -R /vkax

COPY bin/* /usr/local/bin/

RUN chmod a+x /usr/local/bin/*

USER vkax

VOLUME ["/vkax"]

EXPOSE 11111 11110 22222 22220

WORKDIR /vkax
