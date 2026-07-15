#!/usr/bin/env python3
# =============================================================================
#  plotGenCat.py  --  TopCPVGenCategorizer GenCatTree validation plots
#  -----------------------------------------------------------------------------
#  Updated behavior:
#    * Individual plot files are saved as PDF, not PNG.
#    * A combined multipage PDF is still written as gencat_validation.pdf.
#    * Overlay plots are produced by default.
#    * Stack plots are disabled by default and enabled only with --draw-stack.
#    * Channel_Idx and Channel_Idx_Final are available in both forms:
#        - named categorical x-axis: Channel_Idx, Channel_Idx_Final
#        - numeric code x-axis:      Channel_Idx_numeric, Channel_Idx_Final_numeric
#    * --only can restrict the run to one or more observables.
#
#  Typical full run:
#    python3 validation/plotGenCat.py \
#        /eos/user/<initial>/<username>/CPV/TTToSemiLeptonic_2017 \
#        /eos/user/<initial>/<username>/CPV/TTTo2L2Nu_2017 \
#        /eos/user/<initial>/<username>/CPV/TTToHadronic_2017 \
#        --label SemiLep --label 2L2Nu --label Hadronic \
#        --scale SemiLep=365 --scale 2L2Nu=88 --scale Hadronic=380 \
#        --lumi 41480 \
#        -o plots_full_overlay_pdf
# =============================================================================

import argparse
import glob
import os
import subprocess
import sys

import ROOT

ROOT.gROOT.SetBatch(True)
ROOT.gStyle.SetOptStat(0)
ROOT.gStyle.SetOptTitle(0)

try:
    ROOT.EnableImplicitMT()
except Exception:
    pass


PALETTE = [
    ROOT.kAzure + 1,
    ROOT.kRed + 1,
    ROOT.kGreen + 2,
    ROOT.kViolet + 1,
    ROOT.kOrange + 7,
    ROOT.kCyan + 2,
    ROOT.kGray + 2,
]


# Channel_Idx category map: (label, RDataFrame filter expression on a column)
CHANNEL_CATS = [
    ("all-had",    "{c}==0"),
    ("e+jets",     "{c}==11"),
    ("#mu+jets",   "{c}==13"),
    ("ee",         "{c}==22"),
    ("e#mu",       "{c}==24"),
    ("#mu#mu",     "{c}==26"),
    ("#tau-inv.",  "{c}<0"),
    ("other",      "{c}>0 && {c}!=11 && {c}!=13 && {c}!=22 && {c}!=24 && {c}!=26"),
]


