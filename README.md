# Loft
A middleware server and Docker Image based on [NGIX Unit](https://unit.nginx.org/) that exchanges data between [IoT devices](https://docs.circuitdojo.com/nrf9160-introduction.html) and Verizon Thingspace using Verizon's [ThingSpace Connectivity Management API](https://thingspace.verizon.com/documentation/apis/connectivity-management/working-with-verizon/about-non-ip-data-delivery.html).

## Development

### Recomendations
<!-- - Install [NGINX Unit from source](https://unit.nginx.org/installation/#source-code)
  - `make` libunit-install ([ref](https://www.nginx.com/blog/nginx-unit-adds-assembly-language-support/))
  - Make sure `libunit.a` and it's `include` dir are in a path that GCC includes by default; or edit the C include file paths in the [source code](https://github.com/justins-engineering/nidd-sign-serv/blob/main/src/main.c) for testing. -->
- VSCode
  - Build & run tasks are included
  - Configured to use Clang-Tidy
- [Docker Buildx](https://docs.docker.com/engine/reference/commandline/buildx/)

### Setup
```sh
./init.sh
```
This script inits, updates, and resets the git submodules. It then runs `bundle && bun install` on the front-end submodule, and finally it runs `make clean && make`.

### Build
```sh
./build.sh
```
#### `build.sh` Flags:
- `p`: prune Docker containers, images, and buildx cache after build
- `r`: runs the docker container after building with the options `--rm -p 80:80 --name loft_dev`
- `d`: Adds the docker build option `--build-arg debug=true`
  - Adds vim to the built image
  - Adds the aliases `ls='ls -F --color=auto'` & `grep='grep -nI --color=auto'` to `/root/.bashrc`
  - Builds `unitd` & `libunit.a` with the `--debug` optiion
- `m`: Builds `Dockerfile.minsize`; not recomended for development as it offers far less caching

### Running
```sh
docker run --rm -p 80:80 --name loft_dev loft:dev
```
The server will be viewable at http://localhost:80

### Docker container shell
```sh
docker exec -it loft_dev /bin/bash
```
