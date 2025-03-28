# syntax=docker/dockerfile:1

FROM debian:stable-slim

LABEL org.opencontainers.image.title="Loft"
LABEL org.opencontainers.image.description="A middleware server based on NGIX Unit that exchanges data between IoT devices and Verizon Thingspace"
LABEL org.opencontainers.image.url="https://github.com/justins-engineering/loft"
LABEL org.opencontainers.image.source="https://github.com/justins-engineering/loft"

# NGINX Unit URL and version
ARG nginx_unit_link="https://github.com/nginx/unit"
ARG nginx_unit_version="1.34.2-1"

# Redis download URL
ARG redis_stable_link="https://download.redis.io/redis-stable.tar.gz"
ARG redis_stable_sha256="https://download.redis.io/redis-stable.tar.gz.SHA256SUM"

# Debugging build? Override ARG with CLI flag "--build-arg debug=true"
ARG debug=false

# Debugging Unit? Override ARG with CLI flag "--build-arg debug_unit=true"
ARG debug_unit=false

ARG prefix="/usr"
ARG exec_prefix="$prefix"
ARG src_dir="$exec_prefix/src"
ARG bin_dir="$exec_prefix/bin"
ARG include_dir="$prefix/include"
ARG lib_dir="$exec_prefix/lib"
ARG data_root_dir="$prefix/share"
ARG man_dir="$data_root_dir/man"
ARG local_state_dir="$prefix/var"
ARG lib_state_dir="$local_state_dir/run/unit"
ARG log_dir="$local_state_dir/log/unit"
ARG log_file="$log_dir/unit.log"
ARG tmp_dir="/tmp"
ARG unit_group="unit"
ARG unit_user="unit"
ARG unit_clone_dir="$src_dir/unit"

ARG srv_dir="/srv"
ARG app_assets_dir="$srv_dir/assets"
ARG app_clone_dir="$src_dir/loft"
ARG app_src_dir="$app_clone_dir/src"
ARG app_include_dir="$app_clone_dir/include"
ARG app_lib_dir="$app_clone_dir/lib"
ARG app_group="loft"
ARG app_user="loft"
ARG app_firmware_dir="$srv_dir/firmware"

ARG ncpu="getconf _NPROCESSORS_ONLN"

ENV UNIT_RUN_STATE_DIR="$local_state_dir/run/unit"
ENV UNIT_PID_PATH="$UNIT_RUN_STATE_DIR/unit.pid"
ENV UNIT_SOCKET="$UNIT_RUN_STATE_DIR/control.unit.sock"
ENV UNIT_SBIN_DIR="$exec_prefix/sbin"
ENV DEB_CFLAGS_MAINT_APPEND="-Wp,-D_FORTIFY_SOURCE=2 -march=native -mtune=native -fPIC"
ENV DEB_LDFLAGS_MAINT_APPEND="-Wl,--as-needed -pie"
ENV DEB_BUILD_MAINT_OPTIONS="hardening=+all,-pie"

ARG cc_opt="dpkg-buildflags --get CFLAGS"
ARG ld_opt="dpkg-buildflags --get LDFLAGS"
ARG unit_config_args=\
"--cc=gcc \
--openssl \
--user=$unit_user \
--group=$unit_group \
--prefix=$prefix \
--exec-prefix=$exec_prefix \
--bindir=$bin_dir \
--sbindir=$UNIT_SBIN_DIR \
--includedir=$include_dir \
--libdir=$lib_dir \
--modulesdir=$lib_dir/modules \
--datarootdir=$data_root_dir \
--mandir=$man_dir \
--localstatedir=$local_state_dir \
--logdir=$log_dir \
--log=$log_file \
--runstatedir=$UNIT_RUN_STATE_DIR \
--pid=$UNIT_PID_PATH \
--control=unix:$UNIT_SOCKET \
--tmpdir=$tmp_dir"

# Install dependencies
RUN set -ex \
  && apt-get update \
  && if [ "$debug" = "true" ]; then \
    apt-get install --no-install-recommends --no-install-suggests -y ca-certificates git build-essential \
    libssl-dev libpcre2-dev curl pkg-config libcurl4-openssl-dev vim libasan8 libjemalloc-dev\
    && echo "alias ls='ls -F --color=auto'" >> /root/.bashrc \
    && echo "alias grep='grep -nI --color=auto'" >> /root/.bashrc; \
  else \
    apt-get install --no-install-recommends --no-install-suggests -y ca-certificates git build-essential \
    libssl-dev libpcre2-dev curl pkg-config libcurl4-openssl-dev; \
  fi

