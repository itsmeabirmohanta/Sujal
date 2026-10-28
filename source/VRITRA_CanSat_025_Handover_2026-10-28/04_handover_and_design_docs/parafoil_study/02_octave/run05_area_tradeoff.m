% run05_area_tradeoff.m - DECISIVE: does a smaller canopy land better?
% Cloth is uncut, so canopy area is still a free design variable.
source('parafoil_lib.m');
a=aero_cfg(); a.CD0_canopy=0.18;
SITE=80; [~,~,rho6]=isa_atm(SITE+600); d=0.0006; t1=400/18;

function w=wind_at(h,w10,al), w=w10.*max(h,1).^al./(10^al); end

% Corrected guidance: track the target, biased to approach from downwind.
% Below h_fin it flies a track THROUGH the target aligned with the wind.
function [xf,yf]=fly_v2(r,w10,wdir,x0,y0,h0,trmax)
  dt=0.25; x=x0;y=y0;h=h0;psi=atan2(-x,-y); h_fin=120;
  while h>0
    w=wind_at(h,w10,0.17); wx=w*sin(wdir); wy=w*cos(wdir);
    if h>h_fin
      % aim at an initial point downwind of target so final leg is into wind
      wf=wind_at(h_fin/2,w10,0.17); gs=max(r.Vx-wf,0.4);
      dfin=gs*(h_fin/r.Vz);
      ipx=dfin*sin(wdir); ipy=dfin*cos(wdir);
      dx=ipx-x; dy=ipy-y; dr=hypot(dx,dy);
      if dr>40, pc=atan2(dx,dy); else pc=atan2(dx,dy)+pi/2; end
    else
      % final: steer at target, with crosswind correction
      pc=atan2(-x,-y);
      ca=-wx*cos(pc)+wy*sin(pc);      % crosswind component
      pc=pc+asin(max(-0.9,min(0.9,ca/max(r.Vx,0.5))));
    end
    e=atan2(sin(pc-psi),cos(pc-psi));
    psi=psi+max(-trmax*dt,min(trmax*dt,e));
    x=x+(r.Vx*sin(psi)+wx)*dt; y=y+(r.Vx*cos(psi)+wy)*dt; h=h-r.Vz*dt;
  end
  xf=x;yf=y;
end

printf('=== AREA TRADE-OFF: landing accuracy vs canopy area ===\n');
printf('CD0=0.18, m=0.860 kg, AR held at 2.62, 1200-run Monte Carlo each\n\n');
printf('%7s %7s %7s %7s %8s %8s %8s %8s %9s %9s\n', ...
  'S[m2]','Vz','Vx','GR','R-08','CEP50','CEP90','CEP95','<50m','P(blown)');
N=1200;
for S=[0.30 0.35 0.40 0.50 0.60 0.71]
  g=geom_LE(); g.S=S; g.b=sqrt(S*2.62); g.R_line=0.60*sqrt(S/0.71);
  rn=glide(g,a,0.860,rho6,g.alpha_trim_deg,d);
  ok='PASS'; if rn.Vz<1.0||rn.Vz>3.0, ok='FAIL'; end
  rand('seed',11); randn('seed',11);
  ms=zeros(N,1); nb=0;
  for i=1:N
    w10=min(1.0+1.2*abs(randn()),7.0); wdir=2*pi*rand();
    aa=a; aa.CD0_canopy=0.15+0.08*rand();
    rr=glide(g,aa,0.860+0.04*randn(),rho6,g.alpha_trim_deg,d);
    if wind_at(300,w10,0.17)>rr.Vx, nb=nb+1; end
    off=wind_at(800,w10,0.17)*t1;
    [xf,yf]=fly_v2(rr,w10,wdir,-off*sin(wdir),-off*cos(wdir),600,0.25);
    ms(i)=hypot(xf,yf);
  end
  s=sort(ms);
  printf('%7.2f %7.2f %7.2f %7.2f %8s %8.1f %8.1f %8.1f %8.1f%% %8.1f%%\n', ...
    S,rn.Vz,rn.Vx,rn.GR,ok,s(round(.5*N)),s(round(.9*N)),s(round(.95*N)), ...
    100*sum(ms<50)/N,100*nb/N);
end

printf('\n=== WHY: penetration margin drives the tail ===\n');
printf('%7s %8s %22s\n','S[m2]','Vx','P(wind@300m > Vx)');
rand('seed',3); randn('seed',3);
W=min(1.0+1.2*abs(randn(20000,1)),7.0);
w300=wind_at(300,W,0.17);
for S=[0.30 0.40 0.50 0.71]
  g=geom_LE(); g.S=S; g.R_line=0.60*sqrt(S/0.71);
  rn=glide(g,a,0.860,rho6,g.alpha_trim_deg,d);
  printf('%7.2f %8.2f %21.1f%%\n',S,rn.Vx,100*mean(w300>rn.Vx));
end