# =============================================================================
#  Observable table
# =============================================================================
def build_observables():
    def H(**k):
        k.setdefault("kind", "hist")
        k.setdefault("sel", None)
        k.setdefault("logy", False)
        return k

    obs = []

    # -------------------------------------------------------------------------
    # Decay channel classification: named categorical versions
    # -------------------------------------------------------------------------
    obs.append(dict(
        kind="cat",
        key="Channel_Idx",
        title="ttbar decay channel (direct W#rightarrowl)",
        xlabel="channel",
        col="Channel_Idx",
        logy=False,
    ))

    obs.append(dict(
        kind="cat",
        key="Channel_Idx_Final",
        title="channel after #tau#rightarrowl",
        xlabel="channel",
        col="Channel_Idx_Final",
        logy=False,
    ))

    # -------------------------------------------------------------------------
    # Decay channel classification: numeric-code versions
    # These show the raw Channel_Idx value on the x-axis.
    # -------------------------------------------------------------------------
    obs.append(H(
        key="Channel_Idx_numeric",
        title="ttbar decay channel code (direct W#rightarrowl)",
        xlabel="Channel_Idx",
        col="Channel_Idx",
        nb=61,
        lo=-30.5,
        hi=30.5,
        logy=False,
    ))

    obs.append(H(
        key="Channel_Idx_Final_numeric",
        title="channel code after #tau#rightarrowl",
        xlabel="Channel_Idx_Final",
        col="Channel_Idx_Final",
        nb=61,
        lo=-30.5,
        hi=30.5,
        logy=False,
    ))

    obs.append(H(
        key="Channel_Lepton_Count",
        title="prompt lepton multiplicity",
        xlabel="N_{lep}",
        col="Channel_Lepton_Count",
        nb=5,
        lo=-0.5,
        hi=4.5,
    ))

    obs.append(H(
        key="Channel_Lepton_Count_Final",
        title="lepton mult. (incl. #tau#rightarrowl)",
        xlabel="N_{lep}",
        col="Channel_Lepton_Count_Final",
        nb=5,
        lo=-0.5,
        hi=4.5,
    ))

    obs.append(H(
        key="Channel_Jets_Abs",
        title="hadronic-W quark-pair code (normalised)",
        xlabel="Channel_Jets_Abs",
        col="Channel_Jets_Abs",
        sel="Channel_Jets_Abs>0",
        nb=60,
        lo=0,
        hi=6000,
        logy=True,
    ))

    obs.append(H(
        key="Channel_Tau_Lepton",
        title="dressed leptons with #tau ancestor",
        xlabel="N",
        col="Channel_Tau_Lepton",
        nb=4,
        lo=-0.5,
        hi=3.5,
    ))

    obs.append(H(
        key="Channel_Visible_Tau",
        title="hadronic #tau (GenVisTau)",
        xlabel="N",
        col="Channel_Visible_Tau",
        nb=4,
        lo=-0.5,
        hi=3.5,
    ))

    # -------------------------------------------------------------------------
    # Top / anti-top
    # -------------------------------------------------------------------------
    obs.append(H(
        key="GenTop_pt",
        title="generated top p_{T}",
        xlabel="p_{T}(t) [GeV]",
        col="GenTop_pt",
        sel="isSignal",
        nb=60,
        lo=0,
        hi=600,
    ))

    obs.append(H(
        key="GenAnTop_pt",
        title="generated #bar{t} p_{T}",
        xlabel="p_{T}(#bar{t}) [GeV]",
        col="GenAnTop_pt",
        sel="isSignal",
        nb=60,
        lo=0,
        hi=600,
    ))

    obs.append(H(
        key="GenTop_eta",
        title="generated top #eta",
        xlabel="#eta(t)",
        col="GenTop_eta",
        sel="isSignal",
        nb=50,
        lo=-5,
        hi=5,
    ))

    obs.append(H(
        key="GenAnTop_eta",
        title="generated #bar{t} #eta",
        xlabel="#eta(#bar{t})",
        col="GenAnTop_eta",
        sel="isSignal",
        nb=50,
        lo=-5,
        hi=5,
    ))

    # Remaining 4-momentum components. The C++ stores the full 4-vector
    # (pt, eta, phi, energy) for t and tbar; pt/eta above plus phi/energy/mass
    # here complete the kinematic picture. mass is computed from the stored
    # components and is the sharpest validation: it must peak at m_t = 172.5 GeV.
    #   |p| = pt*cosh(eta)  ->  m = sqrt(E^2 - pt^2*cosh^2(eta))
    for tag, sym in [("GenTop", "t"), ("GenAnTop", "#bar{t}")]:
        obs.append(H(
            key="%s_phi" % tag,
            title="generated %s #phi" % sym,
            xlabel="#phi(%s)" % sym,
            col="%s_phi" % tag,
            sel="isSignal",
            nb=64,
            lo=-3.2,
            hi=3.2,
        ))

        obs.append(H(
            key="%s_energy" % tag,
            title="generated %s energy" % sym,
            xlabel="E(%s) [GeV]" % sym,
            col="%s_energy" % tag,
            sel="isSignal",
            nb=60,
            lo=0,
            hi=1200,
            logy=True,
        ))

        obs.append(H(
            key="%s_mass" % tag,
            title="generated %s invariant mass" % sym,
            xlabel="m(%s) [GeV]" % sym,
            col=("%s_mass" % tag,
                 "sqrt(std::max(0.0, (double)%s_energy*%s_energy "
                 "- (double)%s_pt*%s_pt*cosh(%s_eta)*cosh(%s_eta)))"
                 % (tag, tag, tag, tag, tag, tag)),
            sel="isSignal",
            nb=60,
            lo=150,
            hi=195,
        ))

    # -------------------------------------------------------------------------
    # Particle-level jets / MET
    # -------------------------------------------------------------------------
    obs.append(H(
        key="GenJet_N",
        title="GenJet multiplicity",
        xlabel="N_{GenJet}",
        col=("GenJet_N", "(int)GenJet_pt.size()"),
        nb=20,
        lo=-0.5,
        hi=19.5,
    ))

    obs.append(H(
        key="GenJet_pt",
        title="GenJet p_{T} (all)",
        xlabel="p_{T} [GeV]",
        col="GenJet_pt",
        nb=60,
        lo=0,
        hi=600,
        logy=True,
    ))

    obs.append(H(
        key="GenJet_HadronFlavour",
        title="GenJet hadron flavour",
        xlabel="hadronFlavour",
        col="GenJet_HadronFlavour",
        nb=6,
        lo=-0.5,
        hi=5.5,
        logy=True,
    ))

    obs.append(H(
        key="GenMET_pt",
        title="generated MET",
        xlabel="p_{T}^{miss,gen} [GeV]",
        col="GenMET_pt",
        nb=60,
        lo=0,
        hi=300,
    ))

    # -------------------------------------------------------------------------
    # Ghost-B block
    # -------------------------------------------------------------------------
    obs.append(H(
        key="GenBJet_Count",
        title="b-flavour GenJet multiplicity",
        xlabel="N_{b-jet}",
        col="GenBJet_Count",
        nb=8,
        lo=-0.5,
        hi=7.5,
    ))

    obs.append(H(
        key="GenBJet_pt",
        title="b-flavour GenJet p_{T}",
        xlabel="p_{T} [GeV]",
        col="GenBJet_pt",
        nb=60,
        lo=0,
        hi=600,
        logy=True,
    ))

    obs.append(H(
        key="GenBHad_FromTopWeakDecay",
        title="b from top weak decay?",
        xlabel="FromTopWeakDecay",
        col="GenBHad_FromTopWeakDecay",
        nb=2,
        lo=-0.5,
        hi=1.5,
    ))

    obs.append(H(
        key="GenBHad_Flavour",
        title="matched b-quark signed pdgId",
        xlabel="pdgId",
        col=("GenBHad_Flavour_nz", "GenBHad_Flavour[GenBHad_Flavour!=0]"),
        nb=23,
        lo=-11.5,
        hi=11.5,
    ))

    # -------------------------------------------------------------------------
    # Selected gen indices
    # -------------------------------------------------------------------------
    for k, name in [
        (2, "t"),
        (3, "#bar{t}"),
        (4, "W+"),
        (8, "W+ daughter"),
        (10, "W- daughter"),
    ]:
        obs.append(H(
            key="SelectedIdx_%d" % k,
            title="gen index of slot %d (%s)" % (k, name),
            xlabel="SelectedIdx[%d]" % k,
            col=("sidx%d" % k, "SelectedIdx[%d]" % k),
            sel="isSignal",
            nb=60,
            lo=0,
            hi=120,
        ))

    obs.append(H(
        key="GenPar_Idx",
        title="all stored gen indices",
        xlabel="GenPar_Idx",
        col=("GenPar_Idx_pos", "GenPar_Idx[GenPar_Idx>=0]"),
        nb=60,
        lo=0,
        hi=120,
    ))

    obs.append(H(
        key="GenPar_pdgId",
        title="stored particle pdgId spectrum",
        xlabel="pdgId",
        col="GenPar_pdgId",
        nb=61,
        lo=-30.5,
        hi=30.5,
    ))

    return obs


