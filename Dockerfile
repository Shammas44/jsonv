FROM aflplusplus/aflplusplus:latest

ENV DEBIAN_FRONTEND=noninteractive

# Install jq (and a few useful basics)
RUN apt-get update && apt-get install -y \
    jq \
    ca-certificates \
    file \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

CMD ["/bin/bash"]
