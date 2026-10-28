"""
VRITRA - CanSat RF model for Ansys HFSS 2026 R1
Team 2026-INSPACe-CAN-7USAT-025, Lovely Professional University

Builds the four RF tests the competition asks for, in one 2-port model:

    Test 1  radiation pattern      comm whip and GNSS patch, far-field spheres
    Test 2  radio interference     S21 isolation between the two antennas
    Test 3  RF transparency        same model rebuilt with three shell materials
    Test 4  polarization           axial ratio for GNSS, Ludwig-3 cuts for the whip

Run it from Ansys Electronics Desktop with  Tools > Run Script...  or from a
shell with the PyAEDT environment active.  It builds and saves; it does not
solve unless you set SolveNow below.

Values marked UNVERIFIED are engineering placeholders that no team or vendor
document states.  Tune them or measure them before quoting a result.
"""

import os

from ansys.aedt.core import Hfss
from ansys.aedt.core.generic.constants import Axis, Plane, SolutionsHfss
from ansys.aedt.core.generic.general_methods import active_sessions

# --------------------------------------------------------------------------
# 1.  Run control
# --------------------------------------------------------------------------

AedtVersion = "2026.1"
NonGraphical = False
SolveNow = False                    # leave False; press Analyze All yourself
ProjectFolder = os.path.dirname(os.path.abspath(__file__))
ProjectFile = os.path.join(ProjectFolder, "VritraRf.aedt")

# One design per shell material.  This IS the RF transparency test.
#   Abs    - what the CDR now builds (changed from CFRP at CDR stage)
#   Carbon - the PDR baseline, kept so the material change has evidence
#   Open   - the control: no shell, the condition antenna datasheets assume
# Peak gain in Open minus peak gain in Abs is the enclosure's RF cost in dB.
ShellCases = ["Abs", "Carbon", "Open"]

# A throwaway design the project can sit on while the real ones are rebuilt.
# Deleted again at the end of a successful run.
ScratchDesign = "Scratch"

# --------------------------------------------------------------------------
# 2.  Frequencies - assigned, not chosen
#
# Frequency allocation plan row 025:  867.00 MHz main / 865.75 MHz backup,
# SF10, BW 125 kHz, sync 0x98, XBee Ch 23 (2465) main / Ch 16 (2430) backup.
# Never edit these without a new allocation document.
# --------------------------------------------------------------------------

CommMainMHz = 867.00                # LoRa downlink, assigned
CommBackupMHz = 865.75              # LoRa failover, assigned
GnssL1MHz = 1575.42                 # GPS / GLONASS / NavIC L1
GnssL2MHz = 1602.00                 # GLONASS L1 - top of the GNSS band of interest
GnssL5MHz = 1176.45                 # NavIC and GPS L5 civil signal

# The flight antenna is the RadioMaster dual-band 2.4 GHz / Sub-G 900 MHz whip
# (3.58 dBi at 900 MHz, VSWR 1.25 - COMPLIANCE_AUDIT.md section 3.1), so the
# 2.4 GHz channels row 025 assigns really are reachable and ARE modelled.
CommHighMainMHz = 2465.0            # XBee Ch 23 / LR1121 2.4 GHz fallback
CommHighBackupMHz = 2430.0          # XBee Ch 16

LightSpeed = 299792458.0            # m/s, exact by definition


def WaveLengthMm(freqMHz):
    """Free-space wavelength in millimetres."""
    return LightSpeed / (freqMHz * 1e6) * 1000.0


def Mm(value):
    """Format a number as an HFSS length string without trailing zeros."""
    return "%gmm" % value


LambdaComm = WaveLengthMm(CommMainMHz)          # 345.78 mm
LambdaLowest = WaveLengthMm(CommBackupMHz)      # 346.28 mm - sets the padding
LambdaHighest = WaveLengthMm(CommHighMainMHz)   # 121.62 mm - sets the mesh

# --------------------------------------------------------------------------
# 3.  Geometry - millimetres, +Z is the yaw axis and points at the zenith
#
# Shell numbers are from the PDR section 5.1.1.  Internal plate positions are
# measured off the Fusion model.  The STEP files confirm the 65 mm outer
# radius; they are a dimensional reference only and are deliberately not
# imported - see the guide for why.
# --------------------------------------------------------------------------

ShellOuterRadius = 65.0             # PDR: shell diameter 130 mm
ShellWall = 1.5                     # PDR: carbon fibre approx 1.5 mm
ShellBottomZ = 0.0
ShellTopZ = 160.0                   # PDR: shell height 160 mm
BayTopZ = 232.0                     # PDR: total height 232 mm

PlateOuterRadius = 55.0             # Fusion: plate rim diameter 110.00 mm
PlateInnerRadius = 50.0
PlateThickness = 10.0
PlateBottomZ = [24.0, 42.0, 70.0, 96.0, 118.0]

RodRadius = 4.0                     # 4 rods, glass fibre, on a 45 mm circle
RodCircleRadius = 45.0
RodBottomZ = 20.0
RodTopZ = 130.0

BoardSide = 90.0                    # PCB ground plane, 90 x 90 mm
BoardThickness = 1.6
BoardZ = [34.0, 80.0, 106.0]

CellRadius = 10.5                   # 18650 cells
CellHeight = 70.0
CellBottomZ = 26.0
CellCircleRadius = 25.0

WheelOuterRadius = 52.0             # gyro wheel, 104 mm across
WheelInnerRadius = 46.0
WheelThickness = 5.0
WheelBottomZ = 118.0

