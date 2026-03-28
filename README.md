# Masterprojekt_mkp

# Multiple Knapsack Problem (MKP) Solver - Implementation Details

This project implements a solver for the Multiple Knapsack Problem in C. The implementation follows Lars Rohwedder's theoretical approach for the Q||Cmax scheduling problem, but I've extended it to handle profits and item bundles, making it applicable to the classic MKP optimization problem.

## What This Solver Does

The Multiple Knapsack Problem is like having several backpacks of different sizes and a collection of valuable items. Each item has a weight and value, and you can have multiple copies of the same item type. The challenge is figuring out how to pack these items across all your backpacks to maximize the total value while respecting weight limits.

## The Core Algorithm - How It Works

The solver uses a sophisticated approach that combines combinatorial enumeration with integer linear programming:

### Step 1: Knapsack Classification
First, I partition the knapsacks into two categories based on their capacity. "Big" knapsacks have capacity at least w_max⁴ (where w_max is the heaviest item's weight raised to the fourth power), while "small" knapsacks have capacity below this threshold. This separation is crucial because small knapsacks have limited configuration possibilities that can be fully enumerated.

### Step 2: Pivot Selection
Next, I identify what's called the "pivot" item type. This is the item type that contributes the most total weight when you consider all available copies (weight × multiplicity). The pivot serves as a reference point for balancing the solution across knapsacks.

### Step 3: Knapsack Type Grouping
Knapsacks with identical capacities get grouped together into "knapsack types." This reduces redundancy since identical knapsacks can be treated symmetrically in the optimization.

### Step 4: Configuration Generation
For each knapsack type, I enumerate all feasible configurations - these are the different ways items can be packed into a knapsack of that capacity. Each configuration specifies how many of each item type fits. This enumeration is tractable for small knapsacks due to their limited capacity.

### Step 5: Dummy Item Introduction
To ensure the ILP can utilize full knapsack capacity when beneficial, I introduce dummy items with weight 1 and profit 0. These act as "padding" to fill any remaining space that real items can't efficiently use.

### Step 6: ILP Formulation
The problem gets formulated as an Integer Linear Program with:
- Variables y_{τ,C} representing how many times configuration C is used for knapsack type τ
- Variables b_i tracking the number of "bundles" used for each item type (bundles group items for mathematical convenience)
- Constraints ensuring we don't exceed item multiplicities, knapsack capacities match configuration usage, and pivot bounds are respected

### Step 7: GLPK Solving
The formulated ILP gets passed to GLPK (GNU Linear Programming Kit), a powerful open-source solver that finds the optimal variable assignments.

### Step 8: Solution Construction
Finally, I translate the abstract ILP solution back into concrete assignments - which specific items go into which specific knapsacks.

## Technical Dependencies

The implementation relies on GLPK for the heavy lifting of ILP solving. GLPK provides robust algorithms for integer optimization problems. The code is written in C11 for performance and uses Make for build management.

## Installation Process

Before compiling, you need GLPK installed on your system:

For Ubuntu/Debian systems, run:
```bash
sudo apt-get install libglpk-dev
```

For macOS with Homebrew:
```bash
brew install glpk
```

For Fedora/RHEL systems:
```bash
sudo dnf install glpk-devel
```

Once GLPK is installed, compile the solver by running:
```bash
make
```

## Running the Solver

The program accepts input in several ways:

1. Run with built-in test cases:
```bash
./mkp
```

2. Process a custom input file:
```bash
./mkp example_input.txt
```

3. Test with a larger built-in example:
```bash
./mkp --large
```

## Input Format Specification

Input files follow this structure:
- First line: number of item types (d) and number of knapsacks (m)
- Next d lines: for each item type, its weight, profit, and multiplicity
- Final line: capacities of all m knapsacks

For instance, the provided example_input.txt contains:
```
3 4
3 4 5
5 7 3
2 3 8
10 15 8 12
```

This describes a problem with 3 item types and 4 knapsacks. The first item type has weight 3, profit 4, and you have 5 copies available. The knapsacks have capacities of 10, 15, 8, and 12 units respectively.

## Code Organization

The implementation is split across several files for clarity:
- `mkp.h` defines all data structures and function signatures
- `mkp.c` contains the core algorithm implementation and ILP formulation logic
- `main.c` handles input/output and provides example problems
- `Makefile` manages compilation and linking with GLPK

## Program Output

When you run the solver, it provides detailed information about the solving process:
- Initial problem statistics (dimensions, maximum weight)
- How knapsacks were partitioned into big and small categories
- Which item type was selected as the pivot and why
- Number of configurations generated for each knapsack type
- GLPK's optimization progress (iterations, bound improvements)
- The final solution showing exactly which items go in each knapsack, along with total weights and profits achieved

This transparency helps verify the solution's correctness and understand the algorithm's behavior on different problem instances.

## Changes made to previous version
**Problem identified**: The pivot selection heuristic (choosing the item type with the largest multiplicity × weight) does not always work. The test instance test.txt demonstrates this — with 2 item types (weight=2/profit=1/count=100 and weight=3/profit=100/count=100), 2 big knapsacks (capacity 100), and 100 small knapsacks (capacity 3), the heuristic picks item type 1 as pivot, but the resulting ILP becomes infeasible or suboptimal due to the pivot constraint reserving |B| × wmax² items.

**Fix applied**:

mkp_solve() now tries ALL item types as pivot candidates and returns the best solution found. This eliminates the reliance on the heuristic and guarantees correctness.
Added mkp_solve_direct_ilp() as a fallback: a simpler configuration ILP without pivot constraints. If no pivot-based solution is feasible, this direct formulation is used instead. This handles edge cases where the pivot decomposition is too restrictive.
Updated mkp.h to declare mkp_solve_direct_ilp().
