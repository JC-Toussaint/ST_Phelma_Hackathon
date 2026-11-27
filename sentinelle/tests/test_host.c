/**
 * test_host.c - Tests sur PC du code portable de la Sentinelle
 * (prétraitement de scène, carte des changements, alarme avec hystérésis).
 *
 *   make test
 *   ./test_host --gen-csv data_synth [n]
 *        -> normal.csv / anomalie.csv SYNTHÉTIQUES pour essayer la chaîne
 *           NanoEdge AI Studio (Anomaly detection, 512 valeurs par signal) sans matériel.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include "scene_preproc.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  ECHEC : %s (ligne %d)\n", msg, __LINE__); failures++; } \
                              else { printf("  ok    : %s\n", msg); } } while (0)

/* Scène de bureau synthétique : mur à 1500 mm, écran (zones haut-gauche) à
 * 600 mm, mug (zone basse droite) à 450 mm, bruit +-noise mm. */
static void scene(tof_frame_t *f, int with_mug, int person, int noise)
{
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            int i = y * 8 + x;
            int d = 1500;
            if (x < 4 && y < 3) d = 600;               /* écran */
            if (with_mug && x == 6 && y == 6) d = 450; /* mug   */
            if (person && x >= 2 && x <= 4) d = 800;   /* quelqu'un passe */
            f->distance_mm[i] = (int16_t)(d + (noise ? rand() % (2 * noise + 1) - noise : 0));
            f->status[i] = 5;
        }
    }
}

static void test_preproc(void)
{
    tof_frame_t f;
    scene_preproc_t sp;
    float sig[SENT_SIGNAL_LEN];

    printf("[pretraitement]\n");
    CHECK(scene_zone_mm(1000, 5) == 1000, "zone valide conservee");
    CHECK(scene_zone_mm(5000, 5) == SENT_MAX_MM, "distance ecretee a SENT_MAX_MM");
    CHECK(scene_zone_mm(300, 255) == SENT_MAX_MM, "zone sans cible = loin");
    CHECK(scene_zone_mm(300, 9) == 300, "statut 9 accepte");

    scene_preproc_init(&sp);
    scene(&f, 1, 0, 0);
    int ready = 0;
    for (int k = 0; k < SENT_FRAMES_PER_SIGNAL - 1; k++) ready |= scene_preproc_push(&sp, &f);
    CHECK(!ready, "signal incomplet avant SENT_FRAMES_PER_SIGNAL trames");
    CHECK(scene_preproc_push(&sp, &f) == 1, "signal pret");
    scene_preproc_take(&sp, sig);
    CHECK(sp.count == 0, "fenetres disjointes : on repart a zero");
    CHECK(fabsf(sig[0] - 20.0f) < 1e-3f && fabsf(sig[6 * 8 + 6] - 15.0f) < 1e-3f
          && fabsf(sig[63] - 50.0f) < 1e-3f, "valeurs = 100 * d / SENT_MAX_MM");
    CHECK(fabsf(sig[(SENT_FRAMES_PER_SIGNAL - 1) * 64 + 63] - 50.0f) < 1e-3f, "derniere trame a la fin du signal");
}

static void print_map(const char map[64])
{
    for (int y = 0; y < 8; y++) {
        printf("      ");
        for (int x = 0; x < 8; x++) printf("%c ", map[y * 8 + x]);
        printf("\n");
    }
}

static void test_ref(void)
{
    tof_frame_t f;
    scene_ref_t ref;
    char map[64];

    printf("[carte des changements]\n");
    srand(1);
    scene_ref_reset(&ref);
    for (int k = 0; k < 200; k++) {
        scene(&f, 1, 0, 15);
        scene_ref_add(&ref, &f);
    }
    scene(&f, 1, 0, 15);
    CHECK(scene_ref_diff(&ref, &f, map) == 0, "scene apprise + bruit -> aucun changement");

    scene(&f, 0, 0, 15);
    int n = scene_ref_diff(&ref, &f, map);
    CHECK(n == 1 && map[6 * 8 + 6] == '-', "mug retire -> '-' a sa place");
    print_map(map);

    scene(&f, 1, 1, 15);
    n = scene_ref_diff(&ref, &f, map);
    CHECK(n == 24 && map[5 * 8 + 3] == '+' && map[1 * 8 + 3] == '-',
          "personne -> '+' devant le mur, '-' devant l'ecran (plus loin que l'ecran)");
    print_map(map);
}

static void test_alarm(void)
{
    alarm_t a;
    printf("[alarme]\n");
    alarm_init(&a, 90, 3);
    CHECK(alarm_update(&a, 50) == ALARM_NONE && alarm_update(&a, 50) == ALARM_NONE, "2 signaux anormaux : pas encore d'alarme");
    CHECK(alarm_update(&a, 95) == ALARM_NONE && a.streak == 0, "un signal normal remet le compteur a zero");
    alarm_update(&a, 50); alarm_update(&a, 50);
    CHECK(alarm_update(&a, 50) == ALARM_RAISED && a.active, "3 signaux anormaux consecutifs -> ALARME");
    CHECK(alarm_update(&a, 40) == ALARM_NONE, "pas de nouvel evenement tant que l'alarme dure");
    alarm_update(&a, 99); alarm_update(&a, 99);
    CHECK(alarm_update(&a, 99) == ALARM_CLEARED && !a.active, "3 signaux normaux -> fin d'alarme");
    CHECK(alarm_update(&a, 90) == ALARM_NONE && !a.active, "similarite = seuil -> normal");
}

static void gen_csv(const char *dir, int n)
{
    char path[512];
    mkdir(dir, 0755);
    srand(3);
    for (int cls = 0; cls < 2; cls++) {
        snprintf(path, sizeof(path), "%s/%s.csv", dir, cls ? "anomalie" : "normal");
        FILE *fp = fopen(path, "w");
        if (!fp) { perror(path); exit(1); }
        for (int s = 0; s < n; s++) {
            scene_preproc_t sp;
            tof_frame_t f;
            float sig[SENT_SIGNAL_LEN];
            int kind = cls ? 1 + rand() % 2 : 0;     /* 1 = mug retiré, 2 = personne */
            scene_preproc_init(&sp);
            for (int k = 0; k < SENT_FRAMES_PER_SIGNAL; k++) {
                scene(&f, kind != 1, kind == 2, 15);
                scene_preproc_push(&sp, &f);
            }
            scene_preproc_take(&sp, sig);
            for (int i = 0; i < SENT_SIGNAL_LEN; i++)
                fprintf(fp, "%d%c", (int)(sig[i] + 0.5f), i == SENT_SIGNAL_LEN - 1 ? '\n' : ',');
        }
        fclose(fp);
        printf("%s : %d signaux synthetiques\n", path, n);
    }
}

int main(int argc, char **argv)
{
    if (argc >= 3 && strcmp(argv[1], "--gen-csv") == 0) {
        gen_csv(argv[2], argc >= 4 ? atoi(argv[3]) : 300);
        return 0;
    }
    test_preproc();
    test_ref();
    test_alarm();
    printf("\n%s (%d echec(s))\n", failures ? "TESTS EN ECHEC" : "TOUS LES TESTS PASSENT", failures);
    return failures ? 1 : 0;
}