# --------------------------------------------------------------------------
# 4.  Comm antenna - RadioMaster dual-band whip, equivalent sleeve model
#
# The RadioMaster is a vendor part whose internal element is not published.
# antenna.step gives the mechanical envelope only: 96 mm tall, 12 mm across at
# the base, three shell bodies with a 31.5 mm unmodelled gap and no feed point.
# It cannot be meshed as a radiator.
#
# A sleeve monopole is the standard dual-band whip topology and is what we
# model instead.  The upper section resonates at 2.4 GHz on its own; the whole
# element resonates at 868 MHz.  Lengths from the thin-element resonance rule
# (0.24 lambda, not 0.25 - the end effect shortens it):
#
#   total  = 0.240 * 345.78 = 82.99 mm  -> 83 mm    (fits the 96 mm envelope)
#   upper  = 0.240 * 122.36 = 29.37 mm  -> 29.4 mm  (0.24 lambda at 2450 MHz)
#   sleeve = 83 - 29.4                  -> 53.6 mm
#
# Worth noting: the 31.5 mm gap in antenna.step is close to lambda/4 at 2.4 GHz
# (30.6 mm), which is where a sleeve junction sits in a dual-band whip.  That
# is consistent with this topology.  Suggestive, not proof - do not model from
# the STEP file.
#
# What this model is good for: the CHANGE the airframe causes.  What it is not
# good for: reproducing the vendor's 3.58 dBi.  State both in the CDR.
#
# Guideline section 7(d) puts the telemetry antenna along the yaw axis facing
# the ground, so it hangs from the base plate in -Z.
# --------------------------------------------------------------------------

WhipTotalLength = 83.0
WhipUpperLength = 29.4              # UNVERIFIED - the split is a design choice
WhipSleeveLength = WhipTotalLength - WhipUpperLength
WhipRodRadius = 1.0
WhipSleeveRadius = 3.0              # UNVERIFIED - inside the 6 mm STEP envelope

# ---- RadioMaster dual-band T antenna, the actual flight part ---------------
# Vendor: T Antenna, 3.58 dBi @ 900 MHz (VSWR 1.25), 3.83 dBi @ 2.4 GHz
# (VSWR 1.27), total length 100 mm, RP-SMA, 15.78 g.
# A DIPOLE - see the long note in MakeCommAntenna for why that matters.
# GNSS patch offset from the vehicle axis, on the 45 deg diagonal at r = 30 mm.
# Puts the L89's on-module patch at the board RIM so it can see out through the
# perforated shell, while clearing the tie rods and the board edge.
PatchOffsetX = 21.21
PatchOffsetY = 21.21

CommTotalLength = 100.0             # vendor figure, not a free-space resonance
CommSleeveRadius = 3.0              # UNVERIFIED - upper arm / coax sleeve
CommRodRadius = 1.5                 # UNVERIFIED - lower arm
FeedGap = 2.0                       # lumped-port gap, must stay far below lambda
GroundDiscRadius = 15.0             # counterpoise disc at the feed
GroundDiscThickness = 1.0

# Z of the counterpoise disc, i.e. where the whip bolts on.  The whip always
# grows downward from here.  0.0 suits the primitive model, whose base plate
# sits at Z = 0; BuildFromImport.py overrides it to the imported base plate.
WhipMountZ = 0.0

# --------------------------------------------------------------------------
# 5.  GNSS antenna - ceramic patch, corner-truncated for RHCP
#
# Appendix-A section 2.1 requires RHCP with a wide beam looking at the zenith.
# The PDR's part is an external active patch covering 1575.42 / 1602 / 1176.45
# MHz over a u.FL lead; its ceramic and internal feed are not published.
#
# Square patch resonance:   L = c / (2 * f * sqrt(er))
#   at L1:  299792458 / (2 * 1575.42e6 * sqrt(20)) = 21.28 mm
#   at L5:  299792458 / (2 * 1176.45e6 * sqrt(20)) = 28.49 mm
#
# A single patch resonates at one of those, not both.  Build it at L1, sweep
# through L5, and read S22 - a genuine dual-band part shows two dips, a plain
# L1 ceramic shows one.  That is the open item in HANDOFF section 6 item 3,
# turned into a measurement.
# --------------------------------------------------------------------------

GnssBand = "L1"                     # "L1" or "L5"
PatchEpsilon = 20.0                 # UNVERIFIED - typical GPS ceramic
PatchLossTangent = 0.002            # UNVERIFIED
PatchHeight = 4.0                   # UNVERIFIED - substrate thickness
PatchGroundSide = 25.0
PatchGroundThickness = 0.5
PatchTruncation = 2.6               # UNVERIFIED - sets axial ratio, needs tuning
PatchFeedOffset = 4.5               # UNVERIFIED - sets the 50 ohm match
PatchProbeRadius = 0.3
PatchMountZ = BayTopZ + 2.0         # sits on the top deck, looking at +Z

PatchCentreMHz = GnssL1MHz if GnssBand == "L1" else GnssL5MHz
PatchSide = round(
    LightSpeed / (2.0 * PatchCentreMHz * 1e6 * PatchEpsilon ** 0.5) * 1000.0, 2
)

# Derived Z levels for the patch stack, so the port geometry cannot drift.
PatchGroundTopZ = PatchMountZ + PatchGroundThickness
PatchTopZ = PatchGroundTopZ + PatchHeight

