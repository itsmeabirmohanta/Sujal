# --------------------------------------------------------------------------
# VRITRA CanSat - turn the imported STEP assembly into a solvable RF model.
#
# Runs against Project1 / HFSSDesign1, which already holds the 51 bodies
# imported from "rf structure.step".  What the import gives you is geometry
# and nothing else: no materials, no enclosure, no antennas, no air box, no
# ports, no setup.  This script adds all of that.
#
# It reuses the antenna, boundary and setup builders from CansatRfModel.py so
# there is exactly one definition of the whip, the patch and the solver
# settings - the primitive model and the imported model stay in step.
#
# Body identification came from DumpBodies.py, which measured every bounding
# box in the live session.  The name lists below are therefore measured, not
# guessed.  Re-run DumpBodies.py if you re-import, because AEDT renumbers.
# --------------------------------------------------------------------------

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import ansys.aedt.core as pyaedt
from ansys.aedt.core.generic.constants import Axis

import AedtSession
import CansatRfModel as rf


# FULL PATH, and a name AEDT will never generate by itself.
#
# This was "Project1" until 2026-08-09, and that one word destroyed the model
# twice.  AEDT names every new empty project Project1, Project2, Project3 ...
# so a bare project="Project1" resolves to whichever Project1 the running AEDT
# already has in memory - on a fresh launch, the EMPTY default.  The save at the
# end of Main() then writes 4 KB of nothing over the real file, and AEDT clears
# the matching .aedtresults folder to match.  That is how 1 577 MB of converged
# field solution was lost.  The stray Project2.aedtresults / Project3.aedtresults
# folders in Documents\Ansoft are fingerprints of the same collision.
#
# Two rules follow, and both are enforced below:
#   1. always pass the full path, never a bare name;
#   2. never pick a name matching AEDT's own Project<N> pattern.
ProjectDir = r"C:\Users\kalbh\Documents\Ansoft"
# Env overrides let a second shell case be built into its OWN project without
# editing (and risking) the defaults that reproduce the converged AbsLattice
# run. VRITRA_PROJECT is a bare stem; the full path and .aedt are added here so
# the name-collision rule (never a bare "Project<N>", always a full path) still
# holds. Default is unchanged.
_projStem = os.environ.get("VRITRA_PROJECT", "VritraCansatRf")
ProjectName = os.path.join(ProjectDir, _projStem + ".aedt")
DesignName = os.environ.get("VRITRA_DESIGN", "HFSSDesign1")

# --------------------------------------------------------------------------
# 1.  What each imported body is
#
# Grouped from the measured bounding boxes.  Every one of the 51 solids is
# accounted for; anything unlisted trips an error rather than silently
# defaulting to vacuum, which would be an invisible way to get a wrong answer.
# --------------------------------------------------------------------------

# Six structural discs.  The CDR states aluminium for the plates.
Plates = [
    "Body1_10",     # Ø130 x 9, Z -59..-50   base plate
    "Body1_4",      # Ø110 x 9, Z -32.5..-23.5
    "Body1_2",      # Ø110 x 10, Z 16..26
    "Body1_1",      # Ø110 x 10, Z 43.5..53.5
    "Body1_11",     # Ø110 x 10, Z 68.5..78.5
    "Body1_5",      # Ø110 x 9, Z 98..107
]

# Four Ø8 tie rods running the whole length.  CDR: glass fibre.
Rods = ["Body1_6", "Body1_7", "Body1_8", "Body1_9"]

# Reaction wheel: the Ø124 x 78 drum plus its rim vanes and feet.  All one
# electrically continuous aluminium part, so they get united into one object.
WheelDrum = "Body297"
WheelVanes = [
    "Body92", "Body93", "Body217", "Body259", "Body260",
    "Body282", "Body283", "Body285",                        # 65 mm vanes
    "Body288", "Body289", "Body290", "Body291", "Body292",
    "Body293", "Body294", "Body295", "Body296", "Body1_12",  # 44 mm vanes
    "Body235", "Body236", "Body237", "Body238", "Body239",
    "Body240", "Body241", "Body242",                         # 3 mm feet
]
WheelDisc = "Body1_13"      # Ø104 x 10 flywheel disc, Z 117..127

# Trays and decks.
Tray = "Body1_3"            # 74 x 68 x 20, Z -57..-37   battery tray
Deck = "Body1"              # 70 x 70 x 10, Z 133.5..143.5  motor deck
TopLid = "Body2"            # Ø130 x 2, Z 220..222   parachute bay lid

# Motor internals: gearbox, shaft, screws.  Every one of these is buried
# inside the aluminium drum, so it is electrically invisible from outside -
# and each costs mesh.  Deleting them is defeaturing, not cheating.
MotorInternals = [
    "Body1_14", "Body1_15", "Body1_16", "Body2_1",
    "Body3", "Body4", "Body5", "Body6", "Body7", "Body8",
]