# =============================================================================
#  Input discovery
# =============================================================================
def discover_files(spec, redirector, max_files):
    files = []

    if spec.startswith("root://"):
        parts = spec.split("//", 2)
        path = "/" + parts[2] if len(parts) >= 3 else spec

        try:
            out = subprocess.run(
                ["xrdfs", redirector, "ls", "-R", path],
                capture_output=True,
                text=True,
                check=True,
            ).stdout
        except Exception as e:
            sys.stderr.write("[WARN] xrdfs ls failed for %s: %s\n" % (spec, e))
            out = ""

        for line in out.splitlines():
            line = line.strip()
            if line.endswith(".root") and "_chunk" in os.path.basename(line):
                files.append("%s/%s" % (redirector, line))

    elif os.path.isdir(spec):
        files = sorted(glob.glob(
            os.path.join(spec, "**", "*_chunk*.root"),
            recursive=True,
        ))

        if not files:
            files = sorted(glob.glob(
                os.path.join(spec, "**", "*.root"),
                recursive=True,
            ))

    else:
        files = sorted(glob.glob(spec))

    if max_files > 0:
        files = files[:max_files]

    return files


# =============================================================================
#  Group object
# =============================================================================
class Group(object):
    def __init__(self, label, files, color):
        self.label = label
        self.files = files
        self.color = color

        self.chain = ROOT.TChain("GenCatTree")
        for f in files:
            self.chain.Add(f)

        self.hists = {}
        self.n_events = 0
        self.n_signal = 0


