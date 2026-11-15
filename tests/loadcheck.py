# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the optional bench helper without a MIDI backend or attached hardware."""
import contextlib
import io
from pathlib import Path
import sys
import types
import unittest
from unittest import mock

sys.path.insert(0,str(Path(__file__).resolve().parents[1] / "tools"))
import load_test


class LoadTests(unittest.TestCase):
    def test_dry_run_never_imports_midi(self):
        with mock.patch.dict(sys.modules,{"mido":None}), contextlib.redirect_stdout(io.StringIO()) as out:
            self.assertEqual(load_test.main(["8","0.1","--channel","16","--dry-run"]),0)
        self.assertIn("8 notes",out.getvalue())

    def test_invalid_load_is_rejected_before_midi(self):
        for args in (["0"],["9"],["8","0"],["8","nan"],["8","inf"],["--channel","17"]):
            with self.subTest(args=args), mock.patch.dict(sys.modules,{"mido":None}), \
                    contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                load_test.main(args)
            self.assertEqual(error.exception.code,2)

    def test_interruption_sends_channel_correct_cleanup(self):
        port = mock.MagicMock()
        midi = types.SimpleNamespace(get_output_names=lambda:["FM-1 test"],open_output=lambda _:port,
                                     Message=lambda kind,**values:dict(type=kind,**values))
        with mock.patch.dict(sys.modules,{"mido":midi}), \
                mock.patch.object(load_test.time,"sleep",side_effect=KeyboardInterrupt), \
                contextlib.redirect_stdout(io.StringIO()), self.assertRaises(KeyboardInterrupt):
            load_test.main(["8","1","--channel","16"])
        messages = [call.args[0] for call in port.__enter__.return_value.send.call_args_list]
        self.assertEqual(sum(m["type"]=="note_on" for m in messages),8)
        self.assertEqual(sum(m["type"]=="note_off" for m in messages),8)
        self.assertTrue(all(m["channel"]==15 for m in messages))
        self.assertEqual(messages[-1],dict(type="control_change",control=123,value=0,channel=15))

    def test_multiple_candidates_require_an_explicit_port(self):
        midi = types.SimpleNamespace(get_output_names=lambda:["FM-1 A","FM-1 B"],open_output=mock.Mock())
        with mock.patch.dict(sys.modules,{"mido":midi}), contextlib.redirect_stderr(io.StringIO()), \
                self.assertRaises(SystemExit):
            load_test.main(["8","1"])
        midi.open_output.assert_not_called()


if __name__ == "__main__":
    unittest.main()