# --------------------------------------------------------------------------
# 6.  Air box and mesh
#
# Padding is set by the LOWEST frequency, mesh by the HIGHEST.
#   padding  = lambda/4 at  865.75 MHz = 346.28 / 4 = 86.57 mm  -> 90 mm
#   rad mesh = lambda/6 at 2465.00 MHz = 121.62 / 6 = 20.27 mm  -> 20 mm
#
# The lambda/6 to lambda/8 rule on the radiation surface is Ansys's own, from
# the HFSS getting-started guide: far fields are integrated over that surface,
# so a coarse mesh there corrupts gain and axial ratio even when S11 has
# converged perfectly.
# --------------------------------------------------------------------------

AirPad = 90.0
RadiationMeshLength = 20.0

# One sweep spanning the sub-GHz comm band, both GNSS bands and 2.4 GHz.
# CUT DOWN 2026-08-09 so the thing actually solves on this laptop.
#
# The old setup was 700-2600 MHz at 951 points with three adapt frequencies.
# That is a lot of solver for results nobody is going to put in the CDR: the
# 2.4 GHz band is a fallback the mission does not use, and 2 MHz resolution is
# far finer than needed to find a resonance whose bandwidth is tens of MHz.
#
# What the CDR actually needs is 867 MHz (comm) and 1575.42 MHz (GNSS L1).
# Covering 800-1700 MHz at 5 MHz catches both bands, the 865.75 backup, and
# L5/GLONASS at 1176/1602, in a fifth of the points.
SweepStartMHz = 800.0
SweepStopMHz = 1700.0
SweepPoints = 181                   # 5 MHz step

# CUT AGAIN 2026-08-09, to 867 MHz alone, on the team's instruction.
#
# The run this replaces adapted at 867 MHz AND 1575.42 MHz and then started a
# 181-point interpolating sweep.  Its own log says exactly where the money went:
#
#     adaptive, 11 passes, converged           16:23 -> 19:30   ~3 h 07
#         Max Mag. Delta S 0.0099975  (target 0.02)     CONVERGED
#         at  867 MHz     Mag. Delta S 1.57880e-03      <- 12x inside target
#         at 1575.42 MHz  Mag. Delta S 9.99747e-03
#     interpolating sweep, 9 of ~20 basis points   19:30 -> 22:22   ~2 h 52
#         basis #9 at 1.2906775 GHz, S-matrix error still 12.629 %
#
# So the adaptation was never the problem - it converged, and converged hardest
# at the one frequency that matters.  The sweep was eating the machine, and it
# was solving basis points at 1.03, 1.14, 1.25, 1.29, 1.33, 1.41 GHz: frequencies
# this mission does not transmit on, bought at ~20 min each.
#
# Setting a single adapt frequency changes the physics of the run in two ways,
# both favourable here:
#   - the mesh is refined for 867 MHz only.  Half of the 787 840 tetrahedra in
#     the dead run existed to resolve the 1.575 GHz field, whose wavelength is
#     1.8x shorter.  Expect roughly half the elements and well under the 19.3 GB
#     peak that run hit against 23.4 GB of RAM.
#   - each adaptive pass solves one frequency instead of two, so a pass costs
#     about half what it did.
#
# What this run can no longer answer: anything at L1.  GNSS gain, axial ratio and
# handedness are OUT until a separate 1575.42 MHz run.  Isolation is unaffected -
# S(GnssPort,CommPort) at 867 MHz is a 867 MHz quantity and stays valid.
AdaptFrequencies = ["867MHz"]

# No sweep on this run.  An adaptive solve is a DISCRETE solve at the adapt
# frequency, so 867 MHz comes out of the adaptation itself - exact, converged,
# and needing no interpolation.  The 865.75 MHz backup sits 1.25 MHz away, far
# inside a resonance whose bandwidth is tens of MHz, so a sweep would not move
# any number the CDR quotes.
#
# Add one afterwards for the S11-vs-frequency figure, once the certified point
# result is safely exported - not before, because the sweep is what killed the
# last run.
MakeSweep = False
SweepBandStartMHz = 820.0           # used only when MakeSweep is True
SweepBandStopMHz = 920.0
SweepBandPoints = 101               # 1 MHz step

MaxDeltaS = 0.02
MaxPasses = 12                      # was 20; convergence normally lands by 8-10
MinConvergedPasses = 2


# --------------------------------------------------------------------------
# 7.  Materials
# --------------------------------------------------------------------------

def MakeMaterials(app):
    """Create the non-library materials this model needs.

    Carbon fibre is the important one.  It is a CONDUCTOR, not a dielectric,
    and modelling it as a plastic is the easiest way to get a wrong but
    reassuring answer out of this simulation.
    """
    if not app.materials.exists_material("CarbonFibreLaminate"):
        carbon = app.materials.add_material("CarbonFibreLaminate")
        # UNVERIFIED - no team or vendor document states the laminate's
        # conductivity.  1e4 S/m is mid-range for a woven quasi-isotropic
        # CFRP skin measured in plane.  Re-run at 1e3 and 1e5 to bracket it.
        carbon.conductivity = 10000.0
        carbon.permittivity = 6.0
        carbon.dielectric_loss_tangent = 0.05

    if not app.materials.exists_material("AbsPolymer"):
        abs_ = app.materials.add_material("AbsPolymer")
        # UNVERIFIED - typical printed ABS near 1 GHz.  A pure dielectric: no
        # bulk conductivity, so no skin depth and no shielding.  Infill matters
        # more than the resin - a 50% infill part behaves like er ~1.8.
        abs_.permittivity = 2.7
        abs_.dielectric_loss_tangent = 0.008

    if not app.materials.exists_material("GlassFibreLaminate"):
        glass = app.materials.add_material("GlassFibreLaminate")
        # UNVERIFIED - typical woven E-glass / epoxy near 1 GHz.
        glass.permittivity = 4.4
        glass.dielectric_loss_tangent = 0.015

    if not app.materials.exists_material("CellSteel"):
        steel = app.materials.add_material("CellSteel")
        # 18650 cans, as a LINEAR conductor.  Do not reach for the library's
        # steel_1008: it carries a nonlinear B-H permeability from the Maxwell
        # library and HFSS rejects it outright with
        #   "the data type for the property, permeability, ... is not supported
        #    in the hfss solution"
        # UNVERIFIED - 1.45e6 S/m is austenitic stainless.  A nickel-plated
        # magnetic can would be lossier; only the surface impedance changes,
        # and these are solve_inside=False anyway.
        steel.conductivity = 1450000.0
        steel.permeability = 1.0

    if not app.materials.exists_material("PatchCeramic"):
        ceramic = app.materials.add_material("PatchCeramic")
        ceramic.permittivity = PatchEpsilon
        ceramic.dielectric_loss_tangent = PatchLossTangent


