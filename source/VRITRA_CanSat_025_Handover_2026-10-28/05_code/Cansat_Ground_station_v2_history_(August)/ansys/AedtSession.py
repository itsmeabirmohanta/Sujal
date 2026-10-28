"""One safe way to attach to AEDT.  Import this; never hand-roll the attach.

Two failure modes cost this project real work on 2026-08-09, and both are
prevented here rather than documented and re-learned.

1. THE NAME COLLISION THAT DESTROYED THE MODEL, TWICE
   AEDT names every new empty project `Project1`, `Project2`, `Project3` ...
   The saved project was also called `Project1.aedt`.  So

       pyaedt.Hfss(project="Project1", ...)

   binds to whichever `Project1` the running AEDT already holds - on a fresh
   launch, the EMPTY default - while still reporting
   `project_file = ...\\Documents\\Ansoft\\Project1.aedt`, because that is where
   a project of that name would live.  The next `save_project()` then wrote
   4 KB of nothing over 2.2 MB of model, and AEDT cleared the matching
   `Project1.aedtresults` folder to match, taking 1 577 MB of converged field
   solution with it.  Nothing goes to the Recycle Bin.

   Cure, both halves needed:
     - address projects by FULL PATH, never by bare name;
     - use a project name AEDT cannot generate (not `Project<N>`).

2. THE MISSING gRPC SERVER
   Launched without `-grpcsrv`, AEDT reports its port as -1.  PyAEDT then falls
   back to the deprecated DotNet COM path and dies deep inside itself with
       AttributeError: 'Desktop' object has no attribute '_Desktop__close_on_exit'
   which says nothing about the real cause.  Connect() refuses a session
   without a real port and says why.
"""

import os

import ansys.aedt.core as pyaedt
from ansys.aedt.core.generic.general_methods import active_sessions

PROJECT_DIR = r"C:\Users\kalbh\Documents\Ansoft"
PROJECT_NAME = "VritraCansatRf"
PROJECT_PATH = os.path.join(PROJECT_DIR, PROJECT_NAME + ".aedt")
DESIGN_NAME = "HFSSDesign1"
GRPC_PORT = 50051

AEDT_EXE = r"D:\Software\Ansys\ANSYS Inc\v261\AnsysEM\ansysedt.exe"


def GrpcSession():
    """The pid/port of a session with a REAL gRPC port, or (None, None)."""
    sessions = active_sessions() or {}
    good = {p: q for p, q in sessions.items() if q and q > 0}
    if not good:
        return None, None, sessions
    pid = sorted(good)[0]
    return pid, good[pid], sessions


def Connect(create=False, project_path=None, design=DESIGN_NAME):
    """Attach to a live AEDT and open the project BY FULL PATH.

    create=False (default) refuses to continue against a project that is not
    on disk or that comes back with no geometry, because the only thing an
    empty design can do from here is get saved over the real one.

    create=True is for the build script, which legitimately starts from an
    empty design and fills it.
    """
    path = project_path or PROJECT_PATH
    pid, port, seen = GrpcSession()
    if pid is None:
        raise SystemExit(
            "AEDT has no gRPC session (ports seen: %s).\n"
            "Launch it with:\n    \"%s\" -grpcsrv %d"
            % (list(seen.values()) or "none", AEDT_EXE, GRPC_PORT))

    exists = os.path.exists(path)
    size_kb = os.path.getsize(path) / 1024.0 if exists else 0.0
    print("attach: pid %d, port %d" % (pid, port))
    print("project: %s (%s, %.1f KB)"
          % (path, "on disk" if exists else "NEW", size_kb))

    if not create:
        if not exists:
            raise SystemExit("project does not exist: %s" % path)
        # An AEDT project holding real geometry is hundreds of KB at least.
        # Anything near 4 KB is the empty default, and opening it read-only is
        # pointless while saving it is destructive.
        if size_kb < 100:
            raise SystemExit(
                "REFUSING: %.1f KB is an empty default project, not the model.\n"
                "Restore from ansys\\_backup_*\\*.PRISTINE.aedt before going on."
                % size_kb)

    app = pyaedt.Hfss(project=path, design=design, new_desktop=False,
                      port=port, aedt_process_id=pid)

    solids = app.modeler.solid_names
    print("opened: project=%s design=%s type=%s solids=%d"
          % (app.project_name, app.design_name, app.solution_type, len(solids)))

    if not create and not solids:
        app.release_desktop(close_projects=False, close_desktop=False)
        raise SystemExit(
            "EMPTY DESIGN after opening %s.\n"
            "Did NOT get the saved model - refusing to continue so that "
            "nothing saves over it." % path)
    return app


def Release(app):
    """Detach without closing the user's AEDT window or its projects."""
    app.release_desktop(close_projects=False, close_desktop=False)
