/*
 * mkp.c - Multiple Knapsack Problem (MKP) Solver
 *
 * Implementation of the MKP algorithm based on the approach of
 * Lars Rohwedder for Q||Cmax, extended with profits and bundles.
 *
 * Uses GLPK (GNU Linear Programming Kit) for solving the ILP.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glpk.h>
#include "mkp.h"

/* ============================================================
 * Helper: compare ints for qsort
 * ============================================================ */
static int cmp_int(const void *a, const void *b) {
    return (*(const int *)a) - (*(const int *)b);
}

/* ============================================================
 * mkp_init
 * ============================================================ */
void mkp_init(MKPInstance *inst, int d, int m,
              const int *weights, const int *profits,
              const int *mults, const int *caps)
{
    memset(inst, 0, sizeof(MKPInstance));
    inst->d = d;
    inst->m = m;
    inst->total_types = d + 1; /* +1 for dummy item */

    inst->wmax = 0;
    for (int i = 0; i < d; i++) {
        inst->items[i].weight       = weights[i];
        inst->items[i].profit       = profits[i];
        inst->items[i].multiplicity = mults[i];
        if (weights[i] > inst->wmax)
            inst->wmax = weights[i];
    }

    /* Dummy item type at index d: weight 1, profit 0 */
    inst->items[d].weight       = 1;
    inst->items[d].profit       = 0;
    /* Upper bound on dummy items needed: (wmax - 1) * m */
    inst->items[d].multiplicity = (inst->wmax > 0) ? (inst->wmax - 1) * m : 0;

    for (int j = 0; j < m; j++) {
        inst->knapsacks[j].capacity = caps[j];
        inst->knapsacks[j].is_big   = false;
    }

    inst->pivot     = -1;
    inst->num_big   = 0;
    inst->num_small = 0;
}

/* ============================================================
 * mkp_partition_knapsacks
 *
 * Big knapsacks:  capacity >= wmax^4
 * Small knapsacks: capacity < wmax^4
 * ============================================================ */
void mkp_partition_knapsacks(MKPInstance *inst)
{
    long long threshold = (long long)inst->wmax * inst->wmax *
                          (long long)inst->wmax * inst->wmax; /* wmax^4 */

    inst->num_big   = 0;
    inst->num_small = 0;

    for (int j = 0; j < inst->m; j++) {
        if ((long long)inst->knapsacks[j].capacity >= threshold) {
            inst->knapsacks[j].is_big = true;
            inst->num_big++;
        } else {
            inst->knapsacks[j].is_big = false;
            inst->num_small++;
        }
    }

    printf("[MKP] wmax = %d, threshold (wmax^4) = %lld\n",
           inst->wmax, threshold);
    printf("[MKP] Big knapsacks: %d, Small knapsacks: %d\n",
           inst->num_big, inst->num_small);
}

/* ============================================================
 * mkp_find_pivot
 *
 * Choose pivot item type 'a' such that it has at least
 * |B| * wmax^2 items in big knapsacks in an optimal solution.
 * Heuristic: item type with largest (multiplicity * weight).
 * ============================================================ */
void mkp_find_pivot(MKPInstance *inst)
{
    long long best_score = -1;
    int best_idx = 0;

    for (int i = 0; i < inst->d; i++) {
        long long score = (long long)inst->items[i].multiplicity *
                          inst->items[i].weight;
        if (score > best_score) {
            best_score = score;
            best_idx   = i;
        }
    }

    inst->pivot = best_idx;
    printf("[MKP] Pivot item type: %d (weight=%d, profit=%d, mult=%d)\n",
           best_idx,
           inst->items[best_idx].weight,
           inst->items[best_idx].profit,
           inst->items[best_idx].multiplicity);
}

/* ============================================================
 * mkp_build_knapsack_types
 *
 * Group knapsacks by capacity. Each group is a "knapsack type".
 * ============================================================ */
