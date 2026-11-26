"""Bake original short mono PCM field effects; no recordings or external libraries."""
import array
import math
from pathlib import Path
import random
import sys
import wave

RATE = 44100
DEST = Path(__file__).resolve().parents[2] / "project/Resources/farm/field/audio"


class Take:
    def __init__(self, seed):
        self.rng = random.Random(seed)
        self.samples = [0.] * RATE

    def grain(self, start, length, amp, freq, end=None, noise=False, attack=.003):
        idx = round(start * RATE)
        n = round(length * RATE)
        phase = low = base = 0.
        for j in range(n):
            u = j / n
            hz = freq + ((end if end is not None else freq) - freq) * u
            if noise:
                sample = self.rng.uniform(-1, 1)
                low += (1-math.exp(-math.tau*hz/RATE))*(sample-low)
                base += (1-math.exp(-math.tau*hz*.15/RATE))*(low-base)
                sample = low-base
            else:
                phase += math.tau * hz / RATE
                sample = math.sin(phase)
            env = min(1., j/(RATE*attack)) * (1-u)**2 * math.exp(-2.5*u)
            self.samples[idx+j] += amp*sample*env

    def grit(self, start, span, n, pitch, strength):
        for _ in range(n):
            self.grain(start+self.rng.uniform(0,span), self.rng.uniform(.011,.044),
                       strength*self.rng.uniform(.4,1),pitch*self.rng.uniform(.65,1.5),noise=True)

    def drops(self, start, span, n, pitch, strength):
        for i in range(n):
            t = start + span*(i+self.rng.random()*.5)/n
            f = pitch*self.rng.uniform(.65,1.45)
            self.grain(t,.042,strength*.65,f,f*1.8,attack=.0018)
            self.grain(t+.002,.058,strength,3300,1350,True)

    def wood(self, weight):
        for f,amp in [(167,.7),(380,.38),(576,.16),(842,.12)]:
            self.grain(.016,.15*weight,amp,f/weight,f*.95/weight)
        self.grit(.02,.09,9,1800,.4)
        self.grain(.12,.13,.22,250/weight,190/weight)

    def write(self, name):
        last = max(i for i,v in enumerate(self.samples) if abs(v)>1e-6)
        peak = max(map(abs,self.samples))
        pcm = array.array("h",(round(v/peak*24800) for v in self.samples[:last+1]))
        if sys.byteorder != "little":
            pcm.byteswap()
        with wave.open(str(DEST / (name+".wav")), "wb") as out:
            out.setparams((1,2,RATE,0,"NONE","not compressed"))
            out.writeframes(pcm.tobytes())


def bake():
    DEST.mkdir(parents=True,exist_ok=True)
    names = "hoe seed cover pour pull pick cut basket heavy_basket bend seep ripple brush_soil brush_leaf equip".split()
    for i,name in enumerate(names):
        a = Take(6100+i)
        if name=="hoe":
            a.grain(.004,.16,.95,132,62); a.grain(.007,.18,1.5,2400,390,True)
            a.grit(.03,.18,18,2600,.50)
        elif name=="seed":
            for t,f in [(0,1130),(.047,950),(.108,1320),(.175,1010)]:
                a.grain(t,.035,.65,f,f*.87); a.grit(t,.009,3,3300,.5)
        elif name in ("cover","brush_soil","brush_leaf","equip"):
            f,span = {"cover":(1100,.25),"brush_soil":(1600,.13),
                      "brush_leaf":(5000,.20),"equip":(2450,.10)}[name]
            a.grain(.005,span,.9,f,f*.58,True,.022)
            a.grit(.018,span*.84,18,f*1.35,.35)
            if name=="equip": a.grain(.078,.052,.18,670,530)
        elif name in ("pour","bend","seep","ripple"):
            n,span,f = {"pour":(29,.76,1240),"bend":(15,.38,660),
                        "seep":(10,.27,840),"ripple":(2,.067,1050)}[name]
            a.drops(.008,span,n,f,.5)
            if n>2: a.grain(.012,span+.09,1.,3100 if name=="pour" else 1380,650,True,.038)
        elif name=="pull":
            a.grit(.005,.057,13,1950,.47); a.grain(.05,.13,.8,240,86)
            a.grain(.07,.095,.42,1200,470,True)
        elif name in ("pick","cut"):
            a.grain(.01,.044,.7,780 if name=="pick" else 390,220)
            a.grain(.012,.065,1.2,4800,1600,True)
            a.grit(.02,.11,9,4700,.30)
        else:
            a.wood(1.55 if name=="heavy_basket" else 1.)
        a.write(name)


if __name__=="__main__":
    bake()