# --------------------------------------------------------------------------
# 8.  Geometry
# --------------------------------------------------------------------------

def MakeShell(app, shellCase):
    """Outer body, drawn as a real tube so the wall thickness is honest."""
    if shellCase == "Open":
        return

    material = {
        "Abs": "AbsPolymer",
        "Carbon": "CarbonFibreLaminate",
        "Glass": "GlassFibreLaminate",
    }[shellCase]

    outer = app.modeler.create_cylinder(
        orientation=Axis.Z,
        origin=[0, 0, ShellBottomZ],
        radius=ShellOuterRadius,
        height=BayTopZ - ShellBottomZ,
        name="ShellOuter",
        material=material,
    )
    bore = app.modeler.create_cylinder(
        orientation=Axis.Z,
        origin=[0, 0, ShellBottomZ],
        radius=ShellOuterRadius - ShellWall,
        height=BayTopZ - ShellBottomZ,
        name="ShellBore",
        material=material,
    )
    app.modeler.subtract(blank_list=[outer.name], tool_list=[bore.name], keep_originals=False)
    outer.name = "CanSatShell"

    if shellCase == "Carbon":
        # A conductor many skin depths thick should not be meshed internally.
        # At 867 MHz and 1e4 S/m the skin depth is 0.171 mm, so 1.5 mm is
        # roughly 8.8 skin depths and nothing reaches the far side.
        outer.solve_inside = False


def MakeStructure(app):
    """Internal frame: plates, rods, boards, cells, gyro wheel."""
    for index, zBottom in enumerate(PlateBottomZ, start=1):
        disc = app.modeler.create_cylinder(
            orientation=Axis.Z,
            origin=[0, 0, zBottom],
            radius=PlateOuterRadius,
            height=PlateThickness,
            name="PlateDisc%d" % index,
            material="aluminum",
        )
        hole = app.modeler.create_cylinder(
            orientation=Axis.Z,
            origin=[0, 0, zBottom],
            radius=PlateInnerRadius,
            height=PlateThickness,
            name="PlateHole%d" % index,
            material="aluminum",
        )
        app.modeler.subtract(blank_list=[disc.name], tool_list=[hole.name], keep_originals=False)
        disc.name = "Plate%d" % index
        disc.solve_inside = False

    rod = app.modeler.create_cylinder(
        orientation=Axis.Z,
        origin=[RodCircleRadius, 0, RodBottomZ],
        radius=RodRadius,
        height=RodTopZ - RodBottomZ,
        name="TieRod",
        material="GlassFibreLaminate",
    )
    app.modeler.duplicate_around_axis(
        assignment=rod.name, axis=Axis.Z, angle=90, clones=4, create_new_objects=True
    )

    for index, z in enumerate(BoardZ, start=1):
        board = app.modeler.create_box(
            origin=[-BoardSide / 2.0, -BoardSide / 2.0, z],
            sizes=[BoardSide, BoardSide, BoardThickness],
            name="Board%d" % index,
            material="copper",
        )
        board.solve_inside = False

    cell = app.modeler.create_cylinder(
        orientation=Axis.Z,
        origin=[CellCircleRadius, 0, CellBottomZ],
        radius=CellRadius,
        height=CellHeight,
        name="Cell",
        material="CellSteel",
    )
    cell.solve_inside = False
    made, cellCopies = app.modeler.duplicate_around_axis(
        assignment=cell.name, axis=Axis.Z, angle=120, clones=3, create_new_objects=True
    )
    if made:
        for cloneName in cellCopies:
            clone = app.modeler.get_object_from_name(cloneName)
            if clone:
                clone.solve_inside = False

    wheel = app.modeler.create_cylinder(
        orientation=Axis.Z,
        origin=[0, 0, WheelBottomZ],
        radius=WheelOuterRadius,
        height=WheelThickness,
        name="WheelDisc",
        material="aluminum",
    )
    wheelBore = app.modeler.create_cylinder(
        orientation=Axis.Z,
        origin=[0, 0, WheelBottomZ],
        radius=WheelInnerRadius,
        height=WheelThickness,
        name="WheelBore",
        material="aluminum",
    )
    app.modeler.subtract(blank_list=[wheel.name], tool_list=[wheelBore.name], keep_originals=False)
    wheel.name = "GyroWheel"
    wheel.solve_inside = False


