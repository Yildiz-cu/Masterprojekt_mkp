/*
 * main.c - Driver program for the MKP solver
 *
 * Reads an MKP instance from a file or uses built-in examples,
 * then solves it using the ILP-based algorithm.
 *
 * Input file format:
 *   Line 1: d m           (number of item types, number of knapsacks)
 *   Next d lines: w_i p_i n_i   (weight, profit, multiplicity)
 *   Next m lines: C_j           (knapsack capacity)
 *
 * Usage:
 *   ./mkp                         Run built-in examples (both solvers)
 *   ./mkp <input_file>            Read instance from file (both solvers)
 *   ./mkp --direct <input_file>   Solve with direct (standard) ILP only
 *   ./mkp --rohwedder <input_file> Solve with Rohwedder config ILP only
 *   ./mkp --large                 Run larger built-in example
 *   ./mkp --benchmark <input_file> Run both solvers and print timing summary
 */

#define _POSIX_C_SOURCE 199309L  /* for clock_gettime */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mkp.h"

/* Solver mode enumeration */
typedef enum {
    MODE_BOTH,       /* Run both solvers (default) */
    MODE_DIRECT,     /* Standard assignment / direct config ILP only */
    MODE_ROHWEDDER,  /* Rohwedder pivot-based config ILP only */
    MODE_BENCHMARK   /* Run both and print timing comparison */
} SolverMode;

/* ============================================================
 * Read instance from file into arrays
 * Returns 0 on success, 1 on error
 * ============================================================ */
