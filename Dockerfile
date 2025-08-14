FROM alpine:latest

# 1) Install build tools and curl
RUN apk add --no-cache build-base git bash curl

# 2) Download and install ttyd v1.7.7
RUN curl -sL \
  https://github.com/tsl0922/ttyd/releases/download/1.7.7/ttyd.x86_64 \
  -o /usr/local/bin/ttyd \
 && chmod +x /usr/local/bin/ttyd

# 3) Copy and build your C project
COPY . /src
WORKDIR /src
RUN make all

# 4) Expose port and launch ttyd
EXPOSE 8080
CMD ["ttyd", "--writable", "-p", "8080", "bash", "-c", "cd src && ./bin/direttore || bash"]