def MakeCommAntenna(app):
    """Dual-band sleeve whip hanging off the -Z end, plus its lumped port.

    Z layout, all relative to WhipMountZ so the feed sheet cannot end up
    floating.  With WhipMountZ = 0 that reads:
        disc      0     .. +1
        feed     -2     ..  0      <- lumped port lives here
        sleeve  -55.6   .. -2
        rod     -85     .. -55.6
    """
    # ---- vendor data, radiomasterrc.com, read 2026-08-09 -------------------
    #   type          T Antenna, omnidirectional
    #   900 MHz       3.58 dBi peak, VSWR 1.25
    #   2.4 GHz       3.83 dBi peak, VSWR 1.27
    #   total length  100 mm   connector RP-SMA   weight 15.78 g
    #
    # Two corrections over the previous model, both material:
    #
    # 1. It is a DIPOLE, not a monopole.  No counterpoise disc.  3.58 dBi is
    #    dipole-class (ideal dipole 2.15 dBi); a monopole over ground would be
    #    ~5.15 dBi.  A monopole on a small disc radiates asymmetrically and
    #    tilts its beam - a dipole gives the symmetric doughnut the vendor
    #    claims.  The old model had the wrong topology.
    #
    # 2. It is 100 mm, not the 166 mm a free-space resonant dipole needs at
    #    867 MHz.  The real part is electrically loaded to fit and RadioMaster
    #    does not publish how, so we model the true 100 mm envelope.  Each arm
    #    is then 0.142 lambda at 867 MHz - electrically short.
    #
    # What that costs, plainly: S11 here will look worse than the vendor's
    # VSWR 1.25 because we are not modelling their loading.  The PATTERN is
    # still right - a short dipole and a half-wave dipole share almost the same
    # sin(theta) doughnut, differing in match and efficiency, not shape.
    # Pattern and isolation are what this study is for.  Do NOT quote absolute
    # gain from this model.
    #
    # Z layout hanging down from WhipMountZ:
    #   upper arm   WhipMountZ-49  .. WhipMountZ
    #   feed gap    WhipMountZ-51  .. WhipMountZ-49    <- lumped port
    #   lower arm   WhipMountZ-100 .. WhipMountZ-51
    # ------------------------------------------------------------------------
    armLength = (CommTotalLength - FeedGap) / 2.0
    upperBottomZ = WhipMountZ - armLength
    feedBottomZ = upperBottomZ - FeedGap
    lowerBottomZ = WhipMountZ - CommTotalLength

    # Upper arm - the coax sleeve, fatter as on the real part.
    upper = app.modeler.create_cylinder(
        orientation=Axis.Z,
        origin=[0, 0, Mm(upperBottomZ)],
        radius=CommSleeveRadius,
        height=Mm(armLength),
        name="CommArmUpper",
        material="copper",
    )
    upper.solve_inside = False

    # Lower arm - the exposed radiator.
    lower = app.modeler.create_cylinder(
        orientation=Axis.Z,
        origin=[0, 0, Mm(lowerBottomZ)],
        radius=CommRodRadius,
        height=Mm(armLength),
        name="CommArmLower",
        material="copper",
    )
    lower.solve_inside = False

    # Feed sheet bridging the two arms.  ZX plane -> sizes are [along Z, X].
    feed = app.modeler.create_rectangle(
        orientation=Plane.ZX,
        origin=[-CommSleeveRadius, 0, feedBottomZ],
        sizes=[FeedGap, 2.0 * CommSleeveRadius],
        name="CommFeed",
    )

    # Explicit two-point integration line.  Passing an axis constant works but
    # leaves the sign ambiguous; coordinates do not.
    return app.lumped_port(
        assignment=feed.name,
        integration_line=[[0.0, 0.0, feedBottomZ], [0.0, 0.0, upperBottomZ]],
        impedance=50.0,
        name="CommPort",
        renormalize=True,
    )


