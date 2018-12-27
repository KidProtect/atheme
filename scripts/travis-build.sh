#!/bin/bash

PREFIX="/home/travis/atheme-install"

set -e
set -x

./configure                            \
    --prefix="${PREFIX}"               \
    --with-cracklib                    \
    --with-crypt                       \
    --with-ldap                        \
    --with-libidn                      \
    --without-mbedtls                  \
    --with-nettle                      \
    --with-openssl                     \
    --with-pcre                        \
    --with-perl                        \
    --with-qrencode                    \
    --enable-debugging                 \
    --enable-legacy-pwcrypto           \
    --enable-nls                       \
    --enable-reproducible-builds       \
    --enable-warnings                  \
    ${OPTIONAL_CONF_ARGS}

make
make install

"${PREFIX}"/bin/atheme-services -dnT