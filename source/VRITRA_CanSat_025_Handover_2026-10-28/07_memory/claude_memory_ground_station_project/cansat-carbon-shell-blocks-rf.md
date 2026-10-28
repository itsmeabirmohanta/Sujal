---
name: cansat-carbon-shell-blocks-rf
description: "The PDR's 1.5 mm carbon fibre shell is a Faraday cage — its stated RF transparency is physically false, and RF windows cannot fix it at 868 MHz"
metadata: 
  node_type: memory
  type: project
  originSessionId: 9bd653b2-326b-4639-b793-c952b28b745a
  modified: 2026-08-08T18:04:50.392Z
---

PDR §5.1.1 specifies the CAN-7USAT outer body as **carbon fibre, ~1.5 mm thick,
Ø130 mm, shell height 160 mm, total height 232 mm**. PDR Table 8 claims the
carbon body "allows proper transmission of LoRa and GPS signals without
requiring separate antenna openings".

**That claim is false.** CFRP is a conductor. At σ ≈ 10⁴ S/m (typical woven
quasi-isotropic laminate, in-plane):

- 867 MHz → δ = 0.171 mm → 1.5 mm = 8.8 skin depths ≈ **76 dB** absorption
- 1575 MHz → δ = 0.127 mm → 11.8 skin depths ≈ **103 dB**
- 2465 MHz → δ = 0.101 mm → 14.8 skin depths ≈ **129 dB**

The conclusion survives two decades of σ uncertainty: even at 10³ S/m the wall
is 2.8 skin depths (~24 dB). There is no plausible CFRP conductivity at which a
sealed 1.5 mm shell passes RF.

**The cheap fix does not work either.** Guidelines Appendix-A §3.0 permits
"suitable openings", but an aperture must be **> λ/2 = 173 mm** at 865.75 MHz to
radiate, and the shell is only 130 mm in diameter. Cutting windows is
arithmetically impossible at 868 MHz.

**Why:** guidelines §7(e) requires RF transparency in the structural material
choice, and Appendix-A §3.0 says conducting bodies reflect and block RF. This is
a launch-eligibility issue, not a performance tweak.

**RESOLVED in the CDR draft (2026-08-08):** the shell was changed to **ABS
filament**, plates to **aluminium**, rods to **glass fibre**. ABS is a dielectric
(εr ≈ 2.6–3.0, tanδ ≈ 0.005–0.01) — no skin-depth problem. Keep the physics
above: it is the *justification* for the change, and the CDR still carries four
stale CFRP references (system requirement table, §4.6.1 procurement, two BOM
lines, acronym list) that contradict the change.

**New RF consequence of that change:** aluminium plates at Ø110 mm are
0.9994 λ in circumference at 867 MHz — see [[cansat-plate-resonance]].

**How to apply:** `AnsysHfssGuideV2.md` Test 3 now measures ABS vs no shell
(tenths of a dB, not tens). Do not let the CDR claim RF results before that run
— the draft currently claims Ansys showed the new material better "in
perspective of stress, RF jamming", and only the structural half exists.

Related: [[cansat-plate-resonance]], [[cansat-antenna-and-link-margin]],
[[cansat-step-files-not-simulatable]]
