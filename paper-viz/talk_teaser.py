#!/usr/bin/env python3
"""Render the 20-second talk teaser, review frames, captions, and local player.

    python paper-viz/talk_teaser.py --stills-only
    python paper-viz/talk_teaser.py
    python paper-viz/talk_teaser.py --voiceover /path/to/timed-read.wav

Perceived stimuli are illustrative. The spectral pair uses measured primaries;
test fingerprints and response matching follow nullspace_ordering_teaser.py.
"""
from __future__ import annotations

import argparse
import importlib.util
from functools import lru_cache
import io
import json
from pathlib import Path
import subprocess
import sys

import aggdraw
import cairosvg
import imageio_ffmpeg
import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUT = ROOT / "paper-viz/output/talk_teaser"
INK = "#202124"
MUTED = "#62666b"
LINE = "#d8dadd"
BLUE = "#2166ac"
GREEN = "#1b7837"
BG = (204, 200, 206)
PINK = (237, 178, 203)
FONTS = {
    "sans": "/usr/share/fonts/opentype/linux-libertine/LinBiolinum_R.otf",
    "bold": "/usr/share/fonts/opentype/linux-libertine/LinBiolinum_RB.otf",
    "serif": "/usr/share/fonts/opentype/linux-libertine/LinBiolinum_R.otf",
    "italic": "/usr/share/fonts/opentype/linux-libertine/LinBiolinum_RI.otf",
}


def ease(value):
    x = float(np.clip(value, 0, 1))
    return x * x * (3 - 2 * x)


def mix(a, b, u):
    return a + (b - a) * u


def rgb(color):
    if isinstance(color, str):
        return tuple(int(color[i:i + 2], 16) for i in (1, 3, 5))
    return tuple(color)


def pale(color, alpha):
    return tuple(round(255 + (c - 255) * alpha) for c in rgb(color))


@lru_cache(maxsize=80)
def font(size, face="sans"):
    return ImageFont.truetype(FONTS[face], size)


def text(im, xy, message, size=36, face="sans", fill=INK, anchor="la"):
    ImageDraw.Draw(im).text(xy, message, font=font(size, face), fill=fill, anchor=anchor)


def line(im, points, color=INK, width=3, dashed=False):
    ctx = aggdraw.Draw(im)
    path = aggdraw.Path()
    for j, (x, y) in enumerate(points):
        if j == 0 or (dashed and j % 12 < 4):
            path.moveto(x, y)
        else:
            path.lineto(x, y)
    ctx.path(path, None, aggdraw.Pen(color, width))
    ctx.flush()


def prepare_assets(out):
    """Cache scientific inputs with explicit provenance for standalone re-renders."""
    destination = out / "assets.json"
    if destination.exists():
        return json.loads(destination.read_text())
    sys.path.insert(0, str(ROOT / "extern/TetriumColor"))
    from TetriumColor.Plotting.PlotStyle import ALL_PEAKS, COLORS, WAVELENGTHS, build_observers
    observer, peaks = build_observers(WAVELENGTHS)["Hyperobserver"]
    curves = observer.get_sensor_matrix(WAVELENGTHS)
    base = ROOT / "extern/TetriumColor/measurements/2026-05-08/validation_2026-05-08_12-50-05/primaries"
    files = [
        ("R", "#bb2439", "r255g0b0o0_20260508_124435_341_zz_mean.csv"),
        ("G", "#26834b", "r0g255b0o0_20260508_124625_250_zz_mean.csv"),
        ("B", "#2d6cb0", "r0g0b255o0_20260508_124815_176_zz_mean.csv"),
        ("O", "#e78927", "r0g0b0o255_20260508_125005_086_zz_mean.csv"),
    ]
    spectra = []
    for label, color, filename in files:
        data = np.loadtxt(base / filename, delimiter=",", skiprows=1)
        keep = (data[:, 0] >= 400) & (data[:, 0] <= 700)
        data = data[keep]
        spectra.append(dict(label=label, color=color, wavelength=data[:, 0].tolist(),
                            power=(data[:, 1] / data[:, 1].max()).tolist(),
                            source=str((base / filename).relative_to(ROOT))))
    result = dict(wavelength=WAVELENGTHS.tolist(), curves=[
        dict(peak=p, color=COLORS[p], sensitivity=curves[ALL_PEAKS.index(p)].tolist()) for p in peaks
    ], spectra=spectra, cone_source="TetriumColor.Plotting.PlotStyle.build_observers / Hyperobserver",
        display_note="Measured spectra, each normalized to its own maximum.",
        response_note="Responses and candidate elimination are illustrative, not simulated or measured outcomes.")
    destination.write_text(json.dumps(result, indent=2) + "\n")
    return result


