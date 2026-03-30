/-
# Multiple Knapsack Problem — Correctness Proof

We prove that the configuration-based ILP formulation is equivalent to
the original MKP: the two formulations have exactly the same set of
feasible solutions (up to a natural bijection) and the same optimal value.

This establishes that solving the configuration ILP optimally
(e.g., by trying all pivot candidates) yields a correct optimal
solution to the original MKP.

## Main Theorems

1. `configILP_to_mkp_feasible`: Every feasible config-ILP solution
   maps to a feasible MKP assignment.

2. `mkp_to_configILP_feasible`: Every feasible MKP assignment
   maps to a feasible config-ILP solution.

3. `configILP_to_mkp_profit`: The profit is preserved by the mapping.

4. `mkp_to_configILP_profit`: The profit is preserved in the other direction.

5. `configILP_optimal_implies_mkp_optimal`: If a config-ILP solution is
   optimal, then the corresponding MKP assignment is optimal.

6. `mkp_optimal_implies_configILP_optimal`: If an MKP assignment is optimal,
   then the corresponding config-ILP solution is optimal.
-/

import RequestProject.MKPDefs

open Finset BigOperators

/-! ## Feasibility preservation -/

/-- A feasible configuration ILP solution maps to a feasible MKP assignment. -/
theorem configILP_to_mkp_feasible {d m : ℕ} (inst : MKPInstance d m)
    (s : ConfigILPSolution d m) (hs : s.feasible inst) :
    (s.toAssignment).feasible inst := by
  unfold ConfigILPSolution.feasible at hs; unfold MKPAssignment.feasible; aesop

/-- A feasible MKP assignment maps to a feasible configuration ILP solution. -/
theorem mkp_to_configILP_feasible {d m : ℕ} (inst : MKPInstance d m)
    (a : MKPAssignment d m) (ha : a.feasible inst) :
    (a.toConfigILP).feasible inst := by
  convert ha using 1

/-! ## Profit preservation -/

/-- The profit is preserved when converting config-ILP solution to MKP assignment. -/
theorem configILP_to_mkp_profit {d m : ℕ} (inst : MKPInstance d m)
    (s : ConfigILPSolution d m) :
    (s.toAssignment).totalProfit inst = s.totalProfit inst := by
  unfold MKPAssignment.totalProfit ConfigILPSolution.totalProfit; aesop

/-- The profit is preserved when converting MKP assignment to config-ILP solution. -/
theorem mkp_to_configILP_profit {d m : ℕ} (inst : MKPInstance d m)
    (a : MKPAssignment d m) :
    (a.toConfigILP).totalProfit inst = a.totalProfit inst := by
  unfold ConfigILPSolution.totalProfit MKPAssignment.totalProfit; aesop

/-! ## Round-trip properties -/

/-- Converting MKP → ConfigILP → MKP gives back the same assignment. -/
theorem roundtrip_mkp {d m : ℕ} (a : MKPAssignment d m) :
    (a.toConfigILP).toAssignment = a := by
  rfl

/-- Converting ConfigILP → MKP → ConfigILP gives back the same solution. -/
theorem roundtrip_configILP {d m : ℕ} (s : ConfigILPSolution d m) :
    (s.toAssignment).toConfigILP = s := by
  rfl

/-! ## Optimality equivalence -/

/-- If the configuration ILP solution is optimal among all feasible config-ILP solutions,
    then its corresponding MKP assignment is optimal among all feasible MKP assignments. -/
theorem configILP_optimal_implies_mkp_optimal {d m : ℕ} (inst : MKPInstance d m)
    (s : ConfigILPSolution d m)
    (hfeas : s.feasible inst)
    (hopt : ∀ s' : ConfigILPSolution d m, s'.feasible inst →
      s'.totalProfit inst ≤ s.totalProfit inst) :
    (s.toAssignment).optimal inst := by
  refine' ⟨configILP_to_mkp_feasible inst s hfeas, ?_⟩
  intro a' ha'
  specialize hopt (a'.toConfigILP) (mkp_to_configILP_feasible inst a' ha')
  convert hopt using 1

/-- If an MKP assignment is optimal, then the corresponding config-ILP solution
    is optimal among all feasible config-ILP solutions. -/
theorem mkp_optimal_implies_configILP_optimal {d m : ℕ} (inst : MKPInstance d m)
    (a : MKPAssignment d m)
    (hopt : a.optimal inst) :
    ∀ s' : ConfigILPSolution d m, s'.feasible inst →
      s'.totalProfit inst ≤ (a.toConfigILP).totalProfit inst := by
  intro s' hs'
  have := hopt.2 (s'.toAssignment) (configILP_to_mkp_feasible inst s' hs')
  aesop
