# Tests sur PC du code portable de la table qui écoute (découpage des tapes)
FW     := ../NUCLEO_L476RG_IKS01A3_TABLE
CFLAGS := -std=c99 -Wall -Wextra -Werror -O2 -I$(FW)/Core/Inc
SRCS   := test_host.c $(FW)/Core/Src/tap_preproc.c

test_host: $(SRCS) $(wildcard $(FW)/Core/Inc/*.h)
	$(CC) $(CFLAGS) -o $@ $(SRCS) -lm

test: test_host profil collecte cubeide
	./test_host

# Profil de collecte et lanceurs table_logger*.py, piano.py, cohérents avec le firmware (Core/Inc/app_config.h)
profil:
	python3 test_profil.py

# Outil de collecte commun (outils/)
collecte:
	$(MAKE) -C ../../outils/tests test

# CSV synthétiques (5 classes) pour essayer NanoEdge AI Studio sans matériel
synth: test_host
	./test_host --gen-csv data_synth 200

clean:
	rm -rf test_host data_synth

.PHONY: test profil collecte cubeide synth clean

# .cproject du projet CubeIDE : chaîne d'outils, cible MCU, script de liaison (voir ../../CUBEIDE_IMPORT.md)
cubeide:
	@python3 ../../cubeide_cible.py --check ../NUCLEO_*