def prepare_math_assets(out, assets):
    """Reproduce the spectral null construction and the ordering figure's rules."""
    wl=np.asarray(assets["wavelength"])
    h=np.asarray([c["sensitivity"] for c in assets["curves"]])
    peaks=[c["peak"] for c in assets["curves"]]
    primary_data=[]
    for primary in assets["spectra"]:
        measured=np.loadtxt(ROOT/primary["source"],delimiter=",",skiprows=1)
        primary_data.append(np.interp(wl,measured[:,0],measured[:,1]))
    p=np.asarray(primary_data).T
    subset=[peaks.index(peak) for peak in [420,530,559]]
    _,_,vh=np.linalg.svd(h[subset]@p)
    direction=vh[-1]
    direction/=np.linalg.norm(direction)
    if (h[peaks.index(547)]@p)@direction<0:
        direction=-direction
    step=.8*np.min(.5/np.abs(direction[np.abs(direction)>1e-15]))
    weights_bg=.5-step*direction
    weights_blob=.5+step*direction
    spectrum_bg=p@weights_bg
    spectrum_blob=p@weights_blob
    difference=h@(spectrum_blob-spectrum_bg)
    assert np.max(np.abs(difference[subset])) < 1e-10*np.max(np.abs(difference))
    # Reuse the supplied ordering figure's exact fingerprint and response rules.
    sys.path.insert(0,str(ROOT/"extern/TetriumColor"))
    source=ROOT/"paper-viz/nullspace_ordering_teaser.py"
    spec=importlib.util.spec_from_file_location("ordering_figure",source)
    ordering=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(ordering)
    tests=[(420.,530.,555.),(420.,530.,559.),(420.,533.,559.)]
    observer=(420.,530.,559.)
    matrix=ordering._matrix_for_tests(tests)
    trials=[dict(peaks=list(test),fingerprint=matrix[i].tolist(),
                 null=ordering._is_failed(test,observer)) for i,test in enumerate(tests)]
    response=[int(not trial["null"]) for trial in trials]
    candidates=ordering._all_candidate_tests()
    compatible=[list(candidate) for candidate in candidates
                if [int(not ordering._is_failed(test,candidate)) for test in tests]==response]
    result=dict(wavelength=wl.tolist(),background=spectrum_bg.tolist(),blob=spectrum_blob.tolist(),
                weights_background=weights_bg.tolist(),weights_blob=weights_blob.tolist(),
                hyper_difference=difference.tolist(),nulled_peaks=[420,530,559],trials=trials,
                spectral_method="simple_observer_projection.py: raw display-to-cone SVD null direction, symmetric .8-gamut pair",
                primary_order=[s["label"] for s in assets["spectra"]],
                classification_source="paper-viz/nullspace_ordering_teaser.py",
                source_figure="paper-viz/output/nullspace_ordering_teaser/nullspace_ordering_teaser.png",
                response_vector=response,compatible_models=compatible,
                classification_rule="_is_failed: observer cone set is a subset of the test's nulled cone set",
                classification_method="_matrix_for_tests / _is_failed / _all_candidate_tests",
                note="Panel C is transposed: each animated row is a representative test. The o-x-o sequence is model-predicted, not participant data. Matching to the discrete idealized library does not verify a real observer's cone complement or genotype.")
    (out/"math_assets.json").write_text(json.dumps(result,indent=2)+"\n")
    assets["response_note"]="Perceptual views are schematic; the response vector and matching use the supplied ordering figure's model-predicted rules. See math_assets.json."
    (out/"assets.json").write_text(json.dumps(assets,indent=2)+"\n")
    return result


