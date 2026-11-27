# Tests sur PC du code portable (prétraitement + logique de jeu)
FW     := ../NUCLEO_L476RG_TOF_PFC
CFLAGS := -std=c99 -Wall -Wextra -Werror -O2 -I$(FW)/Inc
SRCS   := test_host.c $(FW)/Src/tof_preproc.c $(FW)/Src/rps_game.c

test_host: $(SRCS) $(wildcard $(FW)/Inc/*.h)
	$(CC) $(CFLAGS) -o $@ $(SRCS) -lm

test: test_host collecte augmente
	./test_host

# Outil de collecte commun (outils/) avec le profil de ce projet et ses lanceurs rps_logger*.py
collecte:
	$(MAKE) -C ../../outils/tests test

# Augmentation des données réelles (tools/augmente_pfc.py), essayée sur les données synthétiques
augmente: data_synth/pierre.csv
	python3 test_augmente.py

data_synth/pierre.csv: test_host
	./test_host --gen-csv data_synth 300

synth: test_host
	./test_host --gen-csv data_synth 300

clean:
	rm -rf test_host data_synth

.PHONY: test collecte augmente synth clean
