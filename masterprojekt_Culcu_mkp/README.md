# Multiple Knapsack Problem (MKP) Solver

A C implementation of the Multiple Knapsack Problem solver based on the approach
of Lars Rohwedder for Q||Cmax, extended with profits and bundles.

Uses **GLPK (GNU Linear Programming Kit)** as the ILP solver library.

## Problem Description

Given:
- **d** different item types, each with weight `w_i`, profit `p_i`, and multiplicity `n_i`
- **m** knapsacks with capacities `C_1, ..., C_m`

Goal: Assign items to knapsacks to maximize total profit, without exceeding
any knapsack's capacity or any item type's multiplicity.

## Algorithm Overview

1. **Partition knapsacks** into big (capacity ≥ w_max⁴) and small (capacity < w_max⁴)
2. **Find pivot item type** `a` — the type with the largest total weight contribution
3. **Group knapsacks** by capacity into knapsack types
4. **Enumerate configurations** for each knapsack type (feasible item combinations)
5. **Introduce dummy items** (weight 1, profit 0) to allow exact capacity filling
6. **Build ILP formulation** with:
   - Variables `y_{τ,C}`: how often configuration C is used for knapsack type τ
   - Variables `b_i`: number of bundles used per item type
   - Constraints ensuring type counts, item limits, pivot bounds, and total weight
7. **Solve ILP** using GLPK
8. **Construct solution** — map ILP solution back to concrete item-to-knapsack assignments

## Dependencies

- **GLPK** (GNU Linear Programming Kit) — used for solving the Integer Linear Program
- **GCC** (or any C11-compatible compiler)
- **Make**

### Installing GLPK

```bash
# Ubuntu/Debian
sudo apt-get install libglpk-dev

# macOS (Homebrew)
brew install glpk

# Fedora/RHEL
sudo dnf install glpk-devel
```

## Building

```bash
make
```

## Usage

### Run built-in examples
```bash
./mkp
```

### Run with input file
```bash
./mkp example_input.txt
```

### Run the larger built-in example
```bash
./mkp --large
```

## Input File Format

```
d m
w_1 p_1 n_1
w_2 p_2 n_2
...
w_d p_d n_d
C_1 C_2 ... C_m
```

Where:
- `d` = number of item types
- `m` = number of knapsacks
- `w_i p_i n_i` = weight, profit, multiplicity of item type i
- `C_j` = capacity of knapsack j

### Example (`example_input.txt`)
```
3 4
3 4 5
5 7 3
2 3 8
10 15 8 12
```

This defines 3 item types and 4 knapsacks:
- Type 0: weight=3, profit=4, 5 available
- Type 1: weight=5, profit=7, 3 available
- Type 2: weight=2, profit=3, 8 available
- Knapsacks with capacities: 10, 15, 8, 12

## Project Structure

```
mkp-c/
├── mkp.h              # Header: data structures and function prototypes
├── mkp.c              # Core algorithm and ILP formulation
├── main.c             # Driver program with I/O and examples
├── Makefile           # Build system
├── example_input.txt  # Sample input file
└── README.md          # This file
```

## Output

The solver prints:
1. Instance statistics (item types, knapsacks, wmax)
2. Knapsack partitioning (big vs small)
3. Pivot item selection
4. Configuration enumeration counts
5. ILP solving progress (from GLPK)
6. Final solution: items assigned to each knapsack with weights and profits

## Changes made to previous version
**Problem identified**: The pivot selection heuristic (choosing the item type with the largest `multiplicity × weight`) does not always work. The test instance `test.txt` demonstrates this — with 2 item types (weight=2/profit=1/count=100 and weight=3/profit=100/count=100), 2 big knapsacks (capacity 100), and 100 small knapsacks (capacity 3), the heuristic picks item type 1 as pivot, but the resulting ILP becomes infeasible or suboptimal due to the pivot constraint reserving `|B| × wmax²` items.

**Fix applied**: 
- **`mkp_solve()` now tries ALL item types as pivot candidates** and returns the best solution found. This eliminates the reliance on the heuristic and guarantees correctness.
- **Added `mkp_solve_direct_ilp()`** as a fallback: a simpler configuration ILP without pivot constraints. If no pivot-based solution is feasible, this direct formulation is used instead. This handles edge cases where the pivot decomposition is too restrictive.
- Updated `mkp.h` to declare `mkp_solve_direct_ilp()`.