class Teaser:
    def __init__(self, config, assets, math_assets):
        self.config = config
        self.assets = assets
        self.curves = {c["peak"]: c for c in assets["curves"]}
        self.peaks = list(self.curves)
        self.a = config["observer_a"]
        self.b = config["observer_b"]
        self.math=math_assets
        # Direct reuse of the supplied artwork, without its old text label.
        source = Image.open(ROOT / "teaser-image.png").convert("RGB")
        self.projector = source.crop((994, 531, 1170, 641))
        self.profiles={}
        for which,number in [("A",1),("B",2)]:
            svg=ROOT/f"paper-viz/assets/talk_teaser/observer_{number}.svg"
            png=cairosvg.svg2png(url=str(svg),output_width=540,output_height=660)
            self.profiles[which]=Image.open(io.BytesIO(png)).convert("RGBA")

    def base(self):
        return Image.new("RGB", (1920, 1080), "white")

    def chart(self, im, box, peaks, alpha=1.0, dashed=False, axes=True, width=8, reveal=1):
        x, y, w, h = box
        if axes:
            line(im, [(x, y), (x, y+h), (x+w, y+h)], pale("#a3a6aa", alpha), 3)
        for peak in peaks:
            data = self.curves[peak]
            values = data["sensitivity"]
            pts = [(x + i / 300 * w, y + h * (1 - value * .93)) for i, value in enumerate(values)]
            # The source plot uses alpha .82 for the cone library.
            line(im, pts[:max(2, round(len(pts)*reveal))], pale(data["color"], .82 * alpha), width, dashed)

    def observer(self, im, which, y=425, alpha=1., small=False, x=None):
        x = (250 if which == "A" else 1510) if x is None else x
        scale=.88 if small else 1.
        profile=self.profiles[which].resize((round(180*scale),round(220*scale)),Image.Resampling.LANCZOS)
        if alpha<1:
            profile.putalpha(profile.getchannel("A").point(lambda a:round(a*alpha)))
        im.paste(profile,(x,round(y)),profile)
        text(im,(x+90*scale,y+220*scale+24),f"Observer {1 if which=='A' else 2}",
             34,"sans",pale(INK,alpha),"ma")

    def response(self, im, cx, cy, visible, alpha=1., size=76):
        """Behavioral o/x cells use the green/red response rows of panel C."""
        fill=pale("#1b7837",.58) if visible else pale("#c62828",.82)
        edge="#ffffff"
        ink="#1f1f1f" if visible else "#ffffff"
        d = ImageDraw.Draw(im)
        d.rectangle((cx-size/2,cy-size/2,cx+size/2,cy+size/2),
                    fill=pale(fill,alpha),outline=pale(edge,alpha),width=2)
        text(im,(cx,cy-3),"o" if visible else "x",round(size*.8),
             "sans" if visible else "bold",pale(ink,alpha),"mm")

    def stimulus(self, im, box, strength=1., variant=0, border=True, direction="right"):
        x,y,w,h = [round(v) for v in box]
        if min(w,h) <= 0: return
        patch = blob(w,h,round(strength,3),variant,direction)
        im.paste(patch,(x,y))
        if border:
            ImageDraw.Draw(im).rectangle((x,y,x+w,y+h),outline="#aaa5ae",width=2)
        # A small neutral center marker makes the cardinal offset legible.
        cx,cy=x+w/2,y+h/2
        line(im,[(cx-4,cy),(cx+4,cy)],"#817e84",1.5)
        line(im,[(cx,cy-4),(cx,cy+4)],"#817e84",1.5)

    def intro(self, t):
        im = self.base()
        self.observer(im,"A")
        self.observer(im,"B")
        split=ease((t-.8)/1.6)
        parent=(620,70,680,285)
        # The whole library persists above both observer-specific selections.
        self.chart(im,parent,self.peaks,.9*ease(t/.45),width=7)
        if split>0:
            for peaks,side in [(self.a,0),(self.b,1)]:
                end_x=470 if side==0 else 1100
                cx=end_x+175
                start_x=790 if side==0 else 1130
                branch=[(start_x,375),(start_x,400),(cx,450),(cx,485)]
                line(im,branch,pale("#858990",split),6)
                if split>.8:
                    line(im,[(cx-12,470),(cx,485),(cx+12,470)],pale("#858990",split),6)
                # Copy the actual selected curves from their parent coordinates.
                # Darken-compositing avoids repainting identical strokes darker.
                box=tuple(mix(a,b,split) for a,b in zip(parent,(end_x,510,350,255)))
                emerging=self.base()
                self.chart(emerging,box,peaks,.9,axes=False,width=mix(7,8,split))
                im=ImageChops.darker(im,emerging)
                if split>.85:
                    x,y,w,h=box
                    line(im,[(x,y),(x,y+h),(x+w,y+h)],pale("#a3a6aa",ease((split-.85)/.15)),3)
        return im

    def arrow(self,im,start,end,alpha=1.):
        x1,y1=start; x2,y2=end
        color=pale("#858990",alpha)
        line(im,[start,end],color,5)
        if abs(x2-x1)>abs(y2-y1):
            line(im,[(x2-13,y2-10),(x2,y2),(x2-13,y2+10)],color,5)
        else:
            line(im,[(x2-10,y2-13),(x2,y2),(x2+10,y2-13)],color,5)

    def spectra_pair(self,im,box,alpha=1.):
        x,y,w,h=box
        background=np.asarray(self.math["background"])
        foreground=np.asarray(self.math["blob"])
        scale=max(background.max(),foreground.max())
        line(im,[(x,y),(x,y+h),(x+w,y+h)],pale("#a3a6aa",alpha),3)
        for values,color in [(background,"#858188"),(foreground,"#c36b97")]:
            pts=[(x+i/300*w,y+h*(1-.94*value/scale)) for i,value in enumerate(values)]
            line(im,pts,pale(color,alpha),7)
        text(im,(x,y-47),"Background",34,fill=pale("#858188",alpha))
        text(im,(x+240,y-47),"Blob",34,fill=pale("#c36b97",alpha))
        text(im,(x,y+h+14),"400",26,fill=pale(MUTED,alpha))
        text(im,(x+w,y+h+14),"700 nm",26,fill=pale(MUTED,alpha),anchor="ra")

    def null_vector(self,im,x,y,peaks,cell=52,height=60,alpha=1.,labels=False):
        for i,peak in enumerate(self.peaks):
            nulled=peak in peaks
            ImageDraw.Draw(im).rectangle((x+i*cell,y,x+(i+1)*cell,y+height),
                fill=pale("#f9d6d5" if nulled else "#f7f7f7",alpha),
                outline=pale("#bfc1c4",alpha),width=3)
            if labels:
                label="S" if peak==420 else f"{'M' if peak<540 else 'L'}{peak:g}"
                text(im,(x+(i+.5)*cell,y-30),label,20,fill=pale(MUTED,alpha),anchor="ma")

    def primary_spectra(self,im):
        """Measured primary spectra, individually normalized, below the projector."""
        x,y,w,h=535,798,340,85
        line(im,[(x,y),(x,y+h),(x+w,y+h)],"#a3a6aa",3)
        for primary in self.assets["spectra"]:
            points=[(x+(wavelength-400)/300*w,y+h*(1-.94*power))
                    for wavelength,power in zip(primary["wavelength"],primary["power"])]
            line(im,points,primary["color"],5)
        text(im,(x,y+h+8),"400",20,fill=MUTED)
        text(im,(x+w,y+h+8),"700 nm",20,fill=MUTED,anchor="ra")

    def display(self,t):
        im = self.base()
        self.observer(im,"A",x=220,y=420)
        # Establish the hardware first, before presenting its output or response.
        hardware=self.base()
        projector=self.projector.resize((211,132),Image.Resampling.LANCZOS)
        hardware.paste(projector,(590,636))
        text(hardware,(695,564),"Four-primary display",34,anchor="ma")
        self.primary_spectra(hardware)
        im=ImageChops.darker(im,Image.blend(self.base(),hardware,ease(t/.3)))
        # Then show the physical pattern, its pair of spectra, and null vector.
        pattern=self.base()
        line(pattern,[(794,687),(985,568)],"#999da3",5)
        line(pattern,[(794,702),(985,878)],"#999da3",5)
        self.stimulus(pattern,(985,568,310,310),1-ease((t-2.05)/.55),direction="right")
        im=ImageChops.darker(im,Image.blend(self.base(),pattern,ease((t-.65)/.3)))
        self.spectra_pair(im,(575,180,460,255),ease((t-.9)/.4))
        a=ease((t-1.35)/.35)
        text(im,(1452,215),"Null-space stimulus",34,fill=pale(INK,a),anchor="ma")
        self.null_vector(im,1140,300,self.a,cell=52,height=60,alpha=a,labels=True)
        text(im,(1452,420),"Same cone response",34,
             fill=pale(INK,ease((t-2.05)/.55)),anchor="ma")
        # Only give the response after the illustrated blob has disappeared.
        response_alpha=ease((t-2.6)/.3)
        text(im,(1470,678),"Can’t see",42,fill=pale(INK,response_alpha),anchor="ma")
        self.response(im,310,335,False,response_alpha)
        return im

    def compare(self,t):
        im = self.base()
        self.observer(im,"A")
        self.observer(im,"B")
        # One physical image expands into two illustrated observer views.
        spread=ease((t-.65)/1.0)
        for side in [0,1]:
            x=mix(660,470 if side==0 else 1010,spread)
            y=mix(220,300,spread)
            size=mix(600,440,spread)
            strength=1-ease((t-2.65)/.8) if side==0 else 1
            self.stimulus(im,(x,y,size,size),strength)
        if t>3.45:
            self.response(im,340,344,False,ease((t-3.45)/.25))
            self.response(im,1600,344,True,ease((t-3.45)/.25))
        return im

    def classify(self,t):
        im=self.base()
        self.observer(im,"A",x=220,y=420)
        trials=self.math["trials"]
        collect=ease((t-2.95)/.65)
        classified=ease((t-3.85)/.45)
        for i,trial in enumerate(trials):
            y=170+i*185
            local=t-i*.95
            active=0<=local<.95
            responded=ease((local-.62)/.2)
            if active or (classified>0 and trial["null"]):
                color="#bdbdbf" if active else pale("#d98984",classified)
                ImageDraw.Draw(im).rounded_rectangle((490,y-15,1770,y+163),12,outline=color,width=4)
            strength=ease(local/.15)
            if trial["null"]:
                strength*=1-ease((local-.24)/.35)
            self.stimulus(im,(515,y,150,150),strength,direction=("up","right","left")[i])
            self.null_vector(im,760,y+45,trial["peaks"],cell=52,height=60,
                             alpha=1. if local>=0 else .4,labels=i==0)
            if responded>0:
                cx=mix(1490,850+i*80,collect)
                cy=mix(y+75,850,collect)
                self.response(im,cx,cy,not trial["null"],responded,size=62)
                text(im,(1570,y+57),"Can’t see" if trial["null"] else "See",36,
                     fill=pale(INK,responded))
        if collect>0:
            text(im,(750,850),"r =",42,fill=pale(INK,collect),anchor="rm")
            for x,d in [(795,1),(1065,-1)]:
                line(im,[(x+d*10,806),(x,806),(x,894),(x+d*10,894)],pale(INK,collect),4)
        if classified>0:
            self.arrow(im,(1095,850),(1175,850),classified)
            compatible=self.math["compatible_models"]
            assert len(compatible)==1, "This three-test illustrative sequence expects one idealized matching class."
            # Class shorthand omits the shared S cone; derive it from the match.
            label="("+", ".join(f"{peak:g}" for peak in compatible[0] if peak!=420)+")"
            text(im,(1450,777),"Observer class",36,
                 fill=pale(INK,classified),anchor="ma")
            text(im,(1450,848),label,60,"bold",pale(INK,classified),"mm")
        return im

    def frame(self,t,captions=False):
        scenes=self.config["scenes"]
        scene=next(s for s in scenes if s["start"]<=t<s["end"])
        im=getattr(self,scene["name"])(t-scene["start"])
        return self.presentation_frame(im,t,captions)

    def presentation_frame(self,im,t,captions=False):
        # The talk cut uses the caption-free space. A constant framing across
        # every shot keeps the carried observer stationary at the scene cut.
        scale=1.06 if captions else 1.12
        target_y=475 if captions else 540
        im=im.transform(im.size,Image.Transform.AFFINE,
            (1/scale,0,960-960/scale,0,1/scale,480-target_y/scale),
            resample=Image.Resampling.BICUBIC,fillcolor="white")
        # Retain consistent observer positions across direct, purposeful cuts.
        if captions:
            self.caption(im,t)
        return im

    def caption(self,im,t):
        cue=next(c for c in self.config["narration"] if c["start"]<=t<c["end"])
        rows=wrap(cue["text"],38,"sans",1570)
        y=993-(len(rows)-1)*24
        for j,row in enumerate(rows):
            text(im,(960,y+j*48),row,38,fill=INK,anchor="mm")


