FROM alpine:latest

# 1. Install build tools, curl, git
RUN apk add --no-cache build-base git bash curl

# 2. Download and install ttyd binary
RUN TT_RELEASE="1.7.3" \
 && curl -sL "https://github.com/tsl0922/ttyd/releases/download/${TT_RELEASE}/ttyd_linux_amd64.tar.gz" \
    | tar xz -C /usr/local/bin --strip-components=1 \
 && chmod +x /usr/local/bin/ttyd

# 3. Copy and build your C project
COPY . /src
WORKDIR /src
RUN make all

# 4. Expose port and launch ttyd
EXPOSE 8080
CMD ["ttyd", "-p", "8080", "bash", "-lc", "cd src && ./bin/direttore"]