# --------------------------------------------------------------------------
# 2.  The enclosure the import does not contain
#
# rf structure.step has no outer shell - the widest bodies in it are the
# Ø130 base plate and the Ø130 top lid.  So the ABS body is built here as a
# primitive tube, which is what Test 3 needs anyway: the same three cases as
# the primitive model, swapped by material.
# --------------------------------------------------------------------------

ShellOuterRadius = 65.0
ShellWall = 3.0             # confirmed by the team 2026-08-09: printed ABS
                            # body and parachute bay are both 3 mm.  This
                            # supersedes the 1.5 mm carbon wall in PDR 5.1.1.

# The base plate (Ø130, Z -59..-50) and the top lid (Ø130, Z 220..222) are
# themselves 65 mm in radius, so the shell wall must run BETWEEN them, not
# through them.  Overlapping solids are a geometry error in HFSS, and the
# union of two different materials is meaningless anyway.
# Shell extent, confirmed by the team from AEDT Measure on 2026-08-09:
#   bottom  Arc Center [0, 0, -50],  Ø130   -> base plate top face
#   top     Arc Center [0, 0, 150]          -> where the parachute bay starts
# The body stops at the bay; it does NOT wrap it. The bay has its own wall.
# 200 mm of shell, which mirrors Fusion exactly: there the shell is Z 10..150
# on a base plate topping out at Z 10, and the bay drum runs 140..218, so the
# two overlap by 10 mm at the joint. Same relationship here.
ShellZ0 = -50.0
ShellZ1 = 150.0

# "Real" | "AbsLattice" | "Abs" | "Carbon" | "Open"
#
#   Real        the actual perforated shell, exported from Fusion through the
#               API to ansys/CansatShellReal.step and imported here.  3 280
#               faces, so the heaviest to mesh, and it covers only 140 mm of
#               the 281 mm imported stack because the STEP airframe is an
#               older revision.  Maximum geometric fidelity.
#
#   AbsLattice  the real shell.  Measured in Fusion: volume 81 454 mm3 against
#               167 573 mm3 for a solid tube of the same envelope, so the print
#               is 48.6 % solid and 51.4 % air.  Perforations are ~10 mm, which
#               is lambda/35 at 867 MHz - an order of magnitude below the
#               lambda/2 = 173 mm an aperture needs to propagate.  That is the
#               quasi-static limit, where volume mixing is the correct physics
#               rather than an approximation:
#                   er_eff   = 0.486 * 2.7 + 0.514 * 1.0 = 1.83
#                   tand_eff = 0.008 * (0.486 * 2.7) / 1.83 = 0.0057
#   Abs         solid 3 mm ABS.  Wrong for this print, kept because it is what
#               the CDR text currently implies and the delta is worth showing.
#   Carbon      the PDR's CFRP, as the rejected-option comparison.
#   Open        no enclosure - the control case.
#
# CHANGED 2026-08-09 from "Real" to "AbsLattice".
#
# "Real" imports the actual Fusion print: 4787 faces of hex perforation.  That
# is what wedged the mesher - AEDT sat in geometry caching and the solver engine
# never spawned ("Unable to start analysis on machine 'Local Machine'", then
# "Com Engine non-responsive").
#
# "AbsLattice" is the SAME shell as a smooth tube carrying the effective
# permittivity of the perforated print (er 1.83, tand 0.0057, derived above from
# the measured 48.6 % fill).  Not a shortcut: the holes are ~10 mm = lambda/35 at
# 867 MHz, deep in the quasi-static limit where volume mixing IS the correct
# physics.  An aperture must reach lambda/2 = 173 mm to propagate in its own
# right, and these are 17x below that.
#
# Set back to "Real" only for a CDR figure showing the perforation geometry, and
# expect it not to solve on this machine.
ShellCase = os.environ.get("VRITRA_SHELLCASE", "AbsLattice")

# The Fusion export used by ShellCase = "Real".
RealShellFile = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             "CansatShellReal.step")

# Extremes of the imported airframe, used to place the antennas.
StackBottomZ = -59.0        # base plate underside
StackTopZ = 222.0           # top lid upper face

# --------------------------------------------------------------------------
# 3.  Where the antennas go - AS BUILT, per the user on 2026-08-09
#
# COMM: RadioMaster dual-band T antenna, hung DOWNWARD on the vehicle centre
# axis from the underside of the TOP electronics plate, passing through the
# middle of the plates below it.
#
# Why the top plate and not the middle one, which is what was asked for.
# Measured from the live model:
#
#     top electronics plate  Body1_11  underside  Z  68.5
#     battery cell top       Cell2_9              Z -36.5   (spans y -10.5..
#                                                            +10.5, so it
#                                                            crosses the axis)
#                                                 ----------
#                                    axial space available  105.0 mm
#     RadioMaster T antenna, vendor total length             100.0 mm  -> fits
#
#     middle plate           Body1_1   underside  Z  43.5
#                                    axial space available   80.0 mm
#                                                            -> 100 mm DOES NOT FIT
#
# So the middle plate is not achievable for this part without moving the
# centre battery cell or re-spacing the plates.  Top plate it is, with 5 mm
# to spare.  The four tie rods sit at (+-46,0) and (0,+-46), so the centre
# axis itself is clear.
#
# GNSS: the patch is the one integrated on the 7Semi L89HA module - there is
# NO external GNSS antenna.  The module lives on the top avionics PCB, so the
# patch sits on top of PcbPlane3 (Z 84.5..86.1) looking up.  It stays on the
# axis: the comm dipole occupies Z -31.5..68.5, entirely BELOW the patch, so
# the two do not collide.
#
# Read the warning in section 9 about what is above this patch.
# --------------------------------------------------------------------------

