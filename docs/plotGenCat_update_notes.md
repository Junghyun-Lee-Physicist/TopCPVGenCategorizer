# Update notes for `validation/plotGenCat.py`

This document summarizes the code updates made relative to the previous version of `plotGenCat.py`.

## 1. Motivation

The previous script produced both overlay and stack plots for every observable. The overlay plot was useful for comparing SemiLep, 2L2Nu, and Hadronic samples separately, but the stack plot used `ROOT.THStack`, so the three samples were visually accumulated into one total composition. That behavior was not ideal for the current validation goal, where the main purpose is to check whether each dedicated ttbar sample is classified into the expected generator-level decay category.

The updated version therefore changes the default plotting behavior to overlay-only and disables stacked histograms unless explicitly requested.

The previous script also saved individual plots as PNG files while writing a combined multipage PDF. The updated version saves all individual plot files as PDF as well.

## 2. Summary of functional changes

### 2.1 Individual plot format changed from PNG to PDF

Previous behavior:

```text
<observable>_overlay.png
<observable>_stack.png
```

Updated behavior:

```text
<observable>_overlay.pdf
<observable>_stack.pdf    # only if --draw-stack is given
```

The combined multipage PDF remains:

```text
gencat_validation.pdf
```

This was implemented by replacing direct `c.SaveAs(...png)` usage with a `save_canvas()` helper:

```python

def save_canvas(c, outdir, basename):
    out = os.path.join(outdir, "%s.pdf" % basename)
    c.SaveAs(out)
    return out
```

Both `draw_overlay()` and `draw_stack()` now call this helper.

### 2.2 Stack plots disabled by default

Previous behavior:

```python
for o in observables:
    ov = draw_overlay(...)
    st = draw_stack(...)
```

So every observable produced both an overlay and a stack plot.

Updated behavior:

```python
for o in observables:
    ov = draw_overlay(...)

    if args.draw_stack:
        st = draw_stack(...)
```

A new command-line option was added:

```python
ap.add_argument(
    "--draw-stack",
    action="store_true",
    help="also draw stacked histograms. By default stack plots are disabled.",
)
```

Now the default output avoids merging SemiLep, 2L2Nu, and Hadronic into a stacked composition. Stack plots are still available for composition checks by passing `--draw-stack`.

### 2.3 Added `--only` to restrict observables

A new option was added:

```python
ap.add_argument(
    "--only",
    action="append",
    default=[],
    help="draw only selected observable key(s). Repeatable or comma-separated. "
         "Example: --only Channel_Idx,Channel_Idx_numeric",
)
```

The helper function `filter_observables()` validates the requested keys and filters the observable list. This is useful for quick checks such as:

```bash
--only Channel_Idx,Channel_Idx_numeric
```

If an invalid observable is requested, the script prints all valid keys and exits.

### 2.4 Added numeric x-axis versions of Channel_Idx

The previous `Channel_Idx` and `Channel_Idx_Final` observables were categorical plots. They used the label mapping:

```text
all-had, e+jets, mu+jets, ee, emu, mumu, tau-inv., other
```

The updated script keeps those named versions and adds raw numeric-code versions:

```text
Channel_Idx_numeric
Channel_Idx_Final_numeric
```

Implementation:

```python
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
```

This makes it possible to check both human-readable category names and the actual integer branch values.

### 2.5 Overlay scaling behavior clarified and updated

In the earlier script, `--scale` and `--lumi` were mainly meaningful for stack plots. Since stack plots are now disabled by default, the updated script applies the same per-event luminosity normalization to overlay histograms as well.

The conversion remains:

```text
per-event weight = sigma_or_sigmaBR * lumi / number_of_events
```

This happens after all groups are filled:

```python
if args.lumi > 0:
    for g in groups:
        if g.label in scales and g.n_events > 0:
            scales[g.label] = scales[g.label] * args.lumi / g.n_events
```

Then `draw_overlay()` applies the scale to each cloned histogram:

