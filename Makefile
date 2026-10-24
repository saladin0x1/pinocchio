CC       ?= cc
CPPFLAGS ?=
CFLAGS   ?= -O2 -Wall -Wextra -std=gnu11
LDFLAGS  ?=
BIN      := pinocchio
RECON    := recon/recon_linux
SMC      := lab/smc_canary

.PHONY: all clean recon smc test test-traced run-traced run-stealth

all: $(BIN)

$(BIN): src/poc.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

$(RECON): recon/recon_linux.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

recon: $(RECON)
	./$(RECON)

$(SMC): lab/smc_canary.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

smc: $(SMC)
	./$(SMC)

test: $(BIN)
	sh tests/test.sh ./$(BIN)

# Requires the local images described in README.md.
test-traced:
	sh tests/test-traced.sh

run-traced: $(BIN)
	docker run --rm -v "$(CURDIR):/workdir:ro" tracerpin \
	    Tracer -o /tmp/trace.log -- /workdir/$(BIN)

run-stealth: $(BIN)
	docker run --rm -v "$(CURDIR):/workdir:ro" tracerpin-stealth \
	    kworkerd -o /tmp/trace.log -- /workdir/$(BIN)

clean:
	rm -f $(BIN) $(RECON) $(SMC) lab/naive trace.log trace.db
