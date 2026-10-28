% parafoil_lib.m - shared physics for CAN-7USAT parafoil analysis
% All SI units. Sources cited inline.
1;

function [T,p,rho] = isa_atm(h_amsl)
  % International Standard Atmosphere, troposphere branch.
  T0=288.15; p0=101325; L=0.0065; R=287.058; g=9.80665;
  T   = T0 - L.*h_amsl;
  p   = p0.*(T./T0).^(g/(L*R));
  rho = p./(R.*T);
end

function CLa = lift_slope_lowAR(AR)
  % Helmbold / Diederich low-aspect-ratio lift slope, per radian.
  % Valid for AR < 4 where Prandtl lifting-line is invalid.
  CLa = (2*pi*AR)./(2 + sqrt(AR.^2 + 4));
end

function g = geom_LE()
  % As-designed canopy, LEparagliding PF072-CANSAT (lep-out.txt).
  g.name='LE PF072 (11-cell, as designed)';
  g.S=0.71; g.S_proj=0.67; g.b=1.42; g.b_proj=1.32;
  g.AR=2.82; g.AR_proj=2.62; g.c_mean=g.S/g.b;
  g.n_cells=11; g.arc_m=0.22;
  g.n_lines=18;          % load-bearing lines (6 per plan x 3 plans)
  g.R_line=0.60;         % mean line length incl. riser+cascade, m
  g.alpha_trim_deg=8.61; % LE calage
  g.GR_LE=2.70;          % LE's own finesse claim (to be tested)
end

function g = geom_CDR()
  % Superseded CDR Table 9 canopy, kept only for comparison.
  g.name='CDR Table 9 (7-cell, SUPERSEDED)';
  g.S=0.400; g.S_proj=0.400; g.b=0.960; g.b_proj=0.960;
  g.AR=2.30; g.AR_proj=2.30; g.c_mean=0.420;
  g.n_cells=7; g.arc_m=0.0;
  g.n_lines=14; g.R_line=0.87;
  g.alpha_trim_deg=8.61; g.GR_LE=2.50;
end

function a = aero_cfg()
  % Aerodynamic build-up constants.
  % CL0: cambered ram-air section, ~9% camber (CDR Table 8) -> 0.40 (Lingard, ram-air
  %      parachute design; typical cambered-plate zero-alpha lift).
  % CD0_canopy: fabric surface + inlet + rigging irregularity, 0.12 typical for small
  %      ram-air canopies (Lingard gives 0.10-0.15 for AR 2-3).
  % e: span efficiency, 0.90 for arched low-AR canopy.
  a.CL0=0.40; a.CD0_canopy=0.12; a.e=0.90;
  a.Cd_cyl=1.0;      % circular cylinder in crossflow, Re ~ 500-1000
  a.Cd_body=0.80;    % short cylinder payload, axis-on
  a.A_body=pi*(0.130^2)/4;  % CDR: 130 mm OD -> 0.01327 m^2
end

function r = glide(g, a, m, rho, alpha_deg, d_line)
  % Steady-glide equilibrium with explicit drag build-up.
  % Returns airspeed, sink rate, forward speed, glide ratio and the drag split.
  W = m*9.80665;
  al = alpha_deg*pi/180;
  CLa = lift_slope_lowAR(g.AR_proj);
  r.CL = a.CL0 + CLa*al;
  CDi  = r.CL.^2./(pi*g.AR_proj*a.e);
  % Line drag referenced to canopy area (Lingard): n*d*R*cos^3(alpha)/S
  CDl  = g.n_lines*d_line*g.R_line*cos(al).^3/g.S;
  CDb  = a.Cd_body*a.A_body/g.S;
  r.CD0=a.CD0_canopy; r.CDi=CDi; r.CDl=CDl; r.CDb=CDb;
  r.CD = a.CD0_canopy + CDi + CDl + CDb;
  r.GR = r.CL./r.CD;
  r.gamma = atan2(r.CD, r.CL);
  CR = sqrt(r.CL.^2 + r.CD.^2);
  r.V  = sqrt(2*W./(rho*g.S.*CR));
  r.Vz = r.V.*sin(r.gamma);
  r.Vx = r.V.*cos(r.gamma);
  r.WS = W/g.S;
end