void mkp_build_knapsack_types(MKPInstance *inst)
{
    /* Sort capacities to group them */
    int sorted_caps[MAX_KNAPSACKS];
    for (int j = 0; j < inst->m; j++)
        sorted_caps[j] = inst->knapsacks[j].capacity;
    qsort(sorted_caps, inst->m, sizeof(int), cmp_int);

    inst->num_knap_types = 0;
    int j = 0;
    while (j < inst->m) {
        int cap   = sorted_caps[j];
        int count = 0;
        while (j < inst->m && sorted_caps[j] == cap) {
            count++;
            j++;
        }
        int t = inst->num_knap_types++;
        inst->knap_types[t].capacity    = cap;
        inst->knap_types[t].count       = count;
        inst->knap_types[t].num_configs = 0;
        inst->knap_types[t].config_start = 0;
    }

    printf("[MKP] Knapsack types: %d\n", inst->num_knap_types);
    for (int t = 0; t < inst->num_knap_types; t++) {
        printf("  Type %d: capacity=%d, count=%d\n",
               t, inst->knap_types[t].capacity, inst->knap_types[t].count);
    }
}

/* ============================================================
 * Config enumeration (recursive helper)
 * ============================================================ */
static void enumerate_configs_rec(MKPInstance *inst, int type_idx,
                                  int item_idx, int remaining_cap,
                                  int current_items[],
                                  int current_profit)
{
    int cap = inst->knap_types[type_idx].capacity;

    if (item_idx == inst->total_types) {
        /* Fill remaining capacity with dummy items */
        if (remaining_cap >= 0) {
            if (inst->num_configs >= MAX_CONFIGS) {
                return; /* overflow protection */
            }
            Configuration *cfg = &inst->configs[inst->num_configs];
            memcpy(cfg->items, current_items,
                   sizeof(int) * inst->total_types);
            /* Add dummy items to fill exactly */
            cfg->items[inst->d] += remaining_cap;
            cfg->total_weight = cap;
            cfg->total_profit = current_profit;
            inst->knap_types[type_idx].num_configs++;
            inst->num_configs++;
        }
        return;
    }

    /* For item type item_idx, try 0..max_count copies */
    int w = inst->items[item_idx].weight;
    int p = inst->items[item_idx].profit;
    int n = inst->items[item_idx].multiplicity;

    /* For dummy items (item_idx == d), we handle them at the leaf */
    if (item_idx == inst->d) {
        enumerate_configs_rec(inst, type_idx, item_idx + 1,
                              remaining_cap, current_items, current_profit);
        return;
    }

    int max_by_cap  = (w > 0) ? remaining_cap / w : 0;
    int max_count   = (max_by_cap < n) ? max_by_cap : n;

    /* Limit enumeration to avoid explosion: at most wmax^2 items of
     * any single type in a configuration (based on the algorithm).
     * Also limit total configs. */
    if (max_count > 50 && inst->d > 3) max_count = 50;

    for (int k = 0; k <= max_count; k++) {
        current_items[item_idx] = k;
        enumerate_configs_rec(inst, type_idx, item_idx + 1,
                              remaining_cap - k * w,
                              current_items, current_profit + k * p);

        /* Stop if we've generated too many configs */
        if (inst->num_configs >= MAX_CONFIGS - 1)
            return;
    }
    current_items[item_idx] = 0; /* reset */
}

/* ============================================================
 * mkp_enumerate_configs
 * ============================================================ */
void mkp_enumerate_configs(MKPInstance *inst)
{
    inst->num_configs = 0;

    for (int t = 0; t < inst->num_knap_types; t++) {
        inst->knap_types[t].config_start = inst->num_configs;
        inst->knap_types[t].num_configs  = 0;

        int current_items[MAX_ITEM_TYPES + 1];
        memset(current_items, 0, sizeof(current_items));

        enumerate_configs_rec(inst, t, 0,
                              inst->knap_types[t].capacity,
                              current_items, 0);

        printf("[MKP] Type %d (cap=%d): %d configurations\n",
               t, inst->knap_types[t].capacity,
               inst->knap_types[t].num_configs);
    }

    printf("[MKP] Total configurations: %d\n", inst->num_configs);
}