WhipMountZ = 68.5                   # underside of Body1_11, top electronics plate
PatchMountZ = 86.1                  # top face of PcbPlane3, where the L89 sits

# --------------------------------------------------------------------------
# 4.  Avionics the STEP export leaves out
#
# rf structure.step contains the airframe but none of the electronics, and
# those are not RF-neutral.  A 90 x 90 mm PCB ground plane has a perimeter of
# 1.04 lambda at 867 MHz; the plates are already 0.9994 lambda.  Leaving them
# out would understate exactly the coupling this study exists to measure.
#
# Modelled as round boards, Ø100, drilled for the four tie rods - which is how
# they actually stack in this airframe.
# --------------------------------------------------------------------------

PcbRadius = 50.0
PcbThickness = 1.6
RodPositions = [(46.0, 0.0), (-46.0, 0.0), (0.0, 46.0), (0.0, -46.0)]
RodHoleRadius = 4.5                 # Ø9 clearance around the Ø8 rods

# The plate top face each board sits on.
PcbPlateTopZ = [26.0, 53.5, 78.5]

# CORRECTED 2026-08-09 on the user's description of the real build: each green
# board is a PCB laminated DIRECTLY onto the aluminium plate underneath it -
# there is no 6 mm standoff gap between plate and board.  So each copper plane
# starts exactly at the plate's top face.
#
#   was  PcbZ = [32.0, 59.5, 84.5]   (6 mm standoffs, board floating above)
#   now  PcbZ = [26.0, 53.5, 78.5]   (board bonded flat to the plate)
#
# Electrically this is a bigger change than 6 mm suggests.  Copper touching
# aluminium over the full Ø100 face is a solid DC and RF bond, so plate and
# ground plane become one conductor rather than two coupled ones.  It removes
# the thin 6 mm air gap that would otherwise behave as a parallel-plate region
# between them.
PcbZ = list(PcbPlateTopZ)

# With the boards bonded flat there is nothing for a plate-to-board standoff to
# span, so they are no longer built.  MakeStandoffs() is left in the file but
# is not called - see section 8.
BuildStandoffs = False

StandoffRadius = 3.0                # M3 hex standoff, ~6 mm across flats
StandoffCircleRadius = 40.0
# 45 degrees off the rod axes so nothing collides: centre-to-centre from a
# standoff at (28.28, 28.28) to a rod at (46, 0) is 33.4 mm, and the two radii
# sum to only 7 mm.
StandoffAngles = [45.0, 135.0, 225.0, 315.0]

# Three 21700 cells lying on their sides in the battery tray (Z -57..-37,
# 74 x 68).  Upright will not fit: the tray is 20 mm deep and a cell is 70 mm
# long, so they lie along X, three abreast in Y.
CellRadius = 10.5
CellLength = 70.0
CellXStart = -35.0
CellY = [-22.0, 0.0, 22.0]
CellZ = -47.0                       # tray mid-height


# Everything this script creates.  Cleared at the start of every run so the
# script is idempotent: a failed run leaves half-built antennas behind, and
# rebuilding on top of those is how you get duplicate ports and silent
# geometry errors.
OurObjects = [
    "CanSatShell", "ShellVoid", "ShellLower", "ShellUpper", "ShellTrim",
    "PcbPlane1", "PcbPlane2", "PcbPlane3",
    "Cell1", "Cell2", "Cell3",
    # current dipole parts, plus the legacy monopole names so a re-run over an
    # older build still clears itself
    "CommArmUpper", "CommArmLower", "CommFeed",
    "WhipGroundDisc", "WhipSleeve", "WhipRod",
    "PatchGround", "PatchCeramicBlock", "PatchProbe", "PatchFeed",
    "AirBox", "Region",
]


