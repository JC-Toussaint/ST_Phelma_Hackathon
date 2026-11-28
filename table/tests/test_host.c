/**
 * test_host.c - Tests sur PC du code portable de la table qui écoute (tap_preproc.c)
 *
 *   make test
 *   ./test_host --gen-csv data_synth [n]
 *        -> zone1.csv … zone4.csv, autre.csv SYNTHÉTIQUES (n tapes par classe), passés par le vrai
 *           découpage du firmware, pour essayer NanoEdge AI Studio sans matériel.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include "tap_preproc.h"

static int failures, checks;
#define CHECK(cond, msg) do { checks++; if (cond) printf("  ok    : %s\n", msg); \
                              else { failures++; printf("  ECHEC : %s\n", msg); } } while (0)

static const double PI = 3.14159265358979323846;
#define REST ((TAP_REST_MS * IMU_ODR_HZ + 999) / 1000)

static acc_sample_t still(void)
{
    acc_sample_t s = {12, -8, 1000};   /* carte sous la table : 1 g sur z, petit défaut d'aplomb */
    return s;
}

/* Amène le découpage à l'état « prêt » (moyenne de repos établie, calme suffisant). */
static void arm(tap_pre_t *t)
{
    acc_sample_t s = still();
    tap_pre_init(t);
    for (int i = 0; i < REST + 10; i++) tap_pre_push(t, &s);
}

static void test_scaling(void)
{
    printf("Mise à l'échelle\n");
    CHECK(tap_scale(0) == TAP_VALUE_MAX / 2, "écart nul -> milieu (500)");
    CHECK(tap_scale(IMU_ACC_FS_G * 1000) == TAP_VALUE_MAX, "+pleine échelle -> 1000");
    CHECK(tap_scale(-IMU_ACC_FS_G * 1000) == 0, "-pleine échelle -> 0");
    CHECK(tap_scale(5 * IMU_ACC_FS_G * 1000) == TAP_VALUE_MAX, "hors échelle écrêté");
    CHECK(tap_scale(80) == TAP_VALUE_MAX / 2 + 80 / TAP_MG_PER_UNIT, "80 mg -> 500 + 80 / (mg par unité)");
}

static void test_arming(void)
{
    printf("Repos et armement\n");
    tap_pre_t t;
    acc_sample_t s = still();
    tap_pre_init(&t);
    for (int i = 0; i < REST - 1; i++) tap_pre_push(&t, &s);
    CHECK(tap_pre_state(&t) == TAP_ARMING, "pas armé avant TAP_REST_MS de calme");
    for (int i = 0; i < 2; i++) tap_pre_push(&t, &s);
    CHECK(tap_pre_state(&t) == TAP_ARMED, "armé après TAP_REST_MS de calme");
    CHECK(tap_deviation(&t, &s) <= 1, "la moyenne de repos a rejoint la gravité");

    /* Table qui vibre (quelqu'un écrit) : jamais armée. */
    tap_pre_init(&t);
    for (int i = 0; i < 3 * REST; i++) {
        acc_sample_t v = still();
        v.az += (i % 2) ? 40 : -40;
        tap_pre_push(&t, &v);
    }
    CHECK(tap_pre_state(&t) == TAP_ARMING, "vibrations continues > TAP_CALM_MG : pas armé");
}

static void test_trigger(void)
{
    printf("Déclenchement et fenêtre\n");
    tap_pre_t t;
    uint16_t out[TAP_SIGNAL_LEN];
    arm(&t);

    acc_sample_t s = still();
    s.az += TAP_TRIGGER_MG - 10;
    tap_pre_push(&t, &s);
    CHECK(tap_pre_state(&t) == TAP_ARMED, "écart juste sous le seuil : rien");

    /* Une tape : choc puis oscillation amortie, numérotée sur ax pour suivre les places. */
    int ready = 0, pushed = 0;
    for (int i = 0; i < TAP_SAMPLES && !ready; i++) {
        acc_sample_t v = still();
        v.az += (int32_t)(400.0 * exp(-i / 40.0) * cos(2 * PI * i / 8.0)) + (i == 0 ? 200 : 0);
        v.ax += 1000 + i;
        ready = tap_pre_push(&t, &v);
        pushed++;
    }
    CHECK(ready && pushed == TAP_SAMPLES - TAP_PRETRIGGER, "fenêtre pleine après TAP_SAMPLES - TAP_PRETRIGGER échantillons de tape");
    CHECK(t.win[TAP_PRETRIGGER - 1].ax == still().ax + TAP_TRIGGER_MG - 10 || t.win[TAP_PRETRIGGER - 1].ax == still().ax,
          "le dernier échantillon d'avant la tape est du repos");
    CHECK(t.win[TAP_PRETRIGGER].ax == still().ax + 1000, "la tape commence juste après le pré-déclenchement");
    CHECK(t.win[TAP_SAMPLES - 1].ax == still().ax + 1000 + pushed - 1, "le dernier échantillon est le plus récent");

    tap_pre_take(&t, out);
    CHECK(out[0] == TAP_VALUE_MAX / 2 && out[1] == TAP_VALUE_MAX / 2 && out[2] == TAP_VALUE_MAX / 2,
          "repos retiré : le pré-déclenchement vaut 500 sur les 3 axes");
    CHECK(out[TAP_PRETRIGGER * TAP_AXES + 2] > TAP_VALUE_MAX / 2 + TAP_TRIGGER_MG / TAP_MG_PER_UNIT,
          "le choc ressort nettement sur az");
    CHECK(tap_pre_state(&t) == TAP_ARMING, "après la tape : attente de calme avant la suivante");

    /* Fenêtre récupérée en retard : les échantillons suivants sont ignorés. */
    arm(&t);
    for (int i = 0; i < TAP_SAMPLES + 50; i++) {
        acc_sample_t v = still();
        v.az += 500;
        tap_pre_push(&t, &v);
    }
    CHECK(tap_pre_state(&t) == TAP_READY, "fenêtre prête conservée tant qu'elle n'est pas récupérée");
}

