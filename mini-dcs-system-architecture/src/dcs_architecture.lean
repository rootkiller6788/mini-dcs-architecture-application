/-
Formalization of DCS System Architecture
Knowledge Level: L1-L4 (Definitions through Engineering Standards)

Key formalized concepts:
  - ISA-95 hierarchy levels as an inductive type
  - ISA-88 procedural control model
  - IEC 61508 SIL levels with ordering
  - Redundancy architecture properties
  - Network topology properties

References:
  - ISA-95: Enterprise-Control System Integration
  - IEC 61508: Functional Safety of E/E/PE Systems
  - ISA-88: Batch Control
-/

/- ==============================================================
   L1: ISA-95 Hierarchy Levels
   ============================================================== -/

inductive HierarchyLevel : Type where
  | field       : HierarchyLevel    -- Level 0: Sensors and actuators
  | control     : HierarchyLevel    -- Level 1: Basic control
  | supervisory : HierarchyLevel    -- Level 2: Process monitoring
  | plant_mes   : HierarchyLevel    -- Level 3: Manufacturing execution
  | enterprise  : HierarchyLevel    -- Level 4: Business planning
deriving Repr, DecidableEq

/-- ISA-95 levels are ordered from field (0) to enterprise (4). -/
def HierarchyLevel.toNat : HierarchyLevel → Nat
  | .field       => 0
  | .control     => 1
  | .supervisory => 2
  | .plant_mes   => 3
  | .enterprise  => 4

/-- ISA-95 Level comparison: level a is lower than or equal to level b. -/
def HierarchyLevel.le (a b : HierarchyLevel) : Bool :=
  a.toNat ≤ b.toNat

/-- Theorem: Hierarchy level ordering is transitive. -/
theorem hierarchy_level_transitive (a b c : HierarchyLevel)
    (h_ab : a.le b) (h_bc : b.le c) : a.le c := by
  unfold HierarchyLevel.le at *
  have h : a.toNat ≤ c.toNat := Nat.le_trans h_ab h_bc
  exact h

/-- Theorem: Every level is less than or equal to itself (reflexivity). -/
theorem hierarchy_level_refl (a : HierarchyLevel) : a.le a := by
  unfold HierarchyLevel.le
  exact Nat.le_refl a.toNat

/- ==============================================================
   L1: DCS Node Types
   ============================================================== -/

inductive NodeType : Type where
  | controller          : NodeType
  | ioSubsystem         : NodeType
  | operatorStation     : NodeType
  | engineeringStation  : NodeType
  | applicationStation  : NodeType
  | historian           : NodeType
  | alarmServer         : NodeType
  | safetyController    : NodeType
  | commGateway         : NodeType
  | batchServer         : NodeType
deriving Repr, DecidableEq

/-- Map a DCS node type to its ISA-95 hierarchy level. -/
def NodeType.isa95Level : NodeType → HierarchyLevel
  | .ioSubsystem        => .field
  | .controller         => .control
  | .safetyController   => .control
  | .commGateway        => .control
  | .operatorStation    => .supervisory
  | .alarmServer        => .supervisory
  | .applicationStation => .supervisory
  | .engineeringStation => .supervisory
  | .historian          => .plant_mes
  | .batchServer        => .plant_mes

/-- Theorem: Safety controllers and process controllers operate at the same ISA-95 level. -/
theorem safety_controller_at_control_level :
    NodeType.isa95Level .safetyController = NodeType.isa95Level .controller :=
  rfl

/- ==============================================================
   L2: Redundancy Architecture
   ============================================================== -/

inductive RedundancyArch : Type where
  | oneOutOfOne  : RedundancyArch    -- 1oo1: simplex
  | oneOutOfTwo  : RedundancyArch    -- 1oo2: dual redundant
  | twoOutOfTwo  : RedundancyArch    -- 2oo2: both required
  | twoOutOfThree : RedundancyArch   -- 2oo3: TMR
  | oneOutOfTwoD : RedundancyArch    -- 1oo2 with diagnostics
deriving Repr, DecidableEq

/-- Determine if an architecture tolerates a single failure. -/
def RedundancyArch.toleratesSingleFault : RedundancyArch → Bool
  | .oneOutOfOne  => false
  | .oneOutOfTwo  => true
  | .twoOutOfTwo  => false
  | .twoOutOfThree => true
  | .oneOutOfTwoD => true

