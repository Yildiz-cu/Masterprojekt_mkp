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
 *   ./mkp                  Run built-in example
 *   ./mkp <input_file>     Read instance from file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mkp.h"

/* ============================================================
 * Run a built-in example
 * ============================================================ */
static void run_example(void)
{
    printf("Running built-in example...\n\n");

    /* Example: 3 item types, 4 knapsacks */
    int d = 3, m = 4;

    int weights[] = {3, 5, 2};
    int profits[] = {4, 7, 3};
    int mults[]   = {5, 3, 8};
    int caps[]    = {10, 15, 8, 12};

    MKPInstance inst;
    mkp_init(&inst, d, m, weights, profits, mults, caps);

    MKPSolution sol = mkp_solve(&inst);
    mkp_print_solution(&inst, &sol);
}

/* ============================================================
 * Run a larger built-in example
 * ============================================================ */
static void run_large_example(void)
{
    printf("Running larger built-in example...\n\n");

    /* Example: 5 item types, 6 knapsacks */
    int d = 5, m = 6;

    int weights[] = {2, 4, 6, 3, 5};
    int profits[] = {3, 5, 8, 4, 7};
    int mults[]   = {10, 6, 4, 8, 5};
    int caps[]    = {12, 20, 15, 10, 18, 25};

    MKPInstance inst;
    mkp_init(&inst, d, m, weights, profits, mults, caps);

    MKPSolution sol = mkp_solve(&inst);
    mkp_print_solution(&inst, &sol);
}

/* ============================================================
 * Read instance from file
 * ============================================================ */
static int run_from_file(const char *filename)
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

    int weights[MAX_ITEM_TYPES], profits[MAX_ITEM_TYPES],
        mults[MAX_ITEM_TYPES];
    for (int i = 0; i < d; i++) {
        if (fscanf(f, "%d %d %d", &weights[i], &profits[i], &mults[i]) != 3) {
            fprintf(stderr, "Error: cannot read item type %d\n", i);
            fclose(f);
            return 1;
        }
    }

    int caps[MAX_KNAPSACKS];
    for (int j = 0; j < m; j++) {
        if (fscanf(f, "%d", &caps[j]) != 1) {
            fprintf(stderr, "Error: cannot read knapsack capacity %d\n", j);
            fclose(f);
            return 1;
        }
    }

    fclose(f);

    printf("Read instance from '%s': d=%d, m=%d\n\n", filename, d, m);

    MKPInstance inst;
    mkp_init(&inst, d, m, weights, profits, mults, caps);

    MKPSolution sol = mkp_solve(&inst);
    mkp_print_solution(&inst, &sol);

    return 0;
}

/* ============================================================
 * Main
 * ============================================================ */
int main(int argc, char *argv[])
{
    if (argc >= 2) {
        if (strcmp(argv[1], "--large") == 0) {
            run_large_example();
        } else {
            return run_from_file(argv[1]);
        }
    } else {
        run_example();
        printf("\n\n--- --- --- --- ---\n\n");
        run_large_example();
    }

    return 0;
}