/* ============================================================
 * mkp_solve_ilp
 *
 * Build and solve the ILP formulation using GLPK.
 *
 * Variables:
 *   y_{tau,C}  for each knapsack type tau and config C in C(tau)
 *   b_i        for each item type i in [d+1] \ {a}
 *   b_a        for the pivot item type
 *
 * Objective: maximize
 *   sum_{tau} sum_{C in C(tau)} y_{tau,C} * P(C)
 *   + sum_{i != a} w_a * p_i * b_i
 *   + b_a * p_a
 *
 * Constraints:
 *   (1) sum_{C in C(tau)} y_{tau,C} = m(tau)        for all tau
 *   (2) sum_{tau} sum_C C_a * y_{tau,C} + w_a * b_i <= n_i
 *                                                    for all i != a
 *   (3) sum_{tau} sum_C C_a * y_{tau,C} + b_a <= n_a - |B|*wmax^2
 *   (4) sum_{tau} sum_C y_{tau,C} * W(C) <= sum C_j - |B|*wmax^2*w_a
 *   (5) y_{tau,C} >= 0, integer
 *   (6) b_i >= 0, integer
 * ============================================================ */
MKPSolution mkp_solve_ilp(MKPInstance *inst)
{
    MKPSolution sol;
    memset(&sol, 0, sizeof(sol));
    sol.feasible = false;

    int a = inst->pivot;
    int wa = inst->items[a].weight;
    int pa = inst->items[a].profit;
    long long Bw2 = (long long)inst->num_big * inst->wmax * inst->wmax;

    /* Count variables */
    int num_y = inst->num_configs;   /* one y per (type, config) pair */
    int num_b = inst->total_types;   /* b_i for each item type incl dummy */
    int total_vars = num_y + num_b;

    /* Count constraints */
    /* (1) one per knapsack type */
    int num_eq = inst->num_knap_types;
    /* (2) one per item type i != a (including dummy) */
    int num_item_cstr = inst->total_types - 1;
    /* (3) one for pivot item */
    /* (4) one for total weight */
    int total_cstr = num_eq + num_item_cstr + 1 + 1;

    printf("[MKP] ILP: %d variables, %d constraints\n",
           total_vars, total_cstr);

    /* Create GLPK problem */
    glp_prob *lp = glp_create_prob();
    glp_set_prob_name(lp, "MKP");
    glp_set_obj_dir(lp, GLP_MAX);

    /* Add columns (variables) */
    glp_add_cols(lp, total_vars);

    /* y_{tau,C} variables: columns 1..num_y */
    for (int t = 0; t < inst->num_knap_types; t++) {
        int start = inst->knap_types[t].config_start;
        int nc    = inst->knap_types[t].num_configs;
        for (int c = 0; c < nc; c++) {
            int col = start + c + 1; /* 1-indexed */
            char name[64];
            snprintf(name, sizeof(name), "y_%d_%d", t, c);
            glp_set_col_name(lp, col, name);
            glp_set_col_kind(lp, col, GLP_IV); /* integer */
            glp_set_col_bnds(lp, col, GLP_LO, 0.0, 0.0); /* >= 0 */

            /* Objective coefficient: P(C) */
            Configuration *cfg = &inst->configs[start + c];
            glp_set_obj_coef(lp, col, (double)cfg->total_profit);
        }
    }

    /* b_i variables: columns (num_y+1)..(num_y+num_b) */
    for (int i = 0; i < inst->total_types; i++) {
        int col = num_y + i + 1;
        char name[64];
        snprintf(name, sizeof(name), "b_%d", i);
        glp_set_col_name(lp, col, name);
        glp_set_col_kind(lp, col, GLP_IV);
        glp_set_col_bnds(lp, col, GLP_LO, 0.0, 0.0);

        /* Objective coefficient for bundles */
        if (i == a) {
            /* b_a * p_a */
            glp_set_obj_coef(lp, col, (double)pa);
        } else {
            /* w_a * p_i * b_i  (bundle of w_a items of type i) */
            glp_set_obj_coef(lp, col, (double)(wa * inst->items[i].profit));
        }
    }

    /* Add rows (constraints) */
    glp_add_rows(lp, total_cstr);

    /* We'll build the constraint matrix in sparse format */
    /* Estimate max non-zeros */
    int max_nz = total_vars * total_cstr; /* upper bound */
    if (max_nz > 1000000) max_nz = 1000000;
    int *ia  = (int *)malloc((max_nz + 1) * sizeof(int));
    int *ja  = (int *)malloc((max_nz + 1) * sizeof(int));
    double *ar = (double *)malloc((max_nz + 1) * sizeof(double));
    int nz = 0;

    int row = 0;

    /* ---- Constraint (1): sum_{C in C(tau)} y_{tau,C} = m(tau) ---- */
    for (int t = 0; t < inst->num_knap_types; t++) {
        row++;
        char name[64];
        snprintf(name, sizeof(name), "type_eq_%d", t);
        glp_set_row_name(lp, row, name);
        glp_set_row_bnds(lp, row, GLP_FX,
                         (double)inst->knap_types[t].count,
                         (double)inst->knap_types[t].count);

        int start = inst->knap_types[t].config_start;
        int nc    = inst->knap_types[t].num_configs;
        for (int c = 0; c < nc; c++) {
            nz++;
            ia[nz] = row;
            ja[nz] = start + c + 1;
            ar[nz] = 1.0;
        }
    }

    /* ---- Constraint (2): item usage for i != a ---- */
    for (int i = 0; i < inst->total_types; i++) {
        if (i == a) continue;

        row++;
        char name[64];
        snprintf(name, sizeof(name), "item_%d", i);
        glp_set_row_name(lp, row, name);
        glp_set_row_bnds(lp, row, GLP_UP, 0.0,
                         (double)inst->items[i].multiplicity);

        /* sum_{tau} sum_C C_i * y_{tau,C} */
        for (int t = 0; t < inst->num_knap_types; t++) {
            int start = inst->knap_types[t].config_start;
            int nc    = inst->knap_types[t].num_configs;
            for (int c = 0; c < nc; c++) {
                int cnt = inst->configs[start + c].items[i];
                if (cnt != 0) {
                    nz++;
                    ia[nz] = row;
                    ja[nz] = start + c + 1;
                    ar[nz] = (double)cnt;
                }
            }
        }

        /* + w_a * b_i */
        nz++;
        ia[nz] = row;
        ja[nz] = num_y + i + 1;
        ar[nz] = (double)wa;
    }

    /* ---- Constraint (3): pivot item ---- */
    /* sum_{tau} sum_C C_a * y_{tau,C} + b_a <= n_a - |B|*wmax^2 */
    row++;
    {
        char name[64];
        snprintf(name, sizeof(name), "pivot");
        glp_set_row_name(lp, row, name);
        double rhs = (double)(inst->items[a].multiplicity - Bw2);
        if (rhs < 0) rhs = 0;
        glp_set_row_bnds(lp, row, GLP_UP, 0.0, rhs);

        for (int t = 0; t < inst->num_knap_types; t++) {
            int start = inst->knap_types[t].config_start;
            int nc    = inst->knap_types[t].num_configs;
            for (int c = 0; c < nc; c++) {
                int cnt = inst->configs[start + c].items[a];
                if (cnt != 0) {
                    nz++;
                    ia[nz] = row;
                    ja[nz] = start + c + 1;
                    ar[nz] = (double)cnt;
                }
            }
        }

        /* + b_a */
        nz++;
        ia[nz] = row;
        ja[nz] = num_y + a + 1;
        ar[nz] = 1.0;
    }

    /* ---- Constraint (4): total weight ---- */
    /* sum_{tau} sum_C y_{tau,C} * W(C) <= sum C_j - |B|*wmax^2*w_a */
    row++;
    {
        char name[64];
        snprintf(name, sizeof(name), "total_weight");
        glp_set_row_name(lp, row, name);

        long long total_cap = 0;
        for (int j = 0; j < inst->m; j++)
            total_cap += inst->knapsacks[j].capacity;
        double rhs = (double)(total_cap - Bw2 * wa);
        if (rhs < 0) rhs = 0;
        glp_set_row_bnds(lp, row, GLP_UP, 0.0, rhs);

        for (int t = 0; t < inst->num_knap_types; t++) {
            int start = inst->knap_types[t].config_start;
            int nc    = inst->knap_types[t].num_configs;
            for (int c = 0; c < nc; c++) {
                int w = inst->configs[start + c].total_weight;
                if (w != 0) {
                    nz++;
                    ia[nz] = row;
                    ja[nz] = start + c + 1;
                    ar[nz] = (double)w;
                }
            }
        }

        /* Bundle weights: for i != a, each bundle adds w_a * w_i weight
         * For pivot a, each bundle adds w_a weight */
        for (int i = 0; i < inst->total_types; i++) {
            double bw;
            if (i == a) {
                bw = (double)wa;
            } else {
                bw = (double)(wa * inst->items[i].weight);
            }
            if (bw != 0) {
                nz++;
                ia[nz] = row;
                ja[nz] = num_y + i + 1;
                ar[nz] = bw;
            }
        }
    }

    /* Load the constraint matrix */
    glp_load_matrix(lp, nz, ia, ja, ar);

    /* Solve the ILP */
    glp_iocp parm;
    glp_init_iocp(&parm);
    parm.presolve = GLP_ON;
    parm.msg_lev  = GLP_MSG_ON;
    parm.tm_lim   = 60000; /* 60 second time limit */

    printf("[MKP] Solving ILP with GLPK...\n");
    int ret = glp_intopt(lp, &parm);

    if (ret != 0) {
        printf("[MKP] GLPK solver returned error code %d\n", ret);
        /* Try relaxed LP first then branch-and-bound */
        glp_simplex(lp, NULL);
        ret = glp_intopt(lp, &parm);
    }

    int status = glp_mip_status(lp);
    if (status == GLP_OPT || status == GLP_FEAS) {
        printf("[MKP] ILP solved. Status: %s, Objective: %.0f\n",
               (status == GLP_OPT) ? "OPTIMAL" : "FEASIBLE",
               glp_mip_obj_val(lp));

        /* Extract y values */
        double *y_vals = (double *)calloc(num_y, sizeof(double));
        for (int c = 0; c < num_y; c++) {
            y_vals[c] = glp_mip_col_val(lp, c + 1);
        }

        /* Extract b values */
        double *b_vals = (double *)calloc(num_b, sizeof(double));
        for (int i = 0; i < num_b; i++) {
            b_vals[i] = glp_mip_col_val(lp, num_y + i + 1);
        }

        sol = mkp_construct_solution(inst, y_vals, num_y, b_vals, num_b);

        free(y_vals);
        free(b_vals);
    } else {
        printf("[MKP] ILP infeasible or no solution found. Status: %d\n",
               status);
    }

    /* Cleanup */
    free(ia);
    free(ja);
    free(ar);
    glp_delete_prob(lp);

    return sol;
}

