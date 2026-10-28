% run12_plots.m - mission visuals: descent profile, ground track, dispersion, polar
graphics_toolkit("gnuplot");
set(0,"defaultfigurevisible","off");
disp(["toolkit in use: " graphics_toolkit()]);
source('parafoil_lib.m');
g=geom_LE(); SITE=80; [~,~,rho]=isa_atm(SITE+600);
d=0.0006; CL0=0.0334; CDp=0.053; M=1.000;
a=aero_cfg(); a.CL0=CL0; a.CD0_canopy=CDp+0.0825;
r=glide(g,a,M,rho,g.alpha_trim_deg,d);
printf('1.0 kg trim: GR %.2f Vz %.2f Vx %.2f\n',r.GR,r.Vz,r.Vx);
OUT='D:/linux/Software/Claude/cansat_parafoil/09_plots';
mkdir(OUT);
function w=wind_at(h,w10,al), w=w10.*max(h,1).^al./(10^al); end
function [X,Y,H,T]=fly(r,w10,wdir,x0,y0,h0,trmax)
  dt=0.5; x=x0;y=y0;h=h0;psi=atan2(-x,-y); hf=120; X=[];Y=[];H=[];T=[];t=0;
  while h>0
    w=wind_at(h,w10,0.17); wx=w*sin(wdir); wy=w*cos(wdir);
    if h>hf
      wf=wind_at(hf/2,w10,0.17); gs=max(r.Vx-wf,0.4); df=gs*(hf/r.Vz);
      dx=df*sin(wdir)-x; dy=df*cos(wdir)-y; dr=hypot(dx,dy);
      if dr>40, pc=atan2(dx,dy); else pc=atan2(dx,dy)+pi/2; end
    else
      pc=atan2(-x,-y); ca=-wx*cos(pc)+wy*sin(pc);
      pc=pc+asin(max(-0.9,min(0.9,ca/max(r.Vx,0.5))));
    end
    e=atan2(sin(pc-psi),cos(pc-psi));
    psi=psi+max(-trmax*dt,min(trmax*dt,e));
    x=x+(r.Vx*sin(psi)+wx)*dt; y=y+(r.Vx*cos(psi)+wy)*dt; h=h-r.Vz*dt; t=t+dt;
    X=[X x];Y=[Y y];H=[H h];T=[T t];
  end
end
t1=400/18;
% --- figure 1: altitude vs time, three winds ---
f=figure(); set(f,'position',[0 0 900 600]);
cols={'b','g','r'}; k=1; leg={};
for w10=[1.0 2.0 3.0]
  off=wind_at(800,w10,0.17)*t1;
  [X,Y,H,T]=fly(r,w10,0,0,-off,600,0.25);
  plot(T,H,cols{k},'linewidth',2); hold on;
  leg{k}=sprintf('wind %.0f m/s @10m (land %.0f m off)',w10,hypot(X(end),Y(end))); k++;
end
grid on; xlabel('time since 600 m deployment  [s]'); ylabel('altitude AGL  [m]');
title(sprintf('CAN-7USAT descent under PF072 parafoil, 1.0 kg  (Vz=%.2f m/s, %.0f s)',r.Vz,600/r.Vz));
legend(leg,'location','northeast'); ylim([0 620]);
print(f,[OUT '/01_altitude_vs_time.png'],'-dpng','-r110'); close(f);
% --- figure 2: ground track ---
f=figure(); set(f,'position',[0 0 800 800]);
k=1; leg={};
for w10=[1.0 2.0 3.0]
  off=wind_at(800,w10,0.17)*t1;
  [X,Y,H,T]=fly(r,w10,0,0,-off,600,0.25);
  plot(X,Y,cols{k},'linewidth',1.6); hold on;
  plot(X(1),Y(1),[cols{k} 'o'],'markersize',9,'linewidth',2);
  plot(X(end),Y(end),[cols{k} 'x'],'markersize',12,'linewidth',2);
  leg{k}=sprintf('wind %.0f m/s',w10); k++;
end
plot(0,0,'k*','markersize',18,'linewidth',2);
th=linspace(0,2*pi,100);
plot(50*cos(th),50*sin(th),'k--'); plot(100*cos(th),100*sin(th),'k:');
axis equal; grid on; xlabel('east  [m]'); ylabel('north  [m]');
title('Ground track: o = 600 m start,  x = touchdown,  * = launch pad');
legend(leg,'location','southeast');
print(f,[OUT '/02_ground_track.png'],'-dpng','-r110'); close(f);
% --- figure 3: dispersion scatter ---
f=figure(); set(f,'position',[0 0 800 800]);
N=400; rand('seed',5); randn('seed',5); LX=zeros(N,1); LY=zeros(N,1);
for i=1:N
  w10=min(1.0+1.2*abs(randn()),7.0); wdir=2*pi*rand();
  aa=aero_cfg(); aa.CL0=CL0+0.02*randn(); aa.CD0_canopy=CDp+0.05+0.08*rand();
  rr=glide(g,aa,M+0.04*randn(),rho,g.alpha_trim_deg+randn(),d);
  off=wind_at(800,w10,0.17)*t1;
  [X,Y,H,T]=fly(rr,w10,wdir,-off*sin(wdir),-off*cos(wdir),600,0.25);
  LX(i)=X(end); LY(i)=Y(end);
end
plot(LX,LY,'b.','markersize',8); hold on; plot(0,0,'r*','markersize',18,'linewidth',2);
for R=[25 50 100 200]
  plot(R*cos(th),R*sin(th),'k--'); text(R*0.71,R*0.71,sprintf('%d m',R));
end
axis equal; grid on; xlabel('east [m]'); ylabel('north [m]');
mm=sort(hypot(LX,LY));
title(sprintf('Landing dispersion, %d runs:  CEP50 = %.0f m,  CEP90 = %.0f m',N,mm(round(.5*N)),mm(round(.9*N))));
xlim([-300 300]); ylim([-300 300]);
print(f,[OUT '/03_dispersion.png'],'-dpng','-r110'); close(f);
% --- figure 4: CFD polar ---
f=figure(); set(f,'position',[0 0 1000 420]);
al=[0 2 4 6 8.61 11 14]; CLv=[0.0334 0.2596 0.4702 0.6598 0.8634 1.1153 1.3453];
CDv=[0.0290 0.0312 0.0340 0.0388 0.0529 0.0635 0.0927];
subplot(1,2,1); plot(al,CLv,'bo-','linewidth',2,'markersize',7); grid on;
xlabel('angle of attack  [deg]'); ylabel('C_L'); title('2D RANS lift curve (Fluent, k-\omega SST)');
hold on; plot(8.61,0.8634,'rs','markersize',12,'linewidth',2);
subplot(1,2,2); plot(CDv,CLv,'bo-','linewidth',2,'markersize',7); grid on;
xlabel('C_D'); ylabel('C_L'); title('Drag polar');
hold on; plot(0.0529,0.8634,'rs','markersize',12,'linewidth',2);
print(f,[OUT '/04_cfd_polar.png'],'-dpng','-r110'); close(f);
printf('wrote 4 figures to %s\n',OUT);
