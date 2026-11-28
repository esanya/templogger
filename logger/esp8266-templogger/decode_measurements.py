#!/usr/bin/env python3
"""Decode measurement.bin records written by appendMeasurement()."""

import argparse
import csv
import struct
import sys
from datetime import datetime, timedelta


RECORD_SIZE = 13
DATA_FORMAT = struct.Struct("<IeeeB")


def crc16_ccitt(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def decode_file(source, startdatetime: datetime, deltaTime: int):
    writer = csv.writer(sys.stdout, lineterminator="\n")
    writer.writerow(("count", "t1", "h1", "dewPoint", "sensorError", "Spread", "timestamp"))
    has_errors = False
    record_number = 0
    timestamp = startdatetime

    while True:
        record = source.read(RECORD_SIZE)
        if not record:
            break
        record_number += 1
        if len(record) != RECORD_SIZE:
            print(
                f"record {record_number}: incomplete record ({len(record)} of "
                f"{RECORD_SIZE} bytes)",
                file=sys.stderr,
            )
            has_errors = True
            break

        expected_crc = crc16_ccitt(record[:11])
        stored_crc = int.from_bytes(record[11:13], "little")
        if stored_crc != expected_crc:
            print(
                f"record {record_number}: CRC mismatch "
                f"(stored 0x{stored_crc:04x}, computed 0x{expected_crc:04x})",
                file=sys.stderr,
            )
            has_errors = True
            continue

        count, t1, humidity, dew_point, sensor_error = DATA_FORMAT.unpack(
            record[:11]
        )
        writer.writerow((count, t1, humidity, dew_point, sensor_error, t1-dew_point, timestamp))
        timestamp += timedelta(minutes=deltaTime)

    return 1 if has_errors else 0


def main():
    parser = argparse.ArgumentParser(
        description="Decode ESP8266 templogger measurement records as CSV."
    )
    parser.add_argument("binary_file", help="path to the measurement binary file")
    parser.add_argument(
        "--startdatetime",
        type=datetime.fromisoformat,
        required=True,
        help="timestamp for the first decoded record (ISO 8601)",
    )
    parser.add_argument(
        "--deltaTime",
        type=int,
        required=True,
        help="minutes between decoded records",
    )
    args = parser.parse_args()

    try:
        with open(args.binary_file, "rb") as source:
            return decode_file(source, args.startdatetime, args.deltaTime)
    except OSError as error:
        print(f"{args.binary_file}: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
