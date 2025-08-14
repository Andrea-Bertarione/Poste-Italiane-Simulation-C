FROM alpine:latest

# Install build tools and curl using ASCII hyphens
RUN apk add --no-cache build-base git bash curl

# Download and install ttyd
RUN curl -sL \
  https://github.com/tsl0922/ttyd/releases/download/1.7.7/ttyd.x86_64 \
  -o /usr/local/bin/ttyd \
 && chmod +x /usr/local/bin/ttyd

# Copy and build
WORKDIR /app
COPY . .
RUN make clean && make all

EXPOSE 8080

# Launch a writable shell via ttyd
CMD ["ttyd", "--writable", "-p", "8080", "./bin/direttore"]
