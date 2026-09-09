#!/usr/bin/env python3
"""Build a GPT-partitioned FAT32 disk image for RoboBootloader.

Pure Python (no mtools/mkfs.vfat dependency). Layout:

  LBA 0        protective MBR (0xEE)
  LBA 1        GPT header
  LBA 2..33    GPT partition entries (one ESP entry)
  LBA 2048..   FAT32 filesystem inside an EFI System Partition

A real ESP (type GUID C12A7328-...) is what UEFI firmware requires before it
will auto-create a boot option and load \\EFI\\BOOT\\BOOTX64.EFI. Supports
long file names via standard VFAT LFN entries and proper '.'/'..' entries.

Usage:
  python tools/mkfat32.py --output disk.img --size-mb 64 \
      --file EFI/BOOT/BOOTX64.EFI:build/BOOTX64.EFI ...
  (--file maps <image path>:<host path>; image paths use '/')
"""
import argparse
import struct
import sys
import uuid
import zlib

BYTES_PER_SECTOR = 512
RESERVED_SECTORS = 32
NUM_FATS = 2
PARTITION_START_LBA = 2048
ESP_TYPE_GUID = uuid.UUID("C12A7328-F81F-11D2-BA4B-00A0C93EC93B")


def fat_checksum(short_name: bytes) -> int:
    s = 0
    for b in short_name:
        s = (((s & 1) << 7) + (s >> 1) + b) & 0xFF
    return s


def to_83(name: str) -> bytes:
    """Deterministic 8.3 short name: 6 chars + ~n when too long."""
    if "." in name:
        stem, ext = name.rsplit(".", 1)
    else:
        stem, ext = name, ""
    stem_u = stem.upper()
    ext_b = ext.upper()[:3].encode()
    if len(stem_u) <= 8 and all(c not in " +" for c in stem_u):
        short = stem_u.encode().ljust(8, b" ")
    else:
        base = stem_u.replace(".", "").replace(" ", "")[:6]
        short = (base + "~1").encode().ljust(8, b" ")
    return short + ext_b.ljust(3, b" ")


def lfn_entries(long_name: str, short: bytes) -> list[bytes]:
    """VFAT long-file-name entries (in on-disk reverse order later)."""
    chars = [ord(c) for c in long_name] + [0x0000]
    while len(chars) % 13:
        chars.append(0xFFFF)
    n_entries = len(chars) // 13
    cksum = fat_checksum(short)
    out = []
    for seq in range(1, n_entries + 1):
        chunk = chars[(seq - 1) * 13: seq * 13]
        e = bytearray(32)
        e[0] = seq | (0x40 if seq == n_entries else 0)
        for i, c in enumerate(chunk):
            if i < 5:
                struct.pack_into("<H", e, 1 + i * 2, c)
            elif i < 11:
                struct.pack_into("<H", e, 14 + (i - 5) * 2, c)
            else:
                struct.pack_into("<H", e, 28 + (i - 11) * 2, c)
        e[11] = 0x0F
        e[12] = 0
        e[13] = cksum
        out.append(bytes(e))
    return list(reversed(out))  # LFN entries precede short entry, reversed


class Node:
    def __init__(self, name, is_dir):
        self.name = name
        self.is_dir = is_dir
        self.children = {}      # for dirs
        self.data = b""         # for files
        self.cluster = 0        # first cluster (dirs), 0 for empty root


