# 1. Base image with build tools and ttyd
FROM alpine:latest

RUN apk add --no-cache \
      build-base \
      git \
      bash \
      curl \
      nodejs \
      npm \
   && npm install -g ttyd

# 2. Copy source and build C project
COPY . /src
WORKDIR /src
RUN make all

# 3. Expose port and launch ttyd on container start
EXPOSE 8080
CMD ["ttyd", "-p", "8080", "bash", "-lc", "cd src && ./bin/direttore"]