```python
sc = scales.get(g.label, 1.0)
if sc != 1.0:
    hc.Scale(sc)
```

If `--density` is also given, the histogram is area-normalized after scaling. Therefore, in density mode, only shapes are compared.

### 2.6 Output printout updated

The final summary now distinguishes overlay and stack counts:

```text
[done] overlay plots : N
[done] stack plots   : M
[done] output dir    : ...
[done] combined pdf  : .../gencat_validation.pdf
[done] histograms    : .../gencat_hists.root
```

This makes it clear whether stack plots were intentionally produced.

## 3. Important unchanged behavior

The script still uses:

- `ROOT.RDataFrame` for histogram filling.
- `TChain("GenCatTree")` as the input tree.
- Recursive file discovery under each input directory.
- `*_chunk*.root` preference, falling back to all `*.root` files.
- A combined multipage PDF named `gencat_validation.pdf`.
- A ROOT histogram archive named `gencat_hists.root`.
- `--use-genweight` if a `genWeight` branch exists.

## 4. Recommended commands after the update

### Full physics-normalized overlay run

```bash
python3 validation/plotGenCat.py \
    /eos/user/j/junghyun/CPV/TTToSemiLeptonic_2017 \
    /eos/user/j/junghyun/CPV/TTTo2L2Nu_2017 \
    /eos/user/j/junghyun/CPV/TTToHadronic_2017 \
    --label SemiLep --label 2L2Nu --label Hadronic \
    --scale SemiLep=365 \
    --scale 2L2Nu=88 \
    --scale Hadronic=380 \
    --lumi 41480 \
    -o plots_full_overlay_pdf
```

### Named and numeric Channel_Idx check

```bash
python3 validation/plotGenCat.py \
    /eos/user/j/junghyun/CPV/TTToSemiLeptonic_2017 \
    /eos/user/j/junghyun/CPV/TTTo2L2Nu_2017 \
    /eos/user/j/junghyun/CPV/TTToHadronic_2017 \
    --label SemiLep --label 2L2Nu --label Hadronic \
    --scale SemiLep=365 \
    --scale 2L2Nu=88 \
    --scale Hadronic=380 \
    --lumi 41480 \
    --only Channel_Idx,Channel_Idx_numeric \
    -o plots_channel_idx_named_and_numeric
```

### Shape-only channel sanity check

```bash
python3 validation/plotGenCat.py \
    /eos/user/j/junghyun/CPV/TTToSemiLeptonic_2017 \
    /eos/user/j/junghyun/CPV/TTTo2L2Nu_2017 \
    /eos/user/j/junghyun/CPV/TTToHadronic_2017 \
    --label SemiLep --label 2L2Nu --label Hadronic \
    --only Channel_Idx,Channel_Idx_numeric,Channel_Idx_Final,Channel_Idx_Final_numeric \
    --density \
    -o plots_channel_shape_check
```

### Overlay plus stack, if needed

```bash
python3 validation/plotGenCat.py \
    /eos/user/j/junghyun/CPV/TTToSemiLeptonic_2017 \
    /eos/user/j/junghyun/CPV/TTTo2L2Nu_2017 \
    /eos/user/j/junghyun/CPV/TTToHadronic_2017 \
    --label SemiLep --label 2L2Nu --label Hadronic \
    --scale SemiLep=365 \
    --scale 2L2Nu=88 \
    --scale Hadronic=380 \
    --lumi 41480 \
    --draw-stack \
    -o plots_full_overlay_plus_stack_pdf
```


## 5. Interpretation note: `Channel_Idx` vs `Channel_Idx_Final`

During the validation, `Channel_Idx_overlay.pdf` and `Channel_Idx_Final_overlay.pdf` can look different, especially in the `tau-inv.` category. This is expected and comes from the different definitions of the two branches, not from the plotting update itself.

### 5.1 `Channel_Idx`: direct W-decay category

`Channel_Idx` should be interpreted as the ttbar decay channel before resolving tau decays into their final visible electron or muon daughters.