def ClearPreviousRun(app):
    """Remove anything a previous run of this script created.

    Boundaries and ports must go too, not just geometry.  Deleting a port's
    sheet does not delete the port: AEDT keeps the excitation, and the next
    run's port gets a mangled suffix like GnssPort_0KLOX7.  Validate then
    reports two overlapping ports and refuses to solve.  The import arrived
    with no boundaries at all, so everything here is ours to clear.
    """
    for b in list(app.boundaries):
        b.delete()
    print("   cleared all boundaries and ports")

    # Solution setups too.  create_setup() does not overwrite an existing name,
    # it auto-suffixes - so three re-runs left SolveAllBands, _2, _2_3 and
    # _2_3_4 side by side, and Analyze All would solve all four identically.
    #
    # Read the LIVE list, not app.setups.  app.setups is a cached Python list
    # built when the Hfss object was constructed; on a fresh attach to an
    # already-open design it comes back empty even though setups exist, the
    # cleanup silently does nothing, and the next create_setup auto-suffixes.
    # That is how the sweep became WideSweep_S5DGP1 and every report then
    # failed with "Setup doesn't exist in this design".
    liveSetups = list(app.oanalysis.GetSetups() or [])
    for name in liveSetups:
        try:
            app.oanalysis.DeleteSetups([name])
        except Exception as exc:                       # noqa: BLE001
            print("   could not delete setup %s: %s" % (name, exc))
    remaining = list(app.oanalysis.GetSetups() or [])
    print("   cleared solution setups: %s -> %s"
          % (liveSetups or "none", remaining or "none"))

    # Geometry.  Prefix matching, because AEDT suffixes duplicate names the
    # same way: an exact-name list misses GnssPatch_1, GnssFeed_2 and friends,
    # which is how six orphan sheets accumulated.
    prefixes = tuple(OurObjects) + ("GnssPatch", "GnssFeed", "PatchDrill",
                                    "PcbDrill", "CornerA", "CornerB", "Standoff",
                                    "AntennaBore")
    stale = [n for n in app.modeler.object_names if n.startswith(prefixes)]
    if stale:
        app.modeler.delete(assignment=stale)
        print("   cleared %d objects/sheets" % len(stale))
    else:
        print("   no leftover geometry")


AirframeStepFile = r"C:\Users\kalbh\Downloads\rf structure.step"


def ImportAirframeIfMissing(app):
    """Import rf structure.step when the design has no airframe in it.

    Added 2026-08-09 after a hard AEDT restart let a freshly launched AEDT
    overwrite the saved Project1.aedt with its empty default project, wiping
    the imported airframe.  The airframe used to arrive by a MANUAL
    Modeler > Import, so nothing in this pipeline could put it back and the
    build died with "These bodies are named in this script but not in the
    design".

    Making the import part of the script means the whole model is reproducible
    from the two STEP files and this repo, with no manual step to forget.

    Idempotent: if the airframe is already present it does nothing, so normal
    re-runs are unaffected.
    """
    have = set(app.modeler.object_names)
    if any(n.startswith("Body1") for n in have) or "Body297" in have:
        print("   airframe already present (%d bodies) - not re-importing"
              % len(have))
        return False

    if not os.path.exists(AirframeStepFile):
        raise SystemExit("airframe missing from the design AND the STEP is not "
                         "on disk:\n   %s" % AirframeStepFile)

    print("   design is empty - importing %s" % os.path.basename(AirframeStepFile))
    before = set(app.modeler.object_names)
    app.modeler.import_3d_cad(AirframeStepFile, healing=True)
    added = sorted(set(app.modeler.object_names) - before)
    if not added:
        raise SystemExit("import produced no bodies: %s" % AirframeStepFile)

    print("   imported %d bodies: %s" % (len(added), ", ".join(added[:14])
                                         + (" ..." if len(added) > 14 else "")))
    # The STEP is in CENTIMETRES; a silent unit mis-read is the classic way an
    # imported model ends up 10x wrong, so assert the envelope rather than hope.
    span = 0.0
    for n in added:
        try:
            bb = app.modeler[n].bounding_box
        except Exception:                       # noqa: BLE001
            continue
        span = max(span, bb[3] - bb[0], bb[4] - bb[1])
    print("   widest body spans %.1f mm (expect ~130 for the Oe130 airframe)" % span)
    if not 100.0 < span < 160.0:
        raise SystemExit(
            "imported airframe is %.1f mm across - units were misread.\n"
            "Expected ~130 mm. Check the import unit setting." % span)
    return True


def HealImportedBodies(app):
    """Heal the one body whose Parasolid check fails on import.

    STEP surfaces rarely close exactly; healing stitches the slivers.  This is
    the same "Validation and Healing" the import dialog offers, applied after
    the fact to the body that still complains.
    """
    if Deck in app.modeler.solid_names:
        app.modeler.heal_objects(assignment=Deck)
        print("   healed %s" % Deck)


def Defeature(app):
    """Delete the buried motor internals and unite the reaction wheel.

    Both steps are skipped if a previous run already did them, so this is
    safe to re-run after a failure further down.
    """
    present = set(app.modeler.solid_names)

    doomed = [n for n in MotorInternals if n in present]
    if doomed:
        app.modeler.delete(assignment=doomed)
        print("   deleted %d buried motor bodies" % len(doomed))
    else:
        print("   motor internals already removed")

    # Uniting removes the interior faces between drum and vanes.  Those faces
    # are meshed otherwise, and they carry no current because the parts are
    # bonded metal.
    vanes = [n for n in WheelVanes if n in present]
    if vanes:
        app.modeler.unite(assignment=[WheelDrum] + vanes, keep_originals=False)
        print("   united drum + %d vanes into %s" % (len(vanes), WheelDrum))
    else:
        print("   reaction wheel already united")


