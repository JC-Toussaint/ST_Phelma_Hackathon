/**
 * test_host.c - Tests sur PC du code portable de la baguette (wand_preproc.c)
 *
 *   make test
 *   ./test_host --gen-csv data_synth [n]
 *        -> cercle.csv, zigzag.csv, frappe.csv, v.csv, autre.csv SYNTHÉTIQUES (n gestes par classe),
 *           passés par le vrai découpage du firmware, pour essayer NanoEdge AI Studio sans matériel.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include "wand_preproc.h"

static int failures, checks;
#define CHECK(cond, msg) do { checks++; if (cond) printf("  ok    : %s\n", msg); \
                              else { failures++; printf("  ECHEC : %s\n", msg); } } while (0)

static const double PI = 3.14159265358979323846;

static imu_sample_t still(void)
{
    imu_sample_t s = {0, 0, 1000, 0, 0, 0};   /* posée à plat : 1 g sur z, pas de rotation */
    return s;
}

static imu_sample_t moving(void)
{
    imu_sample_t s = {300, -200, 1000, 400000, 0, 0};   /* 400 °/s autour de x */
    return s;
}

static int rest_samples(void)
{
    return (WAND_REST_MS * IMU_ODR_HZ + 999) / 1000;
}

static void test_scaling(void)
{
    printf("Mise à l'échelle\n");
    CHECK(wand_scale_acc(0) == WAND_VALUE_MAX / 2, "accélération nulle -> milieu (500)");
    CHECK(wand_scale_acc(IMU_ACC_FS_G * 1000) == WAND_VALUE_MAX, "+pleine échelle accéléro -> 1000");
    CHECK(wand_scale_acc(-IMU_ACC_FS_G * 1000) == 0, "-pleine échelle accéléro -> 0");
    CHECK(wand_scale_acc(3 * IMU_ACC_FS_G * 1000) == WAND_VALUE_MAX, "accélération hors échelle écrêtée");
    CHECK(wand_scale_acc(1000) == WAND_VALUE_MAX / 2 + 1000 / WAND_ACC_MG_PER_UNIT, "1 g -> 500 + 1000 mg / (mg par unité)");
    CHECK(wand_scale_gyro(IMU_GYRO_FS_DPS * 1000) == WAND_VALUE_MAX, "+pleine échelle gyro -> 1000");
    CHECK(wand_scale_gyro(-5 * IMU_GYRO_FS_DPS * 1000) == 0, "rotation hors échelle écrêtée");
}

static void test_detection(void)
{
    printf("Mouvement / calme\n");
    imu_sample_t s = still();
    CHECK(!wand_is_moving(&s) && wand_is_calm(&s), "baguette posée : calme, pas de mouvement");
    s = moving();
    CHECK(wand_is_moving(&s) && !wand_is_calm(&s), "rotation rapide : mouvement");
    s = still(); s.gx = (WAND_TRIGGER_DPS + 10) * 1000;
    CHECK(wand_is_moving(&s), "gyro juste au-dessus du seuil : mouvement");
    s = still(); s.ax = 1500;   /* |a| ≈ 1,8 g */
    CHECK(wand_is_moving(&s), "frappe (accélération forte, sans rotation) : mouvement");
    s = still(); s.az = 200;    /* chute libre partielle */
    CHECK(wand_is_moving(&s), "accélération faible (< 1 g - seuil) : mouvement");
    s = still(); s.gx = (WAND_CALM_DPS + WAND_TRIGGER_DPS) / 2 * 1000;
    CHECK(!wand_is_moving(&s) && !wand_is_calm(&s), "petite rotation : ni geste, ni calme");
}

