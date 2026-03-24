/*
 * mkp.h - Multiple Knapsack Problem (MKP) Solver
 *
 * Based on the approach of Lars Rohwedder for Q||Cmax,
 * extended with profits and bundles for MKP.
 *
 * Uses GLPK (GNU Linear Programming Kit) for solving the ILP.
 */

#ifndef MKP_H
#define MKP_H

#include <stdbool.h>

/* ============================================================
 * Constants
 * ============================================================ */
#define MAX_ITEM_TYPES   64
#define MAX_KNAPSACKS    256
#define MAX_CONFIGS      4096
#define MAX_KNAP_TYPES   256

/* ============================================================
 * Data structures
 * ============================================================ */

/* An item type: weight, profit, multiplicity */
typedef struct {
    int weight;
    int profit;
    int multiplicity;  /* n_i: how many items of this type exist */
} ItemType;

/* A knapsack with a given capacity */
typedef struct {
    int capacity;
    bool is_big;       /* true if capacity >= wmax^4 */
} Knapsack;

/* A configuration: how many items of each type (including dummy) are packed.
 * items[0..d-1] correspond to the d item types.
 * items[d]      corresponds to the dummy item type (weight 1, profit 0).
 */
typedef struct {
    int items[MAX_ITEM_TYPES + 1]; /* count per item type (including dummy) */
    int total_weight;
    int total_profit;
} Configuration;

/* A knapsack type: group of knapsacks with the same capacity */
typedef struct {
    int capacity;       /* the shared capacity */
    int count;          /* m(tau): how many knapsacks of this type */
    int num_configs;    /* number of valid configurations for this type */
    int config_start;   /* index into global config array */
} KnapsackType;

/* Full MKP instance */
typedef struct {
    int d;              /* number of item types */
    int m;              /* number of knapsacks */
    int wmax;           /* max item weight */
    ItemType items[MAX_ITEM_TYPES];
    Knapsack knapsacks[MAX_KNAPSACKS];

    /* Derived data */
    int pivot;          /* index of pivot item type 'a' */
    int num_big;        /* |B|: number of big knapsacks */
    int num_small;      /* |S|: number of small knapsacks */

    /* Dummy item type index = d (0-indexed) */
    int total_types;    /* d + 1 (including dummy) */

    /* Knapsack types (groups of knapsacks with equal capacity) */
    int num_knap_types;
    KnapsackType knap_types[MAX_KNAP_TYPES];

    /* All configurations across all knapsack types */
    int num_configs;
    Configuration configs[MAX_CONFIGS];
} MKPInstance;

/* Solution: assignment of items to knapsacks */
typedef struct {
    int assignment[MAX_KNAPSACKS][MAX_ITEM_TYPES + 1];
        /* assignment[k][i] = how many items of type i in knapsack k */
    int total_profit;
    bool feasible;
} MKPSolution;

/* ============================================================
 * Function prototypes
 * ============================================================ */

/**
 * Initialize an MKP instance from raw data.
 *   d         - number of item types
 *   m         - number of knapsacks
 *   weights   - array of d weights
 *   profits   - array of d profits
 *   mults     - array of d multiplicities
 *   caps      - array of m knapsack capacities
 */
void mkp_init(MKPInstance *inst, int d, int m,
              const int *weights, const int *profits,
              const int *mults, const int *caps);

/**
 * Partition knapsacks into big and small.
 * Big: capacity >= wmax^4.  Small: capacity < wmax^4.
 */
void mkp_partition_knapsacks(MKPInstance *inst);

/**
 * Find the pivot item type 'a'.
 * In an optimal solution, item type 'a' has at least |B| * wmax^2
 * copies placed in big knapsacks.
 * Heuristic: choose the item type with the largest (multiplicity * weight).
 */
void mkp_find_pivot(MKPInstance *inst);

/**
 * Group knapsacks by capacity to form knapsack types.
 */
void mkp_build_knapsack_types(MKPInstance *inst);

/**
 * Enumerate all valid configurations for each knapsack type.
 * A configuration for type tau lists how many items of each type
 * (including dummy items) fit exactly into a knapsack of that type's
 * capacity.
 */
void mkp_enumerate_configs(MKPInstance *inst);

/**
 * Build and solve the ILP using GLPK.
 * Returns the optimal MKP solution.
 */
MKPSolution mkp_solve_ilp(MKPInstance *inst);

/**
 * Convert ILP solution back to a concrete MKP assignment.
 */
MKPSolution mkp_construct_solution(MKPInstance *inst,
                                   double *y_vals, int num_y,
                                   double *b_vals, int num_b);

/**
 * Top-level solver: run the full algorithm pipeline.
 */
MKPSolution mkp_solve(MKPInstance *inst);

/**
 * Print the solution to stdout.
 */
void mkp_print_solution(const MKPInstance *inst, const MKPSolution *sol);

#endif /* MKP_H */
