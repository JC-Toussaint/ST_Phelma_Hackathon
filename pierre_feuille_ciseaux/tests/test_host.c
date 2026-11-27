/**
 * test_host.c - Tests sur PC du code portable (prétraitement + logique de jeu).
 *
 *   make test              -> lance les tests
 *   ./test_host --gen-csv data_synth
 *        -> génère pierre.csv / feuille.csv / ciseaux.csv SYNTHÉTIQUES, pour
 *           essayer la chaîne NanoEdge AI Studio (format 512 valeurs par signal) avant
 *           d'avoir le matériel. Ne pas utiliser ces données pour la vraie démo !
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include "tof_preproc.h"
#include "rps_game.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  ECHEC : %s (ligne %d)\n", msg, __LINE__); failures++; } \
                              else { printf("  ok    : %s\n", msg); } } while (0)

/* --- génération de trames synthétiques ---------------------------------- */
static void frame_background(tof_frame_t *f)
{
    for (int i = 0; i < TOF_ZONES; i++) {
        f->distance_mm[i] = 2500; /* plafond lointain : au-delà de PRE_FAR_MM */
        f->status[i] = 5;
    }
}

static void put(tof_frame_t *f, int x, int y, int d)
{
    if (x >= 0 && x < 8 && y >= 0 && y < 8) {
        f->distance_mm[y * 8 + x] = (int16_t)d;
        f->status[y * 8 + x] = 5;
    }
}

/* Distance des mains simulées : au milieu de la plage PRE_NEAR_MM..PRE_FAR_MM, pour que les tests
 * restent valables quel que soit le réglage (voir ../REGLAGE_DISTANCE.md). */
#define HAND_MM ((PRE_NEAR_MM + PRE_FAR_MM) / 2)

/* Dessine un coup centré en (cx,cy) à distance d. */
static void draw_move(tof_frame_t *f, rps_move_t m, int cx, int cy, int d, int noise)
{
    frame_background(f);
    #define N() (noise ? (rand() % (2 * noise + 1)) - noise : 0)
    switch (m) {
    case RPS_PIERRE:   /* poing : blob 3x3 */
        for (int y = -1; y <= 1; y++)
            for (int x = -1; x <= 1; x++)
                put(f, cx + x, cy + y, d + 10 * (abs(x) + abs(y)) + N());
        break;
    case RPS_FEUILLE:  /* main ouverte : grande surface 5x6 */
        for (int y = -3; y <= 2; y++)
            for (int x = -2; x <= 2; x++)
                put(f, cx + x, cy + y, d + 5 * abs(x) + N());
        break;
    case RPS_CISEAUX:  /* paume 3x2 + deux doigts écartés */
        for (int y = 0; y <= 1; y++)
            for (int x = -1; x <= 1; x++)
                put(f, cx + x, cy + y, d + 15 + N());
        for (int y = -3; y <= -1; y++) {
            put(f, cx - 1 - (y < -2), cy + y, d + N());
            put(f, cx + 1 + (y < -2), cy + y, d + N());
        }
        break;
    default:
        break;
    }
    #undef N
}

static void print_img(const float img[TOF_ZONES])
{
    static const char sh[] = " .:-=+*#%@";
    for (int y = 0; y < 8; y++) {
        printf("      |");
        for (int x = 0; x < 8; x++) {
            int v = (int)img[y * 8 + x];
            char c = sh[v * 10 / 101];
            printf("%c%c", c, c);
        }
        printf("|\n");
    }
}

static void centroid(const float img[TOF_ZONES], float *cx, float *cy)
{
    float s = 0, sx = 0, sy = 0;
    for (int i = 0; i < TOF_ZONES; i++) {
        s += img[i]; sx += img[i] * (i % 8); sy += img[i] * (i / 8);
    }
    *cx = sx / s; *cy = sy / s;
}