/* ============================================================
 * mkp_construct_solution
 *
 * Given the ILP solution (y and b values), construct a concrete
 * assignment of items to knapsacks.
 * ============================================================ */
MKPSolution mkp_construct_solution(MKPInstance *inst,
                                   double *y_vals, int num_y __attribute__((unused)),
                                   double *b_vals, int num_b __attribute__((unused)))
{
    MKPSolution sol;
    memset(&sol, 0, sizeof(sol));
    sol.feasible = true;
    sol.total_profit = 0;

    int a  = inst->pivot;
    int wa = inst->items[a].weight;

    /* Track how many items of each type are remaining to assign */
    int remaining[MAX_ITEM_TYPES + 1];
    for (int i = 0; i < inst->total_types; i++)
        remaining[i] = inst->items[i].multiplicity;

    /* Phase 1: Assign configurations to knapsacks.
     * For each knapsack type, distribute the chosen configurations
     * among the knapsacks of that type.
     */
    int knapsack_idx = 0;

    /* We need to iterate knapsacks in the same order as types.
     * First, sort knapsacks by capacity. */
    int ks_order[MAX_KNAPSACKS];
    for (int j = 0; j < inst->m; j++) ks_order[j] = j;

    /* Simple insertion sort by capacity */
    for (int i = 1; i < inst->m; i++) {
        int key = ks_order[i];
        int j = i - 1;
        while (j >= 0 &&
               inst->knapsacks[ks_order[j]].capacity >
               inst->knapsacks[key].capacity) {
            ks_order[j + 1] = ks_order[j];
            j--;
        }
        ks_order[j + 1] = key;
    }

    knapsack_idx = 0;
    for (int t = 0; t < inst->num_knap_types; t++) {
        int start = inst->knap_types[t].config_start;
        int nc    = inst->knap_types[t].num_configs;

        for (int c = 0; c < nc; c++) {
            int y_count = (int)(y_vals[start + c] + 0.5);
            Configuration *cfg = &inst->configs[start + c];

            for (int rep = 0; rep < y_count && knapsack_idx < inst->m;
                 rep++) {
                int kj = ks_order[knapsack_idx];

                for (int i = 0; i < inst->total_types; i++) {
                    int cnt = cfg->items[i];
                    sol.assignment[kj][i] = cnt;
                    remaining[i] -= cnt;
                    sol.total_profit += cnt * inst->items[i].profit;
                }

                knapsack_idx++;
            }
        }
    }

    /* Phase 2: Assign bundle items.
     * For item types i != a, each bundle places w_a items of type i
     * into remaining capacity of knapsacks.
     * For the pivot type a, each bundle places 1 item.
     */
    for (int i = 0; i < inst->total_types; i++) {
        int b_count = (int)(b_vals[i] + 0.5);
        if (b_count <= 0) continue;

        int items_per_bundle = (i == a) ? 1 : wa;
        int total_items = b_count * items_per_bundle;

        /* Distribute these items greedily to knapsacks with remaining capacity */
        for (int j = 0; j < inst->m && total_items > 0; j++) {
            int kj = ks_order[j];
            /* Compute used capacity */
            int used = 0;
            for (int ii = 0; ii < inst->total_types; ii++)
                used += sol.assignment[kj][ii] * inst->items[ii].weight;

            int cap_left = inst->knapsacks[kj].capacity - used;
            int wi = inst->items[i].weight;

            while (cap_left >= wi && total_items > 0) {
                sol.assignment[kj][i]++;
                cap_left -= wi;
                total_items--;
                sol.total_profit += inst->items[i].profit;
            }
        }
    }

    return sol;
}

