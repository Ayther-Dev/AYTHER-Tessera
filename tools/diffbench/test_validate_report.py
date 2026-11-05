import json, pathlib, subprocess, sys, tempfile, unittest
V = pathlib.Path(__file__).with_name("validate_report.py")
GOOD = {"tool": "tessera_diffbench", "contract": 1, "entry": "fx-02", "segment": [0, 600],
        "reference": {"core": "gpgx", "build_id": "r4"}, "candidate": {"core": "gpgx", "build_id": "r4"},
        "frames_compared": 600, "first_divergence": None, "exceptions_applied": [], "audio_writes": {"frames_equal": 600}}

def run(doc):
    with tempfile.TemporaryDirectory() as d:
        p = pathlib.Path(d) / "r.json"; p.write_text(json.dumps(doc), encoding="utf-8")
        return subprocess.run([sys.executable, "-I", str(V), str(p)], capture_output=True, text=True).returncode

class T(unittest.TestCase):
    def test_valid(self): self.assertEqual(run(GOOD), 0)
    def test_rejects_extra_field_such_as_pcm(self):
        d = dict(GOOD); d["pcm"] = [1, 2, 3]; self.assertNotEqual(run(d), 0)
    def test_rejects_missing(self):
        d = dict(GOOD); del d["entry"]; self.assertNotEqual(run(d), 0)
    def test_rejects_bad_plane(self):
        d = dict(GOOD); d["first_divergence"] = {"frame": 1, "plane": "pcm"}; self.assertNotEqual(run(d), 0)
    def test_accepts_divergence(self):
        d = dict(GOOD); d["first_divergence"] = {"frame": 3, "plane": "framebuffer", "line": 1, "x": 2, "reference": "#000000", "candidate": "#FFFFFF"}
        self.assertEqual(run(d), 0)
if __name__ == "__main__": unittest.main()