def MakeGnssAntenna(app):
    """The patch integrated on the 7Semi L89HA module - there is NO external
    GNSS antenna on this vehicle.

    Placed at the RIM of the top avionics board, not on the axis, so it looks
    out through the perforated shell rather than straight up into the plate
    stack.  Offsets are on the 45 degree diagonal, which is the only direction
    that clears both the tie rods at (+-46,0),(0,+-46) and the Oe100 board edge:

        centre at r = 30 mm, 45 deg  ->  (21.21, 21.21)
        patch corners span 8.7 .. 33.7 mm in x and y
        worst-case corner radius = sqrt(2) * 33.71 = 47.7 mm  <  50 mm board OK
        centre-to-rod distance    = 32.6 mm
        patch half-diagonal + rod hole = 17.7 + 4.5 = 22.2 mm  <  32.6  OK
    """
    ox, oy = PatchOffsetX, PatchOffsetY

    ground = app.modeler.create_box(
        origin=[-PatchGroundSide / 2.0 + ox, -PatchGroundSide / 2.0 + oy, PatchMountZ],
        sizes=[PatchGroundSide, PatchGroundSide, PatchGroundThickness],
        name="PatchGround",
        material="copper",
    )
    ground.solve_inside = False

    app.modeler.create_box(
        origin=[-PatchGroundSide / 2.0 + ox, -PatchGroundSide / 2.0 + oy, PatchGroundTopZ],
        sizes=[PatchGroundSide, PatchGroundSide, PatchHeight],
        name="PatchCeramicBlock",
        material="PatchCeramic",
    )

    # Patch edges and corner cuts are driven by design variables so axial ratio
    # can be tuned from Optimetrics.  Neither moves the probe or the feed sheet,
    # so the port stays valid across a sweep.
    patch = app.modeler.create_rectangle(
        orientation=Plane.XY,
        origin=["-PatchSide/2 + %s" % Mm(ox), "-PatchSide/2 + %s" % Mm(oy), PatchTopZ],
        sizes=["PatchSide", "PatchSide"],
        name="GnssPatch",
    )

    # Two opposite corners cut away.  This splits the patch into two degenerate
    # modes 90 degrees apart in phase and produces circular polarization from a
    # single feed.  PatchTruncation drives axial ratio directly and is the
    # parameter to sweep if AR comes out above 3 dB.
    cornerA = app.modeler.create_box(
        origin=["-PatchSide/2 + %s" % Mm(ox), "-PatchSide/2 + %s" % Mm(oy),
                PatchTopZ - 0.5],
        sizes=["PatchCut", "PatchCut", 1.0],
        name="CornerA",
    )
    cornerB = app.modeler.create_box(
        origin=["PatchSide/2 - PatchCut + %s" % Mm(ox),
                "PatchSide/2 - PatchCut + %s" % Mm(oy), PatchTopZ - 0.5],
        sizes=["PatchCut", "PatchCut", 1.0],
        name="CornerB",
    )
    app.modeler.subtract(
        blank_list=[patch.name], tool_list=[cornerA.name, cornerB.name], keep_originals=False
    )
    app.assign_perfecte_to_sheets(assignment=patch.name, name="PatchMetal")

    # Coax probe from the ground plane up to the patch, offset along +X to find
    # 50 ohms.
    probeBottomZ = PatchGroundTopZ + FeedGap
    probeX = PatchFeedOffset + ox
    probeY = oy
    probe = app.modeler.create_cylinder(
        orientation=Axis.Z,
        origin=[probeX, probeY, probeBottomZ],
        radius=PatchProbeRadius,
        height=PatchTopZ - probeBottomZ,
        name="PatchProbe",
        material="copper",
    )
    probe.solve_inside = False

    feed = app.modeler.create_rectangle(
        orientation=Plane.ZX,
        origin=[probeX - PatchProbeRadius, probeY, PatchGroundTopZ],
        sizes=[FeedGap, 2.0 * PatchProbeRadius],
        name="GnssFeed",
    )
    return app.lumped_port(
        assignment=feed.name,
        integration_line=[
            [probeX, probeY, PatchGroundTopZ],
            [probeX, probeY, probeBottomZ],
        ],
        impedance=50.0,
        name="GnssPort",
        renormalize=True,
    )


# --------------------------------------------------------------------------
# 9.  Boundaries, solution setup, far fields
# --------------------------------------------------------------------------

def MakeBoundaries(app):
    """Air box with a radiation boundary, and the lambda/6 surface mesh."""
    region = app.modeler.create_region(
        pad_value=[Mm(AirPad)] * 6,             # order is +X -X +Y -Y +Z -Z
        pad_type="Absolute Offset",
        name="AirBox",
    )
    app.assign_radiation_boundary_to_objects(assignment=region.name, name="OpenAir")

    # Far fields are a surface integral over this box.  Under-meshing it is the
    # classic way to get a converged S11 and a nonsense pattern.
    app.mesh.assign_length_mesh(
        assignment=[region.name],
        inside_selection=False,
        maximum_length=Mm(RadiationMeshLength),
        maximum_elements=None,                  # do not cap the element count
        name="RadBoxMesh",
    )
    return region


def MakeSetup(app):
    """Driven-modal setup adapted at all three working bands at once."""
    setup = app.create_setup(
        name="SolveAllBands",
        Frequency="%gMHz" % CommMainMHz,
        MaxDeltaS=MaxDeltaS,
        MaximumPasses=MaxPasses,
        MinimumConvergedPasses=MinConvergedPasses,
        SaveAnyFields=True,
        SaveRadFieldsOnly=False,
    )
    # Multi-frequency adaptation only makes sense with more than one band in
    # play.  Handing enable_adaptive_setup_multifrequency() a single entry sets
    # up the multi-frequency machinery to do a job the plain single-frequency
    # adaptation already does, so match the mechanism to the list.
    if len(AdaptFrequencies) > 1:
        # One mesh correct at every listed band.  Adapting at a single
        # frequency and then trusting a sweep across a 3.6:1 span is how
        # dual-band antenna models go quietly wrong.
        setup.enable_adaptive_setup_multifrequency(
            frequencies=AdaptFrequencies, max_delta_s=MaxDeltaS
        )
        print("   adaptation: multi-frequency at %s"
              % ", ".join(AdaptFrequencies))
    else:
        setup.props["Frequency"] = AdaptFrequencies[0]
        setup.props["UseMultiFrequency"] = False
        setup.update()
        print("   adaptation: single frequency at %s" % AdaptFrequencies[0])

    if not MakeSweep:
        print("   sweep: NONE - 867 MHz comes from the adaptive solve itself, "
              "which is a discrete converged solve at that frequency")
        return setup

    app.create_linear_count_sweep(
        setup=setup.name,
        unit="MHz",
        start_frequency=SweepBandStartMHz,
        stop_frequency=SweepBandStopMHz,
        num_of_freq_points=SweepBandPoints,
        name="CommBandSweep",
        sweep_type="Interpolating",
        save_fields=False,
        save_rad_fields=False,
    )
    print("   sweep: %g-%g MHz, %d points, Interpolating"
          % (SweepBandStartMHz, SweepBandStopMHz, SweepBandPoints))
    return setup