def RepairIntersections(app):
    """Resolve the overlaps the imported CAD ships with.

    HFSS refuses to mesh interpenetrating solids, and Validate reports nine of
    them here.  None are our doing - they are how the assembly was drawn:

      base plate x 4 rods    the rods are seated 8 mm into the plate, drawn as
                             full cylinders rather than into blind holes
      base plate x tray      the tray sits down into the plate
      reaction wheel x rods  the rod tips poke into the rotating drum, which
                             cannot be true of a part that spins

    Two different repairs, because the physics differs.  Plate and tray are
    both bolted aluminium, so uniting them is exactly right - one conductor.
    Rod-through-metal is a clearance hole, so the metal gets the rod
    subtracted out of it and the rod itself is kept.
    """
    present = set(app.modeler.solid_names)

    if Tray in present:
        app.modeler.unite(assignment=[Plates[0], Tray], keep_originals=False)
        print("   united base plate + battery tray (both bolted aluminium)")
    else:
        print("   base plate and tray already united")

    # keep_originals=True: we are cutting holes in the metal, not consuming
    # the rods.  Without it the rods vanish and the model loses its dielectric.
    for metal in (Plates[0], WheelDrum):
        app.modeler.subtract(blank_list=[metal], tool_list=Rods,
                             keep_originals=True)
        print("   cut rod clearance holes in %s" % metal)


def ClassifyOrFail(app):
    """After defeaturing, the structural bodies should be exactly these."""
    present = set(app.modeler.solid_names) - set(OurObjects)
    expected = set(Plates) | set(Rods) | {WheelDrum, WheelDisc, Deck, TopLid}
    # Tray is optional: RepairIntersections merges it into the base plate, so
    # it is present on a first run and gone on every run after.
    present.discard(Tray)

    missing = sorted(expected - present)
    unknown = sorted(present - expected)
    if missing:
        raise SystemExit(
            "These bodies are named in this script but not in the design:\n  %s\n"
            "AEDT renumbers on re-import - re-run DumpBodies.py and update the "
            "lists." % ", ".join(missing))
    if unknown:
        raise SystemExit(
            "These bodies exist but this script does not classify them:\n  %s\n"
            "Add them to a group rather than letting them default to vacuum." %
            ", ".join(unknown))
    print("   all %d structural bodies classified" % len(present))


def AssignMaterials(app):
    """Give every remaining body a real material and turn solve-inside off.

    Solve Inside = False on metal is the single setting that keeps this model
    solvable on a laptop: aluminium is 2.77 um deep at 867 MHz, so a 10 mm
    plate is 3600 skin depths and meshing its interior is pure waste.  HFSS
    substitutes a finite-conductivity surface impedance instead.
    """
    groups = [
        (Plates, "aluminum", False),
        ([WheelDrum, WheelDisc], "aluminum", False),
        ([Tray, Deck], "aluminum", False),
        (Rods, "GlassFibreLaminate", True),         # dielectric: must solve inside
        ([TopLid], "AbsPolymer", True),     # dielectric: must solve inside
    ]
    present = set(app.modeler.solid_names)
    for names, material, solveInside in groups:
        # Tray is merged into the base plate by RepairIntersections, so it is
        # absent on re-runs.  Skipping silently beats crashing on a body that
        # was deliberately consumed.
        live = [n for n in names if n in present]
        for n in live:
            o = app.modeler[n]
            o.material_name = material
            o.solve_inside = solveInside
        print("   %-12s -> %-20s (%d bodies, solve_inside=%s)"
              % (live[0][:12] if live else "-", material, len(live), solveInside))


def MakePcbPlanes(app):
    """Three round PCB ground planes, drilled for the tie rods.

    Modelled as copper with Solve Inside off: the board is FR4 but the thing
    that matters at 867 MHz is the ground plane on it, and a 35 um copper
    pour is ~16 skin depths, so its interior carries nothing.
    """
    for i, z in enumerate(PcbZ, start=1):
        board = app.modeler.create_cylinder(
            orientation=Axis.Z,
            origin=[0, 0, z],
            radius=PcbRadius,
            height=PcbThickness,
            name="PcbPlane%d" % i,
            material="copper",
        )
        # Drill the four rod holes, otherwise every board intersects every rod
        # and Validate rejects the model - the same fault the imported CAD had.
        drills = []
        for j, (x, y) in enumerate(RodPositions):
            drills.append(app.modeler.create_cylinder(
                orientation=Axis.Z,
                origin=[x, y, z - 1.0],
                radius=RodHoleRadius,
                height=PcbThickness + 2.0,
                name="PcbDrill%d_%d" % (i, j),
            ).name)
        app.modeler.subtract(blank_list=[board.name], tool_list=drills,
                             keep_originals=False)
        board.solve_inside = False
        print("   PcbPlane%d at Z %.1f, Ø%.0f, 4 rod holes"
              % (i, z, PcbRadius * 2))


