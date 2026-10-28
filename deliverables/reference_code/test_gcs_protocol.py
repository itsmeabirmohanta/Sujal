"""Offline protocol tests; no serial or radio connection."""
import io
import unittest
from gcs_protocol import parse_core, current_marker, target_commands, FlightCsvWriter, FIELDS

class ProtocolTests(unittest.TestCase):
    def packet(self):
        return b'2026-INSPACe-CAN-7USAT-025,1,2,100,100000,25,12,43200,26.717535,84.301543,100,8,0,0,9.8,60000,6\r'
    def test_live_csv_and_expiry(self):
        p=parse_core(self.packet(),100,'LIVE_SERIAL')
        self.assertEqual(current_marker(p,101,100),(26.717535,84.301543))
        self.assertIsNone(current_marker(p,201,100))
        self.assertIsNone(current_marker(p,99,100))
        output=io.StringIO();w=FlightCsvWriter(output);w.append(p)
        self.assertEqual(len(output.getvalue().splitlines()),2)
    def test_invalid_transport_and_coordinates(self):
        for transport in ('REPLAY','SIMULATION',''):
            with self.assertRaises(ValueError):parse_core(self.packet(),100,transport)
        for packet in (self.packet().replace(b'26.717535',b'nan'),self.packet().replace(b'84.301543',b''),self.packet()+b'\n'):
            with self.assertRaises(ValueError):parse_core(packet,100,'LIVE_SERIAL')
    def test_missing_measurements_stay_missing(self):
        cells=self.packet().decode().strip().split(',')
        for name in ('GNSS_TIME','LAT','LON','GPS_ALT_M','SATS'):cells[FIELDS.index(name)]=''
        p=parse_core((','.join(cells)+'\r').encode(),100,'LIVE_SERIAL')
        self.assertIsNone(current_marker(p,100,100))
        output=io.StringIO();FlightCsvWriter(output).append(p)
        self.assertIn(',,,,,,',output.getvalue())
    def test_target_transaction_budget(self):
        lines=target_commands(-90,-180,255,0xffffffff)
        self.assertTrue(all(len(line)<=32 and not line.endswith(b'\r') for line in lines))
        self.assertIn(b'-1800000000',lines[1]);self.assertIn(b',21,0,FF,FFFFFFFF',lines[2])
        with self.assertRaises(ValueError):target_commands(float('nan'),0,1,1)
        with self.assertRaises(ValueError):target_commands(0,0,1,0)

if __name__=='__main__':unittest.main()