def MakeFarFields(app):
    """Three radiation spheres, one per question we are asking."""
    # Full 3-D pattern - test 1.
    app.insert_infinite_sphere(
        definition="Theta-Phi",
        theta_start=0, theta_stop=180, theta_step=2,
        phi_start=0, phi_stop=360, phi_step=2,
        name="FullSphere",
    )
    # Principal-plane cuts - tests 1 and 4.  Theta runs -180 to 180 so both
    # halves of the cut land on one polar plot.
    app.insert_infinite_sphere(
        definition="Theta-Phi",
        theta_start=-180, theta_stop=180, theta_step=1,
        phi_start=0, phi_stop=90, phi_step=90,
        name="ElevationCuts",
    )
    # Upper hemisphere only - the GNSS field of view.  Axial ratio is read
    # here, out to the elevation mask the receiver actually uses.
    app.insert_infinite_sphere(
        definition="Theta-Phi",
        theta_start=0, theta_stop=90, theta_step=1,
        phi_start=0, phi_stop=360, phi_step=5,
        name="ZenithCone",
    )


def SafeReport(app, label, **kwargs):
    """Create one report; never let a bad expression name abort the build."""
    try:
        if app.post.create_report(**kwargs):
            return True
        app.logger.warning("report %s was not created", label)
    except Exception as exc:                    # noqa: BLE001 - report only
        app.logger.warning("report %s failed: %s", label, exc)
    return False


def MakeReports(app, setupName):
    """Pre-build the plots so results are one click away after solving."""
    # Never hardcode the sweep name.  create_linear_count_sweep() does not
    # overwrite an existing name, it appends a random suffix - so on a re-run
    # the sweep can come back as "WideSweep_9YUHVQ" and every report asking for
    # "<setup> : WideSweep" fails with "Setup doesn't exist in this design",
    # which points the blame at the setup rather than the sweep.
    # Ask AEDT directly rather than reading app.setups, which is a cached
    # Python list: immediately after the setup is created it is still stale,
    # and every sweep-context report then fails.
    live = []
    try:
        live = [s for s in (app.oanalysis.GetSweeps(setupName) or [])]
    except Exception:                               # noqa: BLE001
        pass

    # With MakeSweep off there is no sweep to point at, and that is not a
    # degraded case: an adaptive pass IS a discrete solve at the adapt
    # frequency, so "LastAdaptive" carries a real, converged 867 MHz point.
    # A report asking for a sweep that does not exist fails with
    # "Setup doesn't exist in this design", which blames the setup.
    if live:
        sParamContext = "%s : %s" % (setupName, live[0])
        sweepVar = "Freq"
        print("   reports: S-parameters vs frequency over sweep %s" % live[0])
    else:
        sParamContext = "%s : LastAdaptive" % setupName
        sweepVar = None
        print("   reports: S-parameters at the adapt frequency (no sweep)")

    fieldContext = "%s : LastAdaptive" % setupName

    # Trap: this design is solution type Terminal, not Modal.  Terminal
    # designs express S-parameters as St(...) under report category
    # "Terminal Solution Data"; asking for S(...) with the default category is
    # how every S-parameter report in this project came back empty before.
    terminal = str(app.solution_type).lower().startswith("terminal")
    sFunc = "St" if terminal else "S"
    sCat = "Terminal Solution Data" if terminal else "Modal Solution Data"
    print("   reports: solution type %s -> %s(), category %r"
          % (app.solution_type, sFunc, sCat))

    def SParamReport(label, expressions, plot):
        kw = dict(expressions=expressions, setup_sweep_name=sParamContext,
                  report_category=sCat, plot_name=plot)
        if sweepVar:
            kw["primary_sweep_variable"] = sweepVar
        SafeReport(app, label, **kw)

    # Tests 1 and 2 - matching and isolation.
    SParamReport(
        "ReturnLoss",
        ["dB(%s(CommPort,CommPort))" % sFunc,
         "dB(%s(GnssPort,GnssPort))" % sFunc],
        "ReturnLoss",
    )
    SParamReport(
        "Isolation",
        ["dB(%s(GnssPort,CommPort))" % sFunc],
        "Isolation",
    )
    SParamReport(
        "Vswr",
        ["VSWR(CommPort)", "VSWR(GnssPort)"],
        "Vswr",
    )

    # Test 1 - patterns.
    SafeReport(
        app, "PatternThreeD",
        expressions=["dB(RealizedGainTotal)"],
        setup_sweep_name=fieldContext,
        report_category="Far Fields",
        plot_type="3D Polar Plot",
        context="FullSphere",
        plot_name="PatternThreeD",
    )
    SafeReport(
        app, "ElevationPattern",
        expressions=["dB(RealizedGainTotal)"],
        setup_sweep_name=fieldContext,
        report_category="Far Fields",
        plot_type="Radiation Pattern",
        context="ElevationCuts",
        primary_sweep_variable="Theta",
        plot_name="ElevationPattern",
    )

    # Test 4 - polarization.  Axial ratio over the GNSS field of view, and the
    # RHCP/LHCP split that says which way the patch is actually turning.
    SafeReport(
        app, "GnssAxialRatio",
        expressions=["dB(AxialRatioValue)"],
        setup_sweep_name=fieldContext,
        report_category="Far Fields",
        plot_type="Radiation Pattern",
        context="ZenithCone",
        primary_sweep_variable="Theta",
        plot_name="GnssAxialRatio",
    )
    SafeReport(
        app, "GnssHandedness",
        expressions=["dB(RealizedGainRHCP)", "dB(RealizedGainLHCP)"],
        setup_sweep_name=fieldContext,
        report_category="Far Fields",
        plot_type="Radiation Pattern",
        context="ZenithCone",
        primary_sweep_variable="Theta",
        plot_name="GnssHandedness",
    )
    # The whip is linearly polarized and has to line up with the ground Yagi.
    # Co-pol against cross-pol is the number that says by how much.
    SafeReport(
        app, "CommPolarization",
        expressions=["dB(RealizedGainTheta)", "dB(RealizedGainPhi)"],
        setup_sweep_name=fieldContext,
        report_category="Far Fields",
        plot_type="Radiation Pattern",
        context="ElevationCuts",
        primary_sweep_variable="Theta",
        plot_name="CommPolarization",
    )


