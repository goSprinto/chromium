# How to Deal with Android Size Alerts

 >
 > Not all alerts should not have a bug created for them. Please read on...
 >

[TOC]

## Step 1: Identify the Commit

### Monochrome.minimal.apks Alerts (Single Commit)

 * Zoom in on the graph to make sure the alert is not
   [off-by-one](https://github.com/catapult-project/catapult/issues/3444)
   * Replace `&num_points=XXXX` with `&rev=COMMIT_POSITION` in the URL.
   * It will be obvious from this whether or not the point is off. Use the
     "nudge" feature to correct it when this happens.

### Monochrome.minimal.apks Alerts (Multiple Commits or Rolls)

 * Bisects [will not help you](https://bugs.chromium.org/p/chromium/issues/detail?id=678338).
 * For rolls, you can sometimes guess the commit(s) that caused the regression
   by looking at the `android-binary-size` trybot result for the roll commit.
 * For V8 rolls, try checking the [V8 size graph](https://chromeperf.appspot.com/report?sid=59435a74c93b42599af4b02e2b3df765faef4685eb015f8aaaf2ecf7f4afb29c)
   to see if any jumps correspond with a CL in the roll.
 * Otherwise, use [diagnose_bloat.py](https://chromium.googlesource.com/chromium/src/+/master/tools/binary_size/README.md#diagnose_bloat_py)
   in a [local Android checkout](https://chromium.googlesource.com/chromium/src/+/master/docs/android_build_instructions.md)
   to build all commits locally and find the culprit.
   * If there were multiple commits due to a build breakage, use `--apply-patch`
     with the fixing commit (last one in the range).

**Example:**

     tools/binary_size/diagnose_bloat.py AFTER_GIT_REV --reference-rev BEFORE_GIT_REV --all [--subrepo v8] [--apply-patch AFTER_GIT_REV]

 * You can usually find the before and after revs in the roll commit message
([example](https://chromium.googlesource.com/chromium/src/+/10c40fd863f4ae106650bba93b845f25c9b733b1))
    * You may need to click through for the list of changes to find the actual
      first commit hash since some rollers (e.g. v8's) use an extra commit for
      tagging. In the linked example `BEFORE_GIT_REV` would actually be
      `876f37c` and not `c1dec05f`.

### SystemWebviewGoogle.apk Alerts

* Check if the same increase happened in Monochrome.minimal.apks.
   * The goal is to ensure nothing creeps into webview unintentionally.

## Step 2: File Bug or Silence Alert

* If the commit message's `Binary-Size:` footer clearly justifies the size
  increase, silence the alert.
* If the regression is < 100kb and caused by an AFDO roll, silence the alert.

Otherwise, file a bug (TODO: [Make this template automatic](https://github.com/catapult-project/catapult/issues/3150)):

 * Change the bug's title from `X%` to `XXkb`
 * Assign to commit author
 * Set description to (replacing **bold** parts):

> Caused by "**First line of commit message**"
>
> Commit: **abc123abc123abc123abc123abc123abc123abcd**
>
> Link to size graph:
> [https://chromeperf.appspot.com/report?sid=6269078068c45a41e23f5ee257da65d3f02da342849cdf3bde6aed0d5c61e450&num_points=10&rev=**$CRREV**](https://chromeperf.appspot.com/report?sid=6269078068c45a41e23f5ee257da65d3f02da342849cdf3bde6aed0d5c61e450&num_points=10&rev=480214)<br>
> Link to trybot result:
> [https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android-binary-size/**$TRYJOB_NUMBER**](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android-binary-size/11111)
>
> Debugging size regressions is documented at:
> https://chromium.googlesource.com/chromium/src/+/master/docs/speed/apk_size_regressions.md#Debugging-Apk-Size-Increase
>
> Based on the trybot result: **20kb of native code, 8kb of pngs. *(or some other explanation as to what caused the growth).***
>
> It's not clear to me whether or not this increase was expected.<br>
> Please have a look and either:
>
> 1. Close as "Won't Fix" with a short justification, or
> 2. Land a revert / fix-up.
>
> _**Optional addition:**_
>
> It typically takes about a week of engineering time to reduce binary size by
> 50kb so we'd really appreciate you taking some time exploring options to
> address this regression!

* If the regression is >50kb, add ReleaseBlock-Stable **M-##** (next branch cut).*
* If the regression was caused by a non-Googler, assign it to the closest Googler
  to the patch (e.g. reviewer). The size graphs are [not public](https://bugs.chromium.org/p/chromium/issues/detail?id=962483).

# Debugging Apk Size Increase

It typically takes about a week of engineering time to reduce binary size by
50kb so it's important that an effort is made to address all new regressions.

## Step 1: Identify what Grew

Figure out which file within the `.apk` increased (native library, dex, pak
resources, etc.) by looking at the trybot results or size graphs that were
linked from the bug (if it was not linked in the bug, see above).

**See [//docs/speed/binary_size/metrics.md](https://chromium.googlesource.com/chromium/src/+/master/docs/speed/binary_size/metrics.md)
for a description of high-level binary size metrics.**

**See [//tools/binary_size/README.md](https://chromium.googlesource.com/chromium/src/+/master/tools/binary_size/README.md)
for a description of binary size tools.**

## Step 2: Analyze

### Growth is from Translations

 * There is likely nothing that can be done. Translations are expensive.
 * Close as `Won't Fix`.

### Growth is from Native Resources (pak files)

 * Ensure `compress="gzip"` or `compress="brotli"` is used for all
   highly-compressible (e.g. text) resources.
   * Brotli compresses more but is much slower to decompress. Use brotli only
     when performance doesn't matter much (e.g. internals pages).
 * Look at the SuperSize reports from the trybot to look for unexpected
   resources, or unreasonably large symbols.

### Growth is from Images

 * Would [a VectorDrawable](https://codereview.chromium.org/2857893003/) be better?
   * If so, try optimizing it with [avocado](https://bugs.chromium.org/p/chromium/issues/detail?id=982302).
 * Would **lossy** compression make sense (often true for large images)?
   * Then [use lossy webp](https://codereview.chromium.org/2615243002/).
   * And omit some densities (e.g. add only an xxhdpi version).
 * Would **near-lossless** compression make sense (try it and see)?
   * [Use pngquant](https://pngquant.org) to reduce the color depth without a
     perceptual difference (use one of the GUI tools to compare before/afters).
 * Are the **lossless** images fully optimized?
   * Use [tools/resources/optimize-png-files.sh](https://cs.chromium.org/chromium/src/tools/resources/optimize-png-files.sh).
   * There is some [Googler-specific guidance](https://goto.google.com/clank/engineering/best-practices/adding-image-assets) as well.

#### What Build-Time Image Optimizations are There?
 * For non-ninepatch images, `drawable-xxxhdpi` are omitted (they are not
   perceptibly different from xxhdpi in most cases).
 * For non-ninepatch images within res/ directories (not for .pak file images),
   they are converted to webp.
   * Use the `android-binary-size` trybot to see the size of the images as webp,
     or just build `ChromePublic.apk` and use `unzip -l` to see the size of the
     images within the built apk.

### Growth is from Native Code

 * Look at the SuperSize reports from the trybot to look for unexpected symbols,
   or unreasonably large symbols.
 * If the diff looks reasonable, close as `Won't Fix`.
 * Otherwise, try to refactor a bit (e.g.
 [move code out of templates](https://bugs.chromium.org/p/chromium/issues/detail?id=716393)).
   * Use [//tools/binary_size/diagnose_bloat.py](https://chromium.googlesource.com/chromium/src/+/master/tools/binary_size/README.md)
     or the android-binary-size trybot to spot-check your local changes.
 * If symbols are larger than expected, use the `Disassemble()` feature of
   `supersize console` to see what is going on.

### Growth is from Java Code

 * Look at the SuperSize reports from the trybot to look for unexpected methods.
 * Ensure any new Java deps are as specific as possible.

### Growth is from "other lib size" or "Unknown files size"

 * File a bug under [Tools > BinarySize](https://bugs.chromium.org/p/chromium/issues/list?q=component%3ATools%3EBinarySize)
   with a link to your commit.

### You Would Like Assistance

 * Feel free to email [binary-size@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/binary-size).

## Step 3: Give Up :/

If you have spent O(days) trying to reduce the size overhead of your patch and
are pretty sure that your implementation is efficient, then add a comment to the
bug with the following:

1) A description of where the size is coming from (show that you spent the time
   to understand why your code translated to a large binary size).
2) What things you tried to reduce the size (show that you've at least read this
   doc and tried any relevant guidance).
3) Why your commit is "worth" the size increase. For new features, feel free
   to link to a design doc (which presumably includes the motivation for adding
   the feature).

Close the bug as "Won't Fix".

# For Binary Size Sheriffs

Here is the [rotation](https://rota-ng.appspot.com/oncall?name=Binary%20Size%20Sheriff)
and [calendar](https://calendar.google.com/calendar/embed?src=c_188b8oq5puj67tl346uj3q4qosaio4gactnmuprcckn66rrd%40group.calendar.google.com&ctz=America%2FToronto).

## Step 1: Check Work Queue Daily

 * Bugs requiring sheriffs to take a look at are labeled `Performance-Sheriff`
   and `Performance-Size` [here](https://bugs.chromium.org/p/chromium/issues/list?q=label:Performance-Sheriff%20label:Performance-Size&sort=-modified).
 * After resolving the bug by finding an owner or debugging or commenting,
   remove the `Performance-Sheriff` label.

## Step 2: Check Alerts Regularly

 * Check [alert page](https://chromeperf.appspot.com/alerts?sheriff=Binary%20Size%20Sheriff) regularly for new alerts.
 * Join [g/chrome-binary-size-alerts](https://goto.google.com/chrome-binary-size-alerts).
 * Deal with alerts as outlined above.

## Step 3: Ping / Clear out Old Regression Bugs
 * https://bugs.chromium.org/p/chromium/issues/list?can=2&q=label%3DPerformance-Size+type%3DBug-Regression+resource_sizes
