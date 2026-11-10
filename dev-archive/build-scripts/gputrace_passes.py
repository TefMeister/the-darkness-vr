"""passes.py -- add up an Nsight GPU trace's exported GPUTRACE_REGIMES.xls by kind of pass (our "DK ..." markers).
  python passes.py <BASE_UNLOCKED folder> [--seq]   (--seq: the passes in frame order instead)"""
import csv,re,collections,sys,os
csv.field_size_limit(10**9)
rows=list(csv.reader(open(os.path.join(sys.argv[1],'GPUTRACE_REGIMES.xls'),encoding='utf-8',errors='replace'),delimiter='\t'))
h=rows[0]; col=h.index; f=lambda r,i: float(r[i] or 0)
c3=col('fe__cycles_active_queue_sync_subchannel_3d.sum'); cc=col('gr__compute_cycles_active_queue_sync.sum'); ce=col('gpu__engine_cycles_active_any_syncce.sum')
px=col('prop__input_pixels_type_3d_realtime.sum'); dr=col('fe__draw_count.sum')
ps=col('Top_Level_Triage.tpc__threads_launched_shader_ps_killmask_on_realtime.sum')
topr=[r for r in rows[1:] if '/DK ' not in r[0]]
T=lambda r: f(r,c3)+f(r,ce)   # compute cycles run inside the 3D subchannel time
tot=sum(T(r) for r in topr)
print('top-level ranges',len(topr),' 3D Mcycles %.1f  copy Mcycles %.1f  compute Mcycles %.1f'%(sum(f(r,c3) for r in topr)/1e6,sum(f(r,ce) for r in topr)/1e6,sum(f(r,cc) for r in topr)/1e6))
if '--seq' in sys.argv:
    for r in topr:
        if T(r)/tot>0.004: print('%5.1f%% %7.1fMpx %5d dr  %s'%(100*T(r)/tot,f(r,px)/1e6,f(r,dr),r[0][:150]))
    sys.exit()
agg=collections.OrderedDict()
for r in rows[1:]:
    n=r[0]
    if '/DK ' in n: k='  (inside a pass) '+re.sub(r'\d+ ranges \d+ KB','',re.sub(r'\| src.*','',n.split('/DK ')[-1]))[:60]
    else: k=re.sub(r' at 0x[0-9A-F]+ len \d+','',re.sub(r'^DK pass \d+ \| ','',n))
    a=agg.setdefault(k,[0,0,0,0,0,0,0]); a[0]+=1; a[1]+=T(r); a[2]+=f(r,px); a[3]+=f(r,dr); a[4]+=f(r,ps); a[5]+=f(r,cc); a[6]+=f(r,ce)
print('%5s %6s %6s %6s %8s %6s %8s  name'%('n','%time','%comp','%copy','Mpix','draws','MpsThr'))
for k,a in sorted(agg.items(), key=lambda x:-x[1][1])[:40]:
    print('%5d %6.1f %6.1f %6.1f %8.1f %6d %8.1f  %s'%(a[0],100*a[1]/tot,100*a[5]/tot,100*a[6]/tot,a[2]/1e6,a[3],a[4]/1e6,k[:140]))