def _model(key, label, o):
    name = "h_%s_%s" % (key, label)
    return ROOT.RDF.TH1DModel(name, o["title"], o["nb"], o["lo"], o["hi"])


# =============================================================================
#  Fill histograms
# =============================================================================
def fill_group(g, observables, use_genweight):
    if not g.files:
        sys.stderr.write("[WARN] group '%s' has no files\n" % g.label)
        return

    has_gw = bool(g.chain.GetBranch("genWeight"))
    weight = "genWeight" if (use_genweight and has_gw) else ""

    rdf = ROOT.RDataFrame(g.chain)

    count_ptr = rdf.Count()
    sig_ptr = rdf.Filter("isSignal").Count()

    booked = {}

    for o in observables:
        if o["kind"] == "hist":
            node = rdf

            if o.get("sel"):
                node = node.Filter(o["sel"])

            col = o["col"]

            if isinstance(col, tuple):
                node = node.Define(col[0], col[1])
                colname = col[0]
            else:
                colname = col

            model = _model(o["key"], g.label, o)

            if weight:
                booked[o["key"]] = ("hist", node.Histo1D(model, colname, weight))
            else:
                booked[o["key"]] = ("hist", node.Histo1D(model, colname))

        else:
            ptrs = []

            for _label, cutfmt in CHANNEL_CATS:
                cut = cutfmt.format(c=o["col"])
                node = rdf.Filter(cut)

                if weight:
                    ptrs.append(node.Sum("genWeight"))
                else:
                    ptrs.append(node.Count())

            booked[o["key"]] = ("cat", ptrs)

    g.n_events = int(count_ptr.GetValue())
    g.n_signal = int(sig_ptr.GetValue())

    for o in observables:
        kind, payload = booked[o["key"]]

        if kind == "hist":
            h = payload.GetValue().Clone("hkeep_%s_%s" % (o["key"], g.label))
            h.SetDirectory(0)

        else:
            ncat = len(CHANNEL_CATS)
            h = ROOT.TH1D(
                "hkeep_%s_%s" % (o["key"], g.label),
                o["title"],
                ncat,
                -0.5,
                ncat - 0.5,
            )
            h.SetDirectory(0)

            for i, (lab, _c) in enumerate(CHANNEL_CATS):
                h.SetBinContent(i + 1, float(payload[i].GetValue()))
                h.GetXaxis().SetBinLabel(i + 1, lab)

        g.hists[o["key"]] = h