/-- Theorem: 1oo2 and 2oo3 both tolerate single faults, but 1oo1 does not. -/
theorem redundancy_fault_tolerance :
    RedundancyArch.toleratesSingleFault .oneOutOfTwo = true ∧
    RedundancyArch.toleratesSingleFault .twoOutOfThree = true ∧
    RedundancyArch.toleratesSingleFault .oneOutOfOne = false := by
  apply And.intro
  · rfl
  · apply And.intro
    · rfl
    · rfl

/-- Count of channels required for each architecture. -/
def RedundancyArch.channelCount : RedundancyArch → Nat
  | .oneOutOfOne   => 1
  | .oneOutOfTwo   => 2
  | .twoOutOfTwo   => 2
  | .twoOutOfThree => 3
  | .oneOutOfTwoD  => 2

/-- Theorem: 2oo3 requires more channels than 1oo2. -/
theorem two_out_of_three_more_channels :
    RedundancyArch.channelCount .twoOutOfThree >
    RedundancyArch.channelCount .oneOutOfTwo := by
  rfl

/- ==============================================================
   L4: Safety Integrity Level (IEC 61508)
   ============================================================== -/

inductive SafetyIntegrityLevel : Type where
  | none : SafetyIntegrityLevel
  | sil1 : SafetyIntegrityLevel
  | sil2 : SafetyIntegrityLevel
  | sil3 : SafetyIntegrityLevel
  | sil4 : SafetyIntegrityLevel
deriving Repr, DecidableEq

/-- Numeric representation of SIL for comparison. -/
def SafetyIntegrityLevel.toNat : SafetyIntegrityLevel → Nat
  | .none => 0
  | .sil1 => 1
  | .sil2 => 2
  | .sil3 => 3
  | .sil4 => 4

/-- SIL comparison: a meets the requirements of b when a ≥ b. -/
def SafetyIntegrityLevel.meets (a b : SafetyIntegrityLevel) : Bool :=
  a.toNat ≥ b.toNat

/-- SIL band (PFDavg range per IEC 61508-1 Table 2). -/
def SafetyIntegrityLevel.pfdavgMax : SafetyIntegrityLevel → Float
  | .none => 1.0
  | .sil1 => 0.01
  | .sil2 => 0.001
  | .sil3 => 0.0001
  | .sil4 => 0.00001

/-- Theorem: SIL4 is the most stringent level. -/
theorem sil4_is_most_stringent (s : SafetyIntegrityLevel) :
    s.toNat ≤ SafetyIntegrityLevel.sil4.toNat := by
  cases s <;> decide

/-- Theorem: SIL ordering is transitive. -/
theorem sil_ordering_transitive (a b c : SafetyIntegrityLevel)
    (h_ab : a.meets b) (h_bc : b.meets c) : a.meets c := by
  unfold SafetyIntegrityLevel.meets at *
  have h : a.toNat ≥ c.toNat := Nat.le_trans h_bc h_ab
  exact h

/- ==============================================================
   L2: Alarm Priority (ISA-18.2)
   ============================================================== -/

inductive AlarmPriority : Type where
  | critical  : AlarmPriority
  | high      : AlarmPriority
  | medium    : AlarmPriority
  | low       : AlarmPriority
  | journal   : AlarmPriority
deriving Repr, DecidableEq

/-- Numeric priority (lower number = higher urgency). -/
def AlarmPriority.toNat : AlarmPriority → Nat
  | .critical => 1
  | .high     => 2
  | .medium   => 3
  | .low      => 4
  | .journal  => 5

/-- Comparison: is a higher urgency than b? -/
def AlarmPriority.higherThan (a b : AlarmPriority) : Bool :=
  a.toNat < b.toNat

/-- Theorem: Critical has higher priority than all others. -/
theorem critical_is_highest (p : AlarmPriority)
    (h : p ≠ AlarmPriority.critical) : AlarmPriority.higherThan .critical p := by
  unfold AlarmPriority.higherThan AlarmPriority.toNat
  cases p <;> simp at h <;> decide

/- ==============================================================
   L3: Network Topology
   ============================================================== -/

