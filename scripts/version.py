# =====================================================================
#  version.py — PlatformIO pre-build hook
#
#  Bakes a build identifier into the firmware as the VORD_BUILD macro so the
#  AP web dashboard can show the same "build number" as the flasher site.
#
#  The format mirrors docs/version.json, produced by
#  .github/workflows/deploy-flasher.yml:
#      {UTC-month}.{UTC-day}.{git-short-sha}     e.g. 6.12.51998fe
#  Built in the same CI run as version.json (same UTC day + same commit), so
#  the embedded string matches what the flasher displays. Local dev builds get
#  their own live git value; builds without git fall back to "nogit".
# =====================================================================
import datetime
import subprocess

Import("env")  # noqa: F821  (injected by PlatformIO/SCons)


def _git_sha7():
    try:
        out = subprocess.check_output(
            ["git", "rev-parse", "HEAD"],
            cwd=env["PROJECT_DIR"],          # noqa: F821
            stderr=subprocess.DEVNULL,
        )
        return out.decode().strip()[:7]
    except Exception:
        return "nogit"


_now = datetime.datetime.now(datetime.timezone.utc)
_build = "{}.{}.{}".format(_now.month, _now.day, _git_sha7())

print("VORD_BUILD = " + _build)

# Pass the build id as a RAW, unquoted token (-DVORD_BUILD=6.12.823127a); config.h
# stringizes it. Avoids quote-escaping differences between SCons and arduino-cli.
env.Append(CPPDEFINES=[("VORD_BUILD", _build)])  # noqa: F821