# --------------------------------------------------------------------------
# 10.  Build
# --------------------------------------------------------------------------

def BuildDesign(app, shellCase):
    app.modeler.model_units = "mm"

    # Parametric handles, so the obvious tuning knobs are editable in the GUI
    # without opening this file again.
    # Tunable from the GUI / Optimetrics - these four drive real geometry.
    app["WhipTotal"] = Mm(WhipTotalLength)      # comm resonance at 868 MHz
    app["WhipUpper"] = Mm(WhipUpperLength)      # comm resonance at 2.4 GHz
    app["PatchSide"] = Mm(PatchSide)            # GNSS resonance
    app["PatchCut"] = Mm(PatchTruncation)       # GNSS axial ratio
    # PatchProbeOffset is deliberately NOT a driving variable.  Moving it would
    # move the probe, the feed sheet AND the port integration line together;
    # a sweep would leave the line hanging off the sheet.  To retune the match,
    # edit PatchFeedOffset at the top of this file and re-run.

    MakeMaterials(app)
    MakeShell(app, shellCase)
    MakeStructure(app)
    MakeCommAntenna(app)
    MakeGnssAntenna(app)
    MakeBoundaries(app)
    setup = MakeSetup(app)
    MakeFarFields(app)
    MakeReports(app, setup.name)
    return setup


def Main():
    print("wavelengths  comm %.2f mm | lowest %.2f mm | highest %.2f mm"
          % (LambdaComm, LambdaLowest, LambdaHighest))
    print("patch side at %s: %.2f mm" % (GnssBand, PatchSide))
    print("air padding %.1f mm | radiation mesh %.1f mm" % (AirPad, RadiationMeshLength))
    print("whip: total %.1f mm, sleeve %.1f mm, upper %.1f mm"
          % (WhipTotalLength, WhipSleeveLength, WhipUpperLength))

    # Attach to an Ansys Electronics Desktop that is already open, so the model
    # appears in the window you are watching.  active_sessions() returns
    # {process id: grpc port}.  If nothing is running, launch a fresh one.
    sessions = active_sessions()
    if sessions:
        pid, port = sorted(sessions.items())[0]
        print("attaching to the open AEDT session: pid %s, port %s" % (pid, port))
    else:
        pid, port = None, 0
        print("no AEDT session found - launching one")

    # The constructor has to open the project on SOME design.  Give it a
    # throwaway, because deleting whichever design is currently active leaves
    # PyAEDT holding a null handle and the next insert_design fails with
    # "NoneType has no attribute SetSolutionType".  Parking on Scratch means
    # every real design can be deleted and rebuilt safely.
    hfss = Hfss(
        project=ProjectFile,
        design=ScratchDesign,
        solution_type=SolutionsHfss.DrivenModal,
        version=AedtVersion,
        non_graphical=NonGraphical,
        new_desktop=False,
        port=port,
        aedt_process_id=pid,
    )
    try:
        for shellCase in ShellCases:
            designName = "Shell%s" % shellCase

            # Rebuild from scratch every run.  Building twice into the same
            # design stacks duplicate geometry on top of itself, which meshes
            # and solves and is completely wrong.  fallback_design keeps a live
            # design selected while this one is removed.
            if designName in hfss.design_list:
                hfss.delete_design(name=designName, fallback_design=ScratchDesign)

            # Create the design with the CONSTRUCTOR, not insert_design.
            # insert_design fails intermittently right after a delete of the
            # same name - AEDT reports "unable to locate design: <name>" and
            # PyAEDT then dies on a null handle.  A fresh Hfss object rebuilds
            # its own handles from the project and is reliable.
            app = Hfss(
                project=ProjectFile,
                design=designName,
                solution_type=SolutionsHfss.DrivenModal,
                version=AedtVersion,
                non_graphical=NonGraphical,
                new_desktop=False,
                port=port,
                aedt_process_id=pid,
            )

            print("building %s" % designName)
            setup = BuildDesign(app, shellCase)
            print("   %d objects" % len(app.modeler.object_names))

            if SolveNow:
                if app.validate_simple() == 1:
                    app.analyze_setup(name=setup.name)
                else:
                    print("validation failed on %s - not solving" % designName)

            hfss = app         # keep working through the newest handle

        # Tidy up: drop the throwaway, landing on the first real design.
        if ScratchDesign in hfss.design_list:
            hfss.delete_design(name=ScratchDesign,
                               fallback_design="Shell%s" % ShellCases[0])

        hfss.save_project()
        print("saved %s" % ProjectFile)
        print("designs now in the project: %s" % hfss.design_list)
    finally:
        # Leave the desktop open so you can look at what was built.
        hfss.release_desktop(close_projects=False, close_desktop=False)


if __name__ == "__main__":
    Main()
