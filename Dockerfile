FROM debian:bookworm-slim AS build
RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY Makefile ./
COPY src ./src
RUN make CFLAGS="-O2 -Wall -Wextra -Werror -std=gnu11"

FROM debian:bookworm-slim
COPY --from=build /src/pinocchio /usr/local/bin/pinocchio
ENTRYPOINT ["/usr/local/bin/pinocchio"]
