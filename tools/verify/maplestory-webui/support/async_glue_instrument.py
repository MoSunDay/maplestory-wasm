#!/usr/bin/env python3
"""Inject Asyncify sleep-state instrumentation into an Emscripten glue file.

The wasm client occasionally wedges under Emscripten Asyncify (single rewind
buffer, concurrent suspension points). This injects a read-only observer
into the glue: it wraps abort/callUserCallback/handleSleep, records every
state transition into window.__asyncLog (capped at 500 entries) and prints
an [ASYNC-STUCK] line every 4s while Asyncify.state stays non-zero, so the
exact park/wake/abort sequence is visible in browser-console.log even when
the renderer is frozen and CDP evaluate cannot reach it.

Usage:
    python3 async_glue_instrument.py <JourneyClient.js>

Idempotent: skips files that already contain __asyncLog. Keep a pristine
copy before injecting; see docs/known-issues/2026-10-31-asyncify-wedge*.md.
"""
import sys

ANCHOR = 'var wasmExports;createWasm();run();'

INSTRUMENT = r''';(function(){
  if (typeof Asyncify === 'undefined') { console.log('[INST] no Asyncify'); return; }
  function caller(){ try { return (new Error().stack||'').split('\n').slice(2,6).join(' <- '); } catch(e){ return 'n/a'; } }
  window.__asyncLog=[];
  function log(tag,extra){ var e={t:Date.now(),tag:tag,st:Asyncify.state,cur:!!Asyncify.currData}; if(extra){for(var k in extra)e[k]=extra[k];} window.__asyncLog.push(e); if(window.__asyncLog.length>500)window.__asyncLog.shift(); }
  if(typeof abort==='function'&&!abort.__inst){ var __oa=abort; var __wa=function(w){ log('ABORT',{why:String(w),at:caller()}); return __oa(w); }; __wa.__inst=1; try{abort=__wa;}catch(e){} }
  if(typeof callUserCallback==='function'&&!callUserCallback.__inst){ var __oc=callUserCallback; var __wc=function(f){ if(Asyncify.state!==0)log('CUC',{at:caller()}); return __oc(f); }; __wc.__inst=1; try{callUserCallback=__wc;}catch(e){} }
  var __oh=Asyncify.handleSleep;
  if(__oh.__inst){return;}
  var __wh=function(startAsync){
    var es=Asyncify.state, cs=caller();
    if(es!==0&&es!==2)log('REENTER',{es:es,at:cs});
    var wfn=function(wakeUp){
      var wc=0;
      return startAsync(function(v){ wc++; log('WAKE'+wc,{v:v,es:es,st:Asyncify.state,at:cs}); return wakeUp(v); });
    };
    var r; try{ r=__oh.call(Asyncify,wfn); }catch(e){ log('HS-THROW',{err:String(e),at:cs}); throw e; }
    if(Asyncify.state===1)log('PARK',{at:cs});
    return r;
  };
  __wh.__inst=1; Asyncify.handleSleep=__wh;
  var lastNormal=Date.now();
  setInterval(function(){
    if(Asyncify.state===0){lastNormal=Date.now();return;}
    if(Date.now()-lastNormal>4000){
      log('STUCK');
      var tail=window.__asyncLog.slice(-16);
      console.log('[ASYNC-STUCK] state='+Asyncify.state+' curr='+!!Asyncify.currData+' :: '+JSON.stringify(tail));
    }
  },2000);
  console.log('[INST] asyncify instrumentation on');
})();
'''


def inject(glue_path):
    with open(glue_path) as fh:
        src = fh.read()
    if '__asyncLog' in src:
        return 'already instrumented: ' + glue_path
    if src.count(ANCHOR) != 1:
        raise SystemExit('anchor not found exactly once in ' + glue_path)
    with open(glue_path, 'w') as fh:
        fh.write(src.replace(ANCHOR, INSTRUMENT + ANCHOR))
    return 'instrumented: ' + glue_path


def main(argv):
    if len(argv) != 2:
        raise SystemExit(__doc__)
    print(inject(argv[1]))


if __name__ == '__main__':
    main(sys.argv)