# =============================================================================
#  Drawing helpers
# =============================================================================
def cms_label(pad, lumi):
    pad.cd()

    lat = ROOT.TLatex()
    lat.SetNDC(True)

    lat.SetTextFont(61)
    lat.SetTextSize(0.045)
    lat.DrawLatex(0.13, 0.945, "CMS")

    lat.SetTextFont(52)
    lat.SetTextSize(0.036)
    lat.DrawLatex(0.21, 0.945, "Simulation")

    lat.SetTextFont(42)
    lat.SetTextSize(0.036)
    lat.DrawLatex(0.72, 0.945, lumi)

    pad.Update()
    return lat


def style_axis(h, o, density):
    h.GetXaxis().SetTitle(o["xlabel"])
    h.GetYaxis().SetTitle("a.u." if density else "events")

    h.GetXaxis().SetTitleSize(0.045)
    h.GetYaxis().SetTitleSize(0.045)


def save_canvas(c, outdir, basename):
    out = os.path.join(outdir, "%s.pdf" % basename)
    c.SaveAs(out)
    return out


def draw_overlay(o, groups, density, scales, outdir, lumi_label):
    drawn = []

    for g in groups:
        if o["key"] not in g.hists:
            continue

        h = g.hists[o["key"]]
        if h.Integral() <= 0:
            continue

        drawn.append((g, h))

    if not drawn:
        return None

    c = ROOT.TCanvas("c_ov_%s" % o["key"], "", 800, 600)
    c.SetLeftMargin(0.13)
    c.SetBottomMargin(0.13)
    c.SetTopMargin(0.10)

    if o["logy"]:
        c.SetLogy()

    ymax = 0.0
    plot_hists = []

    for g, h in drawn:
        hc = h.Clone(h.GetName() + "_ov")
        hc.SetDirectory(0)

        # Apply physical per-event scale to overlay if --scale/--lumi were given.
        # If --density is also given, the later area-normalisation cancels it.
        sc = scales.get(g.label, 1.0)
        if sc != 1.0:
            hc.Scale(sc)

        if density and hc.Integral() > 0:
            hc.Scale(1.0 / hc.Integral())

        hc.SetLineColor(g.color)
        hc.SetLineWidth(2)
        hc.SetFillStyle(0)
        hc.SetMarkerColor(g.color)

        ymax = max(ymax, hc.GetMaximum())
        plot_hists.append((g, hc))

    if ymax <= 0:
        return None

    leg = ROOT.TLegend(0.58, 0.68, 0.88, 0.88)
    leg.SetBorderSize(0)
    leg.SetFillStyle(0)

    first = True

    for g, hc in plot_hists:
        style_axis(hc, o, density)

        if first:
            hc.SetMaximum(ymax * (50 if o["logy"] else 1.4))
            if o["logy"]:
                hc.SetMinimum(0.5)
            hc.Draw("HIST")
            first = False
        else:
            hc.Draw("HIST SAME")

        if density:
            entry = "%s (raw=%d)" % (g.label, int(g.hists[o["key"]].Integral()))
        elif scales:
            entry = "%s (yield=%.1f)" % (g.label, hc.Integral())
        else:
            entry = "%s (%d)" % (g.label, int(g.hists[o["key"]].Integral()))

        leg.AddEntry(hc, entry, "l")

    leg.Draw()
    keep = cms_label(c, lumi_label)

    out = save_canvas(c, outdir, "%s_overlay" % o["key"])

    return (c, plot_hists, leg, keep, out)