def wrap(message,size,face,width):
    rows=[""]
    f=font(size,face)
    for word in message.split():
        candidate=(rows[-1]+" "+word).strip()
        if f.getlength(candidate)>width:
            rows.append(word)
        else:
            rows[-1]=candidate
    return rows


@lru_cache(maxsize=12)
def gaussian(w,h,direction):
    yy,xx=np.mgrid[:h,:w]
    # Same half-radius offset as GaussianBlobGenerator; blob_size=2 matches
    # AppPseudoIsochromaticTest's current default. Diagram pixels are not degrees.
    radius=min(w,h)*.475
    dx,dy={"up":(0,-1),"right":(1,0),"down":(0,1),"left":(-1,0)}[direction]
    cx,cy=w*.5+dx*radius*.5,h*.5+dy*radius*.5
    sigma=radius*.2
    return np.exp(-.5*(((xx-cx)/sigma)**2+((yy-cy)/sigma)**2))[...,None]


def blob(w,h,strength,variant,direction="right"):
    # Original soft Gaussian profile and neutral lilac background.
    # Alternate hues denote different conditions, never different displays.
    foreground=[PINK,(184,210,191),(224,190,203)][variant%3]
    background=np.array(BG,dtype=np.float32)
    pixels=background+gaussian(w,h,direction)*strength*(np.array(foreground)-background)
    # Round instead of truncating tiny negative Gaussian tails into a darker field.
    return Image.fromarray(np.rint(np.clip(pixels,0,255)).astype(np.uint8))