/* --- tests ---------------------------------------------------------------- */
static void test_preproc(void)
{
    tof_frame_t f;
    float img[TOF_ZONES];
    int16_t dmin;
    tof_preproc_t pp;

    printf("[pretraitement]\n");

    frame_background(&f);
    CHECK(tof_preproc_frame(&f, img, &dmin) == 0, "fond seul -> aucune zone main");

    draw_move(&f, RPS_PIERRE, 1, 6, HAND_MM, 0);
    int n = tof_preproc_frame(&f, img, &dmin);
    CHECK(n == 9, "pierre -> 9 zones");
    CHECK(dmin == HAND_MM, "distance mini = distance de la main");
    float mx = 0; for (int i = 0; i < TOF_ZONES; i++) if (img[i] > mx) mx = img[i];
    CHECK(fabsf(mx - 100.0f) < 1e-3f, "point le plus proche = 100");
    float cx, cy; centroid(img, &cx, &cy);
    CHECK(fabsf(cx - 3.5f) <= 1.0f && fabsf(cy - 3.5f) <= 1.0f, "main recentree pres du centre");
    print_img(img);

    draw_move(&f, RPS_PIERRE, 3, 3, HAND_MM, 0);
    f.status[3 * 8 + 3] = 255;   /* zone invalide */
    CHECK(tof_preproc_frame(&f, img, &dmin) == 8, "statut invalide ignore");

    draw_move(&f, RPS_FEUILLE, 4, 4, HAND_MM - 20, 0);
    f.distance_mm[0] = (int16_t)(HAND_MM + PRE_DEPTH_WINDOW_MM + 50); f.status[0] = 5;   /* objet loin derriere la main (> fenetre) */
    CHECK(tof_preproc_frame(&f, img, &dmin) == 30 && dmin == HAND_MM - 20, "objet hors fenetre de profondeur ignore");

    draw_move(&f, RPS_CISEAUX, 4, 4, HAND_MM, 0);
    printf("    ciseaux :\n");
    tof_preproc_frame(&f, img, &dmin);
    print_img(img);
    draw_move(&f, RPS_FEUILLE, 4, 4, HAND_MM, 0);
    printf("    feuille :\n");
    tof_preproc_frame(&f, img, &dmin);
    print_img(img);

    /* empilement des trames */
    tof_preproc_init(&pp);
    int ready = 0;
    for (int k = 0; k < PRE_FRAMES_PER_SIGNAL - 1; k++) {
        draw_move(&f, RPS_PIERRE, 3, 3, HAND_MM, 0);
        ready |= tof_preproc_push(&pp, &f);
    }
    CHECK(!ready, "signal incomplet avant PRE_FRAMES_PER_SIGNAL trames");
    draw_move(&f, RPS_FEUILLE, 4, 4, HAND_MM, 0);   /* trame la plus recente differente */
    CHECK(tof_preproc_push(&pp, &f) == 1, "signal pret apres PRE_FRAMES_PER_SIGNAL trames");
    float sig[SIGNAL_LEN];
    tof_preproc_get_signal(&pp, sig);
    int nz_first = 0, nz_last = 0;
    for (int i = 0; i < TOF_ZONES; i++) {
        nz_first += sig[i] > 0;
        nz_last  += sig[(PRE_FRAMES_PER_SIGNAL - 1) * TOF_ZONES + i] > 0;
    }
    CHECK(nz_first == 9 && nz_last == 30, "ordre du signal : plus ancienne trame d'abord");
    CHECK(tof_preproc_push(&pp, &f) == 1, "fenetre glissante : un signal par nouvelle trame");

    frame_background(&f);
    CHECK(tof_preproc_push(&pp, &f) == 0 && pp.count == 0 && !pp.hand_present, "main retiree -> remise a zero");
}