# Prepare dirs, group, and user
RUN set -ex \
  && mkdir -p \
    $bin_dir \
    $UNIT_SBIN_DIR \
    $include_dir \
    $lib_dir \
    $data_root_dir \
    $man_dir \
    $local_state_dir/lib/unit/certs \
    $local_state_dir/lib/unit/scripts \
    $log_dir \
    $UNIT_RUN_STATE_DIR \
    $tmp_dir \
    $app_include_dir \
    $app_lib_dir \
    $app_clone_dir/config \
    $app_firmware_dir \
    /docker-entrypoint.d \
  && mkdir -p -m=700 $lib_state_dir \
  && groupadd --gid 999 $unit_group \
  && groupadd --gid 1000 $app_group \
  && useradd \
    --uid 999 \
    --gid $unit_group \
    --no-create-home \
    --home /nonexistent \
    --comment "unit user" \
    --shell /bin/false \
    $unit_user \
  && useradd \
    --uid 1000 \
    --gid $app_group \
    --no-create-home \
    --home /nonexistent \
    --comment "loft server user" \
    --shell /bin/false \
    $app_user \
  && chown root:$app_group $app_firmware_dir \
  && chmod -R 775 $app_firmware_dir

# Download Redis stable tarball and check SHA256
RUN set -ex \
&& if [ "$debug" = "true" ]; then \
  curl -O $redis_stable_link \
  && curl $redis_stable_sha256 | sha256sum -c; \
fi

# Install Redis CLI
RUN set -ex \
&& if [ "$debug" = "true" ]; then \
  tar xzf redis-stable.tar.gz \
  && make -C redis-stable \
  && cp redis-stable/src/redis-cli $bin_dir \
  && chown root:$app_group $bin_dir/redis-cli \
  && chmod 755 $bin_dir/redis-cli \
  && rm -rf redis-stable/ redis-stable.tar.gz; \
fi

# Clone Unit
RUN set -ex && git clone --depth 1 -b $nginx_unit_version $nginx_unit_link $unit_clone_dir

# Change working directory
WORKDIR $unit_clone_dir

# Configure, make, and install unitd
RUN set -ex \
  && if [ "$debug_unit" = "true" ]; \
    then ./configure $unit_config_args --debug --cc-opt="$(eval $cc_opt)" --ld-opt="$(eval $ld_opt)"; \
    else ./configure $unit_config_args --cc-opt="$(eval $cc_opt)" --ld-opt="$(eval $ld_opt)"; \
  fi \
  && make -j $(eval $ncpu) unitd \
  && install -pm755 ./build/sbin/unitd "$UNIT_SBIN_DIR/unitd" \
  && ln -sf /dev/stdout "$log_file" \
  && make -j $(eval $ncpu) libunit-install

# Save/apt-mark unitd dependencies
RUN set -ex \
  && cd \
  && savedAptMark="$(apt-mark showmanual)" \
  && for f in $UNIT_SBIN_DIR/unitd; do \
    ldd $f | awk '/=>/{print $(NF-1)}' | while read n; do dpkg-query -S $n; done | sed 's/^\([^:]\+\):.*$/\1/' | sort | uniq >> /requirements.apt; \
    done \
  && apt-mark showmanual | xargs apt-mark auto > /dev/null \
  && { [ -z "$savedAptMark" ] || apt-mark manual $savedAptMark; } \
  && /bin/true \
  && apt-get update \
  && apt-get --no-install-recommends --no-install-suggests -y install $(cat /requirements.apt)

# Change working directory
WORKDIR $app_clone_dir

# Copy module source files
COPY --link ./modules/ "$app_clone_dir"/modules

# Copy app source files
COPY --link ./src/ "$app_src_dir"
COPY --link ./include/definitions.h "$app_include_dir"
COPY --link ./Makefile "$app_clone_dir"

# Copy app assets
COPY --link ./assets/ "$app_assets_dir"

RUN --mount=type=secret,id=vzw_secrets.h set -ex \
  && cp /run/secrets/vzw_secrets.h "$app_clone_dir"/config/vzw_secrets.h

# Compile and link Loft
RUN set -ex \
  && if [ "$debug" = "true" ]; \
    then make -j $(eval $ncpu) CC=gcc DEBUG=1 EXTRA_CFLAGS=-fsanitize=address\ -static-libasan EXTRA_LDFLAGS=-fsanitize=address; \
    else make -j $(eval $ncpu) CC=gcc EXTRA_CFLAGS=-std=gnu2x; \
  fi \
  && make install INSTALL_BIN_PATH="$bin_dir" \
  && rm -rf "$src_dir" \
  && apt-get purge -y --auto-remove build-essential \
  && rm -rf /var/lib/apt/lists/* \
  && rm -f /requirements.apt

# Copy initial config
COPY --link config/config.json /docker-entrypoint.d/

# Copy the default entrypoint
COPY --link ./docker-entrypoint.sh /usr/local/bin/

# Make sure the entrypoint is executable
RUN ["chmod", "+x", "/usr/local/bin/docker-entrypoint.sh"]

STOPSIGNAL SIGTERM

ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
EXPOSE 80
CMD ["unitd", "--no-daemon", "--control", "unix:$UNIT_SOCKET"]