inductive NetworkTopology : Type where
  | bus       : NetworkTopology
  | star      : NetworkTopology
  | ring      : NetworkTopology
  | dualRing  : NetworkTopology
  | mesh      : NetworkTopology
  | tree      : NetworkTopology
  | dualStar  : NetworkTopology
deriving Repr, DecidableEq

/-- Check if a topology supports redundancy natively. -/
def NetworkTopology.supportsRedundancy : NetworkTopology → Bool
  | .bus      => false
  | .star     => false
  | .ring     => true
  | .dualRing => true
  | .mesh     => true
  | .tree     => false
  | .dualStar => true

/-- Theorem: Mesh topology supports redundancy. -/
theorem mesh_supports_redundancy :
    NetworkTopology.supportsRedundancy .mesh = true :=
  rfl

/-- Theorem: Bus topology does not support redundancy. -/
theorem bus_no_redundancy :
    NetworkTopology.supportsRedundancy .bus = false :=
  rfl

/-- Network diameter (max hops) as a function of topology and node count. -/
def NetworkTopology.diameter : NetworkTopology → Nat → Nat
  | .bus,      n => if n > 0 then n - 1 else 0
  | .star,     _ => 2
  | .ring,     n => n / 2
  | .dualRing, n => n / 2
  | .mesh,     _ => 1
  | .tree,     n => 2 * (Nat.log 2 n)
  | .dualStar, _ => 2

/- ==============================================================
   L4: ISA-88 Batch States
   ============================================================== -/

inductive BatchState : Type where
  | idle     : BatchState
  | running  : BatchState
  | complete : BatchState
  | held     : BatchState
  | stopped  : BatchState
  | aborted  : BatchState
deriving Repr, DecidableEq

/-- ISA-88 batch state transition validity.
    Returns true if transition from current state to target is valid. -/
def BatchState.isValidTransition (current target : BatchState) : Bool :=
  match current, target with
  | .idle,     .running  => true
  | .idle,     .aborted  => true
  | .running,  .held     => true
  | .running,  .stopped  => true
  | .running,  .complete => true
  | .running,  .aborted  => true
  | .complete, .aborted  => true
  | .held,     .running  => true
  | .held,     .stopped  => true
  | .held,     .aborted  => true
  | .stopped,  .running  => true
  | .stopped,  .aborted  => true
  | _,         _         => false

/-- Theorem: Idle can transition to Running (start batch). -/
theorem batch_idle_to_running : BatchState.isValidTransition .idle .running := rfl

/-- Theorem: Once the batch is complete, only abort is valid. -/
theorem batch_complete_only_abort (s : BatchState)
    (h : BatchState.isValidTransition .complete s) : s = .aborted := by
  unfold BatchState.isValidTransition at h
  cases s <;> simp at h <;> exact h.symm

/-- Theorem: Aborted is a terminal state — no further transitions. -/
theorem aborted_is_terminal (s : BatchState) :
    ¬ BatchState.isValidTransition .aborted s := by
  unfold BatchState.isValidTransition
  cases s <;> decide

/- ==============================================================
   L5: Hardware Fault Tolerance (IEC 61508-2)
   ============================================================== -/

inductive HardwareFaultTolerance : Type where
  | hft0 : HardwareFaultTolerance
  | hft1 : HardwareFaultTolerance
  | hft2 : HardwareFaultTolerance
deriving Repr, DecidableEq

/-- Compute HFT from architecture.
    HFT = N - M, where N = total channels, M = needed for function. -/
def HardwareFaultTolerance.fromArch : RedundancyArch → HardwareFaultTolerance
  | .oneOutOfOne   => .hft0
  | .oneOutOfTwo   => .hft1
  | .twoOutOfTwo   => .hft0
  | .twoOutOfThree => .hft1
  | .oneOutOfTwoD  => .hft1

/-- Theorem: 1oo2 provides HFT=1 (can tolerate 1 fault). -/
theorem one_out_of_two_hft1 :
    HardwareFaultTolerance.fromArch .oneOutOfTwo = .hft1 :=
  rfl

/-- Theorem: 2oo2 provides HFT=0 (cannot tolerate any fault for safety). -/
theorem two_out_of_two_hft0 :
    HardwareFaultTolerance.fromArch .twoOutOfTwo = .hft0 :=
  rfl