Conceptually:

```text
Channel_Idx
= category based on the direct W decay products
= W -> e / mu are classified as electron or muon channels
= W -> tau is still treated as a tau-side category
```

Therefore, events involving direct `W -> tau nu` can enter the negative-code category, which is displayed in the named plot as:

```text
tau-inv.
```

In the current named categorical plot, this bin is defined by:

```python
("#tau-inv.", "{c}<0")
```

So the label should be understood as the negative-code tau/unresolved category, not as a statement that the tau event disappeared or is physically invisible in all cases.

### 5.2 `Channel_Idx_Final`: category after resolving tau decays

`Channel_Idx_Final` should be interpreted as the category after following tau decays into final visible electron or muon states.

Conceptually:

```text
Channel_Idx_Final
= category after tau -> e / mu decays are propagated
= W -> tau -> e is reclassified as an electron channel
= W -> tau -> mu is reclassified as a muon channel
= W -> tau_had is classified according to the remaining visible e/mu multiplicity
```

Examples:

```text
ttbar -> W_had + W_lep
W_lep -> tau nu
tau -> e nu nu

Channel_Idx       : tau-side / negative-code category
Channel_Idx_Final : e+jets
```

```text
ttbar -> W_had + W_lep
W_lep -> tau nu
tau -> hadrons + nu

Channel_Idx       : tau-side / negative-code category
Channel_Idx_Final : all-had-like, because no final e/mu lepton remains
```

For dileptonic ttbar events, the same logic applies. If one or both W bosons decay through taus, the direct category may be tau-side in `Channel_Idx`, while the final category is redistributed into `ee`, `emu`, `mumu`, `e+jets`, `mu+jets`, or `all-had`, depending on how the taus decay.

### 5.3 Why the tau bin is reduced or absent in `Channel_Idx_Final`

The apparent disappearance of the `tau-inv.` bin in `Channel_Idx_Final` does not mean that tau events were removed. It means that tau-origin events were migrated into the final-state visible categories after tau decays were resolved.

The expected migration pattern is:

```text
Channel_Idx tau-side events
    -> tau -> e      -> e-containing category
    -> tau -> mu     -> mu-containing category
    -> tau -> had    -> category with fewer final e/mu leptons
```

Therefore, it is normal for:

```text
Channel_Idx       : visible tau-side / negative-code content
Channel_Idx_Final : reduced tau-side content, with events redistributed elsewhere
```

This is especially visible for SemiLep and 2L2Nu samples, because direct `W -> tau` events can become electron/muon categories after tau decay, or can become hadronic-like categories when the tau decays hadronically.

### 5.4 Recommended plot captions

For named category plots, the captions should make the distinction explicit.

Recommended wording:

```text
Channel_Idx:
Generator-level ttbar decay category based on direct W decay products, before resolving tau decays.
The tau-inv. bin corresponds to the negative-code tau/unresolved category.
```

```text
Channel_Idx_Final:
Generator-level ttbar decay category after resolving tau -> e/mu decays.
Hadronic tau decays are classified according to the resulting visible e/mu multiplicity.
```

### 5.5 Recommended validation plot

A useful follow-up diagnostic is a 2D migration plot:

```text
x-axis: Channel_Idx
y-axis: Channel_Idx_Final
```

This would directly show where events in the `Channel_Idx` tau-side category migrate in `Channel_Idx_Final`. The expected structure is that entries from the negative-code tau bin move into `all-had`, `e+jets`, `mu+jets`, `ee`, `emu`, or `mumu` depending on the tau decay products.

## 6. Validation performed here

A Python syntax check was performed with:

```bash
python3 -m py_compile plotGenCat.py
```

The full ROOT/EOS run was not executed in this environment because the local sandbox does not have access to the CERN EOS input directories or the CMSSW runtime environment. The commands in the README are intended to be run on lxplus or another environment with ROOT/PyROOT and the EOS paths available.

