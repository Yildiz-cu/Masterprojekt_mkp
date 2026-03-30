/-
# Multiple Knapsack Problem — Definitions

Formalization of the Multiple Knapsack Problem (MKP) and its
configuration-based ILP formulation, following the approach of
Lars Rohwedder for Q||Cmax extended with profits.

## Problem Description

Given:
- `d` item types, each with weight `w i`, profit `p i`, and multiplicity `n i`
- `m` knapsacks with capacities `C j`

Goal: Assign items to knapsacks to maximize total profit, subject to:
- No knapsack exceeds its capacity
- No item type exceeds its multiplicity

## Key Theorem

The configuration-based ILP formulation is equivalent to the original MKP:
every feasible MKP solution corresponds to a configuration ILP solution
with the same objective value, and vice versa. Therefore, solving the
configuration ILP optimally yields an optimal MKP solution.
-/

import Mathlib

open Finset BigOperators

/-- An MKP instance with `d` item types and `m` knapsacks. -/
structure MKPInstance (d m : ℕ) where
  /-- Weight of item type `i` -/
  weight : Fin d → ℕ
  /-- Profit of item type `i` -/
  profit : Fin d → ℕ
  /-- Multiplicity (number available) of item type `i` -/
  mult : Fin d → ℕ
  /-- Capacity of knapsack `j` -/
  cap : Fin m → ℕ

/-- A feasible MKP assignment: `assign j i` = number of items of type `i` in knapsack `j`. -/
structure MKPAssignment (d m : ℕ) where
  assign : Fin m → Fin d → ℕ

/-- An MKP assignment is feasible if:
  1. Each knapsack respects its capacity constraint
  2. Total items of each type do not exceed its multiplicity -/
def MKPAssignment.feasible {d m : ℕ} (inst : MKPInstance d m) (a : MKPAssignment d m) : Prop :=
  (∀ j : Fin m, ∑ i : Fin d, a.assign j i * inst.weight i ≤ inst.cap j) ∧
  (∀ i : Fin d, ∑ j : Fin m, a.assign j i ≤ inst.mult i)

/-- The total profit of an MKP assignment. -/
noncomputable def MKPAssignment.totalProfit {d m : ℕ} (inst : MKPInstance d m) (a : MKPAssignment d m) : ℕ :=
  ∑ j : Fin m, ∑ i : Fin d, a.assign j i * inst.profit i

/-- An MKP assignment is optimal if it is feasible and no other feasible assignment has higher profit. -/
def MKPAssignment.optimal {d m : ℕ} (inst : MKPInstance d m) (a : MKPAssignment d m) : Prop :=
  a.feasible inst ∧ ∀ a' : MKPAssignment d m, a'.feasible inst → a'.totalProfit inst ≤ a.totalProfit inst

/-- A configuration for a knapsack of capacity `C`:
    specifies how many items of each type to pack. -/
structure Configuration (d : ℕ) where
  items : Fin d → ℕ

/-- A configuration is valid for knapsack capacity `C` and item weights `w`
    if the total weight does not exceed C. -/
def Configuration.valid {d : ℕ} (c : Configuration d) (w : Fin d → ℕ) (C : ℕ) : Prop :=
  ∑ i : Fin d, c.items i * w i ≤ C

/-- The profit of a configuration. -/
noncomputable def Configuration.configProfit {d : ℕ} (c : Configuration d) (p : Fin d → ℕ) : ℕ :=
  ∑ i : Fin d, c.items i * p i

/-- A configuration ILP solution assigns a configuration to each knapsack. -/
structure ConfigILPSolution (d m : ℕ) where
  /-- The configuration assigned to knapsack `j` -/
  config : Fin m → Configuration d

/-- A configuration ILP solution is feasible if:
  1. Each configuration is valid for its knapsack's capacity
  2. Total items of each type across all configurations ≤ multiplicity -/
def ConfigILPSolution.feasible {d m : ℕ} (inst : MKPInstance d m) (s : ConfigILPSolution d m) : Prop :=
  (∀ j : Fin m, (s.config j).valid inst.weight (inst.cap j)) ∧
  (∀ i : Fin d, ∑ j : Fin m, (s.config j).items i ≤ inst.mult i)

/-- The total profit of a configuration ILP solution. -/
noncomputable def ConfigILPSolution.totalProfit {d m : ℕ} (inst : MKPInstance d m) (s : ConfigILPSolution d m) : ℕ :=
  ∑ j : Fin m, (s.config j).configProfit inst.profit

/-- Convert a configuration ILP solution to an MKP assignment. -/
def ConfigILPSolution.toAssignment {d m : ℕ} (s : ConfigILPSolution d m) : MKPAssignment d m :=
  ⟨fun j i => (s.config j).items i⟩

/-- Convert an MKP assignment to a configuration ILP solution. -/
def MKPAssignment.toConfigILP {d m : ℕ} (a : MKPAssignment d m) : ConfigILPSolution d m :=
  ⟨fun j => ⟨fun i => a.assign j i⟩⟩

