# Tests sur PC du code portable de la Sentinelle
FW     := ../NUCLEO_L476RG_TOF_SENTINELLE
CFLAGS := -std=c99 -Wall -Wextra -Werror -O2 -I$(FW)/Inc
SRCS   := test_host.c $(FW)/Src/scene_preproc.c

test_host: $(SRCS) $(wildcard $(FW)/Inc/*.h)
	$(CC) $(CFLAGS) -o $@ $(SRCS) -lm

test: test_host collecte
	./test_host

# Outil de collecte commun (outils/) avec le profil de ce projet et ses lanceurs sentinel_logger*.py
collecte:
	$(MAKE) -C ../../outils/tests test

synth: test_host
	./test_host --gen-csv data_synth 300

clean:
	rm -rf test_host data_synth

.PHONY: test collecte synth clean
