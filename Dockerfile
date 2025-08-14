# 1. Base image with build tools and curl
FROM alpine:latest
RUN apk add --no‐cache build‐base git bash curl

# 2. Download and install the ttyd binary
RUN curl -sL \
  https://github.com/tsl0922/ttyd/releases/download/1.7.7/ttyd.x86_64 \
  -o /usr/local/bin/ttyd \
 && chmod +x /usr/local/bin/ttyd

# 3. Copy the entire repo into /app
WORKDIR /app
COPY . .

# 4. Build all C binaries via the root Makefile
RUN make clean && make all

# 5. Expose the terminal port
EXPOSE 8080

# 6. Start a bare bash shell in /app with ttyd
CMD ["ttyd", "--writable", "-p", "8080", "bash", "-lc", "cd /app && ./bin/direttore"]
