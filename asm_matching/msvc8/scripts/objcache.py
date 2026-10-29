"""objcache.py - a content-addressed cache for the oracle's compiles and decodes.

A result is stored under the hash of everything that produced it, so an
unchanged input is never compiled (or decoded) twice, whichever tree, build
directory or run asks. Two levels, as ccache's direct mode:

  manifest  base key -> [(dependency, its hash), ...] -> result key
            The base key covers what is known before the compile (compiler,
            command line, the translated source); the dependencies are the
            headers /showIncludes reported the last time, each recorded with
            its content hash. A manifest entry whose dependencies all still
            hash the same names the result.
  result    result key -> a directory of files (object, log, dependencies)

Dependencies are recorded relative to the repository, as the compiler sees
them (drive:\\<path in the repo>), so two checkouts mapped to the same drive
letter share results; the command line holds the drive, so different letters
never do.

Settings (environment):
  ORACLE_CACHE    the cache directory (default $XDG_CACHE_HOME/d2-msvc8-oracle,
                  else ~/.cache/d2-msvc8-oracle); 'off' disables it
  ORACLE_NOCACHE  1: never read from the cache (still write) - a full run
"""
import fcntl
import hashlib
import json
import os
import shutil
import tempfile

_root = os.environ.get('ORACLE_CACHE') or os.path.join(
    os.environ.get('XDG_CACHE_HOME') or os.path.join(os.path.expanduser('~'), '.cache'), 'd2-msvc8-oracle')
ENABLED = _root != 'off'
ROOT = _root if ENABLED else None
READ = ENABLED and os.environ.get('ORACLE_NOCACHE', '0') != '1'
MANIFEST_KEEP = 16          # entries per base key: one per header state seen recently

_file_memo = {}


def sha_bytes(*parts):
    h = hashlib.sha256()
    for p in parts:
        if isinstance(p, str):
            p = p.encode('utf-8', 'surrogateescape')
        h.update(len(p).to_bytes(8, 'little'))
        h.update(p)
    return h.hexdigest()


def sha_file(path):
    """Content hash of a file, or '-' if it does not exist. Memoised per
    process on (inode, size, mtime): files change between runs, not within one."""
    try:
        st = os.stat(path)
    except OSError:
        return '-'
    k = (path, st.st_ino, st.st_size, st.st_mtime_ns)
    v = _file_memo.get(k)
    if v is None:
        h = hashlib.sha256()
        with open(path, 'rb') as f:
            for chunk in iter(lambda: f.read(1 << 20), b''):
                h.update(chunk)
        v = _file_memo[k] = h.hexdigest()
    return v


def _dir(kind, key):
    return os.path.join(ROOT, kind, key[:2], key)


def get(kind, key):
    """The result directory for `key`, or None."""
    if not READ:
        return None
    d = _dir(kind, key)
    return d if os.path.isfile(os.path.join(d, 'meta.json')) else None


def meta(d):
    with open(os.path.join(d, 'meta.json')) as f:
        return json.load(f)


def put(kind, key, files, info=None):
    """Store `files` (name -> bytes, or name -> source path) under `key`.
    Written aside and renamed, so a reader sees a whole entry or none."""
    if not ENABLED:
        return
    d = _dir(kind, key)
    if os.path.isfile(os.path.join(d, 'meta.json')):
        return
    os.makedirs(os.path.dirname(d), exist_ok=True)
    tmp = tempfile.mkdtemp(prefix='.tmp-', dir=os.path.dirname(d))
    try:
        for name, data in files.items():
            dst = os.path.join(tmp, name)
            if isinstance(data, (bytes, bytearray)):
                with open(dst, 'wb') as f:
                    f.write(data)
            else:
                shutil.copyfile(data, dst)
        with open(os.path.join(tmp, 'meta.json'), 'w') as f:
            json.dump(info or {}, f)
        try:
            os.rename(tmp, d)
        except OSError:
            pass                       # another process stored the same key first
    finally:
        if os.path.isdir(tmp):
            shutil.rmtree(tmp, ignore_errors=True)


def _manifest_path(base):
    return os.path.join(ROOT, 'manifest', base[:2], base + '.json')


def lookup(base, hash_dep):
    """The result key a manifest entry of `base` names whose dependencies all
    hash as recorded (hash_dep(rel) -> current hash), or None."""
    if not READ:
        return None
    p = _manifest_path(base)
    try:
        with open(p) as f:
            entries = json.load(f)
    except (OSError, ValueError):
        return None
    for e in entries:
        if all(hash_dep(rel) == h for rel, h in e['deps']):
            return e['key']
    return None


def record(base, deps, key):
    """Add the entry (deps -> key) to the manifest of `base`, newest first."""
    if not ENABLED:
        return
    p = _manifest_path(base)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p + '.lock', 'w') as lk:
        fcntl.flock(lk, fcntl.LOCK_EX)
        try:
            with open(p) as f:
                entries = json.load(f)
        except (OSError, ValueError):
            entries = []
        entry = {'deps': deps, 'key': key}
        entries = [entry] + [e for e in entries if e['key'] != key][:MANIFEST_KEEP - 1]
        tmp = '%s.%d.tmp' % (p, os.getpid())
        with open(tmp, 'w') as f:
            json.dump(entries, f)
        os.replace(tmp, p)


def install(src, dst):
    """Copy a cached file into place atomically."""
    tmp = '%s.%d.tmp' % (dst, os.getpid())
    shutil.copyfile(src, tmp)
    os.replace(tmp, dst)