def draw_stack(o, groups, scales, outdir, lumi_label):
    drawn = []

    for g in groups:
        if o["key"] not in g.hists:
            continue

        h = g.hists[o["key"]]
        if h.Integral() <= 0:
            continue

        drawn.append((g, h))

    if not drawn:
        return None

    c = ROOT.TCanvas("c_st_%s" % o["key"], "", 800, 600)
    c.SetLeftMargin(0.13)
    c.SetBottomMargin(0.13)
    c.SetTopMargin(0.10)

    if o["logy"]:
        c.SetLogy()

    hs = ROOT.THStack("hs_%s" % o["key"], "")
    leg = ROOT.TLegend(0.58, 0.68, 0.88, 0.88)
    leg.SetBorderSize(0)
    leg.SetFillStyle(0)

    keep_hists = []

    for g, h in drawn:
        hc = h.Clone(h.GetName() + "_st")
        hc.SetDirectory(0)

        sc = scales.get(g.label, 1.0)
        if sc != 1.0:
            hc.Scale(sc)

        hc.SetFillColor(g.color)
        hc.SetLineColor(ROOT.kBlack)
        hc.SetLineWidth(1)

        hs.Add(hc, "HIST")
        keep_hists.append(hc)
        leg.AddEntry(hc, g.label, "f")

    hs.Draw("HIST")

    hs.GetXaxis().SetTitle(o["xlabel"])
    hs.GetYaxis().SetTitle("events")

    if o["kind"] == "cat":
        for i, (lab, _c) in enumerate(CHANNEL_CATS):
            hs.GetXaxis().SetBinLabel(i + 1, lab)

    leg.Draw()
    keep = cms_label(c, lumi_label)

    c.Modified()
    c.Update()

    out = save_canvas(c, outdir, "%s_stack" % o["key"])

    return (c, hs, keep_hists, leg, keep, out)


# =============================================================================
#  Argument parsing utilities
# =============================================================================
def parse_scales(items):
    scales = {}

    for it in items:
        if "=" not in it:
            sys.stderr.write("[WARN] ignoring malformed --scale '%s'\n" % it)
            continue

        k, v = it.split("=", 1)

        try:
            scales[k] = float(v)
        except ValueError:
            sys.stderr.write("[WARN] bad scale value in '%s'\n" % it)

    return scales


def filter_observables(observables, only_items):
    if not only_items:
        return observables

    wanted = set()

    for item in only_items:
        for x in item.split(","):
            x = x.strip()
            if x:
                wanted.add(x)

    known = set(o["key"] for o in observables)
    missing = sorted(wanted - known)

    if missing:
        sys.stderr.write("[ERROR] unknown observable(s): %s\n" % ", ".join(missing))
        sys.stderr.write("[ERROR] valid observable keys are:\n")
        for o in observables:
            sys.stderr.write("  %s\n" % o["key"])
        sys.exit(2)

    return [o for o in observables if o["key"] in wanted]