static void test_slow_drift(void)
{
    printf("Dérive lente\n");
    tap_pre_t t;
    arm(&t);
    /* On pose doucement un objet lourd : la table s'incline de 150 mg en 2 s. La moyenne suit. */
    int fired = 0;
    for (int i = 0; i < 2 * IMU_ODR_HZ; i++) {
        acc_sample_t v = still();
        v.ay += 150 * i / (2 * IMU_ODR_HZ);
        fired |= tap_pre_push(&t, &v) || tap_pre_state(&t) != TAP_ARMED;
    }
    CHECK(!fired, "inclinaison lente de 150 mg : aucun déclenchement");
}

static void test_rearm(void)
{
    printf("Abandon d'une tape\n");
    tap_pre_t t;
    arm(&t);
    acc_sample_t v = still();
    v.az += 500;
    tap_pre_push(&t, &v);
    CHECK(tap_pre_state(&t) == TAP_RECORDING, "tape commencée");
    tap_pre_rearm(&t);
    CHECK(tap_pre_state(&t) == TAP_ARMING && t.win_count == 0, "tap_pre_rearm : tape abandonnée, attente de calme");
}

/* ------------------------------------------------------------------------- */
/* CSV synthétiques : chaque zone = une oscillation amortie de fréquence, d'amortissement et de
 * répartition sur les axes propres, après un petit retard ; « autre » = objet posé, frottement, coude. */
static double urand(void) { return rand() / (double)RAND_MAX; }

static acc_sample_t tap_sample(int cls, int i, double amp, double jitter)
{
    static const double freq[4] = {180, 260, 340, 430};     /* Hz */
    static const double tau[4]  = {0.020, 0.030, 0.015, 0.040};
    static const double dir[4][3] = {{0.3, 0.1, 1.0}, {0.1, 0.4, 1.0}, {-0.3, 0.2, 0.8}, {0.2, -0.4, 0.9}};
    acc_sample_t s = still();
    double t = i / (double)IMU_ODR_HZ;
    double v;
    if (cls < 4) {
        double d = 0.002 * cls + jitter;                      /* retard de propagation */
        v = (t < d) ? 0 : amp * exp(-(t - d) / tau[cls]) * sin(2 * PI * freq[cls] * (t - d));
        s.ax += (int32_t)(v * dir[cls][0]);
        s.ay += (int32_t)(v * dir[cls][1]);
        s.az += (int32_t)(v * dir[cls][2]);
    } else {
        v = amp * 0.6 * (1 - exp(-t / 0.05)) * sin(2 * PI * 35 * t);   /* choc mou, basse fréquence */
        s.az += (int32_t)v;
        s.ax += (int32_t)(v * 0.5);
    }
    s.ax += (int32_t)((urand() - 0.5) * 10);
    s.ay += (int32_t)((urand() - 0.5) * 10);
    s.az += (int32_t)((urand() - 0.5) * 10);
    return s;
}

static int gen_csv(const char *dir, int n)
{
    static const char *names[5] = {"zone1", "zone2", "zone3", "zone4", "autre"};
    char path[512];
    uint16_t out[TAP_SIGNAL_LEN];
    mkdir(dir, 0755);
    srand(1);
    for (int c = 0; c < 5; c++) {
        snprintf(path, sizeof(path), "%s/%s.csv", dir, names[c]);
        FILE *f = fopen(path, "w");
        if (!f) { perror(path); return 1; }
        for (int k = 0; k < n; k++) {
            tap_pre_t t;
            arm(&t);
            double amp = 300 + 500 * urand(), jit = 0.001 * urand();
            for (int i = 0; tap_pre_state(&t) != TAP_READY && i < 4 * TAP_SAMPLES; i++) {
                acc_sample_t s = tap_sample(c, i, amp, jit);
                tap_pre_push(&t, &s);
            }
            if (tap_pre_state(&t) != TAP_READY) continue;
            tap_pre_take(&t, out);
            for (int i = 0; i < TAP_SIGNAL_LEN; i++) fprintf(f, i ? ",%u" : "%u", out[i]);
            fputc('\n', f);
        }
        fclose(f);
        printf("%s : %d tapes\n", path, n);
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 3 && strcmp(argv[1], "--gen-csv") == 0) {
        return gen_csv(argv[2], argc >= 4 ? atoi(argv[3]) : 200);
    }
    test_scaling();
    test_arming();
    test_trigger();
    test_slow_drift();
    test_rearm();
    printf("\n%s (%d verification(s), %d echec(s))\n",
           failures ? "DES TESTS ECHOUENT" : "TOUS LES TESTS PASSENT", checks, failures);
    return failures ? 1 : 0;
}