/* ============================================================
 * mkp_solve  (top-level pipeline)
 * ============================================================ */
MKPSolution mkp_solve(MKPInstance *inst)
{
    printf("========================================\n");
    printf("  Multiple Knapsack Problem Solver\n");
    printf("  (Rohwedder-based approach with ILP)\n");
    printf("========================================\n\n");

    printf("[MKP] Instance: %d item types, %d knapsacks\n",
           inst->d, inst->m);

    /* Step 1: Partition knapsacks */
    mkp_partition_knapsacks(inst);

    /* Step 2: Find pivot item type */
    mkp_find_pivot(inst);

    /* Step 3: Build knapsack types */
    mkp_build_knapsack_types(inst);

    /* Step 4: Enumerate configurations */
    mkp_enumerate_configs(inst);

    /* Step 5: Build and solve ILP */
    MKPSolution sol = mkp_solve_ilp(inst);

    return sol;
}

/* ============================================================
 * mkp_print_solution
 * ============================================================ */
void mkp_print_solution(const MKPInstance *inst, const MKPSolution *sol)
{
    printf("\n========================================\n");
    printf("  MKP Solution\n");
    printf("========================================\n");

    if (!sol->feasible) {
        printf("No feasible solution found.\n");
        return;
    }

    printf("Total profit: %d\n\n", sol->total_profit);

    for (int j = 0; j < inst->m; j++) {
        int used = 0;
        int profit = 0;
        printf("Knapsack %d (capacity=%d):", j, inst->knapsacks[j].capacity);
        bool has_items = false;
        for (int i = 0; i < inst->total_types; i++) {
            int cnt = sol->assignment[j][i];
            if (cnt > 0) {
                const char *label = (i < inst->d) ? "" : " [dummy]";
                printf(" type%d(w=%d,p=%d)x%d%s",
                       i, inst->items[i].weight, inst->items[i].profit,
                       cnt, label);
                used   += cnt * inst->items[i].weight;
                profit += cnt * inst->items[i].profit;
                has_items = true;
            }
        }
        if (!has_items) printf(" (empty)");
        printf("  | used=%d, profit=%d\n", used, profit);
    }
}
