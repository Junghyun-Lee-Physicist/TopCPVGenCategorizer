# GenCatTree validation plotting README

This README documents how to run the updated `validation/plotGenCat.py` script.

The updated script saves every individual plot as a **PDF** and also writes one combined multipage PDF named `gencat_validation.pdf`. By default it draws **overlay only** and does **not** draw stacked histograms. Stack plots are produced only when `--draw-stack` is explicitly given.

## 1. Setup

Run inside a CMSSW environment where ROOT/PyROOT is available:

```bash
cmssw-el8
cd /afs/cern.ch/user/j/junghyun/CMSSW_14_2_1/src/TopCPVGenCategorizer
cmsenv
```

Then replace the script:

```bash
cp /path/to/updated/plotGenCat.py validation/plotGenCat.py
chmod +x validation/plotGenCat.py
```

If you are copying from the ChatGPT artifact, save it as:

```bash
validation/plotGenCat.py
```

## 2. Input convention

The script expects one positional input per group. For example:

```bash
/eos/user/<initial>/<username>/CPV/TTToSemiLeptonic_2017
/eos/user/<initial>/<username>/CPV/TTTo2L2Nu_2017
/eos/user/<initial>/<username>/CPV/TTToHadronic_2017
```

For your account this may look like:

```bash
/eos/user/j/junghyun/CPV/TTToSemiLeptonic_2017
/eos/user/j/junghyun/CPV/TTTo2L2Nu_2017
/eos/user/j/junghyun/CPV/TTToHadronic_2017
```

Each input directory is recursively searched for `*_chunk*.root`. If none are found, the script searches for all `*.root` files.

## 3. Quick test: one file per sample

Before the full run, use `--max-files 1` to check whether the script runs and whether the plot style is reasonable:

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
    --max-files 1 \
    -o plots_test_onefile
```

Expected key outputs:

```text
plots_test_onefile/gencat_validation.pdf
plots_test_onefile/gencat_hists.root
plots_test_onefile/<observable>_overlay.pdf
```

No PNG files are produced.

## 4. Full run: overlay only, physical normalization

This is the main full run command. It draws all observables as overlay PDFs, applies `sigma*BR*lumi/Nevents` scaling if the sample labels match the `--scale` names, and does not stack the three samples.

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

Important: the strings after `--label` must match the keys used in `--scale`. For example, `--label SemiLep` must match `--scale SemiLep=365`.

## 5. Full run: shape comparison only

Use `--density` when you want to compare shapes rather than absolute yields. In this mode each sample histogram is area-normalized to one.

```bash
python3 validation/plotGenCat.py \
    /eos/user/j/junghyun/CPV/TTToSemiLeptonic_2017 \
    /eos/user/j/junghyun/CPV/TTTo2L2Nu_2017 \
    /eos/user/j/junghyun/CPV/TTToHadronic_2017 \
    --label SemiLep --label 2L2Nu --label Hadronic \
    --density \
    -o plots_full_shape_pdf
```

For decay-channel validation, this is often the most readable option because it shows whether the expected decay category dominates each dedicated sample.

## 6. Channel_Idx checks: named version and numeric-code version

The updated script contains both versions:

- `Channel_Idx`: named categorical x-axis, such as `all-had`, `e+jets`, `#mu+jets`, `ee`, `e#mu`, `#mu#mu`, `#tau-inv.`, `other`.
- `Channel_Idx_numeric`: raw integer-code x-axis using the actual `Channel_Idx` branch value.

Run both in one command:

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

Expected individual plot files:

```text
plots_channel_idx_named_and_numeric/Channel_Idx_overlay.pdf
plots_channel_idx_named_and_numeric/Channel_Idx_numeric_overlay.pdf
plots_channel_idx_named_and_numeric/gencat_validation.pdf
plots_channel_idx_named_and_numeric/gencat_hists.root
```

## 7. Channel_Idx_Final checks: named version and numeric-code version

The same dual representation is available for `Channel_Idx_Final`:

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
    --only Channel_Idx_Final,Channel_Idx_Final_numeric \
    -o plots_channel_idx_final_named_and_numeric
```

## 8. Channel validation with shape normalization

For a pure sanity check of channel categorization, use `--density` and both named/numeric versions:

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

## 9. Run a selected subset of observables

Use `--only` with comma-separated keys:

```bash
python3 validation/plotGenCat.py \
    /eos/user/j/junghyun/CPV/TTToSemiLeptonic_2017 \
    /eos/user/j/junghyun/CPV/TTTo2L2Nu_2017 \
    /eos/user/j/junghyun/CPV/TTToHadronic_2017 \
    --label SemiLep --label 2L2Nu --label Hadronic \
    --only Channel_Idx,Channel_Lepton_Count,GenJet_N,GenBJet_Count \
    --density \
    -o plots_selected_observables
```

If an unknown observable key is given, the script prints the list of valid keys and exits.

## 10. Enable stack plots only when needed

Stack plots are disabled by default to avoid visually merging SemiLep, 2L2Nu, and Hadronic samples. If you intentionally want composition-style stacked plots, add `--draw-stack`:

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

Expected stack files have the form:

```text
<observable>_stack.pdf
```

## 11. Use genWeight if present

The original GenCatTree may not store `genWeight`. If a future tree stores it, you can enable it with:

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
    --use-genweight \
    -o plots_full_genweight_pdf
```

If `genWeight` is absent, the script falls back to unweighted filling.

## 12. Output summary

For the default overlay-only full run, the output directory contains:

```text
gencat_validation.pdf          # combined multipage PDF
gencat_hists.root              # histogram archive
<observable>_overlay.pdf       # one PDF per overlay plot
```

If `--draw-stack` is used, it additionally contains:

```text
<observable>_stack.pdf
```

## 13. Useful observable keys

The most useful keys for the channel sanity check are:

```text
Channel_Idx
Channel_Idx_numeric
Channel_Idx_Final
Channel_Idx_Final_numeric
Channel_Lepton_Count
Channel_Lepton_Count_Final
```

The full script also includes:

```text
Channel_Jets_Abs
Channel_Tau_Lepton
Channel_Visible_Tau
GenTop_pt
GenAnTop_pt
GenTop_eta
GenAnTop_eta
GenJet_N
GenJet_pt
GenJet_HadronFlavour
GenMET_pt
GenBJet_Count
GenBJet_pt
GenBHad_FromTopWeakDecay
GenBHad_Flavour
SelectedIdx_2
SelectedIdx_3
SelectedIdx_4
SelectedIdx_8
SelectedIdx_10
GenPar_Idx
GenPar_pdgId
```

