const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const t = target.result;

    const ngtcp2_c = b.dependency("ngtcp2", .{
        .target = target,
        .optimize = optimize,
    });
    const picotls = b.dependency("picotls", .{
        .target = target,
        .optimize = optimize,
    });
    const openssl = b.dependency("openssl", .{
        .target = target,
        .optimize = optimize,
    });

    const ngtcp2 = b.addLibrary(.{
        .name = "ngtcp2",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize }),
    });

    ngtcp2.linkLibC();
    ngtcp2.root_module.addCMacro("BUILDING_NGTCP2", "");
    ngtcp2.root_module.addCMacro("NGTCP2_STATICLIB", "");
    if (t.os.tag != .windows) {
        ngtcp2.root_module.addCMacro("HAVE_ARPA_INET_H", "1");
        ngtcp2.root_module.addCMacro("HAVE_NETINET_IN_H", "1");
        ngtcp2.root_module.addCMacro("HAVE_UNISTD_H", "1");
    }

    ngtcp2.addIncludePath(ngtcp2_c.path("lib/includes"));
    ngtcp2.addIncludePath(ngtcp2_c.path("lib"));
    ngtcp2.addCSourceFiles(.{
        .root = ngtcp2_c.path("lib"),
        .files = &ngtcp2_sources,
        .flags = &.{
            "-fno-sanitize=all",
            "-std=gnu11",
            "-Wall",
            "-O2",
        },
    });

    ngtcp2.installHeadersDirectory(ngtcp2_c.path("lib/includes"), "", .{
        .include_extensions = &.{".h"},
    });

    const ngtcp2_crypto_picotls = b.addLibrary(.{
        .name = "ngtcp2_crypto_picotls",
        .root_module = b.createModule(.{ .target = target, .optimize = optimize }),
    });

    ngtcp2_crypto_picotls.linkLibrary(ngtcp2);
    ngtcp2_crypto_picotls.linkLibrary(picotls.artifact("picotls"));
    ngtcp2_crypto_picotls.linkLibrary(openssl.artifact("ssl"));
    ngtcp2_crypto_picotls.linkLibrary(openssl.artifact("crypto"));
    ngtcp2_crypto_picotls.linkLibC();

    ngtcp2_crypto_picotls.root_module.addCMacro("BUILDING_NGTCP2", "");
    ngtcp2_crypto_picotls.root_module.addCMacro("NGTCP2_STATICLIB", "");
    if (t.os.tag != .windows) {
        ngtcp2_crypto_picotls.root_module.addCMacro("HAVE_ARPA_INET_H", "1");
        ngtcp2_crypto_picotls.root_module.addCMacro("HAVE_NETINET_IN_H", "1");
        ngtcp2_crypto_picotls.root_module.addCMacro("HAVE_UNISTD_H", "1");
    }

    ngtcp2_crypto_picotls.addIncludePath(ngtcp2_c.path("lib/includes"));
    ngtcp2_crypto_picotls.addIncludePath(ngtcp2_c.path("lib"));
    ngtcp2_crypto_picotls.addIncludePath(ngtcp2_c.path("crypto/includes"));
    ngtcp2_crypto_picotls.addIncludePath(ngtcp2_c.path("crypto"));
    ngtcp2_crypto_picotls.addCSourceFiles(.{
        .root = ngtcp2_c.path("crypto"),
        .files = &.{
            "picotls/picotls.c",
            "shared.c",
        },
        .flags = &.{
            "-fno-sanitize=all",
            "-std=gnu11",
            "-Wall",
            "-O2",
        },
    });

    ngtcp2_crypto_picotls.installHeadersDirectory(ngtcp2_c.path("crypto/includes"), "", .{
        .include_extensions = &.{".h"},
    });

    b.installArtifact(ngtcp2);
    b.installArtifact(ngtcp2_crypto_picotls);
}

const ngtcp2_sources = [_][]const u8{
    "ngtcp2_acktr.c",
    "ngtcp2_addr.c",
    "ngtcp2_balloc.c",
    "ngtcp2_bbr.c",
    "ngtcp2_buf.c",
    "ngtcp2_callbacks.c",
    "ngtcp2_cc.c",
    "ngtcp2_cid.c",
    "ngtcp2_conn.c",
    "ngtcp2_conn_info.c",
    "ngtcp2_conv.c",
    "ngtcp2_crypto.c",
    "ngtcp2_dcidtr.c",
    "ngtcp2_err.c",
    "ngtcp2_fmt.c",
    "ngtcp2_frame_chain.c",
    "ngtcp2_gaptr.c",
    "ngtcp2_idtr.c",
    "ngtcp2_ksl.c",
    "ngtcp2_log.c",
    "ngtcp2_map.c",
    "ngtcp2_mem.c",
    "ngtcp2_objalloc.c",
    "ngtcp2_opl.c",
    "ngtcp2_path.c",
    "ngtcp2_pcg.c",
    "ngtcp2_pkt.c",
    "ngtcp2_pmtud.c",
    "ngtcp2_ppe.c",
    "ngtcp2_pq.c",
    "ngtcp2_pv.c",
    "ngtcp2_qlog.c",
    "ngtcp2_range.c",
    "ngtcp2_ratelim.c",
    "ngtcp2_ringbuf.c",
    "ngtcp2_rob.c",
    "ngtcp2_rst.c",
    "ngtcp2_rtb.c",
    "ngtcp2_settings.c",
    "ngtcp2_str.c",
    "ngtcp2_strm.c",
    "ngtcp2_transport_params.c",
    "ngtcp2_unreachable.c",
    "ngtcp2_vec.c",
    "ngtcp2_version.c",
    "ngtcp2_wf.c",
};
