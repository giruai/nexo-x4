#!/usr/bin/env python3
"""Fail-closed, idempotent ABR overlay for the pinned XCloud4 vendor snapshot.

Apply after the release-0.7.35 receive/NACK overlays. This changes only
libdatachannel's receiver SSRC synchronization and bounded outbound REMB
diagnostics. It does not alter media policy or the Xbox application.
"""

from pathlib import Path
import os
import sys
import tempfile


def replace_once(source: str, old: str, new: str, path: Path) -> str:
    if new in source:
        return source
    count = source.count(old)
    if count != 1:
        raise SystemExit(f"ABR anchor mismatch in {path}: expected 1, found {count}: {old[:100]!r}")
    return source.replace(old, new, 1)


def apply(base: Path) -> None:
    vendor = base / "libdatachannel"
    edits: dict[Path, str] = {}

    def patch(relative: str, old: str, new: str) -> None:
        path = vendor / relative
        current = edits.get(path, path.read_text(encoding="utf-8"))
        edits[path] = replace_once(current, old, new, path)

    def upgrade_if_present(relative: str, old: str, new: str) -> None:
        path = vendor / relative
        current = edits.get(path, path.read_text(encoding="utf-8"))
        if old in current:
            edits[path] = replace_once(current, old, new, path)

    patch("include/rtc/rtcpreceivingsession.hpp", "\tSSRC mSsrc = 0;",
          "\tstd::atomic<SSRC> mSsrc{0}; // incoming writer, owner-thread REMB reader")

    path = "src/rtcpreceivingsession.cpp"
    for source in ("rtp->ssrc()", "rr->senderSSRC()", "sr->senderSSRC()"):
        patch(path, f"mSsrc = {source};",
              f"mSsrc.store({source}, std::memory_order_release);")
    patch(path, "mRequestedBitrate.load(); bitrate > 0",
          "mRequestedBitrate.load(std::memory_order_acquire); bitrate > 0")
    patch(path, "\tmRequestedBitrate.store(bitrate);",
          "\tmRequestedBitrate.store(bitrate, std::memory_order_release);")
    patch(path,
          "\tauto message = make_message(RtcpRemb::SizeWithSSRCs(1), Message::Control);\n"
          "\tauto remb = reinterpret_cast<RtcpRemb *>(message->data());\n"
          "\tremb->preparePacket(mSsrc, 1, bitrate);\n"
          "\tremb->setSSRC(0, mSsrc);\n"
          "\tsend(message);",
          "\tconst SSRC mediaSsrc = mSsrc.load(std::memory_order_acquire);\n"
          "\t// A request before first video RTP is retained for the next SR, but\n"
          "\t// zero is not a valid REMB target. One snapshot fills both fields.\n"
          "\tif (mediaSsrc == 0) return;\n"
          "\tauto message = make_message(RtcpRemb::SizeWithSSRCs(1), Message::Control);\n"
          "\tauto remb = reinterpret_cast<RtcpRemb *>(message->data());\n"
          "\tremb->preparePacket(mediaSsrc, 1, bitrate);\n"
          "\tremb->setSSRC(0, mediaSsrc);\n"
          "\tsend(message);")
    patch(path,
          "\tauto message = make_message(RtcpRr::SizeWithReportBlocks(1), Message::Control);\n"
          "\tauto rr = reinterpret_cast<RtcpRr *>(message->data());\n"
          "\trr->preparePacket(mSsrc, 1);",
          "\tconst SSRC mediaSsrc = mSsrc.load(std::memory_order_acquire);\n"
          "\tauto message = make_message(RtcpRr::SizeWithReportBlocks(1), Message::Control);\n"
          "\tauto rr = reinterpret_cast<RtcpRr *>(message->data());\n"
          "\trr->preparePacket(mediaSsrc, 1);")
    patch(path, "reportBlock->preparePacket(mSsrc, fraction, lost,",
          "reportBlock->preparePacket(mediaSsrc, fraction, lost,")
    patch(path,
          "void RtcpReceivingSession::pushFIR(const message_callback &send, const std::vector<SSRC>& targetSSRCs, bool retransmit) {\n"
          "\tstd::vector<RtcpFirFci> firFcisToSend;",
          "void RtcpReceivingSession::pushFIR(const message_callback &send, const std::vector<SSRC>& targetSSRCs, bool retransmit) {\n"
          "\tconst SSRC mediaSsrc = mSsrc.load(std::memory_order_acquire);\n"
          "\tstd::vector<RtcpFirFci> firFcisToSend;")
    for old, new in (
        ("targetSSRC = mSsrc;", "targetSSRC = mediaSsrc;"),
        ("++mRfc5104FirCmdNums[mSsrc];", "++mRfc5104FirCmdNums[mediaSsrc];"),
        ("static_cast<uint8_t>(mRfc5104FirCmdNums[mSsrc] % 256)",
         "static_cast<uint8_t>(mRfc5104FirCmdNums[mediaSsrc] % 256)"),
        ("firFci.preparePacket(mSsrc,", "firFci.preparePacket(mediaSsrc,"),
        ("fir->preparePacket(mSsrc, firFcisToSend);", "fir->preparePacket(mediaSsrc, firFcisToSend);"),
    ):
        patch(path, old, new)
    patch(path,
          "\tauto message = make_message(RtcpPli::Size(), Message::Control);\n"
          "\tauto *pli = reinterpret_cast<RtcpPli *>(message->data());\n"
          "\tpli->preparePacket(mSsrc);",
          "\tconst SSRC mediaSsrc = mSsrc.load(std::memory_order_acquire);\n"
          "\tauto message = make_message(RtcpPli::Size(), Message::Control);\n"
          "\tauto *pli = reinterpret_cast<RtcpPli *>(message->data());\n"
          "\tpli->preparePacket(mediaSsrc);")

    path = "src/impl/dtlssrtptransport.hpp"
    patch(path, "\tstd::mutex sendMutex;",
          "\tstd::mutex sendMutex;\n"
          "#ifdef X4_OPENORBIS\n"
          "\tunsigned int mX4LastReportedRembBps = 0; // protected local send only\n"
          "\tunsigned int mX4RembFailuresReported = 0; // bounded by sendMutex\n"
          "#endif")

    path = "src/impl/dtlssrtptransport.cpp"
    upgrade_if_present(path,
        "if (message->size() == RtcpRemb::SizeWithSSRCs(1)) {",
        "if (size == int(RtcpRemb::SizeWithSSRCs(1))) {")
    upgrade_if_present(path,
        "\tconst bool localResult = Transport::outgoing(std::move(message)); // bypass DTLS DSCP marking\n",
        "\tbool localResult;\n"
        "\ttry {\n"
        "\t\tlocalResult = Transport::outgoing(std::move(message)); // bypass DTLS DSCP marking\n"
        "\t} catch (...) {\n"
        "#ifdef X4_OPENORBIS\n"
        "\t\tif (x4IsRemb && mX4RembFailuresReported++ < 8) X4_SRTP_EVENT(101, -1);\n"
        "#endif\n"
        "\t\tthrow;\n"
        "\t}\n")
    patch(path,
          "\tmessage = make_message(size + SRTP_MAX_TRAILER_LEN, message);\n\n"
          "\tif (IsRtcp(*message)) { // Demultiplex RTCP and RTP using payload type\n",
          "\tmessage = make_message(size + SRTP_MAX_TRAILER_LEN, message);\n\n"
          "#ifdef X4_OPENORBIS\n"
          "\tbool x4IsRemb = false;\n"
          "\tunsigned int x4RembBps = 0;\n"
          "\tint x4RembTargetPresent = 0;\n"
          "#endif\n"
          "\tif (IsRtcp(*message)) { // Demultiplex RTCP and RTP using payload type\n"
          "#ifdef X4_OPENORBIS\n"
          "\t\t// Inspect only the fixed header of the outgoing clear RTCP packet.\n"
          "\t\t// A matching single-SSRC REMB is identified before SRTCP protection.\n"
          "\t\tif (size == int(RtcpRemb::SizeWithSSRCs(1))) {\n"
          "\t\t\tconst auto *bytes = reinterpret_cast<const unsigned char *>(message->data());\n"
          "\t\t\tif ((bytes[0] >> 6) == 2 && (bytes[0] & 31) == 15 &&\n"
          "\t\t\t    bytes[1] == 206 && std::memcmp(bytes + 12, \"REMB\", 4) == 0) {\n"
          "\t\t\t\tconst auto *remb = reinterpret_cast<const RtcpRemb *>(message->data());\n"
          "\t\t\t\tif (remb->getSSRCCount() == 1) {\n"
          "\t\t\t\t\tx4IsRemb = true;\n"
          "\t\t\t\t\tx4RembBps = remb->getBitrate();\n"
          "\t\t\t\t\tx4RembTargetPresent = remb->getSSRC(0) != 0;\n"
          "\t\t\t\t}\n"
          "\t\t\t}\n"
          "\t\t}\n"
          "#endif\n")
    patch(path,
          "\t\tif (srtp_err_status_t err = protectStatus) {",
          "#ifdef X4_OPENORBIS\n"
          "\t\tif (x4IsRemb && protectStatus != srtp_err_status_ok &&\n"
          "\t\t    mX4RembFailuresReported++ < 8) X4_SRTP_EVENT(100, int(protectStatus));\n"
          "#endif\n"
          "\t\tif (srtp_err_status_t err = protectStatus) {")
    patch(path,
          "\treturn Transport::outgoing(std::move(message)); // bypass DTLS DSCP marking",
          "\tbool localResult;\n"
          "\ttry {\n"
          "\t\tlocalResult = Transport::outgoing(std::move(message)); // bypass DTLS DSCP marking\n"
          "\t} catch (...) {\n"
          "#ifdef X4_OPENORBIS\n"
          "\t\tif (x4IsRemb && mX4RembFailuresReported++ < 8) X4_SRTP_EVENT(101, -1);\n"
          "#endif\n"
          "\t\tthrow;\n"
          "\t}\n"
          "#ifdef X4_OPENORBIS\n"
          "\tif (x4IsRemb) {\n"
          "\t\tif (localResult && x4RembBps != mX4LastReportedRembBps) {\n"
          "\t\t\tmX4LastReportedRembBps = x4RembBps;\n"
          "\t\t\tX4_SRTP_EVENT(98, int(x4RembBps));\n"
          "\t\t\tX4_SRTP_EVENT(99, x4RembTargetPresent | 2 | 4);\n"
          "\t\t} else if (!localResult && mX4RembFailuresReported++ < 8) {\n"
          "\t\t\tX4_SRTP_EVENT(101, 0);\n"
          "\t\t}\n"
          "\t}\n"
          "#endif\n"
          "\treturn localResult;")

    # Validate every anchor before the first write. Atomic replacement prevents
    # a partially written source file if interrupted during publication.
    for path, new_content in edits.items():
        if path.read_text(encoding="utf-8") == new_content:
            continue
        fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.abr-", dir=path.parent)
        try:
            with os.fdopen(fd, "w", encoding="utf-8") as out:
                out.write(new_content)
                out.flush()
                os.fsync(out.fileno())
            os.replace(temporary, path)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)
    print("ABR vendor overlay applied or already present")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: apply-libdatachannel-abr.py /path/to/deps/webrtc")
    apply(Path(sys.argv[1]).resolve())