def DrillAntennaClearance(app):
    """Cut a clearance bore on the vehicle axis for the comm dipole.

    The antenna hangs from the top electronics plate down through the plates
    and boards below it, so every one of them needs a hole - exactly as the
    real build would.  Without this, Validate reports:
        Parts PcbPlane1 and CommArmUpper intersect.
        Parts Body1_1  and CommArmLower intersect.  ... etc
    which is HFSS correctly refusing to mesh copper occupying the same space
    as aluminium.

    Bore is sized off the fattest part of the antenna plus 2 mm radial
    clearance, and spans only the antenna's own Z extent so the mounting plate
    above it is left intact.
    """
    boreRadius = rf.CommSleeveRadius + 2.0          # 5 mm -> Oe10 hole
    tipZ = WhipMountZ - rf.CommTotalLength
    zLow = tipZ - 1.5                               # a little past the tip
    zHigh = WhipMountZ                              # stop at the mounting face

    drill = app.modeler.create_cylinder(
        orientation=rf.Axis.Z,
        origin=[0, 0, rf.Mm(zLow)],
        radius=boreRadius,
        height=rf.Mm(zHigh - zLow),
        name="AntennaBore",
        material="vacuum",
    )

    # Anything whose Z range overlaps the bore gets drilled.  Driven off live
    # bounding boxes rather than a hard-coded name list, because AEDT renumbers
    # imported bodies on every re-import.
    victims = []
    for name in app.modeler.object_names:
        if name in (drill.name, "AirBox", "Region"):
            continue
        if not (name.startswith("Body1") or name.startswith("PcbPlane")):
            continue
        try:
            bb = app.modeler[name].bounding_box       # xmin ymin zmin xmax ymax zmax
        except Exception:                             # noqa: BLE001
            continue
        zmin, zmax = bb[2], bb[5]
        # overlaps the bore in Z, and actually spans the axis in X and Y
        if zmax > zLow and zmin < zHigh and bb[0] < 0 < bb[3] and bb[1] < 0 < bb[4]:
            victims.append(name)

    if not victims:
        app.modeler.delete(drill.name)
        print("   nothing to drill")
        return []

    app.modeler.subtract(blank_list=victims, tool_list=[drill.name],
                         keep_originals=False)
    print("   bored Oe%.0f mm on the axis, Z %.1f..%.1f, through %d parts:"
          % (boreRadius * 2, zLow, zHigh, len(victims)))
    print("      %s" % ", ".join(sorted(victims)))
    return victims


def MakeStandoffs(app):
    """Aluminium M3 standoffs carrying each board off the plate below it.

    Not cosmetic.  These bond the PCB ground planes to the aluminium plates,
    so the plates, boards and standoffs behave as one conductor rather than
    as isolated resonators.  Whether that helps or hurts is exactly what the
    pattern result will show - but modelling the boards floating in mid-air
    would have answered a different question from the one being asked.
    """
    import math
    made = []
    for i, (zTop, zBoard) in enumerate(zip(PcbPlateTopZ, PcbZ), start=1):
        for j, deg in enumerate(StandoffAngles):
            r = math.radians(deg)
            post = app.modeler.create_cylinder(
                orientation=Axis.Z,
                origin=[StandoffCircleRadius * math.cos(r),
                        StandoffCircleRadius * math.sin(r),
                        zTop],
                radius=StandoffRadius,
                height=zBoard - zTop,
                name="Standoff%d_%d" % (i, j),
                material="aluminum",
            )
            post.solve_inside = False
            made.append(post.name)
    print("   %d aluminium standoffs, %.0f mm tall, bonding boards to plates"
          % (len(made), PcbZ[0] - PcbPlateTopZ[0]))
    return made


def MakeCells(app):
    """Three 21700 cells lying in the battery tray, and their pocket.

    The tray is solid aluminium in the import, so the cells have to be cut
    out of it or they interpenetrate.  Physically that cut IS the pocket the
    cells sit in.
    """
    made = []
    for i, y in enumerate(CellY, start=1):
        cell = app.modeler.create_cylinder(
            orientation=Axis.X,             # cells lie along X
            origin=[CellXStart, y, CellZ],
            radius=CellRadius,
            height=CellLength,
            name="Cell%d" % i,
            material="CellSteel",
        )
        cell.solve_inside = False
        made.append(cell.name)

    # keep_originals=True so the cells survive being used as cutting tools.
    app.modeler.subtract(blank_list=[Plates[0]], tool_list=made,
                         keep_originals=True)
    print("   %d cells Ø%.0f x %.0f in a pocket cut from the tray"
          % (len(made), CellRadius * 2, CellLength))