static void test_window(void)
{
    printf("Fenêtre et déclenchement\n");
    static wand_pre_t w;
    static uint16_t out[WAND_SIGNAL_LEN];
    imu_sample_t s;
    int ready = 0;

    wand_pre_init(&w);
    s = moving();
    for (int i = 0; i < 50; i++) ready |= wand_pre_push(&w, &s);
    CHECK(!ready && wand_pre_state(&w) == WAND_ARMING, "pas de geste tant que la baguette n'a pas été calme");

    s = still();
    for (int i = 0; i < rest_samples() - 1; i++) wand_pre_push(&w, &s);
    CHECK(wand_pre_state(&w) == WAND_ARMING, "pas encore armée juste avant WAND_REST_MS de calme");
    wand_pre_push(&w, &s);
    CHECK(wand_pre_state(&w) == WAND_ARMED, "armée après WAND_REST_MS de calme");
    for (int i = 0; i < 40; i++) wand_pre_push(&w, &s);

    /* Le geste : des échantillons « mouvement » numérotés (ay = 2000 + i) pour suivre leur place. */
    int pushed = 0;
    for (int i = 0; i < 500 && !ready; i++) {
        s = moving();
        s.ay = 2000 + i;
        ready = wand_pre_push(&w, &s);
        pushed++;
    }
    CHECK(ready && wand_pre_state(&w) == WAND_READY, "fenêtre prête");
    CHECK(pushed == WAND_SAMPLES - WAND_PRETRIGGER,
          "fenêtre complète après WAND_SAMPLES - WAND_PRETRIGGER échantillons de geste");
    CHECK(w.win[0].gx == 0 && w.win[WAND_PRETRIGGER - 1].gx == 0,
          "les WAND_PRETRIGGER premiers échantillons sont ceux d'avant le geste");
    CHECK(w.win[WAND_PRETRIGGER].ay == 2000, "le premier échantillon du geste suit le pré-déclenchement");
    CHECK(w.win[WAND_SAMPLES - 1].ay == 2000 + pushed - 1, "le dernier échantillon est le plus récent");

    s = still();
    CHECK(wand_pre_push(&w, &s) == 0 && wand_pre_state(&w) == WAND_READY,
          "fenêtre non récupérée : les nouveaux échantillons sont ignorés");

    wand_pre_take(&w, out);
    CHECK(out[0] == wand_scale_acc(0) && out[2] == wand_scale_acc(1000) && out[3] == wand_scale_gyro(0),
          "sortie entrelacée ax,ay,az,gx,gy,gz (échantillon calme)");
    CHECK(out[WAND_PRETRIGGER * WAND_AXES + 3] == wand_scale_gyro(400000), "gx du premier échantillon du geste");
    CHECK(wand_pre_state(&w) == WAND_ARMING, "après récupération : il faut de nouveau du calme");

    s = moving();
    ready = 0;
    for (int i = 0; i < 300; i++) ready |= wand_pre_push(&w, &s);
    CHECK(!ready, "un mouvement continu ne produit pas de second geste sans pause");
}

static void test_start_without_history(void)
{
    printf("Geste juste après l'armement\n");
    static wand_pre_t w;
    imu_sample_t s = still();
    wand_pre_init(&w);
    for (int i = 0; i < rest_samples(); i++) wand_pre_push(&w, &s);
    int ready = 0, n = 0;
    s = moving();
    while (!ready && n < 500) { ready = wand_pre_push(&w, &s); n++; }
    CHECK(ready, "fenêtre complète même avec un historique court");
    CHECK(n == WAND_SAMPLES - WAND_PRETRIGGER, "le tampon de pré-déclenchement était déjà plein (calme > 16 éch.)");
}

/* ------------------------------------------------------------------------- */
/* Gestes synthétiques                                                       */
/* ------------------------------------------------------------------------- */
static double urand(void) { return rand() / (double)RAND_MAX; }
static double nrand(void) { return (urand() + urand() + urand() - 1.5) * 0.8; }