static void test_game(void)
{
    printf("[jeu]\n");
    CHECK(rps_judge(RPS_PIERRE, RPS_CISEAUX) == RPS_PLAYER_WINS, "pierre bat ciseaux");
    CHECK(rps_judge(RPS_CISEAUX, RPS_FEUILLE) == RPS_PLAYER_WINS, "ciseaux bat feuille");
    CHECK(rps_judge(RPS_FEUILLE, RPS_PIERRE) == RPS_PLAYER_WINS, "feuille bat pierre");
    CHECK(rps_judge(RPS_CISEAUX, RPS_PIERRE) == RPS_STM32_WINS, "STM32 pierre bat ciseaux");
    int draws = 0;
    for (int m = RPS_PIERRE; m <= RPS_CISEAUX; m++) draws += rps_judge(m, m) == RPS_DRAW;
    CHECK(draws == 3, "coups identiques -> egalite");

    rps_game_t g;
    /* Joueur prévisible : alterne pierre, pierre, feuille... */
    static const rps_move_t pattern[] = { RPS_PIERRE, RPS_PIERRE, RPS_FEUILLE };
    rps_game_init(&g, 1234, RPS_STRAT_ADAPTIVE, 20);
    for (int i = 0; i < 300; i++) {
        rps_move_t s = rps_stm32_choose(&g);
        rps_play_round(&g, pattern[i % 3], s);
    }
    printf("    adaptatif vs joueur previsible : STM32 %d / joueur %d / nuls %d\n",
           g.score_stm32, g.score_player, g.draws);
    CHECK(g.score_stm32 > 150, "la strategie adaptative exploite un joueur previsible (>50% de victoires)");

    rps_game_init(&g, 1234, RPS_STRAT_RANDOM, 0);
    int hist[4] = {0};
    for (int i = 0; i < 3000; i++) hist[rps_stm32_choose(&g)]++;
    CHECK(hist[0] == 0 && hist[1] > 900 && hist[2] > 900 && hist[3] > 900, "aleatoire : 3 coups equiprobables");

    CHECK(rps_commit(RPS_PIERRE, 42) == rps_commit(RPS_PIERRE, 42), "empreinte deterministe");
    CHECK(rps_commit(RPS_PIERRE, 42) != rps_commit(RPS_FEUILLE, 42), "empreinte differente selon le coup");

    rps_vote_t v;
    uint8_t c;
    rps_vote_reset(&v);
    rps_vote_add(&v, RPS_CISEAUX); rps_vote_add(&v, RPS_PIERRE); rps_vote_add(&v, RPS_CISEAUX);
    rps_vote_add(&v, RPS_NONE);
    CHECK(rps_vote_winner(&v, &c) == RPS_CISEAUX && c == 2 && v.total == 3, "vote majoritaire");
}

/* --- CSV synthétiques -------------------------------------------------------- */
static void gen_csv(const char *dir, int per_class)
{
    static const char *names[] = { NULL, "pierre", "feuille", "ciseaux" };
    mkdir(dir, 0755);
    srand(7);
    for (int m = RPS_PIERRE; m <= RPS_CISEAUX; m++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s.csv", dir, names[m]);
        FILE *fp = fopen(path, "w");
        if (!fp) { perror(path); exit(1); }
        for (int s = 0; s < per_class; s++) {
            tof_preproc_t pp;
            tof_frame_t f;
            float sig[SIGNAL_LEN];
            tof_preproc_init(&pp);
            int cx = 2 + rand() % 4, cy = 3 + rand() % 3;
            int d = PRE_NEAR_MM + 20 + rand() % (PRE_FAR_MM - PRE_NEAR_MM - 70);   /* + 38 au plus : < PRE_FAR_MM */
            for (int k = 0; k < PRE_FRAMES_PER_SIGNAL; k++) {
                draw_move(&f, (rps_move_t)m, cx + (rand() % 3 == 0), cy, d + rand() % 10, 8);
                tof_preproc_push(&pp, &f);
            }
            tof_preproc_get_signal(&pp, sig);
            for (int i = 0; i < SIGNAL_LEN; i++)
                fprintf(fp, "%d%c", (int)(sig[i] + 0.5f), i == SIGNAL_LEN - 1 ? '\n' : ',');
        }
        fclose(fp);
        printf("%s : %d signaux synthetiques\n", path, per_class);
    }
}

int main(int argc, char **argv)
{
    if (argc >= 3 && strcmp(argv[1], "--gen-csv") == 0) {
        gen_csv(argv[2], argc >= 4 ? atoi(argv[3]) : 300);
        return 0;
    }
    test_preproc();
    test_game();
    printf("\n%s (%d echec(s))\n", failures ? "TESTS EN ECHEC" : "TOUS LES TESTS PASSENT", failures);
    return failures ? 1 : 0;
}