def stamp(seconds,sep=","):
    ms=round(seconds*1000)
    return f"{ms//3600000:02d}:{ms//60000%60:02d}:{ms//1000%60:02d}{sep}{ms%1000:03d}"


def save_review(teaser,out):
    times=[.3,1.8,4.3,5.8,6.6,7.9,10.3,11.1,11.8,12.9,14.4,15.4,16.3,17.6,19.0]
    sheet=Image.new("RGB",(1920,5*390),"#ececea")
    for i,t in enumerate(times):
        im=teaser.frame(t,True)
        im.save(out/f"frame_{t:04.1f}s.png")
        thumb=im.resize((620,349),Image.Resampling.LANCZOS)
        x=10+(i%3)*640; y=(i//3)*390+30
        sheet.paste(thumb,(x,y))
        text(sheet,(x+8,y-25),f"{t:04.1f} s",20,"bold")
    sheet.save(out/"storyboard.jpg",quality=94)
    teaser.frame(4.3,False).save(out/"poster.jpg",quality=95)


def export_video(teaser,out,voiceover=None):
    cfg=teaser.config
    width,height=cfg["width"],cfg["height"]
    if (width,height)!=(1920,1080):
        raise ValueError("This composition currently requires 1920×1080 output.")
    writers=[]
    try:
        for name in ["talk_teaser.mp4","talk_teaser_clean.mp4"]:
            writer=imageio_ffmpeg.write_frames(str(out/name),(width,height),fps=cfg["fps"],
                codec="libx264",pix_fmt_in="rgb24",pix_fmt_out="yuv420p",quality=None,
                macro_block_size=1,ffmpeg_log_level="warning",
                output_params=["-movflags","+faststart","-preset","medium","-crf","18"]
                    + (["-af","apad","-t","20"] if voiceover else []),
                audio_path=str(voiceover) if voiceover else None,audio_codec="aac" if voiceover else None)
            writer.send(None)
            writers.append(writer)
        for i in range(round(cfg["duration"]*cfg["fps"])):
            t=i/cfg["fps"]
            scene=next(s for s in cfg["scenes"] if s["start"]<=t<s["end"])
            artwork=getattr(teaser,scene["name"])(t-scene["start"])
            clean=teaser.presentation_frame(artwork,t,False)
            captioned=teaser.presentation_frame(artwork,t,True)
            writers[0].send(np.asarray(captioned))
            writers[1].send(np.asarray(clean))
            if i%60==0:
                print(f"Rendered {t:.1f} / {cfg['duration']:.1f} seconds",flush=True)
    finally:
        for writer in writers:
            writer.close()


def save_notes(config,out,voiceover):
    cues=config["narration"]
    srt="\n\n".join(f"{i+1}\n{stamp(c['start'])} --> {stamp(c['end'])}\n{c['text']}" for i,c in enumerate(cues))+"\n"
    (out/"narration.srt").write_text(srt)
    (out/"narration.txt").write_text(" ".join(c["text"] for c in cues)+"\n")
    audio_note="A supplied timed voiceover is included." if voiceover else "Both cuts are silent. The main review cut has narration captions; the clean cut has no narration captions."
    (out/"README.md").write_text(f"""# Twenty-second talk teaser

1920 × 1080, {config['fps']} fps, 20 seconds, H.264 MP4.

- `talk_teaser.mp4`: review cut with narration captions throughout all 20 seconds.
- `talk_teaser_clean.mp4`: larger presentation framing without narration captions.
- `index.html`: local video review page with shot navigation.
- `storyboard.jpg`: fifteen-frame contact sheet, including the staged display reveal.
- `narration.srt` and `narration.txt`: approved script and provisional cue timing.
- `assets.json`: exact cone samples, measured spectra, source paths, and caveats.
- `math_assets.json`: computed spectral pair, representative test fingerprints,
  o-x-o response vector, compatible library models, and source-figure provenance.

{audio_note} Cue timing is provisional until rehearsed aloud.
The user's latest narration is preserved verbatim (57 words). The cut opens on
the same-image / different-perception comparison, then reveals the cone differences
at 5.3 s, the display at 9.8 s, and classification at 13.6 s. The ordering scene
remains for the closing invitation, 17.65–20 s.
No scene headlines, slide numbers, illustration note, or
typographic end card are shown. Mathematical notation labels the spectra and
response-vector workflow. Observer 1 and Observer 2 use identical-size faceless
SVG silhouettes and numbered labels. The opening starts with one stimulus, expands
to two, then removes the blob for Observer 1. Only Observer 1 remains for the display
and classification shots, at a fixed position. The taller hyperobserver fades in
as a whole; selected curves originate at its coordinates and travel out to the
two observers. Darken-compositing prevents coincident copies doubling stroke opacity.
Cone curves use 7–8 px strokes and the branching arrows use 6 px strokes before
framing. All artwork is enlarged 6% for review and 12% for the caption-free talk
cut, which is also vertically recentered to use the available screen height.
Talk details are unconfirmed and will be added verbally by the speaker when available.

The original hyperobserver model and colors are reused with the original plot's
0.82 curve opacity. Observer A selects 420/530/559; B selects 420/536/556. Values are
nominal opsin peaks, not labels of the displayed sensitivity maxima. Primary spectra
are measured May 8, 2026. Their full, unnormalized powers are used to compute the
gray background and pink blob spectra; both are drawn on one common vertical scale.
The raw-cone null direction and symmetric .8-gamut spectral pair follow the method
in `simple_observer_projection.py`. S420/M530/L559 are nulled to floating-point
precision. Difference bars are omitted; the two spectra are labeled Background
and Blob, with “Same cone response” and “Can't see.” A twelve-cell null-space strip
beside the spectra introduces the same representation used in the next scene.
Measured R/G/B/O primary spectra are plotted below the projector on a 400–700 nm
axis, each normalized to its own maximum. There are no color-block swatches.
The display scene first establishes the projector and its primary spectra, then
reveals the projected pattern, the background/blob spectra, and the null vector.
The blob fades only after those elements have appeared; “Can't see” follows its
disappearance. This hardware → stimulus → response sequence replaces the prior
simultaneous reveal.
These are modeled spectra from measured primaries, not measured stimulus spectra.
The original projector is
cropped from `teaser-image.png`; the profiles are editable SVG files in
`paper-viz/assets/talk_teaser/observer_1.svg` and `observer_2.svg`.
The Gaussian field matches the soft pink/lilac appearance of the supplied teaser.
Each blob occupies one of four cardinal positions at half-radius eccentricity,
following TetriumColor/TetraPlate.py GaussianBlobGenerator. The sigma matches its
blob_size=2 setting, the default in AppPseudoIsochromaticTest.h. A small center
cross is a diagram reference marker. The display and both comparison views use
the right-hand location. Classification trials use up/right/left. Trial locations
change discretely rather than sliding between them. Diagram pixels do not specify
a viewing angle. Pink/gray cells indicate nulled/non-zero cone responses; green o
and red x indicate the observer's visible/not-visible response, following panel C
of `nullspace_ordering_teaser.png`. These are not the forced-choice response
interface or its scoring. RGB channels are rounded, not truncated, so
Gaussian tails do not darken the otherwise identical background by one level.

The final scene adapts panel C of the supplied ordering figure, transposed so each
row is a test and each column is a cone. Three representative tests are ordered
(530,555), (530,559), (533,559). It directly calls `_matrix_for_tests`, `_is_failed`,
and `_all_candidate_tests` from `nullspace_ordering_teaser.py`, producing See / Can't
see / See for modeled Observer 1 (420,530,559). Responses animate from their rows
into r=[o,x,o]; the same response rule matches this fingerprint against the discrete
220-candidate library. Its compatible idealized model is shown simply as
“Observer class (530, 559),” omitting the shared S cone in this class shorthand.
The label is derived from the matching model, not a hard-coded alternative class.
There is no final fingerprint strip. Pink/gray test vectors remain, and the
invisible test row is highlighted. This replaces the prior measured
threshold example. It is a model-predicted demonstration, not participant data or
proof of a real person's exact cone complement or genotype. The comparison views
remain schematic depictions, not actual screen metamerism.

Re-render from the repository root:

```sh
python paper-viz/talk_teaser.py
python paper-viz/talk_teaser.py --voiceover /path/to/timed-read.wav
```

Dependencies: the project's TetriumColor environment plus Pillow, NumPy, aggdraw,
CairoSVG, and imageio-ffmpeg. All video text uses Linux Biolinum. After the initial
run the scientific arrays are cached in `assets.json`. Delete only that generated
cache if source models or measurements change and need to be re-exported.

Rehearse a timed read before finalizing pacing (approximately 171 words/minute).
The renderer
accepts voiceover up to 20 seconds without time-stretching, and pads shorter audio.
""")
    (out/"index.html").write_text("""<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>The Hyper-Observer · Talk teaser</title>
<style>
body{margin:0;background:#f3f2ef;color:#202124;font:18px "Linux Biolinum O",sans-serif}main{max-width:1200px;margin:32px auto;padding:0 24px}
header{display:flex;align-items:baseline;justify-content:space-between;gap:20px}h1{font-size:36px;font-weight:400;margin:0 0 20px}
video{display:block;width:100%;background:white;box-shadow:0 8px 35px #0001;border-radius:4px}nav{display:flex;flex-wrap:wrap;gap:8px;margin:20px 0}
button,a{font:inherit;color:inherit}button{background:white;border:1px solid #d4d4d4;border-radius:7px;padding:10px 14px;cursor:pointer}button:hover{border-color:#202124}
p{line-height:1.5;color:#62666b}.downloads{display:flex;gap:24px;flex-wrap:wrap}small{color:#62666b}
</style><main><header><h1>The Hyper-Observer</h1><small>20 seconds · 1080p · 30 fps</small></header>
<video id="video" controls playsinline preload="metadata" poster="poster.jpg"><source src="talk_teaser.mp4" type="video/mp4"></video>
<nav aria-label="Shot navigation"><button data-time="0">00:00 · Two observers</button><button data-time="5.3">00:05.3 · Cone differences</button><button data-time="9.8">00:09.8 · Spectra and null vector</button><button data-time="13.6">00:13.6 · Test ordering</button><button data-time="17.65">00:17.65 · Collected response vector</button></nav>
<p>Captioned review cut. Blobs occupy cardinal positions in the 4AFC task; both observers view the same location. No scene headings or slide numbers. Talk details are omitted.</p>
<div class="downloads"><a href="talk_teaser.mp4" download>Captioned review MP4</a><a href="talk_teaser_clean.mp4" download>Clean MP4</a><a href="storyboard.jpg">Storyboard</a><a href="narration.srt" download>Timed narration cues (SRT)</a></div>
<script>const v=document.getElementById('video');document.querySelectorAll('[data-time]').forEach(b=>b.onclick=()=>{v.currentTime=Number(b.dataset.time);v.play()});</script></main></html>
""")


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--config",type=Path,default=ROOT/"paper-viz/talk_teaser.json")
    p.add_argument("--output-dir",type=Path,default=DEFAULT_OUT)
    p.add_argument("--stills-only",action="store_true")
    p.add_argument("--voiceover",type=Path)
    args=p.parse_args()
    config=json.loads(args.config.read_text())
    if config["duration"]!=20:
        raise ValueError("Scene times are authored for a 20-second cut.")
    if args.voiceover:
        # Refuse to silently truncate a long read. Its pacing needs an explicit edit.
        probe=subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(),"-i",str(args.voiceover)],capture_output=True,text=True)
        import re
        duration=re.search(r"Duration: (\d+):(\d+):(\d+\.\d+)",probe.stderr)
        if not duration:
            raise ValueError("Could not read voiceover duration.")
        h,m,s=map(float,duration.groups())
        if h*3600+m*60+s>20:
            raise ValueError("Voiceover exceeds 20 seconds; record a timed read before locking the edit.")
    args.output_dir.mkdir(parents=True,exist_ok=True)
    assets=prepare_assets(args.output_dir)
    teaser=Teaser(config,assets,prepare_math_assets(args.output_dir,assets))
    save_review(teaser,args.output_dir)
    save_notes(config,args.output_dir,args.voiceover)
    if not args.stills_only:
        export_video(teaser,args.output_dir,args.voiceover)
    print(f"Saved teaser assets to {args.output_dir}",flush=True)


if __name__=="__main__":
    main()