static int read_instance(const char *filename,
                         int *d_out, int *m_out,
                         int weights[], int profits[], int mults[],
                         int caps[])
{
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Error: cannot open file '%s'\n", filename);
        return 1;
    }

    int d, m;
    if (fscanf(f, "%d %d", &d, &m) != 2) {
        fprintf(stderr, "Error: cannot read d and m\n");
        fclose(f);
        return 1;
    }

    if (d <= 0 || d > MAX_ITEM_TYPES) {
        fprintf(stderr, "Error: d=%d out of range [1, %d]\n",
                d, MAX_ITEM_TYPES);
        fclose(f);
        return 1;
    }
    if (m <= 0 || m > MAX_KNAPSACKS) {
        fprintf(stderr, "Error: m=%d out of range [1, %d]\n",
                m, MAX_KNAPSACKS);
        fclose(f);
        return 1;
    }

    for (int i = 0; i < d; i++) {
        if (fscanf(f, "%d %d %d", &weights[i], &profits[i], &mults[i]) != 3) {
            fprintf(stderr, "Error: cannot read item type %d\n", i);
            fclose(f);
            return 1;
        }
    }

    for (int j = 0; j < m; j++) {
        if (fscanf(f, "%d", &caps[j]) != 1) {
            fprintf(stderr, "Error: cannot read knapsack capacity %d\n", j);
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    *d_out = d;
    *m_out = m;
    return 0;
}

/* ============================================================
 * Run the DIRECT (standard configuration) ILP solver
 * ============================================================ */
static MKPSolution run_direct_solver(int d, int m,
                                     const int *weights,
                                     const int *profits,
                                     const int *mults,
                                     const int *caps,
                                     double *elapsed_sec)
{
    MKPInstance inst;
    mkp_init(&inst, d, m, weights, profits, mults, caps);

    printf("========================================\n");
    printf("  DIRECT (Standard) Configuration ILP\n");
    printf("========================================\n\n");
    printf("[DIRECT] Instance: %d item types, %d knapsacks\n", d, m);

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    /* Partition + build types + enumerate configs */
    mkp_partition_knapsacks(&inst);
    mkp_build_knapsack_types(&inst);

    inst.pivot = 0; /* Not used meaningfully in direct ILP */
    inst.num_configs = 0;
    for (int t = 0; t < inst.num_knap_types; t++) {
        inst.knap_types[t].num_configs = 0;
        inst.knap_types[t].config_start = 0;
    }
    mkp_enumerate_configs(&inst);

    MKPSolution sol = mkp_solve_direct_ilp(&inst);

    clock_gettime(CLOCK_MONOTONIC, &t_end);
    *elapsed_sec = (t_end.tv_sec - t_start.tv_sec)
                 + (t_end.tv_nsec - t_start.tv_nsec) / 1e9;

    mkp_print_solution(&inst, &sol);
    printf("\n[DIRECT] Wall-clock time: %.6f seconds\n", *elapsed_sec);

    return sol;
}

/* ============================================================
 * Run the ROHWEDDER (pivot-based configuration) ILP solver
 * ============================================================ */
static MKPSolution run_rohwedder_solver(int d, int m,
                                        const int *weights,
                                        const int *profits,
                                        const int *mults,
                                        const int *caps,
                                        double *elapsed_sec)
{
    MKPInstance inst;
    mkp_init(&inst, d, m, weights, profits, mults, caps);

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    MKPSolution sol = mkp_solve(&inst);

    clock_gettime(CLOCK_MONOTONIC, &t_end);
    *elapsed_sec = (t_end.tv_sec - t_start.tv_sec)
                 + (t_end.tv_nsec - t_start.tv_nsec) / 1e9;

    mkp_print_solution(&inst, &sol);
    printf("\n[ROHWEDDER] Wall-clock time: %.6f seconds\n", *elapsed_sec);

    return sol;
}

/* ============================================================
 * Run a built-in example
 * ============================================================ */
static void run_example(SolverMode mode)
{
    printf("Running built-in example...\n\n");

    int d = 3, m = 4;
    int weights[] = {3, 5, 2};
    int profits[] = {4, 7, 3};
    int mults[]   = {5, 3, 8};
    int caps[]    = {10, 15, 8, 12};

    double t_direct = 0.0, t_rohwedder = 0.0;

    if (mode == MODE_DIRECT || mode == MODE_BOTH || mode == MODE_BENCHMARK) {
        run_direct_solver(d, m, weights, profits, mults, caps, &t_direct);
        printf("\n");
    }

    if (mode == MODE_ROHWEDDER || mode == MODE_BOTH || mode == MODE_BENCHMARK) {
        run_rohwedder_solver(d, m, weights, profits, mults, caps, &t_rohwedder);
        printf("\n");
    }

    if (mode == MODE_BENCHMARK || mode == MODE_BOTH) {
        printf("\n========================================\n");
        printf("  TIMING COMPARISON\n");
        printf("========================================\n");
        printf("  Direct (Standard) ILP:   %.6f s\n", t_direct);
        printf("  Rohwedder (Pivot) ILP:   %.6f s\n", t_rohwedder);
        if (t_direct > 0 && t_rohwedder > 0) {
            printf("  Speedup (Direct/Rohwedder): %.2fx\n",
                   t_direct / t_rohwedder);
        }
        printf("========================================\n");
    }
}

/* ============================================================
 * Run a larger built-in example
 * ============================================================ */
static void run_large_example(SolverMode mode)
{
    printf("Running larger built-in example...\n\n");

    int d = 5, m = 6;
    int weights[] = {2, 4, 6, 3, 5};
    int profits[] = {3, 5, 8, 4, 7};
    int mults[]   = {10, 6, 4, 8, 5};
    int caps[]    = {12, 20, 15, 10, 18, 25};

    double t_direct = 0.0, t_rohwedder = 0.0;

    if (mode == MODE_DIRECT || mode == MODE_BOTH || mode == MODE_BENCHMARK) {
        run_direct_solver(d, m, weights, profits, mults, caps, &t_direct);
        printf("\n");
    }

    if (mode == MODE_ROHWEDDER || mode == MODE_BOTH || mode == MODE_BENCHMARK) {
        run_rohwedder_solver(d, m, weights, profits, mults, caps, &t_rohwedder);
        printf("\n");
    }

    if (mode == MODE_BENCHMARK || mode == MODE_BOTH) {
        printf("\n========================================\n");
        printf("  TIMING COMPARISON\n");
        printf("========================================\n");
        printf("  Direct (Standard) ILP:   %.6f s\n", t_direct);
        printf("  Rohwedder (Pivot) ILP:   %.6f s\n", t_rohwedder);
        if (t_direct > 0 && t_rohwedder > 0) {
            printf("  Speedup (Direct/Rohwedder): %.2fx\n",
                   t_direct / t_rohwedder);
        }
        printf("========================================\n");
    }
}

/* ============================================================
 * Run from file with given mode
 * ============================================================ */
static int run_from_file(const char *filename, SolverMode mode)
{
    int d, m;
    int weights[MAX_ITEM_TYPES], profits[MAX_ITEM_TYPES],
        mults[MAX_ITEM_TYPES], caps[MAX_KNAPSACKS];

    if (read_instance(filename, &d, &m, weights, profits, mults, caps) != 0)
        return 1;

    printf("Read instance from '%s': d=%d, m=%d\n\n", filename, d, m);

    double t_direct = 0.0, t_rohwedder = 0.0;

    if (mode == MODE_DIRECT || mode == MODE_BOTH || mode == MODE_BENCHMARK) {
        run_direct_solver(d, m, weights, profits, mults, caps, &t_direct);
        printf("\n");
    }

    if (mode == MODE_ROHWEDDER || mode == MODE_BOTH || mode == MODE_BENCHMARK) {
        run_rohwedder_solver(d, m, weights, profits, mults, caps, &t_rohwedder);
        printf("\n");
    }

    if (mode == MODE_BENCHMARK || mode == MODE_BOTH) {
        printf("\n========================================\n");
        printf("  TIMING COMPARISON\n");
        printf("========================================\n");
        printf("  Direct (Standard) ILP:   %.6f s\n", t_direct);
        printf("  Rohwedder (Pivot) ILP:   %.6f s\n", t_rohwedder);
        if (t_direct > 0 && t_rohwedder > 0) {
            printf("  Speedup (Direct/Rohwedder): %.2fx\n",
                   t_direct / t_rohwedder);
        }
        printf("========================================\n");
    }

    return 0;
}

/* ============================================================
 * Print usage information
 * ============================================================ */
static void print_usage(const char *progname)
{
    printf("Usage:\n");
    printf("  %s                              Run built-in examples (both solvers)\n", progname);
    printf("  %s <input_file>                 Solve from file (both solvers)\n", progname);
    printf("  %s --direct <input_file>        Solve with direct (standard) ILP only\n", progname);
    printf("  %s --rohwedder <input_file>     Solve with Rohwedder config ILP only\n", progname);
    printf("  %s --benchmark <input_file>     Run both solvers and compare timing\n", progname);
    printf("  %s --large                      Run larger built-in example\n", progname);
    printf("  %s --help                       Show this help message\n", progname);
}

/* ============================================================
 * Main
 * ============================================================ */
int main(int argc, char *argv[])
{
    if (argc == 1) {
        /* No arguments: run both built-in examples with both solvers */
        run_example(MODE_BOTH);
        printf("\n\n--- --- --- --- ---\n\n");
        run_large_example(MODE_BOTH);
        return 0;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "--large") == 0) {
        SolverMode mode = MODE_BOTH;
        if (argc >= 3) {
            if (strcmp(argv[2], "--direct") == 0)     mode = MODE_DIRECT;
            if (strcmp(argv[2], "--rohwedder") == 0)   mode = MODE_ROHWEDDER;
            if (strcmp(argv[2], "--benchmark") == 0)   mode = MODE_BENCHMARK;
        }
        run_large_example(mode);
        return 0;
    }

    if (strcmp(argv[1], "--direct") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: --direct requires an input file.\n");
            print_usage(argv[0]);
            return 1;
        }
        return run_from_file(argv[2], MODE_DIRECT);
    }

    if (strcmp(argv[1], "--rohwedder") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: --rohwedder requires an input file.\n");
            print_usage(argv[0]);
            return 1;
        }
        return run_from_file(argv[2], MODE_ROHWEDDER);
    }

    if (strcmp(argv[1], "--benchmark") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: --benchmark requires an input file.\n");
            print_usage(argv[0]);
            return 1;
        }
        return run_from_file(argv[2], MODE_BENCHMARK);
    }

    /* Default: treat argument as input file, run both solvers */
    return run_from_file(argv[1], MODE_BOTH);
}