/* Échantillon d'un geste de durée dur (s) à l'instant t (s) ; classe 0..4. amp : amplitude relative. */
static imu_sample_t gesture_sample(int cls, double t, double dur, double amp)
{
    imu_sample_t s = still();
    double u = t / dur;                       /* 0..1 pendant le geste */
    if (u < 0.0 || u > 1.0) return s;
    double env = sin(PI * u);                 /* montée puis descente */
    switch (cls) {
    case 0: /* cercle : rotation qui tourne autour des axes x et y */
        s.gx = (int32_t)(amp * 450000 * env * cos(2 * PI * u));
        s.gy = (int32_t)(amp * 450000 * env * sin(2 * PI * u));
        s.ax = (int32_t)(amp * 700 * env * sin(2 * PI * u));
        s.ay = (int32_t)(amp * 700 * env * -cos(2 * PI * u));
        break;
    case 1: { /* zigzag = lettre Z : trait vers la droite, diagonale bas-gauche, trait vers la droite */
        static const double dx[3] = {1.0, -0.7, 1.0}, dz[3] = {0.0, -0.7, 0.0};
        int k = (int)(u * 3.0);
        if (k > 2) k = 2;
        double v = u * 3.0 - k;                         /* 0..1 dans le trait k */
        double a = sin(2 * PI * v);                     /* accélère puis freine */
        s.ax = (int32_t)(amp * 1300 * dx[k] * a);
        s.az += (int32_t)(amp * 1300 * dz[k] * a);
        s.gz = (int32_t)(amp * 220000 * dx[k] * a);
        s.gy = (int32_t)(amp * 220000 * dz[k] * a);
        break;
    }
    case 2: /* frappe : coup bref vers le bas puis arrêt sec */
        if (u < 0.35) { s.az += (int32_t)(-amp * 900 * sin(PI * u / 0.35)); s.gx = (int32_t)(amp * 500000 * sin(PI * u / 0.35)); }
        else if (u < 0.45) { s.az += (int32_t)(amp * 2600 * sin(PI * (u - 0.35) / 0.1)); }
        break;
    case 3: /* V : diagonale vers le bas, puis vers le haut */
        if (u < 0.5) { s.ax = (int32_t)(amp * 800 * sin(2 * PI * u)); s.az += (int32_t)(-amp * 700 * sin(2 * PI * u)); s.gy = (int32_t)(amp * 300000 * sin(2 * PI * u)); }
        else { s.ax = (int32_t)(amp * 800 * sin(2 * PI * (u - 0.5))); s.az += (int32_t)(amp * 700 * sin(2 * PI * (u - 0.5))); s.gy = (int32_t)(-amp * 300000 * sin(2 * PI * (u - 0.5))); }
        break;
    default: /* autre : mouvement quelconque, lent et irrégulier */
        s.gx = (int32_t)(amp * 250000 * env * sin(2 * PI * (1.3 * u + 0.2)));
        s.gy = (int32_t)(amp * 200000 * env * sin(2 * PI * (0.7 * u + 0.5)));
        s.ax = (int32_t)(amp * 500 * env * sin(2 * PI * (0.9 * u)));
        break;
    }
    return s;
}

static void add_noise(imu_sample_t *s)
{
    s->ax += (int32_t)(nrand() * 25); s->ay += (int32_t)(nrand() * 25); s->az += (int32_t)(nrand() * 25);
    s->gx += (int32_t)(nrand() * 3000); s->gy += (int32_t)(nrand() * 3000); s->gz += (int32_t)(nrand() * 3000);
}

static int gen_csv(const char *dir, int n)
{
    static const char *names[5] = {"cercle", "zigzag", "frappe", "v", "autre"};
    static wand_pre_t w;
    static uint16_t out[WAND_SIGNAL_LEN];
    mkdir(dir, 0755);
    srand(1234);
    for (int c = 0; c < 5; c++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s.csv", dir, names[c]);
        FILE *f = fopen(path, "w");
        if (!f) { perror(path); return 1; }
        int written = 0, guard = 0;
        while (written < n && guard++ < n * 10) {
            double dur = 0.6 + 0.5 * urand();          /* vitesse du geste variable */
            double amp = 0.75 + 0.5 * urand();
            wand_pre_init(&w);
            int got = 0;
            for (int k = 0; k < 400 && !got; k++) {     /* calme, geste, calme */
                double t = (k - 60) / (double)IMU_ODR_HZ;
                imu_sample_t s = gesture_sample(c, t, dur, amp);
                add_noise(&s);
                if (wand_pre_push(&w, &s)) { wand_pre_take(&w, out); got = 1; }
            }
            if (!got) continue;
            for (int i = 0; i < WAND_SIGNAL_LEN; i++) fprintf(f, "%u%s", out[i], i + 1 < WAND_SIGNAL_LEN ? "," : "\n");
            written++;
        }
        fclose(f);
        printf("  %s : %d signaux de %d valeurs\n", path, written, WAND_SIGNAL_LEN);
        if (written < n) return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 3 && strcmp(argv[1], "--gen-csv") == 0) {
        return gen_csv(argv[2], argc >= 4 ? atoi(argv[3]) : 200);
    }
    test_scaling();
    test_detection();
    test_window();
    test_start_without_history();
    printf("\n%s (%d verification(s), %d echec(s))\n",
           failures ? "DES TESTS ECHOUENT" : "TOUS LES TESTS PASSENT", checks, failures);
    return failures ? 1 : 0;
}
