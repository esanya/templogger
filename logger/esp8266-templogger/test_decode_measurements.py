import csv
import io
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest

import decode_measurements


SCRIPT = pathlib.Path(__file__).with_name("decode_measurements.py")


def make_record(count, t1, humidity, dew_point, sensor_error):
    payload = struct.pack("<IeeeB", count, t1, humidity, dew_point, sensor_error)
    crc = decode_measurements.crc16_ccitt(payload)
    return payload + crc.to_bytes(2, "little")


class DecodeMeasurementsTests(unittest.TestCase):
    def run_decoder(self, contents):
        with tempfile.TemporaryDirectory() as directory:
            binary_file = pathlib.Path(directory) / "measurement.bin"
            binary_file.write_bytes(contents)
            return subprocess.run(
                [sys.executable, str(SCRIPT), str(binary_file)],
                capture_output=True,
                text=True,
                check=False,
            )

    def test_outputs_valid_records_and_reports_bad_crc(self):
        valid_record = make_record(42, 21.5, 58.25, 12.75, 0)
        invalid_record = bytearray(make_record(43, 22.0, 60.0, 13.0, 1))
        invalid_record[0] ^= 0x01

        result = self.run_decoder(valid_record + invalid_record)

        self.assertEqual(result.returncode, 1)
        rows = list(csv.reader(io.StringIO(result.stdout)))
        self.assertEqual(
            rows,
            [
                ["count", "t1", "h1", "dewPoint", "sensorError"],
                ["42", "21.5", "58.25", "12.75", "0"],
            ],
        )
        self.assertIn("record 2: CRC mismatch", result.stderr)

    def test_reports_incomplete_record(self):
        result = self.run_decoder(make_record(42, 21.5, 58.25, 12.75, 0) + b"\x01\x02")

        self.assertEqual(result.returncode, 1)
        self.assertIn("record 2: incomplete record", result.stderr)


if __name__ == "__main__":
    unittest.main()
