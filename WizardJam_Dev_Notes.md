# WizardJam Development Notes & Gotchas
**Developer:** Marcus Daley  
**Project:** WizardJam (2-Week Game Jam)  
**Engine:** Unreal Engine 5.4  
**Last Updated:** December 23, 2025

---

## Purpose

This document captures hard-won lessons, UE5-specific gotchas, and development standards discovered during WizardJam development. Both Marcus and Claude agents should reference this before making architectural decisions.

---

## 🚨 CRITICAL: Build System Rules

### Never Modify Target.cs Compiler Arguments

**Rule:** Do NOT add `AdditionalCompilerArguments` to `WizardJamEditor.Target.cs`

**Why:** UE5 Editor builds share compiled binaries with the base UnrealEditor. Custom compiler arguments make these incompatible, causing build failure:
```
WizardJamEditor modifies the values of properties: [ AdditionalCompilerArguments ]. 
This is not allowed, as WizardJamEditor has build products in common with UnrealEditor.
```

**Correct Approach:** Use `Build.cs` with `PrivateDefinitions` for preprocessor defines:
```csharp
// WizardJam.Build.cs - CORRECT
if (Target.Platform == UnrealTargetPlatform.Win64)
{
    PrivateDefinitions.Add("_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS=1");
}
```

**Wrong Approach:**
```csharp
// WizardJamEditor.Target.cs - WRONG, breaks build
AdditionalCompilerArguments = "/wd4996";  // DON'T DO THIS
```

**Date Learned:** December 23, 2025

---

### MSVC Version Warnings Are Just Warnings

**Symptom:**
```
Visual Studio 2022 compiler version 14.44.35222 is not a preferred version. 
Please use the latest preferred version 14.38.33130
```

**Action:** Ignore this warning. It does NOT block compilation. UE5.4 prefers an older MSVC version but works fine with newer ones.

**Do NOT:** Try to "fix" this by modifying Target.cs or downgrading Visual Studio.

---

## 🏗️ Project Structure Standards

### File Organization
```
Source/WizardJam/
├── Code/
│   ├── Actors/        # Game actors (characters, pickups, projectiles)
│   ├── AI/            # AI controllers, behavior tree tasks
│   ├── Utility/       # Components, interfaces, helpers
│   ├── UI/            # Widget classes
│   └── GameModes/     # Game mode classes
└── WizardJam.Build.cs
```

### Header Comment Standard
Every .h and .cpp file should have:
```cpp
// FileName.h
// Developer: Marcus Daley
// Date: [Creation Date]
// [Brief description of class purpose and usage]
```

---

## 🎮 Coding Standards (Enforced)

### Property Exposure Rules
| Specifier | Use When |
|-----------|----------|
| `EditDefaultsOnly` | Class defaults configurable in Blueprint CDO |
| `EditInstanceOnly` | Per-instance values in level editor |
| `VisibleAnywhere` | Read-only display (runtime state) |
| **NEVER** `EditAnywhere` | Exposes to both - creates confusion |

### Initialization Rules
- **Always** use constructor initialization list
- **Never** hardcode values in headers
- **Never** hardcode values in function bodies

```cpp
// CORRECT
AMyClass::AMyClass()
    : DamageAmount(25.0f)  // Initialized here
    , FireRate(0.5f)
{
}

// WRONG
UPROPERTY()
float DamageAmount = 25.0f;  // Don't initialize in header
```

### Communication Pattern
- **Always** use delegates (Observer pattern) for system communication
- **Never** poll in Tick() for state changes

```cpp
// CORRECT - Delegate broadcast
OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

// WRONG - Polling
void Tick(float DeltaTime)
{
    if (Health < 50) { Flee(); }  // Don't do this
}
```

---

## 🔧 Enhanced Input System Setup

### Required Assets (Create in Content/Input/)
| Asset Name | Type | Value Type | Purpose |
|------------|------|------------|---------|
| IA_Move | Input Action | Axis2D | WASD movement |
| IA_Look | Input Action | Axis2D | Mouse camera |
| IA_Jump | Input Action | Digital | Spacebar jump |
| IA_Sprint | Input Action | Digital | Shift sprint |
| IA_Fire | Input Action | Digital | LMB spell cast |
| IA_Interact | Input Action | Digital | E key pickup |
| IMC_Default | Input Mapping Context | - | Maps keys to actions |

### Movement Key Modifiers
| Key | Modifiers Needed |
|-----|------------------|
| W | None (natural Y+) |
| S | Negate (Y-) |
| A | Swizzle YXZ (converts to X-) |
| D | Swizzle YXZ + Negate (X+) |

### Blueprint Setup Required
1. Create `BP_WizardPlayer` from `BasePlayer` C++ class
2. Assign all Input Actions in Details panel
3. In BeginPlay: Get Enhanced Input Subsystem → Add Mapping Context (IMC_Default)

---

## 🐛 Common Errors & Solutions

### "Unresolved external symbol" for Enum
**Cause:** Enum defined in header but not found by linker
**Solution:** Ensure enum is defined BEFORE any class that uses it, or use forward declaration with `enum class`

### "Cannot open include file"
**Cause:** Include path doesn't match folder structure
**Solution:** Use project-relative paths: `#include "Code/Actors/BaseCharacter.h"`

### Player Not Moving
**Checklist:**
1. ✅ Input Actions created in Content/Input/
2. ✅ IMC_Default mapping context created with key bindings
3. ✅ BP_WizardPlayer has Input Actions assigned in Details panel
4. ✅ BeginPlay adds IMC_Default to Enhanced Input Subsystem
5. ✅ GameMode uses BP_WizardPlayer as Default Pawn Class

### Live Coding Conflicts
**Rule:** Always close Unreal Editor before building in Visual Studio
**Why:** Live Coding can create conflicts with full VS builds

---

## 📅 Daily Workflow

1. **Start of session:** Pull latest, close UE Editor, build in VS
2. **During development:** Use Hot Reload for small changes only
3. **Before testing:** Close UE Editor → Full VS build → Reopen UE Editor
4. **End of session:** Commit working state, update this doc if new gotchas found

---

## 🅿️ Parked Features (Out of Scope for Jam)

- Spell Combination System
- Dog Possession/Switching Mode
- Companion Fetch Quest
- VoxelWeapon Capture for Rifle
- Full GAS Integration
- Damage Over Time System

---

## 📝 Lessons Log

| Date | Issue | Resolution | Category |
|------|-------|------------|----------|
| 2025-12-23 | Target.cs AdditionalCompilerArguments broke build | Remove custom args, use Build.cs PrivateDefinitions | Build System |
| | | | |
| | | | |

---

## 🔗 Quick References

- **UE5 Enhanced Input Docs:** https://docs.unrealengine.com/5.0/en-US/enhanced-input-in-unreal-engine/
- **Data Layers Docs:** https://docs.unrealengine.com/5.0/en-US/data-layers-in-unreal-engine/
- **Project Schedule:** See main WizardJam agent prompt document

---

*Add new gotchas to this document as they're discovered. Future Claude agents will search this file.*