def ImportRealShell(app):
    """Import the actual perforated shell exported from Fusion.

    CansatShellReal.step is `New Shell:1` exported through the Fusion API:
    Ø130 OD, 3.0 mm wall, 140 mm tall, 3 280 faces, arriving at local Z 0..140.

    Placement: its bottom face goes on the base plate's top face at Z -50, so
    the shell stands on the base plate exactly as it does in Fusion, where it
    runs Z 10..150 off a base plate topping out at Z 10.

    Read the caveat in the module docstring: the imported airframe is an older
    revision, 281 mm tall, while this shell is 140 mm.  It therefore covers the
    avionics stack and stops below the parachute bay.  That is a real
    inconsistency, not a modelling choice, and it goes away with a clean
    re-export of the current assembly.
    """
    def ImportOne(tag, zBottom):
        """Import one copy of the export and land its bottom face at zBottom."""
        before = set(app.modeler.object_names)
        app.modeler.import_3d_cad(
            RealShellFile,
            healing=True,               # STEP surfaces rarely close exactly
            import_materials=False,     # every Fusion tag is default Steel
            create_group=False,
        )
        new = [n for n in app.modeler.object_names if n not in before]
        if not new:
            raise SystemExit("import produced no bodies: %s" % RealShellFile)
        if len(new) > 1:
            app.modeler.unite(assignment=new, keep_originals=False)
        piece = app.modeler[new[0]]

        # Recentre in X and Y as well as Z.  The Fusion component's local
        # origin is 500 mm from the body itself, so a Z-only move left the
        # first attempt parked at X 435..565 - beside the CanSat, not around
        # it.  Validate passed anyway, because a body floating in clear air
        # intersects nothing; the only symptom was the air box stretching to
        # 810 mm.  Centre on the body's own bounding box, never the origin.
        bb = piece.bounding_box
        app.modeler.move(assignment=[piece.name], vector=[
            -(bb[0] + bb[3]) / 2.0,
            -(bb[1] + bb[4]) / 2.0,
            zBottom - bb[2],
        ])
        piece.name = tag
        return piece

    # The export is 140 mm tall but the airframe needs 270 mm of shell
    # (Z -50 to 220).  Stack two copies of the real perforated part rather
    # than inventing a hole pattern: copy A from -50, copy B from 90, so their
    # faces meet exactly at Z = 90 with no overlap and no gap.  Then trim
    # whatever pokes above the parachute-bay lid.
    lower = ImportOne("ShellLower", ShellZ0)
    upper = ImportOne("ShellUpper", ShellZ0 + 140.0)
    app.modeler.unite(assignment=[lower.name, upper.name], keep_originals=False)
    shell = app.modeler[lower.name]

    trim = app.modeler.create_box(
        origin=[-100, -100, ShellZ1],
        sizes=[200, 200, 200],
        name="ShellTrim",
    )
    app.modeler.subtract(blank_list=[shell.name], tool_list=[trim.name],
                         keep_originals=False)
    shell.name = "CanSatShell"

    # Real geometry, so the real bulk material - the perforation is drawn, not
    # homogenised, and applying er 1.83 here would count the holes twice.
    shell.material_name = "AbsPolymer"
    shell.solve_inside = True
    shell.transparency = 0.7

    bb = shell.bounding_box
    # Count faces via the editor, NOT via shell.faces.  PyAEDT's .faces builds a
    # FacePrimitive per face and asks AEDT for each one's centroid; on the 2184
    # NURBS faces of the hex lattice that fails with
    #   "Failed to compute face center as the face is not planar"
    # which looks like a geometry fault but is only a reporting call.
    faceCount = len(app.modeler.oeditor.GetFaceIDs(shell.name))
    print("   real perforated shell: 2 stacked copies, %d faces, Z %.1f..%.1f, "
          "AbsPolymer er 2.7" % (faceCount, bb[2], bb[5]))
    return shell


def MakeShell(app):
    """The ABS body, built as a tube because the STEP file has no shell."""
    if ShellCase == "Open":
        print("   ShellCase=Open - no enclosure built (control case)")
        return None

    if ShellCase == "Real":
        return ImportRealShell(app)

    outer = app.modeler.create_cylinder(
        orientation=Axis.Z,
        origin=[0, 0, ShellZ0],
        radius=ShellOuterRadius,
        height=ShellZ1 - ShellZ0,
        name="CanSatShell",
    )
    inner = app.modeler.create_cylinder(
        orientation=Axis.Z,
        origin=[0, 0, ShellZ0],
        radius=ShellOuterRadius - ShellWall,
        height=ShellZ1 - ShellZ0,
        name="ShellVoid",
    )
    app.modeler.subtract(blank_list=[outer.name], tool_list=[inner.name],
                         keep_originals=False)

    # The lattice is homogenised rather than drawn hole by hole: 3 280 faces of
    # perforation would cost enormous mesh to reproduce an answer that volume
    # mixing already gives exactly, at this hole-size-to-wavelength ratio.
    if ShellCase == "AbsLattice":
        if not app.materials.exists_material("AbsLattice"):
            m = app.materials.add_material("AbsLattice")
            m.permittivity = 1.83
            m.dielectric_loss_tangent = 0.0057
        material = "AbsLattice"
    elif ShellCase == "Abs":
        material = "AbsPolymer"
    else:
        material = "CarbonFibreLaminate"

    outer.material_name = material
    # Carbon is modelled as a lossy conductor, so its interior is skipped;
    # both ABS variants are dielectrics whose interior carries the field.
    outer.solve_inside = ShellCase in ("Abs", "AbsLattice")
    outer.transparency = 0.8
    print("   shell: %s, wall %.1f mm, Z %.0f..%.0f, solve_inside=%s"
          % (material, ShellWall, ShellZ0, ShellZ1, outer.solve_inside))
    return outer