# =============================================================================
#  main
# =============================================================================
def main():
    ap = argparse.ArgumentParser(
        description="GenCatTree validation plots. Default: PDF overlay only, no stack."
    )

    ap.add_argument(
        "inputs",
        nargs="+",
        help="per-group spec: local dir, glob, or xrootd dir URL",
    )

    ap.add_argument(
        "--label",
        action="append",
        default=[],
        help="legend label per input, repeat once per input",
    )

    ap.add_argument(
        "-o",
        "--out",
        default="plots",
        help="output directory",
    )

    ap.add_argument(
        "--density",
        action="store_true",
        help="area-normalise each overlay histogram to compare shapes",
    )

    ap.add_argument(
        "--scale",
        action="append",
        default=[],
        help="per-group scale, e.g. --scale SemiLep=365",
    )

    ap.add_argument(
        "--lumi",
        type=float,
        default=0.0,
        help="integrated luminosity in /pb. If given with --scale, "
             "scale is converted to sigma*lumi/Nevents.",
    )

    ap.add_argument(
        "--draw-stack",
        action="store_true",
        help="also draw stacked histograms. By default stack plots are disabled.",
    )

    ap.add_argument(
        "--only",
        action="append",
        default=[],
        help="draw only selected observable key(s). Repeatable or comma-separated. "
             "Example: --only Channel_Idx,Channel_Idx_numeric",
    )

    ap.add_argument(
        "--use-genweight",
        action="store_true",
        help="use genWeight branch as fill weight if present",
    )

    ap.add_argument(
        "--xrootd-redirector",
        default="root://eosuser.cern.ch",
    )

    ap.add_argument(
        "--max-files",
        type=int,
        default=-1,
        help="maximum number of ROOT files per input group",
    )

    ap.add_argument(
        "--lumi-label",
        default="UL2017 (13 TeV)",
    )

    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)

    scales = parse_scales(args.scale)

    groups = []

    for i, spec in enumerate(args.inputs):
        files = discover_files(spec, args.xrootd_redirector, args.max_files)

        if i < len(args.label):
            label = args.label[i]
        else:
            label = os.path.basename(os.path.normpath(spec)) or ("input%d" % i)

        print("[input] %-12s : %d file(s) from %s" % (label, len(files), spec))

        groups.append(Group(label, files, PALETTE[i % len(PALETTE)]))

    observables = build_observables()
    observables = filter_observables(observables, args.only)

    print("[plot ] %d observable(s) selected" % len(observables))

    for g in groups:
        print("[fill ] %s ..." % g.label)

        fill_group(g, observables, args.use_genweight)

        frac = (100.0 * g.n_signal / g.n_events) if g.n_events else 0.0

        print(
            "        events=%d  signal=%d (%.1f%%)"
            % (g.n_events, g.n_signal, frac)
        )

    # Convert sigma*BR scales into per-event lumi weights if --lumi is given.
    # This scale is applied to overlay and stack.
    if args.lumi > 0:
        for g in groups:
            if g.label in scales and g.n_events > 0:
                scales[g.label] = scales[g.label] * args.lumi / g.n_events

        if scales:
            print("[scale] applied lumi=%.0f /pb normalisation" % args.lumi)

    elif scales:
        print("[scale] using raw --scale factors without --lumi")

    else:
        print("[scale] no scale given: overlay uses raw event counts")

    if args.density:
        print("[draw ] overlay density mode: each group is area-normalised")
    elif scales:
        print("[draw ] overlay uses scaled event yields")
    else:
        print("[draw ] overlay uses raw event counts")

    if args.draw_stack:
        print("[draw ] stack plots are ENABLED")
    else:
        print("[draw ] stack plots are DISABLED")

    pdf = os.path.join(args.out, "gencat_validation.pdf")

    ROOT.gROOT.GetListOfCanvases().Delete()

    n_overlay = 0
    n_stack = 0
    keepalive = []

    c0 = ROOT.TCanvas("c0", "", 800, 600)
    c0.Print(pdf + "[")

    for o in observables:
        ov = draw_overlay(
            o,
            groups,
            args.density,
            scales,
            args.out,
            args.lumi_label,
        )

        if ov:
            ov[0].Print(pdf)
            keepalive.append(ov)
            n_overlay += 1

        if args.draw_stack:
            st = draw_stack(
                o,
                groups,
                scales,
                args.out,
                args.lumi_label,
            )

            if st:
                st[0].Print(pdf)
                keepalive.append(st)
                n_stack += 1

    c0.Print(pdf + "]")

    # Archive histograms
    froot = os.path.join(args.out, "gencat_hists.root")
    fout = ROOT.TFile.Open(froot, "RECREATE")

    for g in groups:
        d = fout.mkdir(g.label)
        d.cd()

        for key, h in g.hists.items():
            if h.Integral() > 0:
                h.Write(key)

    fout.Close()

    print("")
    print("[done] overlay plots : %d" % n_overlay)
    print("[done] stack plots   : %d" % n_stack)
    print("[done] output dir    : %s" % args.out)
    print("[done] combined pdf  : %s" % pdf)
    print("[done] histograms    : %s" % froot)


if __name__ == "__main__":
    main()