class Fat32Image:
    def __init__(self, size_bytes, label="ROBOBOOT"):
        self.label = label
        # filesystem lives inside the ESP partition starting at LBA 2048
        usable = size_bytes - PARTITION_START_LBA * BYTES_PER_SECTOR
        self.total_sectors = usable // BYTES_PER_SECTOR
        # ensure FAT32-legal cluster count
        if self.total_sectors < 66584:
            self.total_sectors = 66584
        self.spc = 1
        for spc in (1, 2, 4, 8, 16, 32, 64):
            clusters = (self.total_sectors - RESERVED_SECTORS) // spc
            if clusters >= 65525:
                self.spc = spc
                break
        self.cluster_count = (self.total_sectors - RESERVED_SECTORS) // self.spc
        self.fat_sectors = (self.cluster_count * 4 + BYTES_PER_SECTOR - 1) // BYTES_PER_SECTOR
        self.data_start = RESERVED_SECTORS + NUM_FATS * self.fat_sectors
        self.total_sectors = self.data_start + self.cluster_count * self.spc
        self.size = self.total_sectors * BYTES_PER_SECTOR

        self.img = bytearray(self.size)
        self.fat = bytearray(self.fat_sectors * BYTES_PER_SECTOR)
        self.next_cluster = 3  # 0,1 reserved; 2 = root
        struct.pack_into("<I", self.fat, 0, 0x0FFFFFF8)
        struct.pack_into("<I", self.fat, 4, 0x0FFFFFFF)

        self._write_boot_sector()
        # reserve root dir cluster
        self._set_fat(2, 0x0FFFFFFF)

    # -- low level ------------------------------------------------------
    def _set_fat(self, cluster, value):
        struct.pack_into("<I", self.fat, cluster * 4, value)

    def _write_boot_sector(self):
        bs = bytearray(BYTES_PER_SECTOR)
        bs[0:3] = b"\xEB\x3C\x90"
        bs[3:11] = b"MSWIN4.1"
        struct.pack_into("<H", bs, 11, BYTES_PER_SECTOR)
        bs[13] = self.spc
        struct.pack_into("<H", bs, 14, RESERVED_SECTORS)
        bs[16] = NUM_FATS
        struct.pack_into("<I", bs, 32, self.total_sectors)
        struct.pack_into("<I", bs, 36, self.fat_sectors)
        struct.pack_into("<H", bs, 24, 63)
        struct.pack_into("<H", bs, 26, 255)
        struct.pack_into("<I", bs, 44, 2)      # root cluster
        struct.pack_into("<H", bs, 48, 1)      # fsinfo
        struct.pack_into("<H", bs, 50, 6)      # backup
        bs[66] = 0x29
        struct.pack_into("<I", bs, 67, 0x524F424F)
        # Volume label: 11 bytes, space-padded, uppercase (FAT spec).
        lab = self.label.upper()[:11].ljust(11)
        bs[71:82] = lab.encode("ascii", "replace")
        bs[82:90] = b"FAT32   "
        struct.pack_into("<H", bs, 510, 0xAA55)
        self.img[0:512] = bs
        fsinfo = bytearray(BYTES_PER_SECTOR)
        struct.pack_into("<I", fsinfo, 0, 0x41615252)
        struct.pack_into("<I", fsinfo, 484, 0x61417272)
        struct.pack_into("<I", fsinfo, 488, self.cluster_count - 2)
        struct.pack_into("<I", fsinfo, 492, 3)
        struct.pack_into("<H", fsinfo, 510, 0xAA55)
        self.img[512:1024] = fsinfo
        self.img[512 * 6:512 * 7] = bs
        self.img[512 * 7:512 * 8] = fsinfo

    def _cluster_offset(self, c):
        return (self.data_start + (c - 2) * self.spc) * BYTES_PER_SECTOR

    def alloc_chain(self, nbytes):
        """Allocate a cluster chain big enough for nbytes; return first."""
        cap = self.spc * BYTES_PER_SECTOR
        n = max(1, (nbytes + cap - 1) // cap)
        first = self.next_cluster
        for i in range(n):
            cur = self.next_cluster
            nxt = (cur + 1) if i < n - 1 else 0x0FFFFFFF
            self._set_fat(cur, nxt)
            self.next_cluster += 1
            if self.next_cluster >= self.cluster_count + 2:
                raise SystemExit("image full: too many clusters")
        return first

    def write_data(self, first_cluster, data):
        cap = self.spc * BYTES_PER_SECTOR
        c, off = first_cluster, 0
        while off < len(data) or (off == 0 and len(data) == 0):
            base = self._cluster_offset(c)
            chunk = data[off:off + cap]
            self.img[base:base + len(chunk)] = chunk
            off += cap
            nxt = struct.unpack_from("<I", self.fat, c * 4)[0]
            if nxt >= 0x0FFFFFF8:
                break
            c = nxt

    # -- tree building ----------------------------------------------------
    def populate(self, files):
        """files: list of (image_path, data)."""
        self.root = Node("", True)
        self.root.cluster = 2  # root dir lives at the fixed root cluster
        for path, data in files:
            parts = [p for p in path.split("/") if p]
            d = self.root
            for p in parts[:-1]:
                if p not in d.children:
                    d.children[p] = Node(p, True)
                d = d.children[p]
                if not d.is_dir:
                    raise SystemExit("path conflict at " + p)
            name = parts[-1]
            f = Node(name, False)
            f.data = data
            d.children[name] = f

        # 1) allocate file data clusters
        self._alloc_files(self.root)
        # 2) allocate dir clusters bottom-up and write their contents
        self._alloc_dirs(self.root, parent_cluster=2)

    def _alloc_files(self, d):
        for ch in d.children.values():
            if ch.is_dir:
                self._alloc_files(ch)
            else:
                ch.cluster = self.alloc_chain(len(ch.data))
                self.write_data(ch.cluster, ch.data)

    def _alloc_dirs(self, d, parent_cluster):
        for ch in d.children.values():
            if ch.is_dir:
                ch.cluster = self.alloc_chain(0)  # placeholder; single cluster
                self._alloc_dirs(ch, parent_cluster=d.cluster)
        # write this directory's entries
        entries = bytearray()
        if d.cluster == 2:  # root: volume-label entry (attr 0x08, no cluster)
            # The UEFI FAT driver reports THIS entry as the volume label;
            # the BPB label alone is not enough.
            vlab = bytearray(32)
            vlab[0:11] = self.label.upper()[:11].ljust(11).encode("ascii", "replace")
            vlab[11] = 0x08
            entries += bytes(vlab)
        if d.cluster != 2:  # non-root: '.' and '..'
            dot = bytearray(32)
            dot[0:11] = b".          "
            dot[11] = 0x10
            struct.pack_into("<H", dot, 20, (d.cluster >> 16) & 0xFFFF)
            struct.pack_into("<H", dot, 26, d.cluster & 0xFFFF)
            entries += dot
            dotdot = bytearray(32)
            dotdot[0:11] = b"..         "
            dotdot[11] = 0x10
            struct.pack_into("<H", dotdot, 20, (parent_cluster >> 16) & 0xFFFF)
            struct.pack_into("<H", dotdot, 26, parent_cluster & 0xFFFF)
            entries += dotdot
        for ch in d.children.values():
            short = to_83(ch.name)
            e = bytearray(32)
            if ch.is_dir:
                e[0:11] = short
                e[11] = 0x10
                struct.pack_into("<H", e, 20, (ch.cluster >> 16) & 0xFFFF)
                struct.pack_into("<H", e, 26, ch.cluster & 0xFFFF)
                struct.pack_into("<H", e, 22, 0x6000)  # date
                entries += bytes(e)
            else:
                if not self._is_exact_83(ch.name):
                    for lfn in lfn_entries(ch.name, short):
                        entries += lfn
                e[0:11] = short
                e[11] = 0x20
                struct.pack_into("<H", e, 20, (ch.cluster >> 16) & 0xFFFF)
                struct.pack_into("<H", e, 26, ch.cluster & 0xFFFF)
                struct.pack_into("<H", e, 22, 0x6000)
                struct.pack_into("<I", e, 28, len(ch.data))
                entries += bytes(e)
        entries += b"\x00" * 32  # end-of-directory marker
        cap = self.spc * BYTES_PER_SECTOR
        if len(entries) > cap:
            raise SystemExit("directory too large for one cluster: " + d.name)
        self.write_data(d.cluster, bytes(entries))

    @staticmethod
    def _is_exact_83(name):
        if "." not in name:
            return len(name) <= 8
        stem, ext = name.rsplit(".", 1)
        return len(stem) <= 8 and len(ext) <= 3 and " " not in name

    def finish(self):
        for i in range(NUM_FATS):
            start = (RESERVED_SECTORS + i * self.fat_sectors) * BYTES_PER_SECTOR
            self.img[start:start + len(self.fat)] = self.fat
        return bytes(self.img)


def build_gpt(disk_bytes, fs_offset_lba, fs_sectors):
    """Return (protective MBR, GPT header+entries) sector bytes."""
    total_lbas = disk_bytes // BYTES_PER_SECTOR

    # -- protective MBR (LBA 0) ------------------------------------------
    mbr = bytearray(BYTES_PER_SECTOR)
    pe = mbr[446:446 + 16]
    pe[0] = 0x00                       # not bootable (protective)
    pe[1:4] = b"\x00\x02\x00"          # CHS first (dummy, cylinder 0)
    pe[4] = 0xEE                       # GPT protective
    pe[5:8] = b"\xFF\xFF\xFF"          # CHS last (dummy)
    struct.pack_into("<I", pe, 8, 1)                       # first LBA
    struct.pack_into("<I", pe, 12, total_lbas - 1)         # count
    mbr[446:446 + 16] = pe
    struct.pack_into("<H", mbr, 510, 0xAA55)

    # -- partition entries (LBA 2..33) ------------------------------------
    entries = bytearray(128 * 128)
    e = bytearray(128)
    e[0:16] = ESP_TYPE_GUID.bytes_le
    e[16:32] = uuid.uuid5(uuid.NAMESPACE_OID, "robobootloader-esp").bytes_le
    struct.pack_into("<Q", e, 32, fs_offset_lba)           # first LBA
    struct.pack_into("<Q", e, 40, fs_offset_lba + fs_sectors - 1)
    struct.pack_into("<Q", e, 48, 0)                       # attributes
    name = "ESP"
    e[56:56 + 2 * len(name)] = name.encode("utf-16-le")
    entries[0:128] = e

    # -- GPT header (LBA 1) -----------------------------------------------
    hdr = bytearray(92)
    hdr[0:8] = b"EFI PART"
    struct.pack_into("<I", hdr, 8, 0x00010000)             # revision 1.0
    struct.pack_into("<I", hdr, 12, 92)                    # header size
    # CRC placeholder 0 first
    struct.pack_into("<I", hdr, 16, 0)
    struct.pack_into("<I", hdr, 20, 0)                     # reserved
    struct.pack_into("<Q", hdr, 24, 1)                     # my LBA
    struct.pack_into("<Q", hdr, 32, total_lbas - 1)        # alternate LBA
    struct.pack_into("<Q", hdr, 40, fs_offset_lba)         # first usable
    struct.pack_into("<Q", hdr, 48, fs_offset_lba + fs_sectors - 1)  # last usable
    hdr[56:72] = uuid.uuid5(uuid.NAMESPACE_OID, "robobootloader-disk").bytes_le
    struct.pack_into("<Q", hdr, 72, 2)                     # entries LBA
    struct.pack_into("<I", hdr, 80, 128)                   # entry count
    struct.pack_into("<I", hdr, 84, 128)                   # entry size
    struct.pack_into("<I", hdr, 88, zlib.crc32(entries) & 0xFFFFFFFF)
    struct.pack_into("<I", hdr, 16, zlib.crc32(hdr) & 0xFFFFFFFF)

    gpt_sectors = bytearray(BYTES_PER_SECTOR * (2 + 128 * 128 // BYTES_PER_SECTOR))
    gpt_sectors[0:BYTES_PER_SECTOR] = hdr.ljust(BYTES_PER_SECTOR, b"\x00")
    gpt_sectors[BYTES_PER_SECTOR:] = entries
    return bytes(mbr), bytes(gpt_sectors)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--output", required=True)
    ap.add_argument("--size-mb", type=int, default=64)
    ap.add_argument("--label", default="ROBOBOOT",
                    help="FAT32 volume label (shown in the boot picker)")
    ap.add_argument("--file", action="append", default=[],
                    help="image_path:host_path (repeatable)")
    args = ap.parse_args()

    files = []
    for spec in args.file:
        if ":" not in spec:
            raise SystemExit("--file must be image_path:host_path")
        image_path, host = spec.split(":", 1)
        with open(host, "rb") as f:
            files.append((image_path.replace("\\", "/"), f.read()))
    if not files:
        raise SystemExit("no --file given")

    fs = Fat32Image(args.size_mb * 1024 * 1024, label=args.label)
    fs.populate(files)
    fsimg = fs.finish()

    mbr, gpt = build_gpt(args.size_mb * 1024 * 1024, PARTITION_START_LBA,
                         len(fsimg) // BYTES_PER_SECTOR)
    img = bytearray(args.size_mb * 1024 * 1024)
    img[0:len(mbr)] = mbr
    img[BYTES_PER_SECTOR:BYTES_PER_SECTOR + len(gpt)] = gpt
    img[PARTITION_START_LBA * BYTES_PER_SECTOR:
        PARTITION_START_LBA * BYTES_PER_SECTOR + len(fsimg)] = fsimg
    with open(args.output, "wb") as f:
        f.write(img)
    print("wrote %s (%d bytes, %d files, GPT+ESP)" % (args.output, len(img), len(files)))


if __name__ == "__main__":
    main()
