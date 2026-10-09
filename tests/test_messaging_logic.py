import hashlib
import struct
import unittest

MAGIC = 0x504D
VERSION = 1
HEADER = ">HBBQQIIH"
HEADER_SIZE = struct.calcsize(HEADER)


def pair_key(code: str, first: int, second: int) -> bytes:
    first, second = sorted((first, second))
    return hashlib.sha256(b"PocketPDA pair v1" + code.encode() + struct.pack(">QQ", first, second)).digest()


def header(packet_type=1, sender=1, recipient=2, message_id=3, counter=4, payload=0):
    return struct.pack(HEADER, MAGIC, VERSION, packet_type, sender, recipient, message_id, counter, payload)


def valid_header(data: bytes) -> bool:
    if len(data) < HEADER_SIZE:
        return False
    magic, version, packet_type, sender, _, _, _, payload = struct.unpack(HEADER, data[:HEADER_SIZE])
    return magic == MAGIC and version == VERSION and 1 <= packet_type <= 4 and sender != 0 and payload <= 160


class RetryModel:
    def __init__(self):
        self.attempts = 0
        self.state = "queued"

    def transmit(self):
        self.attempts += 1
        self.state = "sending"

    def timeout(self):
        self.state = "failed" if self.attempts >= 3 else "queued"


class MessagingLogicTests(unittest.TestCase):
    def test_pair_key_is_symmetric(self):
        self.assertEqual(pair_key("trusted-code", 0xAA, 0xBB), pair_key("trusted-code", 0xBB, 0xAA))

    def test_pair_key_changes_with_code_or_identity(self):
        base = pair_key("trusted-code", 1, 2)
        self.assertNotEqual(base, pair_key("other-code", 1, 2))
        self.assertNotEqual(base, pair_key("trusted-code", 1, 3))

    def test_header_rejects_malformed_packets(self):
        self.assertTrue(valid_header(header(payload=160)))
        self.assertFalse(valid_header(b"short"))
        self.assertFalse(valid_header(header(payload=161)))
        bad = bytearray(header())
        bad[2] = 99
        self.assertFalse(valid_header(bytes(bad)))

    def test_duplicate_identity_is_sender_and_message(self):
        seen = set()
        packets = [(7, 42), (7, 42), (8, 42), (7, 43)]
        accepted = []
        for identity in packets:
            if identity not in seen:
                seen.add(identity)
                accepted.append(identity)
        self.assertEqual(accepted, [(7, 42), (8, 42), (7, 43)])

    def test_retry_is_bounded(self):
        item = RetryModel()
        for expected in ("queued", "queued", "failed"):
            item.transmit()
            item.timeout()
            self.assertEqual(item.state, expected)
        self.assertEqual(item.attempts, 3)


if __name__ == "__main__":
    unittest.main()