def Main():
    # create=True: this script legitimately starts from an empty design and
    # fills it.  Every other script uses create=False, which refuses to open a
    # project that comes back empty rather than risk saving over the real one.
    app = AedtSession.Connect(create=True, project_path=ProjectName,
                              design=DesignName)

    # Push the mount heights into the shared module before its builders run.
    # The derived patch heights must be recomputed, since the module worked
    # them out at import time from its own primitive-model geometry.
    rf.WhipMountZ = WhipMountZ
    rf.PatchMountZ = PatchMountZ
    rf.PatchGroundTopZ = rf.PatchMountZ + rf.PatchGroundThickness
    rf.PatchTopZ = rf.PatchGroundTopZ + rf.PatchHeight

    try:
        # The antenna builders drive their geometry from design variables so
        # they stay tunable in Optimetrics.  Those variables are declared in
        # the primitive script's BuildDesign(), which is not called here, so
        # they must be created before any builder runs - otherwise
        # create_cylinder is handed an undefined symbol and the body it
        # returns is malformed (which is what broke the first run).
        app.modeler.model_units = "mm"
        # NOTE: the comm dipole is built from literals at the vendor's 100 mm
        # length, so it deliberately has NO design variable.  Declaring one
        # that does not drive geometry is worse than none - it looks tunable
        # in Optimetrics and silently does nothing.
        app["PatchSide"] = rf.Mm(rf.PatchSide)         # GNSS resonance
        app["PatchCut"] = rf.Mm(rf.PatchTruncation)    # GNSS axial ratio
        print("\ndesign variables declared")

        print("\nclearing anything a previous run left behind")
        ClearPreviousRun(app)

        print("\nairframe geometry")
        ImportAirframeIfMissing(app)

        print("\ndefeaturing")
        Defeature(app)

        print("\nclassifying what remains")
        ClassifyOrFail(app)

        print("\nrepairing overlaps in the imported CAD")
        RepairIntersections(app)
        HealImportedBodies(app)

        print("\ncreating materials")
        rf.MakeMaterials(app)

        print("\nassigning materials")
        AssignMaterials(app)

        print("\nbuilding the avionics the STEP export omits")
        MakePcbPlanes(app)
        if BuildStandoffs:
            MakeStandoffs(app)
        else:
            print("   standoffs skipped - boards are bonded flat to the plates")
        MakeCells(app)

        print("\nbuilding the enclosure")
        MakeShell(app)

        print("\ndrilling the antenna pass-through")
        DrillAntennaClearance(app)

        print("\nbuilding antennas")
        rf.MakeCommAntenna(app)
        commTipZ = WhipMountZ - rf.CommTotalLength
        print("   comm T dipole: mount Z %.1f, tip Z %.1f, length %.0f mm"
              % (WhipMountZ, commTipZ, rf.CommTotalLength))
        print("      clearance to battery cell top (Z -36.5): %.1f mm"
              % (commTipZ - (-36.5)))
        rf.MakeGnssAntenna(app)
        print("   gnss patch: ground Z %.0f, side %.2f mm"
              % (rf.PatchMountZ, rf.PatchSide))

        separation = rf.PatchMountZ - WhipMountZ
        print("   port separation %.0f mm = %.2f lambda at %.2f MHz"
              % (separation, separation / rf.WaveLengthMm(rf.CommMainMHz),
                 rf.CommMainMHz))

        print("\nair box, radiation boundary and surface mesh")
        rf.MakeBoundaries(app)

        print("\nsolution setup and sweep")
        setup = rf.MakeSetup(app)

        print("\nfar-field spheres")
        rf.MakeFarFields(app)

        print("\nreports")
        rf.MakeReports(app, setup.name)

        print("\nobjects in design: %d" % len(app.modeler.object_names))
        print("excitations: %s" % list(app.excitation_names))
        print("boundaries: %s" % [b.name for b in app.boundaries])

        app.save_project()

        # Validate here rather than leaving it to the GUI, so a geometry
        # problem is reported in the same breath as the build that caused it.
        logPath = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               "ValidationReport.log")
        ok = app.validate_simple(log_file=logPath)
        print("\nvalidation: %s" % ("PASSED" if ok else "FAILED"))
        if not ok and os.path.exists(logPath):
            print("--- %s ---" % logPath)
            with open(logPath) as fh:
                print(fh.read())

        print("saved. NOT solved - use Simulation > Analyze All when ready.")
    finally:
        app.release_desktop(close_projects=False, close_desktop=False)


if __name__ == "__main__":
    Main()
